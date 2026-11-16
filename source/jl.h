// Jerry Life: platform-independent game state and simulation (ported from the web game's script.js).
#ifndef JL_H
#define JL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "itemdata.h"

#define TILE 16
#define GW 64
#define GH 40
#define PW (GW * TILE) // world size in pixels: 1024 x 640
#define PH (GH * TILE)

#define MAX_JERRIES 6
#define MAX_HOUSES 6
#define NAME_LEN 13
#define MAX_SPARKS 5
#define MAX_PARTS 72
#define MAX_POPUPS 10
#define MAX_REL 32
#define MAX_EVENTS 24
#define MAX_DELAYED 12
#define MAX_TOASTS 3
#define EVENT_LEN 60

enum { SEASON_SPRING, SEASON_SUMMER, SEASON_FALL, SEASON_WINTER };
enum { PERS_CHILL, PERS_ENERGETIC, PERS_GRUMPY, PERS_SWEET, PERS_QUIRKY, PERS_COUNT };
enum { MODE_AUTO = 0 };               // seasonMode: 0 = auto, 1..4 = spring..winter
enum { TIME_AUTO, TIME_MORNING, TIME_DAY, TIME_EVENING, TIME_NIGHT, TIME_MODES };

typedef enum { ST_IDLE, ST_WALK, ST_HOME, ST_EAT, ST_TALK, ST_CHAT, ST_PLAY } JState;
typedef enum { CON_NONE, CON_HUNGRY, CON_BORED, CON_LONELY, CON_WANT_FOOD, CON_WANT_GIFT } ConType;
typedef enum { BUB_NONE, BUB_TEXT, BUB_EMOTE } BubKind;
typedef enum { EM_HEART, EM_NOTE, EM_ANGRY, EM_STAR, EM_BROKEN, EM_ZZZ } Emote;

typedef struct {
    bool used;
    int id;
    char name[NAME_LEN];
    uint8_t skin, pers;
    float x, y, tx, ty;
    float hunger, joy;
    int xp, lvl;
    int fav, favGift;     // favourite food / gift (indices)
    int home;             // house slot
    int held;             // gift index shown in hand, or -1
    int charm;
    int lover;            // id of partner, 0 = none
    uint8_t conType;      // ConType
    int conId;            // wanted item index for CON_WANT_*
    float ccool;          // seconds until the next concern
    // runtime-only
    JState st;
    float t;
    int dir;
    uint8_t bubKind;
    char bubText[40];
    float bubTtl;
    bool angry;
    float ang, talkCd, socCd;
    int goToId;
    float goToT;
    bool inside, sleepForce;
    int toy;              // toy being played with, or -1
} Jerry;

typedef struct { int slot, color, owner; } House; // owner = jerry id, 0 = vacant

typedef struct {
    bool on;
    float x, y, vx, vy, hop, ph, rolled;
} ToyW;

typedef struct { bool on; float x, y, t; int spr, v; } Spark;
typedef struct { bool on; uint8_t kind; float x, y, vx, vy, l; uint16_t col; int s; } Part;
enum { PK_HEART, PK_CONF, PK_ANGRY, PK_FLAKE };
typedef struct { bool on; char text[10]; float x, y, l; uint16_t col; } Popup;
typedef struct { uint32_t t; char m[EVENT_LEN]; } Event;
typedef struct { uint16_t a, b; uint8_t v; } Rel;

typedef struct {
    // ---- saved ----
    int coins;
    uint16_t foods[FOOD_COUNT], gifts[GIFT_COUNT];
    uint8_t toysOwned[TOY_COUNT];
    House houses[MAX_HOUSES];
    int nHouses;
    Jerry jerries[MAX_JERRIES];
    int nJerries;
    Rel rel[MAX_REL];
    int nRel;
    Event events[MAX_EVENTS];
    int nEvents;
    int lastDay, lastTruck; // yyyymmdd
    int nextId;
    uint8_t seasonMode, timeMode;
    bool musicOn;
    uint32_t savedAt;
    // ---- runtime ----
    int season;
    float hour;
    float clock;
    ToyW toys[TOY_COUNT];
    Spark sparks[MAX_SPARKS];
    Part parts[MAX_PARTS];
    Popup popups[MAX_POPUPS];
    char toast[MAX_TOASTS][EVENT_LEN];
    float toastTtl[MAX_TOASTS];
    int selected;           // jerry id or 0
    int dragId;             // jerry id being dragged by the stylus, or 0
    bool seasonDirty;       // map/terrain must be rebuilt (season changed)
} JL;

extern JL jl;

// Every kind is a sample from assets/sfx (jerry_*.ogg voices; pop/coin/heart/bad.ogg effects; a random variant
// of the family: yes1/yes2/yes3 ...).
enum { SFX_POP, SFX_COIN, SFX_HEART, SFX_BAD,
       SFX_LEVEL,   // levelup: level ups, moving in, becoming a couple
       SFX_CLICK,   // click: selecting a resident
       SFX_YES,     // yes1-3: liked/loved food, gift or toy
       SFX_NO,      // no1-3: refused/gross food, arguments, can't do that
       SFX_HAGGLE,  // haggle1,3: buying food, gifts and toys
       SFX_HIT,     // hit1-4: kicking a toy (slime squishes)
       SFX_IDLE,    // idle1-3: chatting with a resident
       SFX_KINDS };
void sfx_play(int kind); // implemented by the platform (sfx.c / host stub)

// ----- lifecycle -----
void jl_init(void);                 // fresh state + map (3 empty houses, 300 coins)
void jl_start(void);                // after jl_init / load: place everybody, daily bonus, sparks
size_t jl_pack(uint8_t *out, size_t cap);
bool jl_unpack(const uint8_t *in, size_t len);
void jl_tick(void);                 // one 1/60 s step

// ----- queries -----
Jerry *jl_jerry(int id);
Jerry *jl_selected(void);
int jl_jerry_index(const Jerry *j);
float jl_rel(int a, int b);
const char *jl_rel_name(float v);
const char *jl_pers_label(int pers);
int jl_create_cost(void);
int jl_house_cost(void);            // -1 when every lot is built
int jl_free_houses(void);
int jl_toy_count(void);
int jl_friend_count(void);
int jl_couple_count(void);
int jl_concern_text(const Jerry *j, char *out, size_t n); // returns an icon code (see ui.c) and fills text
int season_now(void);

// verdicts: 0 gross/dislike .. used for item hints
enum { V_GROSS, V_DISLIKE, V_OK, V_LIKE, V_LOVE };
int jl_food_verdict(const Jerry *j, int food);
int jl_gift_verdict(const Jerry *j, int gift);

// ----- actions (return true on success) -----
bool jl_feed(Jerry *j, int food);
bool jl_give(Jerry *j, int gift);
bool jl_give_toy(Jerry *j, int toy);
void jl_talk(Jerry *j);
bool jl_buy_food(int i);
bool jl_buy_gift(int i);
bool jl_buy_toy(int i);
Jerry *jl_create_jerry(const char *name, int skin, int pers);
bool jl_build_house(void);
void jl_move_out(Jerry *j);
void jl_hang_out(Jerry *j, Jerry *other);
void jl_wake(Jerry *j);             // "Find": bring a sleeping resident outside
void jl_select(Jerry *j);           // select a resident (like clicking them in the web game)
bool jl_truck(void);                // snack car; false when already used today
void jl_collect_spark(int i);
void jl_kick_toy(int toy, float fromX, float fromY);
void jl_set_time_mode(int mode);    // TIME_AUTO .. TIME_NIGHT (drives daycycle)
void jl_set_season_mode(int mode);
void jl_note(const char *fmt, ...); // log + toast
void jl_erase(void);                // wipe back to a new game (name picker is up to the UI)

// ----- hit testing on the world (for A-button / minimap taps) -----
enum { HIT_NONE, HIT_JERRY, HIT_SPARK, HIT_TOY, HIT_BUILDING };
typedef struct { int kind, index; } Hit;
Hit jl_hit(float mx, float my);

// Pixel-sprite helpers used by both the simulation and the renderer.
extern const char *const JL_NAMES[16];
#define JL_NAME_COUNT 16

#endif
