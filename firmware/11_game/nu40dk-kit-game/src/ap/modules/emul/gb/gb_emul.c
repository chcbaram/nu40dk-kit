/*
 * gb_emul.c
 *
 *  Game Boy 에뮬레이터 공통부 (코어와 무관)
 *
 *  - 코어  : gb_config.h 의 GB_CORE 로 선택 (core_walnut / core_peanut / core_gnuboy)
 *  - ROM  : Flash rom_storage 파티션 (0x7E000, 512KB) 을 XIP 로 직접 읽는다
 *  - LCD  : 코어의 라인 콜백에서 4 단계(2bit) 회색으로 160x144 버퍼에 저장
 *  - CLI  : gb info / list / load / bench / show
 */
#include "ap_def.h"
#include <pm_config.h>
#include "gb_core.h"
#include "gb_emul.h"
#include "gb_rom.h"


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

  for (int x = 0; x < GB_LCD_WIDTH; x += 4)
  {
    p_dst[x >> 2] = ((pixels[x + 0] & 3) << 6) | ((pixels[x + 1] & 3) << 4) |
                    ((pixels[x + 2] & 3) << 2) | ((pixels[x + 3] & 3) << 0);
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

bool gbLoad(void)
{
  gb_core_cfg_t cfg;

  is_loaded = false;

  if (!gbRomIsValid())
  {
    logPrintf("[E_] gb rom header invalid\n");
    return false;
  }

  memset(cart_ram, 0xFF, sizeof(cart_ram));

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

    lcd_enable = true;
    gbCoreSetOption("dual");

    if (gbLoad())
    {
      for (int i = 2; i < args->argc; i++)
      {
        if (args->isStr(i, "nolcd")) lcd_enable = false;
        else if (args->isStr(i, "skip"))  bench_skip = true;
        else gbCoreSetOption(args->getStr(i));
      }
      gbCoreSetFrameSkip(bench_skip);

      uint32_t start_us = micros();

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

      cliPrintf("core %s, frames %d, lcd %s, skip %d, cgb %d, double speed %d\n", gbCoreName(), frames,
                lcd_enable ? "on" : "off", bench_skip, gbCoreIsCgb(), gbCoreIsDoubleSpeed());
      gbCoreSetFrameSkip(false);
      cliPrintf("avg %d us/frame (max %d us), %d.%d fps, speed %d %%\n",
                avg_us, max_us, 1000000 / avg_us, (10000000 / avg_us) % 10,
                (GB_FRAME_US * 100) / avg_us);
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
    cliPrintf("gb bench <frames> [nolcd] [skip] [core option]\n");
    cliPrintf("gb show [run_frames]\n");
  }
}
