/*
 * gb_emul.c
 *
 *  Game Boy 에뮬레이터 공통부 (코어와 무관)
 *
 *  - 코어  : core/walnut (gb_core.h 인터페이스)
 *  - ROM  : Flash rom_storage 파티션 (0x7E000, 512KB) 을 XIP 로 직접 읽는다
 *  - LCD  : 코어의 라인 콜백에서 4 단계(2bit) 회색으로 160x144 버퍼에 저장
 *  - CLI  : gb info / list / load / bench / show
 */
#include "ap_def.h"
#include <pm_config.h>
#include <hal/nrf_nvmc.h>
#include <hal/nrf_timer.h>
#include <cmsis_core.h>


// PC 샘플링 프로파일러 (gb bench <n> prof): TIMER4 인터럽트에서 끊긴 스레드의 PC 를 모은다
#define PROF_MAX  256
static uint32_t          prof_pc[PROF_MAX];
static uint32_t          prof_lr[PROF_MAX];      // 호출한 쪽 (leaf 함수의 호출자 확인용)
static volatile uint32_t prof_cnt = 0;

ISR_DIRECT_DECLARE(profIsr)
{
  NRF_TIMER4->EVENTS_COMPARE[0] = 0;
  uint32_t *p_sp = (uint32_t *)__get_PSP();     // 스레드 모드에서 끊긴 경우 예외 프레임 (r0~r3, r12, lr, pc, xpsr)
  if (prof_cnt < PROF_MAX) { prof_lr[prof_cnt] = p_sp[5]; prof_pc[prof_cnt++] = p_sp[6]; }
  return 0;
}

static void profStart(uint32_t period_us)
{
  prof_cnt = 0;
  IRQ_DIRECT_CONNECT(TIMER4_IRQn, 2, profIsr, 0);
  NRF_TIMER4->TASKS_STOP  = 1;
  NRF_TIMER4->TASKS_CLEAR = 1;
  NRF_TIMER4->MODE        = 0;                  // timer
  NRF_TIMER4->BITMODE     = 3;                  // 32bit
  NRF_TIMER4->PRESCALER   = 4;                  // 1MHz
  NRF_TIMER4->CC[0]       = period_us;
  NRF_TIMER4->SHORTS      = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
  NRF_TIMER4->INTENSET    = TIMER_INTENSET_COMPARE0_Msk;
  irq_enable(TIMER4_IRQn);
  NRF_TIMER4->TASKS_START = 1;
}

static void profStop(void)
{
  NRF_TIMER4->TASKS_STOP = 1;
  NRF_TIMER4->INTENCLR   = TIMER_INTENCLR_COMPARE0_Msk;
  irq_disable(TIMER4_IRQn);
}
#include "gb_core.h"
#include "gb_emul.h"
#include "gb_rom.h"
#include "gb_run.h"


#define GB_ROM_ADDR         PM_ROM_STORAGE_ADDRESS
#define GB_ROM_SIZE         PM_ROM_STORAGE_SIZE
#define GB_FRAME_US         16743     // 70224 cycles @ 4.194304MHz = 59.73Hz


static bool init(void);
static void cliGb(cli_args_t *args);

MODULE_DEF(gb_emul)
{
  .name     = "gb_emul",
  .priority = MODULE_PRI_NORMAL,
  .init     = init
};


static uint8_t     cart_ram[GB_CART_RAM_MAX];
static uint8_t     lcd_buf[GB_LCD_HEIGHT][GB_LCD_WIDTH / 4];   // 2bit/픽셀, 0 = 흰색 ~ 3 = 검정
static bool        is_loaded  = false;
static bool        lcd_enable = true;

static const uint8_t *rom_base = (const uint8_t *)GB_ROM_ADDR;




static void lcdLineCb(uint8_t line, const uint8_t *pixels)
{
  uint8_t *p_dst = lcd_buf[line];

  if (!lcd_enable) return;

  // 4 픽셀(바이트)을 32비트로 읽어 곱셈 한 번으로 2비트 4개를 한 바이트에 모은다
  //   ((b0 << 6) | (b1 << 4) | (b2 << 2) | b3) = ((v & 0x03030303) * 0x40100401) >> 24
  for (int x = 0; x < GB_LCD_WIDTH; x += 4)
  {
    uint32_t v;
    memcpy(&v, &pixels[x], 4);
    p_dst[x >> 2] = (uint8_t)(((v & 0x03030303UL) * 0x40100401UL) >> 24);
  }
}




bool init(void)
{
  logPrintf("[OK] gbEmulInit()\n");
  logPrintf("     core %s\n", gbCoreName());
  logPrintf("     rom_storage 0x%X, %d KB\n", GB_ROM_ADDR, GB_ROM_SIZE / 1024);
  cliAdd("gb", cliGb);
  return true;
}

bool gbRomIsValid(void)
{
  // 헤더 체크섬 (0x134~0x14C)
  uint8_t chk = 0;

  for (int i = 0x134; i <= 0x14C; i++)
  {
    chk = chk - rom_base[i] - 1;
  }
  return chk == rom_base[0x14D];
}

void gbRomGetTitle(char *p_title, uint32_t len)
{
  uint32_t i;

  for (i = 0; i < len - 1 && i < 15; i++)
  {
    char c = rom_base[0x134 + i];
    if (c == 0 || (uint8_t)c >= 0x80) break;
    p_title[i] = c;
  }
  p_title[i] = 0;
}

// 배터리 백업 세이브 크기 (배터리 없는 카트리지는 0)
uint32_t gbGetSaveSize(void)
{
  static const uint32_t ram_size[6] = {0, 2048, 8192, 32768, 131072, 65536};
  uint8_t  type = rom_base[0x147];
  uint8_t  code = rom_base[0x149];
  uint32_t size;

  switch (type)
  {
    case 0x03: case 0x06: case 0x09: case 0x0D: case 0x0F: case 0x10:
    case 0x13: case 0x1B: case 0x1E:
      break;
    default:
      return 0;
  }
  size = (type == 0x06) ? 512 : (code < 6 ? ram_size[code] : 0);   // MBC2 는 내장 512 x 4bit
  return MIN(size, (uint32_t)GB_CART_RAM_MAX);
}

uint8_t *gbGetCartRam(void)
{
  return cart_ram;
}

void gbClearCartRam(void)
{
  memset(cart_ram, 0xFF, sizeof(cart_ram));
}

bool gbLoad(void)
{
  return gbLoadEx(true);
}

// clear_ram = false 면 cart RAM 을 지우지 않는다 (미리 불러온 세이브 유지)
bool gbLoadEx(bool clear_ram)
{
  gb_core_cfg_t cfg;

  is_loaded = false;

  if (!gbRomIsValid())
  {
    logPrintf("[E_] gb rom header invalid\n");
    return false;
  }

  if (clear_ram) gbClearCartRam();

  cfg.p_rom         = rom_base;
  cfg.p_cart_ram    = cart_ram;
  cfg.cart_ram_size = sizeof(cart_ram);
  cfg.line_cb       = lcdLineCb;

  if (!gbCoreLoad(&cfg))
  {
    logPrintf("[E_] gbCoreLoad()\n");
    return false;
  }

  is_loaded = true;
  return true;
}

void gbRunFrame(void)
{
  gbCoreRunFrame();
}

void gbSetJoypad(uint8_t pressed)
{
  gbCoreSetJoypad(pressed);
}

const uint8_t *gbGetLcdBuf(void)
{
  return &lcd_buf[0][0];
}

bool gbIsCgb(void)
{
  return gbCoreIsCgb();
}




static const char *mbcName(uint8_t type)
{
  switch (type)
  {
    case 0x00:                     return "ROM";
    case 0x01: case 0x02: case 0x03: return "MBC1";
    case 0x05: case 0x06:          return "MBC2";
    case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: return "MBC3";
    case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: return "MBC5";
    default:                       return "?";
  }
}

static bool cliListCb(const char *p_name, uint32_t size, void *arg)
{
  uint32_t *p_idx = (uint32_t *)arg;

  cliPrintf("  %2d  %-32s %4d KB\n", (*p_idx)++, p_name, size / 1024);
  return true;
}

static void cliLoadProgress(uint32_t done, uint32_t total)
{
  cliPrintf("\r  %3d %%  (%d / %d KB)", done * 100 / total, done / 1024, total / 1024);
}

void cliGb(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    char title[16];

    gbRomGetTitle(title, sizeof(title));
    cliPrintf("rom addr   : 0x%X (%d KB)\n", GB_ROM_ADDR, GB_ROM_SIZE / 1024);
    cliPrintf("header     : %s\n", gbRomIsValid() ? "OK" : "invalid");
    cliPrintf("title      : %s\n", title);
    cliPrintf("cgb flag   : 0x%02X %s\n", rom_base[0x143],
              rom_base[0x143] == 0xC0 ? "(CGB only)" : rom_base[0x143] == 0x80 ? "(DMG+CGB)" : "(DMG)");
    cliPrintf("cart       : 0x%02X %s\n", rom_base[0x147], mbcName(rom_base[0x147]));
    cliPrintf("rom size   : %d KB\n", 32 << rom_base[0x148]);
    cliPrintf("ram code   : %d\n", rom_base[0x149]);
    cliPrintf("core       : %s\n", gbCoreName());
    cliPrintf("core state : %d bytes\n", gbCoreStateSize());
    cliPrintf("loaded     : %d, cgb mode %d\n", is_loaded, is_loaded ? gbCoreIsCgb() : 0);
    ret = true;
  }

  // gb bench <frames> [nolcd] : 화면 출력 없이 에뮬레이션 속도 측정
  if (args->argc >= 2 && args->isStr(0, "bench"))
  {
    uint32_t frames = args->getData(1);
    uint32_t pre_us;
    uint32_t exe_us;
    uint32_t max_us = 0;
    bool     bench_skip = false;
    bool     bench_audio = false;
    bool     bench_prof  = false;
    bool     bench_play  = false;   // 정해진 프레임에 Start, 오른쪽+점프 입력 (재현 가능한 플레이 장면)
    uint32_t audio_sum = 0;
    uint32_t audio_crc = 0;       // 생성한 샘플의 체크섬 (최적화 전후 소리가 같은지 확인)
    static int16_t bench_audio_buf[768];       // 22050Hz 스테레오 1 프레임 738 샘플

    gbRunStop();
    lcd_enable = true;
    gbCoreSetOption("dual");

    if (gbLoad())
    {
      for (int i = 2; i < args->argc; i++)
      {
        if (args->isStr(i, "nolcd")) lcd_enable = false;
        else if (args->isStr(i, "skip"))  bench_skip = true;
        else if (args->isStr(i, "audio")) bench_audio = true;
        else if (args->isStr(i, "prof"))  bench_prof = true;
        else if (args->isStr(i, "play"))  bench_play = true;
        else gbCoreSetOption(args->getStr(i));
      }
      gbCoreSetFrameSkip(bench_skip);

      // 플래시 명령 캐시 적중/미스 측정 (NVMC 캐시 프로파일링)
      NRF_NVMC->ICACHECNF |= NVMC_ICACHECNF_CACHEPROFEN_Msk;
      NRF_NVMC->IHIT  = 0;
      NRF_NVMC->IMISS = 0;

      uint32_t start_us = micros();
      if (bench_prof) profStart(19993);           // 프레임 주기와 겹치지 않게 약 20ms 간격

      for (uint32_t i = 0; i < frames; i++)
      {
        if (bench_play)
        {
          uint8_t joy = 0;
          if ((i >= 300 && i < 310) || (i >= 600 && i < 610)) joy |= GB_JOY_START;
          if (i >= 700) joy |= GB_JOY_RIGHT;
          if (i >= 700 && (i % 50) < 15) joy |= GB_JOY_A;
          gbSetJoypad(joy);
        }
        pre_us = micros();
        gbRunFrame();
        exe_us = micros() - pre_us;
        if (exe_us > max_us) max_us = exe_us;
        if (bench_audio)
        {
          // 사운드 합성을 별도로 재서, 에뮬레이터 프레임 시간에 사운드가 주는 영향을 분리한다
          uint32_t a_us = micros();
          uint32_t n = gbCoreAudioGetFrame(bench_audio_buf);
          audio_sum += micros() - a_us;
          for (uint32_t k = 0; k < n; k++) audio_crc = audio_crc * 31 + (uint16_t)bench_audio_buf[k];
        }

        if ((i & 0x3F) == 0 && cliKeepLoop() == false)
        {
          frames = i + 1;
          break;
        }
      }
      if (bench_prof) profStop();
      uint32_t total_us = micros() - start_us - audio_sum;
      uint32_t avg_us   = total_us / frames;

      cliPrintf("core %s, frames %d, lcd %s, skip %d, cgb %d, double speed %d\n", gbCoreName(), frames,
                lcd_enable ? "on" : "off", bench_skip, gbCoreIsCgb(), gbCoreIsDoubleSpeed());
      gbCoreSetFrameSkip(false);
      cliPrintf("avg %d us/frame (max %d us), %d.%d fps, speed %d %%\n",
                avg_us, max_us, 1000000 / avg_us, (10000000 / avg_us) % 10,
                (GB_FRAME_US * 100) / avg_us);
      {
        uint32_t hit = NRF_NVMC->IHIT, miss = NRF_NVMC->IMISS;
        cliPrintf("icache hit %u, miss %u (%u.%u %% miss), miss/frame %u\n", hit, miss,
                  (uint32_t)((uint64_t)miss * 100 / (hit + miss + 1)),
                  (uint32_t)((uint64_t)miss * 1000 / (hit + miss + 1)) % 10, miss / frames);
      }
      if (bench_prof)
      {
        cliPrintf("prof %d samples\n", prof_cnt);
        for (uint32_t i = 0; i < prof_cnt; i++)
        {
          cliPrintf("%08X:%08X%s", prof_pc[i], prof_lr[i], (i % 4) == 3 ? "\n" : " ");
          if ((i % 64) == 63) delay(5);
        }
        cliPrintf("\nprof end\n");
      }
      if (bench_audio) cliPrintf("audio %d us/frame (excluded from emu time), crc %08X\n", audio_sum / frames, audio_crc);
    }
    lcd_enable = true;
    gbCoreSetOption("dual");
    ret = true;
  }

  // gb show : 현재 LCD 버퍼를 문자로 출력 (40x18, 4x8 블럭 평균)
  if (args->argc >= 1 && args->isStr(0, "show"))
  {
    uint32_t run = args->argc >= 2 ? args->getData(1) : 0;
    const char shade_chr[4] = {' ', '.', '+', '#'};

    if (run > 0 && gbLoad())
    {
      for (uint32_t i = 0; i < run; i++) gbRunFrame();
    }
    for (int by = 0; by < GB_LCD_HEIGHT; by += 8)
    {
      for (int bx = 0; bx < GB_LCD_WIDTH; bx += 4)
      {
        uint32_t sum = 0;
        for (int y = 0; y < 8; y++)
        {
          uint8_t b = lcd_buf[by + y][bx >> 2];
          sum += ((b >> 6) & 3) + ((b >> 4) & 3) + ((b >> 2) & 3) + (b & 3);
        }
        cliPrintf("%c", shade_chr[(sum + 16) / 32 > 3 ? 3 : (sum + 16) / 32]);
      }
      cliPrintf("\n");
    }
    ret = true;
  }

  // gb run [번호] : (번호가 있으면 SD 에서 먼저 load) Flash 의 ROM 실행, EPAPER 표시
  if (args->argc >= 1 && args->isStr(0, "run"))
  {
    if (args->argc >= 2)
    {
      char name[64];

      gbRunStop();
      if (gbRomGetName(args->getData(1), name, sizeof(name)))
      {
        cliPrintf("load : %s\n", name);
        cliPrintf("load : %s\n", gbRomErrStr(gbRomLoad(name, false, cliLoadProgress)));
      }
    }
    char title[16];
    gbRomGetTitle(title, sizeof(title));
    cliPrintf("run  : %s %s\n", title, gbRunStart() ? "OK" : "Fail");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "stop"))
  {
    gbRunStop();
    cliPrintf("stop\n");
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "sound"))
  {
    gbRunSetSound(args->isStr(1, "on"));
    cliPrintf("sound %s\n", gbRunGetSound() ? "on" : "off");
    ret = true;
  }

  // gb key <a|b|select|start|right|left|up|down>[,..] [ms] : 테스트용 입력
  if (args->argc >= 2 && args->isStr(0, "key"))
  {
    const char *names[8] = {"a", "b", "select", "start", "right", "left", "up", "down"};
    char        buf[48];
    uint8_t     keys = 0;

    snprintf(buf, sizeof(buf), "%s", args->getStr(1));
    for (char *p_tok = strtok(buf, ","); p_tok != NULL; p_tok = strtok(NULL, ","))
    {
      for (int i = 0; i < 8; i++)
        if (strcmp(p_tok, names[i]) == 0) keys |= (1 << i);
    }
    uint32_t ms = args->argc >= 3 ? args->getData(2) : 200;
    gbRunInjectKey(keys, ms);
    cliPrintf("key 0x%02X, %d ms\n", keys, ms);
    ret = true;
  }

  // gb disp ink|frame : 화면 갱신 방식
  if (args->argc == 2 && args->isStr(0, "disp"))
  {
    gbRunSetDispMode(args->isStr(1, "ink") ? 1 : 0);
    cliPrintf("disp %s\n", gbRunGetDispMode() ? "ink" : "frame");
    ret = true;
  }

  // gb ink [budget frames frame_ms] : ink 모드 설정
  if (args->argc >= 1 && args->isStr(0, "ink"))
  {
    uint8_t  b, f;
    uint32_t ms;

    gbRunGetInk(&b, &f, &ms);
    if (args->argc >= 2) b  = args->getData(1);
    if (args->argc >= 3) f  = args->getData(2);
    if (args->argc >= 4) ms = args->getData(3);
    gbRunSetInk(b, f, ms);
    gbRunGetInk(&b, &f, &ms);
    cliPrintf("ink : budget %d pulses, %d frames/scan, new frame every %d ms (fr: epd opt)\n", b, f, ms);
    ret = true;
  }

  // gb gray on|off [frames] : 회색 모드
  if (args->argc >= 2 && args->isStr(0, "gray"))
  {
    if (args->argc >= 3) gbRunSetGrayFrames(args->getData(2));
    gbRunSetGray(args->isStr(1, "on"));
    cliPrintf("gray %s, %d frames/scan\n", gbRunGetGray() ? "on" : "off", gbRunGetGrayFrames());
    ret = true;
  }

  // gb dose [from to n] : 회색 전환 펄스 수 (0 흰, 1 연회, 2 진회, 3 검정)
  if (args->argc >= 1 && args->isStr(0, "dose"))
  {
    const char *name = "WLDK";

    if (args->argc >= 4) gbRunSetGrayDose(args->getData(1), args->getData(2), args->getData(3));
    cliPrintf("from\\to  W  L  D  K\n");
    for (int f = 0; f < 4; f++)
    {
      cliPrintf("   %c   ", name[f]);
      for (int t = 0; t < 4; t++) cliPrintf("%2d ", gbRunGetGrayDose(f, t));
      cliPrintf("\n");
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "win"))
  {
    gbRunSetWinFull(args->isStr(1, "full"));
    gbRunRequestFull();
    cliPrintf("window %s\n", args->isStr(1, "full") ? "full width (1 DMA)" : "game area (per line)");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "full"))
  {
    gbRunRequestFull();
    ret = true;
  }

  // gb stat : 1 초마다 에뮬 속도와 화면 fps 출력 (아무 키나 누르면 종료)
  // 실행 중 PC 샘플링: gb prof [ms]  (모든 스레드 대상, 약 1000 샘플)
  if (args->argc >= 1 && args->isStr(0, "prof"))
  {
    uint32_t ms = args->argc >= 2 ? args->getData(1) : 10000;
    uint32_t period = ms * 1000 / PROF_MAX;

    profStart(period < 100 ? 100 : period);
    delay(ms);
    profStop();
    cliPrintf("prof %d samples\n", prof_cnt);
    for (uint32_t i = 0; i < prof_cnt; i++)
    {
      cliPrintf("%08X:%08X%s", prof_pc[i], prof_lr[i], (i % 4) == 3 ? "\n" : " ");
      if ((i % 64) == 63) delay(5);
    }
    cliPrintf("\nprof end\n");
    ret = true;
  }

  // 회색 모드 목표 변경 보정: gb retarget <되돌림 % (0=끔)> [같은 방향 더하기 0|1]
  if (args->argc >= 1 && args->isStr(0, "retarget"))
  {
    uint16_t rev;
    bool     same;

    gbRunGetRetarget(&rev, &same);
    if (args->argc >= 2) rev  = args->getData(1);
    if (args->argc >= 3) same = args->getData(2) != 0;
    gbRunSetRetarget(rev, same);
    cliPrintf("retarget : reverse %d %%, same-dir add %s\n", rev, same ? "on" : "off");
    ret = true;
  }

  // 조용한 순간 정리: gb quiet <ms (0=끔)> [프레임당 px] [누적 px]
  if (args->argc >= 1 && args->isStr(0, "quiet"))
  {
    uint32_t ms, px, act;

    gbRunGetQuiet(&ms, &px, &act);
    if (args->argc >= 2) ms  = args->getData(1);
    if (args->argc >= 3) px  = args->getData(2);
    if (args->argc >= 4) act = args->getData(3);
    gbRunSetQuiet(ms, px, act);
    cliPrintf("quiet : %d ms, < %d px/frame, after %d px\n", ms, px, act);
    ret = true;
  }

  // 장면 전환 전체 갱신 기준: gb scene [px|0]  (GB 화면 23040 픽셀 중 바뀐 수)
  if (args->argc >= 1 && args->isStr(0, "scene"))
  {
    if (args->argc >= 2) gbRunSetScenePx(args->getData(1));
    cliPrintf("scene : %d px (%d %%)\n", gbRunGetScenePx(), gbRunGetScenePx() * 100 / (160 * 144));
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "stat"))
  {
    gb_run_stat_t pre, cur;
    uint64_t      emu0, disp0, emu1, disp1;
    k_thread_runtime_stats_t all0, all1;

    gbRunGetStat(&pre);
    gbRunGetThreadCycles(&emu0, &disp0);
    k_thread_runtime_stats_all_get(&all0);
    while (cliKeepLoop())
    {
      delay(1000);
      gbRunGetStat(&cur);

      uint32_t ef = cur.emu_frames  - pre.emu_frames;
      uint32_t df = cur.disp_frames - pre.disp_frames;
      uint32_t eu = cur.emu_us_sum  - pre.emu_us_sum;
      uint32_t dm = cur.disp_ms_sum - pre.disp_ms_sum;

      uint32_t au = cur.audio_us_sum - pre.audio_us_sum;
      uint32_t ad = cur.audio_drop   - pre.audio_drop;

      uint32_t sc = cur.ink_scans   - pre.ink_scans;
      uint32_t pp = cur.ink_pending - pre.ink_pending;

      if (gbRunGetDispMode())
        cliPrintf("    ink cpu: plan %d us/frame, dec %d us/scan, rows %d/scan, scene %d, quiet %d\n",
                  df ? (cur.ink_plan_us - pre.ink_plan_us) / df : 0,
                  sc ? (cur.ink_dec_us  - pre.ink_dec_us)  / sc : 0,
                  sc ? (cur.ink_rows    - pre.ink_rows)    / sc : 0,
                  cur.ink_scene - pre.ink_scene, cur.ink_quiet - pre.ink_quiet);
      if (gbRunGetDispMode())
        cliPrintf("emu %2d fps (%3d %%, %5d us/frame)  ink: frames %d, scans %d (%d ms, %d px)  audio %d us\n",
                  ef, ef * 100 / 60, ef ? eu / ef : 0, df, sc, sc ? dm / sc : 0, sc ? pp / sc : 0, ef ? au / ef : 0);
      else
        cliPrintf("emu %2d fps (%3d %%, %5d us/frame)  disp %d fps (%d ms)  audio %d us, drop %d\n",
                  ef, ef * 100 / 60, ef ? eu / ef : 0, df, df ? dm / df : 0, ef ? au / ef : 0, ad);

      // CPU 사용률: 전체 실행 사이클 대비 각 스레드 (나머지는 BT, I2S, CLI, 인터럽트 등)
      gbRunGetThreadCycles(&emu1, &disp1);
      k_thread_runtime_stats_all_get(&all1);
      uint64_t total = (all1.execution_cycles - all0.execution_cycles) + (all1.idle_cycles - all0.idle_cycles);
      if (total > 0)
      {
        cliPrintf("    cpu: emu %2d%%  disp %2d%%  other %2d%%  idle %2d%%\n",
                  (int)((emu1 - emu0) * 100 / total), (int)((disp1 - disp0) * 100 / total),
                  (int)(((all1.execution_cycles - all0.execution_cycles) - (emu1 - emu0) - (disp1 - disp0)) * 100 / total),
                  (int)((all1.idle_cycles - all0.idle_cycles) * 100 / total));
      }
      emu0 = emu1; disp0 = disp1; all0 = all1;
      pre = cur;
    }
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "list"))
  {
    uint32_t idx = 0;
    uint32_t cnt = gbRomList(cliListCb, &idx);
    cliPrintf("%d files in /roms\n", cnt);
    ret = true;
  }

  // gb load <번호|파일 이름> [force] : SD /roms/<file> → Flash rom_storage
  //   이름에 공백이 있으면 나머지 인자를 이어 붙인다
  if (args->argc >= 2 && args->isStr(0, "load"))
  {
    char         name[64] = {0};
    bool         force = false;
    int          last  = args->argc;
    uint32_t     pre_time;
    gb_rom_err_t err;

    if (args->isStr(last - 1, "force"))
    {
      force = true;
      last--;
    }

    const char *p_arg1 = args->getStr(1);
    if (last == 2 && p_arg1[0] >= '0' && p_arg1[0] <= '9')
    {
      gbRomGetName(args->getData(1), name, sizeof(name));
    }
    else
    {
      for (int i = 1; i < last; i++)
      {
        if (i > 1) strncat(name, " ", sizeof(name) - strlen(name) - 1);
        strncat(name, args->getStr(i), sizeof(name) - strlen(name) - 1);
      }
    }

    cliPrintf("load : %s\n", name);
    pre_time  = millis();
    is_loaded = false;
    err = gbRomLoad(name, force, cliLoadProgress);
    cliPrintf("\nload : %s, %d ms\n", gbRomErrStr(err), millis() - pre_time);
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("gb info\n");
    cliPrintf("gb list\n");
    cliPrintf("gb run [index]\n");
    cliPrintf("gb stop\n");
    cliPrintf("gb stat\n");
    cliPrintf("gb full\n");
    cliPrintf("gb sound on|off\n");
    cliPrintf("gb win full|game\n");
    cliPrintf("gb disp ink|frame\n");
    cliPrintf("gb ink [budget frames frame_ms]\n");
    cliPrintf("gb gray on|off [frames]\n");
    cliPrintf("gb dose [from to n]\n");
    cliPrintf("gb key <a,b,select,start,right,left,up,down> [ms]\n");
    cliPrintf("gb load <index|file> [force]\n");
    cliPrintf("gb bench <frames> [nolcd] [skip] [core option]\n");
    cliPrintf("gb show [run_frames]\n");
  }
}
