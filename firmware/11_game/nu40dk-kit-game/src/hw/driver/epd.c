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
static uint8_t    fast_hold      = 0;       // 유지 픽셀 구동 길이 (0 = 구동 안 함. DC 누적으로 패널 잔상 유발)
static uint8_t    lut_frames     = 0;       // 지금 로드된 커스텀 LUT 의 전환 프레임 수
static uint8_t    lut_hold       = 0xFF;    // 지금 로드된 커스텀 LUT 의 hold
static uint8_t    lut_fr         = 0x05;    // 커스텀 LUT 프레임 속도 (0x05 = 125Hz, 0x02 = 50Hz)
static uint8_t    lut_fr_loaded  = 0xFF;
static bool       lut_vendor     = false;   // 제조사 전압/VCOM 사용 (Waveshare EPD_4in2_V2 4Gray LUT 값)
                                            // 기본 끔: 체감 효과가 없었고, 켠 상태에서 패널이 구동되지 않는 멈춤이 있었음
static bool       lut_vendor_ld  = false;

static const char *mode_str[EPD_MODE_MAX] = {"full", "part", "fast"};
static uint32_t   busy_err_cnt   = 0;       // BUSY 제한시간 초과 → 자동 복구 횟수
static bool       is_sleep       = false;   // 딥슬립 중 (갱신 요청이 오면 자동으로 깨운다)
static bool       analog_on      = false;   // 아날로그 전원이 켜져 있을 수 있음 (0xFC/0xCC/0x8C 후)
static uint8_t    last_ctrl      = 0;       // 마지막으로 보낸 0x22 값 (원인 추적용)

// FAST 모드에서 바꾼 부스터 소프트 스타트를 기본값(POR)으로 복구
static void epdRestoreSoftStart(void)
{
  const uint8_t d[4] = {0x8B, 0x9C, 0x96, 0x0F};
  const uint8_t border = 0x01;                // 테두리: LUT1 을 따름 (공장 갱신용, 초기화 값)
  ssd1683WriteCmdData(0x0C, d, 4);
  ssd1683WriteCmdData(0x3C, &border, 1);
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

// 커스텀 LUT 준비 (전환 frames, 유지 hold). 이미 같은 LUT 가 로드돼 있으면 생략
//   흰→검 VSH1, 검→흰 VSL, FR 125Hz. 전압(0x04)과 EOPT(0x3F)는 OTP 값을 그대로 사용
//   (VSH1/VSL ±17V, EOPT 0x07 실험 후 패널에 점이 남는 현상이 있어 제외)
static void epdCustomLutSetup(uint8_t frames, uint8_t hold)
{
  uint8_t d[4];

  if (fast_ready && lut_frames == frames && lut_hold == hold &&
      lut_fr_loaded == lut_fr && lut_vendor_ld == lut_vendor) return;

  epdBuildLut(frames, 1, 2, lut_fr, hold);
  epdWriteLut();

  if (lut_vendor)
  {
    // 제조사(Waveshare EPD_4in2_V2, SSD1683 4.2") 4 단계 회색 LUT 의 전압/VCOM.
    // OTP 값은 읽을 수 없어 이 패널용으로 공개된 값을 쓴다. EOPT 는 0x22 유지 (0x07 은 점 발생 이력)
    d[0] = 0x17;                              ssd1683WriteCmdData(0x03, d, 1);   // VGH 20V
    d[0] = 0x41; d[1] = 0xA8; d[2] = 0x32;    ssd1683WriteCmdData(0x04, d, 3);   // VSH1 15V, VSH2 5V, VSL -15V
    d[0] = 0x30;                              ssd1683WriteCmdData(0x2C, d, 1);   // VCOM -1.2V
  }
  if (!fast_ready)
  {
    d[0] = 0x8B; d[1] = 0x9C; d[2] = 0x96; d[3] = 0x00;   // 부스터 소프트 스타트 90ms → 30ms
    ssd1683WriteCmdData(0x0C, d, 4);
    // 테두리(VBD)를 VCOM 으로 고정: 0x01(LUT1 따름)이면 게임 중 스캔마다 테두리가 구동되어
    // 화면 바깥 띠가 회색으로 깜빡인다 (부분 갱신에서 흔히 쓰는 설정, GxEPD2 도 0x80)
    d[0] = 0x80;
    ssd1683WriteCmdData(0x3C, d, 1);
    fast_analog_on = false;
  }
  fast_ready    = true;
  lut_frames    = frames;
  lut_hold      = hold;
  lut_fr_loaded = lut_fr;
  lut_vendor_ld = lut_vendor;
}

void epdSetLutOption(uint8_t fr, bool vendor)
{
  lut_fr     = fr;
  lut_vendor = vendor;
}

void epdGetLutOption(uint8_t *p_fr, bool *p_vendor)
{
  *p_fr     = lut_fr;
  *p_vendor = lut_vendor;
}

bool epdUpdateWindow(epd_mode_t mode, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint8_t  data[2];
  uint8_t *p_fb = canvasGetBuffer();
  uint32_t pre_us;
  uint32_t pre_ms;
  bool     ret;

  if (!epdWake()) return false;

  // 이전 동작이 끝나지 않았으면 잠시 기다리고, 그래도 BUSY 면 복구
  if (ssd1683IsBusy() && !ssd1683WaitBusy(3000))
  {
    epdRecover("busy before update");
  }

  // x 는 바이트 경계로 맞춤
  uint16_t x0 = x & ~7;
  uint16_t x1 = (x + w + 7) & ~7;
  if (x1 > EPD_WIDTH) x1 = EPD_WIDTH;
  if (y + h > EPD_HEIGHT) h = EPD_HEIGHT - y;
  x = x0;
  w = x1 - x0;

  pre_ms = millis();
  pre_us = micros();

  if (mode == EPD_MODE_FAST)
  {
    epdCustomLutSetup(fast_frames, fast_hold);
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
    last_ctrl = full_ctrl;
    ssd1683Activate(full_ctrl);
    analog_on = (full_ctrl & 0x02) == 0;      // 0xF7/0xFF 는 끝에 아날로그를 끈다
    fast_ready     = false;                   // OTP LUT 가 다시 로드됨
    fast_analog_on = false;
    lut_hold       = 0xFF;

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
      last_ctrl = fast_analog_on ? 0x8C : 0xCC;
      ssd1683Activate(last_ctrl);
      analog_on = true;
      fast_analog_on = true;
    }
    else
    {
      last_ctrl = part_ctrl;
      ssd1683Activate(part_ctrl);
      analog_on = (part_ctrl & 0x02) == 0;
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

  if (!ret)
  {
    epdRecover(mode_str[mode]);
  }
  return ret;
}

/*
 * 마스크 갱신 (픽셀별 구동, InkBoy 방식)
 *
 *   p_black : 1 = 이 픽셀을 검정 방향으로 구동 → RED RAM
 *   p_white : 1 = 이 픽셀을 흰색 방향으로 구동 → BW RAM
 *   둘 다 0 인 픽셀은 구동하지 않는다 (LUTBB/LUTWW = 전압 0).
 *   커스텀 LUT 에서 (RED, BW) = (1,0) → LUTBW(VSH1, 검정), (0,1) → LUTWB(VSL, 흰색).
 *   전체 폭(400px, stride 50) 의 y ~ y+h 줄. 화면 RAM 의 이미지는 의미가 없어진다.
 */
bool epdUpdateMask(const uint8_t *p_black, const uint8_t *p_white, uint16_t y, uint16_t h, uint8_t frames)
{
  uint8_t  data[2];
  uint32_t pre_us = micros();
  uint32_t pre_ms = millis();
  bool     ret;

  if (!epdWake()) return false;
  if (ssd1683IsBusy() && !ssd1683WaitBusy(3000))
  {
    epdRecover("busy before mask");
  }

  epdCustomLutSetup(frames, 0);

  // p_black/p_white 는 y 줄부터 시작하는 버퍼이므로 y 를 0 으로 넘기고 창만 y 로 맞춘다
  // h = 0 이면 전송 없이 EPD RAM 의 기존 마스크로 구동만 한다
  if (h > 0)
  {
    ssd1683WriteRam(SSD1683_RAM_RED, 0, y, EPD_WIDTH, h, p_black - y * EPD_STRIDE, EPD_STRIDE);
    ssd1683WriteRam(SSD1683_RAM_BW,  0, y, EPD_WIDTH, h, p_white - y * EPD_STRIDE, EPD_STRIDE);
  }
  epd_time.spi_us = micros() - pre_us;

  data[0] = 0x00; data[1] = 0x00;
  ssd1683WriteCmdData(0x21, data, 2);
  last_ctrl = fast_analog_on ? 0x8C : 0xCC;
  ssd1683Activate(last_ctrl);
  fast_analog_on = true;
  analog_on      = true;

  uint32_t busy_ms = millis();
  ret = ssd1683WaitBusy(EPD_PART_TIMEOUT_MS);
  epd_time.busy_ms  = millis() - busy_ms;
  epd_time.total_ms = millis() - pre_ms;
  epd_time.count++;

  if (!ret) epdRecover("mask");
  return ret;
}

// BUSY 가 풀리지 않을 때: 하드웨어 리셋 + 패널 재초기화 (컨트롤러가 가끔 BUSY 에 갇힘, Phase 7 에서 확인)
void epdRecover(const char *p_reason)
{
  busy_err_cnt++;
  logPrintf("[E_] epd busy timeout (%s, last 0x22=0x%02X, cnt %d) -> reset\n", p_reason, last_ctrl, busy_err_cnt);
  epdInitPanel();
  fast_ready     = false;
  fast_analog_on = false;
  lut_hold       = 0xFF;
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

// 흰색으로 한 번 지운 뒤 캔버스를 전체 갱신 (약 1.3초)
//   흑백 전체 갱신(0xFF) 한 번으로는 이전 화면이 비쳐 보여, 화면 전환 시 사용한다.
//   흰색은 캔버스를 건드리지 않고 0xFF 한 줄을 반복 전송해 만든다
bool epdUpdateFullClean(void)
{
  static const uint8_t white_line[EPD_STRIDE] = { [0 ... EPD_STRIDE - 1] = 0xFF };
  uint8_t data[2];

  if (!epdWake()) return false;
  if (ssd1683IsBusy() && !ssd1683WaitBusy(3000)) epdRecover("busy before clean");
  if (fast_ready) epdRestoreSoftStart();

  for (int ram = 0; ram < 2; ram++)
  {
    ssd1683SetWindow(0, 0, EPD_WIDTH, EPD_HEIGHT);
    ssd1683WriteCmd(ram == 0 ? SSD1683_RAM_BW : SSD1683_RAM_RED);
    for (int y = 0; y < EPD_HEIGHT; y++) ssd1683WriteData(white_line, EPD_STRIDE);
  }
  data[0] = 0x40; data[1] = 0x00;
  ssd1683WriteCmdData(0x21, data, 2);
  last_ctrl = full_ctrl;
  ssd1683Activate(full_ctrl);
  fast_ready     = false;
  fast_analog_on = false;
  analog_on      = (full_ctrl & 0x02) == 0;
  lut_hold       = 0xFF;
  if (!ssd1683WaitBusy(EPD_FULL_TIMEOUT_MS)) epdRecover("clean white");

  return epdUpdate(EPD_MODE_FULL);
}

// 3색 웨이브폼(0xF7, 약 2.8초)으로 전체 갱신: 오래 떠 있던 화면의 잔상까지 지운다
bool epdUpdateStrong(void)
{
  uint8_t ctrl = full_ctrl;
  bool    ret;

  full_ctrl = 0xF7;
  ret = epdUpdate(EPD_MODE_FULL);
  full_ctrl = ctrl;
  return ret;
}

// 온도 레지스터를 낮게 지정한 흑백 전체 갱신 (0xDF: 온도 측정 없이 LUT 로드).
// 저온용 공장 웨이브폼은 구동 시간이 길어 0xFF(0.67초)보다 검정이 진하고 잔상이 적다.
// 10°C 에서 약 1.25초로 3색 갱신(2.8초)보다 빠르다. 다음 0xFC/0xFF 는 온도를 다시 측정한다.
bool epdUpdateCold(int8_t temp)
{
  uint8_t ctrl = full_ctrl;
  uint8_t data = (uint8_t)temp;
  bool    ret;

  if (!epdWake()) return false;
  if (ssd1683IsBusy() && !ssd1683WaitBusy(3000)) epdRecover("busy before cold");

  ssd1683WriteCmdData(0x1A, &data, 1);
  full_ctrl = 0xDF;
  ret = epdUpdate(EPD_MODE_FULL);
  full_ctrl = ctrl;
  return ret;
}

/*
 * DC 균형 흔들기 (잔상 제거)
 *   모든 픽셀(LUTWW/BW/WB/BB 동일)에 VSH1(+15V) frames 프레임, VSL(-15V) frames 프레임을 한 사이클로
 *   cycles 번 반복한다. 양 방향 시간이 같아 새로운 DC 가 쌓이지 않고, 갇힌 입자를 흔들어 풀어 준다.
 *   마지막에 3색 웨이브폼 전체 갱신으로 흰 화면을 만든다.
 */
bool epdShake(uint8_t cycles, uint8_t frames)
{
  const uint16_t ofs_tbl[4] = {42, 84, 126, 168};    // LUTWW, LUTBW, LUTWB, LUTBB
  uint8_t        d[4];
  uint32_t       pre_ms = millis();
  bool           ret;

  if (!epdWake()) return false;
  if (ssd1683IsBusy()) ssd1683WaitBusy(3000);

  frames = MIN(frames, 63);
  memset(lut_buf, 0, sizeof(lut_buf));
  lut_buf[0] = cycles;                                // LUTC: VCOM DC, 같은 길이
  lut_buf[1] = frames;
  lut_buf[2] = frames;
  lut_buf[5] = 1;
  for (int i = 0; i < 4; i++)
  {
    uint8_t *p = &lut_buf[ofs_tbl[i]];
    p[0] = cycles;                                    // RP: 사이클 반복
    p[1] = (0x01 << 6) | frames;                      // A: VSH1
    p[2] = (0x02 << 6) | frames;                      // B: VSL
    p[5] = 1;                                         // SR AB
  }
  lut_buf[224] = 0x05;                                // FR 125Hz
  epdWriteLut();

  d[0] = 0x8B; d[1] = 0x9C; d[2] = 0x96; d[3] = 0x0F; // 소프트 스타트 기본값
  ssd1683WriteCmdData(0x0C, d, 4);
  d[0] = 0x00; d[1] = 0x00;
  ssd1683WriteCmdData(0x21, d, 2);
  last_ctrl = 0xCF;
  ssd1683Activate(0xCF);                              // 로드 없이 흑백 모드 구동 후 전원 끔
  analog_on = false;
  ret = ssd1683WaitBusy(120000);

  fast_ready     = false;                             // 커스텀 LUT 가 바뀜
  fast_analog_on = false;
  lut_hold       = 0xFF;
  if (!ret)
  {
    epdRecover("shake");
    return false;
  }
  logPrintf("[OK] epd shake %d cycles, %d ms\n", cycles, millis() - pre_ms);

  canvasClear(CANVAS_WHITE);
  return epdUpdateStrong();
}

// 검정/흰색 전체 갱신을 3색 웨이브폼(가장 강함)으로 반복해 잔상·잔류 입자 정리
void epdClean(int count)
{
  uint8_t ctrl = full_ctrl;

  full_ctrl = 0xF7;
  for (int i = 0; i < count; i++)
  {
    canvasClear(CANVAS_BLACK);
    epdUpdate(EPD_MODE_FULL);
    canvasClear(CANVAS_WHITE);
    epdUpdate(EPD_MODE_FULL);
  }
  full_ctrl = ctrl;
}

void epdSetFastLut(uint8_t frames, uint8_t hold)
{
  fast_frames = frames;
  fast_hold   = hold;
}

// 아날로그 전원 끄기 (FAST 모드 사용 후 대기 상태로 둘 때)
void epdPowerOff(void)
{
  if (!is_init || is_sleep) return;
  if (ssd1683IsBusy()) ssd1683WaitBusy(3000);
  if (!analog_on)                             // 이미 꺼져 있으면 0x03 을 보내지 않는다 (BUSY 가 풀리지 않는 경우가 있었음)
  {
    fast_analog_on = false;
    return;
  }
  last_ctrl = 0x03;
  ssd1683Activate(0x03);
  if (!ssd1683WaitBusy(500))
  {
    epdRecover("power off");
  }
  fast_analog_on = false;
  analog_on      = false;
}

void epdSleep(void)
{
  epdPowerOff();
  ssd1683DeepSleep();
  is_sleep = true;
}

// 딥슬립 중이면 하드웨어 리셋 + 재초기화로 깨운다. 반환: 갱신 가능 여부
bool epdWake(void)
{
  if (!is_init) return false;
  if (!is_sleep) return true;

  is_sleep       = false;
  fast_ready     = false;
  fast_analog_on = false;
  analog_on      = false;
  lut_hold       = 0xFF;
  if (!epdInitPanel())
  {
    logPrintf("[E_] epd wake fail\n");
    return false;
  }
  logPrintf("[OK] epd wake from deep sleep\n");
  return true;
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
    cliPrintf("init    : %s%s\n", is_init ? "OK" : "Fail", is_sleep ? " (deep sleep)" : "");
    cliPrintf("size    : %d x %d\n", EPD_WIDTH, EPD_HEIGHT);
    cliPrintf("spi     : %d Hz\n", ssd1683GetSpiFreq());
    cliPrintf("busy    : %d\n", epdIsBusy());
    cliPrintf("count   : %d\n", t.count);
    cliPrintf("recover : %d (last 0x22=0x%02X)\n", busy_err_cnt, last_ctrl);
    cliPrintf("ctrl    : full 0x%02X, part 0x%02X\n", full_ctrl, part_ctrl);
    cliPrintf("fast    : frames %d, hold %d, ready %d, analog %d\n", fast_frames, fast_hold, fast_ready, fast_analog_on);
    cliPrintf("last    : spi %d us, busy %d ms, total %d ms\n", t.spi_us, t.busy_ms, t.total_ms);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "init"))
  {
    is_init  = ssd1683Init() && epdInitPanel();
    is_sleep = false;
    fast_ready = false; fast_analog_on = false; analog_on = false; lut_hold = 0xFF;
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

  // 현재 canvas 를 그대로 다시 갱신 (화면 진하기 실험용)
  if (args->argc >= 2 && args->isStr(0, "redraw"))
  {
    uint32_t cnt = args->argc >= 3 ? args->getData(2) : 1;

    for (uint32_t i = 0; i < cnt; i++)
    {
      if (args->isStr(1, "clean"))       epdUpdateFullClean();
      else if (args->isStr(1, "strong")) epdUpdateStrong();
      else epdUpdate(getMode(args, 1, EPD_MODE_PART));
    }
    cliPrintf("redraw %s x %d\n", args->getStr(1), cnt);
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
    int cnt = args->argc >= 2 ? args->getData(1) : 2;

    epdClean(cnt);
    cliPrintf("clean %d done (last busy %d ms)\n", cnt, epd_time.busy_ms);
    ret = true;
  }

  // epd opt <fr_hex> <vendor 0|1> : 커스텀 LUT 프레임 속도, 제조사 전압/VCOM 사용
  if (args->argc >= 1 && args->isStr(0, "opt"))
  {
    uint8_t fr; bool vendor;

    epdGetLutOption(&fr, &vendor);
    if (args->argc >= 2) fr     = (uint8_t)strtoul(args->getStr(1), NULL, 16);
    if (args->argc >= 3) vendor = args->getData(2) ? true : false;
    epdSetLutOption(fr, vendor);
    cliPrintf("lut opt : fr 0x%02X (%s), vendor voltage %d\n", fr,
              fr == 0x05 ? "125Hz" : fr == 0x02 ? "50Hz" : fr == 0x01 ? "25Hz" : fr == 0x03 ? "75Hz" : fr == 0x04 ? "100Hz" : "?", vendor);
    ret = true;
  }

  // epd shake [cycles frames] : DC 균형 흔들기로 잔상 제거 (1 사이클 = frames x 2 x 8ms)
  if (args->argc >= 1 && args->isStr(0, "shake"))
  {
    uint8_t  cycles = args->argc >= 2 ? args->getData(1) : 50;
    uint8_t  frames = args->argc >= 3 ? args->getData(2) : 8;
    uint32_t pre_ms = millis();

    cliPrintf("shake %d cycles x %d frames (about %d s)\n", cycles, frames, cycles * frames * 2 * 8 / 1000);
    bool ok = epdShake(cycles, frames);
    cliPrintf("shake %s, %d ms\n", ok ? "OK" : "Fail", millis() - pre_ms);
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
    cliPrintf("epd deep sleep (다음 갱신 명령이나 epd init 으로 자동 복귀)\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("epd info\n");
    cliPrintf("epd init\n");
    cliPrintf("epd clear [full|part|fast]\n");
    cliPrintf("epd redraw full|part|clean|strong [n]\n");
    cliPrintf("epd test 0~3 [full|part|fast]   0:checker 1:gray 2:gb 3:text\n");
    cliPrintf("epd text <str>\n");
    cliPrintf("epd spi <MHz>\n");
    cliPrintf("epd bench <count> [full|part|fast]\n");
    cliPrintf("epd fast <frames> <hold>\n");
    cliPrintf("epd off\n");
    cliPrintf("epd shake [cycles frames]\n");
    cliPrintf("epd opt [fr_hex vendor]\n");
    cliPrintf("epd ctrl [full|part] <hex>\n");
    cliPrintf("epd temp <C>\n");
    cliPrintf("epd lut <frames> [hold vs_black vs_white fr_hex]\n");
    cliPrintf("epd cmd <hex> [data_hex ...]\n");
    cliPrintf("epd sleep\n");
  }
}
#endif

#endif
