# 01. 개발환경과 버전

## 호스트

| 항목 | 버전 |
|---|---|
| OS | macOS 27.2 (arm64) |
| 에디터 | VS Code + nRF Connect / Cortex-Debug 확장 |
| 시리얼 터미널 | baram-term (CLI 확인용) |

## SDK와 툴체인

| 항목 | 버전 | 경로 / 비고 |
|---|---|---|
| nRF Connect SDK | **v3.1.0** | `/opt/nordic/ncs/v3.1.0` |
| Zephyr | 4.1.99 (NCS v3.1.0 포함) | `/opt/nordic/ncs/v3.1.0/zephyr` |
| NCS 툴체인 번들 | `5c0d382932` | `/opt/nordic/ncs/toolchains/5c0d382932` |
| Zephyr SDK | 0.17.0 | 툴체인 번들 안 `opt/zephyr-sdk` |
| GCC | arm-zephyr-eabi-gcc 12.2.0 | |
| west | 1.4.0 | |
| CMake | 3.21.0 | |
| Ninja | 1.10.2 | |
| Python | 3.12.4 | 툴체인 번들 포함 |
| pyOCD | 0.35.1 | 플래시·디버그 |
| nrfutil | 8.2.1 | |
| BLE 컨트롤러 | SoftDevice Controller (`CONFIG_BT_LL_SOFTDEVICE`) | |
| libc | picolibc (Zephyr 기본) | |

> 참고: 10_xbox까지는 NCS **v3.2.2**(Zephyr 4.2.99)로 빌드했다. 현재 이 PC에는 v3.1.0만 설치되어 있어 11_game은 v3.1.0으로 빌드했고 문제없이 빌드된다. v3.2.2로 다시 맞출 경우 이 표를 갱신한다.

## 디버그 프로브

| 항목 | 내용 |
|---|---|
| 프로브 | ST-LINK (pyOCD) |
| 타겟 | `nrf52840` |
| 설정 | `.vscode/launch.json` (servertype `pyocd`, target `nrf52`) |

## 빌드

```bash
TC=/opt/nordic/ncs/toolchains/5c0d382932
export PATH=$TC/bin:$PATH
export ZEPHYR_BASE=/opt/nordic/ncs/v3.1.0/zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=$TC/opt/zephyr-sdk

P=<repo>/firmware/11_game/nu40dk-kit-game
cd /opt/nordic/ncs/v3.1.0
west build --build-dir $P/build $P --pristine --board nu40dk_kit/nrf52840 -- -DBOARD_ROOT=$P
```

- 보드 정의는 프로젝트 안 `boards/baram/nu40dk_kit/`에 있다(out-of-tree). 그래서 `-DBOARD_ROOT`를 꼭 지정한다.
- sysbuild + Partition Manager를 사용한다. 결과물은 `build/merged.hex`이다.

## 플래시

```bash
cd <repo>/firmware/11_game/nu40dk-kit-game
$TC/bin/pyocd flash -t nrf52840 --script tools/pyocd_stlink.py build/merged.hex
$TC/bin/pyocd reset -t nrf52840 --script tools/pyocd_stlink.py
```

- ST-LINK는 nRF52의 CTRL-AP(AP#1)에 접근하면 `STLink error (29): Bad AP`를 낸다. pyOCD 0.35.1의 nrf52 타겟은 연결할 때 CTRL-AP를 검사하므로 그대로는 실패한다.
- `tools/pyocd_stlink.py`는 AHB-AP(AP#0)만 쓰도록 하고, CTRL-AP 관련 단계(`check_ctrl_ap_idr`, `check_flash_security`, `persist_unlock`)를 뺀다. MEM-AP로 Flash 쓰기는 정상 동작한다.
- APPROTECT가 걸린 칩의 복구(mass erase)는 CTRL-AP가 필요해서 ST-LINK로는 불가능하다. 그때는 J-Link나 CMSIS-DAP 프로브를 쓴다.
- 시리얼 CLI는 ST-LINK VCP(`/dev/cu.usbmodem*`, 115200)로 연결된다.

## 프로젝트 구조

```
11_game/
├── docs/                    ← 이 문서들
└── nu40dk-kit-game/
    ├── boards/baram/nu40dk_kit/   보드 DTS, pinctrl
    ├── prj.conf
    ├── CMakeLists.txt
    └── src/
        ├── main.c           hwInit() → apInit() → apMain()
        ├── bsp/             delay/millis, 링커 스크립트(.module 섹션)
        ├── common/          def.h, qbuffer, hw/include/*.h, cli
        ├── hw/              hw_def.h(_USE_HW_XXX), hw.c, driver/*
        └── ap/              ap.c, modules/(MODULE_DEF 자동 등록)
```

- 드라이버는 `hw_def.h`의 `_USE_HW_XXX`로 켜고 끈다. CLI 명령은 `_USE_CLI_HW_XXX`로 켠다.
- 앱 모듈은 `MODULE_DEF(name)`로 등록한다. `moduleInit()`이 우선순위 순서로 초기화한다.
