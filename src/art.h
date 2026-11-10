/* Tile numbers and palettes of the game. Generated once, then yours to change. */
#ifndef ART_H
#define ART_H

#include <stdint.h>
#include <gb/cgb.h>

#define BKG_TILE_COUNT 87
#define OBJ_TILE_COUNT 15
#define T_SKY 0
#define T_FONT 1
#define T_DITHER1 42
#define T_SOLID1 43
#define T_DITHER2 44
#define T_SOLID2 45
#define T_DITHER3 46
#define T_SOLID3 47
#define T_BUILDING 48
#define T_WINDOW 49
#define T_GRASS 50
#define T_DIRT 51
#define T_DIRT_DARK 52
#define T_STAR 53
#define T_HEART 54
#define T_MOON 55
#define T_BIG 59
#define S_HERO 0
#define S_BIT 12
#define S_SLOP 13

#define PAL_SKY 0
#define PAL_CITY 1
#define PAL_GROUND 2
#define PAL_TEXT 3
#define PAL_GROUND_TEXT 4
#define PAL_MOON 5
#define PAL_BKG_COUNT 6

#define PAL_OBJ_HERO 0
#define PAL_OBJ_BIT 1
#define PAL_OBJ_SLOP 2
#define PAL_OBJ_COUNT 3

#define PAL_SET_COUNT 3

extern const uint8_t bkg_tiles[];
extern const uint8_t obj_tiles[];
extern const palette_color_t pal_bkg[PAL_SET_COUNT][PAL_BKG_COUNT * 4];
extern const palette_color_t pal_obj[PAL_SET_COUNT][PAL_OBJ_COUNT * 4];

#endif
