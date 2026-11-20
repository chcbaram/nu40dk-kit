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


bool bldPadIsConnected(void);
bool bldPadGetInfo(xbox_hid_report_t *p_report);

#endif