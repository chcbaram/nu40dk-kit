#include "i2s.h"



#ifdef _USE_HW_I2S
#include <zephyr/drivers/i2s.h>
#include "cli.h"
#include "mixer.h"
#include "buzzer.h"


#ifdef _USE_HW_RTOS
#define lock()      k_mutex_lock(&mutex_lock, K_FOREVER);
#define unLock()    k_mutex_unlock(&mutex_lock);
static K_MUTEX_DEFINE(mutex_lock);
#else
#define lock()      
#define unLock()    
#endif


#define I2S_MAX_CH      1

#define I2S_SAMPLERATE_HZ       16000
#define I2S_BUF_MS              (4)
#define I2S_BUF_CNT             16
#define I2S_BUF_FRAME_LEN       ((48000 * 2 * I2S_BUF_MS) / 1000)  // 48Khz, Stereo, 4ms


#if HW_I2S_LCD > 0
#include "lcd.h"
#define FFT_LEN         512
#define BLOCK_X_CNT     12
#define BLOCK_Y_CNT     12

typedef struct
{
  uint32_t pre_time_lcd;
  uint8_t  update_cnt;
  q15_t    buf_q15[FFT_LEN*2];

  uint8_t block_target[BLOCK_X_CNT];
  uint8_t block_peak[BLOCK_X_CNT];
  uint8_t block_value[BLOCK_X_CNT];

} i2s_cli_t;

static void drawBlock(int16_t bx, int16_t by, uint16_t color);
static bool lcdUpdate(i2s_cli_t *p_args);
#endif

typedef struct
{
  int16_t volume;
} i2s_cfg_t;

typedef struct
{
  const struct device *h_i2s;  
} i2s_hw_t;

typedef struct
{
  bool is_open;  
  struct i2s_config i2s_cfg;
  i2s_hw_t *p_hw;
} i2s_t;



static void i2sThread(void *arg1, void *arg2, void *arg3);
#if CLI_USE(HW_I2S)
static void cliI2s(cli_args_t *args);
#endif

static bool      is_init         = false;
static bool      is_started      = false;
static uint8_t   i2s_ch          = _DEF_CH1;
static uint32_t  i2s_sample_rate = I2S_SAMPLERATE_HZ;
static uint32_t  i2s_frame_len   = 0;
static int16_t   i2s_volume      = 100;
static mixer_t   mixer;
static i2s_cfg_t i2s_cfg;
static int16_t   i2s_frame_buf[I2S_BUF_FRAME_LEN];


K_THREAD_STACK_DEFINE(i2s_stack_area, _HW_DEF_RTOS_THREAD_MEM_I2S);
static struct k_thread i2s_thread_data;
static i2s_t i2s_tbl[I2S_MAX_CH];  

K_MEM_SLAB_DEFINE(tx_mem_slab, I2S_BUF_FRAME_LEN * 2, I2S_BUF_CNT, 32);

static i2s_hw_t i2s_hw_tbl[I2S_MAX_CH] = 
  {
    {DEVICE_DT_GET(DT_NODELABEL(i2s0))},    
  };






bool i2sInit(void)
{
  bool ret = true;


  for (int i=0; i<I2S_MAX_CH; i++)
  {
    i2s_tbl[i].is_open = false;
    i2s_tbl[i].p_hw    = (i2s_hw_t *)&i2s_hw_tbl[i];

    if (!device_is_ready(i2s_tbl[i].p_hw->h_i2s))
    {
      logPrintf("[E_] I2S device not ready\n");
      ret = false;
    }
  }

  i2s_frame_len = (i2s_sample_rate * 2 * I2S_BUF_MS) / 1000;

  mixerInit(&mixer);
  
  k_thread_create(&i2s_thread_data, i2s_stack_area,
                  K_THREAD_STACK_SIZEOF(i2s_stack_area),
                  i2sThread,
                  NULL, NULL, NULL,
                  _HW_DEF_RTOS_THREAD_PRI_I2S, 0, K_NO_WAIT);

  is_init = ret;
  
  logPrintf("[%s] i2sInit()\n", ret ? "OK" : "E_");  

#if CLI_USE(HW_I2S)
  cliAdd("i2s", cliI2s);
#endif  
  return ret;
}

bool i2sCfgLoad(void)
{
  bool ret = true;

  #if 0
  // if (nvsGet(I2S_CFG_NAME, &i2s_cfg, sizeof(i2s_cfg)) == true)
  // {
  //   i2s_volume = i2s_cfg.volume;
  // }
  // else
  // {
  //   i2s_cfg.volume = i2s_volume;
  //   ret = nvsSet(I2S_CFG_NAME, &i2s_cfg, sizeof(i2s_cfg));
  //   logPrintf("[NG] i2sCfgLoad()\n");
  // }
  #else
  i2s_cfg.volume = i2s_volume;
  #endif

  i2sSetVolume(i2s_cfg.volume);
  return ret;
}

bool i2sCfgSave(void)
{
  bool ret = true;

  i2s_cfg.volume = i2s_volume;
  // ret = nvsSet(I2S_CFG_NAME, &i2s_cfg, sizeof(i2s_cfg));

  return ret;
}

bool i2sSetSampleRate(uint32_t freq)
{
  bool ret = true;

  // gpioPinWrite(_PIN_GPIO_SPK_EN, _DEF_LOW);

  i2sStop();

  lock();
  i2s_sample_rate = freq;
  i2s_frame_len   = (i2s_sample_rate * 2 * I2S_BUF_MS) / 1000;


  int i2s_ret;

  /* Configure I2S stream */
  i2s_tbl[i2s_ch].i2s_cfg.word_size      = 16U;
  i2s_tbl[i2s_ch].i2s_cfg.channels       = 2U;
  i2s_tbl[i2s_ch].i2s_cfg.format         = I2S_FMT_DATA_FORMAT_I2S;
  i2s_tbl[i2s_ch].i2s_cfg.frame_clk_freq = i2s_sample_rate;
  i2s_tbl[i2s_ch].i2s_cfg.block_size     = I2S_BUF_FRAME_LEN * 2;
  i2s_tbl[i2s_ch].i2s_cfg.timeout        = 1000;

  /* Configure the Transmit port as Master */
  i2s_tbl[i2s_ch].i2s_cfg.options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER;

  i2s_tbl[i2s_ch].i2s_cfg.mem_slab = &tx_mem_slab;
  i2s_ret                          = i2s_configure(i2s_tbl[i2s_ch].p_hw->h_i2s, I2S_DIR_TX, &i2s_tbl[i2s_ch].i2s_cfg);
  if (i2s_ret == 0)
  {
    ret = true;
  }
  else
  {
    logPrintf("[E_] i2s_configure()\n");
    ret = false;
  }
  unLock();

  i2sStart();

  // gpioPinWrite(_PIN_GPIO_SPK_EN, _DEF_HIGH);

  return ret;
}

uint32_t i2sGetSampleRate(void)
{
  return i2s_sample_rate;
}

bool i2sStart(void)
{
  bool ret = false;
  int i2s_ret;


  lock();
  i2s_ret = i2s_trigger(i2s_tbl[i2s_ch].p_hw->h_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
  if (i2s_ret == 0)
  {
    is_started = true;
  }
  else
  {
    is_started = false;
    logPrintf("[E_] i2s_trigger(I2S_TRIGGER_START)\n");    
  }
  unLock();
  ret = is_started;

  return ret;
}

bool i2sStop(void)
{
  if (!is_started)
    return true;

  lock();
  is_started = false;
  i2s_trigger(i2s_tbl[i2s_ch].p_hw->h_i2s, I2S_DIR_TX, I2S_TRIGGER_STOP);
  unLock();

  return true;
}

int8_t i2sGetEmptyChannel(void)
{
  return mixerGetEmptyChannel(&mixer);
}

uint32_t i2sGetFrameSize(void)
{
  return i2s_frame_len;
}

uint32_t i2sAvailableForWrite(uint8_t ch)
{
  return mixerAvailableForWrite(&mixer, ch);
}

bool i2sWrite(uint8_t ch, int16_t *p_data, uint32_t length)
{
  return mixerWrite(&mixer, ch, p_data, length);
}

uint32_t i2sWriteTimeout(uint8_t ch, int16_t *p_data, uint32_t length, uint32_t timeout)
{
  uint32_t pre_time;
  uint32_t buf_i;
  uint32_t remain;
  uint32_t wr_len;

  buf_i = 0;
  pre_time = millis();
  while(buf_i < length)
  {
    remain = length - buf_i;
    wr_len = cmin(mixerAvailableForWrite(&mixer, ch), remain);

    mixerWrite(&mixer, ch, &p_data[buf_i], wr_len);
    buf_i += wr_len;

    if (millis()-pre_time >= timeout)
    {
      break;
    }
    #ifdef _USE_HW_RTOS
    delay(1);
    #endif
  }

  return buf_i;
}

// https://m.blog.naver.com/PostView.nhn?blogId=hojoon108&logNo=80145019745&proxyReferer=https:%2F%2Fwww.google.com%2F
//
float i2sGetNoteHz(int8_t octave, int8_t note)
{
  float hz;
  float f_note;

  if (octave < 1) octave = 1;
  if (octave > 8) octave = 8;

  if (note <  1) note = 1;
  if (note > 12) note = 12;

  f_note = (float)(note-10)/12.0f;

  hz = pow(2, (octave-1)) * 55 * pow(2, f_note);

  return hz;
}

// https://gamedev.stackexchange.com/questions/4779/is-there-a-faster-sine-function
//
float i2sSin(float x)
{
  const float B = 4 / M_PI;
  const float C = -4 / (M_PI * M_PI);

  return -(B * x + C * x * ((x < 0) ? -x : x));
}

#if HW_I2S_LCD > 0
bool i2sPlayBeepLcd(i2s_cli_t *p_i2s_cli, uint32_t freq_hz, uint16_t volume, uint32_t time_ms)
{
  uint32_t pre_time;
  int32_t sample_rate = i2s_sample_rate;
  int32_t num_samples = i2s_frame_len;
  float sample_point;
  int16_t sample[num_samples];
  int16_t sample_index = 0;
  float div_freq;
  int8_t mix_ch;
  int32_t volume_out;
  uint32_t q15_buf_index = 0;


  volume = constrain(volume, 0, 100);
  volume_out = (INT16_MAX/40) * volume / 100;

  mix_ch =  i2sGetEmptyChannel();

  div_freq = (float)sample_rate/(float)freq_hz;

  pre_time = millis();
  while(millis()-pre_time <= time_ms)
  {
    if (i2sAvailableForWrite(mix_ch) >= num_samples)
    {
      for (int i=0; i<num_samples; i+=2)
      {
        sample_point = i2sSin(2.0f * (float)M_PI * (float)(sample_index) / ((float)div_freq));
        sample[i + 0] = (int16_t)(sample_point * volume_out);
        sample[i + 1] = (int16_t)(sample_point * volume_out);
        sample_index = (sample_index + 1) % (int)(div_freq);

        if (q15_buf_index < 512)
        {
          p_i2s_cli->buf_q15[q15_buf_index*2 + 0] = sample[i + 0];
          p_i2s_cli->buf_q15[q15_buf_index*2 + 1] = 0;
          
          q15_buf_index++;            
        }
      }
      i2sWrite(mix_ch, (int16_t *)sample, num_samples);
    }
    delay(1);

    if (q15_buf_index == 512)
    {        
      if (lcdUpdate(p_i2s_cli) == true)
      {
        q15_buf_index = 0;
      }        
    }
  }

  lcdClear(black);

  return true;
}
#endif

bool i2sPlayBeep(uint32_t freq_hz, uint16_t volume, uint32_t time_ms)
{
  uint32_t pre_time;
  int32_t sample_rate = i2s_sample_rate;
  int32_t num_samples = i2s_frame_len;
  float sample_point;
  int16_t sample[num_samples];
  int16_t sample_index = 0;
  float div_freq;
  int8_t mix_ch;
  int32_t volume_out;


  volume = constrain(volume, 0, 100);
  volume_out = (INT16_MAX/40) * volume / 100;

  mix_ch =  i2sGetEmptyChannel();

  div_freq = (float)sample_rate/(float)freq_hz;

  pre_time = millis();
  while(millis()-pre_time <= time_ms)
  {
    if (i2sAvailableForWrite(mix_ch) >= num_samples)
    {
      for (int i=0; i<num_samples; i+=2)
      {
        sample_point = i2sSin(2.0f * (float)M_PI * (float)(sample_index) / ((float)div_freq));
        sample[i + 0] = (int16_t)(sample_point * volume_out);
        sample[i + 1] = (int16_t)(sample_point * volume_out);
        sample_index = (sample_index + 1) % (int)(div_freq);
      }
      i2sWrite(mix_ch, (int16_t *)sample, num_samples);
    }
    delay(2);
  }

  return true;
}

int16_t i2sGetVolume(void)
{
  return i2s_volume;
}

bool i2sSetVolume(int16_t volume)
{
  volume = constrain(volume, 0, 100);
  i2s_volume = volume;

  mixerSetVolume(&mixer, i2s_volume);
  return true;
}

void i2sThread(void *arg1, void *arg2, void *arg3)
{
  uint32_t pre_time;
  int i2s_ret;

  logPrintf("[  ] i2sThread()\n");


  i2sSetSampleRate(I2S_SAMPLERATE_HZ);

  while (1)
  {
    if (mixerAvailable(&mixer) >= i2s_frame_len)
    {    
      mixerRead(&mixer, &i2s_frame_buf[0], i2s_frame_len);
    }
    else
    {
      memset(&i2s_frame_buf[0], 0, i2s_frame_len * 2);
    }
    
    pre_time = millis();
    while(millis()-pre_time <= 10)
    {      
      i2s_ret = i2s_buf_write(i2s_tbl[i2s_ch].p_hw->h_i2s, i2s_frame_buf, i2s_frame_len * 2);
      if (i2s_ret == 0)
      {
        break;
      }
      if (i2s_ret != -ENOMEM)
      {
        i2s_trigger(i2s_tbl[i2s_ch].p_hw->h_i2s, I2S_DIR_TX, I2S_TRIGGER_PREPARE);
      }
      delay(2);
    }
  }
}

#if CLI_USE(HW_I2S)

#if HW_I2S_LCD > 0
static void drawBlock(int16_t bx, int16_t by, uint16_t color)
{
  int16_t x;
  int16_t y;
  int16_t bw;
  int16_t bh;
  int16_t top_space = 0;
  int16_t sw;
  int16_t sh;

  sw = lcdGetWidth();
  sh = lcdGetHeight()-top_space;

  bw = (sw / BLOCK_X_CNT);
  bh = (sh / BLOCK_Y_CNT);

  x = bx*bw;
  y = sh - bh*by - bh;

  lcdDrawFillRect(x, y+top_space, bw-2, bh-2, color);
}

bool lcdUpdate(i2s_cli_t *p_args)
{
  bool ret = false;

  if (millis()-p_args->pre_time_lcd >= 50 && lcdDrawAvailable() == true)
  {
    p_args->pre_time_lcd = millis();

    lcdClearBuffer(black);

    arm_cfft_q15(&arm_cfft_sR_q15_len512, p_args->buf_q15, 0, 1);

    int16_t xi;

    xi = 0;
    for (int i=0; i<BLOCK_X_CNT; i++)
    {
      int32_t h;
      int32_t max_h;


      max_h = 0;
      for (int j=0; j<FFT_LEN/2/BLOCK_X_CNT; j++)
      {
        h = p_args->buf_q15[2*xi + 1];
        h = constrain(h, 0, 500);
        h = cmap(h, 0, 500, 0, 80);
        if (h > max_h)
        {
          max_h = h;
        }
        xi++;
      }
      h = cmap(max_h, 0, 80, 0, BLOCK_Y_CNT-1);

      p_args->block_target[i] = h;

      if (p_args->update_cnt%2 == 0)
      {
        if (p_args->block_peak[i] > 0)
        {
          p_args->block_peak[i]--;
        }
      }
      if (h >= p_args->block_peak[i])
      {
        p_args->block_peak[i] = h;
        p_args->block_value[i] = h;
      }
    }

    p_args->update_cnt++;

    for (int i=0; i<BLOCK_X_CNT; i++)
    {
      drawBlock(i, p_args->block_peak[i], red);

      if (p_args->block_value[i] > p_args->block_target[i])
      {
        p_args->block_value[i]--;
      }
      for (int j=0; j<p_args->block_value[i]; j++)
      {
        drawBlock(i, j, yellow);
      }
    }

    lcdRequestDraw();
    ret = true;
  }

  return ret;
}
#endif

void cliI2s(cli_args_t *args)
{
  bool ret = false;

  #if HW_I2S_LCD > 0
  // uint32_t q15_buf_index = 0;
  i2s_cli_t i2s_args;

  memset(i2s_args.block_peak, 0, sizeof(i2s_args.block_peak));
  memset(i2s_args.block_value, 0, sizeof(i2s_args.block_value));
  memset(i2s_args.block_target, 0, sizeof(i2s_args.block_target));
  #endif


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {

    cliPrintf("i2s init      : %d\n", is_init);
    cliPrintf("i2s rate      : %d Khz\n", i2s_sample_rate/1000);
    cliPrintf("i2s buf ms    : %d ms\n", I2S_BUF_MS);
    cliPrintf("i2s frame len : %d \n", i2s_frame_len);
    cliPrintf("i2s volume    : %d \n", i2sGetVolume());
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "beep") == true)
  {
    uint32_t freq;
    uint32_t time_ms;

    freq = args->getData(1);
    time_ms = args->getData(2);
    
    #if HW_I2S_LCD > 0
    i2sPlayBeepLcd(&i2s_args, freq, 100, time_ms);
    #else
    i2sPlayBeep(freq, 100, time_ms);
    #endif

    ret = true;
  }

  if (args->argc >= 1 && args->isStr(0, "volume") == true)
  {
    uint8_t volume;


    if (args->argc == 1)
    {
      cliPrintf("Volume : %d%%\n", i2sGetVolume());
    }
    else
    {
      volume = args->getData(1);
      i2sSetVolume(volume);
      i2sCfgSave();
      cliPrintf("i2s volume : %d \n", i2sGetVolume());
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "melody"))
  {
    uint16_t melody[] = {NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};
    int note_durations[] = { 4, 8, 8, 4, 4, 4, 4, 4 };

    for (int i=0; i<8; i++) 
    {
      int note_duration = 1000 / note_durations[i];

      i2sPlayBeep(melody[i], 100, note_duration);
      delay(note_duration * 0.3);    
    }
    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("i2s info\n");
    cliPrintf("i2s melody\n");
    cliPrintf("i2s volume 0~100\n");
    cliPrintf("i2s beep freq time_ms\n");
  }
}
#endif

#endif