/*
 * gb_rom.c
 *
 *  SD 카드의 ROM 파일을 Flash rom_storage 파티션으로 복사한다.
 *  같은 ROM 이 이미 있으면 (헤더 + 전역 체크섬 + 크기 비교) 복사를 건너뛴다.
 */
#include "ap_def.h"
#include "gb_rom.h"
#include <ff.h>
#include <strings.h>
#include <pm_config.h>
#include <zephyr/storage/flash_map.h>


#define GB_ROM_ADDR       PM_ROM_STORAGE_ADDRESS
#define GB_ROM_SIZE       PM_ROM_STORAGE_SIZE
#define GB_ROM_PAGE       4096
#define GB_ROM_DIR        "/roms"
#define GB_SAVE_DIR       "/saves"


static uint8_t page_buf[GB_ROM_PAGE];

static const uint8_t *rom_base = (const uint8_t *)GB_ROM_ADDR;




static void makePath(char *p_dst, uint32_t len, const char *p_name)
{
  if (p_name[0] == '/')
    snprintf(p_dst, len, "%s", p_name);
  else
    snprintf(p_dst, len, "%s/%s", GB_ROM_DIR, p_name);
}

// Flash 에 같은 ROM 이 있는지: 0x100~0x14F (헤더, 전역 체크섬 포함) 와 크기 비교
static bool isSameRom(const uint8_t *p_hdr, uint32_t size)
{
  if (memcmp(&rom_base[0x100], &p_hdr[0x100], 0x50) != 0) return false;
  if ((32UL * 1024) << rom_base[0x148] != size) return false;
  return true;
}

gb_rom_err_t gbRomLoad(const char *p_name, bool force, gb_rom_progress_cb_t cb)
{
  char     path[96];
  FIL      fil;
  UINT     rd;
  uint32_t size;
  uint32_t ofs;
  gb_rom_err_t ret = GB_ROM_OK;
  const struct flash_area *fa;


  makePath(path, sizeof(path), p_name);

  if (f_open(&fil, path, FA_READ) != FR_OK)
  {
    return GB_ROM_ERR_OPEN;
  }

  size = f_size(&fil);
  if (size < 0x8000 || size > GB_ROM_SIZE)
  {
    f_close(&fil);
    return GB_ROM_ERR_SIZE;
  }

  // 헤더 확인
  if (f_read(&fil, page_buf, 0x150, &rd) != FR_OK || rd != 0x150)
  {
    f_close(&fil);
    return GB_ROM_ERR_READ;
  }
  if (!force && isSameRom(page_buf, size))
  {
    f_close(&fil);
    return GB_ROM_SAME;
  }

  if (flash_area_open(PM_ROM_STORAGE_ID, &fa) != 0)
  {
    f_close(&fil);
    return GB_ROM_ERR_FLASH;
  }

  f_lseek(&fil, 0);
  for (ofs = 0; ofs < size; ofs += GB_ROM_PAGE)
  {
    uint32_t len = MIN(GB_ROM_PAGE, size - ofs);

    if (f_read(&fil, page_buf, len, &rd) != FR_OK || rd != len)
    {
      ret = GB_ROM_ERR_READ;
      break;
    }
    if (flash_area_erase(fa, ofs, GB_ROM_PAGE) != 0)
    {
      ret = GB_ROM_ERR_FLASH;
      break;
    }
    if (len < GB_ROM_PAGE) memset(&page_buf[len], 0xFF, GB_ROM_PAGE - len);
    if (flash_area_write(fa, ofs, page_buf, GB_ROM_PAGE) != 0)
    {
      ret = GB_ROM_ERR_FLASH;
      break;
    }
    if (memcmp(&rom_base[ofs], page_buf, len) != 0)
    {
      ret = GB_ROM_ERR_VERIFY;
      break;
    }
    if (cb != NULL) cb(ofs + len, size);
  }

  flash_area_close(fa);
  f_close(&fil);
  return ret;
}

const char *gbRomErrStr(gb_rom_err_t err)
{
  switch (err)
  {
    case GB_ROM_OK:         return "OK";
    case GB_ROM_SAME:       return "same rom (skip)";
    case GB_ROM_ERR_OPEN:   return "file open fail";
    case GB_ROM_ERR_SIZE:   return "size error (32KB ~ 512KB)";
    case GB_ROM_ERR_READ:   return "file read fail";
    case GB_ROM_ERR_FLASH:  return "flash write fail";
    case GB_ROM_ERR_VERIFY: return "verify fail";
    default:                return "?";
  }
}

typedef struct
{
  uint32_t index;
  char    *p_name;
  uint32_t len;
} find_arg_t;

static bool findCb(const char *p_name, uint32_t size, void *arg)
{
  find_arg_t *p_arg = (find_arg_t *)arg;

  if (p_arg->index == 0)
  {
    snprintf(p_arg->p_name, p_arg->len, "%s", p_name);
    return false;
  }
  p_arg->index--;
  return true;
}

// 목록에서 index 번째(0 부터) 파일 이름
bool gbRomGetName(uint32_t index, char *p_name, uint32_t len)
{
  find_arg_t arg = {index, p_name, len};

  p_name[0] = 0;
  gbRomList(findCb, &arg);
  return p_name[0] != 0;
}

// /roms 의 .gb / .gbc 파일 목록. cb 가 false 를 반환하면 중단
uint32_t gbRomList(bool (*cb)(const char *p_name, uint32_t size, void *arg), void *arg)
{
  DIR      dir;
  FILINFO  fno;
  uint32_t cnt = 0;

  if (f_opendir(&dir, GB_ROM_DIR) != FR_OK) return 0;

  while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0)
  {
    const char *p_ext = strrchr(fno.fname, '.');

    if (fno.fattrib & (AM_DIR | AM_HID | AM_SYS)) continue;
    if (fno.fname[0] == '.') continue;          // macOS "._" 메타 파일 등
    if (p_ext == NULL) continue;
    if (strcasecmp(p_ext, ".gb") != 0 && strcasecmp(p_ext, ".gbc") != 0) continue;

    cnt++;
    if (cb != NULL && cb(fno.fname, fno.fsize, arg) == false) break;
  }
  f_closedir(&dir);
  return cnt;
}



// ROM 이름 → 세이브 경로 (/saves/<확장자 뺀 이름>.sav)
static void makeSavePath(char *p_dst, uint32_t len, const char *p_rom_name)
{
  const char *p_base = strrchr(p_rom_name, '/');
  char        base[64];
  char       *p_ext;

  snprintf(base, sizeof(base), "%s", p_base ? p_base + 1 : p_rom_name);
  p_ext = strrchr(base, '.');
  if (p_ext) *p_ext = 0;
  snprintf(p_dst, len, "%s/%s.sav", GB_SAVE_DIR, base);
}

bool gbSaveExists(const char *p_rom_name)
{
  char    path[96];
  FILINFO fno;

  makeSavePath(path, sizeof(path), p_rom_name);
  return f_stat(path, &fno) == FR_OK;
}

// 세이브 파일을 cart RAM 으로 읽는다. 파일이 없으면 false
bool gbSaveRead(const char *p_rom_name, uint8_t *p_buf, uint32_t size)
{
  char path[96];
  FIL  fil;
  UINT rd = 0;

  if (size == 0) return false;
  makeSavePath(path, sizeof(path), p_rom_name);
  if (f_open(&fil, path, FA_READ) != FR_OK) return false;
  f_read(&fil, p_buf, size, &rd);
  f_close(&fil);
  return rd > 0;
}

bool gbSaveWrite(const char *p_rom_name, const uint8_t *p_buf, uint32_t size)
{
  char path[96];
  FIL  fil;
  UINT wr = 0;

  if (size == 0) return false;
  f_mkdir(GB_SAVE_DIR);
  makeSavePath(path, sizeof(path), p_rom_name);
  if (f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
  f_write(&fil, p_buf, size, &wr);
  f_close(&fil);
  return wr == size;
}

// ROM 파일 헤더(0x100~0x14F) 읽기: 런처 INFO 패널용
bool gbRomReadHeader(const char *p_name, uint8_t *p_hdr, uint32_t *p_size)
{
  char path[96];
  FIL  fil;
  UINT rd = 0;

  makePath(path, sizeof(path), p_name);
  if (f_open(&fil, path, FA_READ) != FR_OK) return false;
  *p_size = f_size(&fil);
  f_read(&fil, p_hdr, 0x150, &rd);
  f_close(&fil);
  return rd == 0x150;
}
