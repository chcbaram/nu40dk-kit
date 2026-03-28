#include "fatfs.h"


#ifdef _USE_HW_FATFS
#include "cli.h"
#include "sd.h"
#include <ff.h>

#define DISK_NAME     "SD"
#define DISK_MOUNT_PT DISK_NAME":"


static bool is_init = false;

static FATFS fat_fs;
static const char *disk_mount_pt = DISK_MOUNT_PT;


#if CLI_USE(HW_FATFS)
static void cliFatfs(cli_args_t *args);
#endif

bool fatfsInit(void)
{
  bool ret = true;

  if (sdIsInit() == false)
  {
    return false;
  };


  if(f_mount(&fat_fs, (TCHAR const*)disk_mount_pt, 1) == FR_OK)
  {
    is_init = true;
  }

  ret = is_init;
  logPrintf("[%s] fatfsInit()\n", ret ? "OK":"NG");

#if CLI_USE(HW_FATFS)
  cliAdd("fatfs", cliFatfs);
#endif

  return ret;
}

bool fatfsReInit(void)
{
  bool ret = true;

  if (sdIsInit() == false)
  {
    return false;
  };


  if(f_mount(&fat_fs, (TCHAR const*)disk_mount_pt, 0) == FR_OK)
  {
    is_init = true;
  }

  ret = is_init;

  return ret;
}


#if CLI_USE(HW_FATFS)

FRESULT fatfsDir(char* path)
{
  FRESULT res;
  DIR dir;
  FILINFO fno;


  res = f_opendir(&dir, path);                       /* Open the directory */
  if (res == FR_OK)
  {
    for (;;)
    {
      res = f_readdir(&dir, &fno);                   /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
      if (fno.fattrib & AM_DIR)
      {                    /* It is a directory */
        cliPrintf(" %s/%s \n", path, fno.fname);
      }
      else
      {                                       /* It is a file. */
        cliPrintf(" %s/%32s \t%d bytes\n", path, fno.fname, (int)fno.fsize);
      }
    }
    f_closedir(&dir);
  }

  return res;
}

uint32_t fatfsPrintf(FIL *file_ptr, const char *fmt, ...)
{
  char buf[256];
  va_list args;
  int len;
  uint32_t ret = 0;

  va_start(args, fmt);
  len = vsnprintf(buf, 256, fmt, args);

  UINT written_bytes;

  if (f_write(file_ptr, buf, len, &written_bytes) == FR_OK)
  {
    ret = written_bytes;
  }

  va_end(args);

  return ret;
}

void cliFatfs(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    cliPrintf("fatfs init \t: %d\n", is_init);

    if (is_init == true)
    {
      FATFS *fs;
       DWORD fre_clust, fre_sect, tot_sect;
       FRESULT res;

       /* Get volume information and free clusters of drive 1 */
       res = f_getfree(disk_mount_pt, &fre_clust, &fs);
       if (res == FR_OK)
       {
         /* Get total sectors and free sectors */
         tot_sect = (fs->n_fatent - 2) * fs->csize;
         fre_sect = fre_clust * fs->csize;

         /* Print the free space (assuming 512 bytes/sector) */
         cliPrintf("%10lu KiB total drive space.\n%10lu KiB available.\n", tot_sect / 2, fre_sect / 2);
       }
       else
       {
         cliPrintf(" err : %d\n", res);
       }
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "dir") == true)
  {
    FRESULT res;

    res = fatfsDir("/");
    if (res != FR_OK)
    {
      cliPrintf(" err : %d\n", res);
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "test") == true)
  {
    FRESULT fp_ret;
    FIL log_file;
    uint32_t pre_time;
    

    pre_time = millis();
    fp_ret = f_open(&log_file, "1.csv", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
    if (fp_ret == FR_OK)
    {    
      fatfsPrintf(&log_file, "test1, ");
      fatfsPrintf(&log_file, "test2, ");
      fatfsPrintf(&log_file, "test3, ");
      fatfsPrintf(&log_file, ", ");
      fatfsPrintf(&log_file, "\n");

      for (int i=0; i<8; i++)
      {
        fatfsPrintf(&log_file, "%d \n", i);
      }

      f_rewind(&log_file);


      UINT len;
      uint8_t data;

      while(cliKeepLoop())
      {
        len = 0;
        fp_ret = f_read (&log_file, &data, 1, &len);

        if (fp_ret != FR_OK)
        {
          break;
        }
        if (len == 0)
        {
          break;
        }

        cliPrintf("%c", data);
      }

      f_close(&log_file);
    }
    else
    {
      cliPrintf("f_open fail\r\n");
    }
    cliPrintf("%d ms\r\n", millis()-pre_time);

    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("fatfs info\n");
    cliPrintf("fatfs dir\n");
    cliPrintf("fatfs test\n");
  }
}

#endif



#endif
