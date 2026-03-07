// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  GRAPHICS.C                                                            ��
// ��                                                                        ��
// ��  AESOP graphics interface for Eye III engine                           ��
// ��                                                                        ��
// ��  Version: 1.00 of 6-May-92 -- Initial version                          ��
// ��                                                                        ��
// ��  Project: Eye III                                                      ��
// ��   Author: John Miles                                                   ��
// ��                                                                        ��
// ��  C source compatible with Borland C++ v3.0 or later                    ��
// ��  Large memory model (16-bit DOS)                                       ��
// ��                                                                        ��
// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  Copyright (C) 1992 Miles Design, Inc.                                 ��
// ��                                                                        ��
// ��  Miles Design, Inc.                                                    ��
// ��  10926 Jollyville #308                                                 ��
// ��  Austin, TX 78759                                                      ��
// ��  (512) 345-2642 / BBS (512) 454-9990 / FAX (512) 338-9630              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h> // Tom: added

#include "mouse.h"

#include "defs.h"
#include "shared.h"
#include "rtsystem.h"
#include "rtmsg.h"
#include "rtres.h"
#include "rtlink.h"
#include "rt.h"
#include "intrface.h"
#include "rtobject.h"
#include "graphics.h"

int32_t window_owner[256];

uint32_t lastg_x;
uint32_t lastg_y;
uint32_t lastg_p;

// TEXTWINDOW tw[NTW]; // Tom: commented out
// WINDOW tw_refresh[NTW];
int32_t tw_refresh[NTW];

int8_t strbuf[256];  // used for string resource buffering
int8_t txtbuf[2402]; // used for word-wrapped text (also used as
                     // buffer for dot pattern effects in EYE.C!)
                     // Final 2 bytes used for overflow checking
#define FBEG 0
#define FCNT 192

uint8_t F_fade[11][256]; // fixed      00-AF (also initializes B0-FF)
uint8_t W_fade[11][16];  // wallset    B0-BF
uint8_t M1_fade[11][32]; // monster #1 C0-DF
uint8_t M2_fade[11][32]; // monster #2 E0-FF

uint8_t M1_gry[32];
uint8_t M1_wht[32];
uint8_t M2_gry[32];
uint8_t M2_wht[32];
uint8_t M1_blu[32];
uint8_t M2_blu[32];
uint8_t M1_grn[32];
uint8_t M2_grn[32];
uint8_t M1_brn[32];
uint8_t M2_brn[32];
uint8_t F_grn[256];
uint8_t F_blu[256];
uint8_t F_red[256];
uint8_t F_gry[256];

uint16_t first_color[5] = {0x00, 0xb0, 0xc0, 0xe0, 0xb0};
uint16_t num_colors[5] = {256, 16, 32, 32, 80};

uint8_t *fade_tables[5][16] =
    {{F_fade[0], F_fade[1], F_fade[2], F_fade[3],
      F_fade[4], F_fade[5], F_fade[6], F_fade[7],
      F_fade[8], F_fade[9], F_fade[10],
      F_blu, F_grn, F_red, F_gry, NULL},

     {W_fade[0], W_fade[1], W_fade[2], W_fade[3],
      W_fade[4], W_fade[5], W_fade[6], W_fade[7],
      W_fade[8], W_fade[9], W_fade[10],
      NULL, NULL, NULL, NULL, NULL},

     {M1_fade[0], M1_fade[1], M1_fade[2], M1_fade[3],
      M1_fade[4], M1_fade[5], M1_fade[6], M1_fade[7],
      M1_fade[8], M1_fade[9], M1_fade[10],
      M1_gry, M1_wht, M1_grn, M1_blu, M1_brn},

     {M2_fade[0], M2_fade[1], M2_fade[2], M2_fade[3],
      M2_fade[4], M2_fade[5], M2_fade[6], M2_fade[7],
      M2_fade[8], M2_fade[9], M2_fade[10],
      M2_gry, M2_wht, M2_grn, M2_blu, M2_brn},

     {NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL,
      NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL}};

//
// Fixed-palette color ranges for magical auras, poison effects,
// petrification effects, damage effects, etc.
//

#define BLU_BEG 0x55
#define BLU_NUM 11
#define GRN_BEG 0x70
#define GRN_NUM 11
#define GRY_BEG 0x67
#define GRY_NUM 9
#define RED_BEG 0x22
#define RED_NUM 9
#define BRN_BEG 0x96
#define BRN_NUM 7
#define FIX_WHT 0x0b

uint8_t blu_inten[BLU_NUM];
uint8_t grn_inten[GRN_NUM];
uint8_t gry_inten[GRY_NUM];
uint8_t red_inten[RED_NUM];
uint8_t brn_inten[BRN_NUM];

uint8_t text_colors[9] =
    {
        DK_GRN,
        LT_GRN,
        YEL,
        LT_RED,
        DK_RED,
        BLK,
        WHT,
        WHT,
        WHT};

void init_graphics(void)
{
   printf("[STUB] graphics:init_graphics: void\n");
}

void shutdown_graphics(void)
{
   printf("[STUB] graphics:shutdown_graphics: void\n");
}

// void release_owned_windows(int32_t owner) // Tom: original
void release_owned_windows(uint32_t owner) // Tom: new
{
   (void)owner;
   printf("[STUB] graphics:release_owned_windows: owner=%d\n", owner);
}

void draw_dot(int32_t argcnt, uint32_t page, uint32_t x, uint32_t y, uint32_t color)
{
   (void)argcnt;
   (void)page;
   (void)x;
   (void)y;
   (void)color;
   printf("[STUB] graphics:draw_dot: argcnt=%i page=%u x=%u y=%u color=%u\n", argcnt, page, x, y, color);
}

void draw_line(int32_t argcnt, uint32_t page, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color)
{
   (void)argcnt;
   (void)page;
   (void)x1;
   (void)y1;
   (void)x2;
   (void)y2;
   (void)color;
   printf("[STUB] graphics:draw_line: argcnt=%i page=%u x1=%u y1=%u x2=%u y2=%u color=%u\n", argcnt, page, x1, y1, x2, y2, color);
}

void line_to(int32_t argcnt, uint32_t x, uint32_t y, uint32_t color, ...) // Tom: TODO
{
   (void)argcnt;
   (void)x;
   (void)y;
   (void)color;
   printf("[STUB] graphics:line_to: argcnt=%i x=%u y=%u color=%u\n", argcnt, x, y, color);
}

void draw_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
   (void)argcnt;
   (void)wndnum;
   (void)x1;
   (void)y1;
   (void)x2;
   (void)y2;
   (void)color;
   printf("[STUB] graphics:draw_rectangle: argcnt=%i wndnum=%u x1=%i y1=%i x2=%i y2=%i color=%u\n", argcnt, wndnum, x1, y1, x2, y2, color);
}

void fill_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
   (void)argcnt;
   (void)wndnum;
   (void)x1;
   (void)y1;
   (void)x2;
   (void)y2;
   (void)color;
   printf("[STUB] graphics:fill_rectangle: argcnt=%i wndnum=%u x1=%i y1=%i x2=%i y2=%i color=%u\n", argcnt, wndnum, x1, y1, x2, y2, color);
}

void hash_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
   (void)argcnt;
   (void)wndnum;
   (void)x1;
   (void)y1;
   (void)x2;
   (void)y2;
   (void)color;
   printf("[STUB] graphics:hash_rectangle: argcnt=%i wndnum=%u x1=%i y1=%i x2=%i y2=%i color=%u\n", argcnt, wndnum, x1, y1, x2, y2, color);
}

uint32_t get_bitmap_height(int32_t argcnt, uint32_t table, uint32_t number)
{
   (void)argcnt;
   (void)table;
   (void)number;
   printf("[STUB] graphics:get_bitmap_height: argcnt=%i table=%u number=%u\n", argcnt, table, number);
   return 0;
}

void draw_bitmap(int32_t argcnt, uint32_t page, uint32_t table, uint32_t number, int32_t x, int32_t y, uint32_t scale, uint32_t flip, uint32_t fade_table, uint32_t fade_level)
{
   (void)argcnt;
   (void)page;
   (void)table;
   (void)number;
   (void)x;
   (void)y;
   (void)scale;
   (void)flip;
   (void)fade_table;
   (void)fade_level;
   printf("[STUB] graphics:draw_bitmap: argcnt=%i page=%u table=%u number=%u x=%i y=%i scale=%u flip=%u fade_table=%u fade_level=%u\n", argcnt, page, table, number, x, y, scale, flip, fade_table, fade_level);
}

uint32_t visible_bitmap_rect(int32_t argcnt, int32_t x, int32_t y, uint32_t flip, uint32_t table, uint32_t number, int16_t *array)
{
   (void)argcnt;
   (void)x;
   (void)y;
   (void)flip;
   (void)table;
   (void)number;
   (void)array;
   printf("[STUB] graphics:visible_bitmap_rect: argcnt=%i x=%i y=%i flip=%u table=%u number=%u array=%i\n", argcnt, x, y, flip, table, number, array);
   return 0;
}

void set_palette(int32_t argcnt, uint32_t region, uint32_t resource)
{
   (void)argcnt;
   (void)region;
   (void)resource;
   printf("[STUB] graphics:set_palette: argcnt=%i region=%u resource=%u\n", argcnt, region, resource);
}

void wait_vertical_retrace(void)
{
   printf("[STUB] graphics:wait_vertical_retrace: void\n");
}

uint32_t read_palette(int32_t argcnt, uint32_t regnum)
{
   (void)argcnt;
   (void)regnum;
   printf("[STUB] graphics:read_palette: argcnt=%i regnum=%u\n", argcnt, regnum);
   return 0;
}

void write_palette(int32_t argcnt, uint32_t regnum, uint32_t value)
{
   (void)argcnt;
   (void)regnum;
   (void)value;
   printf("[STUB] graphics:write_palette: argcnt=%i regnum=%u value=%u\n", argcnt, regnum, value);
}

void pixel_fade(int32_t argcnt, uint32_t src_wnd, uint32_t dest_wnd, uint32_t intervals)
{
   (void)argcnt;
   (void)src_wnd;
   (void)dest_wnd;
   (void)intervals;
   printf("[STUB] graphics:pixel_fade: argcnt=%i src_wnd=%u dest_wnd=%u intervals=%u\n", argcnt, src_wnd, dest_wnd, intervals);
}

void color_fade(int32_t argcnt, uint32_t src_wnd, uint32_t dest_wnd)
{
   (void)argcnt;
   (void)src_wnd;
   (void)dest_wnd;
   printf("[STUB] graphics:color_fade: argcnt=%i src_wnd=%u dest_wnd=%u\n", argcnt, src_wnd, dest_wnd);
}

void light_fade(int32_t argcnt, uint32_t src_wnd, uint32_t color)
{
   (void)argcnt;
   (void)src_wnd;
   (void)color;
   printf("[STUB] graphics:light_fade: arcnt=%i src_wnd=%u color=%u\n", argcnt, src_wnd, color);
}

uint32_t assign_window(int32_t argcnt, uint32_t owner, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
   (void)argcnt;
   (void)owner;
   (void)x1;
   (void)y1;
   (void)x2;
   (void)y2;
   printf("[STUB] graphics:assign_window: argcnt=%i owner=%u x1=%u y1=%u x2=%u y2=%u\n", argcnt, owner, x1, y1, x2, y2);
   return 0;
}

uint32_t assign_subwindow(int32_t argcnt, uint32_t owner, uint32_t parent, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
   (void)argcnt;
   (void)owner;
   (void)parent;
   (void)x1;
   (void)y1;
   (void)x2;
   (void)y2;
   printf("[STUB] graphics:assign_subwindow: argcnt=%i owner=%u parent=%u x1=%u y1=%u x2=%u y2=%u\n", argcnt, owner, parent, x1, y1, x2, y2);
   return 0;
}

void release_window(int32_t argcnt, uint32_t window)
{
   (void)argcnt;
   (void)window;
   printf("[STUB] graphics:release_window: argcnt=%i window=%u\n", argcnt, window);
}

uint32_t get_x1(int32_t argcnt, uint32_t window)
{
   (void)argcnt;
   (void)window;
   printf("[STUB] graphics:get_x1: argcnt=%i window=%u\n", argcnt, window);
   return 0;
}

uint32_t get_x2(int32_t argcnt, uint32_t window)
{
   (void)argcnt;
   (void)window;
   printf("[STUB] graphics:get_x2: argcnt=%i window=%u\n", argcnt, window);
   return 0;
}

uint32_t get_y1(int32_t argcnt, uint32_t window)
{
   (void)argcnt;
   (void)window;
   printf("[STUB] graphics:get_y1: argcnt=%i window=%u\n", argcnt, window);
   return 0;
}

uint32_t get_y2(int32_t argcnt, uint32_t window)
{
   (void)argcnt;
   (void)window;
   printf("[STUB] graphics:get_y2: argcnt=%i window=%u\n", argcnt, window);
   return 0;
}

void set_x1(int32_t argcnt, uint32_t window, uint32_t x1)
{
   (void)argcnt;
   (void)window;
   (void)x1;
   printf("[STUB] graphics:set_x1: argcnt=%i window=%u x1=%u\n", argcnt, window, x1);
}

void set_x2(int32_t argcnt, uint32_t window, uint32_t x2)
{
   (void)argcnt;
   (void)window;
   (void)x2;
   printf("[STUB] graphics:set_x2: argcnt=%i window=%u x2=%u\n", argcnt, window, x2);
}

void set_y1(int32_t argcnt, uint32_t window, uint32_t y1)
{
   (void)argcnt;
   (void)window;
   (void)y1;
   printf("[STUB] graphics:set_y1: argcnt=%i window=%u y1=%u\n", argcnt, window, y1);
}

void set_y2(int32_t argcnt, uint32_t window, uint32_t y2)
{
   (void)argcnt;
   (void)window;
   (void)y2;
   printf("[STUB] graphics:set_y2: argcnt=%i window=%u y2=%u\n", argcnt, window, y2);
}

void wipe_window(int32_t argcnt, uint32_t window, uint32_t color)
{
   (void)argcnt;
   (void)window;
   (void)color;
   printf("[STUB] graphics:wipe_window: argcnt=%i window=%u color=%u\n", argcnt, window, color);
}

void text_window(int32_t argcnt, uint32_t wndnum, uint32_t wnd)
{
   (void)argcnt;
   (void)wndnum;
   (void)wnd;
   printf("[STUB] graphics:text_window: argcnt=%i wndnum=%u wnd=%u\n", argcnt, wndnum, wnd);
}

void text_style(int32_t argcnt, uint32_t wndnum, uint32_t font, uint32_t justify)
{
   (void)argcnt;
   (void)wndnum;
   (void)font;
   (void)justify;
   printf("[STUB] graphics:text_style: argcnt=%i wndnum=%u font=%u justify=%u\n", argcnt, wndnum, font, justify);
}

void text_xy(int32_t argcnt, uint32_t wndnum, uint32_t htab, uint32_t vtab)
{
   (void)argcnt;
   (void)wndnum;
   (void)htab;
   (void)vtab;
   printf("[STUB] graphics:text_xy: argcnt=%i wndnum=%u htab=%u vtab=%u\n", argcnt, wndnum, htab, vtab);
}

int32_t get_text_x(int32_t argcnt, uint32_t wndnum)
{
   (void)argcnt;
   (void)wndnum;
   printf("[STUB] graphics:get_text_x: argcnt=%i wndnum=%u\n", argcnt, wndnum);
   return 0;
}

int32_t get_text_y(int32_t argcnt, uint32_t wndnum)
{
   (void)argcnt;
   (void)wndnum;
   printf("[STUB] graphics:get_text_y: argcnt=%i wndnum=%u\n", argcnt, wndnum);
   return 0;
}

void home(int32_t argcnt, uint32_t wndnum)
{
   (void)argcnt;
   (void)wndnum;
   printf("[STUB] graphics:home: argcnt=%i wndnum=%u\n", argcnt, wndnum);
}

void text_color(int32_t argcnt, uint32_t wndnum, uint32_t current, uint32_t new)
{
   (void)argcnt;
   (void)wndnum;
   (void)current;
   (void)new;
   printf("[STUB] graphics:text_color: argcnt=%i wndnum=%u current=%u new=%u\n", argcnt, wndnum, current, new);
}

void text_refresh_window(int32_t argcnt, uint32_t wndnum, int32_t wnd)
{
   (void)argcnt;
   (void)wndnum;
   (void)wnd;
   printf("[STUB] graphics:text_refresh_window: argcnt=%i wndnum=%u wnd=%i\n", argcnt, wndnum, wnd);
}

void vsprint(int32_t argcnt, uint32_t wndnum, int8_t *format, va_list argptr)
{
   (void)argcnt;
   (void)wndnum;
   (void)format;
   (void)argptr;
   printf("[STUB] graphics:vsprint: argcnt=%i wndnum=%u format=%i argprt=%i\n", argcnt, wndnum, format, argptr);
}

void print(int32_t argcnt, uint32_t wndnum, uint32_t format, ...)
{
   (void)argcnt;
   (void)wndnum;
   (void)format;
   printf("[STUB] graphics:print: argcnt=%i wndnum=%u format=%u\n", argcnt, wndnum, format);
}

void sprint(int32_t argcnt, uint32_t wndnum, int8_t *format, ...)
{
   (void)argcnt;
   (void)wndnum;
   (void)format;
   printf("[STUB] graphics:sprint: argcnt=%i wndnum=%u format=%i\n", argcnt, wndnum, format);
}

void dprint(int32_t argcnt, int8_t *format, ...)
{
   (void)argcnt;
   (void)format;
   printf("[STUB] graphics:dprint: argcnt=%i format=%i\n", argcnt, format);
}

void crout(int32_t argcnt, uint32_t wndnum)
{
   (void)argcnt;
   (void)wndnum;
   printf("[STUB] graphics:crout: argcnt=%i wndnum=%u\n", argcnt, wndnum);
}

uint32_t char_width(int32_t argcnt, uint32_t wndnum, uint32_t ch)
{
   (void)argcnt;
   (void)wndnum;
   (void)ch;
   printf("[STUB] graphics:char_width: argcnt=%i wndnum=%u ch=%u\n", argcnt, wndnum, ch);
   return 0;
}

uint32_t font_height(int32_t argcnt, uint32_t wndnum)
{
   (void)argcnt;
   (void)wndnum;
   printf("[STUB] graphics:font_height: argcnt=%i wndnum=%u\n", argcnt, wndnum);
   return 0;
}

void solid_bar_graph(int32_t argcnt, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t lb_border, uint32_t tr_border, uint32_t bkgnd, uint32_t grn, uint32_t yel, uint32_t red, int32_t val, int32_t min, int32_t crit, int32_t max)
{
   (void)argcnt;
   (void)x0;
   (void)y0;
   (void)x1;
   (void)y1;
   (void)lb_border;
   (void)tr_border;
   (void)bkgnd;
   (void)grn;
   (void)yel;
   (void)red;
   (void)val;
   (void)min;
   (void)crit;
   (void)max;
   printf("[STUB] graphics:solid_bar_graph: argcnt=%i x0=%i y0=%i x1=%i y1=%i lb_border=%u tr_border=%u bkgnd=%u grn=%u yel=%u red=%u val=%i min=%i crit=%i max=%i\n", argcnt, x0, y0, x1, y1, lb_border, tr_border, bkgnd, grn, yel, red, val, min, crit, max);
}

void aprint(int32_t argcnt, int8_t *format, ...)
{
   (void)argcnt;
   (void)format;
   printf("[STUB] graphics:aprint argcnt=%i format=%i\n", argcnt, format);
}
