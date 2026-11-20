/*
 * gb_run.c
 *
 *  에뮬레이터 실행 + EPAPER 표시
 *
 *  [에뮬 스레드]    59.7Hz 페이싱으로 프레임 실행. 디스플레이가 새 프레임을 원할 때만 렌더링.
 *  [디스플레이 스레드] 렌더링된 2bpp 버퍼 → 2x2 디더(1bpp) → 캔버스 (40,6) 320x288
 *                   → EPD FAST 모드 윈도우 갱신 (약 0.1초)
 *
 *  두 스레드는 lcd_buf 를 공유한다. 에뮬은 disp_ready(표시 대기) 일 때만 렌더링하고,
 *  디스플레이는 변환을 끝내면 lcd_buf 를 돌려준다. (변환은 수 ms 라 복사 버퍼가 필요 없다)
 */
#include "ap_def.h"
#include "gb_core.h"
#include "gb_emul.h"
#include "gb_rom.h"
#include "gb_run.h"
#include "remote/ble_pad.h"


#define GB_FRAME_US         16743     // 59.73Hz
#define GB_SCR_X            40        // 400x300 화면에서 게임 영역 위치 (2배 확대 320x288)
#define GB_SCR_Y            6
#define GB_SCR_W            (GB_LCD_WIDTH * 2)
#define GB_SCR_H            (GB_LCD_HEIGHT * 2)

#define EMU_THREAD_STACK    (4 * 1024)
#define EMU_THREAD_PRI      7         // 가장 낮게: CPU 를 쉬지 않고 쓰는 게임에서도 CLI/BT 가 돌도록
#define DISP_THREAD_STACK   (2 * 1024)
#define DISP_THREAD_PRI     4


static bool init(void);
static void emuThread(void *arg1, void *arg2, void *arg3);
static void dispThread(void *arg1, void *arg2, void *arg3);

MODULE_DEF(gb_run)
{
  .name     = "gb_run",
  .priority = MODULE_PRI_NORMAL,
  .init     = init
};

static K_THREAD_STACK_DEFINE(emu_stack, EMU_THREAD_STACK);
static K_THREAD_STACK_DEFINE(disp_stack, DISP_THREAD_STACK);
static struct k_thread emu_thread;
static struct k_thread disp_thread;
static K_SEM_DEFINE(frame_sem, 0, 1);

static volatile bool is_run       = false;
static volatile bool disp_ready   = false;   // 디스플레이가 새 프레임을 기다림
static volatile bool frame_ready  = false;   // lcd_buf 에 새 프레임이 있음
static volatile bool req_full     = false;   // 다음 표시를 전체 갱신으로
static volatile bool is_pause     = false;   // 일시정지 (메뉴)
static volatile bool req_pause_draw = false; // 디스플레이에 PAUSE 표시 요청
static volatile bool menu_req     = false;   // 메뉴 요청 (런처가 읽고 지움)

#define MENU_LONG_PRESS_MS  1000            // 보드 BTN4 길게 누르면 메뉴

static gb_run_stat_t stat;

// 2x2 디더 룩업: GB 1 바이트(4 픽셀, 2bit, MSB 왼쪽) → 캔버스 1 바이트(8 픽셀, 1=흰색)
static uint8_t dither_lut[2][256];




static void makeDitherLut(void)
{
  // shade 별 2x2 패턴 (1 = 검정): [shade][row] 2bit (왼쪽, 오른쪽)
  static const uint8_t pat[4][2] =
  {
    {0x0, 0x0},   // 0 흰색
    {0x2, 0x0},   // 1 밝은 회색
    {0x2, 0x1},   // 2 어두운 회색
    {0x3, 0x3},   // 3 검정
  };

  for (int v = 0; v < 256; v++)
  {
    for (int row = 0; row < 2; row++)
    {
      uint8_t black = 0;
      for (int p = 0; p < 4; p++)
      {
        uint8_t shade = (v >> (6 - p * 2)) & 3;
        black |= pat[shade][row] << (6 - p * 2);
      }
      dither_lut[row][v] = ~black;
    }
  }
}

static void convertFrame(void)
{
  const uint8_t *p_src = gbGetLcdBuf();
  uint8_t       *p_fb  = canvasGetBuffer();

  for (int y = 0; y < GB_LCD_HEIGHT; y++)
  {
    uint8_t *p_dst0 = &p_fb[(GB_SCR_Y + y * 2) * CANVAS_STRIDE + GB_SCR_X / 8];
    uint8_t *p_dst1 = p_dst0 + CANVAS_STRIDE;

    for (int x = 0; x < GB_LCD_WIDTH / 4; x++)
    {
      uint8_t v = *p_src++;
      p_dst0[x] = dither_lut[0][v];
      p_dst1[x] = dither_lut[1][v];
    }
  }
}

/*
 * 입력 매핑 (Phase 5)
 *
 *   Game Boy   | Xbox 패드               | 보드 버튼
 *   -----------+------------------------+-----------
 *   방향       | D-pad, 왼쪽 스틱          | -
 *   A          | A, Y                   | BTN3
 *   B          | B, X                   | BTN4 (짧게)
 *   Select     | View                   | BTN2
 *   Start      | Menu                   | BTN1
 *   메뉴/일시정지 | Xbox 버튼               | BTN4 1초 이상
 *
 *   p_menu : 메뉴 버튼이 새로 눌렸으면 true (엣지)
 */
static uint8_t readInput(bool *p_menu)
{
  static bool     pre_menu = false;
  static uint32_t btn4_time = 0;            // BTN4 눌리기 시작한 시각 (0 = 안 눌림)
  xbox_hid_report_t pad;
  uint8_t           joy  = 0;
  bool              menu = false;

  if (buttonGetPressed(_DEF_CH1)) joy |= GB_JOY_START;
  if (buttonGetPressed(_DEF_CH2)) joy |= GB_JOY_SELECT;
  if (buttonGetPressed(_DEF_CH3)) joy |= GB_JOY_A;
  if (buttonGetPressed(_DEF_CH4))
  {
    if (btn4_time == 0) btn4_time = millis() | 1;
    if (millis() - btn4_time >= MENU_LONG_PRESS_MS) menu = true;
    else                                            joy |= GB_JOY_B;
  }
  else
  {
    btn4_time = 0;
  }

  if (blePadIsConnected() && blePadGetInfo(&pad))
  {
    if (pad.btn_xbox) menu = true;

    // D-pad hat: 1 북, 2 북동, 3 동, 4 남동, 5 남, 6 남서, 7 서, 8 북서
    switch (pad.dpad)
    {
      case 1: joy |= GB_JOY_UP;                    break;
      case 2: joy |= GB_JOY_UP   | GB_JOY_RIGHT;   break;
      case 3: joy |= GB_JOY_RIGHT;                 break;
      case 4: joy |= GB_JOY_DOWN | GB_JOY_RIGHT;   break;
      case 5: joy |= GB_JOY_DOWN;                  break;
      case 6: joy |= GB_JOY_DOWN | GB_JOY_LEFT;    break;
      case 7: joy |= GB_JOY_LEFT;                  break;
      case 8: joy |= GB_JOY_UP   | GB_JOY_LEFT;    break;
      default: break;
    }
    if (pad.left_stick_x < 16384) joy |= GB_JOY_LEFT;
    if (pad.left_stick_x > 49152) joy |= GB_JOY_RIGHT;
    if (pad.left_stick_y < 16384) joy |= GB_JOY_UP;
    if (pad.left_stick_y > 49152) joy |= GB_JOY_DOWN;

    if (pad.btn_a)    joy |= GB_JOY_A;
    if (pad.btn_b)    joy |= GB_JOY_B;
    if (pad.btn_x)    joy |= GB_JOY_B;
    if (pad.btn_y)    joy |= GB_JOY_A;
    if (pad.btn_view) joy |= GB_JOY_SELECT;
    if (pad.btn_menu) joy |= GB_JOY_START;
  }

  *p_menu  = menu && !pre_menu;
  pre_menu = menu;
  return joy;
}

static void drawPause(void)
{
  const int w = 160, h = 48;
  const int x = GB_SCR_X + (GB_SCR_W - w) / 2;
  const int y = GB_SCR_Y + (GB_SCR_H - h) / 2;

  canvasFillRect(x, y, w, h, CANVAS_WHITE);
  canvasDrawRect(x, y, w, h, CANVAS_BLACK);
  canvasDrawRect(x + 2, y + 2, w - 4, h - 4, CANVAS_BLACK);
  canvasPrintf(x + (w - canvasGetStrWidth("PAUSE")) / 2, y + 8, CANVAS_BLACK, "PAUSE");
  canvasPrintf(x + (w - canvasGetStrWidth("XBOX: resume")) / 2, y + 26, CANVAS_BLACK, "XBOX: resume");
}




bool init(void)
{
  makeDitherLut();

  k_thread_create(&emu_thread, emu_stack, K_THREAD_STACK_SIZEOF(emu_stack),
                  emuThread, NULL, NULL, NULL, EMU_THREAD_PRI, 0, K_NO_WAIT);
  k_thread_name_set(&emu_thread, "gb_emu");
  k_thread_create(&disp_thread, disp_stack, K_THREAD_STACK_SIZEOF(disp_stack),
                  dispThread, NULL, NULL, NULL, DISP_THREAD_PRI, 0, K_NO_WAIT);
  k_thread_name_set(&disp_thread, "gb_disp");

  logPrintf("[OK] gbRunInit()\n");
  return true;
}

bool gbRunStart(void)
{
  gbRunStop();

  if (!gbLoad()) return false;

  memset(&stat, 0, sizeof(stat));
  is_pause    = false;
  menu_req    = false;
  frame_ready = false;
  disp_ready  = true;
  req_full    = true;
  is_run      = true;
  return true;
}

void gbRunStop(void)
{
  if (!is_run) return;

  is_run = false;
  k_sleep(K_MSEC(50));
  // 디스플레이 갱신이 끝날 때까지 대기 후 아날로그 전원 끄기
  for (int i = 0; i < 100 && !disp_ready; i++) k_sleep(K_MSEC(10));
  epdPowerOff();
}

bool gbRunIsRunning(void)
{
  return is_run;
}

void gbRunSetPause(bool pause)
{
  if (!is_run || is_pause == pause) return;

  is_pause = pause;
  if (pause)
  {
    req_pause_draw = true;
    k_sem_give(&frame_sem);
  }
}

bool gbRunIsPaused(void)
{
  return is_pause;
}

// 메뉴 버튼이 눌렸는지 (읽으면 지워짐). 런처가 게임 메뉴를 띄울 때 사용
bool gbRunGetMenuRequest(void)
{
  bool ret = menu_req;
  menu_req = false;
  return ret;
}

void gbRunRequestFull(void)
{
  req_full = true;
}

void gbRunGetStat(gb_run_stat_t *p_stat)
{
  *p_stat = stat;
}




void emuThread(void *arg1, void *arg2, void *arg3)
{
  uint32_t next_us = 0;

  while (1)
  {
    if (!is_run)
    {
      k_sleep(K_MSEC(20));
      next_us = micros();
      continue;
    }

    bool    menu;
    uint8_t joy = readInput(&menu);

    if (menu)
    {
      // 메뉴 버튼: 일시정지 토글 (런처가 붙으면 런처가 menu_req 를 받아 게임 메뉴를 띄운다)
      menu_req = true;
      gbRunSetPause(!is_pause);
    }
    if (is_pause)
    {
      k_sleep(K_MSEC(20));
      next_us = micros();
      continue;
    }

    // 디스플레이가 기다리고 있고 이전 프레임을 다 가져갔으면 이번 프레임을 렌더링
    bool draw = disp_ready && !frame_ready;

    gbCoreSetJoypad(joy);
    gbCoreSetDrawNext(draw);

    uint32_t pre_us = micros();
    gbCoreRunFrame();
    uint32_t exe_us = micros() - pre_us;

    stat.emu_frames++;
    stat.emu_us_sum += exe_us;

    if (draw)
    {
      disp_ready  = false;
      frame_ready = true;
      k_sem_give(&frame_sem);
    }

    // 59.73Hz 페이싱. 늦으면 쉬지 않고 따라잡는다 (단, 너무 밀리면 기준 재설정)
    next_us += GB_FRAME_US;
    int32_t remain = (int32_t)(next_us - micros());
    if (remain > 0)
    {
      k_usleep(remain);
    }
    else
    {
      if (remain < -100000) next_us = micros();
      k_yield();
    }
  }
}

void dispThread(void *arg1, void *arg2, void *arg3)
{
  epd_time_t t;

  while (1)
  {
    k_sem_take(&frame_sem, K_FOREVER);
    if (!is_run) continue;

    if (req_pause_draw)
    {
      req_pause_draw = false;
      for (int i = 0; i < 50 && !disp_ready; i++) k_sleep(K_MSEC(10));   // 진행 중 갱신 대기
      drawPause();
      epdUpdateWindow(EPD_MODE_FAST, GB_SCR_X, GB_SCR_Y, GB_SCR_W, GB_SCR_H);
      if (!frame_ready) continue;
    }

    if (req_full)
    {
      // 처음 또는 잔상 정리: 화면 전체를 흰 배경 + 게임 영역 테두리로 전체 갱신
      canvasClear(CANVAS_WHITE);
      canvasDrawRect(GB_SCR_X - 2, GB_SCR_Y - 2, GB_SCR_W + 4, GB_SCR_H + 4, CANVAS_BLACK);
      convertFrame();
      frame_ready = false;
      req_full    = false;
      epdUpdate(EPD_MODE_FULL);
    }
    else
    {
      convertFrame();
      frame_ready = false;                    // lcd_buf 반환 (에뮬은 다음 렌더링 가능)
      epdUpdateWindow(EPD_MODE_FAST, GB_SCR_X, GB_SCR_Y, GB_SCR_W, GB_SCR_H);
    }

    epdGetTime(&t);
    stat.disp_frames++;
    stat.disp_ms_sum += t.total_ms;
    disp_ready = true;
  }
}
