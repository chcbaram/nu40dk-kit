# 02. 하드웨어

![시스템 블럭도](images/system_block.svg)

## MCU

| 항목 | 값 |
|---|---|
| SoC | nRF52840 (Cortex-M4F, 64MHz, FPU) |
| 모듈 | Nucode NCRB40N02VC |
| Flash | 1MB 내장 (외부 Flash 없음): 앱 504KB + GB ROM 512KB + 설정 8KB |
| RAM | 256KB (PSRAM 없음). 최종 빌드 약 99% 사용 |
| 무선 | BLE 5 Central (Xbox 컨트롤러 연결) |

## 4.2" EPAPER (WeAct Studio)

| 항목 | 값 |
|---|---|
| 패널 | 4.2" 흑백 (E042A87), 400×300 |
| 컨트롤러 | SSD1683 |
| 인터페이스 | 4-wire SPI (쓰기 전용), **SPIM2 8MHz** |
| 프레임 크기 | 1bpp, 50바이트 × 300줄 = 15,000바이트 |
| 구동 | 커스텀 LUT. 게임은 픽셀별 마스크 구동(회색 모드), 런처·RPG 는 FAST/QUICK/FLASH (04 문서) |
| 자료 | https://github.com/WeActStudio/WeActStudio.EpaperModule |

### EPAPER 핀맵 (확장 포트 2×7 헤더)

| EPAPER | nRF52840 | 기능 | DTS |
|---|---|---|---|
| BUSY | P1.00 | 입력, High = 동작 중 | `epd_busy` |
| RES | P1.01 | 리셋 (Low 활성) | `epd_res` |
| DC | P1.02 | Data/Command 선택 | `epd_dc` |
| CS | P1.03 | SPI 칩 선택 | `&spi2` cs-gpios |
| SCL | P1.04 | SPI SCK | `spi2_default` |
| SDA | P1.14 | SPI MOSI | `spi2_default` |

## 주변장치 할당

| 기능 | 장치 | 핀 | 비고 |
|---|---|---|---|
| CLI / 로그 | UART0 | TX P0.06, RX P0.08 | 115200. 유일한 CLI 경로 |
| EPAPER | SPIM2 | P1.03/04/14 + P1.00~02 | 8MHz, 15KB 전송 약 15ms (4KB 씩 나눠 DMA) |
| SD 카드 | SPIM1 | SCK P0.27, MOSI P0.26, MISO P0.04, CS P0.23, CD P1.15 | 8MHz, FatFs. ROM·세이브·RPG 음성 |
| 오디오 | I2S0 → MAX98357 | SCK P1.09, LRCK P1.08, SDOUT P0.19, MUTE P1.10 | 모노 앰프. 믹서 2채널 (GB 사운드 / RPG 음성) |
| 버튼 4개 | GPIO (gpio-keys) | P0.31, P1.07, P1.06, P1.05 | Active Low, 20ms 디바운스, 입력 콜백에서 눌림 래치 |
| LED | GPIO | P0.14 | |
| 컨트롤러 | BLE (내장 무선) | – | Xbox 무선 컨트롤러, HID over GATT |

### 11_game에서 뺀 장치

| 장치 | 이유 |
|---|---|
| OLED (I2C0, P0.21/22) | EPAPER로 대체. RAM 16KB 절약 |
| PDM 마이크 (P0.05/07) | 사용 안 함. RAM 36KB 절약 |
| WS2812 (SPIM3, P0.20) | 사용 안 함 |
| USB CDC-ACM | RAM 9.5KB 확보 (2026-11-20). 되돌리려면 hw_def.h `_USE_HW_CDC`/`_USE_HW_USB`, prj.conf |

## SPI 인스턴스

nRF52840은 SPIM0/TWIM0, SPIM1/TWIM1이 같은 자원을 공유한다.

| 인스턴스 | 최대 속도 | 용도 |
|---|---|---|
| SPIM0 | 8MHz | 미사용 |
| SPIM1 | 8MHz | SD 카드 |
| SPIM2 | 8MHz | **EPAPER** |
| SPIM3 | 32MHz | 미사용 (errata 198) |

### EPAPER SPI 를 SPIM2 8MHz 로 정한 이유

| 시도 | 결과 |
|---|---|
| SPIM3 32MHz | 무작위 점 (비트 오류). SSD1683 클럭 규격 20MHz 초과 |
| SPIM3 16MHz | 정지 화면은 정상, 게임 중 화면 아래쪽 누락 |
| SPIM3 8MHz | 프레임을 자주 보내자 같은 누락 재발 |
| **SPIM2 8MHz** | **정상 (최종)** |

- 원인은 **nRF52840 errata 198**이다. SPIM3 의 TX 버퍼가 CPU 가 쓰는 RAM 블록과 같으면 전송 데이터가 손상된다. 에뮬레이터가 RAM 을 계속 쓰는 게임 중에만 나타났다.
- 16MHz 는 SSD1683 홀드 시간도 위반한다(아래 표). SPIM0~2 는 8MHz 가 최대라 SPIM2 8MHz 로 확정했다. 핀은 그대로다.
- 비용은 16MHz 대비 프레임당 약 7ms 다. 화면 속도의 병목은 패널 구동 시간이라 체감 차이가 없다.

SSD1683 SPI 쓰기 타이밍 (데이터시트 Table 12-1):

| 항목 | 규격 | 8MHz | 16MHz | 32MHz |
|---|---|---|---|---|
| f<sub>SCL</sub> | 최대 20MHz | OK | OK | 초과 |
| t<sub>SCLHIGH</sub> / t<sub>SCLLOW</sub> | 최소 25ns | 62ns | 31ns | 15.6ns (위반) |
| t<sub>SIHLD</sub> (SCL 상승 후 데이터 유지) | 최소 40ns | 62ns | 31ns (**위반**) | 위반 |

자세한 경과는 09 문서(최적화)에 있다.
