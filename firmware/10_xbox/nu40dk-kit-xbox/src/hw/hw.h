#ifndef HW_H_
#define HW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#include "led.h"
#include "uart.h"
#include "cli.h"
#include "log.h"
#include "cdc.h"
#include "micros.h"
#include "button.h"
#include "keys.h"
#include "spi.h"
#include "eeprom.h"
#include "i2c.h"
#include "flash.h"
#include "rtc.h"
#include "reset.h"
#include "qspi.h"
#include "ws2812.h"
#include "qbuffer.h"
#include "qspi.h"
#include "adc.h"
#include "gpio.h"
#include "spi.h"
#include "lcd.h"
#include "spi_flash.h"
#include "i2c.h"
#include "sd.h"
#include "fatfs.h"
#include "files.h"
#include "i2s.h"
#include "sd.h"
#include "fatfs.h"
#include "files.h"
#include "resize.h"
#include "pdm.h"
#include "cdc.h"
#include "ws2812.h"
#include "usb/usb.h"


bool hwInit(void);


#ifdef __cplusplus
}
#endif

#endif