#ifndef EPD_H_
#define EPD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_EPD

#define EPD_WIDTH         HW_EPD_WIDTH
#define EPD_HEIGHT        HW_EPD_HEIGHT
#define EPD_STRIDE        (EPD_WIDTH / 8)
#define EPD_BUF_SIZE      (EPD_STRIDE * EPD_HEIGHT)


typedef enum
{
  EPD_MODE_FULL = 0,      // 내장 웨이브폼 전체 갱신 (깜빡임, 잔상 정리)
  EPD_MODE_PART,          // 부분 갱신 (OTP 웨이브폼, 차분, 화질 좋음, 약 0.6초)
  EPD_MODE_FAST,          // 고속 부분 갱신 (커스텀 LUT, 약 0.1초, 게임용)
  EPD_MODE_MAX
} epd_mode_t;

typedef struct
{
  uint32_t spi_us;        // RAM 전송 시간
  uint32_t busy_ms;       // 패널 갱신 (BUSY) 시간
  uint32_t total_ms;
  uint32_t count;
} epd_time_t;


bool epdInit(void);
bool epdIsInit(void);
bool epdIsBusy(void);
bool epdUpdate(epd_mode_t mode);
bool epdUpdateWindow(epd_mode_t mode, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void epdGetTime(epd_time_t *p_time);
void epdSetFastLut(uint8_t frames, uint8_t hold);
bool epdUpdateMask(const uint8_t *p_black, const uint8_t *p_white, uint16_t y, uint16_t h, uint8_t frames);
void epdClean(int count);
bool epdShake(uint8_t cycles, uint8_t frames);   // DC 균형 흔들기 (잔상 제거)
void epdSetLutOption(uint8_t fr, bool vendor);     // 커스텀 LUT 프레임 속도(0x02 50Hz, 0x05 125Hz), 제조사 전압
void epdGetLutOption(uint8_t *p_fr, bool *p_vendor);
void epdRecover(const char *p_reason);   // BUSY 에 갇혔을 때 하드웨어 리셋 + 재초기화
bool epdUpdateStrong(void);              // 3색 웨이브폼 전체 갱신 (잔상 제거, 약 2.8초)
bool epdUpdateFullClean(void);           // 흰색으로 지운 뒤 전체 갱신 (약 1.3초)
bool epdUpdateCold(int8_t temp);
bool epdZeroRam(void);                   // RAM 두 면을 0 (마스크 구동에서 구동 안 함)         // 저온 웨이브폼 흑백 전체 갱신 (10°C 약 1.25초)
void epdPowerOff(void);
void epdSleep(void);
bool epdWake(void);              // 딥슬립 중이면 리셋 + 재초기화 (갱신 함수들이 자동 호출)

#endif

#ifdef __cplusplus
}
#endif

#endif
