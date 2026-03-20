#include "ap.h"




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

