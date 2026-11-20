#include "epd.h"


#ifdef _USE_HW_EPD
#include "epd/ssd1683.h"
#include "canvas.h"
#include "cli.h"


#define EPD_FULL_TIMEOUT_MS     5000
#define EPD_PART_TIMEOUT_MS     2000


#if CLI_USE(HW_EPD)
static void cliEpd(cli_args_t *args);
#endif

static bool       is_init = false;
static epd_time_t epd_time;

static const char *mode_str[EPD_MODE_MAX] = {"full", "part"};




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

  if (mode == EPD_MODE_FULL)
  {
    ssd1683WriteRam(SSD1683_RAM_BW,  x, y, w, h, p_fb, EPD_STRIDE);
    ssd1683WriteRam(SSD1683_RAM_RED, x, y, w, h, p_fb, EPD_STRIDE);
    epd_time.spi_us = micros() - pre_us;

    data[0] = 0x40; data[1] = 0x00;
    ssd1683WriteCmdData(0x21, data, 2);
    ssd1683Activate(0xF7);

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
    ssd1683Activate(0xFC);

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

void epdSleep(void)
{
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
    cliPrintf("epd clear [full|part]\n");
    cliPrintf("epd test 0~3 [full|part]   0:checker 1:gray 2:gb 3:text\n");
    cliPrintf("epd text <str>\n");
    cliPrintf("epd spi <MHz>\n");
    cliPrintf("epd bench <count> [full|part]\n");
    cliPrintf("epd sleep\n");
  }
}
#endif

#endif
