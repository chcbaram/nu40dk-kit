#ifndef ST7567_H_
#define ST7567_H_

#include "hw_def.h"


#ifdef _USE_HW_ST7567

#include "lcd.h"
#include "st7567_regs.h"



bool st7567Init(void);
bool st7567InitDriver(lcd_driver_t *p_driver);
bool st7567DrawAvailable(void);
bool st7567RequestDraw(void);
void st7567SetWindow(int32_t x, int32_t y, int32_t w, int32_t h);

uint32_t st7567GetFps(void);
uint32_t st7567GetFpsTime(void);

uint16_t st7567GetWidth(void);
uint16_t st7567GetHeight(void);

bool st7567SetCallBack(void (*p_func)(void));
bool st7567SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms);
bool st7567DrawBuffer(int16_t x, int16_t y, uint16_t *buffer, uint16_t w, uint16_t h);
bool st7567DrawBufferedLine(int16_t x, int16_t y, uint16_t *buffer, uint16_t w);


#endif

#endif 
