#ifndef SSD1315_H_
#define SSD1315_H_

#include "hw_def.h"


#ifdef _USE_HW_SSD1315

#include "lcd.h"
#include "ssd1315_regs.h"

bool ssd1315Init(void);
bool ssd1315InitDriver(lcd_driver_t *p_driver);

#endif


#endif 
