#include "sd.h"



#ifdef _USE_HW_SD
#include <zephyr/storage/disk_access.h>
#include "gpio.h"
#include "spi.h"
#include "cli.h"


#define DISK_NAME   "SD"



static bool is_init = false;
static bool is_detected = false;
static uint8_t is_try = 0;
static sd_state_t sd_state = SDCARD_IDLE;




#ifdef _USE_HW_CLI
static void cliSd(cli_args_t *args);
#endif



bool sdInit(void)
{
  bool ret = false;


  is_detected = sdIsDetected();

  if (is_detected == true)
  {
    int rc = disk_access_init(DISK_NAME);  
    if (rc == 0)
    {
      ret = true;
    }
  }  

  static bool is_reinit = false;

  if (is_reinit == false)
  {
    uint32_t buf[512/4];
    
    ret = sdReadBlocks(0, (uint8_t *)buf, 1, 100);
    
    logPrintf("[%s] sdInit()\n", ret ? "OK":"E_");   
    if (is_detected == true)
    {
      logPrintf("     status : %d\n", disk_access_status(DISK_NAME));            
      logPrintf("     sdcard found\n");
    }
    else
    {
      logPrintf("     sdcard not found\n");   
    }
  }

  is_init = ret;


#if CLI_USE(HW_SD)
  if (is_reinit == false)
    cliAdd("sd", cliSd);
#endif

  return ret;
}

bool sdReInit(void)
{
  bool ret = false;

  for (int i=0; i<3; i++)
  {
    if (disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_DEINIT, NULL) != 0)
    {
      break;
    }
  }

  int rc = disk_access_init(DISK_NAME);  
  if (rc == 0)
  {
    ret = true;
  }

  is_init = ret;

  return ret;
}

bool sdDeInit(void)
{
  bool ret = false;

  if (is_init == true)
  {
    is_init = false;
    disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_DEINIT, NULL);
    ret = true;
  }

  return ret;
}

bool sdIsInit(void)
{
  return is_init;
}

bool sdIsDetected(void)
{
  if (gpioPinRead(SD_CD) == true)
  {
    is_detected = true;
  }
  else
  {
    is_detected = false;
  }

  return is_detected;
}

sd_state_t sdUpdate(void)
{
  sd_state_t ret_state = SDCARD_IDLE;
  static uint32_t pre_time;


  switch(sd_state)
  {
    case SDCARD_IDLE:
      if (sdIsDetected() == true)
      {
        if (is_init)
        {
          sd_state = SDCARD_CONNECTED;
        }
        else
        {
          sd_state = SDCARD_CONNECTTING;
          pre_time = millis();
        }
      }
      else
      {
        is_init = false;
        sd_state  = SDCARD_DISCONNECTED;
        ret_state = SDCARD_DISCONNECTED;
      }
      break;

    case SDCARD_CONNECTTING:
      if (millis()-pre_time >= 100)
      {
        if (sdReInit())
        {
          sd_state  = SDCARD_CONNECTED;
          ret_state = SDCARD_CONNECTED;
        }
        else
        {
          sd_state = SDCARD_IDLE;
          is_try++;

          if (is_try >= 3)
          {
            sd_state = SDCARD_ERROR;
          }
        }
      }
      break;

    case SDCARD_CONNECTED:
      if (sdIsDetected() != true)
      {
        is_try = 0;
        sd_state = SDCARD_IDLE;
      }
      break;

    case SDCARD_DISCONNECTED:
      if (sdIsDetected() == true)
      {
        sd_state = SDCARD_IDLE;
      }
      break;

    case SDCARD_ERROR:
      break;
  }

  return ret_state;
}

bool sdGetInfo(sd_info_t *p_info)
{
  bool ret = false;


  if (is_init == true)
  {
		uint32_t block_count = 0;
		uint32_t block_size = 0;

		disk_access_ioctl(DISK_NAME, DISK_IOCTL_GET_SECTOR_COUNT, &block_count);
		disk_access_ioctl(DISK_NAME, DISK_IOCTL_GET_SECTOR_SIZE, &block_size);

    memset(p_info, 0, sizeof(sd_info_t));
    
    p_info->block_numbers      = block_count;
    p_info->block_size         = block_size;

    p_info->card_size          =  (uint32_t)((uint64_t)p_info->block_numbers * (uint64_t)p_info->block_size / (uint64_t)1024 / (uint64_t)1024);    
    ret = true;
  }

  return ret;
}

bool sdIsBusy(void)
{
  bool is_busy = true;

  if (disk_access_status(DISK_NAME) == 0)
  {
    is_busy = false;
  }

  return is_busy;
}

bool sdIsReady(uint32_t timeout)
{
  uint32_t pre_time;

  pre_time = millis();

  while(millis() - pre_time < timeout)
  {
    if (sdIsBusy() == false)
    {
      return true;
    }
  }

  return false;
}

bool sdReadBlocks(uint32_t block_addr, uint8_t *p_data, uint32_t num_of_blocks, uint32_t timeout_ms)
{
  bool ret = false;

  if (disk_access_read(DISK_NAME, p_data, block_addr, num_of_blocks) == 0) 
  {
    ret = true;
  }

  return ret;
}

bool sdWriteBlocks(uint32_t block_addr, uint8_t *p_data, uint32_t num_of_blocks, uint32_t timeout_ms)
{
  bool ret = false;

  if (disk_access_write(DISK_NAME, p_data, block_addr, num_of_blocks) == 0) 
  {
    ret = true;
  }

  return ret;
}

bool sdEraseBlocks(uint32_t start_addr, uint32_t end_addr)
{
  bool ret = true;


  return ret;
}



#ifdef _USE_HW_CLI
void cliSd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    sd_info_t sd_info;

    cliPrintf("sd init      : %d\n", is_init);
    cliPrintf("sd connected : %d\n", is_detected);

    if (is_init == true)
    {
      if (sdGetInfo(&sd_info) == true)
      {
        cliPrintf("   card_type            : %d\n", sd_info.card_type);
        cliPrintf("   card_version         : %d\n", sd_info.card_version);
        cliPrintf("   card_class           : %d\n", sd_info.card_class);
        cliPrintf("   rel_card_Add         : %d\n", sd_info.rel_card_Add);
        cliPrintf("   block_numbers        : %d\n", sd_info.block_numbers);
        cliPrintf("   block_size           : %d\n", sd_info.block_size);
        cliPrintf("   log_block_numbers    : %d\n", sd_info.log_block_numbers);
        cliPrintf("   log_block_size       : %d\n", sd_info.log_block_size);
        cliPrintf("   card_size            : %d MB, %d.%d GB\n", sd_info.card_size, sd_info.card_size/1024, ((sd_info.card_size * 10)/1024) % 10);
      }
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "read") == true)
  {
    uint32_t number;
    uint32_t buf[512/4];

    number = args->getData(1);

    if (sdReadBlocks(number, (uint8_t *)buf, 1, 100) == true)
    {
      for (int i=0; i<512/4; i++)
      {
        cliPrintf("%d:%04d : 0x%08X\n", number, i*4, buf[i]);
      }
    }
    else
    {
      cliPrintf("sdRead Fail\n");
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "speed-test") == true)
  {
    uint32_t number;
    uint32_t buf[512/4];
    uint32_t cnt;
    uint32_t pre_time;
    uint32_t exe_time;

    number = args->getData(1);

    cnt = 1024*1024 / 512;
    pre_time = millis();
    for (int i=0; i<cnt; i++)
    {
      if (sdReadBlocks(number, (uint8_t *)buf, 1, 100) == false)
      {
        cliPrintf("sdReadBlocks() Fail:%d\n", i);
        break;
      }
    }
    exe_time = millis()-pre_time;
    if (exe_time > 0)
    {
      cliPrintf("%d KB/sec\n", 1024 * 1000 / exe_time);
    }
    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("sd info\n");

    if (is_init == true)
    {
      cliPrintf("sd read block_number\n");
      cliPrintf("sd speed-test\n");
    }
  }
}
#endif





#endif
