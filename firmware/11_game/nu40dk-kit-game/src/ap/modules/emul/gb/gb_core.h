#ifndef GB_CORE_H_
#define GB_CORE_H_

/*
 * 에뮬레이터 코어 공통 인터페이스
 *   각 코어 폴더(core_walnut, core_peanut, core_gnuboy)가 이 함수들을 구현한다.
 *   gb_config.h 의 GB_CORE 로 선택된 코어만 빌드된다.
 */
#include <stdint.h>
#include <stdbool.h>
#include "gb_config.h"


// 조이패드 비트 (1 = 눌림)
#define GB_JOY_A        (1 << 0)
#define GB_JOY_B        (1 << 1)
#define GB_JOY_SELECT   (1 << 2)
#define GB_JOY_START    (1 << 3)
#define GB_JOY_RIGHT    (1 << 4)
#define GB_JOY_LEFT     (1 << 5)
#define GB_JOY_UP       (1 << 6)
#define GB_JOY_DOWN     (1 << 7)


// 한 줄(160 픽셀) 출력 콜백. shade = pixels[x] & 3 (0 흰색 ~ 3 검정)
typedef void (*gb_core_line_cb_t)(uint8_t line, const uint8_t *pixels);

typedef struct
{
  const uint8_t    *p_rom;            // ROM (Flash XIP)
  uint8_t          *p_cart_ram;
  uint32_t          cart_ram_size;
  gb_core_line_cb_t line_cb;
} gb_core_cfg_t;


const char *gbCoreName(void);
bool        gbCoreLoad(const gb_core_cfg_t *p_cfg);
void        gbCoreRunFrame(void);
void        gbCoreSetJoypad(uint8_t pressed);
void        gbCoreSetFrameSkip(bool enable);        // 2 프레임에 1 번만 렌더링
void        gbCoreSetOption(const char *p_opt);     // 코어별 실험 옵션 (예: walnut "single")
bool        gbCoreIsCgb(void);
bool        gbCoreIsDoubleSpeed(void);
uint32_t    gbCoreStateSize(void);                  // 코어 상태 구조체 크기 (RAM 확인용)

#endif
