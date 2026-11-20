#include "ap.h"

LOG_MODULE_REGISTER(ap, LOG_LEVEL_DBG);



void apInit(void)
{    
  moduleInit();
}

void apMain(void)
{
  while(1)
  {
    ledToggle(_DEF_CH1);
    delay(500);
  }
}

