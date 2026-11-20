#ifndef GB_RUN_H_
#define GB_RUN_H_

#include "ap_def.h"


typedef struct
{
  uint32_t emu_frames;      // 실행한 에뮬 프레임 수
  uint32_t emu_us_sum;      // 에뮬 프레임 실행 시간 합
  uint32_t disp_frames;     // EPD 에 표시한 프레임 수
  uint32_t disp_ms_sum;     // EPD 갱신 시간 합
} gb_run_stat_t;


bool gbRunStart(void);      // Flash 의 ROM 으로 실행 시작
void gbRunStop(void);
bool gbRunIsRunning(void);
void gbRunSetPause(bool pause);
bool gbRunIsPaused(void);
bool gbRunGetMenuRequest(void);   // 메뉴 버튼(Xbox / BTN4 길게) 눌림 여부, 읽으면 지워짐
void gbRunRequestFull(void);
void gbRunGetStat(gb_run_stat_t *p_stat);

#endif
