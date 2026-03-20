/*
 * ssd1306_regs.h
 *
 *  Created on: 2020. 12. 30.
 *      Author: baram
 */

#ifndef SRC_COMMON_HW_INCLUDE_LCD_SSD1315_REGS_H_
#define SRC_COMMON_HW_INCLUDE_LCD_SSD1315_REGS_H_


#define SSD1315_BLACK     0   ///< Draw 'off' pixels
#define SSD1315_WHITE     1   ///< Draw 'on' pixels
#define SSD1315_INVERSE   2   ///< Invert pixels

#define SSD1315_MEMORYMODE 0x20          ///< See datasheet
#define SSD1315_COLUMNADDR 0x21          ///< See datasheet
#define SSD1315_PAGEADDR 0x22            ///< See datasheet
#define SSD1315_SETCONTRAST 0x81         ///< See datasheet
#define SSD1315_CHARGEPUMP 0x8D          ///< See datasheet
#define SSD1315_SEGREMAP 0xA0            ///< See datasheet
#define SSD1315_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SSD1315_DISPLAYALLON 0xA5        ///< Not currently used
#define SSD1315_NORMALDISPLAY 0xA6       ///< See datasheet
#define SSD1315_INVERTDISPLAY 0xA7       ///< See datasheet
#define SSD1315_SETMULTIPLEX 0xA8        ///< See datasheet
#define SSD1315_DISPLAYOFF 0xAE          ///< See datasheet
#define SSD1315_DISPLAYON 0xAF           ///< See datasheet
#define SSD1315_COMSCANINC 0xC0          ///< Not currently used
#define SSD1315_COMSCANDEC 0xC8          ///< See datasheet
#define SSD1315_SETDISPLAYOFFSET 0xD3    ///< See datasheet
#define SSD1315_SETDISPLAYCLOCKDIV 0xD5  ///< See datasheet
#define SSD1315_SETPRECHARGE 0xD9        ///< See datasheet
#define SSD1315_SETCOMPINS 0xDA          ///< See datasheet
#define SSD1315_SETVCOMDETECT 0xDB       ///< See datasheet

#define SSD1315_SETLOWCOLUMN 0x00  ///< Not currently used
#define SSD1315_SETHIGHCOLUMN 0x10 ///< Not currently used
#define SSD1315_SETSTARTLINE 0x40  ///< See datasheet

#define SSD1315_EXTERNALVCC 0x01  ///< External display voltage source
#define SSD1315_SWITCHCAPVCC 0x02 ///< Gen. display voltage from 3.3V

#define SSD1315_RIGHT_HORIZONTAL_SCROLL 0x26              ///< Init rt scroll
#define SSD1315_LEFT_HORIZONTAL_SCROLL 0x27               ///< Init left scroll
#define SSD1315_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SSD1315_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A  ///< Init diag scroll
#define SSD1315_DEACTIVATE_SCROLL 0x2E                    ///< Stop scroll
#define SSD1315_ACTIVATE_SCROLL 0x2F                      ///< Start scroll
#define SSD1315_SET_VERTICAL_SCROLL_AREA 0xA3             ///< Set scroll range

// Deprecated size stuff for backwards compatibility with old sketches

#define SSD1315_LCDWIDTH 128 ///< DEPRECATED: width w/SSD1315_128_64 defined
#define SSD1315_LCDHEIGHT 64 ///< DEPRECATED: height w/SSD1315_128_64 defined


#endif /* SRC_COMMON_HW_INCLUDE_LCD_SSD1315_REGS_H_ */
