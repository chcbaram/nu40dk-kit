#ifndef BLE_PAD_H_
#define BLE_PAD_H_


#include "ap_def.h"


typedef struct __attribute__((packed))
{
  // 0~5번 바이트: 아날로그 스틱 (각 16비트 축이지만, BLE 특성상 10비트~16비트 범위로 패킹됨)
  uint16_t left_stick_x;  // 왼쪽 스틱 X축 (0 ~ 65535, 정중앙 약 32768)
  uint16_t left_stick_y;  // 왼쪽 스틱 Y축 (0 ~ 65535)
  uint16_t right_stick_x; // 오른쪽 스틱 X축 (0 ~ 65535)
  uint16_t right_stick_y; // 오른쪽 스틱 Y축 (0 ~ 65535)

  // 8~11번 바이트: 아날로그 트리거 (10비트 범위인 0 ~ 1023)
  uint16_t left_trigger;  // LT 트리거
  uint16_t right_trigger; // RT 트리거

  // 12번 바이트: D-Pad (방향키는 비트가 아니라 1~8 사이의 각도 수치(Hat switch)로 들어옵니다)
  // 1:북, 2:북동, 3:동, 4:남동, 5:남, 6:남서, 7:서, 8:북서, 0:누르지 않음
  uint8_t dpad     : 4;
  uint8_t dummy1   : 4; // 사용하지 않는 비트 정렬용

  // 13~14번 바이트: 주요 버튼들 (비트 단위 매핑)
  uint8_t btn_a    : 1;
  uint8_t btn_b    : 1;
  uint8_t dummy2   : 1; // 패딩 비트
  uint8_t btn_x    : 1;
  uint8_t btn_y    : 1;
  uint8_t dummy3   : 1;
  uint8_t btn_lb   : 1;
  uint8_t btn_rb   : 1;

  uint8_t dummy4   : 2;
  uint8_t btn_view : 1; // 공유/백 버튼 (창 두개 모양)
  uint8_t btn_menu : 1; // 메뉴/스타트 버튼 (석 삼자 모양)
  uint8_t btn_xbox : 1; // 가운데 가이드/홈 버튼
  uint8_t btn_ls   : 1; // 왼쪽 스틱 클릭
  uint8_t btn_rs   : 1; // 오른쪽 스틱 클릭
  uint8_t dummy5   : 1;
} xbox_hid_report_t;


#define BLE_PAD_LATCH_LAUNCHER  0
#define BLE_PAD_LATCH_GAME      1
#define BLE_PAD_LATCH_MAX       2

// 눌림 이벤트 (가상 키 비트). 방향은 D-pad 와 왼쪽 스틱을 합친 것
#define BLE_PAD_K_UP      0x0001
#define BLE_PAD_K_DOWN    0x0002
#define BLE_PAD_K_LEFT    0x0004
#define BLE_PAD_K_RIGHT   0x0008
#define BLE_PAD_K_A       0x0010
#define BLE_PAD_K_B       0x0020
#define BLE_PAD_K_X       0x0040
#define BLE_PAD_K_Y       0x0080
#define BLE_PAD_K_VIEW    0x0100
#define BLE_PAD_K_MENU    0x0200
#define BLE_PAD_K_XBOX    0x0400


typedef enum
{
  BLE_PAD_STATE_OFF = 0,      // BT 초기화 전
  BLE_PAD_STATE_IDLE,         // 스캔 안 함
  BLE_PAD_STATE_SCANNING,     // 패드 찾는 중
  BLE_PAD_STATE_CONNECTING,   // 연결/보안/GATT 탐색 중
  BLE_PAD_STATE_CONNECTED,    // 입력 수신 가능
} ble_pad_state_t;


bool            blePadIsConnected(void);
bool            blePadGetInfo(xbox_hid_report_t *p_report);
bool            blePadGetInfoLatched(uint8_t user, xbox_hid_report_t *p_report);
bool            blePadPopEvent(uint8_t user, uint16_t *p_keys, uint32_t *p_ms);  // 새로 눌린 키 (보고서 단위, 시간 포함)
void            blePadFlushEvents(uint8_t user);
uint32_t        blePadGetBounceCount(void);
void            blePadSetLog(bool on);        // 보고서가 바뀔 때마다 로그   // 떨림으로 무시한 눌림 수
ble_pad_state_t blePadGetState(void);
const char     *blePadGetStateStr(void);
void            blePadScanStart(void);
void            blePadScanStop(void);
bool            blePadIsScanEnabled(void);
void            blePadDisconnect(void);
void            blePadClearBonds(void);
bool            blePadHasBond(void);
int8_t          blePadGetBattery(void);   // 0~100 %, 모르면 -1
int8_t          blePadGetRssi(void);      // 연결 시점의 RSSI
const char     *blePadGetName(void);

#endif