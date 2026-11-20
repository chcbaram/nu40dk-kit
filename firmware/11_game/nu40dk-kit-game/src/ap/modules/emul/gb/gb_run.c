/*
 * gb_run.c
 *
 *  에뮬레이터 실행 + EPAPER 표시
 *
 *  [에뮬 스레드]    59.7Hz 페이싱으로 프레임 실행. 디스플레이가 새 프레임을 원할 때만 렌더링.
 *  [디스플레이 스레드] 렌더링된 2bpp 버퍼 → 2x2 디더(1bpp) → 캔버스 (40,6) 320x288
 *                   → EPD FAST 모드 윈도우 갱신 (약 0.1초)
 *
 *  두 스레드는 lcd_buf 를 공유한다. 에뮬은 disp_ready(표시 대기) 일 때만 렌더링하고,
 *  디스플레이는 변환을 끝내면 lcd_buf 를 돌려준다. (변환은 수 ms 라 복사 버퍼가 필요 없다)
 */
#include "ap_def.h"
#include "gb_core.h"
#include "gb_emul.h"
#include "gb_rom.h"
#include "gb_run.h"
#include "remote/ble_pad.h"


#define GB_FRAME_US         16743     // 59.73Hz
#define GB_SCR_X            40        // 400x300 화면에서 게임 영역 위치 (2배 확대 320x288)
#define GB_SCR_Y            6
#define GB_SCR_W            (GB_LCD_WIDTH * 2)
#define GB_SCR_H            (GB_LCD_HEIGHT * 2)

#define EMU_THREAD_STACK    (4 * 1024)
#define EMU_THREAD_PRI      7         // 가장 낮게: CPU 를 쉬지 않고 쓰는 게임에서도 CLI/BT 가 돌도록
#define DISP_THREAD_STACK   (2 * 1024)
#define DISP_THREAD_PRI     4


static bool init(void);
static void emuThread(void *arg1, void *arg2, void *arg3);
static void dispThread(void *arg1, void *arg2, void *arg3);

MODULE_DEF(gb_run)
{
  .name     = "gb_run",
  .priority = MODULE_PRI_NORMAL,
  .init     = init
};

static K_THREAD_STACK_DEFINE(emu_stack, EMU_THREAD_STACK);
static K_THREAD_STACK_DEFINE(disp_stack, DISP_THREAD_STACK);
static struct k_thread emu_thread;
static struct k_thread disp_thread;
static K_SEM_DEFINE(frame_sem, 0, 1);

static volatile bool is_run       = false;
static volatile bool disp_ready   = false;   // 디스플레이가 새 프레임을 기다림
static volatile bool frame_ready  = false;   // lcd_buf 에 새 프레임이 있음
static volatile bool req_full     = false;   // 다음 표시를 전체 갱신으로
static volatile bool is_pause     = false;   // 일시정지 (메뉴)
static volatile bool menu_req     = false;   // 메뉴 요청 (런처가 읽고 지움)

#define MENU_LONG_PRESS_MS  1000            // 보드 BTN4 길게 누르면 메뉴

static gb_run_stat_t stat;

#define GB_AUDIO_CH         0         // 믹서 채널 (효과음은 빈 채널 사용)
#define GB_AUDIO_MAX        768       // 1 프레임 샘플 수 상한 (22050Hz: 369 x 2 = 738)
#define GB_AUDIO_RATE       22050     // minigb_apu AUDIO_SAMPLE_RATE 와 같아야 함 (CMakeLists.txt)
#define SYS_AUDIO_RATE      16000     // 게임 밖 기본 I2S 주파수
static int16_t       audio_buf[GB_AUDIO_MAX];
static volatile bool sound_enable = true;
static volatile uint8_t  key_inject      = 0;     // CLI 테스트용 입력 (gb key)
static volatile uint32_t key_inject_end  = 0;
static volatile bool win_full     = true;    // true: 전체 폭(400px) DMA 한 번, false: 게임 영역(320px) 줄 단위

// 2x2 디더 룩업: GB 1 바이트(4 픽셀, 2bit, MSB 왼쪽) → 캔버스 1 바이트(8 픽셀, 1=흰색)
static uint8_t dither_lut[2][256];


/*
 * ink 모드: 픽셀(블록)별 펄스 카운터 (InkBoy 방식)
 *
 *   - GB 픽셀(=화면 2x2 블록) 마다 4bit 카운터. 명암이 바뀌면 카운터 = 예산(ink_budget)
 *   - 스캔마다 카운터가 남은 블록만 목표색으로 구동하고 1 씩 줄인다. 0 이면 구동 안 함(HOLD)
 *   - 변하지 않은 블록은 전압이 걸리지 않아 DC 가 쌓이지 않는다 (hold 구동의 패널 잔상 문제 해결)
 *   - 카운터는 비트 슬라이스: ink_cnt[k][w] 의 비트 b = 픽셀 (w*32+b) 카운터의 k 번째 비트
 *   - 마스크는 전체 폭 400px 로 만든다 (SPI DMA 를 큰 덩어리로). 흰색 마스크는 캔버스 버퍼를 빌려 쓴다
 */
#define DISP_MODE_FRAME     0         // 프레임 전체를 FAST 로 갱신 (Phase 4~7)
#define DISP_MODE_INK       1         // 픽셀별 펄스 카운터 (Phase 8)
#define INK_WORDS           ((GB_LCD_WIDTH * GB_LCD_HEIGHT) / 32)    // 720
#define INK_CNT_BITS        4

static volatile uint8_t disp_mode    = DISP_MODE_INK;
static volatile bool    disp_busy    = false;                 // EPD 갱신 중
static uint8_t          ink_budget   = 8;                     // 명암이 바뀐 블록에 줄 펄스 수 (1~7)
static uint8_t          ink_frames   = 3;                     // 스캔 1 회 펄스 길이 (프레임, 8ms)
static uint32_t         ink_frame_ms = 83; 
static uint8_t          ink_fr       = 0x05;                  // 커스텀 LUT 프레임 속도 125Hz (50Hz 는 체감 차이 없이 스캔만 느려짐)                   // 새 GB 프레임을 받는 최소 간격
static uint8_t          ink_shade[GB_LCD_HEIGHT][GB_LCD_WIDTH / 4];   // 현재 목표 (2bpp)
static uint8_t          ink_base[GB_LCD_HEIGHT][GB_LCD_WIDTH / 4];    // 바뀌기 직전 목표 (2bpp): 이 패턴과 다른 디더 점만 구동
static uint32_t         ink_cnt[INK_CNT_BITS][INK_WORDS];
static uint8_t          ink_black[GB_SCR_H * EPD_STRIDE];     // 검정 구동 마스크 (게임 영역 줄, 전체 폭)
static uint8_t          expand4[16];                          // 4 픽셀 니블 → 8 화면 픽셀 바이트
static uint8_t          diff_nib[256];                        // 2bpp 바이트 차이 → 바뀐 픽셀 니블

/*
 * 회색(grayscale) 모드: 디더 점 대신 2x2 블록 전체를 한 방향으로, 전환별 펄스 수만큼만 구동해
 * 입자를 중간에 멈춘다 (InkBoy grayscale). shade: 0 흰색, 1 연회색, 2 진회색, 3 검정
 *   gray_dose[이전][새] : 펄스 수 (InkBoy 기본 표)
 *   ink_dir             : 1 = 검정 방향 구동 (비트 슬라이스, GB 픽셀당 1bit)
 */
static volatile bool    ink_gray     = true;
static uint32_t         ink_scene_px = (GB_LCD_WIDTH * GB_LCD_HEIGHT) * 40 / 100;  // 장면 전환 기준 (바뀐 픽셀 수, 0 = 끔)
static uint32_t         scene_ms     = 0;
static uint32_t         ink_quiet_ms  = 1500;                 // 조용한 상태가 이만큼 이어지면 정리 (0 = 끔)
static uint32_t         ink_quiet_px  = 120;                  // 프레임당 바뀐 픽셀이 이보다 적으면 조용함
static uint32_t         ink_quiet_act = 3000;                 // 직전 정리 뒤 바뀐 픽셀 합이 이 이상일 때만
static uint32_t         quiet_ms      = 0;
static uint32_t         quiet_act     = 0;
static uint16_t         ink_rev_pct  = 0;                     // 되돌아갈 때 펄스 비율 % (0 = 보정 안 함). gb retarget
static bool             ink_same_add = false;                 // 같은 방향으로 더 갈 때 남은 펄스 더하기
static uint8_t          gray_frames  = 4;                     // 회색 모드 스캔 1 회 프레임 수 (32ms)
                                                              //   스캔당 고정 비용(SPI 15ms + 방전 16ms)이 커서, 펄스를 길게·횟수를 적게 하면
                                                              //   한 번 바뀌는 데 걸리는 시간이 짧아져 움직임 잔상이 준다 (16ms x 12 → 32ms x 6)
static uint8_t          gray_dose[4][4] =
{
  //  W  L  D  K   (새)      32ms 펄스 기준. 검정 쪽을 InkBoy 표보다 늘리고 흰색 방향도 같은 비율로 맞춤
  {   0, 1, 2, 6 },   // W (이전)        (흰색 방향을 약하게 두면 검었던 자리가 회색 잔상으로 남음)
  {   5, 0, 1, 4 },   // L               흰색으로 가는 펄스 +1 (L 4→5, D 5→6, K 6→7):
  {   6, 1, 0, 3 },   // D               움직이는 캐릭터 뒤 잔상이 줄어듦 (2026-11-20)
  {   7, 3, 2, 0 },   // K
};
static uint32_t         ink_dir[INK_WORDS];




static void makeDitherLut(void)
{
  // shade 별 2x2 패턴 (1 = 검정): [shade][row] 2bit (왼쪽, 오른쪽)
  static const uint8_t pat[4][2] =
  {
    {0x0, 0x0},   // 0 흰색
    {0x2, 0x0},   // 1 밝은 회색
    {0x2, 0x1},   // 2 어두운 회색
    {0x3, 0x3},   // 3 검정
  };

  for (int v = 0; v < 256; v++)
  {
    for (int row = 0; row < 2; row++)
    {
      uint8_t black = 0;
      for (int p = 0; p < 4; p++)
      {
        uint8_t shade = (v >> (6 - p * 2)) & 3;
        black |= pat[shade][row] << (6 - p * 2);
      }
      dither_lut[row][v] = ~black;
    }
  }
}

static void convertFrameFrom(const uint8_t *p_src)
{
  uint8_t *p_fb = canvasGetBuffer();

  for (int y = 0; y < GB_LCD_HEIGHT; y++)
  {
    uint8_t *p_dst0 = &p_fb[(GB_SCR_Y + y * 2) * CANVAS_STRIDE + GB_SCR_X / 8];
    uint8_t *p_dst1 = p_dst0 + CANVAS_STRIDE;

    for (int x = 0; x < GB_LCD_WIDTH / 4; x++)
    {
      uint8_t v = *p_src++;
      p_dst0[x] = dither_lut[0][v];
      p_dst1[x] = dither_lut[1][v];
    }
  }
}

// 게임 화면 전체(흰 배경 + 테두리 + 게임 영역)를 캔버스에 그린다
static void drawGameCanvas(const uint8_t *p_src)
{
  canvasClear(CANVAS_WHITE);
  canvasDrawRect(GB_SCR_X - 2, GB_SCR_Y - 2, GB_SCR_W + 4, GB_SCR_H + 4, CANVAS_BLACK);
  convertFrameFrom(p_src);
}

/*
 * 마스크 증분 갱신
 *   마스크는 스캔마다 지우지 않고 유지한다. 4 픽셀 묶음(마스크 4 바이트) 단위로 필요한 곳만 다시 계산한다.
 *   전송은 매 스캔 전체 (EPD 가 구동 후 RAM 을 바꾸므로 바뀐 줄만 보내면 화면이 깨짐).
 */
static int16_t ink_dirty_min = 0;          // 마스크가 바뀐 줄 범위 (게임 영역 기준 0~287)
static int16_t ink_dirty_max = -1;

static inline void inkMarkDirty(int y)      // GB 줄 → 화면 2 줄
{
  int r0 = y * 2;
  if (ink_dirty_max < ink_dirty_min) { ink_dirty_min = r0; ink_dirty_max = r0 + 1; return; }
  if (r0 < ink_dirty_min)     ink_dirty_min = r0;
  if (r0 + 1 > ink_dirty_max) ink_dirty_max = r0 + 1;
}

// GB 줄 y, 4 픽셀 묶음 gx(0~39) 의 마스크 4 바이트를 현재 카운터/목표로 다시 계산
static void inkUpdateGroup(uint8_t *p_white, int y, int gx)
{
  int      w      = y * (GB_LCD_WIDTH / 32) + gx / 8;
  int      sh     = (gx % 8) * 4;
  uint8_t  nib    = ((ink_cnt[0][w] | ink_cnt[1][w] | ink_cnt[2][w] | ink_cnt[3][w]) >> sh) & 0xF;
  int      bx     = GB_SCR_X / 8 + gx;
  uint8_t *pb0    = &ink_black[(y * 2) * EPD_STRIDE + bx];
  uint8_t *pw0    = &p_white[(y * 2) * EPD_STRIDE + bx];
  uint8_t  b0 = 0, b1 = 0, w0 = 0, w1 = 0;

  if (nib)
  {
    uint8_t act = expand4[nib];

    if (ink_gray)
    {
      uint8_t dir = (ink_dir[w] >> sh) & 0xF;
      b0 = b1 = expand4[nib & dir];
      w0 = w1 = expand4[nib & ~dir];
    }
    else
    {
      uint8_t v  = ink_shade[y][gx];
      uint8_t bv = ink_base[y][gx];
      uint8_t t0 = dither_lut[0][v];
      uint8_t t1 = dither_lut[1][v];
      uint8_t d0 = act & (t0 ^ dither_lut[0][bv]);
      uint8_t d1 = act & (t1 ^ dither_lut[1][bv]);
      w0 = t0 & d0;  b0 = ~t0 & d0;
      w1 = t1 & d1;  b1 = ~t1 & d1;
    }
  }

  if (pb0[0] != b0 || pb0[EPD_STRIDE] != b1 || pw0[0] != w0 || pw0[EPD_STRIDE] != w1)
  {
    pb0[0] = b0;  pb0[EPD_STRIDE] = b1;
    pw0[0] = w0;  pw0[EPD_STRIDE] = w1;
    inkMarkDirty(y);
  }
}

// 32 픽셀 워드 w 에서 nib_mask(8 묶음 x 4bit) 에 해당하는 묶음들을 다시 계산
static void inkUpdateWord(uint8_t *p_white, int w, uint32_t pix_mask)
{
  int y   = w / (GB_LCD_WIDTH / 32);
  int gx0 = (w % (GB_LCD_WIDTH / 32)) * 8;

  for (int g = 0; g < 8; g++, pix_mask >>= 4)
  {
    if (pix_mask & 0xF) inkUpdateGroup(p_white, y, gx0 + g);
  }
}

static uint8_t *inkWhiteBuf(void)
{
  return canvasGetBuffer() + GB_SCR_Y * EPD_STRIDE;   // 캔버스를 흰색 마스크 버퍼로 사용
}

// 새 프레임과 현재 목표를 비교해 명암이 바뀐 GB 픽셀의 카운터를 예산으로 설정
//   32 픽셀(8 바이트)을 32bit 두 개로 비교해 같으면 건너뛰고, 다르면 바이트당 룩업 한 번
// 장면 전환 감지: 이전 목표(ink_shade)와 비교해 바뀐 GB 픽셀 수 (2비트 픽셀, 32비트 단위)
static uint32_t inkChangedPixels(const uint8_t *p_new)
{
  const uint8_t *p_old = &ink_shade[0][0];
  uint32_t       cnt   = 0;

  for (uint32_t i = 0; i < sizeof(ink_shade); i += 4)
  {
    uint32_t n, o, x;
    memcpy(&n, p_new + i, 4);
    memcpy(&o, p_old + i, 4);
    x = n ^ o;
    cnt += __builtin_popcount((x | (x >> 1)) & 0x55555555UL);
  }
  return cnt;
}

static void inkPlan(const uint8_t *p_new)
{
  uint8_t *p_old = &ink_shade[0][0];

  for (int w = 0; w < INK_WORDS; w++, p_new += 8, p_old += 8)
  {
    uint32_t n0, n1, o0, o1;

    memcpy(&n0, p_new, 4); memcpy(&n1, p_new + 4, 4);
    memcpy(&o0, p_old, 4); memcpy(&o1, p_old + 4, 4);
    if (n0 == o0 && n1 == o1) continue;

    if (ink_gray)
    {
      // 바뀐 픽셀마다 (이전, 새) 명암으로 펄스 수와 방향을 정한다
      for (int b = 0; b < 8; b++)
      {
        uint8_t d = diff_nib[p_new[b] ^ p_old[b]];
        for (int p = 0; p < 4 && d; p++)
        {
          if (!(d & (1 << p))) continue;

          uint8_t  o    = (p_old[b] >> (6 - p * 2)) & 3;
          uint8_t  n    = (p_new[b] >> (6 - p * 2)) & 3;
          int      dose = gray_dose[o][n];
          uint32_t bit  = 1u << (b * 4 + p);

          // retarget: 이전 전환이 끝나기 전에 목표가 바뀌면 남은 펄스(r)로 위치를 추정한다 (RAM 0 근사).
          //   되돌아감(방향 반대): (표 - r) x ink_rev_pct %, 표 값 이하.
          //     100% (= 표 - r) 은 잔상이 늘었다. 입자는 처음 몇 펄스에 크게 움직여서 받은 만큼만
          //     되돌리면 덜 돌아온다 (InkBoy 기본 150%).
          //   같은 방향 더 감: ink_same_add 면 표 + r
          if (ink_rev_pct > 0 || ink_same_add)
          {
            int r = 0;
            for (int k = 0; k < INK_CNT_BITS; k++)
              if (ink_cnt[k][w] & bit) r |= 1 << k;
            if (r > 0)
            {
              bool cur_dark = (ink_dir[w] & bit) != 0;
              bool new_dark = n > o;
              if (cur_dark != new_dark)
              {
                if (ink_rev_pct > 0)
                {
                  int rev = ((dose - r) * ink_rev_pct + 99) / 100;
                  if (rev < 1)    rev = 1;
                  if (rev < dose) dose = rev;
                }
              }
              else if (ink_same_add)
              {
                dose += r;
              }
              if (dose > 15) dose = 15;
            }
          }

          for (int k = 0; k < INK_CNT_BITS; k++)
          {
            if ((dose >> k) & 1) ink_cnt[k][w] |= bit;
            else                 ink_cnt[k][w] &= ~bit;
          }
          if (n > o) ink_dir[w] |= bit;     // 어두워짐 → 검정 방향
          else       ink_dir[w] &= ~bit;
        }
      }
      memcpy(p_old, p_new, 8);
      inkUpdateWord(inkWhiteBuf(), w, 0xFFFFFFFF);
      continue;
    }

    uint32_t changed = 0;
    uint8_t *p_base  = &ink_base[0][0] + (p_old - &ink_shade[0][0]);
    for (int b = 0; b < 8; b++)
    {
      uint8_t d = diff_nib[p_new[b] ^ p_old[b]];
      changed |= (uint32_t)d << (b * 4);

      // 바뀐 픽셀의 기준 = 바뀌기 직전 목표. 블록 안에서 기준 패턴과 새 패턴이 다른 점만 구동한다
      uint8_t m = expand4[d];
      p_base[b] = (p_base[b] & ~m) | (p_old[b] & m);
    }
    for (int k = 0; k < INK_CNT_BITS; k++)
    {
      ink_cnt[k][w] = (ink_cnt[k][w] & ~changed) | (((ink_budget >> k) & 1) ? changed : 0);
    }
    memcpy(p_old, p_new, 8);
    inkUpdateWord(inkWhiteBuf(), w, changed);
  }
}

// 카운터가 남은 블록의 마스크 생성. 반환 = 구동할 GB 픽셀 수
//   버퍼는 한 번에 지우고, 카운터가 남은 32 픽셀 묶음만 채운다 (여백은 0 = 구동 안 함)
static uint32_t inkBuildMask(uint8_t *p_white)
{
  uint32_t pending = 0;

  memset(ink_black, 0, GB_SCR_H * EPD_STRIDE);
  memset(p_white,   0, GB_SCR_H * EPD_STRIDE);

  for (int w = 0; w < INK_WORDS; w++)
  {
    uint32_t active = ink_cnt[0][w] | ink_cnt[1][w] | ink_cnt[2][w] | ink_cnt[3][w];

    if (active == 0) continue;
    pending += __builtin_popcount(active);

    int            y    = w / (GB_LCD_WIDTH / 32);
    int            gx0  = (w % (GB_LCD_WIDTH / 32)) * 8;
    const uint8_t *p_sh = &ink_shade[y][gx0];
    const uint8_t *p_bs = &ink_base[y][gx0];
    uint8_t       *pb0  = &ink_black[(y * 2) * EPD_STRIDE + GB_SCR_X / 8 + gx0];
    uint8_t       *pw0  = &p_white[(y * 2) * EPD_STRIDE + GB_SCR_X / 8 + gx0];

    uint32_t dir = ink_dir[w];

    for (int g = 0; g < 8; g++, active >>= 4, dir >>= 4)
    {
      uint8_t nib = active & 0xF;
      if (nib == 0) continue;

      if (ink_gray)
      {
        // 블록 전체를 한 방향으로 (점 없음)
        uint8_t blk = expand4[nib & (dir & 0xF)];          // 검정 방향 픽셀
        uint8_t wht = expand4[nib & ~(dir & 0xF)];         // 흰색 방향 픽셀
        pb0[g] = blk; pb0[g + EPD_STRIDE] = blk;
        pw0[g] = wht; pw0[g + EPD_STRIDE] = wht;
        continue;
      }

      uint8_t v   = p_sh[g];
      uint8_t bv  = p_bs[g];
      uint8_t act = expand4[nib];
      uint8_t t0  = dither_lut[0][v];       // 1 = 흰색 목표
      uint8_t t1  = dither_lut[1][v];
      uint8_t d0  = act & (t0 ^ dither_lut[0][bv]);   // 패턴이 바뀐 점만 (블록의 나머지 점은 과구동 방지)
      uint8_t d1  = act & (t1 ^ dither_lut[1][bv]);

      pw0[g]               = t0 & d0;  pb0[g]               = ~t0 & d0;
      pw0[g + EPD_STRIDE]  = t1 & d1;  pb0[g + EPD_STRIDE]  = ~t1 & d1;
    }
  }
  return pending;
}

// 구동한 블록의 카운터를 1 줄인다 (비트 슬라이스 감소). 반환 = 아직 남은 GB 픽셀 수
static uint32_t inkDecrement(void)
{
  uint32_t remain = 0;

  for (int w = 0; w < INK_WORDS; w++)
  {
    uint32_t before = ink_cnt[0][w] | ink_cnt[1][w] | ink_cnt[2][w] | ink_cnt[3][w];
    uint32_t borrow = before;

    if (before == 0) continue;
    for (int k = 0; k < INK_CNT_BITS && borrow; k++)
    {
      uint32_t old = ink_cnt[k][w];
      ink_cnt[k][w] = old ^ borrow;
      borrow &= ~old;
    }
    uint32_t after = ink_cnt[0][w] | ink_cnt[1][w] | ink_cnt[2][w] | ink_cnt[3][w];
    remain += __builtin_popcount(after);

    // 카운터가 0 이 된 픽셀의 묶음은 마스크에서 꺼야 한다
    if (before & ~after) inkUpdateWord(inkWhiteBuf(), w, before & ~after);
  }
  return remain;
}

static void convertFrame(void)
{
  const uint8_t *p_src = gbGetLcdBuf();
  uint8_t       *p_fb  = canvasGetBuffer();

  for (int y = 0; y < GB_LCD_HEIGHT; y++)
  {
    uint8_t *p_dst0 = &p_fb[(GB_SCR_Y + y * 2) * CANVAS_STRIDE + GB_SCR_X / 8];
    uint8_t *p_dst1 = p_dst0 + CANVAS_STRIDE;

    for (int x = 0; x < GB_LCD_WIDTH / 4; x++)
    {
      uint8_t v = *p_src++;
      p_dst0[x] = dither_lut[0][v];
      p_dst1[x] = dither_lut[1][v];
    }
  }
}

/*
 * 입력 매핑 (Phase 5)
 *
 *   Game Boy   | Xbox 패드               | 보드 버튼
 *   -----------+------------------------+-----------
 *   방향       | D-pad, 왼쪽 스틱          | -
 *   A          | A, Y                   | BTN3
 *   B          | B, X                   | BTN4 (짧게)
 *   Select     | View                   | BTN2
 *   Start      | Menu                   | BTN1
 *   메뉴/일시정지 | Xbox 버튼               | BTN4 1초 이상
 *
 *   p_menu : 메뉴 버튼이 새로 눌렸으면 true (엣지)
 */
static uint8_t readInput(bool *p_menu)
{
  static bool     pre_menu = false;
  static uint32_t btn4_time = 0;            // BTN4 눌리기 시작한 시각 (0 = 안 눌림)
  xbox_hid_report_t pad;
  uint8_t           joy  = 0;
  bool              menu = false;

  if (buttonGetPressed(_DEF_CH1)) joy |= GB_JOY_START;
  if (buttonGetPressed(_DEF_CH2)) joy |= GB_JOY_SELECT;
  if (buttonGetPressed(_DEF_CH3)) joy |= GB_JOY_A;
  if (buttonGetPressed(_DEF_CH4))
  {
    if (btn4_time == 0) btn4_time = millis() | 1;
    if (millis() - btn4_time >= MENU_LONG_PRESS_MS) menu = true;
    else                                            joy |= GB_JOY_B;
  }
  else
  {
    btn4_time = 0;
  }

  if (blePadIsConnected() && blePadGetInfoLatched(BLE_PAD_LATCH_GAME, &pad))
  {
    if (pad.btn_xbox) menu = true;

    // D-pad hat: 1 북, 2 북동, 3 동, 4 남동, 5 남, 6 남서, 7 서, 8 북서
    switch (pad.dpad)
    {
      case 1: joy |= GB_JOY_UP;                    break;
      case 2: joy |= GB_JOY_UP   | GB_JOY_RIGHT;   break;
      case 3: joy |= GB_JOY_RIGHT;                 break;
      case 4: joy |= GB_JOY_DOWN | GB_JOY_RIGHT;   break;
      case 5: joy |= GB_JOY_DOWN;                  break;
      case 6: joy |= GB_JOY_DOWN | GB_JOY_LEFT;    break;
      case 7: joy |= GB_JOY_LEFT;                  break;
      case 8: joy |= GB_JOY_UP   | GB_JOY_LEFT;    break;
      default: break;
    }
    if (pad.left_stick_x < 16384) joy |= GB_JOY_LEFT;
    if (pad.left_stick_x > 49152) joy |= GB_JOY_RIGHT;
    if (pad.left_stick_y < 16384) joy |= GB_JOY_UP;
    if (pad.left_stick_y > 49152) joy |= GB_JOY_DOWN;

    if (pad.btn_a)    joy |= GB_JOY_A;
    if (pad.btn_b)    joy |= GB_JOY_B;
    if (pad.btn_x)    joy |= GB_JOY_B;
    if (pad.btn_y)    joy |= GB_JOY_A;
    if (pad.btn_view) joy |= GB_JOY_SELECT;
    if (pad.btn_menu) joy |= GB_JOY_START;
  }

  *p_menu  = menu && !pre_menu;
  pre_menu = menu;
  return joy;
}





/*
 * 사운드 출력: 1 프레임 분량을 믹서로. 믹서가 가득 차면 버린다.
 *   (동적 레이트 조절(실제 경과 시간만큼 늘리기)을 시험했으나 비율이 프레임마다 흔들려
 *    음정이 출렁여 제외. 에뮬이 원래 속도보다 느리면 소리가 끊길 수 있다)
 */
static void writeAudio(void)
{
  uint32_t n = gbCoreAudioGetFrame(audio_buf);

  if (i2sAvailableForWrite(GB_AUDIO_CH) >= n)
    i2sWrite(GB_AUDIO_CH, audio_buf, n);
  else
    stat.audio_drop++;
}

bool init(void)
{
  makeDitherLut();
  for (int n = 0; n < 16; n++)
  {
    uint8_t out = 0;
    for (int i = 0; i < 4; i++) if (n & (1 << i)) out |= 0xC0 >> (i * 2);
    expand4[n] = out;
  }
  for (int d = 0; d < 256; d++)
  {
    uint8_t nib = 0;
    for (int i = 0; i < 4; i++) if (d & (0xC0 >> (i * 2))) nib |= 1 << i;
    diff_nib[d] = nib;
  }

  k_thread_create(&emu_thread, emu_stack, K_THREAD_STACK_SIZEOF(emu_stack),
                  emuThread, NULL, NULL, NULL, EMU_THREAD_PRI, 0, K_NO_WAIT);
  k_thread_name_set(&emu_thread, "gb_emu");
  k_thread_create(&disp_thread, disp_stack, K_THREAD_STACK_SIZEOF(disp_stack),
                  dispThread, NULL, NULL, NULL, DISP_THREAD_PRI, 0, K_NO_WAIT);
  k_thread_name_set(&disp_thread, "gb_disp");

  logPrintf("[OK] gbRunInit()\n");
  return true;
}

bool gbRunStart(void)
{
  return gbRunStartEx(true);
}

// clear_ram = false: 미리 불러온 세이브(cart RAM)를 유지하고 시작
bool gbRunStartEx(bool clear_ram)
{
  gbRunStop();

  if (!gbLoadEx(clear_ram)) return false;

  i2sSetSampleRate(GB_AUDIO_RATE);
  epdSetLutOption(ink_fr, false);

  memset(&stat, 0, sizeof(stat));
  is_pause    = false;
  menu_req    = false;
  frame_ready = false;
  disp_ready  = true;
  req_full    = true;
  is_run      = true;
  return true;
}

void gbRunStop(void)
{
  if (!is_run) return;

  is_run = false;
  k_sleep(K_MSEC(50));
  // 디스플레이 갱신이 끝날 때까지 대기 후 아날로그 전원 끄기
  for (int i = 0; i < 100 && !disp_ready; i++) k_sleep(K_MSEC(10));
  epdPowerOff();
  i2sSetSampleRate(SYS_AUDIO_RATE);
}

bool gbRunIsRunning(void)
{
  return is_run;
}

void gbRunSetPause(bool pause)
{
  if (!is_run || is_pause == pause) return;

  is_pause = pause;
}

// 디스플레이 스레드가 진행 중인 갱신을 끝낼 때까지 대기 (런처가 캔버스/EPD 를 쓰기 전에 호출)
void gbRunWaitDisplayIdle(void)
{
  for (int i = 0; i < 100; i++)
  {
    if (!disp_busy && (disp_mode == DISP_MODE_INK || (disp_ready && !frame_ready))) break;
    k_sleep(K_MSEC(10));
  }
  // ink 모드는 캔버스를 마스크 버퍼로 쓰므로, 런처가 그 위에 메뉴를 그리기 전에 화면 이미지를 복원
  if (disp_mode == DISP_MODE_INK)
  {
    drawGameCanvas(&ink_shade[0][0]);
  }
}

void gbRunSetGray(bool enable)
{
  ink_gray = enable;
  req_full = true;
}

bool gbRunGetGray(void)
{
  return ink_gray;
}

void gbRunSetGrayDose(uint8_t from, uint8_t to, uint8_t dose)
{
  if (from < 4 && to < 4 && from != to) gray_dose[from][to] = MIN(dose, 15);
}

uint8_t gbRunGetGrayDose(uint8_t from, uint8_t to)
{
  return (from < 4 && to < 4) ? gray_dose[from][to] : 0;
}

void gbRunSetGrayFrames(uint8_t frames)
{
  gray_frames = constrain(frames, 1, 63);
}

uint8_t gbRunGetGrayFrames(void)
{
  return gray_frames;
}

void gbRunSetDispMode(uint8_t mode)
{
  disp_mode = mode ? DISP_MODE_INK : DISP_MODE_FRAME;
  req_full  = true;
}

uint8_t gbRunGetDispMode(void)
{
  return disp_mode;
}

void gbRunSetInk(uint8_t budget, uint8_t frames, uint32_t frame_ms)
{
  ink_budget   = constrain(budget, 1, 15);
  ink_frames   = constrain(frames, 1, 63);
  ink_frame_ms = frame_ms;
}

void gbRunGetInk(uint8_t *p_budget, uint8_t *p_frames, uint32_t *p_frame_ms)
{
  *p_budget   = ink_budget;
  *p_frames   = ink_frames;
  *p_frame_ms = ink_frame_ms;
}

// 게임 화면 영역 (런처가 게임 메뉴를 그 위에 그릴 때 사용)
void gbRunGetScreenRect(int16_t *p_x, int16_t *p_y, int16_t *p_w, int16_t *p_h)
{
  *p_x = GB_SCR_X;
  *p_y = GB_SCR_Y;
  *p_w = GB_SCR_W;
  *p_h = GB_SCR_H;
}

void gbRunSetRetarget(uint16_t rev_pct, bool same_add)
{
  ink_rev_pct  = rev_pct;
  ink_same_add = same_add;
}

void gbRunGetRetarget(uint16_t *p_rev_pct, bool *p_same_add)
{
  *p_rev_pct  = ink_rev_pct;
  *p_same_add = ink_same_add;
}

void gbRunSetQuiet(uint32_t ms, uint32_t px, uint32_t act)
{
  ink_quiet_ms  = ms;
  ink_quiet_px  = px;
  ink_quiet_act = act;
}

void gbRunGetQuiet(uint32_t *p_ms, uint32_t *p_px, uint32_t *p_act)
{
  *p_ms  = ink_quiet_ms;
  *p_px  = ink_quiet_px;
  *p_act = ink_quiet_act;
}

void gbRunSetScenePx(uint32_t px)
{
  ink_scene_px = px;
}

uint32_t gbRunGetScenePx(void)
{
  return ink_scene_px;
}

bool gbRunIsPaused(void)
{
  return is_pause;
}

// 메뉴 버튼이 눌렸는지 (읽으면 지워짐). 런처가 게임 메뉴를 띄울 때 사용
bool gbRunGetMenuRequest(void)
{
  bool ret = menu_req;
  menu_req = false;
  return ret;
}

void gbRunInjectKey(uint8_t keys, uint32_t ms)
{
  key_inject_end = millis() + ms;
  key_inject     = keys;
}

void gbRunSetWinFull(bool full)
{
  win_full = full;
}

void gbRunSetSound(bool enable)
{
  sound_enable = enable;
}

bool gbRunGetSound(void)
{
  return sound_enable;
}

void gbRunRequestFull(void)
{
  req_full = true;
}

void gbRunGetStat(gb_run_stat_t *p_stat)
{
  *p_stat = stat;
}

// 스레드별 누적 실행 사이클 (CONFIG_THREAD_RUNTIME_STATS)
void gbRunGetThreadCycles(uint64_t *p_emu, uint64_t *p_disp)
{
  k_thread_runtime_stats_t rt;

  k_thread_runtime_stats_get(&emu_thread, &rt);
  *p_emu = rt.execution_cycles;
  k_thread_runtime_stats_get(&disp_thread, &rt);
  *p_disp = rt.execution_cycles;
}




void emuThread(void *arg1, void *arg2, void *arg3)
{
  uint32_t next_us = 0;

  while (1)
  {
    if (!is_run)
    {
      xbox_hid_report_t dummy;
      blePadGetInfoLatched(BLE_PAD_LATCH_GAME, &dummy);   // 런처에서 누른 버튼이 게임 시작 때 들어가지 않도록 비움
      k_sleep(K_MSEC(20));
      next_us = micros();
      continue;
    }

    bool    menu;
    uint8_t joy = readInput(&menu);

    if (menu && !is_pause)
    {
      // 메뉴 버튼: 일시정지하고 런처에 메뉴 요청 (런처가 게임 메뉴를 그린다)
      is_pause = true;
      menu_req = true;
    }
    if (is_pause)
    {
      k_sleep(K_MSEC(20));
      next_us = micros();
      continue;
    }

    // 렌더링 여부
    //   frame 모드: 디스플레이가 기다리고 있고 이전 프레임을 다 가져갔을 때
    //   ink 모드  : 일정 간격(ink_frame_ms)마다 에뮬레이터가 스스로 그려 둔다. 디스플레이는 스캔을
    //               시작할 때 가장 최근 프레임을 가져간다 (요청을 스캔 끝에 하면 2 스캔에 1 프레임밖에 못 받음)
    static uint32_t draw_ms = 0;
    bool draw;
    if (disp_mode == DISP_MODE_INK && !req_full)
      draw = !frame_ready && (millis() - draw_ms >= ink_frame_ms);
    else
      draw = disp_ready && !frame_ready;
    if (draw) draw_ms = millis();

    if (key_inject && (int32_t)(millis() - key_inject_end) < 0) joy |= key_inject;
    else key_inject = 0;

    gbCoreSetJoypad(joy);
    gbCoreSetDrawNext(draw);

    uint32_t pre_us = micros();
    gbCoreRunFrame();
    uint32_t exe_us = micros() - pre_us;

    stat.emu_frames++;
    stat.emu_us_sum += exe_us;

    if (sound_enable)
    {
      uint32_t pre_audio = micros();
      writeAudio();
      stat.audio_us_sum += micros() - pre_audio;
    }

    if (draw)
    {
      disp_ready  = false;
      frame_ready = true;
      k_sem_give(&frame_sem);
    }

    // 59.73Hz 페이싱. 늦으면 쉬지 않고 따라잡는다 (단, 너무 밀리면 기준 재설정)
    next_us += GB_FRAME_US;
    int32_t remain = (int32_t)(next_us - micros());
    if (remain > 0)
    {
      k_usleep(remain);
    }
    else
    {
      if (remain < -100000) next_us = micros();
      k_yield();
    }
  }
}

static void dispFrameMode(void)
{
  epd_time_t t;

  k_sem_take(&frame_sem, K_FOREVER);
  if (!is_run || is_pause) return;

  disp_busy = true;
  if (req_full)
  {
    // 처음 또는 잔상 정리: 화면 전체를 흰 배경 + 게임 영역 테두리로 전체 갱신
    drawGameCanvas(gbGetLcdBuf());
    frame_ready = false;
    req_full    = false;
    epdUpdate(EPD_MODE_FULL);
  }
  else
  {
    convertFrame();
    frame_ready = false;                    // lcd_buf 반환 (에뮬은 다음 렌더링 가능)
    // 전체 폭(400px)으로 보내면 RAM 이 연속이라 SPI DMA 를 큰 덩어리로 보낸다
    // (320px 창은 줄마다 288 번 x 2 전송이 생겨 CPU 를 많이 씀)
    if (win_full)
      epdUpdateWindow(EPD_MODE_FAST, 0, GB_SCR_Y, EPD_WIDTH, GB_SCR_H);
    else
      epdUpdateWindow(EPD_MODE_FAST, GB_SCR_X, GB_SCR_Y, GB_SCR_W, GB_SCR_H);
  }
  disp_busy = false;

  epdGetTime(&t);
  stat.disp_frames++;
  stat.disp_ms_sum += t.total_ms;
  disp_ready = true;
}

static void dispInkMode(void)
{
  static uint32_t take_ms = 0;
  static uint32_t remain  = 0;              // 아직 구동할 GB 픽셀 수 (0 이면 마스크를 만들 필요 없음)
  epd_time_t      t;

  if (!is_run || is_pause)
  {
    k_sem_take(&frame_sem, K_MSEC(50));
    return;
  }

  if (req_full)
  {
    if (!frame_ready)
    {
      disp_ready = true;
      k_sem_take(&frame_sem, K_MSEC(50));
      return;
    }
    // 시작/재개: 전체 갱신으로 기준 화면을 만들고 카운터 초기화
    disp_busy = true;
    memset(ink_cnt, 0, sizeof(ink_cnt));
    memset(ink_dir, 0, sizeof(ink_dir));
    if (ink_gray)
    {
      // 회색 모드: 흰 화면(+테두리)을 기준으로 시작해 모든 픽셀을 흰색에서 목표 회색까지 구동
      memset(ink_shade, 0, sizeof(ink_shade));
      drawGameCanvas(&ink_shade[0][0]);
      epdUpdateCold(10);                      // 저온 웨이브폼: 0xFF 보다 흰색이 깨끗함 (약 1.25초)
      inkPlan(gbGetLcdBuf());
    }
    else
    {
      drawGameCanvas(gbGetLcdBuf());
      memcpy(ink_shade, gbGetLcdBuf(), sizeof(ink_shade));
      memcpy(ink_base, ink_shade, sizeof(ink_base));
      epdUpdateCold(10);
    }
    // 마스크 전체 재생성 (캔버스를 이미지로 쓴 뒤라 다시 만든다), 전 영역 전송 필요
    inkBuildMask(inkWhiteBuf());
    ink_dirty_min = 0;
    ink_dirty_max = GB_SCR_H - 1;
    frame_ready = false;
    req_full    = false;
    disp_busy   = false;
    take_ms     = millis();
    remain      = ink_gray ? 1 : 0;
    disp_ready  = true;
    return;
  }

  // 장면 전환(타이틀 → 게임, 스테이지 시작 등): 바뀐 픽셀이 많으면 전체 갱신으로 흰 바탕을 다시 만든다.
  //   ink 모드는 바뀐 픽셀만 한 번 구동해서, 대비가 떨어진 패널에서는 이전 장면이 연하게 남고
  //   그 자리를 다시 구동할 일이 없었다 (캐릭터가 지나간 자리만 깨끗해짐)
  //
  // 조용한 순간 정리: 스크롤 중 얇은 선(산 윤곽, 구름)은 흰→검이 끝나기 전에 되돌아와 연한 흔적이
  //   남는다. 변화가 적은 상태(프레임당 ink_quiet_px 미만)가 ink_quiet_ms 이어지고, 직전 정리 뒤
  //   바뀐 픽셀이 ink_quiet_act 이상 쌓였으면 전체 갱신 1회로 흔적을 지운다 (가만히 있을 때 반복 안 함)
  if (frame_ready && (ink_scene_px > 0 || ink_quiet_ms > 0))
  {
    uint32_t changed = inkChangedPixels(gbGetLcdBuf());

    if (ink_scene_px > 0 && changed >= ink_scene_px && millis() - scene_ms >= 1000)
    {
      stat.ink_scene++;
      scene_ms  = millis();
      quiet_act = 0;
      req_full  = true;
      return;                               // 다음 호출에서 req_full 처리 (프레임은 그대로 사용)
    }

    quiet_act += changed;
    if (changed >= ink_quiet_px) quiet_ms = millis();
    if (ink_quiet_ms > 0 && quiet_act >= ink_quiet_act && millis() - quiet_ms >= ink_quiet_ms)
    {
      stat.ink_quiet++;
      scene_ms  = millis();
      quiet_act = 0;
      req_full  = true;
      return;
    }
  }

  // 새 프레임이 있으면 목표 갱신
  bool planned = false;
  if (frame_ready)
  {
    uint32_t t0 = micros();
    inkPlan(gbGetLcdBuf());
    stat.ink_plan_us += micros() - t0;
    frame_ready = false;
    take_ms     = millis();
    planned     = true;
    stat.disp_frames++;
  }
  // 다음 프레임 요청 (너무 자주 받으면 렌더링 비용이 커서 간격을 둔다)
  if (!disp_ready && millis() - take_ms >= ink_frame_ms)
  {
    disp_ready = true;
  }

  // 남은 구동도 없고 새 프레임도 없으면 마스크를 만들지 않고 대기 (CPU 절약)
  if (!planned && remain == 0)
  {
    k_sem_take(&frame_sem, K_MSEC(20));
    return;
  }

  uint8_t *p_white = inkWhiteBuf();
  uint32_t pending = remain;

  if (planned)
  {
    // 새 계획 뒤 남은 구동 수 다시 계산
    pending = 0;
    for (int w = 0; w < INK_WORDS; w++)
      pending += __builtin_popcount(ink_cnt[0][w] | ink_cnt[1][w] | ink_cnt[2][w] | ink_cnt[3][w]);
  }
  if (pending == 0)
  {
    remain = 0;
    k_sem_take(&frame_sem, K_MSEC(20));     // 구동할 것이 없으면 새 프레임 대기
    return;
  }

  // 전송은 매 스캔 게임 영역 전체 (두 면). 바뀐 줄만 보내면 화면이 깨졌다:
  // SSD168x 는 구동(0x0C) 후 RAM 을 BW↔RED 로 복사하는 것으로 보여 (InkBoy host_fakes 주석),
  // EPD RAM 이 이전 마스크를 유지한다고 가정할 수 없다. 마스크 "생성" 만 증분으로 한다.
  int16_t y0 = 0, h = GB_SCR_H;
  ink_dirty_min = 0;
  ink_dirty_max = -1;
  stat.ink_rows += h;

  disp_busy = true;
  epdUpdateMask(ink_black + y0 * EPD_STRIDE, p_white + y0 * EPD_STRIDE, GB_SCR_Y + y0, h,
                ink_gray ? gray_frames : ink_frames);
  disp_busy = false;
  uint32_t t2 = micros();
  remain = inkDecrement();
  stat.ink_dec_us += micros() - t2;

  epdGetTime(&t);
  stat.disp_ms_sum += t.total_ms;
  stat.ink_scans++;
  stat.ink_pending += pending;
}

void dispThread(void *arg1, void *arg2, void *arg3)
{
  while (1)
  {
    if (disp_mode == DISP_MODE_INK)
      dispInkMode();
    else
      dispFrameMode();
  }
}
