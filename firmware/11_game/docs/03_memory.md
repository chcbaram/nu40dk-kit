# 03. 메모리 예산

![메모리 맵](images/memory_map.svg)

## 빌드 결과 (Phase 0)

| 항목 | 10_xbox | 11_game Phase 0 |
|---|---|---|
| FLASH | 약 340KB | **329KB / 1016KB (31.6%)** |
| RAM | 약 184KB | **111KB / 256KB (42.5%)** |
| RAM 여유 | 약 72KB | **약 145KB** |

## 줄인 항목

| 항목 | 10_xbox | 11_game |
|---|---|---|
| PDM 마이크 버퍼·스택 | 36KB | 제거 |
| OLED 프레임버퍼·폰트 버퍼 | 16KB | 제거 |
| 로그 부트·리스트 버퍼 | 8KB | 2KB (`HW_LOG_*_BUF_MAX` 1024) |
| I2S TX slab | 12KB (16개) | 6KB (8개 × 4ms) |
| 믹서 | 4채널, 12KB | 2채널, 6KB |
| WS2812, I2C 드라이버 | 사용 | 제거 |

다음에 더 줄일 수 있는 것: 스레드 스택(main 8KB, 시스템 워크큐 8KB, CLI 8KB, sd_mgr 6KB). Thread analyzer로 실측한 뒤 조정한다.

## Phase 3 후 (2026-11-20)

| 항목 | 값 |
|---|---|
| FLASH (app) | 407KB / 504KB |
| RAM | 186KB / 256KB (여유 약 70KB) |
| Walnut `struct gb_s` (GB 전용) | 16.9KB (GBC 지원 시 약 50KB) |
| cart RAM | 32KB |
| LCD 2bpp 버퍼 | 5.6KB |

## 예정 사용처 (Phase 1~7)

| 용도 | 크기 |
|---|---|
| EPD 1bpp 버퍼 2개 (현재/이전, 디더·차분용) | 30KB |
| Peanut-GB 상태 (WRAM 8K, VRAM 8K, OAM, HRAM 등) | 약 20KB |
| Cart RAM (배터리 세이브) | 최대 32KB |
| APU 버퍼 | 약 4KB |
| 런처 텍스트 셀 버퍼 | 1KB 미만 |
| **합계** | **약 90KB** (여유 145KB 안에 들어감) |

## Flash 파티션 (계획)

| 파티션 | 주소 | 크기 | 용도 |
|---|---|---|---|
| app | 0x00000 | 504KB | 펌웨어 |
| rom_storage | 0x7E000 (예정) | 512KB | 실행할 GB ROM (SD에서 복사, XIP로 직접 실행) |
| settings_storage | 0xFE000 | 8KB | BT 본딩 정보 (NVS) |

- 지금은 Partition Manager가 app에 0x0~0xFE000 전체를 준다. Phase 3에서 `pm_static.yml`로 `rom_storage`를 나눈다.
- 플래시가 메모리 맵에 들어가 있어서(XIP), Peanut-GB의 ROM 읽기 콜백은 `rom_base[addr]`를 그대로 반환하면 된다. RAM에 복사할 필요가 없다.
