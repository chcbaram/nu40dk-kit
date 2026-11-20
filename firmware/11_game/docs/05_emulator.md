# 05. 에뮬레이터

![에뮬레이터 데이터 흐름](images/emulator_flow.svg)

## 선정 결과: Game Boy (DMG) — Peanut-GB + minigb_apu

| 후보 | RAM | 판단 |
|---|---|---|
| **Peanut-GB** (MIT, 헤더 하나) | 약 20KB | **선정.** ROM을 콜백으로 읽고, 화면을 라인 단위 2bit로 출력 |
| gnuboy (기존 포팅, GPL-2) | 약 510KB 정적 (`patpix` 256KB 등) | RAM 부족 |
| pNesX (NES, 기존 포팅) | 약 70KB | 라이선스 불명확, 컬러→회색 변환 필요 → 추후 옵션 |
| Doom / fMSX / fake86 | MB 단위 | 불가 |

**GB를 고른 이유**
- 원래 4단계 회색이라 흑백 EPAPER와 잘 맞는다.
- 2배 확대(320×288)가 400×300 안에 들어가고 2×2 디더로 4단계를 그대로 표현할 수 있다.

## 기존 포팅 참고

| 항목 | 위치 |
|---|---|
| gnuboy 포팅 (가장 깔끔) | `Teensy41/emul_gnuboy/src/ap/gnuboy/sys/orocaboy` |
| 입력 매핑 (`pad_set`) | `esp32-s3-boy_fw/fw/s3-boy-gnuboy/main` |
| 파일 목록 (`filer.c`) | `esp32-s3-boy_fw/fw/s3-boy-gnuboy/main/ap/gnuboy/src/port/filer.c` |

## ROM 로딩

1. 런처에서 SD 카드 `/roms/*.gb`를 고른다.
2. 헤더 CRC를 비교해 `rom_storage` Flash 파티션에 이미 같은 ROM이 있으면 건너뛴다.
3. 다르면 Flash를 지우고 복사한다(진행률 표시).
4. Peanut-GB의 `rom_read(addr)` 콜백은 XIP 포인터 `rom_base[addr]`를 반환한다.
5. Cart RAM은 RAM 버퍼(최대 32KB)에 두고, 게임 종료 시 SD에 `<rom>.sav`로 저장한다.

## 실행 구조

| 항목 | 내용 |
|---|---|
| 에뮬 스레드 | 59.7Hz 프레임 단위로 `gb_run_frame()` 실행 |
| 화면 | `lcd_draw_line(line, pixels[160])` 콜백 → 2×2 디더 → EPD 1bpp 버퍼 |
| 표시 | 디스플레이 스레드가 BUSY가 풀릴 때마다 최신 프레임만 부분 갱신 (에뮬 속도와 분리) |
| 프레임 스킵 | 표시하지 않을 프레임은 라인 렌더링 생략 (CPU 절약) |

## 성능 리스크와 대응 (Phase 3에서 확인)

64MHz Cortex-M4에서 GB를 원래 속도로 돌릴 수 있는지가 가장 큰 불확실성이다. `gb bench`로 화면 없이 측정한 뒤 다음 순서로 대응한다.

1. 렌더 스킵 (`frame_skip`, 인터레이스 옵션)
2. `-O2`/`-O3`, Peanut-GB 빌드 옵션 조정
3. CPU 핫루프를 RAM에서 실행 (`__ramfunc`)
4. 그래도 부족하면 저속 실행을 허용 (EPAPER 특성상 체감 영향이 작음)

## 계획 중인 CLI

`gb load <file> | run | stop | bench | info`
