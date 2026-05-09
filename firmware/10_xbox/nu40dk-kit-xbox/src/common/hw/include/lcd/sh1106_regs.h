#ifndef SH1106_REGS_H_
#define SH1106_REGS_H_


#define SH1106_BLACK     0   ///< Draw 'off' pixels
#define SH1106_WHITE     1   ///< Draw 'on' pixels
#define SH1106_INVERSE   2   ///< Invert pixels

#define SH1106_MEMORYMODE 0x20          ///< See datasheet
#define SH1106_COLUMNADDR 0x21          ///< See datasheet
#define SH1106_PAGEADDR 0x22            ///< See datasheet
#define SH1106_SETCONTRAST 0x81         ///< See datasheet
#define SH1106_CHARGEPUMP 0x8D          ///< See datasheet
#define SH1106_SEGREMAP 0xA0            ///< See datasheet
#define SH1106_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SH1106_DISPLAYALLON 0xA5        ///< Not currently used
#define SH1106_NORMALDISPLAY 0xA6       ///< See datasheet
#define SH1106_INVERTDISPLAY 0xA7       ///< See datasheet
#define SH1106_SETMULTIPLEX 0xA8        ///< See datasheet
#define SH1106_DISPLAYOFF 0xAE          ///< See datasheet
#define SH1106_DISPLAYON 0xAF           ///< See datasheet
#define SH1106_COMSCANINC 0xC0          ///< Not currently used
#define SH1106_COMSCANDEC 0xC8          ///< See datasheet
#define SH1106_SETDISPLAYOFFSET 0xD3    ///< See datasheet
#define SH1106_SETDISPLAYCLOCKDIV 0xD5  ///< See datasheet
#define SH1106_SETPRECHARGE 0xD9        ///< See datasheet
#define SH1106_SETCOMPINS 0xDA          ///< See datasheet
#define SH1106_SETVCOMDETECT 0xDB       ///< See datasheet

#define SH1106_SETLOWCOLUMN 0x00  ///< Not currently used
#define SH1106_SETHIGHCOLUMN 0x10 ///< Not currently used
#define SH1106_SETSTARTLINE 0x40  ///< See datasheet

#define SH1106_EXTERNALVCC 0x01  ///< External display voltage source
#define SH1106_SWITCHCAPVCC 0x02 ///< Gen. display voltage from 3.3V

#define SH1106_RIGHT_HORIZONTAL_SCROLL 0x26              ///< Init rt scroll
#define SH1106_LEFT_HORIZONTAL_SCROLL 0x27               ///< Init left scroll
#define SH1106_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SH1106_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A  ///< Init diag scroll
#define SH1106_DEACTIVATE_SCROLL 0x2E                    ///< Stop scroll
#define SH1106_ACTIVATE_SCROLL 0x2F                      ///< Start scroll
#define SH1106_SET_VERTICAL_SCROLL_AREA 0xA3             ///< Set scroll range

// Deprecated size stuff for backwards compatibility with old sketches

#define SH1106_LCDWIDTH 128 ///< DEPRECATED: width w/SH1106_128_64 defined
#define SH1106_LCDHEIGHT 64 ///< DEPRECATED: height w/SH1106_128_64 defined


#endif /* SRC_COMMON_HW_INCLUDE_LCD_SH1106_REGS_H_ */
