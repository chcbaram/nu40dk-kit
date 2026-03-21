#include "gpio.h"


#ifdef _USE_HW_GPIO
#include "cli.h"
#include <zephyr/drivers/gpio.h>


#define NAME_DEF(x)  x, #x


typedef struct
{
  struct gpio_dt_spec h_dt;
  uint8_t             mode;
  uint8_t             on_state;
  uint8_t             off_state;
  bool                init_value;
  GpioPinName_t       pin_name;
  const char         *p_name;
} gpio_tbl_t;


#if CLI_USE(HW_GPIO)
static void cliGpio(cli_args_t *args);
#endif

static const gpio_tbl_t gpio_tbl[GPIO_MAX_CH] =
{
  {GPIO_DT_SPEC_GET(DT_NODELABEL(i2s_mute), gpios), _DEF_OUTPUT, _DEF_HIGH, _DEF_LOW, _DEF_HIGH, NAME_DEF(I2S_MUTE)},
};

static uint8_t gpio_data[GPIO_MAX_CH];



bool gpioInit(void)
{
  bool ret = true;



  for (int i = 0; i < GPIO_MAX_CH; i++)
  {
    gpio_flags_t extra_flags;

    extra_flags = (gpio_tbl[i].mode & _DEF_OUTPUT) ? GPIO_OUTPUT:GPIO_INPUT;
    if (gpio_pin_configure_dt(&gpio_tbl[i].h_dt, extra_flags) < 0)
    {
      ret = false;
    }

    if (gpio_tbl[i].mode & _DEF_OUTPUT)
    {
      gpioPinWrite(i, gpio_tbl[i].init_value);
    }
  }

#if CLI_USE(HW_GPIO)
  cliAdd("gpio", cliGpio);
#endif

  return ret;
}

void gpioPinWrite(uint8_t ch, uint8_t value)
{
  if (ch >= GPIO_MAX_CH)
  {
    return;
  }

  if (value == _DEF_HIGH)
  {
    gpio_pin_set_dt(&gpio_tbl[ch].h_dt, gpio_tbl[ch].on_state);
  }
  else
  {
    gpio_pin_set_dt(&gpio_tbl[ch].h_dt, gpio_tbl[ch].off_state);
  }

  gpio_data[ch] = value;
}

uint8_t gpioPinRead(uint8_t ch)
{
  uint8_t ret = _DEF_LOW;

  if (ch >= GPIO_MAX_CH)
  {
    return ret;
  }

  if (gpio_pin_get_dt(&gpio_tbl[ch].h_dt) == gpio_tbl[ch].on_state)
  {
    ret = _DEF_HIGH;
  }

    gpio_data[ch] = ret;
  return ret;
}

void gpioPinToggle(uint8_t ch)
{
  if (ch >= GPIO_MAX_CH)
  {
    return;
  }
  
  gpio_data[ch] = !gpio_data[ch];
  gpioPinWrite(ch, gpio_data[ch]);
}





#if CLI_USE(HW_GPIO)
void cliGpio(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    for (int i=0; i<GPIO_MAX_CH; i++)
    {
      cliPrintf("%02d. %s %-16s - %d\n", 
        i,
        gpio_tbl[i].mode & _DEF_INPUT ? "I":"O", 
        gpio_tbl[i].p_name, 
        gpioPinRead(i));
    }
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "show") == true)
  {
    while(cliKeepLoop())
    {
      for (int i=0; i<GPIO_MAX_CH; i++)
      {
        cliPrintf("%d", gpioPinRead(i));
      }
      cliPrintf("\n");
      delay(100);
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "show") && args->isStr(1, "input"))
  {
    uint16_t line = 0;
    
    while(cliKeepLoop())
    {  
      line = 0;
      for (int i=0; i<GPIO_MAX_CH; i++)
      {
        if (gpio_tbl[i].mode & _DEF_INPUT)
        {
          cliPrintf("%02d. %s %-16s - %d\n", 
            i,
            gpio_tbl[i].mode & _DEF_INPUT ? "I":"O",             
            gpio_tbl[i].p_name, 
            gpioPinRead(i));
          line++;
        }
      }
      cliMoveUp(line);
    }
    cliMoveDown(line);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "show") && args->isStr(1, "output"))
  {
    uint16_t line = 0;
    
    while(cliKeepLoop())
    {  
      line = 0;
      for (int i=0; i<GPIO_MAX_CH; i++)
      {
        if (gpio_tbl[i].mode & _DEF_OUTPUT)
        {
          cliPrintf("%02d. %s %-16s - %d\n", 
            i,
            gpio_tbl[i].mode & _DEF_INPUT ? "I":"O", 
            gpio_tbl[i].p_name, 
            gpioPinRead(i));
          line++;
        }
      }
      cliMoveUp(line);
    } 
    cliMoveDown(line);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "read") == true)
  {
    uint8_t ch;

    ch = (uint8_t)args->getData(1);

    while(cliKeepLoop())
    {
      cliPrintf("gpio read %s %d : %d\n", gpio_tbl[ch].p_name, ch, gpioPinRead(ch));
      delay(100);
    }

    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "write") == true)
  {
    uint8_t ch;
    uint8_t data;

    ch   = (uint8_t)args->getData(1);
    data = (uint8_t)args->getData(2);

    gpioPinWrite(ch, data);

    cliPrintf("gpio write %s %d : %d\n", gpio_tbl[ch].p_name, ch, data);
    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("gpio info\n");
    cliPrintf("gpio show\n");
    cliPrintf("gpio show input\n");
    cliPrintf("gpio show output\n");
    cliPrintf("gpio read ch[0~%d]\n", GPIO_MAX_CH-1);
    cliPrintf("gpio write ch[0~%d] 0:1\n", GPIO_MAX_CH-1);
  }
}
#endif


#endif