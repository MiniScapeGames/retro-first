/* Slot Zero: catch bits, dodge slop. The starter game of Vibe Retro Studio. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>
#include "art.h"

#define SCREEN_TILES_W 20
#define GROUND_ROW     13
#define GROUND_Y       ((uint8_t)(GROUND_ROW * 8))
#define HERO_SIZE      16
#define HERO_FLOOR     ((uint8_t)(GROUND_Y - HERO_SIZE))
#define HERO_MAX_X     ((uint8_t)(160 - HERO_SIZE))
#define HERO_SPEED     2
#define JUMP_POWER     947 /* 8.8 fixed point, pixels per frame */
#define GRAVITY        56
#define FAST_FALL      128 /* extra gravity while Down is held in the air */
#define CROUCH_PX      3

#define MAX_ITEMS      6
#define OAM_HERO       0
#define OAM_ITEMS      4
#define WIN_SCORE      2048
#define BIT_VALUE      64
#define START_LIVES    3
#define HURT_FRAMES    80

enum { STATE_TITLE, STATE_PLAY, STATE_OVER, STATE_WIN };
enum { KIND_BIT, KIND_SLOP };

typedef struct {
    uint8_t active;
    uint8_t kind;
    uint8_t x;
    uint16_t y; /* 8.8 fixed point */
    uint8_t speed;
} Item;

static Item items[MAX_ITEMS];
static uint8_t state;
static uint8_t hero_x;
static uint16_t hero_y; /* 8.8 fixed point */
static int16_t hero_vy;
static uint8_t crouching;
static uint8_t lives;
static uint16_t score;
static uint8_t hurt;
static uint8_t spawn_timer;
static uint8_t frame;
static uint8_t palette_set;
static uint8_t muted;

/* ---- screen helpers ---- */

static uint8_t glyph(char c) {
    if (c >= 'A' && c <= 'Z') return T_FONT + (uint8_t)(c - 'A');
    if (c >= '0' && c <= '9') return T_FONT + 26 + (uint8_t)(c - '0');
    switch (c) {
        case '.': return T_FONT + 36;
        case ':': return T_FONT + 37;
        case '-': return T_FONT + 38;
        case '/': return T_FONT + 39;
        case '!': return T_FONT + 40;
        default:  return T_SKY;
    }
}

static void put(uint8_t x, uint8_t y, uint8_t tile, uint8_t palette) {
    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tile_xy(x, y, palette);
    VBK_REG = VBK_TILES;
    set_bkg_tile_xy(x, y, tile);
}

static void print(uint8_t x, uint8_t y, const char *text, uint8_t palette) {
    while (*text) put(x++, y, glyph(*text++), palette);
}

static void print_big(uint8_t x, uint8_t y, const char *text) {
    for (; *text; text++, x += 2) {
        uint8_t base;
        switch (*text) {
            case 'S': base = 0; break;
            case 'L': base = 1; break;
            case 'O': base = 2; break;
            case 'T': base = 3; break;
            case 'Z': base = 4; break;
            case 'E': base = 5; break;
            default:  base = 6; break; /* R */
        }
        base = (uint8_t)(T_BIG + base * 4);
        put(x, y, base, PAL_TEXT);
        put(x + 1, y, base + 1, PAL_TEXT);
        put(x, y + 1, base + 2, PAL_TEXT);
        put(x + 1, y + 1, base + 3, PAL_TEXT);
    }
}

static void apply_palettes(void) {
    set_bkg_palette(0, PAL_BKG_COUNT, pal_bkg[palette_set]);
    set_sprite_palette(0, PAL_OBJ_COUNT, pal_obj[palette_set]);
}

/* ---- the world ---- */

static const uint8_t skyline[SCREEN_TILES_W] = {
    2, 3, 1, 4, 2, 2, 3, 4, 1, 2, 4, 3, 1, 3, 2, 4, 1, 3, 2, 2
};

static const uint8_t sky_rows[GROUND_ROW] = {
    T_SKY, T_SKY, T_SKY, T_SKY, T_SKY, T_SKY, T_SKY,
    T_DITHER1, T_SOLID1, T_DITHER2, T_SOLID2, T_DITHER3, T_SOLID3
};

static void draw_world(void) {
    uint8_t x, y;
    for (y = 0; y < GROUND_ROW; y++) {
        for (x = 0; x < SCREEN_TILES_W; x++) {
            uint8_t height = skyline[x];
            if (y >= (uint8_t)(GROUND_ROW - height)) {
                uint8_t lit = (uint8_t)((x + y) & 1) && y != (uint8_t)(GROUND_ROW - height);
                put(x, y, lit ? T_WINDOW : T_BUILDING, PAL_CITY);
            } else {
                put(x, y, sky_rows[y], PAL_SKY);
            }
        }
    }
    for (x = 0; x < SCREEN_TILES_W; x++) {
        put(x, GROUND_ROW, T_GRASS, PAL_GROUND);
        for (y = GROUND_ROW + 1; y < 18; y++) put(x, y, T_DIRT, PAL_GROUND);
    }
    put(16, 2, T_MOON, PAL_MOON);
    put(17, 2, T_MOON + 1, PAL_MOON);
    put(16, 3, T_MOON + 2, PAL_MOON);
    put(17, 3, T_MOON + 3, PAL_MOON);
    put(2, 2, T_STAR, PAL_TEXT);
    put(13, 1, T_STAR, PAL_TEXT);
    put(4, 6, T_STAR, PAL_TEXT);
    put(18, 6, T_STAR, PAL_TEXT);
}

static void clear_row(uint8_t row, uint8_t tile, uint8_t palette) {
    uint8_t x;
    for (x = 0; x < SCREEN_TILES_W; x++) put(x, row, tile, palette);
}

/* ---- sound ---- */

static void beep(uint16_t pitch, uint8_t envelope) {
    NR11_REG = 0x80;
    NR12_REG = envelope;
    NR13_REG = (uint8_t)pitch;
    NR14_REG = (uint8_t)(0x80 | (pitch >> 8));
}

static void set_muted(uint8_t value) {
    muted = value;
    NR50_REG = muted ? 0x00 : 0x77;
}

/* ---- sprites ---- */

static void hide_item(uint8_t index) {
    move_sprite(OAM_ITEMS + index * 2, 0, 0);
    move_sprite(OAM_ITEMS + index * 2 + 1, 0, 0);
}

static void draw_hero(void) {
    uint8_t screen_y = (uint8_t)(hero_y >> 8);
    uint8_t px = hero_x + DEVICE_SPRITE_PX_OFFSET_X;
    uint8_t py = screen_y + DEVICE_SPRITE_PX_OFFSET_Y;
    uint8_t pose = screen_y < HERO_FLOOR ? 2 : (uint8_t)((frame >> 3) & 1);
    uint8_t first = (uint8_t)(S_HERO + pose * 4);
    uint8_t top_y;
    uint8_t i;
    /* blink while hurt */
    if (hurt && (frame & 4)) py = 0;
    /* crouching pushes the top half into the bottom half, so it needs no extra tiles */
    top_y = py ? (uint8_t)(py + (crouching ? CROUCH_PX : 0)) : 0;
    for (i = 0; i < 4; i++) set_sprite_tile(OAM_HERO + i, first + i);
    move_sprite(OAM_HERO, px, top_y);
    move_sprite(OAM_HERO + 1, px + 8, top_y);
    move_sprite(OAM_HERO + 2, px, py ? py + 8 : 0);
    move_sprite(OAM_HERO + 3, px + 8, py ? py + 8 : 0);
}

static void draw_item(uint8_t index) {
    Item *item = &items[index];
    uint8_t oam = OAM_ITEMS + index * 2;
    uint8_t px = item->x + DEVICE_SPRITE_PX_OFFSET_X;
    uint8_t py = (uint8_t)(item->y >> 8) + DEVICE_SPRITE_PX_OFFSET_Y;
    if (item->kind == KIND_BIT) {
        set_sprite_tile(oam, S_BIT);
        set_sprite_prop(oam, PAL_OBJ_BIT);
        move_sprite(oam, px, py);
        move_sprite(oam + 1, 0, 0);
    } else {
        set_sprite_tile(oam, S_SLOP);
        set_sprite_tile(oam + 1, S_SLOP + 1);
        set_sprite_prop(oam, PAL_OBJ_SLOP);
        set_sprite_prop(oam + 1, PAL_OBJ_SLOP);
        move_sprite(oam, px, py);
        move_sprite(oam + 1, px + 8, py);
    }
}

/* ---- heads-up display ---- */

static void draw_lives(void) {
    uint8_t i;
    for (i = 0; i < START_LIVES; i++) put(i, 0, i < lives ? T_HEART : T_SKY, PAL_TEXT);
}

static void draw_score(void) {
    uint16_t value = score;
    uint8_t x = 15;
    uint8_t i;
    for (i = 0; i < 4; i++) {
        put(x--, 0, T_FONT + 26 + (uint8_t)(value % 10), PAL_TEXT);
        value /= 10;
    }
    print(16, 0, "KB", PAL_TEXT);
}

/* ---- states ---- */

static void clear_items(void) {
    uint8_t i;
    for (i = 0; i < MAX_ITEMS; i++) {
        items[i].active = 0;
        hide_item(i);
    }
}

static void show_title(void) {
    state = STATE_TITLE;
    clear_items();
    draw_world();
    print_big(6, 2, "SLOT");
    print_big(6, 5, "ZERO");
    print(0, 8, " CATCH BITS. DODGE  ", PAL_TEXT);
    print(7, 9, "SLOP.", PAL_TEXT);
    print(4, 15, "PRESS START", PAL_GROUND_TEXT);
    hero_x = 72;
    hero_y = (uint16_t)HERO_FLOOR << 8;
    hero_vy = 0;
    hurt = 0;
}

static void begin_game(void) {
    initrand(DIV_REG);
    state = STATE_PLAY;
    draw_world();
    clear_items();
    lives = START_LIVES;
    score = 0;
    spawn_timer = 30;
    draw_lives();
    draw_score();
    beep(1750, 0xA2);
}

static void finish(uint8_t next, const char *headline, const char *line) {
    state = next;
    clear_row(7, T_DIRT_DARK, PAL_GROUND_TEXT);
    clear_row(8, T_DIRT_DARK, PAL_GROUND_TEXT);
    clear_row(9, T_DIRT_DARK, PAL_GROUND_TEXT);
    clear_row(10, T_DIRT_DARK, PAL_GROUND_TEXT);
    print((uint8_t)(SCREEN_TILES_W / 2 - 5), 8, headline, PAL_GROUND_TEXT);
    print(1, 9, line, PAL_GROUND_TEXT);
    print(4, 15, "PRESS START", PAL_GROUND_TEXT);
}

static void spawn_item(void) {
    uint8_t i;
    uint8_t level = (uint8_t)(score >> 8);
    spawn_timer = (uint8_t)(34 - level * 2 + (rand() & 7));
    for (i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) {
            items[i].active = 1;
            items[i].kind = (rand() & 3) ? KIND_BIT : KIND_SLOP;
            items[i].x = (uint8_t)(4 + (rand() & 127) + (rand() & 15));
            items[i].y = (uint16_t)12 << 8;
            items[i].speed = (uint8_t)(190 + level * 24 + (rand() & 63));
            return;
        }
    }
}

static void hit_slop(void) {
    if (hurt) return;
    lives--;
    hurt = HURT_FRAMES;
    draw_lives();
    beep(400, 0xF3);
    if (lives == 0) finish(STATE_OVER, "BUILD FAILED", "SLOP GOT IN THE ROM");
}

static void catch_bit(void) {
    score += BIT_VALUE;
    draw_score();
    beep((uint16_t)(1700 + (score >> 3)), 0x81);
    if (score >= WIN_SCORE) finish(STATE_WIN, "  BUILD OK", " 2048 KB. WELL DONE ");
}

static void update_items(void) {
    uint8_t i;
    uint8_t hero_screen_y = (uint8_t)(hero_y >> 8);
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *item = &items[i];
        if (!item->active) continue;
        item->y += item->speed;
        uint8_t top = (uint8_t)(item->y >> 8);
        uint8_t width = item->kind == KIND_BIT ? 8 : 16;
        if (top >= (uint8_t)(GROUND_Y - 8)) {
            item->active = 0;
            hide_item(i);
            continue;
        }
        if (item->x < (uint8_t)(hero_x + HERO_SIZE - 2) && (uint8_t)(item->x + width) > (uint8_t)(hero_x + 2) &&
            top < (uint8_t)(hero_screen_y + HERO_SIZE) && (uint8_t)(top + 8) > (uint8_t)(hero_screen_y + 3)) {
            item->active = 0;
            hide_item(i);
            if (item->kind == KIND_BIT) catch_bit(); else hit_slop();
            if (state != STATE_PLAY) return;
            continue;
        }
        draw_item(i);
    }
}

/* The hero answers all four directions, on the title screen too. */
static void update_hero(uint8_t held, uint8_t pressed) {
    uint8_t on_floor = (uint8_t)(hero_y >> 8) >= HERO_FLOOR;
    if ((held & J_LEFT) && hero_x > 1) hero_x -= HERO_SPEED;
    if ((held & J_RIGHT) && hero_x < HERO_MAX_X) hero_x += HERO_SPEED;

    if ((pressed & (J_A | J_UP)) && on_floor) {
        hero_vy = -JUMP_POWER;
        beep(1500, 0x71);
    }
    hero_vy += GRAVITY;
    if ((held & J_DOWN) && !on_floor) hero_vy += FAST_FALL;
    crouching = (uint8_t)((held & J_DOWN) && on_floor);
    hero_y = (uint16_t)((int16_t)hero_y + hero_vy);
    if ((int16_t)hero_y >= (int16_t)((uint16_t)HERO_FLOOR << 8) || (hero_y >> 8) > 200) {
        hero_y = (uint16_t)HERO_FLOOR << 8;
        hero_vy = 0;
    }
}

static void update_play(uint8_t held, uint8_t pressed) {
    update_hero(held, pressed);
    if (hurt) hurt--;
    if (--spawn_timer == 0) spawn_item();
    update_items();
}

/* ---- main ---- */

void main(void) {
    uint8_t held = 0;
    uint8_t last = 0;
    uint8_t i;

    DISPLAY_OFF;
    NR52_REG = 0x80;
    NR51_REG = 0xFF;
    set_muted(0);
    SPRITES_8x8;
    set_bkg_data(0, BKG_TILE_COUNT, bkg_tiles);
    set_sprite_data(0, OBJ_TILE_COUNT, obj_tiles);
    palette_set = 0;
    apply_palettes();
    for (i = 0; i < 4; i++) set_sprite_prop(OAM_HERO + i, PAL_OBJ_HERO);
    show_title();
    draw_hero();
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    while (1) {
        vsync();
        frame++;
        last = held;
        held = joypad();
        {
            uint8_t pressed = (uint8_t)(held & ~last);

            if (pressed & J_SELECT) {
                palette_set = (uint8_t)((palette_set + 1) % PAL_SET_COUNT);
                apply_palettes();
                beep(1800, 0x51);
            }
            if (pressed & J_B) {
                set_muted(!muted);
                if (!muted) beep(1600, 0x61);
            }

            if (state == STATE_TITLE) {
                if (pressed & (J_START | J_A)) begin_game();
                else update_hero(held, pressed);
            } else if (state == STATE_PLAY) {
                update_play(held, pressed);
            } else if (pressed & (J_START | J_A)) {
                show_title();
            }
        }
        draw_hero();
    }
}
