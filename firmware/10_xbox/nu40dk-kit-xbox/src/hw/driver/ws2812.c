#include "ws2812.h"



#ifdef _USE_HW_WS2812
#include "cli.h"
#include <zephyr/drivers/led_strip.h>


#define BIT_PERIOD      (104) // 1300ns, 80Mhz
#define BIT_HIGH        (56)  // 700ns
#define BIT_LOW         (28)  // 350ns
#define BIT_ZERO        (50)




typedef struct
{
  uint16_t led_cnt;
} ws2812_t;

static bool                 is_init = false;
static struct led_rgb       color_buf[WS2812_MAX_CH];
static const struct device *h_led = DEVICE_DT_GET(DT_NODELABEL(led_strip));
static ws2812_t             ws2812;


#if CLI_USE(HW_WS2812)
static void cliCmd(cli_args_t *args);
#endif






bool ws2812Init(void)
{

  memset(color_buf, 0, sizeof(color_buf));

  if (!device_is_ready(h_led))
  {    
    logPrintf("[E_] ws2812 : device is not ready");
    return false;
  }

  ws2812.led_cnt = WS2812_MAX_CH;
  is_init = true;

  for (int i=0; i<WS2812_MAX_CH; i++)
  {
    ws2812SetColor(i, WS2812_COLOR_OFF);
  }
  ws2812Refresh();


  logPrintf("[%s] ws2812Init()\n", is_init ? "OK" : "E_");  

#if CLI_USE(HW_WS2812)
  cliAdd("ws2812", cliCmd);
#endif
  return true;
}

bool ws2812Refresh(void)
{
  led_strip_update_rgb(h_led, color_buf, WS2812_MAX_CH);
  return true;
}

void ws2812SetColor(uint32_t ch, uint32_t color)
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;  

  if (ch >= WS2812_MAX_CH)
    return;

  red   = (color >> 16) & 0xFF;
  green = (color >> 8) & 0xFF;
  blue  = (color >> 0) & 0xFF;

  color_buf[ch].r = red;
  color_buf[ch].g = green;
  color_buf[ch].b = blue;
}


#if CLI_USE(HW_WS2812)
void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("ws2812 led cnt : %d\n", WS2812_MAX_CH);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "test"))
  {
    uint32_t color[6] = {WS2812_COLOR_RED,
                         WS2812_COLOR_OFF,
                         WS2812_COLOR_GREEN,
                         WS2812_COLOR_OFF,
                         WS2812_COLOR_BLUE,
                         WS2812_COLOR_OFF};

    uint8_t color_idx = 0;
    uint32_t pre_time;


    pre_time = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time >= 500)
      {
        pre_time = millis();
        
        for (int i=0; i<WS2812_MAX_CH; i++)
        {      
          ws2812SetColor(i, color[color_idx]);
        }
        ws2812Refresh();
        color_idx = (color_idx + 1) % 6;
      }
      delay(1);
    }

    for (int i=0; i<WS2812_MAX_CH; i++)
    {
      ws2812SetColor(i, WS2812_COLOR_OFF);
    }
    ws2812Refresh();

    ret = true;
  }


  if (args->argc == 5 && args->isStr(0, "color"))
  {
    uint8_t  ch;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    ch    = (uint8_t)args->getData(1);
    red   = (uint8_t)args->getData(2);
    green = (uint8_t)args->getData(3);
    blue  = (uint8_t)args->getData(4);

    ws2812SetColor(ch, WS2812_COLOR(red, green, blue));
    ws2812Refresh();

    while(cliKeepLoop())
    {
      delay(10);
    }
    ws2812SetColor(ch, 0);
    ws2812Refresh();
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("ws2812 info\n");
    cliPrintf("ws2812 test\n");
    cliPrintf("ws2812 color ch r g b\n");
  }
}
#endif

#endif