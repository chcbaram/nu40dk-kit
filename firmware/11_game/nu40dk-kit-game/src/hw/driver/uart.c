#include "uart.h"


#ifdef _USE_HW_UART
#include "qbuffer.h"
#include "cli.h"
#include <zephyr/console/console.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/uart.h>

#define UART_RX_BUF_LENGTH        1024


enum
{
  HW_TYPE_UART,
  HW_TYPE_CONSOLE,
};


typedef struct
{
  bool is_open;
  uint32_t baud;

  uint8_t  rx_buf[UART_RX_BUF_LENGTH];
  qbuffer_t qbuffer;

  uint32_t rx_cnt;
  uint32_t tx_cnt;
} uart_tbl_t;

typedef struct
{
  uint8_t              type;
  const struct device *h_uart;
  const char          *p_msg;
  bool                 is_rs485;
} uart_hw_t;



#if CLI_USE(HW_UART)
static void cliUart(cli_args_t *args);
#endif
static int  uartConsoleThread(void *args);
// static int  uartReadThread(void *args);
static void uartISR(const struct device *dev, void *user_data);


static bool is_init = false;

static uart_tbl_t         uart_tbl[UART_MAX_CH];
static struct uart_config uart_cfg[UART_MAX_CH];
static uint8_t            console_rx_buf[UART_RX_BUF_LENGTH];

const static uart_hw_t uart_hw_tbl[UART_MAX_CH] =
{
  {HW_TYPE_CONSOLE, DEVICE_DT_GET(DT_NODELABEL(uart0)),   "console      ", false},
  {HW_TYPE_UART,    DEVICE_DT_GET(DT_NODELABEL(usb_cdc)), "usb cdc      ", false},
};


K_THREAD_DEFINE(uart_console_thread,
                _HW_DEF_RTOS_THREAD_MEM_UART,
                uartConsoleThread, NULL, NULL, NULL,
                _HW_DEF_RTOS_THREAD_PRI_UART, 0, 50);

// K_THREAD_DEFINE(uart_read_thread,
//                 _HW_DEF_RTOS_THREAD_MEM_UART,
//                 uartReadThread, NULL, NULL, NULL,
//                 _HW_DEF_RTOS_THREAD_PRI_UART, 0, 50);


bool uartInit(void)
{
  for (int i=0; i<UART_MAX_CH; i++)
  {
    uart_tbl[i].is_open = false;
    uart_tbl[i].baud = 57600;
    uart_tbl[i].rx_cnt = 0;
    uart_tbl[i].tx_cnt = 0;    
  }

  console_init();

  is_init = true;

#if CLI_USE(HW_UART)
  cliAdd("uart", cliUart);
#endif
  return true;
}

bool uartDeInit(void)
{
  return true;
}

bool uartIsInit(void)
{
  return is_init;
}

bool uartSuspend(uint8_t ch)
{
  int err;

  err = device_is_ready(uart_hw_tbl[ch].h_uart);
  if (!err)
  {
    logPrintf("[E_] uart : UART device is not ready, err: %d\n", err);
    return false;
  }
    
  pm_device_action_run(uart_hw_tbl[ch].h_uart, PM_DEVICE_ACTION_SUSPEND);
  
  return true;
}

bool uartResume(uint8_t ch)
{
  int err;

  err = device_is_ready(uart_hw_tbl[ch].h_uart);
  if (!err)
  {
    logPrintf("[E_] uart : UART device is not ready, err: %d\n", err);
    return false;
  }
    
  pm_device_action_run(uart_hw_tbl[ch].h_uart, PM_DEVICE_ACTION_RESUME);
  
  return true;
}

bool uartOpen(uint8_t ch, uint32_t baud)
{
  bool ret = false;
  

  if (ch >= UART_MAX_CH) return false;

  if (uart_tbl[ch].is_open == true && uart_tbl[ch].baud == baud)
  {
    return true;
  }


  if (uart_hw_tbl[ch].type == HW_TYPE_CONSOLE)
  {
    uart_tbl[ch].baud = baud;
    qbufferCreate(&uart_tbl[ch].qbuffer, &uart_tbl[ch].rx_buf[0], UART_RX_BUF_LENGTH);
    ret                  = true;
    uart_tbl[ch].is_open = true;
  }

  if (uart_hw_tbl[ch].type == HW_TYPE_UART)
  {
    switch(ch)
    {
      default:
        uart_cfg[ch].baudrate  = baud;
        uart_cfg[ch].parity    = UART_CFG_PARITY_NONE;
        uart_cfg[ch].stop_bits = UART_CFG_STOP_BITS_1;
        uart_cfg[ch].data_bits = UART_CFG_DATA_BITS_8;
        uart_cfg[ch].flow_ctrl = UART_CFG_FLOW_CTRL_NONE;

        uart_tbl[ch].baud      = baud;
        qbufferCreate(&uart_tbl[ch].qbuffer, &uart_tbl[ch].rx_buf[0], UART_RX_BUF_LENGTH);

        if (!uart_tbl[ch].is_open)
        {
          uint32_t user_data = ch;

          uart_irq_callback_user_data_set(uart_hw_tbl[ch].h_uart, uartISR, (void *)user_data);
          uart_irq_rx_enable(uart_hw_tbl[ch].h_uart);
        }

        if (uart_configure(uart_hw_tbl[ch].h_uart, &uart_cfg[ch]) == 0)
        {
          ret = true;
          uart_tbl[ch].is_open = true;
        }
        break;
    }
  }

  return ret;
}

bool uartClose(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return false;

  uart_tbl[ch].is_open = false;

  return true;
}

void uartISR(const struct device *dev, void *user_data)
{
  uint32_t ch = (uint32_t)user_data;


  while (uart_irq_update(dev) && uart_irq_is_pending(dev))
  {
    if (uart_irq_rx_ready(dev))
    {
      int     recv_len;
      uint8_t buffer[64];
      
      recv_len = uart_fifo_read(dev, buffer, sizeof(buffer));
      if (recv_len > 0)
      {
        qbufferWrite(&uart_tbl[ch].qbuffer, buffer, recv_len);    
      }
    }
  }
}

static int uartConsoleThread(void *args)
{
  ARG_UNUSED(args);


  // logPrintf("[  ] uartConsoleThread()\n");
  while (1)
  {
    size_t rx_len;

    rx_len = console_read(NULL, console_rx_buf, 1);
    qbufferWrite(&uart_tbl[0].qbuffer, console_rx_buf, rx_len);    
  }
  return 0;
}

// static int uartReadThread(void *args)
// {
//   ARG_UNUSED(args);


//   // logPrintf("[  ] uartReadThread()\n");
//   while (1)
//   {
//     for (int ch=0; ch<UART_MAX_CH; ch++)
//     {
//       if (uart_hw_tbl[ch].type == HW_TYPE_UART)
//       {
//         uint8_t rx_data;

//         for (int i=0; i<128; i++)
//         {
//           if (uart_poll_in(uart_hw_tbl[ch].h_uart, &rx_data) == 0)
//           {
//             qbufferWrite(&uart_tbl[ch].qbuffer, &rx_data, 1);    
//           }
//           else
//           {
//             break;
//           }
//         }        
//       }
//     }
//     delay(1);
//   }
//   return 0;
// }

uint32_t uartAvailable(uint8_t ch)
{
  uint32_t ret = 0;


  switch(ch)
  {
    default:
      ret = qbufferAvailable(&uart_tbl[ch].qbuffer);      
      break;    
  }

  return ret;
}

bool uartFlush(uint8_t ch)
{
  uint32_t pre_time;


  pre_time = millis();
  while(uartAvailable(ch))
  {
    if (millis()-pre_time >= 10)
    {
      break;
    }
    uartRead(ch);
  }

  return true;
}

uint8_t uartRead(uint8_t ch)
{
  uint8_t ret = 0;


  switch(ch)
  {
    default:
      qbufferRead(&uart_tbl[ch].qbuffer, &ret, 1);
      break;
  }
  uart_tbl[ch].rx_cnt++;

  return ret;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length)
{
  uint32_t ret = 0;


  if (uart_hw_tbl[ch].type == HW_TYPE_CONSOLE)
  {
    ret = console_write(NULL, (const void *)p_data, length);
  }

  if (uart_hw_tbl[ch].type == HW_TYPE_UART)
  {
    while(ret < length)
    {
      uart_poll_out	(uart_hw_tbl[ch].h_uart, p_data[ret]);
      ret++;
    }
  }  
  uart_tbl[ch].tx_cnt += ret;

  return ret;
}

uint32_t uartPrintf(uint8_t ch, const char *fmt, ...)
{
  char buf[256];
  va_list args;
  int len;
  uint32_t ret;

  va_start(args, fmt);
  len = vsnprintf(buf, 256, fmt, args);

  ret = uartWrite(ch, (uint8_t *)buf, len);

  va_end(args);


  return ret;
}

uint32_t uartGetBaud(uint8_t ch)
{
  uint32_t ret = 0;


  if (ch >= UART_MAX_CH) return 0;

  #if HW_USE_CDC == 1
  if (ch == HW_UART_CH_USB)
    ret = cdcGetBaud();
  else
    ret = uart_tbl[ch].baud;
  #else
  ret = uart_tbl[ch].baud;
  #endif
  
  return ret;
}

uint32_t uartGetRxCnt(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return 0;

  return uart_tbl[ch].rx_cnt;
}

uint32_t uartGetTxCnt(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return 0;

  return uart_tbl[ch].tx_cnt;
}

#if CLI_USE(HW_UART)
void cliUart(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    for (int i=0; i<UART_MAX_CH; i++)
    {
      cliPrintf("_DEF_UART%d : %s, %d bps\n", i+1, uart_hw_tbl[i].p_msg, uartGetBaud(i));
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "test"))
  {
    uint8_t uart_ch;

    uart_ch = constrain(args->getData(1), 1, UART_MAX_CH) - 1;

    if (uart_ch != cliGetPort())
    {
      uint8_t rx_data;

      while(1)
      {
        if (uartAvailable(uart_ch) > 0)
        {
          rx_data = uartRead(uart_ch);
          cliPrintf("<- _DEF_UART%d RX : 0x%X\n", uart_ch + 1, rx_data);
        }

        if (cliAvailable() > 0)
        {
          rx_data = cliRead();
          if (rx_data == 'q')
          {
            break;
          }
          else
          {
            uartWrite(uart_ch, &rx_data, 1);
            cliPrintf("-> _DEF_UART%d TX : 0x%X\n", uart_ch + 1, rx_data);            
          }
        }

        delay(1);
      }
    }
    else
    {
      cliPrintf("This is cliPort\n");
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("uart info\n");
    cliPrintf("uart test ch[1~%d]\n", HW_UART_MAX_CH);
  }
}
#endif


#endif

