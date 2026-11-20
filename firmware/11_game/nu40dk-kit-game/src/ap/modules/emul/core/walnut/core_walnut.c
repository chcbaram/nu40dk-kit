/*
 * core_walnut.c
 *
 *  Walnut-CGB (https://github.com/Mr-PauI/Walnut-CGB, MIT) 코어 연결부
 *  walnut_cgb.h 수정 사항 ([nu40dk-kit] 주석):
 *    - WGB_ROM_READ/16/32 매크로: ROM 읽기를 콜백 대신 포인터 직접 읽기로 바꿀 수 있게 함
 *    - WGB_HOT 속성 자리 (RAM 실행 실험용, 기본 비활성)
 *  사운드: minigb_apu (emul/apu/minigb_apu, MIT), 16kHz int16 스테레오
 */
#include "gb_core.h"

#include <string.h>


#define ENABLE_SOUND                1
#define ENABLE_LCD                  1
#define WALNUT_GB_12_COLOUR         GB_CGB_SUPPORT
#define WALNUT_FULL_GBC_SUPPORT     GB_CGB_SUPPORT
#define WALNUT_GB_HIGH_LCD_ACCURACY 1

// ROM 은 Flash(XIP) 에 있으므로 콜백 대신 포인터로 직접 읽는다 (Cortex-M4 는 비정렬 접근 지원)
static const uint8_t *rom_base;
#define WGB_ROM_BASE          rom_base      // 명령 가져오기 빠른 경로 (walnut_cgb.h wgb_fetch16)
#define WGB_ROM_READ(g, a)    (rom_base[(a)])
#define WGB_ROM_READ16(g, a)  (*(const uint16_t __attribute__((aligned(1))) *)(rom_base + (a)))
#define WGB_ROM_READ32(g, a)  (*(const uint32_t __attribute__((aligned(1))) *)(rom_base + (a)))

// 사운드: Walnut 이 사운드 레지스터(0xFF10~0xFF3F)를 읽고 쓸 때 minigb_apu 로 넘긴다
#include "minigb_apu.h"
static struct minigb_apu_ctx apu;
uint8_t audio_read(uint16_t addr)               { return minigb_apu_audio_read(&apu, addr); }
void    audio_write(uint16_t addr, uint8_t val) { minigb_apu_audio_write(&apu, addr, val); }

struct gb_s;                      // walnut_cgb.h 의 앞쪽 함수 원형이 구조체 정의보다 먼저 나옴
#include "walnut_cgb.h"


static struct gb_s       gb;
static gb_core_cfg_t     cfg;
static bool              run_dualfetch = true;
#if GB_CGB_SUPPORT
static uint8_t           cgb_shade[0x40];
static uint8_t           line_buf[160];
#endif




static uint8_t romRead(struct gb_s *p_gb, const uint_fast32_t addr)     { return rom_base[addr]; }
static uint16_t romRead16(struct gb_s *p_gb, const uint_fast32_t addr)  { return WGB_ROM_READ16(p_gb, addr); }
static uint32_t romRead32(struct gb_s *p_gb, const uint_fast32_t addr)  { return WGB_ROM_READ32(p_gb, addr); }

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
    uint32_t y = (r * 2 * 77 + g * 150 + b * 2 * 29) >> 8;
    cgb_shade[i] = 3 - (y >> 4);
  }
}
#endif

static void lcdDrawLine(struct gb_s *p_gb, const uint8_t *pixels, const uint_fast8_t line)
{
#if GB_CGB_SUPPORT
  if (p_gb->cgb.cgbMode)
  {
    if (line == 0) updateCgbShade();
    for (int x = 0; x < 160; x++) line_buf[x] = cgb_shade[pixels[x]];
    cfg.line_cb(line, line_buf);
    return;
  }
#endif
  cfg.line_cb(line, pixels);
}




const char *gbCoreName(void)
{
  return GB_CGB_SUPPORT ? "walnut-cgb (gbc)" : "walnut-cgb (dmg)";
}

bool gbCoreLoad(const gb_core_cfg_t *p_cfg)
{
  cfg      = *p_cfg;
  rom_base = cfg.p_rom;

  if (gb_init(&gb, romRead, romRead16, romRead32, cartRamRead, cartRamWrite, gbError, NULL) != GB_INIT_NO_ERROR)
  {
    return false;
  }
  gb_init_lcd(&gb, lcdDrawLine);
  minigb_apu_audio_init(&apu);
  return true;
}

uint32_t gbCoreAudioGetFrame(int16_t *p_buf)
{
  minigb_apu_audio_callback(&apu, p_buf);
  return AUDIO_SAMPLES_TOTAL;
}

uint32_t gbCoreAudioFrameSamples(void)
{
  return AUDIO_SAMPLES_TOTAL;
}

void gbCoreRunFrame(void)
{
  if (run_dualfetch)
    gb_run_frame_dualfetch(&gb);
  else
    gb_run_frame(&gb);
}

void gbCoreSetJoypad(uint8_t pressed)
{
  gb.direct.joypad = ~pressed;    // Walnut: 0 = 눌림
}

void gbCoreSetFrameSkip(bool enable)
{
  gb.direct.frame_skip = enable;
}

// frame_skip 을 켜 두고 frame_skip_count 를 직접 넣으면 프레임 단위로 렌더링을 정할 수 있다.
//   gb_run_frame() 은 VBlank(LY=144) 에서 끝나고, 그 시점에 frame_skip_count 를 뒤집는다.
//   다음 프레임의 0~143 줄은 실행 직전에 넣은 값으로 렌더링 여부가 정해진다.
void gbCoreSetDrawNext(bool draw)
{
  gb.direct.frame_skip         = true;
  gb.display.frame_skip_count  = draw;
}

void gbCoreSetOption(const char *p_opt)
{
  if (strcmp(p_opt, "single") == 0) run_dualfetch = false;
  if (strcmp(p_opt, "dual") == 0)   run_dualfetch = true;
  if (strcmp(p_opt, "idle") == 0)   wgb_idle_off = false;    // 대기 루프 건너뛰기 (기본)
  if (strcmp(p_opt, "noidle") == 0) wgb_idle_off = true;
}

uint32_t gbCoreGetIdleCycles(void)
{
  return wgb_idle_skips;
}

bool gbCoreIsCgb(void)
{
#if GB_CGB_SUPPORT
  return gb.cgb.cgbMode ? true : false;
#else
  return false;
#endif
}

bool gbCoreIsDoubleSpeed(void)
{
#if GB_CGB_SUPPORT
  return gb.cgb.doubleSpeed ? true : false;
#else
  return false;
#endif
}

uint32_t gbCoreStateSize(void)
{
  return sizeof(gb);
}
