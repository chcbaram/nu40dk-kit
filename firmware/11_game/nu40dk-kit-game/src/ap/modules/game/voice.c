#include "voice.h"
#include "gb_run.h"
#include <ff.h>
#include <string.h>
#include <zephyr/kernel.h>


#define VOICE_CH        0           // 믹서 채널 (게임 밖에서는 에뮬레이터 채널이 비어 있음)
#define VOICE_RATE      16000
#define VOICE_CHUNK     128         // 한 번에 읽는 모노 샘플 수
#define VOICE_PERIOD_MS 20
#define VOICE_SCRATCH_OFS 12800     // 앞쪽은 앱(rpg)이 쓴다 (app_api scratch)

static struct k_work_delayable voice_work;
static bool             voice_init_done = false;
static volatile bool    playing = false;
static bool             file_open = false;
static uint32_t         remain;     // 남은 데이터 바이트
static FIL             *p_fil;
static int16_t         *p_mono;
static int16_t         *p_st;


static void voiceClose(void)
{
  if (file_open)
  {
    f_close(p_fil);
    file_open = false;
  }
  playing = false;
}

static void voiceWork(struct k_work *work)
{
  (void)work;

  if (!playing)
  {
    voiceClose();
    return;
  }

  while (remain > 0 && i2sAvailableForWrite(VOICE_CH) >= VOICE_CHUNK * 2)
  {
    UINT     br = 0;
    uint32_t n  = remain < VOICE_CHUNK * 2 ? remain : VOICE_CHUNK * 2;

    if (f_read(p_fil, p_mono, n, &br) != FR_OK || br == 0)
    {
      remain = 0;
      break;
    }
    uint32_t cnt = br / 2;
    for (uint32_t i = 0; i < cnt; i++)
    {
      p_st[i * 2 + 0] = p_mono[i];
      p_st[i * 2 + 1] = p_mono[i];
    }
    i2sWrite(VOICE_CH, p_st, cnt * 2);
    remain -= br;
  }

  if (remain == 0)
  {
    voiceClose();
    return;
  }
  k_work_schedule(&voice_work, K_MSEC(VOICE_PERIOD_MS));
}

void voiceStop(void)
{
  struct k_work_sync sync;

  if (!voice_init_done) return;
  playing = false;
  k_work_cancel_delayable_sync(&voice_work, &sync);
  voiceClose();
}

bool voiceIsPlaying(void)
{
  return playing;
}

bool voicePlay(const char *path)
{
  uint32_t size;
  uint8_t *p_scratch;
  uint8_t  hdr[12];
  UINT     br;

  if (!voice_init_done)
  {
    k_work_init_delayable(&voice_work, voiceWork);
    voice_init_done = true;
  }
  voiceStop();

  p_scratch = gbRunGetScratch(&size);
  if (p_scratch == NULL) return false;

  // scratch 뒤쪽: FIL (8 바이트 정렬) + 모노 + 스테레오 버퍼
  uintptr_t a = ((uintptr_t)p_scratch + VOICE_SCRATCH_OFS + 7) & ~(uintptr_t)7;
  uint32_t  need = sizeof(FIL) + 8 + VOICE_CHUNK * 2 + VOICE_CHUNK * 4;
  if (a + need > (uintptr_t)p_scratch + size) return false;
  p_fil  = (FIL *)a;
  a      = (a + sizeof(FIL) + 7) & ~(uintptr_t)7;
  p_mono = (int16_t *)a;
  p_st   = (int16_t *)(a + VOICE_CHUNK * 2);

  if (f_open(p_fil, path, FA_READ) != FR_OK) return false;
  file_open = true;

  // RIFF/WAVE 헤더 → "data" 청크 찾기
  if (f_read(p_fil, hdr, 12, &br) != FR_OK || br != 12 ||
      memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0)
  {
    voiceClose();
    return false;
  }
  while (1)
  {
    uint8_t  ck[8];
    if (f_read(p_fil, ck, 8, &br) != FR_OK || br != 8) { voiceClose(); return false; }
    uint32_t len = ck[4] | (ck[5] << 8) | (ck[6] << 16) | ((uint32_t)ck[7] << 24);
    if (memcmp(ck, "fmt ", 4) == 0)
    {
      uint8_t fmt[16];
      if (len < 16 || f_read(p_fil, fmt, 16, &br) != FR_OK) { voiceClose(); return false; }
      uint16_t ch   = fmt[2] | (fmt[3] << 8);
      uint32_t rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
      uint16_t bits = fmt[14] | (fmt[15] << 8);
      if (ch != 1 || rate != VOICE_RATE || bits != 16) { voiceClose(); return false; }
      if (len > 16) f_lseek(p_fil, f_tell(p_fil) + (len - 16 + (len & 1)));
    }
    else if (memcmp(ck, "data", 4) == 0)
    {
      remain = len & ~1u;
      break;
    }
    else
    {
      f_lseek(p_fil, f_tell(p_fil) + len + (len & 1));
    }
  }

  playing = true;
  k_work_schedule(&voice_work, K_NO_WAIT);
  return true;
}
