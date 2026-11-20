#ifndef CANVAS_H_
#define CANVAS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_CANVAS

#define CANVAS_WIDTH      HW_EPD_WIDTH
#define CANVAS_HEIGHT     HW_EPD_HEIGHT
#define CANVAS_STRIDE     (CANVAS_WIDTH / 8)


// 1bpp 버퍼: 비트 1 = 흰색, 0 = 검정 (SSD1683 RAM 형식과 동일), MSB 가 왼쪽 픽셀
typedef enum
{
  CANVAS_WHITE = 0,
  CANVAS_BLACK = 1,
  CANVAS_INVERT = 2,
} canvas_color_t;


bool     canvasInit(void);
uint8_t *canvasGetBuffer(void);
void     canvasClear(canvas_color_t color);
void     canvasDrawPixel(int16_t x, int16_t y, canvas_color_t color);
bool     canvasGetPixel(int16_t x, int16_t y);
void     canvasDrawHLine(int16_t x, int16_t y, int16_t w, canvas_color_t color);
void     canvasDrawVLine(int16_t x, int16_t y, int16_t h, canvas_color_t color);
void     canvasDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, canvas_color_t color);
void     canvasFillRect(int16_t x, int16_t y, int16_t w, int16_t h, canvas_color_t color);
void     canvasDrawGray(int16_t x, int16_t y, uint8_t gray);   // 0=검정 ~ 255=흰색, Bayer 4x4
void     canvasFillGray(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray);
void     canvasDrawGb2x2(int16_t x, int16_t y, uint8_t shade);  // GB 4단계(0=흰 ~ 3=검정) → 2x2 패턴
int16_t  canvasPrintf(int16_t x, int16_t y, canvas_color_t color, const char *fmt, ...);
uint16_t canvasGetStrWidth(const char *str);

#endif

#ifdef __cplusplus
}
#endif

#endif
