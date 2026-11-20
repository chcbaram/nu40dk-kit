#include "epd/ssd1683.h"


#ifdef _USE_HW_EPD
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>


#define EPD_SPI_OP    (SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER)


static struct spi_dt_spec spi_dev = SPI_DT_SPEC_GET(DT_NODELABEL(epd_spi), EPD_SPI_OP, 0);

static const struct gpio_dt_spec pin_busy = GPIO_DT_SPEC_GET(DT_NODELABEL(epd_busy), gpios);
static const struct gpio_dt_spec pin_res  = GPIO_DT_SPEC_GET(DT_NODELABEL(epd_res),  gpios);
static const struct gpio_dt_spec pin_dc   = GPIO_DT_SPEC_GET(DT_NODELABEL(epd_dc),   gpios);

// RAM 전송용 라인 버퍼 (윈도우 전송 시 한 줄씩 보냄)
static uint8_t line_buf[SSD1683_LINE_BYTES];




bool ssd1683Init(void)
{
  if (!spi_is_ready_dt(&spi_dev))
  {
    logPrintf("[E_] epd spi not ready\n");
    return false;
  }
  if (!gpio_is_ready_dt(&pin_busy) || !gpio_is_ready_dt(&pin_res) || !gpio_is_ready_dt(&pin_dc))
  {
    logPrintf("[E_] epd gpio not ready\n");
    return false;
  }

  gpio_pin_configure_dt(&pin_busy, GPIO_INPUT);
  gpio_pin_configure_dt(&pin_res,  GPIO_OUTPUT_HIGH);
  gpio_pin_configure_dt(&pin_dc,   GPIO_OUTPUT_HIGH);

  return true;
}

bool ssd1683Reset(void)
{
  gpio_pin_set_dt(&pin_res, 0);
  delay(10);
  gpio_pin_set_dt(&pin_res, 1);
  delay(10);

  return ssd1683WaitBusy(100);
}

bool ssd1683IsBusy(void)
{
  return gpio_pin_get_dt(&pin_busy) ? true : false;
}

bool ssd1683WaitBusy(uint32_t timeout_ms)
{
  uint32_t pre_time = millis();

  while (ssd1683IsBusy())
  {
    if (millis() - pre_time >= timeout_ms)
    {
      return false;
    }
    k_sleep(K_MSEC(1));
  }
  return true;
}

static void spiWrite(const uint8_t *p_data, uint32_t length)
{
  struct spi_buf     buf  = {.buf = (void *)p_data, .len = length};
  struct spi_buf_set bufs = {.buffers = &buf, .count = 1};

  spi_write_dt(&spi_dev, &bufs);
}

void ssd1683WriteCmd(uint8_t cmd)
{
  gpio_pin_set_dt(&pin_dc, 0);
  spiWrite(&cmd, 1);
  gpio_pin_set_dt(&pin_dc, 1);
}

void ssd1683WriteData(const uint8_t *p_data, uint32_t length)
{
  if (length == 0) return;
  gpio_pin_set_dt(&pin_dc, 1);
  spiWrite(p_data, length);
}

void ssd1683WriteCmdData(uint8_t cmd, const uint8_t *p_data, uint32_t length)
{
  ssd1683WriteCmd(cmd);
  ssd1683WriteData(p_data, length);
}

// x, w 는 8 의 배수로 맞춰서 사용 (바이트 단위 주소)
//
void ssd1683SetWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint8_t  data[4];
  uint16_t xs = x / 8;
  uint16_t xe = (x + w - 1) / 8;
  uint16_t ys = y;
  uint16_t ye = y + h - 1;

  data[0] = xs;
  data[1] = xe;
  ssd1683WriteCmdData(0x44, data, 2);

  data[0] = ys & 0xFF;
  data[1] = (ys >> 8) & 0x01;
  data[2] = ye & 0xFF;
  data[3] = (ye >> 8) & 0x01;
  ssd1683WriteCmdData(0x45, data, 4);

  data[0] = xs;
  ssd1683WriteCmdData(0x4E, data, 1);

  data[0] = ys & 0xFF;
  data[1] = (ys >> 8) & 0x01;
  ssd1683WriteCmdData(0x4F, data, 2);
}

void ssd1683WriteRam(ssd1683_ram_t ram, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const uint8_t *p_fb, uint16_t fb_stride)
{
  uint16_t xs = x / 8;
  uint16_t xe = (x + w - 1) / 8;
  uint16_t bytes = xe - xs + 1;

  ssd1683SetWindow(x, y, w, h);
  ssd1683WriteCmd((uint8_t)ram);

  if (xs == 0 && bytes == fb_stride)
  {
    // 전체 폭이면 한 번에 DMA 전송
    ssd1683WriteData(&p_fb[y * fb_stride], (uint32_t)bytes * h);
  }
  else
  {
    for (uint16_t i = 0; i < h; i++)
    {
      memcpy(line_buf, &p_fb[(y + i) * fb_stride + xs], bytes);
      ssd1683WriteData(line_buf, bytes);
    }
  }
}

void ssd1683Activate(uint8_t update_ctrl2)
{
  ssd1683WriteCmdData(0x22, &update_ctrl2, 1);
  ssd1683WriteCmd(0x20);
}

void ssd1683DeepSleep(void)
{
  uint8_t data = 0x01;
  ssd1683WriteCmdData(0x10, &data, 1);
}

bool ssd1683SetSpiFreq(uint32_t freq_hz)
{
  spi_dev.config.frequency = freq_hz;
  return true;
}

uint32_t ssd1683GetSpiFreq(void)
{
  return spi_dev.config.frequency;
}

#endif
