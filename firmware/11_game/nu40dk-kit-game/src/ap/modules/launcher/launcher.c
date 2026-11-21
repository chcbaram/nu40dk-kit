/*
 * launcher.c
 *
 *  레트로 터미널 스타일 런처 (Phase 7)
 *
 *  - 화면: 400x300 1bpp, 8x16 글꼴 50 열 x 18 줄 (위 반전 바, 아래 버튼 안내)
 *  - 입력: 보드 BTN1 위 / BTN2 아래 / BTN3 선택 / BTN4 뒤로, Xbox D-pad / A / B
 *  - 갱신: 메뉴 이동은 EPD PART(공장 웨이브폼), 화면 전환과 일정 횟수마다 FULL,
 *          입력이 없으면 아날로그 전원을 끈다
 *  - 설정: Zephyr settings ("game/...") 로 Flash 에 저장
 */
#include "launcher.h"
#include "gb_core.h"
#include "gb_emul.h"
#include "gb_rom.h"
#include "gb_run.h"
#include "remote/ble_pad.h"
#include "game/app_api.h"
#include "game/rpg/rpg.h"
#include "game/voice.h"
#include <zephyr/settings/settings.h>
#include <stdarg.h>


#define UI_COLS             50
#define UI_ROWS             18
#define UI_X0               0
#define UI_Y0               6
#define UI_CW               8
#define UI_CH               16

#define LAUNCHER_COLD_TEMP  10        // 화면 전환 갱신에 쓸 온도 (°C)
#define FAST_PER_FULL       10        // 부분 갱신 N 번마다 FULL 로 정리 (흰 배경에 점이 쌓이기 전에)
#define IDLE_CLEAN_MS       5000      // 부분 갱신 뒤 입력이 없으면 전체 갱신으로 정리
#define IDLE_POWER_OFF_MS   60000     // 입력이 없으면 EPD 아날로그 전원 끄기 (3초였을 때 다음 입력마다 부스터 기동만큼 늦었다)
#define KEY_EVENT_MAX_AGE   1500      // 이보다 오래된 눌림 이벤트는 버린다 (긴 갱신 중 눌린 키가 한참 뒤에 적용되지 않게)
#define REPEAT_DELAY_MS     400
#define REPEAT_RATE_MS      120

#define LIST_MAX            64
#define NAME_MAX            48


typedef enum
{
  KEY_NONE = 0,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_OK,
  KEY_BACK,
} ui_key_t;

typedef enum
{
  SCR_MAIN = 0,
  SCR_GAMES,
  SCR_CONTROLLER,
  SCR_SETTINGS,
  SCR_INFO,
  SCR_APPS,         // 네이티브 게임 (GAMES)
} screen_t;

typedef struct
{
  uint8_t sound;          // 0/1
  uint8_t volume;         // 0~100
  uint8_t screen;         // 게임 화면: 0 GRAY (회색 펄스), 1 DITHER (2x2 디더)
} launcher_cfg_t;


static launcher_cfg_t cfg = {1, 80, 0};
static const char    *screen_str[2] = {"GRAY", "DITHER"};

static screen_t  screen       = SCR_MAIN;
static int       sel[6]       = {0};
static int       scroll       = 0;
static char      rom_list[LIST_MAX][NAME_MAX];
static uint32_t  rom_size[LIST_MAX];
static int       rom_cnt      = 0;
static char      cur_rom[NAME_MAX] = "";      // 실행 중인 ROM 의 SD 파일 이름 (세이브 경로용)

static uint32_t  fast_cnt     = 0;
static uint32_t  last_draw_ms = 0;
static bool      epd_on       = false;
static bool      need_full    = true;
static bool      need_strong  = true;     // 부팅/게임 종료 후 첫 화면은 강한 갱신으로 잔상 제거
static uint32_t  status_ms    = 0;




/*---------------------------------------------------------------------------
 * 설정 저장 (Zephyr settings)
 *-------------------------------------------------------------------------*/
static int cfgSet(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
  if (strcmp(name, "cfg") == 0 && len == sizeof(cfg))
  {
    read_cb(cb_arg, &cfg, sizeof(cfg));
  }
  return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(game, "game", NULL, cfgSet, NULL, NULL);

static void cfgApply(void)
{
  cfg.volume  = MIN(cfg.volume, 100);
  cfg.screen  = MIN(cfg.screen, 1);
  gbRunSetSound(cfg.sound);
  i2sSetVolume(cfg.volume);
  gbRunSetDispMode(1);                      // ink 모드 (픽셀별 펄스 카운터)
  gbRunSetGray(cfg.screen == 0);
}

static void cfgSave(void)
{
  settings_save_one("game/cfg", &cfg, sizeof(cfg));
  cfgApply();
}




/*---------------------------------------------------------------------------
 * 입력
 *-------------------------------------------------------------------------*/
static uint8_t padKeyBits(const xbox_hid_report_t *p_pad)
{
  uint8_t bits = 0;

  if (p_pad->dpad == 1 || p_pad->dpad == 2 || p_pad->dpad == 8 || p_pad->left_stick_y < 12000) bits |= (1 << KEY_UP);
  if (p_pad->dpad == 4 || p_pad->dpad == 5 || p_pad->dpad == 6 || p_pad->left_stick_y > 53000) bits |= (1 << KEY_DOWN);
  if (p_pad->dpad == 7 || p_pad->left_stick_x < 12000) bits |= (1 << KEY_LEFT);
  if (p_pad->dpad == 3 || p_pad->left_stick_x > 53000) bits |= (1 << KEY_RIGHT);
  if (p_pad->btn_a || p_pad->btn_menu)  bits |= (1 << KEY_OK);
  if (p_pad->btn_b || p_pad->btn_xbox)  bits |= (1 << KEY_BACK);
  return bits;
}

// 현재 눌린 키 (보드 버튼 + 패드 현재 상태). 자동 반복과 "누르고 있는지" 판단에 쓴다
static uint8_t readKeyBits(void)
{
  xbox_hid_report_t pad;
  uint8_t           bits = 0;

  if (buttonGetPressed(_DEF_CH1)) bits |= (1 << KEY_UP);
  if (buttonGetPressed(_DEF_CH2)) bits |= (1 << KEY_DOWN);
  if (buttonGetPressed(_DEF_CH3)) bits |= (1 << KEY_OK);
  if (buttonGetPressed(_DEF_CH4)) bits |= (1 << KEY_BACK);

  if (blePadIsConnected() && blePadGetInfo(&pad)) bits |= padKeyBits(&pad);
  return bits;
}

static ui_key_t padEventKey(uint16_t k)
{
  if (k & BLE_PAD_K_UP)    return KEY_UP;
  if (k & BLE_PAD_K_DOWN)  return KEY_DOWN;
  if (k & BLE_PAD_K_LEFT)  return KEY_LEFT;
  if (k & BLE_PAD_K_RIGHT) return KEY_RIGHT;
  if (k & (BLE_PAD_K_A | BLE_PAD_K_MENU)) return KEY_OK;
  if (k & (BLE_PAD_K_B | BLE_PAD_K_XBOX)) return KEY_BACK;
  return KEY_NONE;
}

// 쌓인 눌림 이벤트 하나 (패드 이벤트 큐, 보드 버튼 래치). 오래된 것은 버린다
static uint8_t btn_pending = 0;
static bool    key_log     = false;           // ui keylog on : 키 이벤트 로그 (씹힘·두 번 입력 확인용)               // 보드 버튼 래치에서 꺼냈지만 아직 안 쓴 비트

static ui_key_t readKeyEvent(void)
{
  uint16_t k;
  uint32_t ms;

  while (blePadPopEvent(BLE_PAD_LATCH_LAUNCHER, &k, &ms))
  {
    if (millis() - ms > KEY_EVENT_MAX_AGE) continue;
    ui_key_t key = padEventKey(k);
    if (key_log) logPrintf("[KEY] pad evt 0x%03X age %u ms -> %d (bounce %u)\n", k, millis() - ms, key, blePadGetBounceCount());
    if (key != KEY_NONE) return key;
  }

  uint32_t bms;
  uint8_t  b = buttonGetPressLatch(&bms);
  if (b && millis() - bms <= KEY_EVENT_MAX_AGE) btn_pending |= b;
  static const ui_key_t btn_key[4] = {KEY_UP, KEY_DOWN, KEY_OK, KEY_BACK};
  for (int i = 0; i < 4; i++)
  {
    if (btn_pending & (1 << i))
    {
      btn_pending &= ~(1 << i);
      return btn_key[i];
    }
  }
  return KEY_NONE;
}

static void flushKeyEvents(void)
{
  blePadFlushEvents(BLE_PAD_LATCH_LAUNCHER);
  buttonGetPressLatch(NULL);
  btn_pending = 0;
}

// 눌림 이벤트 + 위/아래/좌/우 자동 반복
//   눌림은 이벤트로 받는다 (화면 갱신 중 여러 번 누른 것도 횟수대로, 스틱·보드 버튼 포함).
//   자동 반복은 현재 누르고 있는 상태로 판단한다.
static volatile ui_key_t inject_key = KEY_NONE;   // CLI 'ui key' 로 넣은 키 (시험용)
static uint8_t           move_mode  = 0;           // 메뉴 이동 갱신: 0 = FAST 파형, 1 = 공장 부분 갱신 (ui mode)
#define MENU_FAST_FRAMES 45                         // FAST 파형 길이 (8ms 프레임). 카메라 비교: 10 은 지나간 줄에 회색 바,
                                                    //   45(0.41초) 는 깨끗함, 공장 부분 갱신 0.6초

static uint32_t press_ms  = 0;
static uint32_t repeat_ms = 0;

static ui_key_t readKey(void)
{
  if (inject_key != KEY_NONE)
  {
    ui_key_t k = inject_key;
    inject_key = KEY_NONE;
    return k;
  }

  ui_key_t key = readKeyEvent();
  if (key != KEY_NONE)
  {
    press_ms  = millis();
    repeat_ms = press_ms;
    return key;
  }

  uint8_t bits = readKeyBits();
  if (bits & ((1 << KEY_UP) | (1 << KEY_DOWN) | (1 << KEY_LEFT) | (1 << KEY_RIGHT)))
  {
    if (millis() - press_ms >= REPEAT_DELAY_MS && millis() - repeat_ms >= REPEAT_RATE_MS)
    {
      repeat_ms = millis();
      for (int k = KEY_UP; k <= KEY_RIGHT; k++)
        if (bits & (1 << k))
        {
          if (key_log) logPrintf("[KEY] repeat %d (held %u ms)\n", k, millis() - press_ms);
          return (ui_key_t)k;
        }
    }
  }
  else
  {
    press_ms = millis();      // 뗐으면 반복 타이머 초기화
  }
  return KEY_NONE;
}

// 모든 키를 뗄 때까지 대기 (게임 → 메뉴 전환 시 길게 누른 BTN4 등이 바로 입력되지 않도록)
static void waitKeyRelease(void)
{
  for (int i = 0; i < 300 && readKeyBits() != 0; i++) delay(10);
  flushKeyEvents();
}




/*---------------------------------------------------------------------------
 * 화면 그리기
 *-------------------------------------------------------------------------*/
static void text(int col, int row, bool inv, const char *fmt, ...)
{
  char    buf[UI_COLS + 1];
  va_list arg;

  va_start(arg, fmt);
  vsnprintf(buf, sizeof(buf), fmt, arg);
  va_end(arg);

  int x = UI_X0 + col * UI_CW;
  int y = UI_Y0 + row * UI_CH;

  if (inv)
  {
    canvasFillRect(x, y, canvasGetStrWidth(buf), UI_CH, CANVAS_BLACK);
    canvasPrintf(x, y, CANVAS_WHITE, "%s", buf);
  }
  else
  {
    canvasPrintf(x, y, CANVAS_BLACK, "%s", buf);
  }
}

static void bar(int row, int col, int w, bool inv)
{
  canvasFillRect(UI_X0 + col * UI_CW, UI_Y0 + row * UI_CH, w * UI_CW, UI_CH,
                 inv ? CANVAS_BLACK : CANVAS_WHITE);
}

static void box(int col, int row, int w, int h, const char *title)
{
  int x = UI_X0 + col * UI_CW;
  int y = UI_Y0 + row * UI_CH + UI_CH / 2;

  canvasDrawRect(x + 2, y, w * UI_CW - 4, h * UI_CH - UI_CH / 2, CANVAS_BLACK);
  canvasDrawRect(x + 4, y + 2, w * UI_CW - 8, h * UI_CH - UI_CH / 2 - 4, CANVAS_BLACK);
  if (title)
  {
    canvasFillRect(x + 12, y - 2, (strlen(title) + 2) * UI_CW, 6, CANVAS_WHITE);
    text(col + 2, row, false, " %s ", title);
  }
}

static void drawTopBar(const char *title)
{
  char    pad[24];
  int8_t  bat = blePadGetBattery();

  switch (blePadGetState())
  {
    case BLE_PAD_STATE_CONNECTED:  snprintf(pad, sizeof(pad), "PAD:ON %d%%", bat >= 0 ? bat : 0); break;
    case BLE_PAD_STATE_SCANNING:   snprintf(pad, sizeof(pad), "PAD:SCAN");    break;
    case BLE_PAD_STATE_CONNECTING: snprintf(pad, sizeof(pad), "PAD:CONN..");  break;
    default:                       snprintf(pad, sizeof(pad), "PAD:OFF");     break;
  }

  bar(0, 0, UI_COLS, true);
  text(1, 0, true, "NU40DK GAME BOY > %s", title);
  text(UI_COLS - 1 - strlen(pad), 0, true, "%s", pad);
}

static void drawBottomBar(const char *hint)
{
  canvasDrawHLine(0, UI_Y0 + 17 * UI_CH - 3, 400, CANVAS_BLACK);
  text(1, 17, false, "%s", hint);
}

static void present(void)
{
  if (need_strong)
  {
    epdUpdateStrong();
    fast_cnt    = 0;
    need_full   = false;
    need_strong = false;
  }
  else if (need_full || fast_cnt >= FAST_PER_FULL)
  {
    // 화면 전환·주기 정리: 저온(10°C) 흑백 전체 갱신 (약 1.25초).
    //   0xFF(현재 온도, 0.67초) 는 이 패널에서 검정이 흐리고 이전 화면이 비친다.
    //   3색 강한 갱신(F7) 은 선명하지만 2.8초가 걸린다.
    //   같은 이미지로 부분 갱신을 반복하는 방법은 과구동된 픽셀이 다음 흰색 전환에서
    //   돌아오지 않아 잔상이 남았다 (실험 결과).
    epdUpdateCold(LAUNCHER_COLD_TEMP);
    fast_cnt  = 0;
    need_full = false;
  }
  else
  {
    // 메뉴 이동: 커스텀 FAST 웨이브폼 (약 0.1초, 바뀐 픽셀만 구동, hold 0).
    //   공장 부분 갱신(0.6초)은 반응이 한 박자 늦게 보였다. 예전 FAST 의 패널 잔상은 바뀌지 않은
    //   픽셀까지 구동한 hold 때문이었고 지금은 hold 0. 검정이 덜 진한 것은 10회마다·멈춘 뒤 5초의
    //   저온 전체 갱신이 정리한다.
    epdUpdate(move_mode == 0 ? EPD_MODE_FAST : EPD_MODE_PART);
    fast_cnt++;
  }
  epd_on       = true;
  last_draw_ms = millis();
}

static void menuList(int col, int row, int w, const char **items, int cnt, int cur)
{
  for (int i = 0; i < cnt; i++)
  {
    if (i == cur)
    {
      bar(row + i, col, w, true);
      text(col, row + i, true, " > %s", items[i]);
    }
    else
    {
      text(col, row + i, false, "   %s", items[i]);
    }
  }
}




/*---------------------------------------------------------------------------
 * 각 화면
 *-------------------------------------------------------------------------*/
static const char *main_items[] = {"EMUL (Game Boy)", "GAMES", "CONTROLLER", "SETTINGS", "INFO"};
#define MAIN_CNT 5

static void drawMain(void)
{
  drawTopBar("MAIN");
  box(1, 1, 48, 15, "MENU");
  menuList(4, 3, 42, main_items, MAIN_CNT, sel[SCR_MAIN]);

  text(4, 9,  false, "ROM in flash : ");
  {
    char title[16];
    gbRomGetTitle(title, sizeof(title));
    text(19, 9, false, "%s", gbRomIsValid() ? title : "-");
  }
  text(4, 10, false, "SD card      : %s", sdIsInit() ? "OK" : "NONE");
  text(4, 12, false, "Game: Xbox / BTN4 long = menu");
  drawBottomBar("[1]UP [2]DOWN [3]OK         Pad: D-pad A B");
}

static bool listCb(const char *p_name, uint32_t size, void *arg)
{
  if (rom_cnt >= LIST_MAX) return false;
  snprintf(rom_list[rom_cnt], NAME_MAX, "%s", p_name);
  rom_size[rom_cnt] = size;
  rom_cnt++;
  return true;
}

static void loadRomList(void)
{
  rom_cnt = 0;
  gbRomList(listCb, NULL);
  if (sel[SCR_GAMES] >= rom_cnt) sel[SCR_GAMES] = 0;
}

static const char *mbcStr(uint8_t type)
{
  if (type == 0x00) return "ROM ONLY";
  if (type <= 0x03) return "MBC1";
  if (type <= 0x06) return "MBC2";
  if (type >= 0x0F && type <= 0x13) return "MBC3";
  if (type >= 0x19 && type <= 0x1E) return "MBC5";
  return "UNKNOWN";
}

#define GAMES_ROWS    14

static void drawGames(void)
{
  int cur = sel[SCR_GAMES];

  drawTopBar("EMUL");
  box(0, 1, 28, 16, "ROMS");
  box(28, 1, 22, 16, "INFO");

  if (cur < scroll) scroll = cur;
  if (cur >= scroll + GAMES_ROWS) scroll = cur - GAMES_ROWS + 1;

  if (rom_cnt == 0)
  {
    text(2, 3, false, "no rom in /roms");
  }
  for (int i = 0; i < GAMES_ROWS && scroll + i < rom_cnt; i++)
  {
    int  idx = scroll + i;
    char name[25];

    snprintf(name, sizeof(name), "%s", rom_list[idx]);
    if (idx == cur)
    {
      bar(2 + i, 1, 26, true);
      text(1, 2 + i, true, ">%s", name);
    }
    else
    {
      text(1, 2 + i, false, " %s", name);
    }
  }

  if (rom_cnt > 0)
  {
    uint8_t  hdr[0x150];
    uint32_t size;

    if (gbRomReadHeader(rom_list[cur], hdr, &size))
    {
      char title[17] = {0};

      for (int i = 0; i < 16; i++)
      {
        char c = hdr[0x134 + i];
        if (c == 0 || (uint8_t)c >= 0x80) break;
        title[i] = c;
      }
      text(30, 3,  false, "%s", title);
      text(30, 5,  false, "SIZE : %d KB", size / 1024);
      text(30, 6,  false, "MBC  : %s", mbcStr(hdr[0x147]));
      text(30, 7,  false, "TYPE : %s", hdr[0x143] == 0xC0 ? "GBC ONLY" : hdr[0x143] == 0x80 ? "GB+GBC" : "GB");
      text(30, 8,  false, "SAVE : %s", gbSaveExists(rom_list[cur]) ? "YES" : "-");
      if (hdr[0x143] == 0xC0)
      {
        text(30, 10, false, "! color only");
        text(30, 11, false, "  may not run");
      }
    }
  }
  drawBottomBar("[1]UP [2]DOWN [3]PLAY [4]BACK");
}

static const char *ctrl_items[] = {"SCAN ON/OFF", "DISCONNECT", "CLEAR PAIRING"};

static void drawController(void)
{
  int8_t bat = blePadGetBattery();

  drawTopBar("CONTROLLER");
  box(1, 1, 48, 15, "XBOX PAD");
  text(4, 3, false, "STATE   : %s", blePadGetStateStr());
  text(4, 4, false, "SCAN    : %s", blePadIsScanEnabled() ? "ON" : "OFF");
  text(4, 5, false, "PAIRED  : %s", blePadHasBond() ? "YES" : "NO");
  text(4, 6, false, "BATTERY : %s", bat >= 0 ? "" : "-");
  if (bat >= 0) text(14, 6, false, "%d %%", bat);
  text(4, 7, false, "RSSI    : %d dBm", blePadGetRssi());

  menuList(4, 9, 42, ctrl_items, 3, sel[SCR_CONTROLLER]);
  text(4, 13, false, "pairing: hold the pad's pair button");
  drawBottomBar("[1]UP [2]DOWN [3]OK [4]BACK");
}

static void drawSettings(void)
{
  char        items[5][40];
  const char *p_items[5];

  snprintf(items[0], 40, "SOUND    : %s", cfg.sound ? "ON" : "OFF");
  snprintf(items[1], 40, "VOLUME   : %d", cfg.volume);
  snprintf(items[2], 40, "SCREEN   : %s", screen_str[cfg.screen]);
  snprintf(items[3], 40, "CLEAN SCREEN");
  snprintf(items[4], 40, "WHITE SCREEN");
  for (int i = 0; i < 5; i++) p_items[i] = items[i];

  drawTopBar("SETTINGS");
  box(1, 1, 48, 15, "SETTINGS");
  menuList(4, 3, 42, p_items, 5, sel[SCR_SETTINGS]);
  text(4, 10, false, "SCREEN: game screen drawing");
  text(4, 11, false, "  GRAY = smooth shades, DITHER = dots");
  text(4, 12, false, "WHITE : blank panel (rest/storage)");
  text(4, 14, false, "LEFT/RIGHT or OK to change");
  drawBottomBar("[1]UP [2]DOWN [3]CHANGE [4]BACK");
}

static void drawInfo(void)
{
  char title[16];

  gbRomGetTitle(title, sizeof(title));
  drawTopBar("INFO");
  box(1, 1, 48, 15, "SYSTEM");
  text(4, 3,  false, "BOARD    : %s", _DEF_BOARD_NAME);
  text(4, 4,  false, "FIRMWARE : %s", _DEF_FIRMWATRE_VERSION);
  text(4, 5,  false, "BUILD    : %s", __DATE__);
  text(4, 6,  false, "MCU      : nRF52840 64MHz");
  text(4, 7,  false, "EMULATOR : %s", gbCoreName());
  text(4, 8,  false, "DISPLAY  : SSD1683 400x300");
  text(4, 10, false, "FLASH ROM: %s", gbRomIsValid() ? title : "-");
  text(4, 11, false, "SD CARD  : %s, %d roms", sdIsInit() ? "OK" : "NONE", rom_cnt);
  text(4, 12, false, "UPTIME   : %d s", millis() / 1000);
  drawBottomBar("[4]BACK");
}

// 네이티브 게임 목록 (app_api.h)
static const app_def_t *const app_list[] = { &rpg_app };
#define APP_CNT ((int)(sizeof(app_list) / sizeof(app_list[0])))

static void drawApps(void)
{
  drawTopBar("GAMES");
  box(1, 1, 48, 15, "NATIVE GAMES");
  for (int i = 0; i < APP_CNT; i++)
  {
    bool cur = (i == sel[SCR_APPS]);
    if (cur) bar(3 + i, 4, 42, true);
    text(4, 3 + i, cur, "%s%s", cur ? " > " : "   ", app_list[i]->name);
  }
  text(4, 12, false, "Pad: D-pad move, A talk, B menu");
  drawBottomBar("[1]UP [2]DOWN [3]RUN [4]BACK");
}

static void draw(void)
{
  canvasClear(CANVAS_WHITE);
  switch (screen)
  {
    case SCR_MAIN:       drawMain();       break;
    case SCR_GAMES:      drawGames();      break;
    case SCR_CONTROLLER: drawController(); break;
    case SCR_SETTINGS:   drawSettings();   break;
    case SCR_INFO:       drawInfo();       break;
    case SCR_APPS:       drawApps();       break;
  }
  present();
}




/*---------------------------------------------------------------------------
 * 게임 실행 / 게임 메뉴
 *-------------------------------------------------------------------------*/
static void drawMessage(const char *title, const char *msg)
{
  canvasClear(CANVAS_WHITE);
  drawTopBar(title);
  box(8, 6, 34, 5, NULL);
  text(10, 8, false, "%s", msg);
  present();
}

static void loadProgress(uint32_t done, uint32_t total)
{
  static uint32_t pre_ms = 0;

  // 진행 막대는 0.7 초마다 FAST 갱신 (Flash 쓰기를 크게 늦추지 않도록)
  if (done < total && millis() - pre_ms < 700) return;
  pre_ms = millis();

  int w = 30 * UI_CW;
  int x = (400 - w) / 2;
  int y = UI_Y0 + 9 * UI_CH;

  canvasFillRect(x, y, w, 12, CANVAS_WHITE);
  canvasDrawRect(x, y, w, 12, CANVAS_BLACK);
  canvasFillRect(x + 2, y + 2, (w - 4) * done / total, 8, CANVAS_BLACK);
  epdUpdate(EPD_MODE_PART);
}

static void saveGame(void)
{
  uint32_t size = gbGetSaveSize();

  if (size > 0 && cur_rom[0] != 0)
  {
    bool ok = gbSaveWrite(cur_rom, gbGetCartRam(), size);
    logPrintf("[%s] save %s (%d bytes)\n", ok ? "OK" : "E_", cur_rom, size);
  }
}

static void runGame(int index)
{
  gb_rom_err_t err;
  char         msg[48];

  if (index >= rom_cnt) return;

  snprintf(cur_rom, sizeof(cur_rom), "%s", rom_list[index]);
  need_full = true;
  drawMessage("LOADING", cur_rom);

  err = gbRomLoad(cur_rom, false, loadProgress);
  if (err != GB_ROM_OK && err != GB_ROM_SAME)
  {
    snprintf(msg, sizeof(msg), "load fail: %s", gbRomErrStr(err));
    drawMessage("ERROR", msg);
    delay(2000);
    need_full = true;
    return;
  }

  // 배터리 세이브가 있으면 불러온다
  gbClearCartRam();
  if (gbGetSaveSize() > 0)
  {
    bool ok = gbSaveRead(cur_rom, gbGetCartRam(), gbGetSaveSize());
    logPrintf("[%s] load save %s\n", ok ? "OK" : "--", cur_rom);
  }

  epdPowerOff();
  epd_on = false;
  cfgApply();
  if (!gbRunStartEx(false))
  {
    drawMessage("ERROR", "rom start fail");
    delay(2000);
    need_full = true;
  }
}

static const char *game_items[] = {"RESUME", "SAVE & QUIT", "RESET", "QUIT (NO SAVE)"};

// 게임 중 메뉴 버튼 → 게임 화면 위에 메뉴 상자. 반환: true = 게임 종료
static bool gameMenu(void)
{
  int16_t gx, gy, gw, gh;
  int     cur = 0;
  bool    redraw = true;

  gbRunWaitDisplayIdle();
  gbRunGetScreenRect(&gx, &gy, &gw, &gh);
  waitKeyRelease();

  const int w = 26 * UI_CW, h = 7 * UI_CH;
  const int x = gx + (gw - w) / 2;
  const int y = gy + (gh - h) / 2;

  while (1)
  {
    if (redraw)
    {
      redraw = false;
      canvasFillRect(x, y, w, h, CANVAS_WHITE);
      canvasDrawRect(x, y, w, h, CANVAS_BLACK);
      canvasDrawRect(x + 2, y + 2, w - 4, h - 4, CANVAS_BLACK);
      canvasPrintf(x + 16, y + 6, CANVAS_BLACK, "PAUSE");
      for (int i = 0; i < 4; i++)
      {
        int ty = y + 6 + (i + 1) * UI_CH + 4;
        if (i == cur)
        {
          canvasFillRect(x + 8, ty, w - 16, UI_CH, CANVAS_BLACK);
          canvasPrintf(x + 16, ty, CANVAS_WHITE, "> %s", game_items[i]);
        }
        else
        {
          canvasPrintf(x + 16, ty, CANVAS_BLACK, "  %s", game_items[i]);
        }
      }
      gbRunDrawOverlay(y, h);               // 메뉴 상자만 구동 (바깥 게임 화면은 건드리지 않음)
    }

    ui_key_t key = readKey();
    switch (key)
    {
      case KEY_UP:   cur = (cur + 3) % 4; redraw = true; break;
      case KEY_DOWN: cur = (cur + 1) % 4; redraw = true; break;
      case KEY_BACK:
        cur = 0;
        /* fall through */
      case KEY_OK:
        switch (cur)
        {
          case 0:                                   // RESUME
            waitKeyRelease();
            gbRunRequestFull();
            gbRunSetPause(false);
            return false;
          case 1:                                   // SAVE & QUIT
            gbRunStop();
            saveGame();
            return true;
          case 2:                                   // RESET (세이브는 다시 불러옴)
            gbRunStop();
            saveGame();
            gbClearCartRam();
            if (gbGetSaveSize() > 0) gbSaveRead(cur_rom, gbGetCartRam(), gbGetSaveSize());
            waitKeyRelease();
            gbRunStartEx(false);
            return false;
          default:                                  // QUIT (NO SAVE)
            gbRunStop();
            return true;
        }
      default:
        break;
    }
    delay(20);
  }
}




/*---------------------------------------------------------------------------
 * 키 처리
 *-------------------------------------------------------------------------*/
// 전체 흰 화면: 패널을 쉬게 하거나 보관할 때. 아무 키나 누르면 설정 화면으로 돌아간다
static void whiteScreen(void)
{
  canvasClear(CANVAS_WHITE);
  epdUpdateStrong();
  epdPowerOff();
  epd_on = false;

  waitKeyRelease();
  while (readKey() == KEY_NONE) delay(20);
  waitKeyRelease();

  fast_cnt  = 0;
  need_full = true;
}


/*---------------------------------------------------------------------------
 * 네이티브 앱 (app_api.h): 런처 기능을 함수 포인터로 넘긴다
 *-------------------------------------------------------------------------*/
static app_key_t apiReadKey(void)            { return (app_key_t)readKey(); }
static void      apiWaitKeyRelease(void)     { waitKeyRelease(); }
static uint8_t   apiKeyHeld(void)            { return readKeyBits(); }
static uint8_t   app_quick_frames = 8;       // 걷기용 짧은 FAST 파형 길이 (ui quick N)
static uint8_t   app_lut_frames   = 0;
static bool      app_fast_part    = false;   // 앱의 FAST(메뉴·대화창) 를 공장 부분 갱신(0.6초)으로 (ui dlg part|fast)
static bool      app_clean_strong = true;    // 앱의 CLEAN 갱신: true = 3색 강한 갱신(F7, 2.8초), false = 저온(1.3초)
                                             //   저온(0xDF) 뒤에는 커스텀 펄스가 약 절반 세기라 걷기·대화창 자리에 잔상이 남았다
static void      apiPowerOff(void)           { epdPowerOff(); }
static void      apiClear(app_color_t c)     { canvasClear(c == APP_BLACK ? CANVAS_BLACK : CANVAS_WHITE); }
static void      apiFillRect(int16_t x, int16_t y, int16_t w, int16_t h, app_color_t c)
{
  canvasFillRect(x, y, w, h, c == APP_BLACK ? CANVAS_BLACK : CANVAS_WHITE);
}
static void      apiDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, app_color_t c)
{
  canvasDrawRect(x, y, w, h, c == APP_BLACK ? CANVAS_BLACK : CANVAS_WHITE);
}
static int16_t   apiPrint(int16_t x, int16_t y, app_color_t c, const char *fmt, ...)
{
  char    buf[128];
  va_list arg;
  va_start(arg, fmt);
  vsnprintf(buf, sizeof(buf), fmt, arg);
  va_end(arg);
  return canvasPrintf(x, y, c == APP_BLACK ? CANVAS_BLACK : CANVAS_WHITE, "%s", buf);
}
static uint16_t  apiStrWidth(const char *str) { return canvasGetStrWidth(str); }
static void      apiBeep(uint32_t hz, uint32_t ms)
{
  if (cfg.sound) i2sPlayBeep(hz, cfg.volume / 2, ms);
}
static bool      apiVoicePlay(const char *path) { return cfg.sound ? voicePlay(path) : false; }
static void      apiVoiceStop(void)          { voiceStop(); }
static bool      apiVoiceIsPlaying(void)     { return voiceIsPlaying(); }
static uint32_t  apiMillis(void)             { return millis(); }
static void      apiDelay(uint32_t ms)       { delay(ms); }
static void      apiLog(const char *fmt, ...)
{
  char    buf[128];
  va_list arg;
  va_start(arg, fmt);
  vsnprintf(buf, sizeof(buf), fmt, arg);
  va_end(arg);
  logPrintf("%s", buf);
}
static void      apiRefresh(app_refresh_t mode)
{
  if (mode == APP_REFRESH_STRONG)     epdUpdateStrong();
  else if (mode == APP_REFRESH_FLASH)
  {
    if (app_lut_frames != MENU_FAST_FRAMES)
    {
      epdSetFastLut(MENU_FAST_FRAMES, 0);
      app_lut_frames = MENU_FAST_FRAMES;
    }
    epdUpdateFlash();
  }
  else if (mode == APP_REFRESH_CLEAN)
  {
    if (app_clean_strong) epdUpdateStrong();
    else                  epdUpdateCold(LAUNCHER_COLD_TEMP);
  }
  else if (mode == APP_REFRESH_FAST && app_fast_part)
  {
    epdUpdate(EPD_MODE_PART);
    app_lut_frames = 0;                       // 공장 LUT 가 커스텀 LUT 를 덮어씀
  }
  else
  {
    uint8_t frames = (mode == APP_REFRESH_QUICK) ? app_quick_frames : MENU_FAST_FRAMES;
    if (frames != app_lut_frames)
    {
      epdSetFastLut(frames, 0);
      app_lut_frames = frames;
    }
    epdUpdate(EPD_MODE_FAST);
  }
}

static void      apiRefreshRect(app_refresh_t mode, int16_t x, int16_t y, int16_t w, int16_t h)
{
  if (mode == APP_REFRESH_STRONG || mode == APP_REFRESH_CLEAN || w <= 0 || h <= 0)
  {
    apiRefresh(mode);
    return;
  }
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > CANVAS_WIDTH)  w = CANVAS_WIDTH - x;
  if (y + h > CANVAS_HEIGHT) h = CANVAS_HEIGHT - y;

  uint8_t frames = (mode == APP_REFRESH_QUICK) ? app_quick_frames : MENU_FAST_FRAMES;
  if (frames != app_lut_frames)
  {
    epdSetFastLut(frames, 0);
    app_lut_frames = frames;
  }
  epdUpdateWindow(EPD_MODE_FAST, x, y, w, h);
}

static void      apiRefreshFrom(const uint8_t *p_old, int16_t y, int16_t h)
{
  if (y < 0 || h <= 0 || y + h > CANVAS_HEIGHT) return;
  if (app_fast_part) return;                  // 공장 부분 갱신은 한 번이면 된다
  if (app_lut_frames != MENU_FAST_FRAMES)
  {
    epdSetFastLut(MENU_FAST_FRAMES, 0);
    app_lut_frames = MENU_FAST_FRAMES;
  }
  epdSetOldImage(p_old, y, h);
  epdUpdateWindow(EPD_MODE_FAST, 0, y, CANVAS_WIDTH, h);
}

static char        app_dbg_buf[48];
static volatile bool app_dbg_ready = false;

static const char *apiDebugCmd(void)
{
  static char cmd[48];
  if (!app_dbg_ready) return NULL;
  memcpy(cmd, app_dbg_buf, sizeof(cmd));
  app_dbg_ready = false;
  return cmd;
}

static void runApp(const app_def_t *app)
{
  static const app_api_t api =
  {
    .version        = APP_API_VERSION,
    .width          = CANVAS_WIDTH,
    .height         = CANVAS_HEIGHT,
    .stride         = CANVAS_STRIDE,
    .fb             = NULL,
    .readKey        = apiReadKey,
    .waitKeyRelease = apiWaitKeyRelease,
    .keyHeld        = apiKeyHeld,
    .refresh        = apiRefresh,
    .refreshRect    = apiRefreshRect,
    .refreshFrom    = apiRefreshFrom,
    .powerOff       = apiPowerOff,
    .clear          = apiClear,
    .fillRect       = apiFillRect,
    .drawRect       = apiDrawRect,
    .print          = apiPrint,
    .strWidth       = apiStrWidth,
    .beep           = apiBeep,
    .voicePlay      = apiVoicePlay,
    .voiceStop      = apiVoiceStop,
    .voiceIsPlaying = apiVoiceIsPlaying,
    .millis         = apiMillis,
    .delay          = apiDelay,
    .log            = apiLog,
    .debugCmd       = apiDebugCmd,
  };
  app_api_t run_api = api;

  run_api.fb = canvasGetBuffer();
  {
    uint32_t size = 0;
    uint8_t *p    = gbRunGetScratch(&size);
    run_api.scratch      = p;
    run_api.scratch_size = p != NULL && size > 12800 ? 12800 : 0;   // 뒤쪽은 음성 재생 버퍼 (voice.c)
  }
  logPrintf("[  ] app start: %s\n", app->name);
  app->main(&run_api);
  logPrintf("[  ] app end: %s\n", app->name);
  voiceStop();

  epdSetFastLut(MENU_FAST_FRAMES, 0);   // 런처 메뉴 파형으로 되돌림
  app_lut_frames = 0;

  epdPowerOff();
  epd_on      = false;
  need_strong = true;
  waitKeyRelease();
}

static void changeScreen(screen_t next)
{
  screen    = next;
  need_full = true;
  if (next == SCR_GAMES) { loadRomList(); scroll = 0; }
}

static bool handleKey(ui_key_t key)
{
  int *p_sel = &sel[screen];

  switch (screen)
  {
    case SCR_MAIN:
      if (key == KEY_UP)   { *p_sel = (*p_sel + MAIN_CNT - 1) % MAIN_CNT; return true; }
      if (key == KEY_DOWN) { *p_sel = (*p_sel + 1) % MAIN_CNT; return true; }
      if (key == KEY_OK)
      {
        if (*p_sel == 0)      changeScreen(SCR_GAMES);
        else if (*p_sel == 1) changeScreen(SCR_APPS);
        else                  changeScreen((screen_t)(SCR_CONTROLLER + *p_sel - 2));
        return true;
      }
      break;

    case SCR_GAMES:
      if (key == KEY_BACK) { changeScreen(SCR_MAIN); return true; }
      if (rom_cnt == 0) break;
      if (key == KEY_UP)   { *p_sel = (*p_sel + rom_cnt - 1) % rom_cnt; return true; }
      if (key == KEY_DOWN) { *p_sel = (*p_sel + 1) % rom_cnt; return true; }
      if (key == KEY_OK)   { runGame(*p_sel); return true; }
      break;

    case SCR_CONTROLLER:
      if (key == KEY_BACK) { changeScreen(SCR_MAIN); return true; }
      if (key == KEY_UP)   { *p_sel = (*p_sel + 2) % 3; return true; }
      if (key == KEY_DOWN) { *p_sel = (*p_sel + 1) % 3; return true; }
      if (key == KEY_OK)
      {
        if (*p_sel == 0) { if (blePadIsScanEnabled()) blePadScanStop(); else blePadScanStart(); }
        if (*p_sel == 1) blePadDisconnect();
        if (*p_sel == 2) blePadClearBonds();
        delay(300);
        return true;
      }
      break;

    case SCR_SETTINGS:
      if (key == KEY_BACK) { changeScreen(SCR_MAIN); return true; }
      if (key == KEY_UP)   { *p_sel = (*p_sel + 4) % 5; return true; }
      if (key == KEY_DOWN) { *p_sel = (*p_sel + 1) % 5; return true; }
      if (key == KEY_OK && *p_sel == 4)
      {
        whiteScreen();
        return true;
      }
      if (key == KEY_OK || key == KEY_LEFT || key == KEY_RIGHT)
      {
        int dir = key == KEY_LEFT ? -1 : 1;
        switch (*p_sel)
        {
          case 0: cfg.sound = !cfg.sound; break;
          case 1: cfg.volume = (uint8_t)constrain((int)cfg.volume + dir * 10, 0, 100);
                  if (key == KEY_OK && cfg.volume == 100) cfg.volume = 0;
                  i2sSetVolume(cfg.volume);
                  i2sPlayBeep(1000, 100, 60);
                  break;
          case 2: cfg.screen = !cfg.screen; break;
          case 3:
            drawMessage("SETTINGS", "cleaning screen...");
            epdClean(2);
            need_full = true;
            break;
        }
        cfgSave();
        return true;
      }
      break;

    case SCR_INFO:
      if (key == KEY_BACK || key == KEY_OK) { changeScreen(SCR_MAIN); return true; }
      break;

    case SCR_APPS:
      if (key == KEY_BACK) { changeScreen(SCR_MAIN); return true; }
      if (key == KEY_UP)   { *p_sel = (*p_sel + APP_CNT - 1) % APP_CNT; return true; }
      if (key == KEY_DOWN) { *p_sel = (*p_sel + 1) % APP_CNT; return true; }
      if (key == KEY_OK)   { runApp(app_list[*p_sel]); return true; }
      break;
  }
  return false;
}




/*---------------------------------------------------------------------------
 * 메인 루프
 *-------------------------------------------------------------------------*/
#ifdef _USE_HW_CLI
// 런처 시험용: ui key up|down|left|right|ok|back, ui mode fast|part
static void cliUi(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 2 && args->isStr(0, "key"))
  {
    const char *names[] = {"up", "down", "left", "right", "ok", "back"};
    const ui_key_t keys[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_OK, KEY_BACK};
    for (int i = 0; i < 6; i++)
      if (args->isStr(1, names[i])) { inject_key = keys[i]; ret = true; }
    if (ret) cliPrintf("ui key %s\n", args->getStr(1));
  }
  if (args->argc >= 1 && args->isStr(0, "mode"))
  {
    if (args->argc >= 2) move_mode = args->isStr(1, "fast") ? 0 : 1;
    cliPrintf("ui mode %s\n", move_mode == 0 ? "fast" : "part");
    ret = true;
  }
  if (args->argc >= 2 && (args->isStr(0, "app") || args->isStr(0, "rpg")))
  {
    // ui rpg <명령...> : 실행 중인 앱에 시험 명령 전달 (RPG: ch N, room R [x y], heal, state)
    int n = 0;
    app_dbg_buf[0] = 0;
    for (int i = 1; i < args->argc && n < (int)sizeof(app_dbg_buf) - 1; i++)
      n += snprintf(&app_dbg_buf[n], sizeof(app_dbg_buf) - n, "%s%s", i > 1 ? " " : "", args->getStr(i));
    app_dbg_ready = true;
    cliPrintf("app cmd: %s (지도 화면에서 실행)\n", app_dbg_buf);
    ret = true;
  }
  if (args->argc == 2 && args->isStr(0, "voice"))
  {
    // ui voice <파일 또는 8자리 번호> : SD 음성 재생 시험 (번호면 /rpg/v/<번호>.WAV)
    char path[64];
    const char *arg = args->getStr(1);
    if (strlen(arg) == 8 && strchr(arg, '/') == NULL) snprintf(path, sizeof(path), "/rpg/v/%s.WAV", arg);
    else                                             snprintf(path, sizeof(path), "%s", arg);
    bool ok = voicePlay(path);
    cliPrintf("voice %s : %s\n", path, ok ? "playing" : "fail (파일·형식 확인)");
    ret = true;
  }
  if (args->argc == 2 && args->isStr(0, "keylog"))
  {
    key_log = args->isStr(1, "on");
    blePadSetLog(key_log);
    cliPrintf("keylog %s\n", key_log ? "on" : "off");
    ret = true;
  }
  if (args->argc >= 1 && args->isStr(0, "quick"))
  {
    if (args->argc >= 2) app_quick_frames = (uint8_t)constrain(args->getData(1), 4, 60);
    cliPrintf("app quick frames %d (%d ms)\n", app_quick_frames, app_quick_frames * 8);
    ret = true;
  }
  if (args->argc >= 1 && args->isStr(0, "dlg"))
  {
    if (args->argc >= 2) app_fast_part = args->isStr(1, "part");
    cliPrintf("app dialog/menu refresh %s\n", app_fast_part ? "part (factory 0.6s)" : "fast (custom)");
    ret = true;
  }
  if (args->argc >= 1 && args->isStr(0, "clean"))
  {
    if (args->argc >= 2) app_clean_strong = args->isStr(1, "strong");
    cliPrintf("app clean %s\n", app_clean_strong ? "strong" : "cold");
    ret = true;
  }
  if (args->argc == 1 && args->isStr(0, "shot"))
  {
    // 화면 버퍼 덤프 (카메라 없이 확인): 줄마다 "S<y> <hex 50 바이트>"
    uint8_t *fb = canvasGetBuffer();
    for (int y = 0; y < CANVAS_HEIGHT; y++)
    {
      cliPrintf("S%03d ", y);
      for (int i = 0; i < CANVAS_STRIDE; i++) cliPrintf("%02X", fb[y * CANVAS_STRIDE + i]);
      cliPrintf("\n");
      if ((y & 15) == 15) delay(5);
    }
    cliPrintf("shot end\n");
    ret = true;
  }
  if (!ret)
  {
    cliPrintf("ui key up|down|left|right|ok|back\n");
    cliPrintf("ui shot\n");
    cliPrintf("ui rpg ch N | room R [x y] | heal | state\n");
    cliPrintf("ui mode fast|part\n");
  }
}
#endif

void launcherMain(void)
{
#ifdef _USE_HW_CLI
  cliAdd("ui", cliUi);
#endif
  epdSetFastLut(MENU_FAST_FRAMES, 0);
  // BT 모듈이 settings_load() 를 끝낼 때까지 잠시 대기 후 설정 적용
  delay(500);
  cfgApply();
  loadRomList();
  draw();

  while (1)
  {
    // CLI 캘리브레이션·테스트 패턴 중에는 화면을 건드리지 않는다
    if (gbRunIsExtBusy() || gbRunIsPattern())
    {
      delay(20);
      continue;
    }
    if (gbRunIsRunning())
    {
      flushKeyEvents();                   // 게임 중 패드 입력은 런처 큐에 쌓아 두지 않는다 (메뉴에서 뒤늦게 적용 방지)
      // 게임 중: 메뉴 요청만 처리
      if (gbRunGetMenuRequest())
      {
        if (gbRunIsPaused() && gameMenu())
        {
          epdPowerOff();
          need_strong = true;
          changeScreen(SCR_GAMES);
          draw();
        }
      }
      delay(20);
      continue;
    }

    ui_key_t key = readKey();
    if (key != KEY_NONE)
    {
      bool     changed = handleKey(key);
      screen_t scr     = screen;

      // 갱신 중 쌓인 키를 더 처리하고 한 번만 그린다 (빠르게 여러 번 누르면 그만큼 이동)
      while (!gbRunIsRunning() && screen == scr && (key = readKeyEvent()) != KEY_NONE)
      {
        changed |= handleKey(key);
      }
      if (gbRunIsRunning()) continue;     // 게임 시작됨
      if (changed) draw();
    }

    // 상단 바의 패드 상태는 5 초마다 갱신 (변화가 있을 때만)
    static ble_pad_state_t pre_state = BLE_PAD_STATE_OFF;
    static int8_t          pre_bat   = -2;
    if (millis() - status_ms >= 5000)
    {
      status_ms = millis();
      if (blePadGetState() != pre_state || blePadGetBattery() != pre_bat)
      {
        pre_state = blePadGetState();
        pre_bat   = blePadGetBattery();
        // 상단 바는 검정 바 위 흰 글자라 부분 갱신으로 바꾸면 이전 글자가 겹쳐 보인다 → 깨끗한 갱신
        need_full = true;
        draw();
      }
    }

    // 부분 갱신은 흰색 그대로인 배경을 구동하지 않아 시간이 지나면 점이 생긴다.
    // 메뉴를 멈추고 잠시 지나면 전체 갱신으로 정리한다.
    if (fast_cnt > 0 && millis() - last_draw_ms >= IDLE_CLEAN_MS)
    {
      epdUpdateCold(LAUNCHER_COLD_TEMP);
      fast_cnt     = 0;
      epd_on       = true;
      last_draw_ms = millis();
    }

    if (epd_on && millis() - last_draw_ms >= IDLE_POWER_OFF_MS)
    {
      epdPowerOff();
      epd_on = false;
    }
    delay(20);
  }
}
