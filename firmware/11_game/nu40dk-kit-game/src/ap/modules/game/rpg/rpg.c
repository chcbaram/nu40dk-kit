// 바람의 펌웨어 수련기 (RPG 프로토타입)
//
//   개발자 바람이 펌웨어 고수가 되기 위해 수련을 떠나 버그를 디버깅으로 무찌르는 이야기.
//   e-paper 에 맞춰 칸 단위 이동(한 걸음 = FAST 부분 갱신), 방 단위 화면 넘김(전체 갱신으로 잔상 정리),
//   턴제 전투로 만든다. 펌웨어 기능은 런처가 넘겨준 app_api_t 만 쓴다.
//
//   에셋: tools/rpg/gen_assets.py → rpg_assets.h

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "game/app_api.h"
#include "rpg.h"
#include "rpg_assets.h"


#define TILE          32                        // 16x16 그림을 2배로
#define MAP_X0        8                         // 12 칸 x 32 = 384, 양옆 8 px
#define MAP_PX_H      (RPG_MAP_H * TILE)        // 256
#define STATUS_Y      MAP_PX_H                  // 아래 44 px 상태 줄
#define CLEAN_AFTER   120                       // FAST 갱신이 이만큼 쌓이면 멈췄을 때 전체 갱신
#define IDLE_CLEAN_MS 5000
#define IDLE_OFF_MS   60000                     // 고전압 끄기 (3초였을 때 대사를 읽다 넘기면 켠 직후 첫 갱신이 약해 앞 글자가 덜 지워졌다)
#define HOLD_WALK_MS  250                       // 이만큼 누르고 있어야 이어 걷는다 (톡 누르면 한 칸)
#define WALK_STEPS    1                         // 한 칸에 한 번 그린다 (반 칸 그림을 넣으면 한 번 눌러 두 번 움직이는 것처럼 보였다)

enum { DIR_DOWN = 0, DIR_UP, DIR_LEFT, DIR_RIGHT };

typedef struct
{
  bool     used;            // 진행 중인 게임이 있음 (이어하기)
  uint8_t  room, x, y, dir, step;
  int8_t   ox, oy;
  int16_t  hp, hp_max;
  uint8_t  lv;
  uint8_t  skills;          // 배운 기술 비트 (0 번 printf 는 처음부터)
  uint8_t  items[RPG_ITEM_CNT];
  uint8_t  equips;          // 가진 장비 비트 (가장 좋은 것을 든다)
  uint32_t flags;           // 진행 깃발 (story.py FLAGS)
  uint32_t dead;            // 쓰러뜨린 버그 엔티티 비트
  uint8_t  ckpt;            // 쓰러지면 돌아갈 방
  bool     duck;            // 고무오리: 다음 공격 2 배
  bool     ended;           // 엔딩을 봤다
} game_t;

static const app_api_t *api;
static game_t           g;
static uint32_t         fast_cnt;
static uint32_t         last_ms;
static bool             epd_on;
static uint32_t         rnd = 12345;


// 대사 음성: 페이지 글자(줄을 '\n' 으로 이음)의 CRC32 → /rpg/v/XXXXXXXX.WAV (tools/rpg/gen_voice.py)
static uint32_t crc32Add(uint32_t crc, const char *s)
{
  while (*s)
  {
    crc ^= (uint8_t)*s++;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
  }
  return crc;
}

static void voicePage(const char *const *lines, int n)
{
  uint32_t crc = 0xFFFFFFFFu;
  char     path[32];

  for (int k = 0; k < n; k++)
  {
    if (k > 0) crc = crc32Add(crc, "\n");
    crc = crc32Add(crc, lines[k]);
  }
  snprintf(path, sizeof(path), "/rpg/v/%08X.WAV", (unsigned)(crc ^ 0xFFFFFFFFu));
  api->voicePlay(path);
}

static uint32_t rand32(void)
{
  rnd = rnd * 1103515245u + 12345u;
  return rnd >> 8;
}

static int randRange(int lo, int hi)
{
  return lo + (int)(rand32() % (uint32_t)(hi - lo + 1));
}

/*---------------------------------------------------------------------------
 * 화면 갱신
 *-------------------------------------------------------------------------*/
static void showRect(app_refresh_t mode, int16_t x, int16_t y, int16_t w, int16_t h)
{
  api->refreshRect(mode, x, y, w, h);
  fast_cnt++;
  last_ms = api->millis();
  epd_on  = true;
}

// 덜 구동된 픽셀 지도: 걷기(짧은 펄스)로 바뀐 픽셀을 표시해 두었다가, 멈추거나 장면이 바뀌기 전에
// 그 픽셀만 지금 목표 색으로 정상 길이 펄스를 준다. 지나갔다가 원래 그림으로 돌아온 픽셀(캐릭터가 지나간
// 풀밭)은 "이전 = 새 그림"이라 차분 갱신으로는 다시 구동되지 않아 옅은 캐릭터로 남았다.
//   지도 영역 0 ~ MAP_PX_H 줄, 비트 1 = 덜 구동. 버퍼는 api->scratch.
static void    drawMap(void);
static bool    dirty_any = false;
static uint8_t rect_prev[512];                // 걷기 갱신 영역 (최대 64x64 → 8 x 64 바이트)

static uint8_t *dirtyMap(void)
{
  uint32_t need = (uint32_t)MAP_PX_H * api->stride;
  return (api->scratch != NULL && api->scratch_size >= need) ? api->scratch : NULL;
}

static void dirtyClear(void)
{
  uint8_t *d = dirtyMap();
  if (d != NULL) memset(d, 0, (uint32_t)MAP_PX_H * api->stride);
  dirty_any = false;
}

// 짧은 펄스로 한 장면: 그리기 전 영역을 기억했다가 바뀐 픽셀을 지도에 표시하고 영역만 갱신
static void quickFrame(int16_t x, int16_t y, int16_t w, int16_t h)
{
  uint8_t *d  = dirtyMap();
  int16_t  bx = x >> 3;
  int16_t  bw = w >> 3;

  if (d != NULL && bw * h <= (int)sizeof(rect_prev) && y + h <= MAP_PX_H)
  {
    for (int r = 0; r < h; r++) memcpy(&rect_prev[r * bw], &api->fb[(y + r) * api->stride + bx], bw);
    drawMap();
    for (int r = 0; r < h; r++)
    {
      uint8_t *pd = &d[(y + r) * api->stride + bx];
      uint8_t *pf = &api->fb[(y + r) * api->stride + bx];
      for (int c = 0; c < bw; c++) pd[c] |= rect_prev[r * bw + c] ^ pf[c];
    }
    dirty_any = true;
  }
  else
  {
    drawMap();
  }
  showRect(APP_REFRESH_QUICK, x, y, w, h);
}

// 덜 구동된 픽셀만 정상 길이 펄스로 마무리 (깜빡임 없음, 바뀌지 않은 픽셀은 건드리지 않음)
//   이전 이미지 = 지금 화면에서 표시된 픽셀만 반전 → 그 픽셀만 지금 목표 색 쪽으로 구동된다
static void finishDirty(void)
{
  uint8_t *d = dirtyMap();
  uint32_t n = (uint32_t)MAP_PX_H * api->stride;

  if (d == NULL || !dirty_any) return;
  for (uint32_t i = 0; i < n; i++) d[i] ^= api->fb[i];
  api->refreshFrom(d, 0, MAP_PX_H);
  fast_cnt++;
  last_ms = api->millis();
  epd_on  = true;
  dirtyClear();
}

static void show(app_refresh_t mode)
{
  api->refresh(mode);
  if (mode == APP_REFRESH_FAST) fast_cnt++;
  else                          fast_cnt = 0;
  last_ms = api->millis();
  epd_on  = true;
}

// 장면 전환 (대화창 닫기·방 이동·전투): 깜빡이는 전체 갱신 대신 FAST 를 두 번.
//   한 번이면 큰 영역(대화창 등)이 덜 지워져, 다음 걷기 갱신 때 이전 장면이 다시 떠올랐다 (카메라).
//   sceneSnap() 으로 새 장면을 그리기 전 화면을 기억해 두고, 같은 전환(이전 → 새)을 한 번 더 구동한다.
static bool snap_valid = false;

static void sceneSnap(void)
{
  uint8_t *d = dirtyMap();
  if (d == NULL) return;
  memcpy(d, api->fb, (uint32_t)MAP_PX_H * api->stride);   // 덜 구동 표시는 버림 (두 번 구동이 덮는다)
  dirty_any  = false;
  snap_valid = true;
}

static void transition(void)
{
  show(APP_REFRESH_FAST);
  if (snap_valid)
  {
    api->refreshFrom(dirtyMap(), 0, MAP_PX_H);
    fast_cnt++;
  }
  snap_valid = false;
  dirtyClear();
}

// 입력이 없을 때 할 일: 잔상 정리, 전원 끄기
static void idleWork(void)
{
  uint32_t idle = api->millis() - last_ms;

  if (fast_cnt >= CLEAN_AFTER && idle >= IDLE_CLEAN_MS)
  {
    show(APP_REFRESH_CLEAN);
    return;
  }
  if (epd_on && idle >= IDLE_OFF_MS)
  {
    api->powerOff();
    epd_on = false;
  }
}

static app_key_t waitKey(void)
{
  app_key_t key;

  while ((key = api->readKey()) == APP_KEY_NONE)
  {
    idleWork();
    api->delay(20);
  }
  return key;
}

static void waitOk(void)
{
  app_key_t key;
  do { key = waitKey(); } while (key != APP_KEY_OK && key != APP_KEY_BACK);
}

/*---------------------------------------------------------------------------
 * 그리기
 *-------------------------------------------------------------------------*/
static inline void putWord(int16_t x, int16_t y, uint16_t ink, uint16_t mask)
{
  // x 는 8 의 배수. ink 비트 1 = 검정 (버퍼는 1 = 흰색)
  uint8_t *p = &api->fb[y * api->stride + (x >> 3)];
  uint16_t cur = ((uint16_t)p[0] << 8) | p[1];

  cur  = (cur & ~mask) | (~ink & mask);
  p[0] = cur >> 8;
  p[1] = cur & 0xFF;
}

static uint16_t flip16(uint16_t v)
{
  uint16_t r = 0;
  for (int i = 0; i < 16; i++) { r = (r << 1) | (v & 1); v >>= 1; }
  return r;
}

// 8 비트를 16 비트로 (각 비트를 두 번): 2배 확대
static uint16_t dbl8(uint8_t v)
{
  uint16_t r = 0;
  for (int i = 7; i >= 0; i--) r = (r << 2) | ((v >> i) & 1 ? 3 : 0);
  return r;
}

// 16x16 그림을 32x32 로 (px 는 8 의 배수)
static void put2x(int16_t px, int16_t py, uint16_t ink, uint16_t mask)
{
  uint16_t ih = dbl8(ink >> 8),  il = dbl8(ink & 0xFF);
  uint16_t mh = dbl8(mask >> 8), ml = dbl8(mask & 0xFF);

  for (int k = 0; k < 2; k++)
  {
    putWord(px,      py + k, ih, mh);
    putWord(px + 16, py + k, il, ml);
  }
}

static void drawTileAt(int16_t px, int16_t py, uint8_t t)
{
  for (int i = 0; i < 16; i++) put2x(px, py + i * 2, rpg_tile_bits[t][i], 0xFFFF);
}

static void drawSpriteAt(int16_t px, int16_t py, uint8_t s, bool flip)
{
  for (int i = 0; i < 16; i++)
  {
    uint16_t b = rpg_spr_bits[s][i];
    uint16_t m = rpg_spr_mask[s][i];
    if (flip) { b = flip16(b); m = flip16(m); }
    put2x(px, py + i * 2, b, m);
  }
}

// 큰 스프라이트 (전투 화면): scale 배
static void drawSpriteBig(int16_t px, int16_t py, uint8_t s, int scale)
{
  for (int y = 0; y < 16; y++)
  {
    for (int x = 0; x < 16; x++)
    {
      uint16_t bit = 0x8000 >> x;
      if (!(rpg_spr_mask[s][y] & bit)) continue;
      api->fillRect(px + x * scale, py + y * scale, scale, scale,
                    (rpg_spr_bits[s][y] & bit) ? APP_BLACK : APP_WHITE);
    }
  }
}

static bool flagOn(int f)
{
  return f >= 0 && (g.flags & (1u << f)) != 0;
}

static void flagSet(int f)
{
  if (f >= 0) g.flags |= (1u << f);
}

static uint8_t tileAt(uint8_t room, int x, int y)
{
  uint8_t t = rpg_map[room][y][x];

  if (t == RPG_T_GATE && flagOn(rpg_room_gate[room])) return RPG_T_PATH;
  if (t == RPG_T_CHEST)
  {
    for (int i = 0; i < RPG_ENT_CNT; i++)
    {
      const rpg_ent_t *e = &rpg_ent[i];
      if (e->room == room && e->x == x && e->y == y && e->kind == RPG_ENT_OBJ && flagOn(e->flag))
        return RPG_T_CHEST_OPEN;
    }
  }
  if (t == RPG_T_GRASS && ((x * 7 + y * 5 + room * 3) % 4) == 0) return RPG_T_GRASS2;
  return t;
}

static bool entAlive(int i)
{
  if (rpg_ent[i].kind == RPG_ENT_BUG) return !(g.dead & (1u << i));
  return true;
}

static int entAt(uint8_t room, int x, int y)
{
  for (int i = 0; i < RPG_ENT_CNT; i++)
  {
    const rpg_ent_t *e = &rpg_ent[i];
    if (e->room == room && e->x == x && e->y == y && entAlive(i)) return i;
  }
  return -1;
}

// 가진 장비 중 가장 좋은 것 (-1 = 없음)
static int equipBest(void)
{
  for (int i = RPG_EQUIP_CNT - 1; i >= 0; i--)
    if (g.equips & (1 << i)) return i;
  return -1;
}

static void drawStatus(void)
{
  int16_t y = STATUS_Y + 14;
  int     eq = equipBest();

  api->fillRect(0, STATUS_Y, api->width, api->height - STATUS_Y, APP_BLACK);
  api->print(10, y, APP_WHITE, "%s", rpg_room_name[g.room]);
  api->print(170, y, APP_WHITE, "Lv%d HP %d/%d", g.lv, g.hp, g.hp_max);
  if (eq >= 0) api->print(300, y, APP_WHITE, "%s", rpg_equip[eq].name);
}

static void drawMap(void)
{
  api->fillRect(0, 0, MAP_X0, MAP_PX_H, APP_WHITE);
  api->fillRect(MAP_X0 + RPG_MAP_W * TILE, 0, api->width - MAP_X0 - RPG_MAP_W * TILE, MAP_PX_H, APP_WHITE);
  for (int y = 0; y < RPG_MAP_H; y++)
    for (int x = 0; x < RPG_MAP_W; x++)
      drawTileAt(MAP_X0 + x * TILE, y * TILE, tileAt(g.room, x, y));

  for (int i = 0; i < RPG_ENT_CNT; i++)
  {
    const rpg_ent_t *e = &rpg_ent[i];
    if (e->room == g.room && e->spr >= 0 && entAlive(i)) drawSpriteAt(MAP_X0 + e->x * TILE, e->y * TILE, e->spr, false);
  }

  uint8_t s;
  bool    flip = false;
  switch (g.dir)
  {
    case DIR_UP:    s = RPG_S_HERO_UP0;   break;
    case DIR_LEFT:  s = RPG_S_HERO_SIDE0; flip = true; break;
    case DIR_RIGHT: s = RPG_S_HERO_SIDE0; break;
    default:        s = RPG_S_HERO_DOWN0; break;
  }
  drawSpriteAt(MAP_X0 + g.x * TILE + g.ox, g.y * TILE + g.oy, s + (g.step & 1), flip);
  drawStatus();
}

// 초상화 32x32 를 2배로 (px 는 8 의 배수)
static void drawPortrait(int16_t px, int16_t py, int p)
{
  for (int i = 0; i < 32; i++)
  {
    uint32_t v = rpg_portrait[p][i];
    for (int k = 0; k < 2; k++)
    {
      putWord(px,      py + i * 2 + k, dbl8(v >> 24), 0xFFFF);
      putWord(px + 16, py + i * 2 + k, dbl8((v >> 16) & 0xFF), 0xFFFF);
      putWord(px + 32, py + i * 2 + k, dbl8((v >> 8) & 0xFF), 0xFFFF);
      putWord(px + 48, py + i * 2 + k, dbl8(v & 0xFF), 0xFFFF);
    }
  }
}

// 대화창 (대화 한 번 = 창 한 번)
//   NPC 와 이야기하는 동안 같은 크기의 창을 한 번만 열고, 쪽 넘김·화자 바뀜·문답·대답은 모두
//   같은 창 안에서 "비우기 → 그리기" 로 바꾼다. 대화가 끝나면 짧은 깜빡임 갱신으로 지도로 돌아간다.
//   (예전에는 덩어리마다 창을 닫고 크기가 다른 창을 다시 열어, 지울 때 남은 흔적이 쌓여 겹쳐 보였다)
#define BOX_X   8
#define BOX_Y   132
#define BOX_W   384
#define BOX_H   120

static bool conv_open = false;

// 지도 + 빈 창 (화자·글자 없음)
static void drawConvFrame(void)
{
  drawMap();
  api->fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, APP_WHITE);
  api->drawRect(BOX_X, BOX_Y, BOX_W, BOX_H, APP_BLACK);
  api->drawRect(BOX_X + 2, BOX_Y + 2, BOX_W - 4, BOX_H - 4, APP_BLACK);
}

// 초상화 (창 왼쪽 위에 걸침) 와 이름표 (창 위 테두리 탭)
static void drawSpeaker(const char *who, int portrait)
{
  int16_t nx = BOX_X;

  if (portrait >= 0)
  {
    const int16_t px = BOX_X + 4, py = BOX_Y - 64 - 2;
    api->fillRect(px - 4, py - 4, 72, 72, APP_WHITE);
    api->drawRect(px - 4, py - 4, 72, 72, APP_BLACK);
    api->drawRect(px - 2, py - 2, 68, 68, APP_BLACK);
    drawPortrait(px, py, portrait);
    nx = px - 4 + 72 - 2;
  }
  if (who != NULL)
  {
    int16_t w = api->strWidth(who) + 16;
    api->fillRect(nx, BOX_Y - 20, w, 22, APP_BLACK);
    api->print(nx + 8, BOX_Y - 18, APP_WHITE, "%s", who);
  }
}

// 창 열기: 지도 → 빈 창을 두 번 구동. 이미 열려 있으면 빈 창으로 비우기만 (이전 글자·화자 지우기)
static void convClear(void)
{
  if (!conv_open)
  {
    finishDirty();              // 걸은 자리부터 마무리
    drawMap();
    sceneSnap();
    drawConvFrame();
    transition();
    conv_open = true;
    return;
  }
  drawConvFrame();
  show(APP_REFRESH_FAST);
}

// 창 닫기: 짧은 깜빡임 갱신으로 지도
static void convClose(void)
{
  if (!conv_open) return;
  api->voiceStop();
  dirtyClear();
  drawMap();
  show(APP_REFRESH_FLASH);
  conv_open = false;
}

// 대화 한 덩어리. lines 는 NULL 로 끝나고, "" 는 쪽 넘김 (한 쪽 최대 3 줄). portrait >= 0 이면 초상화
static void dialogP(const char *who, int portrait, const char *const *lines)
{
  int i = 0;

  while (lines[i] != NULL)
  {
    convClear();
    drawConvFrame();
    drawSpeaker(who, portrait);

    int16_t y = BOX_Y + 14;
    int     page = i;
    for (int n = 0; lines[i] != NULL && lines[i][0] != 0 && n < 3; n++, i++)
    {
      api->print(BOX_X + 14, y, APP_BLACK, "%s", lines[i]);
      y += 22;
    }
    if (lines[i] != NULL && lines[i][0] == 0) i++;
    api->print(BOX_X + BOX_W - 20, BOX_Y + BOX_H - 22, APP_BLACK, ">");
    voicePage(&lines[page], i - page);
    show(APP_REFRESH_FAST);
    waitOk();
    api->voiceStop();
  }
}

/*---------------------------------------------------------------------------
 * 전투
 *   명령 3x3: 기술 7 개 (배운 것만), 가방, 도망. 장비 공격력이 더해지고, 약점 기술은 2 배,
 *   단단한 적(수련용·하드폴트)은 약점이 아니면 1/3. 고무오리는 다음 공격 2 배.
 *-------------------------------------------------------------------------*/
#define CMD_BAG   7
#define CMD_RUN   8
#define CMD_CNT   9

static void drawHpBar(int16_t x, int16_t y, int16_t w, int16_t hp, int16_t max)
{
  api->drawRect(x, y, w, 10, APP_BLACK);
  api->fillRect(x + 1, y + 1, w - 2, 8, APP_WHITE);
  if (hp > 0) api->fillRect(x + 2, y + 2, (w - 4) * hp / max, 6, APP_BLACK);
}

// 전투 그림 32x32 를 scale 배로
static void drawBat(int16_t px, int16_t py, int b, int scale)
{
  for (int y = 0; y < 32; y++)
  {
    uint32_t m = rpg_bat_mask[b][y];
    uint32_t k = rpg_bat_bits[b][y];
    for (int x = 0; x < 32; x++)
    {
      uint32_t bit = 0x80000000u >> x;
      if (!(m & bit)) continue;
      api->fillRect(px + x * scale, py + y * scale, scale, scale, (k & bit) ? APP_BLACK : APP_WHITE);
    }
  }
}

static const char *cmdName(int i)
{
  if (i < RPG_SKILL_CNT) return (g.skills & (1 << i)) ? rpg_skill[i].name : "???";
  return i == CMD_BAG ? "가방" : "도망";
}

// bag >= 0 이면 명령 대신 가방 (아이템 3 + 뒤로) 을 보인다
static void drawBattle(const rpg_enemy_t *en, int16_t en_hp, int cur, int bag, const char *m1, const char *m2)
{
  char buf[40];
  int  eq = equipBest();

  api->clear(APP_WHITE);

  // 적
  api->print(16, 8, APP_BLACK, "%s", en->name);
  drawHpBar(16, 30, 160, en_hp, en->hp);
  api->print(184, 26, APP_BLACK, "%d/%d", en_hp > 0 ? en_hp : 0, en->hp);
  drawBat(40, 50, en->bat, 3);

  // 바람
  api->drawRect(224, 88, 168, 64, APP_BLACK);
  drawSpriteAt(232, 100, RPG_S_HERO_SIDE0, true);
  api->print(272, 94, APP_BLACK, "바람 Lv%d", g.lv);
  drawHpBar(272, 116, 80, g.hp, g.hp_max);
  api->print(356, 112, APP_BLACK, "%d", g.hp > 0 ? g.hp : 0);
  if (eq >= 0) api->print(272, 130, APP_BLACK, "%s", rpg_equip[eq].name);
  if (g.duck) api->print(352, 130, APP_BLACK, "오리");

  // 메시지
  api->drawRect(8, 158, 384, 46, APP_BLACK);
  if (m1) api->print(16, 162, APP_BLACK, "%s", m1);
  if (m2) api->print(16, 182, APP_BLACK, "%s", m2);

  // 명령 3x3 또는 가방
  api->fillRect(0, 210, api->width, api->height - 210, APP_BLACK);
  int cnt = bag >= 0 ? RPG_ITEM_CNT + 1 : (bag == -2 ? 0 : CMD_CNT);   // -2 = 빈 칸 (명령 ↔ 가방 바꿀 때)
  int sel = bag >= 0 ? bag : cur;
  for (int i = 0; i < cnt; i++)
  {
    int16_t x = 8 + (i % 3) * 128;
    int16_t y = 216 + (i / 3) * 28;
    const char *name;
    if (bag >= 0)
    {
      if (i < RPG_ITEM_CNT) { snprintf(buf, sizeof(buf), "%s %d", rpg_item[i].name, g.items[i]); name = buf; }
      else                  name = "뒤로";
    }
    else
    {
      name = cmdName(i);
    }
    if (i == sel)
    {
      api->fillRect(x, y - 2, 124, 22, APP_WHITE);
      api->print(x + 4, y + 1, APP_BLACK, ">%s", name);
    }
    else
    {
      api->print(x + 4, y + 1, APP_WHITE, " %s", name);
    }
  }
}

// 전투 메시지 바꾸기: 메시지 칸을 먼저 비운 화면을 한 번 보이고 새 메시지를 그린다.
//   같은 자리에서 글자를 바로 바꾸면 이전 글자가 덜 지워져 겹쳐 보였다.
static void battleMsg(const rpg_enemy_t *en, int16_t en_hp, int cur, const char *m1, const char *m2)
{
  drawBattle(en, en_hp, cur, -1, NULL, NULL);
  show(APP_REFRESH_FAST);
  drawBattle(en, en_hp, cur, -1, m1, m2);
  show(APP_REFRESH_FAST);
}

// 3x3 칸 이동
static int gridMove(int cur, app_key_t key, int cnt)
{
  int r = cur / 3, c = cur % 3, rows = (cnt + 2) / 3;
  if (key == APP_KEY_UP)    r = (r + rows - 1) % rows;
  if (key == APP_KEY_DOWN)  r = (r + 1) % rows;
  if (key == APP_KEY_LEFT)  c = (c + 2) % 3;
  if (key == APP_KEY_RIGHT) c = (c + 1) % 3;
  int n = r * 3 + c;
  return n < cnt ? n : cur;
}

// 가방에서 고르기. 반환: 아이템 번호, -1 = 뒤로
static int bagSelect(const rpg_enemy_t *en, int16_t en_hp, int cur)
{
  int sel = 0;

  drawBattle(en, en_hp, cur, -2, "무엇을 쓸까?", NULL);   // 명령 글자를 먼저 지운다
  show(APP_REFRESH_FAST);

  while (1)
  {
    drawBattle(en, en_hp, cur, sel, "무엇을 쓸까?", NULL);
    show(APP_REFRESH_FAST);
    app_key_t key = waitKey();
    if (key == APP_KEY_BACK) return -1;
    if (key == APP_KEY_OK)
    {
      if (sel >= RPG_ITEM_CNT) return -1;
      if (g.items[sel] > 0) return sel;
      continue;
    }
    sel = gridMove(sel, key, RPG_ITEM_CNT + 1);
  }
}

static int randDmg(int skill)
{
  int d = randRange(rpg_skill[skill].lo, rpg_skill[skill].hi);
  int eq = equipBest();
  if (eq >= 0) d += rpg_equip[eq].atk;
  return d;
}

// 반환: true = 이김, false = 짐 또는 도망 (p_run 으로 구분)
static bool battle(int enemy, bool *p_run)
{
  const rpg_enemy_t *en = &rpg_enemy[enemy];
  int16_t            en_hp = en->hp;
  int                cur = 0;
  char               m1[64], m2[64];

  *p_run = false;
  api->voiceStop();
  conv_open = false;          // 대화창이 열려 있어도 전투 화면 깜빡임이 지운다
  finishDirty();
  dirtyClear();
  snprintf(m1, sizeof(m1), "%s 이(가) 나타났다!", en->name);
  drawBattle(en, en_hp, cur, -1, m1, en->desc);
  api->beep(440, 80);
  show(APP_REFRESH_FLASH);    // 지도 → 전투: 화면 전체가 바뀌므로 짧은 깜빡임으로 이전 장면을 지운다
  waitOk();

  while (1)
  {
    // 명령 고르기 (메시지는 한 번만 바꾸고, 커서 이동은 같은 메시지로 다시 그린다)
    battleMsg(en, en_hp, cur, "무엇을 할까?", NULL);
    int act = -1, item = -1;
    while (act < 0)
    {
      app_key_t key = waitKey();
      int       pre = cur;
      if (key == APP_KEY_OK)
      {
        if (cur < RPG_SKILL_CNT)
        {
          if (g.skills & (1 << cur)) act = cur;
        }
        else if (cur == CMD_BAG)
        {
          item = bagSelect(en, en_hp, cur);
          if (item >= 0) act = CMD_BAG;
          else
          {
            drawBattle(en, en_hp, cur, -2, "무엇을 할까?", NULL);   // 가방 글자를 먼저 지운다
            show(APP_REFRESH_FAST);
            drawBattle(en, en_hp, cur, -1, "무엇을 할까?", NULL);
            show(APP_REFRESH_FAST);
          }
        }
        else
        {
          act = CMD_RUN;
        }
        continue;
      }
      cur = gridMove(cur, key, CMD_CNT);
      if (cur != pre)
      {
        drawBattle(en, en_hp, cur, -1, "무엇을 할까?", NULL);
        show(APP_REFRESH_FAST);
      }
    }

    // 바람의 차례
    m2[0] = 0;
    if (act < RPG_SKILL_CNT)
    {
      int d = randDmg(act);
      bool duck = g.duck;
      g.duck = false;
      if (duck) d *= 2;
      snprintf(m1, sizeof(m1), "%s", rpg_skill[act].msg);
      if (en->weak == act)
      {
        d *= 2;
        snprintf(m2, sizeof(m2), "효과가 굉장했다! %d 데미지", d);
      }
      else if (en->tough)
      {
        d = d / 3 > 0 ? d / 3 : 1;
        snprintf(m2, sizeof(m2), "잘 듣지 않는다... %d 데미지", d);
      }
      else
      {
        snprintf(m2, sizeof(m2), "%s%d 데미지", duck ? "오리 덕분에 " : "", d);
      }
      en_hp -= d;
    }
    else if (act == CMD_BAG)
    {
      const rpg_item_t *it = &rpg_item[item];
      g.items[item]--;
      if (it->kind == RPG_ITEM_HEAL)
      {
        int h = it->val ? it->val : g.hp_max;
        g.hp = (g.hp + h > g.hp_max) ? g.hp_max : g.hp + h;
        snprintf(m1, sizeof(m1), "%s 을(를) 마셨다!", it->name);
        snprintf(m2, sizeof(m2), "HP %d 회복", h);
      }
      else
      {
        g.duck = true;
        snprintf(m1, sizeof(m1), "고무오리에게 버그를 설명했다.");
        snprintf(m2, sizeof(m2), "약점이 보인다! 다음 공격 2 배");
      }
    }
    else
    {
      if (!en->tough && randRange(0, 99) < 70)
      {
        battleMsg(en, en_hp, cur, "버그를 TODO 로 미뤄 두고 도망쳤다...", NULL);
        api->delay(600);
        *p_run = true;
        return false;
      }
      snprintf(m1, sizeof(m1), en->tough ? "이 시험에서는 도망칠 수 없다!" : "도망치지 못했다!");
    }
    api->beep(act == CMD_BAG ? 880 : 660, 40);
    battleMsg(en, en_hp, cur, m1, m2[0] ? m2 : NULL);
    api->delay(400);

    if (en_hp <= 0)
    {
      g.lv++;
      g.hp_max += 3;
      g.hp = g.hp_max;
      snprintf(m1, sizeof(m1), "%s 을(를) 고쳤다!", en->name);
      snprintf(m2, sizeof(m2), "레벨 업! Lv%d, 최대 HP %d", g.lv, g.hp_max);
      api->beep(1047, 120);
      battleMsg(en, 0, -1, m1, m2);
      waitOk();
      return true;
    }

    // 버그의 차례
    int d = en->atk + randRange(-1, 1);
    g.hp -= d;
    snprintf(m1, sizeof(m1), "%s", en->atk_msg);
    snprintf(m2, sizeof(m2), "바람은 %d 데미지를 입었다.", d);
    api->beep(220, 60);
    battleMsg(en, en_hp, cur, m1, m2);
    api->delay(400);

    if (g.hp <= 0)
    {
      battleMsg(en, en_hp, -1, "바람의 시스템이 리셋되었다...", "마지막 쉼터에서 다시 시작한다.");
      waitOk();
      return false;
    }
  }
}

/*---------------------------------------------------------------------------
 * 이야기 (story.py → rpg_assets.h 의 스크립트)
 *-------------------------------------------------------------------------*/
static void showBlocks(const rpg_block_t *b, int n)
{
  for (int i = 0; i < n; i++)
  {
    if (b[i].who != NULL && strcmp(b[i].who, "바람") == 0) dialogP(b[i].who, RPG_P_BARAM, b[i].lines);
    else                                                   dialogP(b[i].who, -1, b[i].lines);
  }
}

// 문답: 같은 대화창 안에 질문 2 줄 + 보기 3 개
static void drawQuiz(const rpg_quiz_t *q, const char *who, int cur)
{
  drawConvFrame();
  drawSpeaker(who, -1);
  int16_t y = BOX_Y + 10;
  for (int i = 0; q->q[i] != NULL; i++, y += 20) api->print(BOX_X + 14, y, APP_BLACK, "%s", q->q[i]);
  y = BOX_Y + 56;
  for (int i = 0; i < 3; i++, y += 20)
  {
    if (i == cur)
    {
      api->fillRect(BOX_X + 20, y - 1, 220, 19, APP_BLACK);
      api->print(BOX_X + 26, y, APP_WHITE, "%d. %s", i + 1, q->opt[i]);
    }
    else
    {
      api->print(BOX_X + 26, y, APP_BLACK, "%d. %s", i + 1, q->opt[i]);
    }
  }
}

static void runQuiz(const rpg_quiz_t *q, const char *who)
{
  int  cur = 0;
  bool ask = true;            // 처음 물을 때와 틀린 뒤 다시 물을 때: 창을 비우고 질문 음성

  while (1)
  {
    if (ask) convClear();
    drawQuiz(q, who, cur);
    if (ask)
    {
      int n = 0;
      while (q->q[n] != NULL) n++;
      voicePage(q->q, n);
      ask = false;
    }
    show(APP_REFRESH_FAST);
    app_key_t key = waitKey();
    if (key == APP_KEY_UP)   cur = (cur + 2) % 3;
    if (key == APP_KEY_DOWN) cur = (cur + 1) % 3;
    if (key == APP_KEY_OK)
    {
      api->voiceStop();
      if (cur == q->ans)
      {
        api->beep(1047, 80);
        dialogP(who, -1, q->right);     // 같은 창에서 비우고 대답
        return;
      }
      api->beep(220, 80);
      dialogP(who, -1, q->wrong);
      ask = true;
    }
  }
}

// 전투 뒤 지도로: 짧은 깜빡임 갱신 (전투 화면이 겹쳐 남지 않게)
static void restoreMap(void)
{
  dirtyClear();
  drawMap();
  show(APP_REFRESH_FLASH);
}

// 쓰러지면 마지막 쉼터(체크포인트) 방의 서쪽 입구에서 다시
static void respawn(void)
{
  g.room = g.ckpt;
  if (g.room == RPG_START_ROOM) { g.x = RPG_START_X; g.y = RPG_START_Y; }
  else                          { g.x = 1; g.y = 4; }
  g.dir = DIR_RIGHT;
  g.hp  = g.hp_max;
}

static void runScriptsInner(uint8_t owner)
{
  while (1)
  {
    const rpg_script_t *s = NULL;
    for (int i = 0; i < RPG_SCRIPT_CNT; i++)
    {
      const rpg_script_t *c = &rpg_script[i];
      if (c->owner != owner) continue;
      if (c->need >= 0 && !flagOn(c->need)) continue;
      if (c->nope >= 0 && flagOn(c->nope)) continue;
      s = c;
      break;
    }
    if (s == NULL) return;

    showBlocks(s->pre, s->npre);
    if (s->quiz >= 0) runQuiz(&rpg_quiz[s->quiz], s->npre > 0 ? s->pre[s->npre - 1].who : NULL);

    flagSet(s->set);
    if (s->skill >= 0) g.skills |= (1 << s->skill);
    if (s->item >= 0)  g.items[s->item] += s->item_n;
    if (s->equip >= 0) g.equips |= (1 << s->equip);

    if (s->battle >= 0)
    {
      bool run;
      bool win = battle(s->battle, &run);
      if (!win)
      {
        if (!run) respawn();
        restoreMap();
        return;
      }
      flagSet(s->win_set);
      restoreMap();
    }

    showBlocks(s->post, s->npost);
    if (s->end) { g.ended = true; return; }
    if (!s->chain) return;
  }
}

// 스크립트 실행이 끝나면 (어느 길로 끝나든) 대화창을 닫는다
static void runScripts(uint8_t owner)
{
  runScriptsInner(owner);
  convClose();
}

/*---------------------------------------------------------------------------
 * 이동
 *-------------------------------------------------------------------------*/
// 다른 방으로: 짧은 깜빡임 갱신으로 이전 방의 잔상을 지운다
static void enterRoom(void)
{
  if (rpg_room_ckpt[g.room]) g.ckpt = g.room;
  dirtyClear();
  drawMap();
  show(APP_REFRESH_FLASH);
}

// 지금 눌린 방향 (없으면 -1)
static int heldDir(void)
{
  uint8_t h = api->keyHeld();
  if (h & (1 << APP_KEY_UP))    return DIR_UP;
  if (h & (1 << APP_KEY_DOWN))  return DIR_DOWN;
  if (h & (1 << APP_KEY_LEFT))  return DIR_LEFT;
  if (h & (1 << APP_KEY_RIGHT)) return DIR_RIGHT;
  return -1;
}

// 한 칸 이동. 반 칸 그림(짧은 갱신) → 도착(짧은 갱신) 두 장면으로 걷는다.
// 반환: true = 계속 걸어도 됨 (전투·방 넘김이 없었음)
static bool move(int dir)
{
  static const int8_t dx[4] = { 0, 0, -1, 1 };
  static const int8_t dy[4] = { 1, -1, 0, 0 };
  int nx = g.x + dx[dir];
  int ny = g.y + dy[dir];

  g.dir = (uint8_t)dir;

  // 방 넘김: 반 칸 걸어 나간 뒤 다음 방
  if (nx < 0 || nx >= RPG_MAP_W || ny < 0 || ny >= RPG_MAP_H)
  {
    static const uint8_t link_dir[4] = { 1, 0, 2, 3 };   // 아래 → 남, 위 → 북, 서, 동
    int next = rpg_room_link[g.room][link_dir[dir]];
    if (next < 0) return false;
    g.ox = dx[dir] * (TILE / 2);
    g.oy = dy[dir] * (TILE / 2);
    g.step++;
    drawMap();
    show(APP_REFRESH_QUICK);
    g.ox = g.oy = 0;
    g.room = (uint8_t)next;
    g.x    = (uint8_t)((nx + RPG_MAP_W) % RPG_MAP_W);
    g.y    = (uint8_t)((ny + RPG_MAP_H) % RPG_MAP_H);
    g.step++;
    enterRoom();
    return false;
  }

  int e = entAt(g.room, nx, ny);
  if (e >= 0 && rpg_ent[e].kind == RPG_ENT_BUG)
  {
    bool run;
    if (battle(rpg_ent[e].id, &run))
    {
      g.dead |= (1u << e);
      int f = rpg_ent[e].flag;
      flagSet(f);
      restoreMap();
      // 장 보스: 철문이 열린다
      if (f >= 0 && rpg_room_gate[g.room] == f)
      {
        showBlocks(rpg_gate_open, RPG_GATE_OPEN_CNT);
        convClose();
      }
    }
    else
    {
      if (!run) respawn();
      restoreMap();
    }
    return false;
  }

  if (e >= 0 || !rpg_tile_walk[tileAt(g.room, nx, ny)])
  {
    // 막힘: 방향만 바꾼다
    quickFrame(MAP_X0 + g.x * TILE, g.y * TILE, TILE, TILE);
    return false;
  }

  // 8 px 씩 4 단계로 걷는다. 캐릭터가 지나는 두 칸만 짧은 파형으로 갱신
  int16_t rx = MAP_X0 + (dx[dir] < 0 ? nx : g.x) * TILE;
  int16_t ry = (dy[dir] < 0 ? ny : g.y) * TILE;
  int16_t rw = dx[dir] ? TILE * 2 : TILE;
  int16_t rh = dy[dir] ? TILE * 2 : TILE;
  bool    held = true;

  for (int k = 1; k <= WALK_STEPS; k++)
  {
    if (k == WALK_STEPS)
    {
      g.ox = g.oy = 0;
      g.x  = (uint8_t)nx;
      g.y  = (uint8_t)ny;
    }
    else
    {
      g.ox = dx[dir] * (TILE / WALK_STEPS) * k;
      g.oy = dy[dir] * (TILE / WALK_STEPS) * k;
    }
    g.step = (uint8_t)((g.step + 1) & 1);   // 칸마다 다리 모양을 바꾼다
    quickFrame(rx, ry, rw, rh);
    if (k >= WALK_STEPS - 1 && heldDir() != dir) held = false;   // 마지막 쯤 키를 뗐으면 도착 후 멈춘다
  }
  return held;
}


static void interact(void)
{
  static const int8_t dx[4] = { 0, 0, -1, 1 };
  static const int8_t dy[4] = { 1, -1, 0, 0 };
  int x = g.x + dx[g.dir];
  int y = g.y + dy[g.dir];

  if (x < 0 || x >= RPG_MAP_W || y < 0 || y >= RPG_MAP_H) return;

  int e = entAt(g.room, x, y);
  if (e < 0) return;
  if (rpg_ent[e].kind == RPG_ENT_BUG) (void)move(g.dir);   // 버그 → 전투
  else                                runScripts(rpg_ent[e].id);
}

/*---------------------------------------------------------------------------
 * 시험용 CLI (ui rpg ...): 지도 화면에서 키를 기다리는 동안만 실행
 *-------------------------------------------------------------------------*/
// k 장을 깬 상태로 만든다 (깃발, 기술, 장비, 아이템)
static void completeChapter(int k)
{
  switch (k)
  {
    case 1: flagSet(RPG_F_SENIOR); flagSet(RPG_F_GATE1); g.items[0] += 2; break;
    case 2: flagSet(RPG_F_LEARN1); flagSet(RPG_F_GATE2); flagSet(RPG_F_CHEST2);
            g.skills |= 1 << 1; g.equips |= 1 << 0; g.items[0] += 1; g.items[2] += 1; break;
    case 3: flagSet(RPG_F_LEARN2); flagSet(RPG_F_GATE3); flagSet(RPG_F_CHEST1);
            g.skills |= 1 << 2; g.equips |= 1 << 1; g.items[0] += 1; break;
    case 4: flagSet(RPG_F_LEARN3); flagSet(RPG_F_GATE4); g.skills |= 1 << 3; g.items[0] += 1; break;
    case 5: flagSet(RPG_F_LEARN4); flagSet(RPG_F_GATE5); flagSet(RPG_F_CHEST3);
            g.skills |= 1 << 4; g.equips |= 1 << 2; g.items[0] += 1; g.items[2] += 1; break;
    case 6: flagSet(RPG_F_LEARN5); flagSet(RPG_F_GATE6); g.skills |= 1 << 5; g.items[1] += 1; break;
  }
}

// 반환: true = 화면을 다시 그려야 함
static bool runDebugCmd(const char *cmd)
{
  char op[12] = {0};
  int  a = -1, b = -1, c = -1;

  sscanf(cmd, "%11s %d %d %d", op, &a, &b, &c);
  if (strcmp(op, "ch") == 0 && a >= 1 && a <= 7)
  {
    // a 장 시작: 앞 장들을 깬 상태로 a 장 첫 방(2*(a-1))의 서쪽 입구에
    int keep_used = g.used;
    memset(&g, 0, sizeof(g));
    g.used   = keep_used;
    g.skills = 1;
    for (int k = 1; k < a; k++) completeChapter(k);
    g.lv     = (uint8_t)(1 + (a - 1) * 2);
    g.hp_max = (int16_t)(20 + (g.lv - 1) * 3);
    g.hp     = g.hp_max;
    g.room   = (uint8_t)(2 * (a - 1));
    for (int i = 0; i < RPG_ENT_CNT; i++)     // 앞 장의 버그는 쓰러뜨린 것으로
      if (rpg_ent[i].kind == RPG_ENT_BUG && rpg_ent[i].room < g.room) g.dead |= (1u << i);
    if (a == 1) { g.x = RPG_START_X; g.y = RPG_START_Y; }
    else        { g.x = 1; g.y = 4; }
    g.dir  = DIR_RIGHT;
    g.ckpt = g.room;
    api->log("[RPG] ch %d: room %d, Lv%d, flags 0x%08X, skills 0x%02X\n", a, g.room, g.lv, (unsigned)g.flags, g.skills);
    return true;
  }
  if (strcmp(op, "room") == 0 && a >= 0 && a < RPG_ROOM_CNT)
  {
    g.room = (uint8_t)a;
    g.x    = (uint8_t)(b >= 0 && b < RPG_MAP_W ? b : 1);
    g.y    = (uint8_t)(c >= 0 && c < RPG_MAP_H ? c : 4);
    api->log("[RPG] room %d (%d,%d)\n", g.room, g.x, g.y);
    return true;
  }
  if (strcmp(op, "heal") == 0)
  {
    g.hp = g.hp_max;
    return true;
  }
  if (strcmp(op, "state") == 0)
  {
    api->log("[RPG] room %d (%d,%d) Lv%d HP %d/%d flags 0x%08X skills 0x%02X equips 0x%02X items %d/%d/%d dead 0x%08X\n",
             g.room, g.x, g.y, g.lv, g.hp, g.hp_max, (unsigned)g.flags, g.skills, g.equips,
             g.items[0], g.items[1], g.items[2], (unsigned)g.dead);
    return false;
  }
  api->log("[RPG] ? %s (ch N | room R [x y] | heal | state)\n", cmd);
  return false;
}

// 지도 화면에서 키 기다리기 (시험 명령도 여기서 처리)
static app_key_t waitKeyMap(void)
{
  app_key_t key;

  while ((key = api->readKey()) == APP_KEY_NONE)
  {
    const char *cmd = api->debugCmd ? api->debugCmd() : NULL;
    if (cmd != NULL && runDebugCmd(cmd))
    {
      convClose();
      dirtyClear();
      drawMap();
      show(APP_REFRESH_FLASH);
    }
    idleWork();
    api->delay(20);
  }
  return key;
}

/*---------------------------------------------------------------------------
 * 타이틀 / 메뉴
 *-------------------------------------------------------------------------*/
static void newGame(void)
{
  memset(&g, 0, sizeof(g));
  g.used   = true;
  g.room   = RPG_START_ROOM;
  g.x      = RPG_START_X;
  g.y      = RPG_START_Y;
  g.dir    = DIR_DOWN;
  g.lv     = 1;
  g.hp_max = 20;
  g.hp     = g.hp_max;
  g.skills = 1;               // printf
  g.ckpt   = RPG_START_ROOM;
}

// 반환: 0 새로 시작, 1 이어하기, -1 나가기
static int title(void)
{
  int  cnt = g.used ? 3 : 2;
  const char *items_new[]  = { "새로 시작", "런처로" };
  const char *items_cont[] = { "이어하기", "새로 시작", "런처로" };
  const char **items = g.used ? items_cont : items_new;
  int  cur = 0;
  bool first = true;

  while (1)
  {
    api->clear(APP_WHITE);
    for (int x = 0; x < RPG_MAP_W; x++)
    {
      drawTileAt(MAP_X0 + x * TILE, 0, RPG_T_TREE);
      drawTileAt(MAP_X0 + x * TILE, 268, RPG_T_TREE);
    }
    api->drawRect(40, 42, 320, 64, APP_BLACK);
    api->drawRect(43, 45, 314, 58, APP_BLACK);
    api->print(200 - api->strWidth("바람의 펌웨어 수련기") / 2, 54, APP_BLACK, "바람의 펌웨어 수련기");
    api->print(200 - api->strWidth("디버깅으로 버그를 무찌르는 RPG") / 2, 76, APP_BLACK, "디버깅으로 버그를 무찌르는 RPG");

    // 주인공 바람 VS 최종 보스 하드폴트
    drawSpriteBig(56, 112, RPG_S_HERO_DOWN0, 5);
    api->print(84, 196, APP_BLACK, "바람");
    drawBat(248, 112, RPG_B_FAULT, 3);
    api->print(264, 196, APP_BLACK, "하드폴트");
    api->print(188, 150, APP_BLACK, "VS");

    for (int i = 0; i < cnt; i++)
    {
      int16_t y = 214 + i * 18;
      if (i == cur)
      {
        api->fillRect(140, y - 1, 120, 18, APP_BLACK);
        api->print(150, y, APP_WHITE, "> %s", items[i]);
      }
      else
      {
        api->print(150, y, APP_BLACK, "  %s", items[i]);
      }
    }
    show(first ? APP_REFRESH_CLEAN : APP_REFRESH_FAST);
    first = false;

    app_key_t key = waitKey();
    if (key == APP_KEY_UP)   cur = (cur + cnt - 1) % cnt;
    if (key == APP_KEY_DOWN) cur = (cur + 1) % cnt;
    if (key == APP_KEY_BACK) return -1;
    if (key == APP_KEY_OK)
    {
      if (g.used) return cur == 0 ? 1 : (cur == 1 ? 0 : -1);
      return cur == 0 ? 0 : -1;
    }
  }
}

static void rpgMain(const app_api_t *p_api)
{
  api = p_api;
  if (api->version != APP_API_VERSION) return;
  rnd ^= api->millis();
  fast_cnt = 0;

  api->waitKeyRelease();
  while (1)
  {
    int sel = title();
    if (sel < 0) break;
    if (sel == 0) newGame();

    enterRoom();
    if (sel == 0)
    {
      showBlocks(rpg_intro, RPG_INTRO_CNT);
      convClose();
    }

    while (1)
    {
      app_key_t key = waitKeyMap();
      if (key >= APP_KEY_UP && key <= APP_KEY_RIGHT)
      {
        static const uint8_t key_dir[5] = { 0, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
        uint32_t t0   = api->millis();
        bool     cont = move(key_dir[key]);
        // 누르고 있으면 이어 걷기. 한 칸(약 0.25초) 걷는 동안 톡 누른 키가 아직 눌려 있어 두 칸 가는 일이
        // 없도록, 처음 누른 뒤 HOLD_WALK_MS 이상 누르고 있을 때만 이어 간다
        while (cont)
        {
          int d = heldDir();
          if (d < 0) break;
          if (api->millis() - t0 < HOLD_WALK_MS) { api->delay(10); continue; }
          cont = move(d);
        }
        finishDirty();      // 멈추면 걸은 자리를 정상 펄스로 마무리
      }
      else if (key == APP_KEY_OK)
      {
        interact();
        if (g.ended) { g.ended = false; break; }   // 엔딩 → 타이틀
      }
      else if (key == APP_KEY_BACK)
      {
        break;      // 타이틀로 (진행은 g 에 남아 이어하기 가능)
      }
    }
  }

  api->clear(APP_WHITE);
}

const app_def_t rpg_app = { "바람의 펌웨어 수련기", rpgMain };
