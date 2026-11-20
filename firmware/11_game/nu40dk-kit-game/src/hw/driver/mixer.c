/*
 * mixer.c
 *
 *  Created on: 2020. 8. 8.
 *      Author: Baram
 */




#include "mixer.h"
#include <string.h>



#ifdef _USE_HW_MIXER



static uint32_t mixerBufAvailable(mixer_t *p_mixer, uint8_t ch);
static int16_t mixerBufRead(mixer_t *p_mixer, uint8_t ch);




bool mixerInit(mixer_t *p_mixer)
{
  uint32_t i;


  for (i=0; i<MIXER_MAX_CH; i++)
  {
    p_mixer->buf[i].in = 0;
    p_mixer->buf[i].out = 0;
    p_mixer->buf[i].length = MIXER_MAX_BUF_LEN;
  }
  p_mixer->volume = 100;

  return true;
}

bool mixerSetVolume(mixer_t *p_mixer, int32_t volume)
{
  p_mixer->volume = constrain(volume, 0, 100);
  return true;
}

int32_t mixerGetVolume(mixer_t *p_mixer)
{
  return p_mixer->volume;
}

// 링 버퍼에 쓰기. 빈 공간만큼 최대 두 번의 memcpy 로 복사한다 (샘플 단위 루프 대신).
// 공간이 모자라면 들어가는 만큼만 쓰고 false (원래 동작과 같음)
bool mixerWrite(mixer_t *p_mixer, uint8_t ch, int16_t *p_data, uint32_t length)
{
  mixer_buf_t *p_buf;
  uint32_t     in, out, len, space, n, first;

  if (ch >= MIXER_MAX_CH) return false;

  p_buf = &p_mixer->buf[ch];
  in    = p_buf->in;
  out   = p_buf->out;
  len   = p_buf->length;
  space = (out + len - in - 1) % len;
  n     = length < space ? length : space;

  first = len - in;
  if (first > n) first = n;
  memcpy(&p_buf->buf[in], p_data, first * sizeof(int16_t));
  if (n > first)
  {
    memcpy(&p_buf->buf[0], &p_data[first], (n - first) * sizeof(int16_t));
  }
  __asm volatile("" ::: "memory");            // 데이터를 쓴 뒤에 in 갱신 (I2S 스레드가 읽음)
  p_buf->in = (in + n) % len;

  return n == length;
}


//  http://atastypixel.com/blog/how-to-mix-audio-samples-properly-on-ios/
//
int16_t mixerSamples(int16_t a, int16_t b)
{
 return
    // If both samples are negative, mixed signal must have an amplitude between the lesser of A and B, and the minimum permissible negative amplitude
    a < 0 && b < 0 ?
        ((int)a + (int)b) - (((int)a * (int)b)/INT16_MIN) :

    // If both samples are positive, mixed signal must have an amplitude between the greater of A and B, and the maximum permissible positive amplitude
    ( a > 0 && b > 0 ?
        ((int)a + (int)b) - (((int)a * (int)b)/INT16_MAX)

    // If samples are on opposite sides of the 0-crossing, mixed signal should reflect that samples cancel each other out somewhat
    :
        a + b);
}

// 비어 있는 채널은 섞어도 값이 그대로(mixerSamples(a, 0) = a)라 건너뛴다.
// 시작할 때 채널별 남은 샘플 수를 한 번 구해 둔다.
bool mixerRead(mixer_t *p_mixer, int16_t *p_data, uint32_t length)
{
  uint32_t i;
  uint32_t ch;
  int16_t  mixer_out;
  uint32_t avail[MIXER_MAX_CH];

  for (ch=0; ch<MIXER_MAX_CH; ch++)
  {
    avail[ch] = mixerBufAvailable(p_mixer, ch);
  }

  for (i=0; i<length; i++)
  {
    mixer_out = 0;
    if (avail[0] > 0)
    {
      avail[0]--;
      mixer_out = mixerBufRead(p_mixer, 0);
    }
    for (ch=1; ch<MIXER_MAX_CH; ch++)
    {
      if (avail[ch] > 0)
      {
        avail[ch]--;
        mixer_out = mixerSamples(mixer_out, mixerBufRead(p_mixer, ch));
      }
    }

    p_data[i] = mixer_out * p_mixer->volume / 100;
  }

  return true;
}

uint32_t mixerBufAvailable(mixer_t *p_mixer, uint8_t ch)
{
  uint32_t ret = 0;

  if (ch >= MIXER_MAX_CH) return 0;

  ret = (p_mixer->buf[ch].length + p_mixer->buf[ch].in - p_mixer->buf[ch].out) % p_mixer->buf[ch].length;

  return ret;
}

int16_t mixerBufRead(mixer_t *p_mixer, uint8_t ch)
{
  int16_t ret = 0;
  uint32_t index;
  uint32_t next_index;

  if (ch >= MIXER_MAX_CH) return 0;


  index      = p_mixer->buf[ch].out;
  next_index = p_mixer->buf[ch].out + 1;

  if (next_index == p_mixer->buf[ch].length)
  {
    next_index = 0;
  }

  if (index != p_mixer->buf[ch].in)
  {
    ret = p_mixer->buf[ch].buf[index];
    p_mixer->buf[ch].out = next_index;
  }

  return ret;
}

uint32_t mixerAvailable(mixer_t *p_mixer)
{
  uint32_t ret = 0;
  uint32_t i;
  uint32_t buf_len;

  for (i=0; i<MIXER_MAX_CH; i++)
  {
    buf_len = mixerBufAvailable(p_mixer, i);
    if (buf_len > ret)
    {
      ret = buf_len;
    }
  }


  return ret;
}

uint32_t mixerAvailableForWrite(mixer_t *p_mixer, uint8_t ch)
{
  uint32_t rx_len;
  uint32_t wr_len;

  if (ch >= MIXER_MAX_CH) return 0;

  rx_len = mixerBufAvailable(p_mixer, ch);
  wr_len = (p_mixer->buf[ch].length - 1) - rx_len;

  return wr_len;
}

bool mixerIsEmpty(mixer_t *p_mixer, uint8_t ch)
{
  if (mixerBufAvailable(p_mixer, ch) > 0) 
    return false;
  else                                    
    return true;
}

int8_t mixerGetEmptyChannel(mixer_t *p_mixer)
{
  int8_t ret = -1;
  uint32_t i;

  for (i=0; i<MIXER_MAX_CH; i++)
  {
    if (mixerBufAvailable(p_mixer, i) == 0)
    {
      ret = i;
      break;
    }
  }

  return ret;
}

int8_t mixerGetValidChannel(mixer_t *p_mixer, uint32_t length)
{
  int8_t ret = -1;
  uint32_t i;

  for (i=0; i<MIXER_MAX_CH; i++)
  {
    if (mixerAvailableForWrite(p_mixer, i) >= length)
    {
      ret = i;
    }
  }

  return ret;
}


#endif