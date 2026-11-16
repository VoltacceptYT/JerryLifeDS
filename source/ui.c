#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "daycycle.h"
#include "jl.h"
#include "map.h"
#include "sprites.h"
#include "util.h"
#include "world.h"

#define C_WHITE RGB8(255, 255, 255)
#define C_DIM RGB8(170, 170, 170)
#define C_GOLD RGB8(255, 170, 0)
#define C_GREEN RGB8(85, 255, 85)
#define C_RED RGB8(255, 85, 85)
#define C_ORANGE RGB8(255, 150, 60)
#define C_CYAN RGB8(111, 214, 255)
#define C_PINK RGB8(255, 122, 168)

enum { M_MAP, M_JERRY, M_PICK, M_HANG, M_SHOP, M_HALL, M_CREATE, M_NAME, M_NEWS, M_RES, M_ITEMS, M_MENU, M_CONFIRM };

enum {
    ID_NONE, ID_BACK, ID_NEWS, ID_RES, ID_ITEMS, ID_MENU, ID_FEED, ID_GIFT, ID_TALK, ID_HANG, ID_MOVEOUT, ID_CLOSE,
    ID_CREATE, ID_BUILD, ID_YES, ID_NO, ID_PREV, ID_NEXT, ID_EDIT, ID_GO, ID_SEASON, ID_TIME, ID_MUSIC, ID_SAVE,
    ID_ERASE, ID_BKSP, ID_SPACE, ID_DONE,
    ID_TAB = 100, ID_SKIN = 120, ID_PERS = 140, ID_KEY = 200, ID_ROW = 1000
};

typedef struct { int16_t x, y, w, h; int id; bool list; } Btn;
typedef struct { int x, y, w, h, rowH, count; } List;

static struct {
    int mode;
    int shopKind;       // 0 = cafe, 1 = toy store
    int tab;
    int pickKind;       // 0 = feed, 1 = gift
    int confirmKind;    // 0 = move out, 1 = erase island
    int camX, camY;
    bool follow;
    int scroll;         // list scroll in pixels
    int focusId;
    bool focusVis;
    int pressed;        // button held by the stylus
    int downX, downY, lastY;
    bool drag, listTouch;
    bool mmPress, mmMoved;
    int mmWx, mmWy;
    int dragJ;          // jerry id being carried on the minimap, 0 = none
    // new resident
    char name[NAME_LEN];
    int nameIdx, skin, pers;
    bool first, fromHall;
    // footer message
    char msg[40];
    int msgT;
    bool dirty;
} U;

static Btn btns[110];
static int nbtn;
static List lst;

static const char *const SEASON_NAMES[4] = {"Spring", "Summer", "Fall", "Winter"};
static const char *const SEASON_MODE[5] = {"Auto", "Spring", "Summer", "Fall", "Winter"};
static const char *const TIME_MODE[TIME_MODES] = {"Auto", "Morning", "Day", "Evening", "Night"};
static const char *const TASTE_TAB[5] = {"Sweet", "Savory", "Spicy", "Drink", "Gross"};

/* ================================================================ small helpers */

static void go(int mode) {
    U.mode = mode;
    U.scroll = 0;
    U.focusId = 0;
    U.tab = 0;
    U.pressed = 0;
    U.drag = U.listTouch = false;
    U.dirty = true;
}

static void say_msg(const char *m, bool bad) {
    snprintf(U.msg, sizeof U.msg, "%s", m);
    U.msgT = 150;
    if (bad) sfx_play(SFX_NO);
    U.dirty = true;
}

static void center_cam_on(float x, float y) {
    U.camX = clampi((int)x - VIEW_W / 2, 0, PW - VIEW_W);
    U.camY = clampi((int)y - VIEW_H / 2, 0, PH - VIEW_H);
}

static void select_jerry(Jerry *j, bool wake) {
    if (wake) jl_wake(j);
    sfx_play(SFX_CLICK);
    jl_select(j);
    U.follow = true;
    go(M_JERRY);
}

static void open_create(bool first, bool fromHall) {
    U.first = first;
    U.fromHall = fromHall;
    U.nameIdx = RI(0, JL_NAME_COUNT - 1);
    snprintf(U.name, sizeof U.name, "%s", JL_NAMES[U.nameIdx]);
    U.skin = 0;
    U.pers = RI(0, PERS_COUNT - 1);
    go(M_CREATE);
}

static void open_shop(int kind) {
    U.shopKind = kind;
    go(M_SHOP);
}

static int tab_count(void) {
    if (U.mode == M_SHOP) return U.shopKind == 0 ? 5 : 2;
    if (U.mode == M_RES) return 2;
    if (U.mode == M_ITEMS) return 3;
    return 0;
}

void ui_init(void) {
    memset(&U, 0, sizeof U);
    center_cam_on(512, 330);
    U.mode = M_MAP;
    U.dirty = true;
    if (jl.nJerries == 0) open_create(true, false);
}

void ui_mark_bottom(void) { U.dirty = true; }
bool ui_bottom_dirty(void) { return U.dirty; }

/* ---------------------------------------------------------------- item lists */

static int owned_foods(int *out) {
    int n = 0;
    for (int i = 0; i < FOOD_COUNT; i++) if (jl.foods[i]) out[n++] = i;
    return n;
}
static int owned_gifts(int *out) {  // gifts, then toys (encoded as GIFT_COUNT + toy)
    int n = 0;
    for (int i = 0; i < GIFT_COUNT; i++) if (jl.gifts[i]) out[n++] = i;
    for (int i = 0; i < TOY_COUNT; i++) if (jl.toysOwned[i]) out[n++] = GIFT_COUNT + i;
    return n;
}
static int shop_items(int *out) {
    int n = 0;
    if (U.shopKind == 0) {
        for (int i = 0; i < FOOD_COUNT; i++) if (FOODS[i].taste == U.tab) out[n++] = i;
    } else if (U.tab == 0) {
        for (int i = 0; i < TOY_COUNT; i++) out[n++] = i;
    } else {
        for (int i = 0; i < GIFT_COUNT; i++) out[n++] = i;
    }
    return n;
}
typedef struct { int a, b, v; } Pair;
static int friend_pairs(Pair *out, int max) {
    int n = 0;
    for (int i = 0; i < jl.nRel && n < max; i++) {
        if (jl.rel[i].v < 8) continue;
        if (!jl_jerry(jl.rel[i].a) || !jl_jerry(jl.rel[i].b)) continue;
        Pair p = {jl.rel[i].a, jl.rel[i].b, jl.rel[i].v};
        int k = n++;
        while (k > 0 && out[k - 1].v < p.v) { out[k] = out[k - 1]; k--; }
        out[k] = p;
    }
    return n;
}

/* ================================================================ drawing primitives */

static void reg(int id, int x, int y, int w, int h, bool inList) {
    if (nbtn >= (int)(sizeof btns / sizeof btns[0])) return;
    if (inList) {
        if (y < lst.y) { h -= lst.y - y; y = lst.y; }
        if (y + h > lst.y + lst.h) h = lst.y + lst.h - y;
        if (h <= 0) return;
    }
    btns[nbtn++] = (Btn){(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, id, inList};
}

static void panel(Canvas *c, int x, int y, int w, int h) {
    gfx_bevel_panel(c, x, y, w, h, RGB8(34, 34, 34), RGB8(85, 85, 85), RGB8(0, 0, 0));
}

static void T8(Canvas *c, int x, int y, const char *s, px col) { gfx_text(c, &fontsm, x, y, s, col, 1); }
static void T10(Canvas *c, int x, int y, const char *s, px col) { gfx_text(c, &font10, x, y, s, col, 1); }

static const char *const B_COIN[5] = {".oyyo.", "oyyyyo", "oyoyyo", "oyyyyo", ".oyyo."};
static const char *const B_LARR[7] = {"...w", "..ww", ".www", "wwww", ".www", "..ww", "...w"};
static const char *const B_RARR[7] = {"w...", "ww..", "www.", "wwww", "www.", "ww..", "w..."};

static void coin_text_right(Canvas *c, int rx, int y, int amount) {
    char a[16];
    snprintf(a, sizeof a, "%d", amount);
    int w = gfx_text_width(&font10, a);
    T10(c, rx - w, y, a, C_WHITE);
    fx_bits(c, rx - w - 9, y + 2, B_COIN, 5);
}

static void header(Canvas *c, const char *title, bool coins) {
    panel(c, 0, 0, SCREEN_W, 17);
    T10(c, 6, 4, title, C_GOLD);
    if (coins) coin_text_right(c, 250, 4, jl.coins);
}

static void focus_ring(Canvas *c, int id, int x, int y, int w, int h) {
    if (U.focusVis && U.focusId == id) {
        gfx_frame(c, x - 1, y - 1, w + 2, h + 2, C_GOLD);
        gfx_frame(c, x, y, w, h, RGB8(200, 130, 0));
    }
}

enum { BS_NORMAL, BS_OK, BS_CANCEL };

static void button_f(Canvas *c, const Font *f, int id, int x, int y, int w, int h, const char *label, int style,
                     bool enabled) {
    bool pr = U.pressed == id;
    const uint16_t *tex = style == BS_OK ? (pr ? tex_ok_active : tex_ok)
                        : style == BS_CANCEL ? (pr ? tex_cancel_active : tex_cancel)
                        : (pr ? tex_button_active : tex_button);
    gfx_nineslice(c, tex, x, y, w, h);
    gfx_text_centered(c, f, x + w / 2, y + (h - f->height) / 2 + (pr ? 1 : 0), label, enabled ? C_WHITE : C_DIM, 1);
    reg(id, x, y, w, h, false);
    focus_ring(c, id, x, y, w, h);
}
static void button(Canvas *c, int id, int x, int y, int w, int h, const char *label, int style, bool enabled) {
    button_f(c, &font10, id, x, y, w, h, label, style, enabled);
}

static void footer(Canvas *c, const char *hint) {
    button(c, ID_BACK, 4, 166, 80, 24, "Back", BS_CANCEL, true);
    if (U.msgT > 0) T8(c, 92, 174, U.msg, C_GOLD);
    else if (hint) T8(c, 92, 174, hint, C_DIM);
}

static void tabs(Canvas *c, const char *const *labels, int n, int y) {
    int w = 248 / n;
    for (int i = 0; i < n; i++) {
        int x = 4 + i * w;
        bool on = i == U.tab, pr = U.pressed == ID_TAB + i;
        gfx_nineslice(c, (on || pr) ? tex_button_active : tex_button, x, y, w - 2, 15);
        gfx_text_centered(c, &fontsm, x + (w - 2) / 2, y + 4 + (on ? 1 : 0), labels[i], on ? C_GOLD : C_WHITE, 1);
        reg(ID_TAB + i, x, y, w - 2, 15, false);
        focus_ring(c, ID_TAB + i, x, y, w - 2, 15);
    }
}

static void bar(Canvas *c, int x, int y, int w, int h, float v, px col) {
    gfx_fill(c, x, y, w, h, RGB8(16, 16, 16));
    gfx_frame(c, x, y, w, h, RGB8(70, 70, 70));
    int fw = (int)((w - 2) * clampf(v, 0, 100) / 100.0f);
    gfx_fill(c, x + 1, y + 1, fw, h - 2, col);
}

/* ---------------------------------------------------------------- lists */

static void list_begin(Canvas *c, int x, int y, int w, int h, int rowH, int count) {
    lst = (List){x, y, w, h, rowH, count};
    int maxScroll = count * rowH - h;
    if (maxScroll < 0) maxScroll = 0;
    int fr = U.focusVis && U.focusId >= ID_ROW ? U.focusId - ID_ROW : -1;
    if (fr >= 0 && fr < count) {
        if (fr * rowH < U.scroll) U.scroll = fr * rowH;
        if ((fr + 1) * rowH > U.scroll + h) U.scroll = (fr + 1) * rowH - h;
    }
    U.scroll = clampi(U.scroll, 0, maxScroll);
    gfx_clip(c, x, y, w, h);
}

static int list_row_y(int i) { return lst.y + i * lst.rowH - U.scroll; }

static bool list_visible(int i) {
    int y = list_row_y(i);
    return y + lst.rowH > lst.y && y < lst.y + lst.h;
}

static void list_end(Canvas *c, const char *emptyMsg) {
    gfx_unclip(c);
    if (lst.count == 0 && emptyMsg) {
        gfx_text_centered(c, &fontsm, lst.x + lst.w / 2, lst.y + lst.h / 2 - 4, emptyMsg, C_DIM, 1);
        return;
    }
    int total = lst.count * lst.rowH;
    if (total <= lst.h) return;
    int tx = lst.x + lst.w - 4;
    gfx_fill(c, tx, lst.y, 4, lst.h, RGB8(20, 20, 20));
    int thumbH = lst.h * lst.h / total;
    if (thumbH < 8) thumbH = 8;
    int maxScroll = total - lst.h;
    gfx_fill(c, tx, lst.y + (lst.h - thumbH) * U.scroll / maxScroll, 4, thumbH, C_DIM);
}

// A row background that is also a button. Returns false if the row is off screen.
static bool row_bg(Canvas *c, int i, px fill, px light, px dark) {
    if (!list_visible(i)) return false;
    int y = list_row_y(i), w = lst.w - 6, id = ID_ROW + i;
    if (U.pressed == id) { fill = (px)(fill + RGB8(24, 24, 24)); }
    gfx_bevel_panel(c, lst.x, y, w, lst.rowH - 1, fill, light, dark);
    reg(id, lst.x, y, w, lst.rowH - 1, true);
    focus_ring(c, id, lst.x, y, w, lst.rowH - 1);
    return true;
}
#define ROW_NORMAL RGB8(48, 48, 48), RGB8(110, 110, 110), RGB8(16, 16, 16)
#define ROW_GOOD RGB8(20, 60, 20), RGB8(60, 130, 60), RGB8(0, 24, 0)
#define ROW_BAD RGB8(70, 24, 24), RGB8(122, 40, 40), RGB8(28, 0, 0)
#define ROW_GOLD RGB8(76, 56, 8), RGB8(200, 150, 30), RGB8(30, 20, 0)

static int wrap(const char *s, int maxChars, char lines[][48], int maxLines) {
    int nl = 0, len = 0;
    lines[0][0] = 0;
    while (*s) {
        char w[48];
        int n = 0;
        while (*s && *s != ' ' && n < 47) w[n++] = *s++;
        w[n] = 0;
        while (*s == ' ') s++;
        if (len && len + 1 + n > maxChars && nl + 1 < maxLines) { nl++; lines[nl][0] = 0; len = 0; }
        if (len) { strcat(lines[nl], " "); len++; }
        strncat(lines[nl], w, 47 - (size_t)len);
        len += n;
    }
    return nl + 1;
}

static void verdict_tag(int v, const char **t, px *col) {
    switch (v) {
    case V_LOVE: *t = "Loves it"; *col = C_GOLD; break;
    case V_LIKE: *t = "Likes"; *col = C_GREEN; break;
    case V_OK: *t = "OK"; *col = C_WHITE; break;
    case V_DISLIKE: *t = "Meh"; *col = C_ORANGE; break;
    default: *t = "Yuck"; *col = C_RED; break;
    }
}

/* ================================================================ hit testing / interaction */

static Hit find_hit(float wx, float wy, int tol) {
    static const int off[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    Hit h = jl_hit(wx, wy);
    if (h.kind != HIT_NONE) return h;
    for (int r = 6; r <= tol; r += 6)
        for (int k = 0; k < 8; k++) {
            h = jl_hit(wx + (float)(off[k][0] * r), wy + (float)(off[k][1] * r));
            if (h.kind != HIT_NONE) return h;
        }
    return (Hit){HIT_NONE, -1};
}

static const Jerry *house_owner(const Building *b) {
    for (int i = 0; i < jl.nHouses; i++)
        if (jl.houses[i].slot == b->slot && jl.houses[i].owner) return jl_jerry(jl.houses[i].owner);
    return NULL;
}

static void interact(Hit h, float wx, float wy) {
    U.dirty = true;
    if (h.kind == HIT_NONE) { jl.selected = 0; return; }
    if (h.kind == HIT_JERRY) { select_jerry(&jl.jerries[h.index], false); return; }
    if (h.kind == HIT_SPARK) { jl_collect_spark(h.index); return; }
    if (h.kind == HIT_TOY) { jl_kick_toy(h.index, wx, wy); return; }
    const Building *b = &map_buildings[h.index];
    switch (b->type) {
    case BT_CAFE: open_shop(0); break;
    case BT_SHOP: open_shop(1); break;
    case BT_HALL: go(M_HALL); break;
    case BT_TOWER: go(M_NEWS); break;
    case BT_CAR: jl_truck(); break;
    case BT_HOUSE: {
        const Jerry *ow = house_owner(b);
        if (ow) select_jerry(jl_jerry(ow->id), true);
        else go(M_HALL);
        break;
    }
    }
}

static const char *hint_for(Hit h, char *buf, size_t n) {
    switch (h.kind) {
    case HIT_JERRY: snprintf(buf, n, "A: %s", jl.jerries[h.index].name); return buf;
    case HIT_SPARK: return "A: Collect";
    case HIT_TOY: return "A: Kick";
    case HIT_BUILDING: {
        const Building *b = &map_buildings[h.index];
        if (b->type == BT_HOUSE) {
            const Jerry *ow = house_owner(b);
            if (ow) snprintf(buf, n, "A: %s's home", ow->name);
            else snprintf(buf, n, "A: Vacant home");
        } else snprintf(buf, n, "A: %s", b->name);
        return buf;
    }
    default: return NULL;
    }
}

/* ================================================================ actions */

static void back(void) {
    switch (U.mode) {
    case M_PICK: case M_HANG: go(M_JERRY); break;
    case M_NAME: go(M_CREATE); break;
    case M_CREATE: if (!U.first) go(U.fromHall ? M_HALL : M_MAP); break;
    case M_CONFIRM: go(U.confirmKind == 0 ? M_JERRY : M_MENU); break;
    case M_JERRY: jl.selected = 0; go(M_MAP); break;
    default: go(M_MAP); break;
    }
}

static void act(int id) {
    U.dirty = true;
    Jerry *sel = jl_selected();
    int items[64];

    if (id == ID_BACK) { back(); return; }
    if (id >= ID_TAB && id < ID_TAB + 10) {
        if (id - ID_TAB < tab_count()) { U.tab = id - ID_TAB; U.scroll = 0; U.focusId = 0; }
        return;
    }
    switch (U.mode) {
    case M_MAP:
        if (id == ID_NEWS) go(M_NEWS);
        else if (id == ID_RES) go(M_RES);
        else if (id == ID_ITEMS) go(M_ITEMS);
        else if (id == ID_MENU) go(M_MENU);
        break;

    case M_JERRY:
        if (!sel) { go(M_MAP); break; }
        if (id == ID_FEED) { U.pickKind = 0; go(M_PICK); }
        else if (id == ID_GIFT) { U.pickKind = 1; go(M_PICK); }
        else if (id == ID_TALK) jl_talk(sel);
        else if (id == ID_HANG) go(M_HANG);
        else if (id == ID_MOVEOUT) { U.confirmKind = 0; go(M_CONFIRM); }
        else if (id == ID_CLOSE) back();
        break;

    case M_PICK:
        if (!sel || id < ID_ROW) break;
        {
            int n = U.pickKind == 0 ? owned_foods(items) : owned_gifts(items), i = id - ID_ROW;
            if (i >= n) break;
            bool ok;
            if (U.pickKind == 0) ok = jl_feed(sel, items[i]);
            else if (items[i] >= GIFT_COUNT) ok = jl_give_toy(sel, items[i] - GIFT_COUNT);
            else ok = jl_give(sel, items[i]);
            if (ok) go(M_JERRY);
        }
        break;

    case M_HANG:
        if (sel && id >= ID_ROW) {
            int k = 0, i = id - ID_ROW;
            for (int j = 0; j < jl.nJerries; j++) {
                Jerry *o = &jl.jerries[j];
                if (o == sel || o->inside) continue;
                if (k++ == i) { jl_hang_out(sel, o); go(M_JERRY); break; }
            }
        }
        break;

    case M_SHOP:
        if (id >= ID_ROW) {
            int n = shop_items(items), i = id - ID_ROW;
            if (i >= n) break;
            int it = items[i];
            bool ok;
            if (U.shopKind == 0) ok = jl_buy_food(it);
            else if (U.tab == 0) {
                if (jl.toysOwned[it]) { say_msg("You already own this", true); break; }
                ok = jl_buy_toy(it);
            } else ok = jl_buy_gift(it);
            if (!ok) {
                int price = U.shopKind == 0 ? FOODS[it].price : U.tab == 0 ? TOYS[it].price : GIFTS[it].price;
                say_msg(jl.coins < price ? "Not enough coins" : "Bag full (99)", true);
            }
        }
        break;

    case M_HALL:
        if (id == ID_CREATE) {
            if (jl_free_houses() == 0) say_msg("No free house: build one", true);
            else if (jl.nJerries >= MAX_JERRIES) say_msg("The island is full", true);
            else if (jl.coins < jl_create_cost()) say_msg("Not enough coins", true);
            else open_create(false, true);
        } else if (id == ID_BUILD) {
            int hc = jl_house_cost();
            if (hc < 0) say_msg("All lots are built", true);
            else if (!jl_build_house()) say_msg("Not enough coins", true);
            else plat_save_now();
        } else if (id >= ID_ROW && id - ID_ROW < jl.nJerries) {
            select_jerry(&jl.jerries[id - ID_ROW], true);
        }
        break;

    case M_CREATE:
        if (id == ID_PREV || id == ID_NEXT) {
            U.nameIdx = (U.nameIdx + JL_NAME_COUNT + (id == ID_NEXT ? 1 : -1)) % JL_NAME_COUNT;
            snprintf(U.name, sizeof U.name, "%s", JL_NAMES[U.nameIdx]);
        } else if (id == ID_EDIT) go(M_NAME);
        else if (id >= ID_SKIN && id < ID_SKIN + SKIN_COUNT) U.skin = id - ID_SKIN;
        else if (id >= ID_PERS && id < ID_PERS + PERS_COUNT) U.pers = id - ID_PERS;
        else if (id == ID_GO) {
            if (jl.coins < jl_create_cost()) { say_msg("Not enough coins", true); break; }
            Jerry *j = jl_create_jerry(U.name, U.skin, U.pers);
            if (!j) { say_msg("No free house", true); break; }
            U.first = false;
            plat_save_now();
            select_jerry(j, false);
        }
        break;

    case M_NAME:
        {
            size_t n = strlen(U.name);
            if (id >= ID_KEY && id < ID_KEY + 128) {
                if (n < NAME_LEN - 1) { U.name[n] = (char)(id - ID_KEY); U.name[n + 1] = 0; }
            } else if (id == ID_SPACE) {
                if (n && n < NAME_LEN - 1 && U.name[n - 1] != ' ') { U.name[n] = ' '; U.name[n + 1] = 0; }
            } else if (id == ID_BKSP) {
                if (n) U.name[n - 1] = 0;
            } else if (id == ID_DONE) {
                while (strlen(U.name) && U.name[strlen(U.name) - 1] == ' ') U.name[strlen(U.name) - 1] = 0;
                if (!U.name[0]) snprintf(U.name, sizeof U.name, "%s", JL_NAMES[U.nameIdx]);
                go(M_CREATE);
            }
        }
        break;

    case M_RES:
        if (U.tab == 0 && id >= ID_ROW && id - ID_ROW < jl.nJerries) select_jerry(&jl.jerries[id - ID_ROW], true);
        break;

    case M_MENU:
        if (id == ID_SEASON) jl_set_season_mode((jl.seasonMode + 1) % 5);
        else if (id == ID_TIME) jl_set_time_mode((jl.timeMode + 1) % TIME_MODES);
        else if (id == ID_MUSIC) { jl.musicOn = !jl.musicOn; plat_set_music(jl.musicOn); }
        else if (id == ID_SAVE) plat_save_now();
        else if (id == ID_ERASE) { U.confirmKind = 1; go(M_CONFIRM); }
        break;

    case M_CONFIRM:
        if (id == ID_YES) {
            if (U.confirmKind == 0 && sel) {
                jl_move_out(sel);
                plat_save_now();
                go(M_MAP);
            } else if (U.confirmKind == 1) {
                jl_erase();
                plat_set_music(jl.musicOn);
                plat_save_now();
                open_create(true, false);
            }
        } else if (id == ID_NO) back();
        break;

    default: break;
    }
}

/* ================================================================ input */

static const Btn *btn_at(int x, int y) {
    for (int i = nbtn - 1; i >= 0; i--)
        if (x >= btns[i].x && x < btns[i].x + btns[i].w && y >= btns[i].y && y < btns[i].y + btns[i].h) return &btns[i];
    return NULL;
}

static void nav(int dir) {  // 0 up, 1 down, 2 left, 3 right
    U.focusVis = true;
    U.dirty = true;
    if (!nbtn) return;
    int cur = -1;
    for (int i = 0; i < nbtn; i++) if (btns[i].id == U.focusId) cur = i;
    if (cur < 0) { U.focusId = btns[0].id; return; }
    if (U.focusId >= ID_ROW && dir < 2) {  // step through list rows (they may be scrolled out of sight)
        int n = U.focusId - ID_ROW + (dir ? 1 : -1);
        if (n >= 0 && n < lst.count) U.focusId = ID_ROW + n;
        else if (dir == 0) { /* top row: fall through to the other widgets above */ }
        else return;
        if (n >= 0 && n < lst.count) return;
    }
    int cx = btns[cur].x + btns[cur].w / 2, cy = btns[cur].y + btns[cur].h / 2;
    int best = -1, bestScore = 1 << 30;
    for (int i = 0; i < nbtn; i++) {
        if (i == cur) continue;
        int dx = btns[i].x + btns[i].w / 2 - cx, dy = btns[i].y + btns[i].h / 2 - cy;
        int along = dir == 0 ? -dy : dir == 1 ? dy : dir == 2 ? -dx : dx;
        int perp = dir < 2 ? abs(dx) : abs(dy);
        if (along <= 2) continue;
        int score = along + perp * 2;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    if (best >= 0) U.focusId = btns[best].id;
}

static void touch_map(const UiInput *in) {
    int wx = clampi(in->tx, 0, MM_W - 1) * MM_SCALE + MM_SCALE / 2;
    int wy = clampi(in->ty, 0, MM_H - 1) * MM_SCALE + MM_SCALE / 2;
    U.mmWx = wx;
    U.mmWy = wy;
    center_cam_on((float)wx, (float)wy);
    U.follow = false;
}

static void drag_jerry_to(int wx, int wy) {
    Jerry *j = jl_jerry(U.dragJ);
    if (!j) return;
    float fx = (float)wx, fy = (float)wy;
    if (map_walk(fx, fy)) { j->x = fx; j->y = fy; }
    else if (map_walk(fx, j->y)) j->x = fx;   // slide along obstacles instead of sticking
    else if (map_walk(j->x, fy)) j->y = fy;
    j->tx = j->x;
    j->ty = j->y;
}

static void drop_jerry(void) {
    Jerry *j = jl_jerry(U.dragJ);
    if (j && !map_walk(j->x, j->y)) map_rand_spot(NULL, 0, &j->x, &j->y);
    if (j) { j->tx = j->x; j->ty = j->y; j->st = ST_IDLE; j->t = 0.5f; }
    U.dragJ = 0;
    jl.dragId = 0;
}

static void update_touch(const UiInput *in) {
    if (in->touchDown) {
        U.focusVis = false;
        U.downX = in->tx;
        U.downY = in->ty;
        U.lastY = in->ty;
        U.drag = false;
        U.dirty = true;
        if (U.mode == M_MAP && in->ty < MM_H) {
            U.mmPress = true;
            U.mmMoved = false;
            int wx = clampi(in->tx, 0, MM_W - 1) * MM_SCALE + MM_SCALE / 2;
            int wy = clampi(in->ty, 0, MM_H - 1) * MM_SCALE + MM_SCALE / 2;
            U.mmWx = wx;
            U.mmWy = wy;
            Hit h = find_hit((float)wx, (float)wy, 12);
            if (h.kind == HIT_JERRY) { // grab a resident: drag to move, tap to select
                U.dragJ = jl.jerries[h.index].id;
                jl.dragId = U.dragJ;
            } else touch_map(in);
        } else {
            const Btn *b = btn_at(in->tx, in->ty);
            U.pressed = b ? b->id : 0;
            U.listTouch = in->tx >= lst.x && in->tx < lst.x + lst.w && in->ty >= lst.y && in->ty < lst.y + lst.h &&
                          (U.mode != M_MAP);
        }
    } else if (in->touchHeld) {
        if (U.mmPress) {
            if (abs(in->tx - U.downX) + abs(in->ty - U.downY) > 3) U.mmMoved = true;
            if (U.dragJ) {
                if (U.mmMoved) {
                    U.mmWx = clampi(in->tx, 0, MM_W - 1) * MM_SCALE + MM_SCALE / 2;
                    U.mmWy = clampi(in->ty, 0, MM_H - 1) * MM_SCALE + MM_SCALE / 2;
                    drag_jerry_to(U.mmWx, U.mmWy);
                }
            } else touch_map(in);
            U.dirty = true;
        } else if (U.listTouch) {
            if (abs(in->ty - U.downY) > 5) {
                U.drag = true;
                U.pressed = 0;
            }
            if (U.drag) { U.scroll -= in->ty - U.lastY; U.dirty = true; }
            U.lastY = in->ty;
        }
    }
    if (in->touchUp) {
        if (U.mmPress) {
            U.mmPress = false;
            if (U.dragJ) {
                int id = U.dragJ;
                bool moved = U.mmMoved;
                drop_jerry();
                if (!moved) select_jerry(jl_jerry(id), false);
            } else if (!U.mmMoved) {
                Hit h = find_hit((float)U.mmWx, (float)U.mmWy, 12);
                interact(h, (float)U.mmWx, (float)U.mmWy);
            }
        } else {
            const Btn *b = btn_at(in->tx, in->ty);
            int pressed = U.pressed;
            U.pressed = 0;
            U.listTouch = false;
            if (pressed && b && b->id == pressed && !U.drag) act(pressed);
        }
        U.drag = false;
        U.dirty = true;
    }
}

void ui_update(const UiInput *in) {
    if (U.msgT > 0 && --U.msgT == 0) U.dirty = true;
    if ((U.mode == M_JERRY || U.mode == M_PICK || U.mode == M_HANG) && !jl_selected()) go(M_MAP);

    if (in->down & UK_SELECT) { jl.musicOn = !jl.musicOn; plat_set_music(jl.musicOn); U.dirty = true; }

    update_touch(in);

    if (U.mode == M_MAP) {
        int step = (in->held & UK_Y) ? 6 : 3;
        int dx = ((in->held & UK_RIGHT) ? 1 : 0) - ((in->held & UK_LEFT) ? 1 : 0);
        int dy = ((in->held & UK_DOWN) ? 1 : 0) - ((in->held & UK_UP) ? 1 : 0);
        if (dx || dy) {
            U.camX = clampi(U.camX + dx * step, 0, PW - VIEW_W);
            U.camY = clampi(U.camY + dy * step, 0, PH - VIEW_H);
            U.follow = false;
            U.dirty = true;
        }
        if (in->down & UK_A) {
            float wx = (float)(U.camX + VIEW_W / 2), wy = (float)(U.camY + VIEW_H / 2);
            interact(find_hit(wx, wy, 0), wx, wy);
        }
        if (in->down & UK_B) { jl.selected = 0; U.dirty = true; }
        if (in->down & UK_L) act(ID_NEWS);
        if (in->down & UK_R) act(ID_ITEMS);
        if (in->down & UK_X) act(ID_RES);
        if (in->down & UK_START) act(ID_MENU);
    } else {
        if (in->repeat & UK_UP) nav(0);
        if (in->repeat & UK_DOWN) nav(1);
        if (in->repeat & UK_LEFT) nav(2);
        if (in->repeat & UK_RIGHT) nav(3);
        if ((in->down & UK_A) && U.focusVis && U.focusId) act(U.focusId);
        else if ((in->down & UK_A) && !U.focusVis) { U.focusVis = true; U.dirty = true; if (!U.focusId && nbtn) U.focusId = btns[0].id; }
        if (in->down & UK_B) back();
        int nt = tab_count();
        if (nt) {
            if (in->down & UK_L) act(ID_TAB + (U.tab + nt - 1) % nt);
            if (in->down & UK_R) act(ID_TAB + (U.tab + 1) % nt);
        }
        if (in->down & UK_START) { go(M_MAP); }
    }

    // camera follows the selected resident until the player scrolls away
    const Jerry *s = jl_selected();
    if (s && U.follow && U.mode != M_MAP + 100) {
        int tx = clampi((int)s->x - VIEW_W / 2, 0, PW - VIEW_W), ty = clampi((int)s->y - VIEW_H / 2, 0, PH - VIEW_H);
        U.camX += (tx - U.camX) / 6 + ((tx - U.camX) % 6 ? ((tx > U.camX) - (tx < U.camX)) : 0);
        U.camY += (ty - U.camY) / 6 + ((ty - U.camY) % 6 ? ((ty > U.camY) - (ty < U.camY)) : 0);
        U.camX = clampi(U.camX, 0, PW - VIEW_W);
        U.camY = clampi(U.camY, 0, PH - VIEW_H);
    }
}

/* ================================================================ top screen */

void ui_draw_top(px *fb) {
    Canvas c;
    gfx_begin(&c, fb);
    world_prepare();
    world_draw(&c, U.camX, U.camY);

    // HUD
    fx_shade_rect(&c, 0, 0, SCREEN_W, 12);
    char a[48];
    fx_bits(&c, 4, 3, B_COIN, 5);
    snprintf(a, sizeof a, "%d", jl.coins);
    T8(&c, 13, 3, a, C_GOLD);
    daycycle_time_string(a, sizeof a);
    char b[64];
    snprintf(b, sizeof b, "%s %s", SEASON_NAMES[jl.season & 3], a);
    gfx_text_centered(&c, &fontsm, 128, 3, b, C_WHITE, 1);
    snprintf(a, sizeof a, "%d resident%s", jl.nJerries, jl.nJerries == 1 ? "" : "s");
    gfx_text_right(&c, &fontsm, 252, 3, a, C_WHITE, 1);

    // crosshair + what A would do
    if (U.mode == M_MAP) {
        px k = RGB8(0, 0, 0), w = C_WHITE;
        for (int pass = 0; pass < 2; pass++) {
            int o = pass == 0 ? 1 : 0;
            px col = pass == 0 ? k : w;
            gfx_fill(&c, 128 - 6 + o, 96 + o, 4, 1, col);
            gfx_fill(&c, 128 + 3 + o, 96 + o, 4, 1, col);
            gfx_fill(&c, 128 + o, 96 - 6 + o, 1, 4, col);
            gfx_fill(&c, 128 + o, 96 + 3 + o, 1, 4, col);
        }
        char buf[48];
        const char *hint = hint_for(find_hit((float)(U.camX + 128), (float)(U.camY + 96), 0), buf, sizeof buf);
        if (hint) {
            int tw = gfx_text_width(&fontsm, hint);
            fx_shade_rect(&c, 128 - tw / 2 - 3, 14, tw + 6, 11);
            gfx_text_centered(&c, &fontsm, 128, 16, hint, C_GOLD, 1);
        }
    }

    // toasts (newest information the simulation logged)
    int row = 0;
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (jl.toastTtl[i] <= 0) continue;
        int w = gfx_text_width(&fontsm, jl.toast[i]);
        int y = SCREEN_H - 11 * (row + 1);
        fx_shade_rect(&c, 1, y - 1, w + 6, 10);
        T8(&c, 4, y + 1, jl.toast[i], C_WHITE);
        row++;
    }
}

/* ================================================================ bottom screen */

static void draw_minimap(Canvas *c) {
    memcpy(c->buf, world_minimap(), (size_t)MM_W * MM_H * sizeof(px));
    for (int i = 0; i < MAX_SPARKS; i++)
        if (jl.sparks[i].on) gfx_fill(c, (int)jl.sparks[i].x / MM_SCALE - 1, (int)jl.sparks[i].y / MM_SCALE - 1, 2, 2, RGB8(255, 248, 176));
    for (int i = 0; i < TOY_COUNT; i++)
        if (jl.toys[i].on) gfx_fill(c, (int)jl.toys[i].x / MM_SCALE - 1, (int)jl.toys[i].y / MM_SCALE - 1, 2, 2, C_CYAN);
    for (int i = 0; i < jl.nJerries; i++) {
        const Jerry *j = &jl.jerries[i];
        if (j->inside) continue;
        int x = (int)j->x / MM_SCALE, y = (int)j->y / MM_SCALE;
        bool s = j->id == jl.selected;
        if (s) gfx_frame(c, x - 3, y - 4, 7, 7, C_WHITE);
        gfx_fill(c, x - 1, y - 2, 3, 3, RGB8(255, 216, 74));
        gfx_frame(c, x - 2, y - 3, 5, 5, RGB8(40, 20, 0));
    }
    gfx_frame(c, U.camX / MM_SCALE, U.camY / MM_SCALE, VIEW_W / MM_SCALE, VIEW_H / MM_SCALE, C_WHITE);
    gfx_frame(c, 0, 0, MM_W, MM_H, RGB8(0, 0, 0));
}

static void screen_map(Canvas *c) {
    draw_minimap(c);
    gfx_fill(c, 0, MM_H, SCREEN_W, SCREEN_H - MM_H, RGB8(0, 0, 0));
    gfx_tile(c, tex_stone, 0, MM_H + 1, SCREEN_W, SCREEN_H - MM_H - 1);
    static const struct { int id; const char *l; } B[4] = {{ID_NEWS, "News"}, {ID_RES, "Jerries"}, {ID_ITEMS, "Items"}, {ID_MENU, "Menu"}};
    for (int i = 0; i < 4; i++) button(c, B[i].id, 4 + i * 63, 164, 60, 24, B[i].l, BS_NORMAL, true);
}

static void screen_jerry(Canvas *c) {
    const Jerry *j = jl_selected();
    if (!j) return;
    char a[96];
    panel(c, 0, 0, SCREEN_W, 17);
    T10(c, 6, 4, j->name, C_GOLD);
    snprintf(a, sizeof a, "LV %d  %s", j->lvl, jl_pers_label(j->pers));
    gfx_text_right(c, &font10, 250, 4, a, C_WHITE, 1);

    gfx_bevel_panel(c, 6, 22, 36, 36, RGB8(26, 26, 26), RGB8(0, 0, 0), RGB8(85, 85, 85));
    spr_draw(c, SKINS[j->skin].spr, 6 + 2 + 14, 22 + 2 + 30, 2, false, false);

    static const char *const L[3] = {"Hunger", "Joy", "XP"};
    float v[3] = {j->hunger, j->joy, (float)j->xp / (float)j->lvl};
    px col[3] = {RGB8(232, 163, 58), RGB8(255, 122, 168), RGB8(111, 214, 255)};
    for (int i = 0; i < 3; i++) {
        int y = 22 + i * 12;
        T8(c, 50, y + 1, L[i], C_DIM);
        bar(c, 92, y, 120, 8, v[i], col[i]);
        if (i < 2) { snprintf(a, sizeof a, "%d", (int)v[i]); gfx_text_right(c, &fontsm, 250, y + 1, a, C_WHITE, 1); }
    }

    T8(c, 6, 66, "Loves", C_DIM);
    spr_icon(c, FOODS[j->fav].spr, 40, 62);
    T8(c, 60, 66, FOODS[j->fav].name, C_WHITE);
    spr_icon(c, GIFTS[j->favGift].spr, 40, 80);
    T8(c, 60, 84, GIFTS[j->favGift].name, C_WHITE);

    panel(c, 4, 99, 248, 26);
    char text[96];
    int code = jl_concern_text(j, text, sizeof text);
    if (code) {
        if (code == 4) spr_icon(c, FOODS[j->conId].spr, 8, 104);
        else if (code == 5) spr_icon(c, GIFTS[j->conId].spr, 8, 104);
        else fx_concern_icon(c, code, 16, 112);
        char lines[3][48];
        int n = wrap(text, 41, lines, 2);
        for (int i = 0; i < n; i++) T8(c, 28, 103 + i * 10, lines[i], C_GOLD);
    } else {
        T8(c, 10, 108, "Nothing on their mind.", C_DIM);
    }

    const Jerry *lover = j->lover ? jl_jerry(j->lover) : NULL;
    if (lover) { fx_heart(c, 9, 131, C_PINK); snprintf(a, sizeof a, "Partner: %s", lover->name); T8(c, 16, 128, a, C_PINK); }
    int fr[3], fv[3], nf = 0;
    for (int i = 0; i < jl.nJerries; i++) {
        const Jerry *o = &jl.jerries[i];
        if (o == j) continue;
        int rv = (int)jl_rel(j->id, o->id);
        if (rv < 8) continue;
        int k = nf < 3 ? nf++ : 2;
        if (nf == 3 && k == 2 && fv[2] >= rv && i && nf == 3) { if (fv[2] >= rv) continue; }
        while (k > 0 && fv[k - 1] < rv) { fr[k] = fr[k - 1]; fv[k] = fv[k - 1]; k--; }
        fr[k] = i; fv[k] = rv;
    }
    if (nf) {
        strcpy(a, "Friends: ");
        for (int i = 0; i < nf; i++) { strcat(a, jl.jerries[fr[i]].name); if (i + 1 < nf) strcat(a, ", "); }
        T8(c, 6, 138, a, C_DIM);
    }

    static const struct { int id; const char *l; } B[6] = {{ID_FEED, "Feed"}, {ID_GIFT, "Gift"}, {ID_TALK, "Talk"},
                                                           {ID_HANG, "Hang out"}, {ID_MOVEOUT, "Move out"}, {ID_CLOSE, "Close"}};
    for (int i = 0; i < 6; i++) {
        int st = B[i].id == ID_MOVEOUT || B[i].id == ID_CLOSE ? BS_CANCEL : (i < 3 ? BS_OK : BS_NORMAL);
        button(c, B[i].id, 4 + (i % 3) * 84, i < 3 ? 148 : 169, 80, 20, B[i].l, st, true);
    }
}

static void screen_pick(Canvas *c) {
    const Jerry *j = jl_selected();
    if (!j) return;
    int items[64];
    int n = U.pickKind == 0 ? owned_foods(items) : owned_gifts(items);
    char a[64];
    snprintf(a, sizeof a, "%s %s", U.pickKind == 0 ? "Feed" : "Gift for", j->name);
    header(c, a, false);
    list_begin(c, 4, 20, 248, 142, 20, n);
    for (int i = 0; i < n; i++) {
        int it = items[i];
        bool toy = U.pickKind == 1 && it >= GIFT_COUNT;
        const char *name; int spr, v = V_OK, count; bool wanted = false;
        if (U.pickKind == 0) {
            name = FOODS[it].name; spr = FOODS[it].spr; count = jl.foods[it]; v = jl_food_verdict(j, it);
            wanted = j->conType == CON_WANT_FOOD && j->conId == it;
        } else if (toy) {
            name = TOYS[it - GIFT_COUNT].name; spr = TOYS[it - GIFT_COUNT].spr; count = 0;
        } else {
            name = GIFTS[it].name; spr = GIFTS[it].spr; count = jl.gifts[it]; v = jl_gift_verdict(j, it);
            wanted = j->conType == CON_WANT_GIFT && j->conId == it;
        }
        bool vis = wanted ? row_bg(c, i, ROW_GOLD) : row_bg(c, i, ROW_NORMAL);
        if (!vis) continue;
        int y = list_row_y(i);
        spr_icon(c, spr, 8, y + 2);
        T8(c, 28, y + 3, name, C_WHITE);
        if (toy) T8(c, 28, y + 11, "Toy - play together", C_CYAN);
        else {
            snprintf(a, sizeof a, "X%d", count);
            T8(c, 28, y + 11, a, C_DIM);
            const char *t; px col;
            verdict_tag(v, &t, &col);
            if (wanted) { t = "Wanted!"; col = C_GOLD; }
            gfx_text_right(c, &fontsm, 232, y + 11, t, col, 1);
        }
    }
    list_end(c, U.pickKind == 0 ? "Nothing in your bag. visit the Cafe!" : "Nothing in your bag. visit the Toy Store!");
    footer(c, "Tap an item");
}

static void screen_hang(Canvas *c) {
    const Jerry *j = jl_selected();
    if (!j) return;
    header(c, "Hang out with...", false);
    const Jerry *others[MAX_JERRIES];
    int n = 0;
    for (int i = 0; i < jl.nJerries; i++) if (&jl.jerries[i] != j && !jl.jerries[i].inside) others[n++] = &jl.jerries[i];
    list_begin(c, 4, 20, 248, 142, 22, n);
    for (int i = 0; i < n; i++) {
        if (!row_bg(c, i, ROW_NORMAL)) continue;
        int y = list_row_y(i);
        spr_icon(c, SKINS[others[i]->skin].spr, 6, y + 3);
        T10(c, 28, y + 6, others[i]->name, C_WHITE);
        gfx_text_right(c, &fontsm, 232, y + 8, jl_rel_name(jl_rel(j->id, others[i]->id)), C_DIM, 1);
    }
    list_end(c, "Nobody else is around.");
    footer(c, "Tap who to visit");
}

static void screen_shop(Canvas *c) {
    int items[64];
    int n = shop_items(items);
    header(c, U.shopKind == 0 ? "Jerry Cafe" : "Jerry Toys", true);
    if (U.shopKind == 0) tabs(c, TASTE_TAB, 5, 19);
    else { static const char *const T[2] = {"Toys", "Gifts"}; tabs(c, T, 2, 19); }
    list_begin(c, 4, 36, 248, 126, 20, n);
    char a[48];
    for (int i = 0; i < n; i++) {
        int it = items[i];
        const char *name; int spr, price, own; bool owned = false;
        if (U.shopKind == 0) { name = FOODS[it].name; spr = FOODS[it].spr; price = FOODS[it].price; own = jl.foods[it]; }
        else if (U.tab == 0) { name = TOYS[it].name; spr = TOYS[it].spr; price = TOYS[it].price; own = jl.toysOwned[it]; owned = own; }
        else { name = GIFTS[it].name; spr = GIFTS[it].spr; price = GIFTS[it].price; own = jl.gifts[it]; }
        bool afford = jl.coins >= price && !owned;
        bool vis = owned ? row_bg(c, i, ROW_GOOD) : afford ? row_bg(c, i, ROW_NORMAL) : row_bg(c, i, ROW_BAD);
        if (!vis) continue;
        int y = list_row_y(i);
        spr_icon(c, spr, 8, y + 2);
        T8(c, 28, y + 3, name, afford || owned ? C_WHITE : C_DIM);
        if (owned) T8(c, 28, y + 11, "Owned", C_GREEN);
        else { snprintf(a, sizeof a, "X%d", own); T8(c, 28, y + 11, a, C_DIM); }
        if (!owned) {
            snprintf(a, sizeof a, "%d", price);
            int w = gfx_text_width(&font10, a);
            T10(c, 232 - w, y + 5, a, afford ? C_GREEN : C_RED);
            fx_bits(c, 232 - w - 9, y + 7, B_COIN, 5);
        }
    }
    list_end(c, NULL);
    footer(c, U.shopKind == 0 && U.tab == 4 ? "Prank food. Jerries hate it." : "Tap to buy");
}

static void card(Canvas *c, int x, int y, int w, int h, const char *title, const char *l1, const char *l2) {
    panel(c, x, y, w, h);
    T8(c, x + 6, y + 5, title, C_GOLD);
    T8(c, x + 6, y + 15, l1, C_DIM);
    T8(c, x + 6, y + 24, l2, C_DIM);
}

static void screen_hall(Canvas *c) {
    header(c, "Town hall", true);
    char a[48], b[48];
    int cost = jl_create_cost();
    if (cost) snprintf(a, sizeof a, "Cost: %d coins", cost); else snprintf(a, sizeof a, "Cost: free");
    snprintf(b, sizeof b, "Free homes: %d", jl_free_houses());
    card(c, 4, 21, 122, 52, "Move in a Jerry", a, b);
    button(c, ID_CREATE, 10, 52, 110, 17, "Create", BS_OK, true);
    int hc = jl_house_cost();
    if (hc < 0) snprintf(a, sizeof a, "All lots built"); else snprintf(a, sizeof a, "Cost: %d coins", hc);
    snprintf(b, sizeof b, "Homes: %d/%d", jl.nHouses, MAX_HOUSES);
    card(c, 130, 21, 122, 52, "Build a house", a, b);
    button(c, ID_BUILD, 136, 52, 110, 17, "Build", BS_OK, true);
    T8(c, 6, 77, "Residents", C_GOLD);
    list_begin(c, 4, 87, 248, 75, 20, jl.nJerries);
    for (int i = 0; i < jl.nJerries; i++) {
        if (!row_bg(c, i, ROW_NORMAL)) continue;
        const Jerry *j = &jl.jerries[i];
        int y = list_row_y(i);
        spr_icon(c, SKINS[j->skin].spr, 6, y + 2);
        T8(c, 26, y + 6, j->name, C_WHITE);
        snprintf(a, sizeof a, "LV %d %s", j->lvl, jl_pers_label(j->pers));
        gfx_text_right(c, &fontsm, 190, y + 6, a, C_DIM, 1);
        T8(c, 198, y + 6, "Find", C_GREEN);
    }
    list_end(c, "Nobody lives here yet.");
    footer(c, NULL);
}

static void screen_create(Canvas *c) {
    header(c, U.first ? "Welcome to Jerry Life!" : "New resident", true);
    T8(c, 6, 26, "Name", C_DIM);
    button(c, ID_PREV, 40, 20, 22, 20, "", BS_NORMAL, true);
    fx_bits(c, 47 + (U.pressed == ID_PREV), 26 + (U.pressed == ID_PREV), B_LARR, 7);
    panel(c, 64, 20, 100, 20);
    gfx_text_centered(c, &font10, 114, 25, U.name, C_WHITE, 1);
    button(c, ID_NEXT, 166, 20, 22, 20, "", BS_NORMAL, true);
    fx_bits(c, 173 + (U.pressed == ID_NEXT), 26 + (U.pressed == ID_NEXT), B_RARR, 7);
    button(c, ID_EDIT, 192, 20, 60, 20, "Type", BS_NORMAL, true);

    T8(c, 6, 46, "Skin", C_DIM);
    for (int i = 0; i < SKIN_COUNT; i++) {
        int x = 3 + i * 36, y = 55;
        gfx_bevel_panel(c, x, y, 34, 46, RGB8(26, 26, 26), RGB8(0, 0, 0), RGB8(85, 85, 85));
        spr_draw(c, SKINS[i].spr, x + 16, y + 34, 2, false, false);
        gfx_text_centered(c, &fontsm, x + 17, y + 36, SKINS[i].name, i == U.skin ? C_GOLD : C_DIM, 1);
        reg(ID_SKIN + i, x, y, 34, 46, false);
        if (i == U.skin) { gfx_frame(c, x, y, 34, 46, C_GOLD); gfx_frame(c, x + 1, y + 1, 32, 44, RGB8(200, 130, 0)); }
        focus_ring(c, ID_SKIN + i, x, y, 34, 46);
    }
    T8(c, 6, 106, "Personality", C_DIM);
    for (int i = 0; i < PERS_COUNT; i++) {
        int x = 3 + i * 50;
        bool on = i == U.pers;
        bool pr = U.pressed == ID_PERS + i;
        gfx_nineslice(c, (on || pr) ? tex_button_active : tex_button, x, 117, 48, 20);
        gfx_text_centered(c, &fontsm, x + 24, 124 + (on ? 1 : 0), jl_pers_label(i), on ? C_GOLD : C_WHITE, 1);
        reg(ID_PERS + i, x, 117, 48, 20, false);
        focus_ring(c, ID_PERS + i, x, 117, 48, 20);
    }
    char a[64];
    int cost = jl_create_cost();
    if (cost) snprintf(a, sizeof a, "Cost: %d coins   Free homes: %d", cost, jl_free_houses());
    else snprintf(a, sizeof a, "First three residents are free   Homes: %d", jl_free_houses());
    T8(c, 6, 143, a, C_DIM);
    if (U.msgT > 0) T8(c, 6, 153, U.msg, C_GOLD);
    if (cost) snprintf(a, sizeof a, "MOVE IN (%d)", cost); else snprintf(a, sizeof a, "Move in");
    button(c, ID_GO, 4, 166, U.first ? 248 : 150, 24, a, BS_OK, true);
    if (!U.first) button(c, ID_BACK, 160, 166, 92, 24, "Cancel", BS_CANCEL, true);
}

static void screen_name(Canvas *c) {
    header(c, "Type a name", false);
    panel(c, 4, 22, 248, 24);
    char a[NAME_LEN + 2];
    snprintf(a, sizeof a, "%s%s", U.name, (jl.clock > 0 && ((int)(jl.clock * 2) & 1)) || U.pressed ? "_" : " ");
    gfx_text_centered(c, &font14, 128, 28, a, C_WHITE, 1);
    static const char *const R[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
    static const int X0[3] = {3, 15, 28};
    for (int r = 0; r < 3; r++)
        for (int k = 0; R[r][k]; k++) {
            char s[2] = {R[r][k], 0};
            button(c, ID_KEY + R[r][k], X0[r] + k * 25, 54 + r * 26, 24, 24, s, BS_NORMAL, true);
        }
    button(c, ID_BKSP, 28 + 7 * 25, 54 + 2 * 26, 49, 24, "Del", BS_CANCEL, true);
    button(c, ID_SPACE, 4, 54 + 3 * 26, 160, 24, "Space", BS_NORMAL, true);
    button(c, ID_DONE, 168, 54 + 3 * 26, 84, 24, "Done", BS_OK, true);
    T8(c, 6, 142, "Up to 12 letters", C_DIM);
}

static void screen_news(Canvas *c) {
    header(c, "Island news", false);
    char a[32];
    const struct { const char *l; int v; } S[4] = {{"Residents", jl.nJerries}, {"Friends", jl_friend_count()},
                                                    {"Couples", jl_couple_count()}, {"Coins", jl.coins}};
    for (int i = 0; i < 4; i++) {
        panel(c, 4 + i * 63, 20, 60, 22);
        gfx_text_centered(c, &fontsm, 34 + i * 63, 23, S[i].l, C_DIM, 0);
        snprintf(a, sizeof a, "%d", S[i].v);
        gfx_text_centered(c, &font10, 34 + i * 63, 32, a, C_WHITE, 1);
    }
    list_begin(c, 4, 46, 248, 116, 21, jl.nEvents);
    for (int i = 0; i < jl.nEvents; i++) {
        if (!row_bg(c, i, ROW_NORMAL)) continue;
        int y = list_row_y(i);
        time_t t = (time_t)jl.events[i].t;
        struct tm *lt = localtime(&t);
        if (lt) snprintf(a, sizeof a, "%02d:%02d", lt->tm_hour, lt->tm_min); else a[0] = 0;
        T8(c, 6, y + 2, a, C_DIM);
        char lines[2][48];
        int n = wrap(jl.events[i].m, 40, lines, 2);
        // first line shares the row with the time stamp
        T8(c, 36, y + 2, lines[0], C_WHITE);
        if (n > 1) T8(c, 36, y + 11, lines[1], C_WHITE);
    }
    list_end(c, "Quiet day on the island.");
    footer(c, "Scroll to read more");
}

static void screen_res(Canvas *c) {
    header(c, "Residents", false);
    static const char *const T[2] = {"Residents", "Friendships"};
    tabs(c, T, 2, 19);
    char a[64];
    if (U.tab == 0) {
        list_begin(c, 4, 36, 248, 126, 24, jl.nJerries);
        for (int i = 0; i < jl.nJerries; i++) {
            if (!row_bg(c, i, ROW_NORMAL)) continue;
            const Jerry *j = &jl.jerries[i];
            int y = list_row_y(i);
            spr_icon(c, SKINS[j->skin].spr, 6, y + 4);
            T10(c, 26, y + 3, j->name, C_WHITE);
            snprintf(a, sizeof a, "Joy %d  Hunger %d", (int)j->joy, (int)j->hunger);
            T8(c, 26, y + 14, a, C_DIM);
            snprintf(a, sizeof a, "LV %d %s", j->lvl, jl_pers_label(j->pers));
            gfx_text_right(c, &fontsm, 232, y + 5, a, C_WHITE, 1);
            if (j->lover) fx_heart(c, 238, y + 16, C_PINK);
        }
        list_end(c, "Nobody lives here yet.");
    } else {
        Pair p[MAX_REL];
        int n = friend_pairs(p, MAX_REL);
        list_begin(c, 4, 36, 248, 126, 22, n);
        for (int i = 0; i < n; i++) {
            if (!row_bg(c, i, ROW_NORMAL)) continue;
            int y = list_row_y(i);
            snprintf(a, sizeof a, "%s & %s", jl_jerry(p[i].a)->name, jl_jerry(p[i].b)->name);
            T8(c, 6, y + 3, a, C_WHITE);
            T8(c, 6, y + 12, jl_rel_name((float)p[i].v), C_PINK);
            bar(c, 130, y + 6, 100, 8, (float)p[i].v, C_PINK);
        }
        list_end(c, "No friendships yet. let them mingle!");
    }
    footer(c, "Tap a resident to select");
}

static void screen_items(Canvas *c) {
    header(c, "Items", true);
    static const char *const T[3] = {"Food", "Gifts", "Toys"};
    tabs(c, T, 3, 19);
    int items[64], n;
    if (U.tab == 0) n = owned_foods(items);
    else if (U.tab == 1) { n = 0; for (int i = 0; i < GIFT_COUNT; i++) if (jl.gifts[i]) items[n++] = i; }
    else { n = 0; for (int i = 0; i < TOY_COUNT; i++) if (jl.toysOwned[i]) items[n++] = i; }
    list_begin(c, 4, 36, 248, 126, 20, n);
    char a[16];
    for (int i = 0; i < n; i++) {
        if (!row_bg(c, i, ROW_NORMAL)) continue;
        int y = list_row_y(i), it = items[i];
        const char *name = U.tab == 0 ? FOODS[it].name : U.tab == 1 ? GIFTS[it].name : TOYS[it].name;
        int spr = U.tab == 0 ? FOODS[it].spr : U.tab == 1 ? GIFTS[it].spr : TOYS[it].spr;
        spr_icon(c, spr, 8, y + 2);
        T10(c, 28, y + 6, name, C_WHITE);
        if (U.tab < 2) { snprintf(a, sizeof a, "X%d", U.tab == 0 ? jl.foods[it] : jl.gifts[it]); gfx_text_right(c, &font10, 232, y + 6, a, C_GOLD, 1); }
    }
    list_end(c, U.tab == 0 ? "No food. visit the Cafe!" : U.tab == 1 ? "No gifts. visit the Toy Store!" : "No toys yet.");
    footer(c, NULL);
}

static void screen_menu(Canvas *c) {
    header(c, "Menu", false);
    char a[48];
    snprintf(a, sizeof a, "SEASON: %s", SEASON_MODE[jl.seasonMode % 5]);
    button(c, ID_SEASON, 6, 22, 244, 22, a, BS_NORMAL, true);
    snprintf(a, sizeof a, "TIME OF DAY: %s", TIME_MODE[jl.timeMode % TIME_MODES]);
    button(c, ID_TIME, 6, 48, 244, 22, a, BS_NORMAL, true);
    snprintf(a, sizeof a, "MUSIC: %s", jl.musicOn ? "On" : "Off");
    button(c, ID_MUSIC, 6, 74, 244, 22, a, BS_NORMAL, true);
    button(c, ID_SAVE, 6, 100, 244, 22, plat_storage_ok() ? "Save now" : "No sd card: saving off", BS_OK, plat_storage_ok());
    button(c, ID_ERASE, 6, 126, 244, 22, "Erase island", BS_CANCEL, true);
    const char *st = plat_save_status();
    if (st && st[0]) T8(c, 6, 152, st, C_DIM);
    footer(c, NULL);
}

static void screen_confirm(Canvas *c) {
    header(c, U.confirmKind == 0 ? "Move out?" : "Erase island?", false);
    const Jerry *j = jl_selected();
    char a[64];
    panel(c, 8, 30, 240, 100);
    if (U.confirmKind == 0 && j) {
        snprintf(a, sizeof a, "MOVE %s OUT?", j->name);
        gfx_text_centered(c, &font10, 128, 52, a, C_WHITE, 1);
        gfx_text_centered(c, &fontsm, 128, 72, "They will leave the island for good.", C_DIM, 1);
    } else {
        gfx_text_centered(c, &font10, 128, 52, "Erase everything?", C_RED, 1);
        gfx_text_centered(c, &fontsm, 128, 72, "All residents, items and coins are lost.", C_DIM, 1);
    }
    button(c, ID_YES, 4, 150, 120, 30, U.confirmKind == 0 ? "Move out" : "Erase", BS_OK, true);
    button(c, ID_NO, 132, 150, 120, 30, "Keep", BS_CANCEL, true);
}

void ui_draw_bottom(px *fb) {
    Canvas c;
    gfx_begin(&c, fb);
    nbtn = 0;
    lst.count = 0;
    lst.x = lst.y = lst.w = lst.h = 0;
    gfx_tile(&c, tex_stone, 0, 0, SCREEN_W, SCREEN_H);
    switch (U.mode) {
    case M_MAP: screen_map(&c); break;
    case M_JERRY: screen_jerry(&c); break;
    case M_PICK: screen_pick(&c); break;
    case M_HANG: screen_hang(&c); break;
    case M_SHOP: screen_shop(&c); break;
    case M_HALL: screen_hall(&c); break;
    case M_CREATE: screen_create(&c); break;
    case M_NAME: screen_name(&c); break;
    case M_NEWS: screen_news(&c); break;
    case M_RES: screen_res(&c); break;
    case M_ITEMS: screen_items(&c); break;
    case M_MENU: screen_menu(&c); break;
    case M_CONFIRM: screen_confirm(&c); break;
    }
    U.dirty = false;
}
