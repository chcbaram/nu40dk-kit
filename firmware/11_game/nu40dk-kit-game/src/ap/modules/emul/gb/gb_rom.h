#ifndef GB_ROM_H_
#define GB_ROM_H_

#include "ap_def.h"


typedef enum
{
  GB_ROM_OK = 0,
  GB_ROM_SAME,          // 같은 ROM 이 이미 Flash 에 있음 (복사 생략)
  GB_ROM_ERR_OPEN,
  GB_ROM_ERR_SIZE,
  GB_ROM_ERR_READ,
  GB_ROM_ERR_FLASH,
  GB_ROM_ERR_VERIFY,
} gb_rom_err_t;

typedef void (*gb_rom_progress_cb_t)(uint32_t done, uint32_t total);


gb_rom_err_t gbRomLoad(const char *p_name, bool force, gb_rom_progress_cb_t cb);   // 이름만 주면 /roms/ 기준
const char  *gbRomErrStr(gb_rom_err_t err);
bool         gbRomGetName(uint32_t index, char *p_name, uint32_t len);
bool         gbRomReadHeader(const char *p_name, uint8_t *p_hdr, uint32_t *p_size);   // p_hdr: 0x150 바이트
bool         gbSaveExists(const char *p_rom_name);
bool         gbSaveRead(const char *p_rom_name, uint8_t *p_buf, uint32_t size);
bool         gbSaveWrite(const char *p_rom_name, const uint8_t *p_buf, uint32_t size);
uint32_t     gbRomList(bool (*cb)(const char *p_name, uint32_t size, void *arg), void *arg);

#endif
