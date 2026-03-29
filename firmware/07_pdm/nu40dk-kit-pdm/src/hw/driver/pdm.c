#include "pdm.h"



#ifdef _USE_HW_PDM
#include "cli.h"
#include "i2s.h"
#include "qbuffer.h"
#include <zephyr/audio/dmic.h>
#include <nrfx_pdm.h>
#ifdef _USE_HW_LCD
#include "lcd.h"
#include "button.h"
#include "files.h"
#endif





#ifdef _USE_HW_RTOS
#define lock()      k_mutex_lock(&mutex_lock, K_FOREVER);
#define unLock()    k_mutex_unlock(&mutex_lock);
static K_MUTEX_DEFINE(mutex_lock);
#else
#define lock()      
#define unLock()    
#endif


#define PDM_SAMPLERATE_HZ       16000
#define PDM_BIT_WIDTH           16
#define PDM_MIC_CH              1
#define PDM_BUF_MS              (4)
#define PDM_BUF_CNT             32
#define PDM_BUF_FRAME_LEN       ((PDM_SAMPLERATE_HZ * 2 * PDM_BUF_MS) / 1000)  // 16Khz, 2 mics, 4ms
#define PDM_CTL_IDX             0

#define PCM_GET_MS_TO_LEN(x)    ((PDM_SAMPLERATE_HZ * (x)) / 1000)  




#ifdef _USE_HW_CLI
static void cliCmd(cli_args_t *args);
#endif
static void pdmThread(void *arg1, void *arg2, void *arg3);
static bool pdmInitHw(void);


static bool is_init = false;


K_MEM_SLAB_DEFINE_STATIC(mem_slab, PDM_BUF_FRAME_LEN * sizeof(pcm_data_t), PDM_BUF_CNT, 4);

static bool is_started = false;

static qbuffer_t pcm_msg_q;
static pcm_data_t pcm_msg_buf[PDM_BUF_FRAME_LEN * PDM_BUF_CNT];


K_THREAD_STACK_DEFINE(pdm_stack_area, _HW_DEF_RTOS_THREAD_MEM_PDM);
static struct k_thread pdm_thread_data;

const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));






bool pdmInit(void)
{
  bool ret = false;



  qbufferCreateBySize(&pcm_msg_q, (uint8_t *)pcm_msg_buf, sizeof(pcm_data_t), PDM_BUF_FRAME_LEN * PDM_BUF_CNT);


  logPrintf("[  ] pdmInit()\n");
  logPrintf("     PDM_SAMPLERATE_HZ : %d Hz\n", PDM_SAMPLERATE_HZ);
  logPrintf("     PDM_BUF_MS        : %d ms\n", PDM_BUF_MS);
  logPrintf("     PDM_BUF_FRAME_LEN : %d \n",   PDM_BUF_FRAME_LEN);

  ret = pdmInitHw();

  k_thread_create(&pdm_thread_data, pdm_stack_area,
                  K_THREAD_STACK_SIZEOF(pdm_stack_area),
                  pdmThread,
                  NULL, NULL, NULL,
                  _HW_DEF_RTOS_THREAD_PRI_PDM, 0, K_NO_WAIT);  

  is_init = ret;

#ifdef _USE_HW_CLI
  cliAdd("pdm", cliCmd);
#endif
  return true;
}

bool pdmInitHw(void)
{
  bool ret = true;

  if (!device_is_ready(dmic_dev))
  {
    logPrintf("[E_] %s is not ready", dmic_dev->name);    
    return false;
  }

  struct pcm_stream_cfg stream = 
  {
    .pcm_width = 16,
    .mem_slab  = &mem_slab,
  };

  struct dmic_cfg cfg =
  {
    .io =
    {
      /* These fields can be used to limit the PDM clock
      * configurations that the driver is allowed to use
      * to those supported by the microphone.
      */
      .min_pdm_clk_freq = 1000000,
      .max_pdm_clk_freq = 3500000,
      .min_pdm_clk_dc   = 40,
      .max_pdm_clk_dc   = 60,
    },
    .streams = &stream,
    .channel = 
    {
      .req_num_streams = 1,
    },
  };

  cfg.channel.req_num_chan = PDM_MIC_CH;
  if (PDM_MIC_CH == 1)
  {
    cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, PDM_CTL_IDX, PDM_CHAN_LEFT);
  }
  else
  {
    cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, PDM_CTL_IDX, PDM_CHAN_LEFT) |
                                  dmic_build_channel_map(1, PDM_CTL_IDX, PDM_CHAN_RIGHT);
  }
  cfg.streams[0].pcm_rate   = PDM_SAMPLERATE_HZ;
  cfg.streams[0].block_size = PDM_BUF_FRAME_LEN * sizeof(pcm_data_t);

  
  int pdm_ret;

  pdm_ret = dmic_configure(dmic_dev, &cfg);
	if (pdm_ret < 0) 
  {
    ret = false;
    logPrintf("[E_] dmic_configure()\n");
	}

  // GAIN
  //
  nrf_pdm_gain_set(NRF_PDM, NRF_PDM_GAIN_MAXIMUM, NRF_PDM_GAIN_MAXIMUM);

  return ret;
}

bool pdmIsBusy(void)
{
  return is_started;
}

uint32_t pdmAvailable(void)
{
  return qbufferAvailable(&pcm_msg_q);
}

uint32_t pdmGetSampleRate(void)
{
  return PDM_SAMPLERATE_HZ;
}

bool pdmRead(pcm_data_t *p_buf, uint32_t length)
{
  bool ret;

  ret = qbufferRead(&pcm_msg_q, (uint8_t *)p_buf, length);

  return ret;
}

uint32_t pdmGetTimeToLengh(uint32_t ms)
{
  return PCM_GET_MS_TO_LEN(ms);
}

void pdmThread(void *arg1, void *arg2, void *arg3)
{
  int pdm_ret;

  logPrintf("[  ] pdmThread()\n");

  while(1)
  {
    if (is_started)
    {
      lock();
      void *buffer;
      uint32_t size;

      pdm_ret = dmic_read(dmic_dev, 0, &buffer, &size, 10);
      if (pdm_ret == 0)
      {
        uint16_t pcm_cnt;
        pcm_data_t pcm_data;
        int16_t *p_buf = buffer;

        pcm_cnt = size / (PDM_BIT_WIDTH/8);        
        for (int i=0; i<pcm_cnt; i++)
        {
          if (PDM_MIC_CH == 1)
          {
            pcm_data.L = p_buf[i];
            pcm_data.R = p_buf[i];
          }
          else
          {
            pcm_data.L = p_buf[i*2 + 0];
            pcm_data.R = p_buf[i*2 + 1];
          }

          qbufferWrite(&pcm_msg_q, (uint8_t *)&pcm_data, 1);
        }
        // logPrintf("size %d\n", size);
        k_mem_slab_free(&mem_slab, buffer);
      }
      unLock();
    }
    delay(1);
  }
}

bool pdmBegin(void)
{
  bool ret = false;
  int pdm_ret;


  lock();
  pdm_ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
  if (pdm_ret == 0)
  {
    is_started = true;
  }
  else
  {
    is_started = false;
    logPrintf("[E_] dmic_trigger(DMIC_TRIGGER_START)\n");    
  }
  qbufferFlush(&pcm_msg_q);
  unLock();  
  ret = is_started;

  return ret;
}

bool pdmEnd(void)
{
  if (!is_started)
    return true;

  lock();
  is_started = false;
  dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
  delay(10);
  unLock();
  
  return true;
}

#ifdef _USE_HW_CLI
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    cliPrintf("is_init       : %s\n", is_init ? "True":"False");
    
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "read"))
  {
    pdmBegin();
    while(cliKeepLoop())
    {
      uint32_t pdm_len;

      pdm_len = pdmAvailable();

      if (pdm_len > 0)
      {
        pdmRead(NULL, pdm_len);
        cliPrintf("len %d \n", pdm_len);
      }

      delay(2);
    }
    pdmEnd();
    ret = true;
  }

  #ifdef _USE_HW_LCD
  if (args->argc >= 1 && args->isStr(0, "file"))
  {
    typedef struct wavfile_header_s
    {
        char    ChunkID[4];     /* "RIFF" */
        int32_t ChunkSize;      /* 4 + (8 + Subchunk1Size) + (8 + Subchunk2Size) */
        char    Format[4];      /* "WAVE" */

        char    Subchunk1ID[4]; /* "fmt " */
        int32_t Subchunk1Size;  /* 16 for PCM */
        int16_t AudioFormat;    /* PCM = 1 */
        int16_t NumChannels;    /* 1 = Mono, 2 = Stereo */
        int32_t SampleRate;     /* 16000 */
        int32_t ByteRate;       /* SampleRate * NumChannels * BitsPerSample/8 */
        int16_t BlockAlign;     /* NumChannels * BitsPerSample/8 */
        int16_t BitsPerSample;  /* 16 */

        char    Subchunk2ID[4]; /* "data" */
        int32_t Subchunk2Size;  /* NumSamples * NumChannels * BitsPerSample/8 */
    } wavfile_header_t;

    uint32_t point_i;
    uint32_t pdm_len;
    pcm_data_t pcm_buf_line[LCD_WIDTH] = {0, };
    uint32_t pcm_buf_i = 0;
    uint32_t pcm_buf_len = LCD_WIDTH;

    uint32_t rec_idx = 0;
    uint32_t rec_len = 0;

    char* filename = "pdm.wav";
    FILE* fp = NULL;
    pcm_data_t rec_buf[256];
  
    wavfile_header_t header;

    // 설정값 정의
    int32_t sample_rate     = 16000;
    int16_t num_channels    = 2;
    int16_t bits_per_sample = 16;

    // RIFF Chunk
    memcpy(header.ChunkID, "RIFF", 4);
    header.Format[0] = 'W';
    header.Format[1] = 'A';
    header.Format[2] = 'V';
    header.Format[3] = 'E';

    // fmt Subchunk
    memcpy(header.Subchunk1ID, "fmt ", 4);
    header.Subchunk1Size = 16;
    header.AudioFormat   = 1; // PCM
    header.NumChannels   = num_channels;
    header.SampleRate    = sample_rate;
    header.BitsPerSample = bits_per_sample;
    header.ByteRate      = sample_rate * num_channels * (bits_per_sample / 8);
    header.BlockAlign    = num_channels * (bits_per_sample / 8);

    // data Subchunk
    memcpy(header.Subchunk2ID, "data", 4);
    header.Subchunk2Size = 0; // 나중에 데이터 쓰기가 끝난 후 업데이트
    header.ChunkSize     = 36 + header.Subchunk2Size;


    if (args->argc == 2)
    {
      filename = args->getStr(1);
    }

    fp = fopen(filename, "wb");
    if (fp)
    {
      fwrite(&header, sizeof(wavfile_header_t), 1, fp);                  
    }
    else
    {
      cliPrintf("[E_] fopen()\n");                  
    }


    cliPrintf("rec : %s\n", filename);
    delay(200);

    pdmBegin();
    point_i = 0;
    while(cliKeepLoop())
    {
      if (lcdDrawAvailable() == true)
      {
        lcdClearBuffer(black);

        pdm_len = pdmAvailable();
        // logPrintf("%d \n", pdm_len);
        if (pdm_len > 0)
        {
          pcm_data_t pcm_data;
          int16_t l_y;
          int16_t r_y;

          for (int i=0; i<pdm_len; i++)
          {
            pdmRead(&pcm_data, 1);  
            
            point_i++;

            if (point_i%50 == 0)
            {
              pcm_buf_line[pcm_buf_i] = pcm_data;
              pcm_buf_i = (pcm_buf_i + 1) % pcm_buf_len;              
            }

      
            rec_buf[rec_idx] = pcm_data;
            rec_idx++;

            if (fp && rec_idx >= 256)
            {
              fwrite(&rec_buf[0], sizeof(pcm_data_t), rec_idx, fp);
              rec_len += sizeof(pcm_data_t) * rec_idx;
              rec_idx = 0;
            }
          }

          uint32_t index;
          int16_t pre_x[2] = {0};
          int16_t pre_y[2] = {0};
          int16_t x[2];
          int16_t y[2];

          for (int i=0; i<LCD_WIDTH-1; i++)
          {
            index = (pcm_buf_i + i) % pcm_buf_len;
            l_y = cmap(pcm_buf_line[index].L, -32768, 32767, -15, 15);
            r_y = cmap(pcm_buf_line[index].R, -32768, 32767, -15, 15);
            
            x[0] = i;
            y[0] = LCD_HEIGHT - 15 - l_y;

            if (i > 0)
            {
              lcdDrawLine(pre_x[0], pre_y[0], x[0], y[0], green);
            }
            pre_x[0] = x[0];
            pre_y[0] = y[0];
          }             
          lcdRequestDraw();     
        }
      }
      delay(2);
    }
    pdmEnd();

    lcdClear(black);

    if (fp != NULL)
    {
      // 헤더 정보 업데이트 (파일 포인터를 처음으로 돌려서 사이즈 기록)
      header.Subchunk2Size = rec_len;
      header.ChunkSize     = 36 + rec_len;

      fseek(fp, 0, SEEK_SET);
      fwrite(&header, sizeof(wavfile_header_t), 1, fp);

      fclose(fp);
    }
    
    ret = true;
  }
  #endif

  if (ret == false)
  {
    cliPrintf("pdm info\n");
    cliPrintf("pdm read\n");
    #ifdef _USE_HW_LCD
    cliPrintf("pdm file [file_name]\n");
    #endif
  }
}
#endif

#endif