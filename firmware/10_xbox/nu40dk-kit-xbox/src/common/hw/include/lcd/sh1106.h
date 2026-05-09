#ifndef SH1106_H_
#define SH1106_H_

#include "hw_def.h"


#ifdef _USE_HW_SH1106

#include "lcd.h"
#include "sh1106_regs.h"

bool sh1106Init(void);
bool sh1106InitDriver(lcd_driver_t *p_driver);

#endif


#endif 
