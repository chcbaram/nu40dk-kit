/*
 * gb_emul.c
 *
 *  Game Boy / Game Boy Color 에뮬레이터 포팅 (Walnut-CGB, MIT)
 *
 *  - ROM  : Flash rom_storage 파티션 (0x7E000, 512KB) 을 XIP 로 직접 읽는다
 *  - LCD  : 라인 콜백에서 4 단계(2bit) 회색으로 변환해 160x144 버퍼에 저장
 *           CGB 는 RGB565 팔레트를 밝기로 변환
 *  - 사운드: Phase 6 에서 minigb_apu 연결 (지금은 비활성)
 */
#include "ap_def.h"
#include <pm_config.h>


// GB_CGB_SUPPORT 1: GBC 지원 (RAM +32KB, 메모리 접근마다 CGB 분기)
//                0: GB(DMG) 전용
#ifndef GB_CGB_SUPPORT
#define GB_CGB_SUPPORT            0
#endif

#define ENABLE_SOUND              0
#define ENABLE_LCD                1
#define WALNUT_GB_12_COLOUR       GB_CGB_SUPPORT
#define WALNUT_FULL_GBC_SUPPORT   GB_CGB_SUPPORT
#define WALNUT_GB_HIGH_LCD_ACCURACY 1
// RAM 실행(.ramfunc)은 Flash 캐시 덕분에 이득이 없어 사용하지 않음 (Phase 3 측정)
// #define WGB_HOT  __attribute__((noinline, long_call, section(".ramfunc")))
// ROM 은 Flash(XIP) 에 있으므로 콜백 대신 포인터로 직접 읽는다 (Cortex-M4 는 비정렬 접근 지원)
#define WGB_ROM_BASE          ((const uint8_t *)PM_ROM_STORAGE_ADDRESS)
#define WGB_ROM_READ(g, a)    (WGB_ROM_BASE[(a)])
#define WGB_ROM_READ16(g, a)  (*(const uint16_t __attribute__((aligned(1))) *)(WGB_ROM_BASE + (a)))
#define WGB_ROM_READ32(g, a)  (*(const uint32_t __attribute__((aligned(1))) *)(WGB_ROM_BASE + (a)))
struct gb_s;                      // walnut_cgb.h 의 앞쪽 함수 원형이 구조체 정의보다 먼저 나옴
#include "walnut_cgb.h"
#include "gb_emul.h"
#include "gb_rom.h"


#define GB_ROM_ADDR         PM_ROM_STORAGE_ADDRESS
#define GB_ROM_SIZE         PM_ROM_STORAGE_SIZE
#define GB_CART_RAM_MAX     (32 * 1024)
#define GB_FRAME_US         16743     // 70224 cycles @ 4.194304MHz = 59.73Hz


static bool init(void);
static void cliGb(cli_args_t *args);

MODULE_DEF(gb_emul)
{
  .name     = "gb_emul",
  .priority = MODULE_PRI_NORMAL,
  .init     = init
};


static struct gb_s gb;
static uint8_t     cart_ram[GB_CART_RAM_MAX];
static uint8_t     lcd_buf[GB_LCD_HEIGHT][GB_LCD_WIDTH / 4];   // 2bit/픽셀, 0 = 흰색 ~ 3 = 검정
#if GB_CGB_SUPPORT
static uint8_t     cgb_shade[0x40];                            // CGB 팔레트 → 4 단계 (프레임마다 갱신)
#endif
static bool        is_loaded = false;
static bool        lcd_enable = true;
static bool        run_dualfetch = true;
static uint32_t    gb_err_cnt = 0;

static const uint8_t *rom_base = (const uint8_t *)GB_ROM_ADDR;




static uint8_t gbRomRead(struct gb_s *p_gb, const uint_fast32_t addr)
{
  return rom_base[addr];
}

static uint16_t gbRomRead16(struct gb_s *p_gb, const uint_fast32_t addr)
{
  uint16_t v;
  memcpy(&v, &rom_base[addr], 2);
  return v;
}

static uint32_t gbRomRead32(struct gb_s *p_gb, const uint_fast32_t addr)
{
  uint32_t v;
  memcpy(&v, &rom_base[addr], 4);
  return v;
}

static uint8_t gbCartRamRead(struct gb_s *p_gb, const uint_fast32_t addr)
{
  return addr < GB_CART_RAM_MAX ? cart_ram[addr] : 0xFF;
}

static void gbCartRamWrite(struct gb_s *p_gb, const uint_fast32_t addr, const uint8_t val)
{
  if (addr < GB_CART_RAM_MAX) cart_ram[addr] = val;
}

static void gbError(struct gb_s *p_gb, const enum gb_error_e err, const uint16_t addr)
{
  if (gb_err_cnt++ < 4)
  {
    logPrintf("[E_] gb error %d at 0x%04X\n", err, addr);
  }
}

#if GB_CGB_SUPPORT
static void updateCgbShade(void)
{
  // RGB565 → 밝기 (0~63) → 4 단계 (밝을수록 0)
  for (int i = 0; i < 0x40; i++)
  {
    uint16_t c = gb.cgb.fixPalette[i];
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t g = (c >> 5)  & 0x3F;
    uint32_t b = (c >> 0)  & 0x1F;
    uint32_t y = (r * 2 * 77 + g * 150 + b * 2 * 29) >> 8;      // 0~63
    cgb_shade[i] = 3 - (y >> 4);
  }
}
#endif

static void gbLcdDrawLine(struct gb_s *p_gb, const uint8_t *pixels, const uint_fast8_t line)
{
  uint8_t *p_dst = lcd_buf[line];

  if (!lcd_enable) return;

#if GB_CGB_SUPPORT
  if (p_gb->cgb.cgbMode)
  {
    if (line == 0) updateCgbShade();
    for (int x = 0; x < GB_LCD_WIDTH; x += 4)
    {
      p_dst[x >> 2] = (cgb_shade[pixels[x + 0]] << 6) | (cgb_shade[pixels[x + 1]] << 4) |
                      (cgb_shade[pixels[x + 2]] << 2) | (cgb_shade[pixels[x + 3]] << 0);
    }
  }
  else
#endif
  {
    // DMG: 하위 2bit 가 팔레트 적용된 shade (0 흰색 ~ 3 검정)
    for (int x = 0; x < GB_LCD_WIDTH; x += 4)
    {
      p_dst[x >> 2] = ((pixels[x + 0] & 3) << 6) | ((pixels[x + 1] & 3) << 4) |
                      ((pixels[x + 2] & 3) << 2) | ((pixels[x + 3] & 3) << 0);
    }
  }
}




bool init(void)
{
  logPrintf("[OK] gbEmulInit()\n");
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

bool gbLoad(void)
{
  enum gb_init_error_e ret;

  is_loaded  = false;
  gb_err_cnt = 0;

  if (!gbRomIsValid())
  {
    logPrintf("[E_] gb rom header invalid\n");
    return false;
  }

  memset(cart_ram, 0xFF, sizeof(cart_ram));

  ret = gb_init(&gb, gbRomRead, gbRomRead16, gbRomRead32,
                gbCartRamRead, gbCartRamWrite, gbError, NULL);
  if (ret != GB_INIT_NO_ERROR)
  {
    logPrintf("[E_] gb_init() %d\n", ret);
    return false;
  }
  gb_init_lcd(&gb, gbLcdDrawLine);

  is_loaded = true;
  return true;
}

void gbRunFrame(void)
{
  if (run_dualfetch)
    gb_run_frame_dualfetch(&gb);
  else
    gb_run_frame(&gb);
}

void gbSetJoypad(uint8_t joypad)
{
  gb.direct.joypad = joypad;
}

const uint8_t *gbGetLcdBuf(void)
{
  return &lcd_buf[0][0];
}

bool gbIsCgb(void)
{
#if GB_CGB_SUPPORT
  return gb.cgb.cgbMode ? true : false;
#else
  return false;
#endif
}

static uint8_t gbCgbMode(void)        { return gbIsCgb() ? 1 : 0; }
static uint8_t gbDoubleSpeed(void)
{
#if GB_CGB_SUPPORT
  return gb.cgb.doubleSpeed;
#else
  return 0;
#endif
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
    cliPrintf("gb_s size  : %d bytes\n", sizeof(gb));
    cliPrintf("cgb build  : %d\n", GB_CGB_SUPPORT);
    cliPrintf("loaded     : %d, cgb mode %d\n", is_loaded, is_loaded ? gbCgbMode() : 0);
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

    lcd_enable    = true;
    run_dualfetch = true;
    for (int i = 2; i < args->argc; i++)
    {
      if (args->isStr(i, "nolcd"))  lcd_enable = false;
      if (args->isStr(i, "single")) run_dualfetch = false;
      if (args->isStr(i, "skip"))   bench_skip    = true;
    }

    if (gbLoad())
    {
      uint32_t start_us = micros();

      gb.direct.frame_skip = bench_skip;

      for (uint32_t i = 0; i < frames; i++)
      {
        pre_us = micros();
        gbRunFrame();
        exe_us = micros() - pre_us;
        if (exe_us > max_us) max_us = exe_us;

        if ((i & 0x3F) == 0 && cliKeepLoop() == false)
        {
          frames = i + 1;
          break;
        }
      }
      uint32_t total_us = micros() - start_us;
      uint32_t avg_us   = total_us / frames;

      cliPrintf("frames %d, lcd %s, %s, skip %d, cgb %d, double speed %d\n", frames, lcd_enable ? "on" : "off",
                run_dualfetch ? "dualfetch" : "single", bench_skip, gbCgbMode(), gbDoubleSpeed());
      gb.direct.frame_skip = false;
      cliPrintf("avg %d us/frame (max %d us), %d.%d fps, speed %d %%\n",
                avg_us, max_us, 1000000 / avg_us, (10000000 / avg_us) % 10,
                (GB_FRAME_US * 100) / avg_us);
    }
    lcd_enable    = true;
    run_dualfetch = true;
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
    cliPrintf("gb load <index|file> [force]\n");
    cliPrintf("gb bench <frames> [nolcd] [single] [skip]\n");
    cliPrintf("gb show [run_frames]\n");
  }
}
