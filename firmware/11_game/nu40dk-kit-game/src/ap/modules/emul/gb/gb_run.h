#ifndef GB_RUN_H_
#define GB_RUN_H_

#include "ap_def.h"


typedef struct
{
  uint32_t emu_frames;      // 실행한 에뮬 프레임 수
  uint32_t emu_us_sum;      // 에뮬 프레임 실행 시간 합
  uint32_t disp_frames;     // EPD 에 표시한 프레임 수
  uint32_t disp_ms_sum;     // EPD 갱신 시간 합
  uint32_t audio_us_sum;    // 사운드 샘플 생성 시간 합
  uint32_t audio_drop;      // 믹서가 가득 차 버린 프레임 수
} gb_run_stat_t;


bool gbRunStart(void);      // Flash 의 ROM 으로 실행 시작 (cart RAM 초기화)
bool gbRunStartEx(bool clear_ram);
void gbRunStop(void);
bool gbRunIsRunning(void);
void gbRunSetPause(bool pause);
void gbRunWaitDisplayIdle(void);
void gbRunGetScreenRect(int16_t *p_x, int16_t *p_y, int16_t *p_w, int16_t *p_h);
bool gbRunIsPaused(void);
bool gbRunGetMenuRequest(void);   // 메뉴 버튼(Xbox / BTN4 길게) 눌림 여부, 읽으면 지워짐
void gbRunSetSound(bool enable);
void gbRunSetWinFull(bool full);
void gbRunInjectKey(uint8_t keys, uint32_t ms);   // CLI 테스트용 입력
bool gbRunGetSound(void);
void gbRunRequestFull(void);
void gbRunGetStat(gb_run_stat_t *p_stat);
void gbRunGetThreadCycles(uint64_t *p_emu, uint64_t *p_disp);

#endif
