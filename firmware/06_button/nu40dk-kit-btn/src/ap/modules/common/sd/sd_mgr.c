#include "ap_def.h"


static bool init(void);
static void sdMgrThread(void const *arg);

MODULE_DEF(sd_mgr) 
{
  .name = "sd_mgr",
  .priority = MODULE_PRI_LOW,
  .init = init
};

// K_THREAD_DEFINE(sdMgr_thread,
//                 _HW_DEF_RTOS_THREAD_MEM_CLI,
//                 sdMgrThread, NULL, NULL, NULL,
//                 _HW_DEF_RTOS_THREAD_PRI_CLI, 0, 0);
static K_THREAD_STACK_DEFINE(thread_stack, _HW_DEF_RTOS_THREAD_MEM_SD_MGR);
static struct k_thread thread_data;




bool init(void)
{
  bool ret;

  k_tid_t tid = k_thread_create(&thread_data, thread_stack,
                                  K_THREAD_STACK_SIZEOF(thread_stack),
                                  (k_thread_entry_t)sdMgrThread,
                                  NULL, NULL, NULL,
                                  _HW_DEF_RTOS_THREAD_PRI_SD_MGR, 0, K_NO_WAIT);

  ret = tid != NULL ? true:false;                                  
  logPrintf("[%s] sdMgrInit()\n", ret ? "OK":"E_");

  return ret;
}

void sdMgrThread(void const *arg)
{
  bool init_ret = true;


  moduleIsReady();

  logPrintf("[%s] Thread Started : SD_MGR\n", init_ret ? "OK":"E_" );
  while(1)
  {
    sd_state_t sd_state;


    sd_state = sdUpdate();
    if (sd_state == SDCARD_CONNECTED)
    {
      logPrintf("\n[  ] SDCARD_CONNECTED\n");
    }
    if (sd_state == SDCARD_DISCONNECTED)
    {
      logPrintf("\n[  ] SDCARD_DISCONNECTED\n");
    }  
    delay(10);
  }
}

