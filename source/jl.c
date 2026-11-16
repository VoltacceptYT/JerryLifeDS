#include "jl.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "atlasdata.h"
#include "daycycle.h"
#include "gfx.h"
#include "map.h"
#include "sprites.h"
#include "util.h"

JL jl;

const char *const JL_NAMES[16] = {"Jerry", "Jerrold", "Jeremy", "Jerri", "Gerald", "Jez", "Jerrick", "Geri",
                                  "Jeroen", "Jerrimiah", "Jezza", "Jerrilyn", "Gary", "Jerbo", "Jerrito", "Jeremiah"};

typedef struct {
    const char *label;
    float spd;
    uint8_t tastes[2], cats[2], badTaste, badCat;
    const char *lines[5];
} Pers;

static const Pers PERS[PERS_COUNT] = {
    {"Chill", 20, {TASTE_SAVORY, TASTE_DRINK}, {CAT_PLANT, CAT_FUN}, TASTE_SPICY, CAT_HERO,
     {"Nice day, huh.", "Just vibing.", "So relaxed...", "Want to nap later.", "The breeze is lovely."}},
    {"Energetic", 34, {TASTE_SWEET, TASTE_SPICY}, {CAT_HERO, CAT_FUN}, TASTE_SAVORY, CAT_PLANT,
     {"Let's GO!", "I have so much energy!", "Race you!", "Today is awesome!", "Can't sit still!"}},
    {"Grumpy", 22, {TASTE_SAVORY, TASTE_SPICY}, {CAT_MAGIC, CAT_HERO}, TASTE_SWEET, CAT_FUN,
     {"Hmph.", "Whatever.", "Too loud around here.", "Don't bother me.", "I suppose it's fine."}},
    {"Sweet", 26, {TASTE_SWEET, TASTE_DRINK}, {CAT_PET, CAT_BOX}, TASTE_SPICY, CAT_HERO,
     {"Hi friend!", "You're so nice!", "I love it here!", "Want a hug?", "Everyone is lovely!"}},
    {"Quirky", 28, {TASTE_DRINK, TASTE_SPICY}, {CAT_MAGIC, CAT_CHARM}, TASTE_SWEET, CAT_PET,
     {"Did you see that cloud?", "Beep boop.", "Jerry is life.", "I count things.", "Strange vibes today."}},
};

const char *jl_pers_label(int p) { return PERS[p].label; }

static bool pair_is(int a, int b, int x, int y) { return (a == x && b == y) || (a == y && b == x); }
static int compat(int a, int b) {
    if (pair_is(a, b, PERS_CHILL, PERS_SWEET) || pair_is(a, b, PERS_ENERGETIC, PERS_QUIRKY) ||
        pair_is(a, b, PERS_CHILL, PERS_GRUMPY) || pair_is(a, b, PERS_QUIRKY, PERS_SWEET))
        return 1;
    if (pair_is(a, b, PERS_ENERGETIC, PERS_GRUMPY) || pair_is(a, b, PERS_GRUMPY, PERS_SWEET) ||
        pair_is(a, b, PERS_CHILL, PERS_ENERGETIC))
        return -1;
    return 0;
}

static const char *pick_line(int pers) { return PERS[pers].lines[RI(0, 4)]; }

/* ---------------------------------------------------------------- helpers */

static int today_int(void) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    return lt ? (lt->tm_year + 1900) * 10000 + (lt->tm_mon + 1) * 100 + lt->tm_mday : 0;
}

int season_now(void) {
    if (jl.seasonMode != MODE_AUTO) return jl.seasonMode - 1;
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    int m = lt ? lt->tm_mon : 5;
    return m >= 2 && m <= 4 ? SEASON_SPRING : (m >= 5 && m <= 7 ? SEASON_SUMMER : (m >= 8 && m <= 10 ? SEASON_FALL : SEASON_WINTER));
}

Jerry *jl_jerry(int id) {
    if (id <= 0) return NULL;
    for (int i = 0; i < jl.nJerries; i++)
        if (jl.jerries[i].id == id) return &jl.jerries[i];
    return NULL;
}
Jerry *jl_selected(void) { return jl_jerry(jl.selected); }
int jl_jerry_index(const Jerry *j) { return (int)(j - jl.jerries); }

static Rel *rel_find(int a, int b) {
    if (a > b) { int t = a; a = b; b = t; }
    for (int i = 0; i < jl.nRel; i++)
        if (jl.rel[i].a == a && jl.rel[i].b == b) return &jl.rel[i];
    return NULL;
}
float jl_rel(int a, int b) {
    Rel *r = rel_find(a, b);
    return r ? r->v : 0;
}
static void rel_set(int a, int b, int v) {
    if (a > b) { int t = a; a = b; b = t; }
    Rel *r = rel_find(a, b);
    if (!r) {
        if (jl.nRel >= MAX_REL) return;
        r = &jl.rel[jl.nRel++];
        r->a = (uint16_t)a;
        r->b = (uint16_t)b;
    }
    r->v = (uint8_t)clampi(v, 0, 100);
}
static void rel_drop(int id) {
    int n = 0;
    for (int i = 0; i < jl.nRel; i++)
        if (jl.rel[i].a != id && jl.rel[i].b != id) jl.rel[n++] = jl.rel[i];
    jl.nRel = n;
}
const char *jl_rel_name(float v) {
    return v >= 85 ? "Besties" : v >= 55 ? "Good friends" : v >= 25 ? "Friends" : v >= 8 ? "Acquainted" : "Strangers";
}

static void log_event(const char *m) {
    int n = jl.nEvents < MAX_EVENTS ? jl.nEvents : MAX_EVENTS - 1;
    for (int i = n; i > 0; i--) jl.events[i] = jl.events[i - 1];
    jl.events[0].t = (uint32_t)time(NULL);
    snprintf(jl.events[0].m, EVENT_LEN, "%s", m);
    if (jl.nEvents < MAX_EVENTS) jl.nEvents++;
}

void jl_note(const char *fmt, ...) {
    char buf[EVENT_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    log_event(buf);
    int slot = 0;
    float least = 1e9f;
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (jl.toastTtl[i] <= 0) { slot = i; least = -1; break; }
        if (jl.toastTtl[i] < least) { least = jl.toastTtl[i]; slot = i; }
    }
    snprintf(jl.toast[slot], EVENT_LEN, "%s", buf);
    jl.toastTtl[slot] = 4.0f;
}

static void add_part(int kind, float x, float y, float vx, float vy, float l, uint16_t col) {
    for (int i = 0; i < MAX_PARTS; i++)
        if (!jl.parts[i].on) {
            jl.parts[i] = (Part){true, (uint8_t)kind, x, y, vx, vy, l, col, 0};
            return;
        }
}
static void hearts(const Jerry *j, int n) {
    for (int i = 0; i < n; i++) add_part(PK_HEART, j->x + R(-10, 10), j->y - 30, R(-8, 8), R(-30, -18), 1.4f + rnd01() * .4f, 0);
}
static void confetti(const Jerry *j) {
    static const uint16_t cols[4] = {RGB8(255, 216, 74), RGB8(255, 122, 168), RGB8(111, 214, 255), RGB8(141, 255, 122)};
    for (int i = 0; i < 14; i++) add_part(PK_CONF, j->x, j->y - 20, R(-60, 60), R(-90, -20), 1.4f, cols[RI(0, 3)]);
}
static void popup(float x, float y, uint16_t col, const char *fmt, ...) {
    for (int i = 0; i < MAX_POPUPS; i++)
        if (!jl.popups[i].on) {
            Popup *p = &jl.popups[i];
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(p->text, sizeof p->text, fmt, ap);
            va_end(ap);
            p->on = true;
            p->x = x;
            p->y = y;
            p->l = 1.8f;
            p->col = col;
            return;
        }
}
#define GOLD RGB8(255, 216, 74)

static void say(Jerry *j, const char *text, float ttl) {
    j->bubKind = BUB_TEXT;
    snprintf(j->bubText, sizeof j->bubText, "%s", text);
    j->bubTtl = ttl;
}
static void emote(Jerry *j, int e, float ttl) {
    j->bubKind = BUB_EMOTE;
    j->bubText[0] = (char)e;
    j->bubText[1] = 0;
    j->bubTtl = ttl;
}

/* delayed actions (the web game uses setTimeout) */
enum { DL_SAYLINE, DL_COUPLE, DL_REJECT };
typedef struct { bool on; float t; int kind, a, b, c; } Delayed;
static Delayed delayed[MAX_DELAYED];
static void delay(float t, int kind, int a, int b, int c) {
    for (int i = 0; i < MAX_DELAYED; i++)
        if (!delayed[i].on) { delayed[i] = (Delayed){true, t, kind, a, b, c}; return; }
}

/* ---------------------------------------------------------------- concerns / mood */

static void solve(Jerry *j, int bonus);
static void gain(Jerry *j, int joy, int xp);

static void check_concern(Jerry *j) {
    if (j->conType == CON_HUNGRY && j->hunger >= 60) solve(j, 0);
    else if (j->conType == CON_BORED && j->joy >= 60) solve(j, 0);
}

static void solve(Jerry *j, int bonus) {
    if (j->conType == CON_NONE) return;
    int type = j->conType;
    j->conType = CON_NONE;
    j->ccool = R(70, 150);
    int r = (type == CON_WANT_FOOD || type == CON_WANT_GIFT) ? 60 : 30;
    int coins = r + bonus;
    jl.coins += coins;
    sfx_play(SFX_COIN);
    popup(j->x, j->y - 44, GOLD, "+%d", coins);
    jl_note("%s's concern was solved! +%d coins", j->name, coins);
    gain(j, 6, 25);
    hearts(j, 4);
}

static void gain(Jerry *j, int joy, int xp) {
    j->joy = clampf(j->joy + (float)joy, 0, 100);
    if (xp > 0) {
        j->xp += xp;
        while (j->xp >= 100 * j->lvl) {
            j->xp -= 100 * j->lvl;
            j->lvl++;
            int c = 40 * j->lvl;
            jl.coins += c;
            sfx_play(SFX_LEVEL);
            jl_note("%s reached happiness level %d! +%d coins", j->name, j->lvl, c);
            emote(j, EM_STAR, 3);
            confetti(j);
        }
    }
    check_concern(j);
}

static void add_concern(Jerry *j) {
    float r = rnd01();
    if (j->hunger < 38) { j->conType = CON_HUNGRY; return; }
    if (j->joy < 38) { j->conType = CON_BORED; return; }
    if (r < .45f) { j->conType = CON_WANT_FOOD; j->conId = j->fav; }
    else if (r < .8f) { j->conType = CON_WANT_GIFT; j->conId = j->favGift; }
    else if (jl.nJerries > 1) j->conType = CON_LONELY;
}

int jl_concern_text(const Jerry *j, char *out, size_t n) {
    out[0] = 0;
    switch (j->conType) {
    case CON_HUNGRY: snprintf(out, n, "I'm hungry!"); return 1;
    case CON_BORED: snprintf(out, n, "I'm bored... a gift or chat helps."); return 2;
    case CON_LONELY: snprintf(out, n, "I want to hang out with someone."); return 3;
    case CON_WANT_FOOD: snprintf(out, n, "I'd love some %s!", FOODS[j->conId].name); return 4;
    case CON_WANT_GIFT: snprintf(out, n, "I've been wanting a %s!", GIFTS[j->conId].name); return 5;
    default: return 0;
    }
}

/* ---------------------------------------------------------------- feeding, gifts, talking */

typedef struct { int v, joy; const char *line; } Verdict;

static Verdict taste_verdict(const Jerry *j, const FoodDef *f, int idx) {
    const Pers *p = &PERS[j->pers];
    if (f->taste == TASTE_GROSS) {
        static const char *const L[4] = {"BLEGH!", "That's rotten!", "Why?!", "I'm going to be sick..."};
        return (Verdict){V_GROSS, -14, L[RI(0, 3)]};
    }
    if (idx == j->fav) {
        static const char *const L[3] = {"My FAVORITE!!", "I LOVE this!", "Best. Day. Ever!"};
        return (Verdict){V_LOVE, 28, L[RI(0, 2)]};
    }
    if (f->taste == p->tastes[0] || f->taste == p->tastes[1]) {
        static const char *const L[3] = {"Yum!", "Delicious!", "Mmm, so good!"};
        return (Verdict){V_LIKE, 15, L[RI(0, 2)]};
    }
    if (f->taste == p->badTaste) {
        static const char *const L[3] = {"Not my thing...", "Ehh.", "I'll eat it, I guess."};
        return (Verdict){V_DISLIKE, -3, L[RI(0, 2)]};
    }
    static const char *const L[3] = {"Tasty.", "Thanks!", "Not bad."};
    return (Verdict){V_OK, 7, L[RI(0, 2)]};
}

static Verdict gift_verdict(const Jerry *j, const GiftDef *g, int idx) {
    const Pers *p = &PERS[j->pers];
    if (idx == j->favGift) {
        static const char *const L[3] = {"This is EXACTLY what I wanted!", "I'm crying!", "You remembered!"};
        return (Verdict){V_LOVE, 36, L[RI(0, 2)]};
    }
    if (g->cat == p->cats[0] || g->cat == p->cats[1]) {
        static const char *const L[3] = {"I love it!", "So cool!", "Thank you!!"};
        return (Verdict){V_LIKE, 22, L[RI(0, 2)]};
    }
    if (g->cat == p->badCat) {
        static const char *const L[3] = {"Um... thanks?", "Not really me.", "Oh. A gift."};
        return (Verdict){V_DISLIKE, -2, L[RI(0, 2)]};
    }
    static const char *const L[3] = {"Aww, thanks!", "Neat!", "For me?"};
    return (Verdict){V_OK, 12, L[RI(0, 2)]};
}

int jl_food_verdict(const Jerry *j, int food) { return taste_verdict(j, &FOODS[food], food).v; }
int jl_gift_verdict(const Jerry *j, int gift) { return gift_verdict(j, &GIFTS[gift], gift).v; }

bool jl_feed(Jerry *j, int id) {
    if (id < 0 || id >= FOOD_COUNT || !jl.foods[id]) return false;
    const FoodDef *f = &FOODS[id];
    if (j->hunger > 93 && f->taste != TASTE_GROSS) {
        say(j, "I'm stuffed!", 2.5f);
        sfx_play(SFX_BAD);
        return false;
    }
    jl.foods[id]--;
    Verdict v = taste_verdict(j, f, id);
    j->hunger = clampf(j->hunger + (f->taste == TASTE_GROSS ? 3 : clampf(14 + f->price * .4f, 14, 42)), 0, 100);
    say(j, v.line, 3);
    j->st = ST_EAT;
    j->t = 1.6f;
    j->held = -1;
    if (v.v == V_LOVE) { hearts(j, 6); sfx_play(SFX_HEART); }
    else if (v.v == V_LIKE) { hearts(j, 2); sfx_play(SFX_POP); }
    else if (v.v == V_GROSS) { sfx_play(SFX_BAD); add_part(PK_ANGRY, j->x, j->y - 34, 0, -12, 1.5f, 0); }
    else sfx_play(SFX_POP);
    gain(j, v.joy, maxi(0, v.joy) + 4);
    if (j->conType == CON_WANT_FOOD && j->conId == id) solve(j, 40);
    return true;
}

bool jl_give(Jerry *j, int id) {
    if (id < 0 || id >= GIFT_COUNT || jl.gifts[id] == 0) return false;
    const GiftDef *g = &GIFTS[id];
    jl.gifts[id]--;
    Verdict v = gift_verdict(j, g, id);
    say(j, v.line, 3.4f);
    j->st = ST_EAT;
    j->t = 1.4f;
    j->held = id;
    if (v.v == V_LOVE) { hearts(j, 7); sfx_play(SFX_HEART); }
    else if (v.v == V_LIKE) { hearts(j, 3); sfx_play(SFX_POP); }
    else sfx_play(SFX_POP);
    int extra = (int)floorf(g->price / 40.0f + 0.5f);
    if (g->cat == CAT_CHARM) {
        j->charm = mini(4, j->charm + 1);
        popup(j->x, j->y - 50, RGB8(153, 204, 255), "Charm +1");
    }
    if (g->cat == CAT_BOX) {
        int c = RI((int)floorf(g->price * .3f + .5f), (int)floorf(g->price * 1.2f + .5f));
        jl.coins += c;
        popup(j->x, j->y - 50, GOLD, "+%d coins", c);
        sfx_play(SFX_COIN);
    }
    gain(j, v.joy + extra, maxi(0, v.joy) + extra + 6);
    if (j->conType == CON_WANT_GIFT && j->conId == id) solve(j, 60);
    return true;
}

bool jl_give_toy(Jerry *j, int toy) {
    if (toy < 0 || toy >= TOY_COUNT || !jl.toysOwned[toy]) return false;
    ToyW *t = &jl.toys[toy];
    if (t->on) { t->x = j->x + 10; t->y = j->y + 2; t->vx = t->vy = 0; }
    static const char *const L[3] = {"A toy! Let's play!", "Yay, playtime!", "Whee!"};
    say(j, L[RI(0, 2)], 3);
    hearts(j, 3);
    sfx_play(SFX_HEART);
    j->st = ST_PLAY;
    j->t = 6;
    j->toy = toy;
    bool fun = PERS[j->pers].cats[0] == CAT_FUN || PERS[j->pers].cats[1] == CAT_FUN;
    gain(j, fun ? 24 : 16, 16);
    return true;
}

void jl_talk(Jerry *j) {
    if (j->talkCd > 0) { say(j, "...", 1.5f); return; }
    j->talkCd = 6;
    j->st = ST_TALK;
    j->t = 2;
    say(j, pick_line(j->pers), 3);
    gain(j, 3, 4);
    sfx_play(SFX_POP);
    if (j->angry) { j->angry = false; j->ang = 0; say(j, "Okay... I feel better.", 3); }
}

/* ---------------------------------------------------------------- shopping & town */

bool jl_buy_food(int i) {
    if (i < 0 || i >= FOOD_COUNT || jl.coins < FOODS[i].price || jl.foods[i] >= 99) return false;
    jl.coins -= FOODS[i].price;
    jl.foods[i]++;
    sfx_play(SFX_COIN);
    return true;
}
bool jl_buy_gift(int i) {
    if (i < 0 || i >= GIFT_COUNT || jl.coins < GIFTS[i].price || jl.gifts[i] >= 99) return false;
    jl.coins -= GIFTS[i].price;
    jl.gifts[i]++;
    sfx_play(SFX_COIN);
    return true;
}

static void spawn_toy(int id) {
    float c[2] = {512, 340}, x, y;
    map_rand_spot(c, 120, &x, &y);
    jl.toys[id] = (ToyW){true, x, y, 0, 0, 0, 0, 0};
}

bool jl_buy_toy(int i) {
    if (i < 0 || i >= TOY_COUNT || jl.toysOwned[i] || jl.coins < TOYS[i].price) return false;
    jl.coins -= TOYS[i].price;
    jl.toysOwned[i] = 1;
    spawn_toy(i);
    sfx_play(SFX_COIN);
    return true;
}
int jl_toy_count(void) {
    int n = 0;
    for (int i = 0; i < TOY_COUNT; i++) n += jl.toysOwned[i];
    return n;
}

int jl_create_cost(void) { return jl.nJerries < 3 ? 0 : 100 * (jl.nJerries - 2); }
int jl_house_cost(void) { return jl.nHouses >= MAX_HOUSES ? -1 : HOUSE_COST[jl.nHouses]; }
int jl_free_houses(void) {
    int n = 0;
    for (int i = 0; i < jl.nHouses; i++) n += jl.houses[i].owner == 0;
    return n;
}
static House *free_house(void) {
    for (int i = 0; i < jl.nHouses; i++)
        if (jl.houses[i].owner == 0) return &jl.houses[i];
    return NULL;
}

static void spawn_jerry(Jerry *j) {
    int bi = -1;
    for (int i = 0; i < jl.nHouses; i++)
        if (jl.houses[i].owner == j->id) bi = map_building_for_slot(jl.houses[i].slot);
    if (bi >= 0) {
        j->x = (float)map_buildings[bi].x;
        j->y = (float)map_buildings[bi].y - 2;
    } else map_rand_spot(NULL, 0, &j->x, &j->y);
    j->tx = j->x;
    j->ty = j->y;
}

static void reset_runtime(Jerry *j) {
    j->st = ST_IDLE;
    j->t = 1;
    j->dir = 1;
    j->bubKind = BUB_NONE;
    j->bubTtl = 0;
    j->angry = false;
    j->ang = j->talkCd = 0;
    j->socCd = R(2, 8);
    j->goToId = 0;
    j->goToT = 0;
    j->inside = j->sleepForce = false;
    j->toy = -1;
}

Jerry *jl_create_jerry(const char *name, int skin, int pers) {
    House *h = free_house();
    int cost = jl_create_cost();
    if (!h || jl.coins < cost || jl.nJerries >= MAX_JERRIES) return NULL;
    Jerry *j = &jl.jerries[jl.nJerries++];
    memset(j, 0, sizeof *j);
    j->used = true;
    j->id = jl.nextId++;
    snprintf(j->name, NAME_LEN, "%s", name && name[0] ? name : JL_NAMES[RI(0, JL_NAME_COUNT - 1)]);
    for (int i = 0; i < jl.nJerries - 1; i++) // names stay unique
        if (strcmp(jl.jerries[i].name, j->name) == 0) {
            char base[10];
            snprintf(base, sizeof base, "%.9s", j->name);
            snprintf(j->name, NAME_LEN, "%s%d", base, clampi(RI(2, 99), 2, 99));
            break;
        }
    j->skin = (uint8_t)clampi(skin, 0, SKIN_COUNT - 1);
    j->pers = (uint8_t)clampi(pers, 0, PERS_COUNT - 1);
    j->hunger = 70;
    j->joy = 70;
    j->lvl = 1;
    j->held = -1;
    j->ccool = 60;
    const Pers *p = &PERS[j->pers];
    int pool[FOOD_COUNT], n = 0;
    for (int i = 0; i < FOOD_COUNT; i++)
        if (FOODS[i].taste != TASTE_GROSS && (FOODS[i].taste == p->tastes[0] || FOODS[i].taste == p->tastes[1])) pool[n++] = i;
    j->fav = n ? pool[RI(0, n - 1)] : 0;
    n = 0;
    for (int i = 0; i < GIFT_COUNT; i++)
        if (GIFTS[i].cat == p->cats[0] || GIFTS[i].cat == p->cats[1]) pool[n++] = i;
    j->favGift = n ? pool[RI(0, n - 1)] : 0;
    j->home = h->slot;
    h->owner = j->id;
    reset_runtime(j);
    jl.coins -= cost;
    spawn_jerry(j);
    jl_note("%s moved into the island!", j->name);
    jl.selected = j->id;
    sfx_play(SFX_LEVEL);
    return j;
}

bool jl_build_house(void) {
    int hc = jl_house_cost();
    if (hc < 0 || jl.coins < hc) return false;
    jl.coins -= hc;
    jl.houses[jl.nHouses] = (House){jl.nHouses, RI(1, 6), 0};
    jl.nHouses++;
    map_layout(jl.season);
    jl_note("A new house was built!");
    sfx_play(SFX_COIN);
    return true;
}

void jl_move_out(Jerry *j) {
    for (int i = 0; i < jl.nHouses; i++)
        if (jl.houses[i].owner == j->id) jl.houses[i].owner = 0;
    int id = j->id;
    char name[NAME_LEN];
    snprintf(name, sizeof name, "%s", j->name);
    int idx = jl_jerry_index(j);
    for (int i = idx; i < jl.nJerries - 1; i++) jl.jerries[i] = jl.jerries[i + 1];
    jl.nJerries--;
    memset(&jl.jerries[jl.nJerries], 0, sizeof(Jerry));
    for (int i = 0; i < jl.nJerries; i++) {
        if (jl.jerries[i].lover == id) jl.jerries[i].lover = 0;
        if (jl.jerries[i].goToId == id) jl.jerries[i].goToId = 0;
    }
    rel_drop(id);
    if (jl.selected == id) jl.selected = 0;
    jl_note("%s left the island.", name);
}

void jl_hang_out(Jerry *j, Jerry *o) {
    j->goToId = o->id;
    j->goToT = 25;
    jl_note("%s is heading over to %s.", j->name, o->name);
}

void jl_wake(Jerry *j) {
    if (j->inside) { j->inside = false; j->sleepForce = true; j->st = ST_IDLE; j->t = 1; }
}

bool jl_truck(void) {
    int t = today_int();
    if (jl.lastTruck == t) { jl_note("The snack car is out of free food for today."); return false; }
    jl.lastTruck = t;
    for (int i = 0; i < 2; i++) {
        int f;
        do f = RI(0, FOOD_COUNT - 1); while (FOODS[f].taste == TASTE_GROSS);
        jl.foods[f]++;
        jl_note("Snack car: %s!", FOODS[f].name);
    }
    sfx_play(SFX_COIN);
    return true;
}

void jl_collect_spark(int i) {
    Spark *s = &jl.sparks[i];
    if (!s->on) return;
    jl.coins += s->v;
    popup(s->x, s->y - 10, GOLD, "+%d", s->v);
    sfx_play(SFX_COIN);
    s->on = false;
}

void jl_set_time_mode(int mode) {
    static const int SECS[TIME_MODES] = {-1, 8 * 3600, 13 * 3600, 18 * 3600 + 2520, 23 * 3600 + 1800};
    jl.timeMode = (uint8_t)(mode % TIME_MODES);
    daycycle_set_time(SECS[jl.timeMode]);
    daycycle_update();
}
void jl_set_season_mode(int mode) {
    jl.seasonMode = (uint8_t)(mode % 5);
}

/* ---------------------------------------------------------------- toys */

static void roll_dice(ToyW *t) {
    int n = RI(1, 6);
    t->rolled = 2;
    popup(t->x, t->y - 18, n == 6 ? GOLD : n == 1 ? RGB8(255, 106, 90) : RGB8(255, 255, 255), "%d", n);
}

static void kick(int ti, Jerry *j) {
    ToyW *t = &jl.toys[ti];
    float a = atan2f(t->y - j->y, t->x - j->x) + R(-.5f, .5f);
    float f = ti == TOY_MAGIC_8_BALL ? 60 : ti == TOY_BEACH_BALL ? 150 : 110;
    t->vx = cosf(a) * f;
    t->vy = sinf(a) * f;
    t->hop = ti == TOY_BEACH_BALL ? 1 : .4f;
    if (rnd01() < .5f) { gain(j, 2, 1); emote(j, EM_NOTE, 1.4f); }
    if (ti == TOY_WHITE_DICE && t->rolled <= 0) roll_dice(t);
}

void jl_kick_toy(int ti, float mx, float my) {
    ToyW *t = &jl.toys[ti];
    if (!t->on) return;
    float a = atan2f(t->y - my, t->x - mx);
    t->vx = cosf(a) * 120;
    t->vy = sinf(a) * 120;
    t->hop = 1;
    sfx_play(SFX_POP);
    if (ti == TOY_WHITE_DICE) roll_dice(t);
}

static void upd_toy(int ti, float dt) {
    ToyW *t = &jl.toys[ti];
    if (!t->on) return;
    t->rolled = fmaxf(0, t->rolled - dt);
    t->hop = fmaxf(0, t->hop - dt * 1.6f);
    t->ph += dt;
    if (ti == TOY_SQUEAKY_MOUSE && rnd01() < dt * .6f) {
        float a = R(0, 6.28f);
        t->vx = cosf(a) * 45;
        t->vy = sinf(a) * 45;
    }
    if (ti == TOY_BALLOON_SNAKE) {
        t->vx += sinf(t->ph * .5f) * dt * 4;
        t->vy += cosf(t->ph * .4f) * dt * 3;
    }
    float nx = t->x + t->vx * dt, ny = t->y + t->vy * dt;
    if (map_walk(nx, t->y)) t->x = nx; else t->vx *= -.7f;
    if (map_walk(t->x, ny)) t->y = ny; else t->vy *= -.7f;
    float fr = ti == TOY_BALLOON_SNAKE ? .995f : ti == TOY_MAGIC_8_BALL ? .93f : .97f;
    (void)dt;  // jl_tick() runs at a fixed 1/60 s, so fr^(dt*60) == fr
    t->vx *= fr;
    t->vy *= fr;
}

/* ---------------------------------------------------------------- residents */

static bool move_j(Jerry *j, float dt, float m) {
    float sp = PERS[j->pers].spd * m * (j->angry ? 1.3f : 1);
    float dx = j->tx - j->x, dy = j->ty - j->y, d = sqrtf(dx * dx + dy * dy);
    if (d < 0.001f) d = 1;
    float nx = j->x + dx / d * sp * dt, ny = j->y + dy / d * sp * dt;
    // A resident standing in a doorway (inside the house's own collision box) may walk out freely.
    if (j->st == ST_HOME || !map_walk(j->x, j->y) || map_walk(nx, ny)) {
        j->x = nx;
        j->y = ny;
        if (fabsf(dx) > 1) j->dir = dx > 0 ? 1 : -1;
        return true;
    }
    if (map_walk(nx, j->y)) { j->x = nx; return true; }
    if (map_walk(j->x, ny)) { j->y = ny; return true; }
    return false;
}

static int tier(float v) { return v >= 85 ? 3 : v >= 55 ? 2 : v >= 25 ? 1 : 0; }

static void confess(Jerry *a, Jerry *b, int nv) {
    Jerry *who = rnd01() < .5f ? a : b, *tgt = who == a ? b : a;
    jl_note("%s confessed their feelings to %s...", who->name, tgt->name);
    if (compat(a->pers, b->pers) >= 0 || rnd01() < .4f) delay(1.5f, DL_COUPLE, a->id, b->id, 0);
    else delay(1.5f, DL_REJECT, who->id, tgt->id, nv);
}

static void social(Jerry *a, Jerry *b, bool force) {
    if (!force && (a->socCd > 0 || b->socCd > 0)) return;
    a->socCd = b->socCd = R(14, 30);
    int cp = compat(a->pers, b->pers);
    float old = jl_rel(a->id, b->id);
    a->st = b->st = ST_CHAT;
    a->t = b->t = 3;
    a->dir = b->x > a->x ? 1 : -1;
    b->dir = -a->dir;
    int d = force ? 8 : RI(2, 5) + (cp > 0 ? 2 : 0);
    if (cp < 0 && !force && old < 40 && rnd01() < .4f) {
        d = -8;
        a->angry = b->angry = true;
        a->ang = b->ang = 18;
        a->st = b->st = ST_CHAT;
        emote(a, EM_ANGRY, 3);
        emote(b, EM_ANGRY, 3);
        sfx_play(SFX_BAD);
        jl_note("%s and %s had an argument!", a->name, b->name);
    } else {
        say(a, pick_line(a->pers), 2.8f);
        delay(.9f, DL_SAYLINE, b->id, 0, 0);
        hearts(a, 1);
    }
    int nv = clampi((int)old + d, 0, 100);
    rel_set(a->id, b->id, nv);
    Jerry *both[2] = {a, b};
    for (int i = 0; i < 2; i++) {
        Jerry *j = both[i];
        if (j->conType == CON_LONELY && d > 0) solve(j, 0);
        gain(j, d > 0 ? 4 : -4, d > 0 ? 3 : 0);
    }
    if (tier((float)nv) > tier(old) && d > 0) {
        static const char *const NM[4] = {"", "are now friends", "are good friends now", "are besties now"};
        jl_note("%s and %s %s!", a->name, b->name, NM[tier((float)nv)]);
        jl.coins += tier((float)nv) * 8;
        sfx_play(SFX_HEART);
        hearts(a, 4);
        hearts(b, 4);
    }
    if (nv >= 85 && !a->lover && !b->lover && rnd01() < .35f) confess(a, b, nv);
}

static void add_toy_play(Jerry *j) {
    int own[TOY_COUNT], n = 0;
    for (int i = 0; i < TOY_COUNT; i++)
        if (jl.toys[i].on) own[n++] = i;
    if (n) j->toy = own[RI(0, n - 1)];
}

static void upd_jerry(Jerry *j, float dt) {
    j->talkCd = fmaxf(0, j->talkCd - dt);
    j->socCd = fmaxf(0, j->socCd - dt);
    static const float DECAY[5] = {1.0f, 0.88f, 0.7744f, 0.681472f, 0.59969536f};  // .88^charm
    float decay = DECAY[clampi(j->charm, 0, 4)];
    j->hunger = fmaxf(0, j->hunger - dt / 36);
    j->joy = fmaxf(0, j->joy - dt / 70 * decay);
    if (j->bubKind != BUB_NONE) {
        j->bubTtl -= dt;
        if (j->bubTtl <= 0) j->bubKind = BUB_NONE;
    }
    if (j->angry) {
        j->ang -= dt;
        if (j->ang <= 0) j->angry = false;
    }
    if (j->conType == CON_NONE) {
        j->ccool -= dt;
        if (j->ccool <= 0) { add_concern(j); j->ccool = R(60, 140); }
    } else {
        check_concern(j);
        if (j->conType == CON_LONELY && jl.nJerries < 2) j->conType = CON_NONE;
    }
    bool night = jl.hour >= 23 || jl.hour < 6;
    if (j->inside) {
        if (!night && !j->sleepForce) { j->inside = false; j->st = ST_IDLE; j->t = 1; }
        else if (!night && j->sleepForce) { j->sleepForce = false; }
        return;
    }
    if (night && j->st != ST_HOME) {
        int bi = -1;
        for (int i = 0; i < jl.nHouses; i++)
            if (jl.houses[i].owner == j->id) bi = map_building_for_slot(jl.houses[i].slot);
        if (bi >= 0) {
            j->st = ST_HOME;
            j->tx = (float)map_buildings[bi].x;
            j->ty = (float)map_buildings[bi].y - 2;
        }
    }
    if (j->st == ST_HOME) {
        float d = hypotf(j->tx - j->x, j->ty - j->y);
        if (d < 4) { j->inside = true; j->st = ST_IDLE; return; }
        move_j(j, dt, 1.3f);
        return;
    }
    if (j->goToId) {
        Jerry *o = jl_jerry(j->goToId);
        j->goToT -= dt;
        if (!o || j->goToT <= 0 || o->inside) j->goToId = 0;
        else {
            j->tx = o->x - 12;
            j->ty = o->y;
            if (hypotf(o->x - j->x, o->y - j->y) < 30) { j->goToId = 0; social(j, o, true); }
            else { move_j(j, dt, 1.4f); return; }
        }
    }
    if (j->st == ST_EAT || j->st == ST_TALK || j->st == ST_CHAT) {
        j->t -= dt;
        if (j->t <= 0) j->st = ST_IDLE;
        return;
    }
    if (j->st == ST_PLAY) {
        j->t -= dt;
        int ti = (j->toy >= 0 && jl.toys[j->toy].on) ? j->toy : -1;
        if (ti < 0)
            for (int i = 0; i < TOY_COUNT; i++)
                if (jl.toys[i].on) { ti = i; break; }
        if (ti >= 0) {
            ToyW *t = &jl.toys[ti];
            j->tx = t->x - 8;
            j->ty = t->y;
            if (hypotf(t->x - j->x, t->y - j->y) < 14) kick(ti, j);
            else move_j(j, dt, 1.2f);
        }
        if (j->t <= 0) { j->st = ST_IDLE; j->toy = -1; }
        return;
    }
    if (j->st == ST_IDLE) {
        j->t -= dt;
        if (j->t <= 0) {
            if (j->lover && rnd01() < .35f) {
                Jerry *l = jl_jerry(j->lover);
                if (l && !l->inside) { j->tx = l->x + 14; j->ty = l->y; j->st = ST_WALK; return; }
            }
            bool anyToy = false;
            for (int i = 0; i < TOY_COUNT; i++) anyToy |= jl.toys[i].on;
            if (anyToy && rnd01() < .18f) {
                j->st = ST_PLAY;
                j->t = R(5, 9);
                add_toy_play(j);
                return;
            }
            float nearXY[2] = {j->x, j->y};
            map_rand_spot(nearXY, R(40, 130), &j->tx, &j->ty);
            j->st = ST_WALK;
            j->t = R(8, 14);
        }
        return;
    }
    if (j->st == ST_WALK) {
        j->t -= dt;
        float d = hypotf(j->tx - j->x, j->ty - j->y);
        if (d < 3 || j->t <= 0) { j->st = ST_IDLE; j->t = R(1, 5); return; }
        if (!move_j(j, dt, 1)) { j->st = ST_IDLE; j->t = R(.5f, 2); }
    }
}

static void social_tick(void) {
    Jerry *act[MAX_JERRIES];
    int n = 0;
    for (int i = 0; i < jl.nJerries; i++) {
        Jerry *j = &jl.jerries[i];
        if (!j->inside && (j->st == ST_IDLE || j->st == ST_WALK)) act[n++] = j;
    }
    for (int i = 0; i < n; i++)
        for (int k = i + 1; k < n; k++)
            if (hypotf(act[i]->x - act[k]->x, act[i]->y - act[k]->y) < 26) social(act[i], act[k], false);
}

static void spawn_spark(void) {
    int n = 0;
    for (int i = 0; i < MAX_SPARKS; i++) n += jl.sparks[i].on;
    if (n >= MAX_SPARKS) return;
    static const char *const K[6] = {"obj_shell", "obj_starfish", "obj_gshell", "obj_coral", "obj_spiral", "obj_conch"};
    for (int tries = 0; tries < 30; tries++) {
        float x = R(40, PW - 40), y = R(40, PH - 40);
        if (map_kind((int)(x / 16), (int)(y / 16)) == K_SAND && map_walk(x, y)) {
            for (int i = 0; i < MAX_SPARKS; i++)
                if (!jl.sparks[i].on) {
                    jl.sparks[i] = (Spark){true, x, y, rnd01() * 6, spr_find(K[RI(0, 5)]), RI(6, 24)};
                    return;
                }
        }
    }
}

static void upd_parts(float dt) {
    for (int i = 0; i < MAX_PARTS; i++) {
        Part *p = &jl.parts[i];
        if (!p->on) continue;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->l -= dt;
        if (p->kind == PK_CONF) p->vy += 120 * dt;
        if (p->l <= 0) p->on = false;
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        Popup *p = &jl.popups[i];
        if (!p->on) continue;
        p->y -= 14 * dt;
        p->l -= dt;
        if (p->l <= 0) p->on = false;
    }
}

static void run_delayed(float dt) {
    for (int i = 0; i < MAX_DELAYED; i++) {
        Delayed *d = &delayed[i];
        if (!d->on) continue;
        d->t -= dt;
        if (d->t > 0) continue;
        d->on = false;
        if (d->kind == DL_SAYLINE) {
            Jerry *b = jl_jerry(d->a);
            if (b && !b->inside) say(b, pick_line(b->pers), 2.8f);
        } else if (d->kind == DL_COUPLE) {
            Jerry *a = jl_jerry(d->a), *b = jl_jerry(d->b);
            if (!a || !b) continue;
            a->lover = b->id;
            b->lover = a->id;
            jl_note("%s and %s are now a couple!", a->name, b->name);
            hearts(a, 10);
            hearts(b, 10);
            sfx_play(SFX_LEVEL);
            jl.coins += 50;
        } else if (d->kind == DL_REJECT) {
            Jerry *who = jl_jerry(d->a), *tgt = jl_jerry(d->b);
            if (!who || !tgt) continue;
            jl_note("%s turned %s down gently.", tgt->name, who->name);
            rel_set(who->id, tgt->id, maxi(0, d->c - 25));
            emote(who, EM_BROKEN, 3);
        }
    }
}

void jl_tick(void) {
    const float dt = 1.0f / 60.0f;
    jl.clock += dt;
    jl.hour = (float)daycycle_hour() + (float)daycycle_minute() / 60.0f;
    static int seasonCheck = 0;
    if (++seasonCheck >= 60) {
        seasonCheck = 0;
        int ns = season_now();
        if (ns != jl.season) {
            jl.season = ns;
            map_layout(ns);
            jl.seasonDirty = true;
            for (int i = 0; i < jl.nJerries; i++) {
                Jerry *j = &jl.jerries[i];
                if (!j->inside && !map_walk(j->x, j->y)) map_rand_spot(NULL, 0, &j->x, &j->y);
            }
        }
    }
    for (int i = 0; i < jl.nJerries; i++) upd_jerry(&jl.jerries[i], dt);
    for (int i = 0; i < TOY_COUNT; i++) upd_toy(i, dt);
    for (int i = 0; i < MAX_SPARKS; i++) jl.sparks[i].t += dt;
    for (int i = 0; i < MAX_TOASTS; i++)
        if (jl.toastTtl[i] > 0) jl.toastTtl[i] -= dt;
    upd_parts(dt);
    run_delayed(dt);
    static float accSoc = 0, accSpark = 0;
    accSoc += dt;
    if (accSoc > 1.2f) { accSoc = 0; social_tick(); }
    accSpark += dt;
    if (accSpark > 18) { accSpark = 0; spawn_spark(); }
}

/* ---------------------------------------------------------------- counts, hit testing */

void jl_select(Jerry *j) {  // clicking a resident in the web game: select, look up, chat for a moment
    jl.selected = j->id;
    j->t = fmaxf(j->t, 1.2f);
    if (!j->inside && (j->st == ST_WALK || j->st == ST_IDLE)) { j->st = ST_TALK; j->t = 1.2f; }
    hearts(j, 1);
    sfx_play(SFX_POP);
}

int jl_friend_count(void) {
    int n = 0;
    for (int i = 0; i < jl.nRel; i++) n += jl.rel[i].v >= 25;
    return n;
}
int jl_couple_count(void) {
    int n = 0;
    for (int i = 0; i < jl.nJerries; i++) n += jl.jerries[i].lover != 0;
    return n / 2;
}

Hit jl_hit(float mx, float my) {
    Hit best = {HIT_NONE, -1};
    float bestY = -1e9f;
    for (int i = 0; i < jl.nJerries; i++) {
        const Jerry *j = &jl.jerries[i];
        if (j->inside) continue;
        if (mx > j->x - 14 && mx < j->x + 14 && my > j->y - 34 && my < j->y + 12 && j->y > bestY) {
            best = (Hit){HIT_JERRY, i};
            bestY = j->y;
        }
    }
    if (best.kind != HIT_NONE) return best;
    for (int i = MAX_SPARKS - 1; i >= 0; i--) {
        const Spark *s = &jl.sparks[i];
        if (s->on && hypotf(mx - s->x, my - s->y + 4) < 14) return (Hit){HIT_SPARK, i};
    }
    for (int i = 0; i < TOY_COUNT; i++)
        if (jl.toys[i].on && hypotf(mx - jl.toys[i].x, my - jl.toys[i].y + 4) < 12) return (Hit){HIT_TOY, i};
    int bi = -1, by = -100000;
    for (int i = 0; i < map_nbuildings; i++) {
        const Building *b = &map_buildings[i];
        const SprDef *r = spr_def(b->spr);
        float x0 = (float)(b->x - r->ax), y0 = (float)(b->y - r->ay);
        if (mx > x0 && mx < x0 + r->w && my > y0 && my < y0 + r->h && b->y > by) { bi = i; by = b->y; }
    }
    if (bi >= 0) return (Hit){HIT_BUILDING, bi};
    return best;
}

/* ---------------------------------------------------------------- lifecycle */

void jl_init(void) {
    memset(&jl, 0, sizeof jl);
    memset(delayed, 0, sizeof delayed);
    jl.coins = 300;
    jl.nextId = 1;
    static const int colors[3] = {1, 3, 5};
    for (int i = 0; i < 3; i++) jl.houses[i] = (House){i, colors[i], 0};
    jl.nHouses = 3;
    jl.musicOn = true;
    map_init();
}

void jl_erase(void) {
    jl_init();
    jl_start();
}

void jl_start(void) {
    daycycle_update();
    jl_set_time_mode(jl.timeMode);
    jl.hour = (float)daycycle_hour() + (float)daycycle_minute() / 60.0f;
    jl.season = season_now();
    map_layout(jl.season);
    jl.seasonDirty = true;
    for (int i = 0; i < jl.nJerries; i++) {
        Jerry *j = &jl.jerries[i];
        bool hasHouse = false;
        for (int h = 0; h < jl.nHouses; h++) hasHouse |= jl.houses[h].owner == j->id;
        if (!hasHouse) {
            House *f = free_house();
            if (f) { f->owner = j->id; j->home = f->slot; }
        }
        spawn_jerry(j);
    }
    for (int i = 0; i < TOY_COUNT; i++)
        if (jl.toysOwned[i]) spawn_toy(i);
    int t = today_int();
    if (jl.lastDay != t) {
        jl.lastDay = t;
        jl.coins += 50;
        jl_note("Daily bonus: +50 coins!");
    }
    for (int i = 0; i < 3; i++) spawn_spark();
}

/* ---------------------------------------------------------------- saving */

#define SAVE_MAGIC 0x31464C4Au /* "JLF1" */

typedef struct { uint8_t *p; size_t n, cap; bool ok; } Wr;
typedef struct { const uint8_t *p; size_t n, pos; bool ok; } Rd;

static void w8(Wr *w, unsigned v) { if (w->n < w->cap) w->p[w->n++] = (uint8_t)v; else w->ok = false; }
static void w16(Wr *w, unsigned v) { w8(w, v & 255); w8(w, v >> 8); }
static void w32(Wr *w, uint32_t v) { w16(w, v & 0xFFFF); w16(w, v >> 16); }
static void wstr(Wr *w, const char *s, int n) { for (int i = 0; i < n; i++) w8(w, s[i]); }
static unsigned r8(Rd *r) { if (r->pos < r->n) return r->p[r->pos++]; r->ok = false; return 0; }
static unsigned r16(Rd *r) { unsigned a = r8(r); return a | (r8(r) << 8); }
static uint32_t r32(Rd *r) { uint32_t a = r16(r); return a | ((uint32_t)r16(r) << 16); }
static void rstr(Rd *r, char *s, int n) { for (int i = 0; i < n; i++) s[i] = (char)r8(r); s[n - 1] = 0; }

size_t jl_pack(uint8_t *out, size_t cap) {
    Wr w = {out, 0, cap, true};
    w32(&w, SAVE_MAGIC);
    w32(&w, (uint32_t)time(NULL));
    w32(&w, (uint32_t)jl.coins);
    w8(&w, FOOD_COUNT);
    w8(&w, GIFT_COUNT);
    w8(&w, TOY_COUNT);
    for (int i = 0; i < FOOD_COUNT; i++) w16(&w, jl.foods[i]);
    for (int i = 0; i < GIFT_COUNT; i++) w16(&w, jl.gifts[i]);
    for (int i = 0; i < TOY_COUNT; i++) w8(&w, jl.toysOwned[i]);
    w8(&w, jl.nHouses);
    for (int i = 0; i < jl.nHouses; i++) { w8(&w, jl.houses[i].slot); w8(&w, jl.houses[i].color); w16(&w, jl.houses[i].owner); }
    w8(&w, jl.nJerries);
    for (int i = 0; i < jl.nJerries; i++) {
        const Jerry *j = &jl.jerries[i];
        w16(&w, j->id);
        wstr(&w, j->name, NAME_LEN);
        w8(&w, j->skin); w8(&w, j->pers);
        w8(&w, (unsigned)clampf(j->hunger, 0, 100)); w8(&w, (unsigned)clampf(j->joy, 0, 100));
        w16(&w, j->xp); w8(&w, j->lvl);
        w8(&w, j->fav); w8(&w, j->favGift); w8(&w, j->home);
        w8(&w, j->charm); w16(&w, j->lover);
        w8(&w, j->conType); w8(&w, j->conId);
    }
    w8(&w, jl.nRel);
    for (int i = 0; i < jl.nRel; i++) { w16(&w, jl.rel[i].a); w16(&w, jl.rel[i].b); w8(&w, jl.rel[i].v); }
    w8(&w, jl.nEvents);
    for (int i = 0; i < jl.nEvents; i++) { w32(&w, jl.events[i].t); wstr(&w, jl.events[i].m, EVENT_LEN); }
    w32(&w, (uint32_t)jl.lastDay);
    w32(&w, (uint32_t)jl.lastTruck);
    w16(&w, jl.nextId);
    w8(&w, jl.seasonMode); w8(&w, jl.timeMode); w8(&w, jl.musicOn);
    return w.ok ? w.n : 0;
}

bool jl_unpack(const uint8_t *in, size_t len) {
    Rd r = {in, len, 0, true};
    if (r32(&r) != SAVE_MAGIC) return false;
    static JL tmp; // static: too big for the ARM9 stack
    memset(&tmp, 0, sizeof tmp);
    uint32_t saved = r32(&r);
    tmp.coins = (int)r32(&r);
    unsigned nf = r8(&r), ng = r8(&r), nt = r8(&r);
    if (nf != FOOD_COUNT || ng != GIFT_COUNT || nt != TOY_COUNT) return false;
    for (int i = 0; i < FOOD_COUNT; i++) tmp.foods[i] = (uint16_t)r16(&r);
    for (int i = 0; i < GIFT_COUNT; i++) tmp.gifts[i] = (uint16_t)r16(&r);
    for (int i = 0; i < TOY_COUNT; i++) tmp.toysOwned[i] = (uint8_t)r8(&r);
    tmp.nHouses = (int)r8(&r);
    if (tmp.nHouses < 1 || tmp.nHouses > MAX_HOUSES) return false;
    for (int i = 0; i < tmp.nHouses; i++) {
        tmp.houses[i].slot = (int)r8(&r);
        tmp.houses[i].color = (int)r8(&r);
        tmp.houses[i].owner = (int)r16(&r);
        if (tmp.houses[i].slot >= MAX_HOUSES || tmp.houses[i].color < 1 || tmp.houses[i].color > 6) return false;
    }
    tmp.nJerries = (int)r8(&r);
    if (tmp.nJerries > MAX_JERRIES) return false;
    double el = saved ? (double)((uint32_t)time(NULL) - saved) : 0;
    if (el < 0 || el > 6 * 3600) el = el < 0 ? 0 : 6 * 3600;
    for (int i = 0; i < tmp.nJerries; i++) {
        Jerry *j = &tmp.jerries[i];
        j->used = true;
        j->id = (int)r16(&r);
        rstr(&r, j->name, NAME_LEN);
        j->skin = (uint8_t)r8(&r);
        j->pers = (uint8_t)r8(&r);
        j->hunger = (float)r8(&r);
        j->joy = (float)r8(&r);
        j->xp = (int)r16(&r);
        j->lvl = (int)r8(&r);
        j->fav = (int)r8(&r);
        j->favGift = (int)r8(&r);
        j->home = (int)r8(&r);
        j->charm = (int)r8(&r);
        j->lover = (int)r16(&r);
        j->conType = (uint8_t)r8(&r);
        j->conId = (int)r8(&r);
        if (j->skin >= SKIN_COUNT || j->pers >= PERS_COUNT || j->fav >= FOOD_COUNT || j->favGift >= GIFT_COUNT ||
            j->lvl < 1 || j->conType > CON_WANT_GIFT || j->conId >= (j->conType == CON_WANT_GIFT ? GIFT_COUNT : FOOD_COUNT) || j->id <= 0)
            return false;
        j->hunger = fmaxf(20, j->hunger - (float)el / 72);
        j->joy = fmaxf(20, j->joy - (float)el / 140);
        j->held = -1;
        j->ccool = 30;
        j->toy = -1;
        j->dir = 1;
        j->t = 1;
        j->socCd = R(2, 8);
    }
    tmp.nRel = (int)r8(&r);
    if (tmp.nRel > MAX_REL) return false;
    for (int i = 0; i < tmp.nRel; i++) {
        tmp.rel[i].a = (uint16_t)r16(&r);
        tmp.rel[i].b = (uint16_t)r16(&r);
        tmp.rel[i].v = (uint8_t)r8(&r);
    }
    tmp.nEvents = (int)r8(&r);
    if (tmp.nEvents > MAX_EVENTS) return false;
    for (int i = 0; i < tmp.nEvents; i++) {
        tmp.events[i].t = r32(&r);
        rstr(&r, tmp.events[i].m, EVENT_LEN);
    }
    tmp.lastDay = (int)r32(&r);
    tmp.lastTruck = (int)r32(&r);
    tmp.nextId = (int)r16(&r);
    tmp.seasonMode = (uint8_t)r8(&r);
    tmp.timeMode = (uint8_t)r8(&r);
    tmp.musicOn = r8(&r) != 0;
    if (!r.ok || tmp.seasonMode > 4 || tmp.timeMode >= TIME_MODES || tmp.nextId < 1) return false;
    for (int i = 0; i < tmp.nHouses; i++) { // a house must not stay owned by a resident that is gone
        bool found = false;
        for (int k = 0; k < tmp.nJerries; k++) found |= tmp.jerries[k].id == tmp.houses[i].owner;
        if (!found) tmp.houses[i].owner = 0;
    }
    tmp.savedAt = saved;
    jl = tmp;
    return true;
}
