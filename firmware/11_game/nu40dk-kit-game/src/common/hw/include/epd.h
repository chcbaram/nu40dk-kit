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
  EPD_MODE_PART,          // 부분 갱신 (차분, 깜빡임 없음)
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
void epdSleep(void);

#endif

#ifdef __cplusplus
}
#endif

#endif
