#include "button.h"


#ifdef _USE_HW_BUTTON
#include "cli.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#ifdef _USE_HW_LCD
#include "lcd.h"
#endif
#ifdef _USE_HW_WS2812
#include "ws2812.h"
#endif


typedef struct
{
  struct gpio_dt_spec h_dt;
  uint32_t            pull;
  uint8_t             on_state;
} button_pin_t;


#if CLI_USE(HW_BUTTON)
static void cliButton(cli_args_t *args);
#endif
static bool buttonGetPin(uint8_t ch);
static void buttonInputCB(struct input_event *evt, void *user_data);


static bool   is_log         = false;
static bool   is_log_pressed = false;
static int8_t log_keycode    = -1;

static const button_pin_t button_pin[BUTTON_MAX_CH] =
{
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn0), gpios), _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn1), gpios), _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn2), gpios), _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn3), gpios), _DEF_LOW},
};


INPUT_CALLBACK_DEFINE(NULL, buttonInputCB, NULL);




bool buttonInit(void)
{
  bool ret = true;



  for (int i = 0; i < BUTTON_MAX_CH; i++)
  {
    if (gpio_pin_configure_dt(&button_pin[i].h_dt, GPIO_INPUT) < 0)
    {
      ret = false;
    }
  }

#if CLI_USE(HW_BUTTON)
  cliAdd("button", cliButton);
#endif

  return ret;
}

static void buttonInputCB(struct input_event *evt, void *user_data)
{
  if (evt->sync == 0)
  {
    return;
  }

  if (is_log)
  {
    is_log_pressed = true;
    log_keycode = evt->code;

    logPrintf("[  ] Button %d %s at %" PRIu32 "\n",
              evt->code,
              evt->value ? "pressed" : "released",
              millis());
  }
}

bool buttonGetPin(uint8_t ch)
{
  bool ret = false;

  if (ch >= BUTTON_MAX_CH)
  {
    return false;
  }

  if (gpio_pin_get_dt(&button_pin[ch].h_dt) == button_pin[ch].on_state)
  {
    ret = true;
  }

  return ret;
}


bool buttonGetPressed(uint8_t ch)
{
  if (ch >= BUTTON_MAX_CH)
  {
    return false;
  }

  return buttonGetPin(ch);
}

uint32_t buttonGetData(void)
{
  uint32_t ret = 0;


  for (int i=0; i<BUTTON_MAX_CH; i++)
  {
    ret |= (buttonGetPressed(i)<<i);
  }

  return ret;
}

uint8_t  buttonGetPressedCount(void)
{
  uint32_t i;
  uint8_t ret = 0;

  for (i=0; i<BUTTON_MAX_CH; i++)
  {
    if (buttonGetPressed(i) == true)
    {
      ret++;
    }
  }

  return ret;
}


#if CLI_USE(HW_BUTTON)
void cliButton(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "log"))
  { 
    #ifdef _USE_HW_LCD      
    uint32_t pre_time;
    uint8_t  state = 0;
    uint8_t  keycode;
    #endif

    is_log = true;
    while(cliKeepLoop())
    {
      #ifdef _USE_HW_LCD      
      if (lcdDrawAvailable())
      {
        lcdClearBuffer(black);
       
        switch(state)
        {
          case 0:            
            if (is_log_pressed)
            {
              is_log_pressed = false;
              pre_time = millis();
              keycode = log_keycode;
              state = 1;
            }
            break;

          case 1:
            lcdPrintf(0, 0, white, "%d 눌림", keycode);
            if (is_log_pressed)
            {              
              is_log_pressed = false;
              pre_time = millis();
              keycode = log_keycode;
            }    
            if (millis()-pre_time >= 500)
            {
              log_keycode = -1;
              state = 0;
            }    
            break;
        }
        
        lcdRequestDraw();
      }
      #endif

      delay(10);
    }
    is_log = false;

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "show"))
  {    
    #ifdef _USE_HW_WS2812
    #endif
    while(cliKeepLoop())
    {
      for (int i=0; i<BUTTON_MAX_CH; i++)
      {
        cliPrintf("%d", buttonGetPressed(i));

        #ifdef _USE_HW_WS2812
        if (buttonGetPressed(i))
          ws2812SetColor(i, WS2812_COLOR_GREEN);
        else
          ws2812SetColor(i, WS2812_COLOR_OFF);
        #endif

      }
      #ifdef _USE_HW_WS2812
      ws2812Refresh();    
      #endif
      delay(50);
      cliPrintf("\r");
    }
    #ifdef _USE_HW_WS2812
    for (int i=0; i<BUTTON_MAX_CH; i++)
    {
      ws2812SetColor(i, 0);
    }
    ws2812Refresh();    
    #endif
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("button info\n");
    cliPrintf("button log\n");
    cliPrintf("button show\n");
  }
}
#endif



#endif