# 04. EPAPER 드라이버와 회색 표현

![EPAPER 갱신 파이프라인](images/epaper_pipeline.svg)

## SSD1683 기본

| 항목 | 내용 |
|---|---|
| 해상도 | 400×300, 1bpp (0xFF = 흰색) |
| RAM | 0x24 = 새 이미지(B/W), 0x26 = 이전 이미지(부분 갱신 시 비교용) |
| 윈도우 | 0x44 (X 바이트 범위), 0x45 (Y 범위), 0x4E/0x4F (주소 카운터) |
| 갱신 | 0x22 (Display Update Control 2) 설정 → 0x20 (Master Activation) → BUSY 대기 |
| LUT | 0x32로 MCU에서 웨이브폼 직접 로드 가능 |
| 온도 | 0x18 (센서 선택), 0x1A (온도값 쓰기) → 내장 웨이브폼 선택에 영향 |

### WeAct 예제 초기화 순서 (시작점)

```
HW reset → 0x12 (SW reset) → BUSY 대기
0x01 2B 01 00   ; Driver output, 300 gates
0x11 01         ; X 증가, Y 감소
0x44 00 31      ; X 0..49 바이트
0x45 2B 01 00 00; Y 299..0
0x3C 01         ; Border
0x21 40         ; Display update control 1
0x4E 00 / 0x4F 2B 01
0x24 <15000 bytes>
0x22 F7 → 0x20 → BUSY 대기   ; 전체 갱신 (내장 웨이브폼)
```

## Phase 1 측정 결과 (2026-11-20)

| 항목 | 측정값 | 비고 |
|---|---|---|
| 전체 갱신 (0x22=0xF7) | **BUSY 2,776ms**, 합계 2.8초 | 0x24 + 0x26 각 15KB 전송 30.8ms |
| 부분 갱신 (0x21=0x00, 0x22=0xFC) | **BUSY 565ms, 1.66fps** | 내장 웨이브폼, 0x24 15KB 전송 15.3ms |
| SPI | SPIM3 8MHz | 15KB 약 15ms |
| 화면 방향 | Data entry 0x11=0x03 (X·Y 증가)에서 정상 | WeAct 예제(0x01, Y 감소)와 다르게 써도 문제없음 |
| GB 2×2 4단계 | 육안으로 4단계 모두 구분됨 | 게임 화면 방식으로 확정 |

## 갱신 전략

| 모드 | 용도 | 예상 시간 |
|---|---|---|
| 전체 갱신 (0x22=0xF7) | 화면 전환, 잔상 정리 | 1~3초 (Phase 1에서 실측) |
| 부분 갱신 (0x22=0xFF 계열, 데이터시트 확인) | 메뉴 이동, 게임 화면 | 0.3~0.5초 (내장), 커스텀 LUT로 단축 시도 |
| 윈도우 부분 갱신 | 게임 영역 320×288만 | SPI 전송량 11.5KB |

SPI 전송 시간은 SPIM3 8MHz 기준으로 15KB에 약 15ms, 16MHz면 약 8ms이다. 병목은 패널 갱신 시간이다.

### 잔상(고스팅) 관리

- 부분 갱신을 N번 하면 전체 갱신을 한 번 한다(N은 CLI·설정으로 조절).
- 큰 검은 영역은 잔상이 잘 남는다. UI는 흰 배경 위주로 만든다.

## Hackaday 30fps 기법 검토

출처: https://hackaday.com/2026/10/14/tiny-e-paper-display-delivers-30-fps-of-game-boy-fun/

| 항목 | 내용 |
|---|---|
| 하드웨어 | Waveshare ESP32-S3-ePaper-1.54 (1.54" 패널) |
| 원리 | 픽셀마다 변화량에 맞춰 최대 8펄스만 짧게 구동하는 커스텀 웨이브폼 |
| 회색 | 일부러 덜 구동(under-drive)해 4단계 회색을 만듦 |
| 보정 | 색 전환 종류별로 타이밍을 영상 촬영으로 보정 |
| 공개 여부 | 소스 미공개 (추후 공개 예정) |

**적용 판단**
- SSD1683도 0x32 커스텀 LUT와 부분 갱신을 지원하므로 같은 방향으로 시도할 수 있다.
- 4.2"는 면적이 약 7배이고 전송량도 커서 30fps는 비현실적이다. **목표는 5~10fps**이고 Phase 2에서 실측한다.
- 빠른 웨이브폼은 잔상과 패널 수명에 영향이 있다. 주기적 전체 갱신으로 완화한다.

## 회색 표현 (디더링)

![디더링 패턴](images/dither_patterns.svg)

| 방식 | 내용 | 용도 |
|---|---|---|
| 2×2 블록 디더 | GB 픽셀 1개 → 2×2 화소. 흰/밝은회색/어두운회색/검정 = 0/1/2/4 dot | 게임 화면 (160×144 → 320×288) |
| Bayer 4×4 | 8bit 밝기 → 1bit, 17단계 | 런처 썸네일, 이미지 |
| 웨이브폼 회색 | 0x24/0x26 조합과 짧은 펄스 LUT로 실제 회색 구동 | Phase 2 실험 |

GB 화면을 2배로 키우면 320×288이 400×300 안에 들어간다. 2×2 패턴 4단계가 GB의 4단계 회색과 1:1로 맞는다.

## 구현 (Phase 1)

| 파일 | 역할 |
|---|---|
| `src/hw/driver/epd/ssd1683.c` | SPI(Zephyr `spi_dt_spec`, `epd_spi` 노드)와 BUSY/RES/DC 핀, 명령·RAM 쓰기, 윈도우 설정 |
| `src/hw/driver/epd.c` | 패널 초기화, 전체·부분 갱신(윈도우 지원), 시간 측정, CLI `epd` |
| `src/hw/driver/canvas.c` | 1bpp 400×300 버퍼(15KB), 점·선·박스, 8×16 영문/16×16 한글(`hangul` 폰트), Bayer 4×4, GB 2×2 패턴 |

```c
bool epdInit(void);
bool epdUpdate(epd_mode_t mode);                                   // EPD_MODE_FULL / EPD_MODE_PART
bool epdUpdateWindow(epd_mode_t mode, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
bool epdIsBusy(void);
void epdGetTime(epd_time_t *p_time);                               // spi_us, busy_ms, total_ms

uint8_t *canvasGetBuffer(void);
void     canvasClear(canvas_color_t color);
int16_t  canvasPrintf(int16_t x, int16_t y, canvas_color_t color, const char *fmt, ...);
void     canvasDrawGray(int16_t x, int16_t y, uint8_t gray);      // Bayer 4x4
void     canvasDrawGb2x2(int16_t x, int16_t y, uint8_t shade);     // GB 4단계
```

| CLI | 동작 |
|---|---|
| `epd info` | 상태, SPI 속도, 마지막 갱신 시간 |
| `epd clear [full\|part]` | 흰 화면 |
| `epd test 0~3 [full\|part]` | 0 체커, 1 Bayer 그라데이션, 2 GB 4단계, 3 텍스트 |
| `epd text <str>` | 문자열 출력 |
| `epd spi <MHz>` | SPI 클럭 변경 |
| `epd bench <n> [full\|part]` | 연속 갱신 fps 측정 |
| `epd sleep` | 딥슬립 |

부분 갱신 순서: 0x24에 새 이미지 → 0x21=0x00 → 0x22=0xFC → 0x20 → BUSY 대기 → 0x26에 같은 이미지(다음 차분 기준).
