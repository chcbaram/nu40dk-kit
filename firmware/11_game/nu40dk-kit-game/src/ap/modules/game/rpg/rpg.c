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
#include "game/app_api.h"
#include "rpg.h"
#include "rpg_assets.h"


#define TILE          32                        // 16x16 그림을 2배로
#define MAP_X0        8                         // 12 칸 x 32 = 384, 양옆 8 px
#define MAP_PX_H      (RPG_MAP_H * TILE)        // 256
#define STATUS_Y      MAP_PX_H                  // 아래 44 px 상태 줄
#define CLEAN_AFTER   40                        // FAST 갱신이 이만큼 쌓이면 멈췄을 때 전체 갱신
#define IDLE_CLEAN_MS 2000
#define IDLE_OFF_MS   3000
#define WALK_STEPS    4                         // 한 칸 = 8 px x 4

enum { DIR_DOWN = 0, DIR_UP, DIR_LEFT, DIR_RIGHT };

typedef struct
{
  const char *name;
  uint8_t     spr;
  int16_t     hp;
  int16_t     atk;
  const char *attack_msg;
} enemy_t;

static const enemy_t enemies[] =
{
  { "잔상 유령",     RPG_S_BUG_GHOST, 9,  2, "잔상이 화면에 번졌다!" },
  { "키 씹힘 벌레",  RPG_S_BUG_KEY,   8,  3, "입력을 씹어 먹었다!" },
  { "스택 오버플로", RPG_S_BUG_STACK, 13, 3, "스택을 덮어썼다!" },
  { "하드폴트",      RPG_S_BUG_FAULT, 28, 4, "CPU를 멈춰 세웠다!" },
};
#define BOSS_ENEMY 3

typedef struct
{
  bool    used;            // 진행 중인 게임이 있음 (이어하기)
  uint8_t room, x, y, dir, step;
  int8_t  ox, oy;          // 걷는 중 그림 위치 (px, 반 칸 = 16)
  int16_t hp, hp_max;
  uint8_t lv;
  uint8_t coffee;
  bool    has_bp;          // 브레이크포인트 배움
  bool    has_stlink;
  bool    chest_open;
  bool    gate_open;
  bool    cleared;
  uint8_t bug_dead;        // 엔티티 번호 비트
} game_t;

static const app_api_t *api;
static game_t           g;
static uint32_t         fast_cnt;
static uint32_t         last_ms;
static bool             epd_on;
static uint32_t         rnd = 12345;


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

static void show(app_refresh_t mode)
{
  api->refresh(mode);
  if (mode == APP_REFRESH_FAST) fast_cnt++;
  else                          fast_cnt = 0;
  last_ms = api->millis();
  epd_on  = true;
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

static uint8_t tileAt(uint8_t room, int x, int y)
{
  uint8_t t = rpg_map[room][y][x];

  if (t == RPG_T_GATE && g.gate_open) return RPG_T_PATH;
  if (t == RPG_T_CHEST && g.chest_open) return RPG_T_CHEST_OPEN;
  if (t == RPG_T_GRASS && ((x * 7 + y * 5 + room * 3) % 4) == 0) return RPG_T_GRASS2;
  return t;
}

static bool entAlive(int i)
{
  if (rpg_ent_def[i].kind == RPG_ENT_BUG) return !(g.bug_dead & (1 << i));
  return true;
}

static int entAt(uint8_t room, int x, int y)
{
  for (int i = 0; i < RPG_ENT_CNT; i++)
  {
    const rpg_ent_def_t *e = &rpg_ent_def[i];
    if (e->room == room && e->x == x && e->y == y && entAlive(i)) return i;
  }
  return -1;
}

static void drawStatus(void)
{
  int16_t y = STATUS_Y + 14;
  api->fillRect(0, STATUS_Y, api->width, api->height - STATUS_Y, APP_BLACK);
  api->print(10, y, APP_WHITE, "%s", rpg_room_name[g.room]);
  api->print(150, y, APP_WHITE, "Lv%d HP %d/%d", g.lv, g.hp, g.hp_max);
  int16_t x = 286;
  if (g.coffee) x = api->print(x, y, APP_WHITE, "커피%d ", g.coffee);
  if (g.has_stlink) api->print(x, y, APP_WHITE, "ST-LINK");
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
    const rpg_ent_def_t *e = &rpg_ent_def[i];
    if (e->room == g.room && entAlive(i)) drawSpriteAt(MAP_X0 + e->x * TILE, e->y * TILE, e->spr, false);
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

// 대화 상자 (지도 아래쪽). lines 는 NULL 로 끝나고, "" 는 쪽 넘김.
// portrait >= 0 이면 왼쪽에 초상화
static void dialogP(const char *who, int portrait, const char *const *lines)
{
  // 대화창: 아래쪽 3 줄. 초상화는 창 왼쪽 위에 걸쳐 올리고, 이름표는 그 옆 창 테두리에 붙인다.
  const int16_t bx = 8, by = 172, bw = 384, bh = 80;
  int i = 0;

  while (lines[i] != NULL)
  {
    drawMap();
    api->fillRect(bx, by, bw, bh, APP_WHITE);
    api->drawRect(bx, by, bw, bh, APP_BLACK);
    api->drawRect(bx + 2, by + 2, bw - 4, bh - 4, APP_BLACK);

    int16_t nx = bx;                         // 이름표 x (얼굴이 있으면 얼굴 박스 오른쪽에 붙임)
    if (portrait >= 0)
    {
      // 64x64 초상화 + 흰 여백 + 두 줄 테두리, 아래 가장자리가 창 테두리와 겹친다
      const int16_t px = bx + 4, py = by - 64 - 2;     // 바깥 테두리를 창 왼쪽에 맞춤
      api->fillRect(px - 4, py - 4, 72, 72, APP_WHITE);
      api->drawRect(px - 4, py - 4, 72, 72, APP_BLACK);
      api->drawRect(px - 2, py - 2, 68, 68, APP_BLACK);
      drawPortrait(px, py, portrait);
      nx = px - 4 + 72 - 2;                  // 얼굴 박스 오른쪽 테두리와 겹치게
    }
    if (who != NULL)
    {
      // 탭 모양: 아래 끝이 창 위 테두리에 붙는다
      int16_t w = api->strWidth(who) + 16;
      api->fillRect(nx, by - 20, w, 22, APP_BLACK);
      api->print(nx + 8, by - 18, APP_WHITE, "%s", who);
    }

    int16_t y = by + 12;
    for (int n = 0; lines[i] != NULL && lines[i][0] != 0 && n < 3; n++, i++)
    {
      api->print(bx + 14, y, APP_BLACK, "%s", lines[i]);
      y += 20;
    }
    if (lines[i] != NULL && lines[i][0] == 0) i++;
    api->print(bx + bw - 20, by + bh - 22, APP_BLACK, ">");
    show(APP_REFRESH_FAST);
    waitOk();
  }
  drawMap();
  show(APP_REFRESH_FAST);
}

static void dialog(const char *who, const char *const *lines)
{
  dialogP(who, -1, lines);
}

static void say(const char *const *lines)
{
  dialogP("바람", RPG_P_BARAM, lines);
}

/*---------------------------------------------------------------------------
 * 전투
 *-------------------------------------------------------------------------*/
static const char *cmd_name[4] = { "printf 디버깅", "브레이크포인트", "커피 마시기", "도망치기" };

static void drawHpBar(int16_t x, int16_t y, int16_t w, int16_t hp, int16_t max)
{
  api->drawRect(x, y, w, 10, APP_BLACK);
  api->fillRect(x + 1, y + 1, w - 2, 8, APP_WHITE);
  if (hp > 0) api->fillRect(x + 2, y + 2, (w - 4) * hp / max, 6, APP_BLACK);
}

static void drawBattle(const enemy_t *en, int16_t en_hp, int cur, const char *m1, const char *m2)
{
  api->clear(APP_WHITE);

  // 적
  api->print(16, 12, APP_BLACK, "%s", en->name);
  drawHpBar(16, 34, 160, en_hp, en->hp);
  api->print(184, 31, APP_BLACK, "%d/%d", en_hp > 0 ? en_hp : 0, en->hp);
  drawSpriteBig(40, 56, en->spr, 6);

  // 바람
  api->drawRect(232, 120, 156, 60, APP_BLACK);
  drawSpriteAt(240, 134, RPG_S_HERO_SIDE0, true);
  api->print(280, 128, APP_BLACK, "바람 Lv%d", g.lv);
  drawHpBar(280, 150, 72, g.hp, g.hp_max);
  api->print(356, 146, APP_BLACK, "%d", g.hp > 0 ? g.hp : 0);

  // 메시지
  api->drawRect(8, 200, 384, 44, APP_BLACK);
  if (m1) api->print(16, 204, APP_BLACK, "%s", m1);
  if (m2) api->print(16, 224, APP_BLACK, "%s", m2);

  // 명령
  api->fillRect(0, 252, api->width, 48, APP_BLACK);
  for (int i = 0; i < 4; i++)
  {
    int16_t x = 12 + (i % 2) * 196;
    int16_t y = 256 + (i / 2) * 20;
    const char *name = (i == 1 && !g.has_bp) ? "???" : cmd_name[i];
    if (i == cur)
    {
      api->fillRect(x - 4, y, 188, 19, APP_WHITE);
      api->print(x, y + 1, APP_BLACK, "> %s", name);
    }
    else
    {
      api->print(x, y + 1, APP_WHITE, "  %s", name);
    }
  }
}

// 반환: true = 이김, false = 짐 또는 도망 (p_run 으로 구분)
static bool battle(int enemy, bool *p_run)
{
  const enemy_t *en = &enemies[enemy];
  int16_t        en_hp = en->hp;
  int            cur = 0;
  char           m1[48], m2[48];
  bool           boss = (enemy == BOSS_ENEMY);

  *p_run = false;
  snprintf(m1, sizeof(m1), "%s 이(가) 나타났다!", en->name);
  drawBattle(en, en_hp, cur, m1, boss ? "printf 로는 잡기 어려워 보인다..." : NULL);
  api->beep(440, 80);
  show(APP_REFRESH_CLEAN);

  while (1)
  {
    // 명령 고르기
    while (1)
    {
      app_key_t key = waitKey();
      int       pre = cur;
      if (key == APP_KEY_UP || key == APP_KEY_DOWN)    cur ^= 2;
      if (key == APP_KEY_LEFT || key == APP_KEY_RIGHT) cur ^= 1;
      if (key == APP_KEY_OK)
      {
        if (cur == 1 && !g.has_bp) continue;
        if (cur == 2 && g.coffee == 0) continue;
        break;
      }
      if (cur != pre)
      {
        drawBattle(en, en_hp, cur, "무엇을 할까?", NULL);
        show(APP_REFRESH_FAST);
      }
    }

    // 바람의 차례
    m2[0] = 0;
    if (cur == 0)
    {
      int d = boss ? 1 : randRange(2, 4);
      en_hp -= d;
      snprintf(m1, sizeof(m1), "printf 로 로그를 찍었다!");
      snprintf(m2, sizeof(m2), boss ? "로그가 안 나온다... %d 데미지" : "버그 위치가 좁혀졌다. %d 데미지", d);
    }
    else if (cur == 1)
    {
      int d = randRange(5, 8);
      en_hp -= d;
      snprintf(m1, sizeof(m1), "브레이크포인트를 걸었다!");
      snprintf(m2, sizeof(m2), "레지스터를 낱낱이 봤다. %d 데미지", d);
    }
    else if (cur == 2)
    {
      int h = 10;
      g.coffee--;
      g.hp = (g.hp + h > g.hp_max) ? g.hp_max : g.hp + h;
      snprintf(m1, sizeof(m1), "커피를 마셨다! HP %d 회복", h);
    }
    else
    {
      if (!boss && randRange(0, 99) < 70)
      {
        drawBattle(en, en_hp, cur, "버그를 TODO 로 미뤄 두고 도망쳤다...", NULL);
        show(APP_REFRESH_FAST);
        api->delay(900);
        *p_run = true;
        return false;
      }
      snprintf(m1, sizeof(m1), boss ? "보스에게서는 도망칠 수 없다!" : "도망치지 못했다!");
    }
    drawBattle(en, en_hp, cur, m1, m2[0] ? m2 : NULL);
    api->beep(cur == 2 ? 880 : 660, 40);
    show(APP_REFRESH_FAST);
    api->delay(700);

    if (en_hp <= 0)
    {
      g.lv++;
      g.hp_max += 4;
      g.hp = g.hp_max;
      snprintf(m1, sizeof(m1), "%s 을(를) 고쳤다!", en->name);
      snprintf(m2, sizeof(m2), "레벨 업! Lv%d, 최대 HP %d", g.lv, g.hp_max);
      drawBattle(en, 0, -1, m1, m2);
      api->beep(1047, 120);
      show(APP_REFRESH_FAST);
      waitOk();
      return true;
    }

    // 버그의 차례
    int d = en->atk + randRange(-1, 1);
    g.hp -= d;
    snprintf(m1, sizeof(m1), "%s: %s", en->name, en->attack_msg);
    snprintf(m2, sizeof(m2), "바람은 %d 데미지를 입었다.", d);
    drawBattle(en, en_hp, cur, m1, m2);
    api->beep(220, 60);
    show(APP_REFRESH_FAST);
    api->delay(700);

    if (g.hp <= 0)
    {
      drawBattle(en, en_hp, -1, "바람의 시스템이 리셋되었다...", "부트로더 마을에서 다시 시작한다.");
      show(APP_REFRESH_FAST);
      waitOk();
      return false;
    }
  }
}

/*---------------------------------------------------------------------------
 * 이야기
 *-------------------------------------------------------------------------*/
static void talkNpc(int val)
{
  if (val == 0)      // 선배 개발자
  {
    if (g.cleared)
    {
      static const char *const l[] = { "하드폴트를 잡았다고? 이제 너도 고수구나!", "다음엔 RTOS 산맥에 가 보는 건 어때?", NULL };
      dialog("선배 개발자", l);
    }
    else if (!g.has_bp)
    {
      static const char *const l[] = {
        "바람, 펌웨어 고수가 되고 싶다고?", "그럼 수련을 떠나야지.", "",
        "남쪽 인터럽트 호수에 장인이 계셔.", "동쪽 레지스터 숲엔 버그가 들끓으니", "조심하고. 커피는 챙겼어? 여기 하나.", NULL };
      if (g.coffee < 1) g.coffee = 1;
      dialog("선배 개발자", l);
    }
    else
    {
      static const char *const l[] = { "브레이크포인트를 배웠구나!", "숲 속 서쪽 커널 사당에", "펌웨어 고수가 계셔.", NULL };
      dialog("선배 개발자", l);
    }
  }
  else if (val == 1) // 인터럽트 장인
  {
    if (!g.has_bp)
    {
      static const char *const l[] = {
        "허허, 물고기는 기다림으로 잡고", "버그는 관찰로 잡는 법이지.", "",
        "인터럽트는 짧게! 오래 붙잡지 마라.", "그리고 printf 만 믿지 말고...", "",
        "브레이크포인트를 배웠다!", "(전투 명령이 늘었다)", "",
        "사당 문은 디버그 포트로만 열린다네.", "숲 어딘가 상자에 ST-LINK 가 있지.", NULL };
      g.has_bp = true;
      api->beep(988, 120);
      dialog("인터럽트 장인", l);
    }
    else
    {
      static const char *const l[] = { "ISR 안에서 delay() 를 부르면 안 되네.", "스택 오버플로 슬라임도 조심하고.", NULL };
      dialog("인터럽트 장인", l);
    }
  }
  else               // 펌웨어 고수
  {
    if (g.cleared)
    {
      static const char *const l[] = { "잘했다. 버그는 늘 다시 온다.", "그때마다 침착하게 재현하고 관찰하거라.", NULL };
      dialog("펌웨어 고수", l);
      return;
    }
    static const char *const l[] = {
      "여기까지 왔구나, 바람.", "고수란 버그를 두려워하지 않는 자다.", "",
      "마지막 시험이다.", "부팅마다 멈추는 저 하드폴트를", "잡아 보거라!", NULL };
    dialog("펌웨어 고수", l);

    bool run;
    if (battle(BOSS_ENEMY, &run))
    {
      g.cleared = true;
      drawMap();
      show(APP_REFRESH_CLEAN);
      static const char *const e[] = {
        "스택 프레임에서 PC 를 찾았구나.", "널 포인터 하나가 범인이었지.", "",
        "이제 너는 펌웨어 고수다, 바람!", NULL };
      dialog("펌웨어 고수", e);
      static const char *const b[] = { "버그는 무섭지 않아.", "재현하고, 관찰하고, 고치면 돼!", NULL };
      say(b);
      static const char *const f[] = { "- 프로토타입 끝 -", "플레이 고마워요!", NULL };
      dialog(NULL, f);
    }
    else
    {
      g.room = RPG_START_ROOM; g.x = RPG_START_X; g.y = RPG_START_Y; g.dir = DIR_DOWN;
      g.hp = g.hp_max;
      drawMap();
      show(APP_REFRESH_CLEAN);
    }
  }
}

static void examine(int x, int y)
{
  uint8_t t = rpg_map[g.room][y][x];

  if (t == RPG_T_SIGN)
  {
    if (g.room == 1)
    {
      static const char *const l[] = { "[부트로더 마을]", "동: 레지스터 숲", "서쪽 마을 남쪽: 인터럽트 호수", NULL };
      dialog(NULL, l);
    }
    else
    {
      static const char *const l[] = { "[인터럽트 호수]", "커널 사당은 숲 속 서쪽에 있다.", NULL };
      dialog(NULL, l);
    }
  }
  else if (t == RPG_T_CHEST)
  {
    if (!g.chest_open)
    {
      static const char *const l[] = { "상자를 열었다!", "ST-LINK 디버거와 커피 2잔을 얻었다.", NULL };
      static const char *const b[] = { "ST-LINK 다!", "이걸로 디버그 포트를 열 수 있겠어.", NULL };
      g.chest_open = true;
      g.has_stlink = true;
      g.coffee += 2;
      api->beep(1047, 120);
      dialog(NULL, l);
      say(b);
    }
    else
    {
      static const char *const l[] = { "빈 상자다.", NULL };
      dialog(NULL, l);
    }
  }
  else if (t == RPG_T_GATE && !g.gate_open)
  {
    if (g.has_stlink)
    {
      static const char *const l[] = { "문에 디버그 포트가 있다.", "ST-LINK 를 연결했다... 문이 열렸다!", NULL };
      g.gate_open = true;
      api->beep(784, 150);
      dialog(NULL, l);
    }
    else
    {
      static const char *const l[] = { "굳게 잠겨 있다.", "작은 디버그 포트(SWD)가 보인다...", NULL };
      dialog(NULL, l);
    }
  }
  else if (t == RPG_T_ALTAR)
  {
    static const char *const l[] = { "제단에 리셋 벡터가 새겨져 있다.", "0x0000_0004 ...", NULL };
    dialog(NULL, l);
  }
  else if (t == RPG_T_DOOR)
  {
    static const char *const l[] = { "바람의 집이다.", "책상 위에 굽다 만 보드가 있다.", NULL };
    dialog(NULL, l);
  }
}

/*---------------------------------------------------------------------------
 * 이동
 *-------------------------------------------------------------------------*/
static void enterRoom(void)
{
  drawMap();
  show(APP_REFRESH_CLEAN);
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
  if (e >= 0 && rpg_ent_def[e].kind == RPG_ENT_BUG)
  {
    bool run;
    if (battle(rpg_ent_def[e].val, &run))
    {
      g.bug_dead |= (1 << e);
    }
    else if (!run)
    {
      g.room = RPG_START_ROOM; g.x = RPG_START_X; g.y = RPG_START_Y; g.dir = DIR_DOWN;
      g.hp = g.hp_max;
    }
    enterRoom();
    return false;
  }

  if (e >= 0 || !rpg_tile_walk[tileAt(g.room, nx, ny)])
  {
    // 막힘: 방향만 바꾼다
    drawMap();
    showRect(APP_REFRESH_QUICK, MAP_X0 + g.x * TILE, g.y * TILE, TILE, TILE);
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
    g.step = (uint8_t)(k & 1);
    drawMap();
    showRect(APP_REFRESH_QUICK, rx, ry, rw, rh);
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
  if (e >= 0)
  {
    if (rpg_ent_def[e].kind == RPG_ENT_NPC) talkNpc(rpg_ent_def[e].val);
    else                                    (void)move(g.dir);  // 버그 → 전투
    return;
  }
  examine(x, y);
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
    api->print(200 - api->strWidth("- RPG prototype -") / 2, 76, APP_BLACK, "- RPG prototype -");

    drawSpriteBig(64, 116, RPG_S_HERO_DOWN0, 6);
    drawSpriteBig(256, 124, RPG_S_BUG_FAULT, 5);
    api->print(184, 166, APP_BLACK, "VS");

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
      static const char *const l[] = {
        "개발자 바람은 오늘도 보드 앞에 앉았다.", "하지만 이유 모를 리셋, 사라지는 키 입력,", "화면에 남는 잔상...", NULL };
      dialog(NULL, l);
      static const char *const b[] = { "펌웨어 고수가 되고 싶다!", "수련을 떠나자.", NULL };
      say(b);
      static const char *const h[] = { "방향키: 이동  A: 조사/대화  B: 메뉴", NULL };
      dialog(NULL, h);
    }

    while (1)
    {
      app_key_t key = waitKey();
      if (key >= APP_KEY_UP && key <= APP_KEY_RIGHT)
      {
        static const uint8_t key_dir[5] = { 0, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
        bool cont = move(key_dir[key]);
        // 누르고 있는 동안 쉬지 않고 이어 걷기 (키 자동 반복의 첫 지연 없이)
        while (cont)
        {
          int d = heldDir();
          if (d < 0) break;
          cont = move(d);
        }
        api->readKey();     // 키 상태 다시 맞춤 (뗀 뒤 한 걸음 더 가지 않게)
      }
      else if (key == APP_KEY_OK)
      {
        interact();
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
