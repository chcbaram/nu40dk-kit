#ifndef SSD1683_H_
#define SSD1683_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_EPD

#define SSD1683_WIDTH         400
#define SSD1683_HEIGHT        300
#define SSD1683_LINE_BYTES    (SSD1683_WIDTH / 8)


typedef enum
{
  SSD1683_RAM_BW  = 0x24,     // 새 이미지
  SSD1683_RAM_RED = 0x26,     // 부분 갱신 시 이전 이미지
} ssd1683_ram_t;


bool     ssd1683Init(void);
bool     ssd1683Reset(void);
bool     ssd1683IsBusy(void);
bool     ssd1683WaitBusy(uint32_t timeout_ms);
void     ssd1683WriteCmd(uint8_t cmd);
void     ssd1683WriteData(const uint8_t *p_data, uint32_t length);
void     ssd1683WriteCmdData(uint8_t cmd, const uint8_t *p_data, uint32_t length);
void     ssd1683SetWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void     ssd1683WriteRam(ssd1683_ram_t ram, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                         const uint8_t *p_fb, uint16_t fb_stride);
void     ssd1683Activate(uint8_t update_ctrl2);
void     ssd1683DeepSleep(void);
bool     ssd1683SetSpiFreq(uint32_t freq_hz);
uint32_t ssd1683GetSpiFreq(void);

#endif

#ifdef __cplusplus
}
#endif

#endif
