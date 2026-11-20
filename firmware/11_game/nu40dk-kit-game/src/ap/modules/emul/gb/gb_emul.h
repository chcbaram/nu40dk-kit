#ifndef GB_EMUL_H_
#define GB_EMUL_H_

#include "ap_def.h"
#include "gb_config.h"


bool           gbRomIsValid(void);
void           gbRomGetTitle(char *p_title, uint32_t len);
bool           gbLoad(void);
bool           gbLoadEx(bool clear_ram);
uint32_t       gbGetSaveSize(void);   // 배터리 세이브 크기 (없으면 0)
uint8_t       *gbGetCartRam(void);
void           gbClearCartRam(void);
void           gbRunFrame(void);
void           gbSetJoypad(uint8_t pressed);  // GB_JOY_* 비트, 1 = 눌림
const uint8_t *gbGetLcdBuf(void);   // [144][40] 2bit/픽셀, MSB 가 왼쪽, 0 흰색 ~ 3 검정
bool           gbIsCgb(void);

#endif
