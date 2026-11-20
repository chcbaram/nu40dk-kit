#include "cdc.h"


#ifdef _USE_HW_CDC





static bool is_init = false;






bool cdcInit(void)
{
  bool ret = true;


  is_init = ret;

  return ret;
}

bool cdcIsInit(void)
{
  return is_init;
}

bool cdcIsConnect(void)
{
  return 0;
}

uint32_t cdcAvailable(void)
{
  return 0;
}

uint8_t cdcRead(void)
{
  return 0;
}

uint32_t cdcWrite(uint8_t *p_data, uint32_t length)
{
  return 0;
}

uint32_t cdcGetBaud(void)
{
  // return cdcIfGetBaud();
  return 0;
}

uint8_t cdcGetType(void)
{
  // return cdcIfGetType();
  return 0;
}

#endif
