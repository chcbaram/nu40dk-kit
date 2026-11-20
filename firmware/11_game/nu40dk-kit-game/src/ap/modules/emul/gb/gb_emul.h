#ifndef GB_EMUL_H_
#define GB_EMUL_H_

#include "ap_def.h"


#define GB_LCD_WIDTH      160
#define GB_LCD_HEIGHT     144


bool           gbRomIsValid(void);
void           gbRomGetTitle(char *p_title, uint32_t len);
bool           gbLoad(void);
void           gbRunFrame(void);
void           gbSetJoypad(uint8_t joypad);
const uint8_t *gbGetLcdBuf(void);   // [144][40] 2bit/픽셀, MSB 가 왼쪽, 0 흰색 ~ 3 검정
bool           gbIsCgb(void);

#endif
