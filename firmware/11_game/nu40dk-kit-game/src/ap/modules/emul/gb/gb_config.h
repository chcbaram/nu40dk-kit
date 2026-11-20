#ifndef GB_CONFIG_H_
#define GB_CONFIG_H_


// 사용할 에뮬레이터 코어 (한 번에 하나만 빌드)
//   빌드 시 -DGB_CORE=<n> 으로 바꿀 수 있다
#define GB_CORE_WALNUT      1     // core_walnut : Walnut-CGB (MIT), GBC 지원 가능
#define GB_CORE_PEANUT      2     // core_peanut : Peanut-GB  (MIT), GB 전용
#define GB_CORE_GNUBOY      3     // core_gnuboy : gnuboy     (GPL-2)

#ifndef GB_CORE
#define GB_CORE             GB_CORE_WALNUT
#endif

// Walnut-CGB 전용: 1 이면 GBC 지원 (RAM +32KB, 메모리 접근마다 CGB 분기로 느려짐)
#ifndef GB_CGB_SUPPORT
#define GB_CGB_SUPPORT      0
#endif

#define GB_LCD_WIDTH        160
#define GB_LCD_HEIGHT       144
#define GB_CART_RAM_MAX     (32 * 1024)

#endif
