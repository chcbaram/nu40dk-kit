#include "bsp.h"
#include <hal/nrf_power.h>





bool bspInit(void)
{
  bool ret = true;


  #if NRF54L_ERRATA_20_PRESENT
  if (nrf54l_errata_20())
  {
    nrf_power_task_trigger(NRF_POWER, NRF_POWER_TASK_CONSTLAT);
  }
  #endif 

  return ret;
}

void delay(uint32_t ms)
{
  if (ms > 0)
  {
    k_msleep(ms);
  }
}

uint32_t millis(void)
{
  return k_uptime_get_32();
}



