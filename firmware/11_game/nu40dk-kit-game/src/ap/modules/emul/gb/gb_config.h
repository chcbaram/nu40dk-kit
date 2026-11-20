#ifndef GB_CONFIG_H_
#define GB_CONFIG_H_


// 에뮬레이터 코어: core/walnut (Walnut-CGB, MIT)
//   Peanut-GB 와 속도 동일, gnuboy 는 이식 시험 중 폴트로 제외 (docs/05_emulator.md)

// 1 이면 GBC 지원 (RAM +32KB, 메모리 접근마다 CGB 분기로 느려짐)
#ifndef GB_CGB_SUPPORT
#define GB_CGB_SUPPORT      0
#endif

#define GB_LCD_WIDTH        160
#define GB_LCD_HEIGHT       144
#define GB_CART_RAM_MAX     (32 * 1024)

#endif
