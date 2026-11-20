/*
 * core_peanut.c
 *
 *  Peanut-GB (https://github.com/deltabeard/Peanut-GB, MIT) 코어 연결부. GB(DMG) 전용
 *  peanut_gb.h 수정 사항 ([nu40dk-kit] 주석):
 *    - PGB_ROM_READ 매크로: ROM 읽기를 콜백 대신 포인터 직접 읽기로 바꿀 수 있게 함
 */
#include "gb_core.h"

#if GB_CORE == GB_CORE_PEANUT
#include <string.h>


#define ENABLE_SOUND                0
#define ENABLE_LCD                  1
#define PEANUT_GB_12_COLOUR         0
#define PEANUT_GB_HIGH_LCD_ACCURACY 1

static const uint8_t *rom_base;
#define PGB_ROM_READ(g, a)    (rom_base[(a)])

#include "peanut_gb.h"


static struct gb_s   gb;
static gb_core_cfg_t cfg;




static uint8_t romRead(struct gb_s *p_gb, const uint_fast32_t addr)  { return rom_base[addr]; }

static uint8_t cartRamRead(struct gb_s *p_gb, const uint_fast32_t addr)
{
  return addr < cfg.cart_ram_size ? cfg.p_cart_ram[addr] : 0xFF;
}

static void cartRamWrite(struct gb_s *p_gb, const uint_fast32_t addr, const uint8_t val)
{
  if (addr < cfg.cart_ram_size) cfg.p_cart_ram[addr] = val;
}

static void gbError(struct gb_s *p_gb, const enum gb_error_e err, const uint16_t addr)
{
}

static void lcdDrawLine(struct gb_s *p_gb, const uint8_t *pixels, const uint_fast8_t line)
{
  cfg.line_cb(line, pixels);
}




const char *gbCoreName(void)
{
  return "peanut-gb (dmg)";
}

bool gbCoreLoad(const gb_core_cfg_t *p_cfg)
{
  cfg      = *p_cfg;
  rom_base = cfg.p_rom;

  if (gb_init(&gb, romRead, cartRamRead, cartRamWrite, gbError, NULL) != GB_INIT_NO_ERROR)
  {
    return false;
  }
  gb_init_lcd(&gb, lcdDrawLine);
  return true;
}

void gbCoreRunFrame(void)
{
  gb_run_frame(&gb);
}

void gbCoreSetJoypad(uint8_t pressed)
{
  gb.direct.joypad = ~pressed;    // Peanut: 0 = 눌림
}

void gbCoreSetFrameSkip(bool enable)
{
  gb.direct.frame_skip = enable;
}

void gbCoreSetOption(const char *p_opt)
{
}

bool gbCoreIsCgb(void)          { return false; }
bool gbCoreIsDoubleSpeed(void)  { return false; }

uint32_t gbCoreStateSize(void)
{
  return sizeof(gb);
}

#endif
