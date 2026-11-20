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
  uint32_t ink_scans;       // ink 모드 스캔 수
  uint32_t ink_scene;       // 장면 전환으로 한 전체 갱신 수
  uint32_t ink_quiet;       // 조용한 순간 정리 전체 갱신 수
  uint32_t ink_pending;     // ink 모드 스캔당 구동 GB 픽셀 합
  uint32_t ink_plan_us;     // 변화 감지 시간 합
  uint32_t ink_mask_us;     // 마스크 생성 시간 합
  uint32_t ink_dec_us;      // 카운터 감소 시간 합 (마스크 증분 갱신 포함)
  uint32_t ink_rows;        // 전송한 줄 수 합
} gb_run_stat_t;


bool gbRunStart(void);      // Flash 의 ROM 으로 실행 시작 (cart RAM 초기화)
bool gbRunStartEx(bool clear_ram);
void gbRunStop(void);
bool gbRunIsRunning(void);
void gbRunSetPause(bool pause);
void gbRunWaitDisplayIdle(void);
void gbRunDrawOverlay(uint16_t y, uint16_t h);   // 게임 위 상자(메뉴)만 구동 (ink 모드)
void gbRunGetScreenRect(int16_t *p_x, int16_t *p_y, int16_t *p_w, int16_t *p_h);
bool gbRunIsPaused(void);
void     gbRunSetEdge(uint8_t n);      // 테두리였던 머물던 픽셀이 흰색으로 갈 때 추가 펄스
uint8_t  gbRunGetEdge(void);
void     gbRunSetThin(uint8_t n);      // 1px 선이었던 픽셀 추가 펄스
uint8_t  gbRunGetThin(void);
void     gbRunSetSettle(uint8_t l, uint8_t d, uint8_t k);   // 머물던 픽셀이 흰색으로 갈 때 추가 펄스
void     gbRunGetSettle(uint8_t *p_l, uint8_t *p_d, uint8_t *p_k);
void     gbRunSetRetarget(uint16_t rev_pct, bool same_add);  // 회색 모드 목표 변경 보정 (0 = 끔)
void     gbRunGetRetarget(uint16_t *p_rev_pct, bool *p_same_add);
bool     gbRunStartPattern(uint16_t speed, uint16_t frames, uint16_t pause, uint16_t reps, uint8_t big);  // 잔상 측정용 움직이는 테스트 패턴
bool     gbRunIsPattern(void);
void     gbRunSetExtBusy(bool busy);   // CLI 측정 중 표시 (런처가 화면을 건드리지 않음)
bool     gbRunIsExtBusy(void);
uint8_t *gbRunGetScratch(uint32_t *p_size);  // 게임 정지 중 ink 마스크 버퍼 빌리기 (실행 중 NULL)
void     gbRunSetQuiet(uint32_t ms, uint32_t px, uint32_t act);   // 조용한 순간 정리 (ms = 0 끔)
void     gbRunGetQuiet(uint32_t *p_ms, uint32_t *p_px, uint32_t *p_act);
void     gbRunSetScenePx(uint32_t px);   // 장면 전환 전체 갱신 기준 (바뀐 GB 픽셀 수, 0 = 끔)
uint32_t gbRunGetScenePx(void);
bool gbRunGetMenuRequest(void);   // 메뉴 버튼(Xbox / BTN4 길게) 눌림 여부, 읽으면 지워짐
void gbRunSetSound(bool enable);
void gbRunSetWinFull(bool full);
void gbRunInjectKey(uint8_t keys, uint32_t ms);
void    gbRunSetGray(bool enable);             // ink 회색 모드 (디더 없이 단계별 펄스 수)
bool    gbRunGetGray(void);
void    gbRunSetGrayDose(uint8_t from, uint8_t to, uint8_t dose);   // shade 0 흰 ~ 3 검정
uint8_t gbRunGetGrayDose(uint8_t from, uint8_t to);
void    gbRunSetGrayFrames(uint8_t frames);
uint8_t gbRunGetGrayFrames(void);
void    gbRunSetDispMode(uint8_t mode);    // 0: frame (FAST 전체), 1: ink (픽셀별 펄스 카운터)
uint8_t gbRunGetDispMode(void);
void    gbRunSetInk(uint8_t budget, uint8_t frames, uint32_t frame_ms);
void    gbRunGetInk(uint8_t *p_budget, uint8_t *p_frames, uint32_t *p_frame_ms);   // CLI 테스트용 입력
bool gbRunGetSound(void);
void gbRunRequestFull(void);
void gbRunGetStat(gb_run_stat_t *p_stat);
void gbRunGetThreadCycles(uint64_t *p_emu, uint64_t *p_disp);

#endif
