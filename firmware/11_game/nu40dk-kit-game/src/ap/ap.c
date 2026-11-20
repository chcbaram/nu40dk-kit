#include "ap.h"
#include "launcher/launcher.h"

LOG_MODULE_REGISTER(ap, LOG_LEVEL_DBG);



void apInit(void)
{    
  moduleInit();
}

void apMain(void)
{
  launcherMain();
}

