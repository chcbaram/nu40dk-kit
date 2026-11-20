# 06. 입력: Xbox BLE 패드와 보드 버튼

![Xbox BLE 연결 순서](images/ble_sequence.svg)

## Xbox BLE 패드 (`src/ap/modules/remote/ble_pad.c`, 10_xbox에서 이전)

### 연결 흐름

1. `bt_enable` → `settings_load` (본딩 정보 복원)
2. 액티브 스캔 (interval = window = 60ms)
3. 광고 이름에 `"Xbox"`가 있거나 본딩된 주소이고, RSSI > -75이면 연결 대상으로 고른다.
4. `bt_conn_le_create` (연결 간격 15ms)
5. `bt_conn_set_security(L2)` → Just Works 페어링·본딩
6. GATT 탐색: HID Control Point(0x2A4C), Report Map(0x2A4B), CCCD 구독
7. 활성화: Report Map 읽기 → Control Point 쓰기 → 연결 완료
8. Notification(길이 14 이상)으로 입력 리포트 수신
9. 끊기면 다시 스캔

### 공개 API (Phase 5)

```c
bool            blePadIsConnected(void);
bool            blePadGetInfo(xbox_hid_report_t *p_report);   // mutex 로 복사
bool            blePadGetInfoLatched(uint8_t user, xbox_hid_report_t *p_report); // 현재 + 그동안 눌린 버튼
ble_pad_state_t blePadGetState(void);        // OFF / IDLE / SCANNING / CONNECTING / CONNECTED
const char     *blePadGetStateStr(void);
void            blePadScanStart(void);       // 스캔 허용 (끊기면 자동 재스캔)
void            blePadScanStop(void);        // 스캔 중지 (자동 재스캔 안 함)
bool            blePadIsScanEnabled(void);
void            blePadDisconnect(void);
void            blePadClearBonds(void);      // 모든 페어링 삭제
bool            blePadHasBond(void);
int8_t          blePadGetBattery(void);      // 0~100 %, 모르면 -1
int8_t          blePadGetRssi(void);
const char     *blePadGetName(void);
```

이름의 오타(`bldPad*`)를 `blePad*` 로 고쳤다.

### 배터리

- GATT 특성 탐색 때 Battery Level(0x2A19) 핸들을 찾는다. Xbox 패드는 핸들 20 이었다(10_xbox 에서 구독하던 핸들 20 이 이것).
- 연결 0.5초 뒤 읽고, 이후 60초마다 읽는다. 알림(notify)이 오면 그때도 갱신한다.

### Phase 5 에서 고친 것

| 문제 | 수정 |
|---|---|
| BT 콜백(연결 실패, 끊김) 안에서 `delay()` 로 BT RX 스레드를 막음 | 지연 작업 `ble_pad_scan_work` 로 스캔 재시작 |
| `blepad clear` 가 비어 있음 | `bt_unpair(BT_ID_DEFAULT, NULL)` 로 전체 삭제 |
| `cliAdd` 가 `CLI_USE(HW_SD)` 로 감싸짐 | `_USE_HW_CLI` 로 수정 |
| `ble_pad_conn` 중복 선언 | 제거 |
| 활성화 중 `bt_att: Unable to allocate buffer for op 0x52` | TX 버퍼 3 → 8 (`BT_BUF_ACL_TX_COUNT`, `BT_L2CAP_TX_BUF_COUNT`, `BT_ATT_TX_COUNT`, `BT_CONN_TX_MAX`, `BT_CTLR_SDC_TX_PACKET_COUNT`) |

### CLI

| 명령 | 동작 |
|---|---|
| `blepad info` | 상태, 본딩 주소, RSSI, 배터리, 수신 횟수 |
| `blepad scan on\|off` | 스캔 허용/중지 |
| `blepad disc` | 연결 끊기 |
| `blepad clear` | 모든 페어링 삭제 |
| `blepad show` | 입력 실시간 표시 |
| `blepad reset` | 보드 재부팅 |

`xbox_hid_report_t`의 구성은 다음과 같다.
- 스틱 4축: `uint16`, 중심 약 32768
- 트리거 2개: 0~1023
- D-pad: hat 0~8
- 버튼: A, B, X, Y, LB, RB, View, Menu, Xbox, LS, RS



### 버튼 래치 (2026-11-20)

런처는 20ms 마다 현재 상태를 읽어 눌림 엣지를 찾는데, EPD 갱신(0.6~1.25초) 중에는 읽지 않는다. 이 사이에 Xbox 버튼 등을 짧게 눌렀다 떼면 입력을 놓쳤다.

- BLE 알림 콜백에서 버튼 바이트(보고서 13, 14)를 OR 로 모으고, D-pad 는 마지막으로 눌린 방향을 기억한다.
- `blePadGetInfoLatched(user, ...)` 는 현재 보고서에 래치를 합쳐 돌려주고 래치를 지운다. 런처(`BLE_PAD_LATCH_LAUNCHER`)와 게임(`BLE_PAD_LATCH_GAME`)이 따로 쓴다.
- 남은 래치가 엉뚱하게 들어가지 않도록: 게임 스레드는 게임이 멈춰 있는 동안 래치를 계속 비우고, 런처는 게임 메뉴 진입 때 `waitKeyRelease()` 로 비운다.
- 디바운스: 패드는 컨트롤러가 정리한 디지털 값을 바뀔 때만 보내므로 필요 없다. 보드 버튼은 gpio-keys 20ms 디바운스가 있다. 보드 버튼은 래치하지 않아, EPD 갱신 중 짧게 누르면 놓칠 수 있다.

## 보드 버튼 4개

| 버튼 | 핀 | 런처 (Phase 7) | 게임 중 |
|---|---|---|---|
| BTN1 | P0.31 | 위 | Start |
| BTN2 | P1.07 | 아래 | Select |
| BTN3 | P1.06 | 선택/실행 | A |
| BTN4 | P1.05 | 뒤로/메뉴 | 짧게 B, **1초 이상 메뉴(일시정지)** |

API: `buttonGetPressed(ch)`, `buttonEventGetPressed/Released/Repeat` (반복 입력 지원)

## Game Boy 키 매핑 (`gb/gb_run.c` `readInput()`)

| Game Boy | Xbox 패드 | 보드 버튼 |
|---|---|---|
| 방향 | D-pad, 왼쪽 스틱 (중앙 ±50% 넘으면) | - |
| A | A, Y | BTN3 |
| B | B, X | BTN4 (짧게) |
| Select | View | BTN2 |
| Start | Menu | BTN1 |
| 메뉴 / 일시정지 | Xbox 버튼 | BTN4 1초 이상 |

- 메뉴 버튼은 엣지로 감지해 일시정지를 토글하고, 화면 중앙에 "PAUSE" 상자를 FAST 갱신으로 표시한다.
- `gbRunGetMenuRequest()` 로 런처가 메뉴 요청을 받는다 (Phase 7 에서 게임 메뉴로 연결).
- 보드에서 패드 자동 재연결, 일시정지/재개, 배터리 표시까지 확인 (2026-11-20).
