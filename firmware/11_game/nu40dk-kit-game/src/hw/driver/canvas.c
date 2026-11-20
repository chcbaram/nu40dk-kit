#include "canvas.h"


#ifdef _USE_HW_CANVAS
#include <stdarg.h>
#include "hangul/han.h"


static uint8_t canvas_buf[CANVAS_STRIDE * CANVAS_HEIGHT];

static const uint8_t bayer4[4][4] =
{
  { 0,  8,  2, 10},
  {12,  4, 14,  6},
  { 3, 11,  1,  9},
  {15,  7, 13,  5},
};

// GB 4단계 2x2 패턴 (비트 1 = 검정 점): [shade][row] , bit1 = 왼쪽, bit0 = 오른쪽
static const uint8_t gb_pat[4][2] =
{
  {0x0, 0x0},   // 0 흰색
  {0x2, 0x0},   // 1 밝은 회색 (1 dot)
  {0x2, 0x1},   // 2 어두운 회색 (체커 2 dot)
  {0x3, 0x3},   // 3 검정
};




bool canvasInit(void)
{
  canvasClear(CANVAS_WHITE);
  return true;
}

uint8_t *canvasGetBuffer(void)
{
  return canvas_buf;
}

void canvasClear(canvas_color_t color)
{
  memset(canvas_buf, color == CANVAS_BLACK ? 0x00 : 0xFF, sizeof(canvas_buf));
}

void canvasDrawPixel(int16_t x, int16_t y, canvas_color_t color)
{
  if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) return;

  uint8_t *p   = &canvas_buf[y * CANVAS_STRIDE + (x >> 3)];
  uint8_t mask = 0x80 >> (x & 7);

  switch (color)
  {
    case CANVAS_WHITE:  *p |= mask;  break;
    case CANVAS_BLACK:  *p &= ~mask; break;
    default:            *p ^= mask;  break;
  }
}

bool canvasGetPixel(int16_t x, int16_t y)
{
  if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) return false;

  // true = 검정
  return (canvas_buf[y * CANVAS_STRIDE + (x >> 3)] & (0x80 >> (x & 7))) ? false : true;
}

void canvasDrawHLine(int16_t x, int16_t y, int16_t w, canvas_color_t color)
{
  for (int16_t i = 0; i < w; i++) canvasDrawPixel(x + i, y, color);
}

void canvasDrawVLine(int16_t x, int16_t y, int16_t h, canvas_color_t color)
{
  for (int16_t i = 0; i < h; i++) canvasDrawPixel(x, y + i, color);
}

void canvasDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, canvas_color_t color)
{
  canvasDrawHLine(x, y, w, color);
  canvasDrawHLine(x, y + h - 1, w, color);
  canvasDrawVLine(x, y + 1, h - 2, color);
  canvasDrawVLine(x + w - 1, y + 1, h - 2, color);
}

void canvasFillRect(int16_t x, int16_t y, int16_t w, int16_t h, canvas_color_t color)
{
  for (int16_t i = 0; i < h; i++) canvasDrawHLine(x, y + i, w, color);
}

void canvasDrawGray(int16_t x, int16_t y, uint8_t gray)
{
  // gray 0..255 → 0..16 단계, 임계값보다 어두우면 검정
  uint8_t level = (uint8_t)(((uint16_t)gray * 17) >> 8);
  canvasDrawPixel(x, y, bayer4[y & 3][x & 3] >= level ? CANVAS_BLACK : CANVAS_WHITE);
}

void canvasFillGray(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray)
{
  for (int16_t j = 0; j < h; j++)
    for (int16_t i = 0; i < w; i++)
      canvasDrawGray(x + i, y + j, gray);
}

void canvasDrawGb2x2(int16_t x, int16_t y, uint8_t shade)
{
  const uint8_t *p = gb_pat[shade & 3];

  for (int r = 0; r < 2; r++)
  {
    canvasDrawPixel(x,     y + r, (p[r] & 0x2) ? CANVAS_BLACK : CANVAS_WHITE);
    canvasDrawPixel(x + 1, y + r, (p[r] & 0x1) ? CANVAS_BLACK : CANVAS_WHITE);
  }
}

static void drawFont(int16_t x, int16_t y, han_font_t *p_font, canvas_color_t color)
{
  uint16_t bytes = p_font->Size_Char > 2 ? 2 : p_font->Size_Char;

  for (int i = 0; i < 16; i++)
  {
    int16_t ix = 0;
    for (int j = 0; j < bytes; j++)
    {
      uint8_t data = p_font->FontBuffer[i * bytes + j];
      for (int b = 0; b < 8; b++)
      {
        if (data & (0x80 >> b))
        {
          canvasDrawPixel(x + ix, y + i, color);
        }
        ix++;
      }
    }
  }
}

// 8x16 영문 / 16x16 한글. 반환값은 다음 x 위치
//
int16_t canvasPrintf(int16_t x, int16_t y, canvas_color_t color, const char *fmt, ...)
{
  va_list    arg;
  char       buf[128];
  int        len;
  han_font_t font;

  va_start(arg, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, arg);
  va_end(arg);

  if (len > (int)sizeof(buf) - 1) len = sizeof(buf) - 1;

  for (int i = 0; i < len;)
  {
    hanFontLoad(&buf[i], &font);
    if (font.Code_Type == PHAN_END_CODE || font.Size_Char == 0) break;

    drawFont(x, y, &font, color);
    x += font.Size_Char * 8;
    i += font.Size_Char;
  }
  return x;
}

uint16_t canvasGetStrWidth(const char *str)
{
  han_font_t font;
  uint16_t   width = 0;
  int        len = strlen(str);

  for (int i = 0; i < len;)
  {
    hanFontLoad((char *)&str[i], &font);
    if (font.Code_Type == PHAN_END_CODE || font.Size_Char == 0) break;
    width += font.Size_Char * 8;
    i += font.Size_Char;
  }
  return width;
}

#endif
