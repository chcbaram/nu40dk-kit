#ifndef APP_API_H_
#define APP_API_H_

// 런처가 앱(네이티브 게임)에 넘겨주는 API.
//   앱은 펌웨어 함수를 직접 부르지 않고 이 구조체만 쓴다. 지금은 펌웨어에 같이 빌드하지만,
//   경계가 이것뿐이라 나중에 앱을 별도 바이너리(SD → flash)로 떼어 내기 쉽다.

#include <stdint.h>
#include <stdbool.h>

#define APP_API_VERSION   1

typedef enum
{
  APP_KEY_NONE = 0,
  APP_KEY_UP,
  APP_KEY_DOWN,
  APP_KEY_LEFT,
  APP_KEY_RIGHT,
  APP_KEY_OK,       // 패드 A / Menu, 보드 BTN3
  APP_KEY_BACK,     // 패드 B / Xbox, 보드 BTN4
} app_key_t;

typedef enum
{
  APP_REFRESH_FAST = 0,   // 바뀐 픽셀만 (약 0.4초), 메뉴·대화
  APP_REFRESH_QUICK,      // 바뀐 픽셀만, 짧은 파형 (약 0.2초), 걷기 애니메이션 (조금 흐릴 수 있음)
  APP_REFRESH_CLEAN,      // 흑백 전체 갱신 (약 1.3초, 깜빡임), 장면 전환·잔상 정리
  APP_REFRESH_STRONG,     // 3색 강한 갱신 (약 2.8초), 시작·종료
  APP_REFRESH_FLASH,      // 짧은 깜빡임 (검정 → 흰색 → 그림, 약 1.2초), 장면 전환·잔상 정리
} app_refresh_t;

typedef enum
{
  APP_WHITE = 0,
  APP_BLACK = 1,
} app_color_t;

typedef struct
{
  uint32_t version;
  uint16_t width;             // 400
  uint16_t height;            // 300
  uint16_t stride;            // 50 (바이트)
  uint8_t *fb;                // 1bpp 화면 버퍼: 비트 1 = 흰색, MSB 가 왼쪽 픽셀
  uint8_t *scratch;           // 앱이 마음대로 쓰는 RAM (에뮬레이터가 멈춘 동안 빌림), 없으면 NULL
  uint32_t scratch_size;

  // 입력
  app_key_t (*readKey)(void);         // 눌림 + 방향 자동 반복
  void      (*waitKeyRelease)(void);
  uint8_t   (*keyHeld)(void);           // 지금 눌린 키 비트 (1 << app_key_t)

  // 화면
  void    (*refresh)(app_refresh_t mode);
  void    (*refreshRect)(app_refresh_t mode, int16_t x, int16_t y, int16_t w, int16_t h);  // FAST/QUICK: 이 영역만 전송
  void    (*refreshFrom)(const uint8_t *p_old, int16_t y, int16_t h);   // FAST: 이전 이미지를 p_old 로 보고 y~y+h 줄 구동
                                                                         //   (같은 전환을 다시 구동해 잔상 정리, p_old 는 0 줄부터)
  void    (*powerOff)(void);          // 화면 아날로그 전원 끄기 (그림은 유지)
  void    (*clear)(app_color_t color);
  void    (*fillRect)(int16_t x, int16_t y, int16_t w, int16_t h, app_color_t color);
  void    (*drawRect)(int16_t x, int16_t y, int16_t w, int16_t h, app_color_t color);
  int16_t (*print)(int16_t x, int16_t y, app_color_t color, const char *fmt, ...);   // 8x16 영문, 16x16 한글(UTF-8)
  uint16_t (*strWidth)(const char *str);

  // 소리·시간·로그
  void     (*beep)(uint32_t hz, uint32_t ms);
  bool     (*voicePlay)(const char *path);   // SD 의 WAV (PCM 16bit 모노 16kHz), 화면 갱신 중에도 이어서 재생
  void     (*voiceStop)(void);
  bool     (*voiceIsPlaying)(void);
  uint32_t (*millis)(void);
  void     (*delay)(uint32_t ms);
  void     (*log)(const char *fmt, ...);
} app_api_t;

typedef struct
{
  const char *name;
  void      (*main)(const app_api_t *api);    // 끝나면 런처로 돌아온다
} app_def_t;

#endif
