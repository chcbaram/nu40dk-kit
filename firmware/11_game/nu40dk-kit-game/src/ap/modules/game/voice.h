#ifndef VOICE_H_
#define VOICE_H_

#include "ap_def.h"

// SD 의 WAV (PCM 16bit 모노 16kHz) 를 믹서로 재생. 시스템 workqueue 가 20ms 마다 공급하므로
// 화면 갱신(BUSY 대기) 중에도 끊기지 않는다. 버퍼는 에뮬레이터가 멈춘 동안의 scratch 를 빌린다.
bool voicePlay(const char *path);
void voiceStop(void);
bool voiceIsPlaying(void);

#endif
