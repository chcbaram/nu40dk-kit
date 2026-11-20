#include "epd.h"


#ifdef _USE_HW_EPD
#include "epd/ssd1683.h"
#include "canvas.h"
#include "cli.h"
#include <stdlib.h>


#define EPD_FULL_TIMEOUT_MS     5000
#define EPD_PART_TIMEOUT_MS     2000


#if CLI_USE(HW_EPD)
static void cliEpd(cli_args_t *args);
#endif

static bool       is_init = false;
static epd_time_t epd_time;
static uint8_t    full_ctrl = 0xFF;     // 0x22 값: 0xFF 흑백 모드 전체 (0.67s), 0xF7 3색 모드 전체 (2.8s)
static uint8_t    part_ctrl = 0xFC;     // 0x22 값: 0xFC 온도/LUT 로드 후 흑백 표시
static uint8_t    lut_buf[227];         // 0x32 커스텀 LUT (흑백 모드 형식)

// FAST 모드 (Phase 2 실험으로 정한 값)
static bool       fast_ready     = false;   // 커스텀 LUT/전압 설정 완료 여부 (OTP LUT 로드 시 해제)
static bool       fast_analog_on = false;   // 아날로그 전원 유지 중이면 0x8C 로 갱신
static uint8_t    fast_frames    = 10;      // 전환 펄스 길이 (프레임, 8ms @125Hz)
static uint8_t    fast_hold      = 10;      // 유지 픽셀 구동 길이

static const char *mode_str[EPD_MODE_MAX] = {"full", "part", "fast"};

// FAST 모드에서 바꾼 부스터 소프트 스타트를 기본값(POR)으로 복구
static void epdRestoreSoftStart(void)
{
  const uint8_t d[4] = {0x8B, 0x9C, 0x96, 0x0F};
  ssd1683WriteCmdData(0x0C, d, 4);
}

void epdBuildLut(uint8_t frames, uint8_t vs_black, uint8_t vs_white, uint8_t fr, uint8_t hold);
void epdWriteLut(void);




static bool epdInitPanel(void)
{
  uint8_t data[4];

  if (!ssd1683Reset())
  {
    return false;
  }

  ssd1683WriteCmd(0x12);                      // SW reset
  delay(10);
  if (!ssd1683WaitBusy(1000))
  {
    return false;
  }

  data[0] = (SSD1683_HEIGHT - 1) & 0xFF;      // Driver output control: 300 gates
  data[1] = ((SSD1683_HEIGHT - 1) >> 8) & 0x01;
  data[2] = 0x00;
  ssd1683WriteCmdData(0x01, data, 3);

  data[0] = 0x40;                             // Display update control 1: RED RAM bypass
  data[1] = 0x00;
  ssd1683WriteCmdData(0x21, data, 2);

  data[0] = 0x01;                             // Border waveform
  ssd1683WriteCmdData(0x3C, data, 1);

  data[0] = 0x80;                             // 내장 온도 센서
  ssd1683WriteCmdData(0x18, data, 1);

  data[0] = 0x03;                             // Data entry: X 증가, Y 증가
  ssd1683WriteCmdData(0x11, data, 1);

  ssd1683SetWindow(0, 0, SSD1683_WIDTH, SSD1683_HEIGHT);

  return true;
}

bool epdInit(void)
{
  is_init = ssd1683Init();

  if (is_init)
  {
    is_init = epdInitPanel();
  }
  canvasInit();

  logPrintf("[%s] epdInit()\n", is_init ? "OK" : "E_");
  if (is_init)
  {
    logPrintf("     SSD1683 %dx%d, spi %d MHz\n", EPD_WIDTH, EPD_HEIGHT, ssd1683GetSpiFreq() / 1000000);
  }

#if CLI_USE(HW_EPD)
  cliAdd("epd", cliEpd);
#endif
  return is_init;
}

bool epdIsInit(void)
{
  return is_init;
}

bool epdIsBusy(void)
{
  return ssd1683IsBusy();
}

bool epdUpdateWindow(epd_mode_t mode, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint8_t  data[2];
  uint8_t *p_fb = canvasGetBuffer();
  uint32_t pre_us;
  uint32_t pre_ms;
  bool     ret;

  if (!is_init) return false;

  // x 는 바이트 경계로 맞춤
  uint16_t x0 = x & ~7;
  uint16_t x1 = (x + w + 7) & ~7;
  if (x1 > EPD_WIDTH) x1 = EPD_WIDTH;
  if (y + h > EPD_HEIGHT) h = EPD_HEIGHT - y;
  x = x0;
  w = x1 - x0;

  pre_ms = millis();
  pre_us = micros();

  if (mode == EPD_MODE_FAST && !fast_ready)
  {
    uint8_t d[4];

    // 커스텀 LUT: 전환 fast_frames, 유지 fast_hold, 흰→검 VSH1, 검→흰 VSL, FR 125Hz
    //   전압(0x04)과 EOPT(0x3F)는 OTP 값을 그대로 사용한다.
    //   (VSH1/VSL ±17V, EOPT 0x07 실험 후 패널에 점이 남는 현상이 있어 제외)
    epdBuildLut(fast_frames, 1, 2, 0x05, fast_hold);
    epdWriteLut();
    d[0] = 0x8B; d[1] = 0x9C; d[2] = 0x96; d[3] = 0x00;   // 부스터 소프트 스타트 90ms → 30ms
    ssd1683WriteCmdData(0x0C, d, 4);
    fast_ready     = true;
    fast_analog_on = false;
  }

  if (mode != EPD_MODE_FAST && fast_ready)
  {
    epdRestoreSoftStart();
  }

  if (mode == EPD_MODE_FULL)
  {
    ssd1683WriteRam(SSD1683_RAM_BW,  x, y, w, h, p_fb, EPD_STRIDE);
    ssd1683WriteRam(SSD1683_RAM_RED, x, y, w, h, p_fb, EPD_STRIDE);
    epd_time.spi_us = micros() - pre_us;

    data[0] = 0x40; data[1] = 0x00;
    ssd1683WriteCmdData(0x21, data, 2);
    ssd1683Activate(full_ctrl);
    fast_ready     = false;                   // OTP LUT 가 다시 로드됨
    fast_analog_on = false;

    uint32_t busy_ms = millis();
    ret = ssd1683WaitBusy(EPD_FULL_TIMEOUT_MS);
    epd_time.busy_ms = millis() - busy_ms;
  }
  else
  {
    ssd1683WriteRam(SSD1683_RAM_BW, x, y, w, h, p_fb, EPD_STRIDE);
    epd_time.spi_us = micros() - pre_us;

    data[0] = 0x00; data[1] = 0x00;           // RED RAM 을 이전 이미지로 사용 (차분 갱신)
    ssd1683WriteCmdData(0x21, data, 2);

    if (mode == EPD_MODE_FAST)
    {
      // 아날로그가 이미 켜져 있으면 0x8C (클럭 + 흑백 표시만) 로 부스터 기동 시간을 생략
      ssd1683Activate(fast_analog_on ? 0x8C : 0xCC);
      fast_analog_on = true;
    }
    else
    {
      ssd1683Activate(part_ctrl);
      if (part_ctrl & 0x10) fast_ready = false; // LUT 로드 비트가 있으면 커스텀 LUT 가 덮어써짐
    }

    uint32_t busy_ms = millis();
    ret = ssd1683WaitBusy(EPD_PART_TIMEOUT_MS);
    epd_time.busy_ms = millis() - busy_ms;

    // 다음 차분 갱신의 기준이 되도록 이전 이미지 RAM 갱신
    ssd1683WriteRam(SSD1683_RAM_RED, x, y, w, h, p_fb, EPD_STRIDE);
  }

  epd_time.total_ms = millis() - pre_ms;
  epd_time.count++;

  return ret;
}

bool epdUpdate(epd_mode_t mode)
{
  return epdUpdateWindow(mode, 0, 0, EPD_WIDTH, EPD_HEIGHT);
}

void epdGetTime(epd_time_t *p_time)
{
  *p_time = epd_time;
}

// 온도 레지스터를 강제로 쓰고 흑백 모드 LUT 만 OTP 에서 로드 (0x22=0x99).
// 이후 part_ctrl 을 0xCF/0xCC (LUT 로드 없음) 로 쓰면 이 온도의 웨이브폼이 사용된다.
//
uint32_t epdLoadLutByTemp(int8_t temp)
{
  uint8_t  data = (uint8_t)temp;
  uint32_t pre_time = millis();

  ssd1683WriteCmdData(0x1A, &data, 1);
  ssd1683Activate(0x99);
  ssd1683WaitBusy(1000);
  return millis() - pre_time;
}

// 흑백 모드 커스텀 LUT (227 바이트, 데이터시트 Figure 6-7)
//   LUTC 0, LUTWW 42, LUTBW 84, LUTWB 126, LUTBB 168 (각 6 그룹 x 7 바이트)
//   그룹: [RP][VS|TP A][VS|TP B][VS|TP C][VS|TP D][SR AB][SR CD]
//   VS: 00 VSS, 01 VSH1, 10 VSL, 11 VSH2
//   R RAM(이전) / BW RAM(새): 1,0 → LUTBW (흰→검), 0,1 → LUTWB (검→흰)
//
void epdBuildLut(uint8_t frames, uint8_t vs_black, uint8_t vs_white, uint8_t fr, uint8_t hold)
{
  const uint16_t ofs_c  = 0;
  const uint16_t ofs_ww = 42;
  const uint16_t ofs_bw = 84;
  const uint16_t ofs_wb = 126;
  const uint16_t ofs_bb = 168;
  uint8_t tp = frames > 63 ? 63 : frames;

  memset(lut_buf, 0, sizeof(lut_buf));

  const uint16_t ofs_tbl[5] = {ofs_c, ofs_ww, ofs_bw, ofs_wb, ofs_bb};
  // hold: 색이 그대로인 픽셀(WW, BB)도 목표색 방향으로 함께 구동해 잔상(덜 지워진 회색)을 정리
  //   hold = 0 → 구동 안 함, n → n 프레임 구동 (TP 는 frames 와 같은 phase 에서 병렬)
  const uint8_t  vs_tbl[5]  = {0, hold ? vs_white : 0, vs_black, vs_white, hold ? vs_black : 0};
  const uint8_t  tp_tbl[5]  = {tp, hold ? (hold > tp ? tp : hold) : tp, tp, tp, hold ? (hold > tp ? tp : hold) : tp};

  for (int i = 0; i < 5; i++)
  {
    uint8_t *p = &lut_buf[ofs_tbl[i]];

    p[0] = 1;                                   // RP group0 = 1
    p[1] = ((vs_tbl[i] & 0x03) << 6) | tp_tbl[i]; // phase A
    p[5] = 1;                                   // SR AB = 1
    p[6] = 0;                                   // SR CD = 0 (skip)
  }
  lut_buf[224] = fr;                            // FR
  lut_buf[225] = 0;                             // XON
  lut_buf[226] = 0;
}

void epdWriteLut(void)
{
  ssd1683WriteCmdData(0x32, lut_buf, sizeof(lut_buf));
}

void epdSetFastLut(uint8_t frames, uint8_t hold)
{
  fast_frames = frames;
  fast_hold   = hold;
  fast_ready  = false;
}

// 아날로그 전원 끄기 (FAST 모드 사용 후 대기 상태로 둘 때)
void epdPowerOff(void)
{
  ssd1683Activate(0x03);
  ssd1683WaitBusy(500);
  fast_analog_on = false;
}

void epdSleep(void)
{
  epdPowerOff();
  ssd1683DeepSleep();
  is_init = false;
}




#if CLI_USE(HW_EPD)
static void drawTestPattern(int index)
{
  canvasClear(CANVAS_WHITE);

  switch (index)
  {
    case 0:   // 체커 + 테두리 + 방향 표시
      for (int y = 0; y < EPD_HEIGHT; y += 20)
        for (int x = 0; x < EPD_WIDTH; x += 20)
          if (((x / 20) + (y / 20)) & 1) canvasFillRect(x, y, 20, 20, CANVAS_BLACK);
      canvasFillRect(0, 0, 120, 20, CANVAS_WHITE);
      canvasPrintf(4, 2, CANVAS_BLACK, "TOP-LEFT");
      break;

    case 1:   // Bayer 그라데이션 17단계
      canvasPrintf(4, 4, CANVAS_BLACK, "Bayer 4x4 gradient");
      for (int x = 0; x < EPD_WIDTH; x++)
        for (int y = 30; y < 140; y++)
          canvasDrawGray(x, y, (uint8_t)(x * 255 / (EPD_WIDTH - 1)));
      for (int i = 0; i < 17; i++)
        canvasFillGray(i * 23 + 4, 160, 20, 60, (uint8_t)(i * 255 / 16));
      break;

    case 2:   // GB 4단계 2x2 패턴
      canvasPrintf(4, 4, CANVAS_BLACK, "GB 4-shade 2x2 dither");
      for (int s = 0; s < 4; s++)
      {
        for (int y = 0; y < 100; y += 2)
          for (int x = 0; x < 96; x += 2)
            canvasDrawGb2x2(4 + s * 100 + x, 40 + y, s);
        canvasPrintf(4 + s * 100 + 40, 150, CANVAS_BLACK, "%d", s);
      }
      break;

    case 3:   // 텍스트
    default:
      canvasFillRect(0, 0, EPD_WIDTH, 16, CANVAS_BLACK);
      canvasPrintf(8, 0, CANVAS_WHITE, "NU40DK GAME BOY");
      canvasDrawRect(8, 24, 384, 200, CANVAS_BLACK);
      canvasPrintf(16, 32, CANVAS_BLACK, "4.2\" EPAPER SSD1683 400x300");
      canvasPrintf(16, 52, CANVAS_BLACK, "한글 출력 테스트 - 게임 런처");
      canvasPrintf(16, 72, CANVAS_BLACK, "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      canvasPrintf(16, 92, CANVAS_BLACK, "abcdefghijklmnopqrstuvwxyz !@#$%%^&*()");
      canvasFillRect(16, 112, 300, 16, CANVAS_BLACK);
      canvasPrintf(16, 112, CANVAS_WHITE, "> 선택 바 (반전)");
      canvasPrintf(8, 280, CANVAS_BLACK, "[1]UP [2]DOWN [3]RUN [4]MENU");
      break;
  }
}

static epd_mode_t getMode(cli_args_t *args, uint8_t index, epd_mode_t def)
{
  if (args->argc > index && args->isStr(index, "part")) return EPD_MODE_PART;
  if (args->argc > index && args->isStr(index, "full")) return EPD_MODE_FULL;
  if (args->argc > index && args->isStr(index, "fast")) return EPD_MODE_FAST;
  return def;
}

static void printTime(epd_mode_t mode)
{
  epd_time_t t;

  epdGetTime(&t);
  cliPrintf("mode %s : spi %d us, busy %d ms, total %d ms\n",
            mode_str[mode], t.spi_us, t.busy_ms, t.total_ms);
}

void cliEpd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    epd_time_t t;

    epdGetTime(&t);
    cliPrintf("init    : %s\n", is_init ? "OK" : "Fail");
    cliPrintf("size    : %d x %d\n", EPD_WIDTH, EPD_HEIGHT);
    cliPrintf("spi     : %d Hz\n", ssd1683GetSpiFreq());
    cliPrintf("busy    : %d\n", epdIsBusy());
    cliPrintf("count   : %d\n", t.count);
    cliPrintf("ctrl    : full 0x%02X, part 0x%02X\n", full_ctrl, part_ctrl);
    cliPrintf("fast    : frames %d, hold %d, ready %d, analog %d\n", fast_frames, fast_hold, fast_ready, fast_analog_on);
    cliPrintf("last    : spi %d us, busy %d ms, total %d ms\n", t.spi_us, t.busy_ms, t.total_ms);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "init"))
  {
    is_init = ssd1683Init() && epdInitPanel();
    cliPrintf("epd init : %s\n", is_init ? "OK" : "Fail");
    ret = true;
  }

  if (args->argc >= 1 && args->isStr(0, "clear"))
  {
    epd_mode_t mode = getMode(args, 1, EPD_MODE_FULL);

    canvasClear(CANVAS_WHITE);
    epdUpdate(mode);
    printTime(mode);
    ret = true;
  }

  if (args->argc >= 2 && args->isStr(0, "test"))
  {
    epd_mode_t mode = getMode(args, 2, EPD_MODE_FULL);

    drawTestPattern(args->getData(1));
    epdUpdate(mode);
    printTime(mode);
    ret = true;
  }

  if (args->argc >= 2 && args->isStr(0, "text"))
  {
    canvasClear(CANVAS_WHITE);
    canvasPrintf(8, 8, CANVAS_BLACK, "%s", args->getStr(1));
    epdUpdate(EPD_MODE_FULL);
    printTime(EPD_MODE_FULL);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "spi"))
  {
    uint32_t mhz = args->getData(1);

    ssd1683SetSpiFreq(mhz * 1000000);
    cliPrintf("spi : %d Hz\n", ssd1683GetSpiFreq());
    ret = true;
  }

  // 부분 갱신 반복 속도 측정: 박스가 이동하는 화면
  if (args->argc >= 2 && args->isStr(0, "bench"))
  {
    uint32_t   cnt  = args->getData(1);
    epd_mode_t mode = getMode(args, 2, EPD_MODE_PART);
    uint32_t   pre_time = millis();
    uint32_t   busy_sum = 0;
    uint32_t   spi_sum  = 0;
    epd_time_t t;

    for (uint32_t i = 0; i < cnt; i++)
    {
      canvasClear(CANVAS_WHITE);
      canvasPrintf(8, 8, CANVAS_BLACK, "bench %d/%d", i + 1, cnt);
      canvasFillRect(20 + (i * 24) % 340, 100, 40, 40, CANVAS_BLACK);
      epdUpdate(mode);
      epdGetTime(&t);
      busy_sum += t.busy_ms;
      spi_sum  += t.spi_us;
      if (cliKeepLoop() == false) { cnt = i + 1; break; }
    }
    uint32_t total = millis() - pre_time;
    cliPrintf("%s x %d : avg %d ms/frame (spi %d us, busy %d ms), %d.%02d fps\n",
              mode_str[mode], cnt, total / cnt, spi_sum / cnt, busy_sum / cnt,
              (cnt * 1000) / total, ((cnt * 100000) / total) % 100);
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "ctrl"))
  {
    uint8_t value = (uint8_t)strtoul(args->getStr(2), NULL, 16);

    if (args->isStr(1, "full")) full_ctrl = value;
    if (args->isStr(1, "part")) part_ctrl = value;
    cliPrintf("ctrl full 0x%02X, part 0x%02X\n", full_ctrl, part_ctrl);
    ret = true;
  }
  if (args->argc == 3 && args->isStr(0, "fast"))
  {
    epdSetFastLut(args->getData(1), args->getData(2));
    cliPrintf("fast lut : frames %d, hold %d\n", fast_frames, fast_hold);
    ret = true;
  }

  // epd clean [count] : 검정/흰색 전체 갱신을 번갈아 반복해 잔상·잔류 입자 정리 (3색 웨이브폼)
  if (args->argc >= 1 && args->isStr(0, "clean"))
  {
    int     cnt  = args->argc >= 2 ? args->getData(1) : 2;
    uint8_t ctrl = full_ctrl;

    full_ctrl = 0xF7;
    for (int i = 0; i < cnt; i++)
    {
      canvasClear(CANVAS_BLACK);
      epdUpdate(EPD_MODE_FULL);
      canvasClear(CANVAS_WHITE);
      epdUpdate(EPD_MODE_FULL);
      cliPrintf("clean %d/%d\n", i + 1, cnt);
    }
    full_ctrl = ctrl;
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "off"))
  {
    epdPowerOff();
    cliPrintf("epd analog off\n");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "ctrl"))
  {
    cliPrintf("ctrl full 0x%02X, part 0x%02X\n", full_ctrl, part_ctrl);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "temp"))
  {
    int8_t   temp = (int8_t)args->getData(1);
    uint32_t ms   = epdLoadLutByTemp(temp);

    cliPrintf("temp %d C, LUT load %d ms (part ctrl 을 CF 또는 CC 로 설정)\n", temp, ms);
    ret = true;
  }

  // epd lut <frames> [hold vs_black vs_white fr(hex)]
  if (args->argc >= 2 && args->isStr(0, "lut"))
  {
    uint8_t frames   = args->getData(1);
    uint8_t hold     = args->argc >= 3 ? args->getData(2) : 0;
    uint8_t vs_black = args->argc >= 4 ? args->getData(3) : 1;
    uint8_t vs_white = args->argc >= 5 ? args->getData(4) : 2;
    uint8_t fr       = args->argc >= 6 ? (uint8_t)strtoul(args->getStr(5), NULL, 16) : 0x55;

    epdBuildLut(frames, vs_black, vs_white, fr, hold);
    epdWriteLut();
    cliPrintf("lut : frames %d, hold %d, vs_black %d, vs_white %d, fr 0x%02X (part ctrl 을 CF 또는 CC 로 설정)\n",
              frames, hold, vs_black, vs_white, fr);
    ret = true;
  }

  // epd cmd <cmd_hex> [data_hex ...] : 레지스터 직접 쓰기 (실험용)
  if (args->argc >= 2 && args->isStr(0, "cmd"))
  {
    uint8_t cmd = (uint8_t)strtoul(args->getStr(1), NULL, 16);
    uint8_t data[16];
    uint8_t len = 0;

    for (int i = 2; i < args->argc && len < sizeof(data); i++)
    {
      data[len++] = (uint8_t)strtoul(args->getStr(i), NULL, 16);
    }
    ssd1683WriteCmdData(cmd, data, len);
    cliPrintf("cmd 0x%02X, len %d\n", cmd, len);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "sleep"))
  {
    epdSleep();
    cliPrintf("epd deep sleep (epd init 으로 복귀)\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("epd info\n");
    cliPrintf("epd init\n");
    cliPrintf("epd clear [full|part|fast]\n");
    cliPrintf("epd test 0~3 [full|part|fast]   0:checker 1:gray 2:gb 3:text\n");
    cliPrintf("epd text <str>\n");
    cliPrintf("epd spi <MHz>\n");
    cliPrintf("epd bench <count> [full|part|fast]\n");
    cliPrintf("epd fast <frames> <hold>\n");
    cliPrintf("epd off\n");
    cliPrintf("epd ctrl [full|part] <hex>\n");
    cliPrintf("epd temp <C>\n");
    cliPrintf("epd lut <frames> [hold vs_black vs_white fr_hex]\n");
    cliPrintf("epd cmd <hex> [data_hex ...]\n");
    cliPrintf("epd sleep\n");
  }
}
#endif

#endif
