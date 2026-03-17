// ############################################################################
// ##                                                                        ##
// ##  GRAPHICS.C                                                            ##
// ##                                                                        ##
// ##  AESOP graphics interface for Eye III engine                           ##
// ##                                                                        ##
// ##  Version: 1.00 of 6-May-92 -- Initial version                          ##
// ##                                                                        ##
// ##  Project: Eye III                                                      ##
// ##   Author: John Miles                                                   ##
// ##                                                                        ##
// ##  C source compatible with Borland C++ v3.0 or later                    ##
// ##  Large memory model (16-bit DOS)                                       ##
// ##                                                                        ##
// ############################################################################
// ##                                                                        ##
// ##  Copyright (C) 1992 Miles Design, Inc.                                 ##
// ##                                                                        ##
// ##  Miles Design, Inc.                                                    ##
// ##  10926 Jollyville #308                                                 ##
// ##  Austin, TX 78759                                                      ##
// ##  (512) 345-2642 / BBS (512) 454-9990 / FAX (512) 338-9630              ##
// ##                                                                        ##
// ############################################################################

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "vfx.h"
#include "mouse.h"
#include "gil2vfx.h"

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
#include "utils.h"

VFX_DESC *VFX;

int32_t window_owner[256];

uint32_t lastg_x;
uint32_t lastg_y;
uint32_t lastg_p;

TEXTWINDOW tw[NTW];
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
   uint32_t i, j;

   printf("[graphics] init_graphics\n");

   GIL2VFX_init();

   for (i = 0; i < 256; i++)
      window_owner[i] = -1;

   GIL2VFX_wipe_window(PAGE2, 0);

   txtbuf[sizeof(txtbuf) - 1] = 0x69; // constants for integrity checking
   txtbuf[sizeof(txtbuf) - 2] = 0x77;

   for (i = 0; i < NTW; i++)
   {
      tw[i].font = NULL;
      tw[i].window = 0;
      tw[i].htab = tw[i].vtab = 0;
      tw[i].delay = 0;
      tw[i].continueFunction = NULL;
      tw[i].txtbuf = txtbuf;
      tw[i].txtpnt = txtbuf;
      tw[i].justify = J_LEFT;

      for (j = 0; j < 256; j++)
         tw[i].lookaside[j] = j;

      tw_refresh[i] = -1;

      tw[i].font = (void *)-1U;
   }
}

void shutdown_graphics(void)
{
   printf("[graphics] shutdown_graphics\n");

   GIL2VFX_shutdown_driver();
}

// void release_owned_windows(int32_t owner) // Tom: original
void release_owned_windows(uint32_t owner) // Tom: new
{
   uint32_t i;

   printf("[STUB] [graphics] release_owned_windows: owner=%u\n", owner);

   if (owner == -1)
   {
      for (i = PAGE2 + 1; i < 256; i++)
         if ((window_owner[i] != -1) &&
             (window_owner[i] < NUM_ENTITIES))
         {
            window_owner[i] = -1;
            GIL2VFX_release_window(i);
         }
   }
   else
   {
      for (i = PAGE2 + 1; i < 256; i++)
         if (window_owner[i] == owner)
         {
            window_owner[i] = -1;
            GIL2VFX_release_window(i);
         }
   }
}

void draw_dot(int32_t argcnt, uint32_t page, uint32_t x, uint32_t y, uint32_t color)
{
   printf("[graphics] draw_dot: argcnt=%i page=%u x=%u y=%u color=%u\n", argcnt, page, x, y, color);

   GIL2VFX_draw_dot(lastg_p = page, lastg_x = x, lastg_y = y, color);
}

void draw_line(int32_t argcnt, uint32_t page, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color)
{
   printf("[graphics] draw_line: argcnt=%i page=%u x1=%u y1=%u x2=%u y2=%u color=%u\n", argcnt, page, x1, y1, x2, y2, color);

   GIL2VFX_draw_line(lastg_p = page, x1, y1, lastg_x = x2, lastg_y = y2, color);
}

void line_to(int32_t argcnt, uint32_t x, uint32_t y, uint32_t color, ...) // Tom: TODO
{
   uint32_t i;
   va_list argptr;
   uint32_t cx, cy, cc, lx, ly;

   printf("[graphics] line_to: argcnt=%i x=%u y=%u color=%u\n", argcnt, x, y, color);

   va_start(argptr, argcnt);

   for (i = 0; i < argcnt; i += 3)
   {
      cx = va_arg(argptr, uint32_t);
      cy = va_arg(argptr, uint32_t);
      cc = va_arg(argptr, uint32_t);

      GIL2VFX_draw_line(lastg_p, lastg_x, lastg_y, lx = cx, ly = cy, cc);

      lastg_x = lx;
      lastg_y = ly;
   }

   va_end(argptr);
}

void draw_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
   printf("[graphics] draw_rectangle: argcnt=%i wndnum=%u x1=%i y1=%i x2=%i y2=%i color=%u\n", argcnt, wndnum, x1, y1, x2, y2, color);

   GIL2VFX_draw_rect(wndnum, x1, y1, x2, y2, color);
}

void fill_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
   printf("[graphics] fill_rectangle: argcnt=%i wndnum=%u x1=%i y1=%i x2=%i y2=%i color=%u\n", argcnt, wndnum, x1, y1, x2, y2, color);

   GIL2VFX_fill_rect(wndnum, x1, y1, x2, y2, color);
}

void hash_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
   printf("[graphics] hash_rectangle: argcnt=%i wndnum=%u x1=%i y1=%i x2=%i y2=%i color=%u\n", argcnt, wndnum, x1, y1, x2, y2, color);

   GIL2VFX_hash_rect(wndnum, x1, y1, x2, y2, color);
}

uint32_t get_bitmap_height(int32_t argcnt, uint32_t table, uint32_t number)
{
   uint32_t handle;
   uint32_t h;

   printf("[graphics] get_bitmap_height: argcnt=%i table=%u number=%u\n", argcnt, table, number);

   handle = RTR_get_resource_handle(RTR, table, DA_DEFAULT);

   RTR_lock(RTR, handle);

   h = GIL2VFX_get_bitmap_height(RTR_addr(handle), number);

   RTR_unlock(handle);

   return h;
}

void draw_bitmap(
    int32_t argcnt,
    uint32_t page,
    uint32_t table,
    uint32_t number,
    int32_t x,
    int32_t y,
    uint32_t scale,
    uint32_t flip,
    uint32_t fade_table,
    uint32_t fade_level)
{
   uint32_t handle;
   static uint32_t last_handle = 0;
   static uint32_t last_table;
   uint16_t *lookaside;

   printf("[graphics] draw_bitmap: "
          "argcnt=%i "
          "page=%u "
          "table=%u "
          "number=%u "
          "x=%i "
          "y=%i "
          "scale=%u "
          "flip=%u "
          "fade_table=%u "
          "fade_level=%u"
          "\n",
          argcnt, page, table, number, x, y, scale, flip, fade_table, fade_level);

   if (table == last_table)
      handle = last_handle;
   else
   {
      handle = RTR_get_resource_handle(RTR, table, DA_DEFAULT);
      last_handle = handle;
      last_table = table;
   }

   if ((fade_level > 10) && (!scale))
      scale = 256;

   // lookaside = fade_tables[fade_table][fade_level] - first_color[fade_table]; // Tom: commented out
   lookaside = (uint16_t *)(fade_tables[fade_table][fade_level] - first_color[fade_table]); // Tom: new version

   RTR_lock(RTR, handle);

   // GIL2VFX_draw_bitmap(page, x, y, flip, scale, lookaside, RTR_addr(handle), number); // Tom: commented out
   GIL2VFX_draw_bitmap(page, x, y, flip, scale, (uint8_t *)lookaside, RTR_addr(handle), number); // Tom: new version

   RTR_unlock(handle);
}

uint32_t visible_bitmap_rect(int32_t argcnt, int32_t x, int32_t y, uint32_t flip, uint32_t table, uint32_t number, int16_t *array)
{
   uint32_t handle;
   uint32_t array_offset;
   int8_t *new_array;
   int16_t bounds[4];
   uint32_t result;

   printf("[graphics] visible_bitmap_rect: argcnt=%i x=%i y=%i flip=%u table=%u number=%u array=%p\n", argcnt, x, y, flip, table, number, (void *)array);

   array_offset = (uint32_t)array - (uint32_t)RTR_addr(objlist[current_this]);

   handle = RTR_get_resource_handle(RTR, table, DA_DEFAULT);

   RTR_lock(RTR, handle);

   result = GIL2VFX_visible_bitmap_rect(x, y, flip, RTR_addr(handle), number, bounds);

   new_array = add_ptr(RTR_addr(objlist[current_this]), array_offset);

   far_memmove(new_array, bounds, sizeof(bounds));

   RTR_unlock(handle);

   return result;
}

void set_palette(int32_t argcnt, uint32_t region, uint32_t resource)
{
   uint32_t handle;
   PAL_HDR *PHDR;
   RGB *array;
   int32_t i, j, k, n, f, m, dm, d;
   uint8_t *fade;

   printf("[graphics] set_palette: argcnt=%i region=%u resource=%u\n", argcnt, region, resource);

   handle = RTR_get_resource_handle(RTR, resource, DA_DEFAULT);
   RTR_lock(RTR, handle);

   PHDR = RTR_addr(handle);

   if ((region == PAL_FIXED) ||
       (region == PAL_WALLS) ||
       (region == PAL_M1) ||
       (region == PAL_M2))
   {
      for (i = 0; i < 11; i++)
      {
         fade = add_ptr(PHDR, PHDR->fade[i]);

         for (j = 0; j < PHDR->ncolors; j++)
            fade_tables[region][i][j] = first_color[region] + fade[j];
      }
   }

   array = add_ptr(PHDR, PHDR->RGB);

   for (i = 0; i < PHDR->ncolors; i++)
   {
      if (VFX_DAC_write) // Tom: TODO
         VFX_DAC_write(i + first_color[region], &array[i]);

      if (!(i & 0x0f))
      {
         if (VFX_wait_vblank_leading) // Tom: TODO
            VFX_wait_vblank_leading();
      }
   }

   switch (region)
   {
   case PAL_FIXED:

      for (n = 0, i = BLU_BEG; n < BLU_NUM; n++, i++)
         blu_inten[n] = array[i].r + array[i].g + array[i].b;
      for (n = 0, i = RED_BEG; n < RED_NUM; n++, i++)
         red_inten[n] = array[i].r + array[i].g + array[i].b;
      for (n = 0, i = GRN_BEG; n < GRN_NUM; n++, i++)
         grn_inten[n] = array[i].r + array[i].g + array[i].b;
      for (n = 0, i = GRY_BEG; n < GRY_NUM; n++, i++)
         gry_inten[n] = array[i].r + array[i].g + array[i].b;
      for (n = 0, i = BRN_BEG; n < BRN_NUM; n++, i++)
         brn_inten[n] = array[i].r + array[i].g + array[i].b;

      f = first_color[region];
      n = num_colors[region];
      for (i = 0; i < n; i++)
      {
         j = array[i].r + array[i].g + array[i].b;

         m = 0;
         dm = 32767;
         for (k = 0; k < BLU_NUM; k++)
         {
            d = abs(j - blu_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         F_blu[i] = BLU_BEG + m;

         m = 0;
         dm = 32767;
         for (k = 0; k < GRN_NUM; k++)
         {
            d = abs(j - grn_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         F_grn[i] = GRN_BEG + m;

         m = 0;
         dm = 32767;
         for (k = 0; k < RED_NUM; k++)
         {
            d = abs(j - red_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         F_red[i] = RED_BEG + m;

         m = 0;
         dm = 32767;
         for (k = 0; k < GRY_NUM; k++)
         {
            d = abs(j - gry_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         F_gry[i] = GRY_BEG + m;
      }
      break;

   case PAL_M1:
   case PAL_M2:

      f = first_color[region];
      n = num_colors[region];
      for (i = 0; i < n; i++)
      {
         j = array[i].r + array[i].g + array[i].b;

         m = 0;
         dm = 32767;
         for (k = 0; k < BRN_NUM; k++)
         {
            d = abs(j - brn_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         if (region == PAL_M1)
            M1_brn[i] = BRN_BEG + m;
         else
            M2_brn[i] = BRN_BEG + m;

         m = 0;
         dm = 32767;
         for (k = 0; k < GRY_NUM; k++)
         {
            d = abs(j - gry_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         if (region == PAL_M1)
            M1_gry[i] = GRY_BEG + m;
         else
            M2_gry[i] = GRY_BEG + m;

         m = 0;
         dm = 32767;
         for (k = 0; k < GRN_NUM; k++)
         {
            d = abs(j - grn_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         if (region == PAL_M1)
            M1_grn[i] = GRN_BEG + m;
         else
            M2_grn[i] = GRN_BEG + m;

         m = 0;
         dm = 32767;
         for (k = 0; k < BLU_NUM; k++)
         {
            d = abs(j - blu_inten[k]);
            if (d < dm)
            {
               dm = d;
               m = k;
            }
         }

         if (region == PAL_M1)
            M1_blu[i] = BLU_BEG + m;
         else
            M2_blu[i] = BLU_BEG + m;

         if (region == PAL_M1)
            M1_wht[i] = FIX_WHT;
         else
            M2_wht[i] = FIX_WHT;
      }
      break;
   }

   RTR_unlock(handle);
}

void wait_vertical_retrace(void)
{
   printf("[STUB] [graphics] wait_vertical_retrace\n");

   if (VFX_wait_vblank_leading) // Tom: TODO
      VFX_wait_vblank_leading();
}

uint32_t read_palette(int32_t argcnt, uint32_t regnum)
{
   RGB triplet;
   uint32_t val;

   printf("[STUB] [graphics] read_palette: argcnt=%i regnum=%u\n", argcnt, regnum);

   if (VFX_DAC_read) // Tom: TODO
      VFX_DAC_read(regnum, &triplet);

   val = ((uint32_t)triplet.r << 12) | ((uint32_t)triplet.g << 6) | ((uint32_t)triplet.b);

   return val;
}

void write_palette(int32_t argcnt, uint32_t regnum, uint32_t value)
{
   RGB triplet;

   printf("[STUB] [graphics] write_palette: argcnt=%i regnum=%u value=%u\n", argcnt, regnum, value);

   triplet.r = value >> 12;
   triplet.g = (value >> 6) & 63L;
   triplet.b = value & 63L;

   if (VFX_DAC_write) // Tom: TODO
      VFX_DAC_write(regnum, &triplet);
}

void pixel_fade(int32_t argcnt, uint32_t src_wnd, uint32_t dest_wnd, uint32_t intervals)
{
   printf("[graphics] pixel_fade: argcnt=%i src_wnd=%u dest_wnd=%u intervals=%u\n", argcnt, src_wnd, dest_wnd, intervals);

   GIL2VFX_pixel_fade(src_wnd, dest_wnd, intervals);
}

void color_fade(int32_t argcnt, uint32_t src_wnd, uint32_t dest_wnd)
{
   printf("[graphics] color_fade: argcnt=%i src_wnd=%u dest_wnd=%u\n", argcnt, src_wnd, dest_wnd);

   GIL2VFX_color_fade(src_wnd, dest_wnd);
}

void light_fade(int32_t argcnt, uint32_t src_wnd, uint32_t color)
{
   printf("[graphics] light_fade: arcnt=%i src_wnd=%u color=%u\n", argcnt, src_wnd, color);

   GIL2VFX_light_fade(src_wnd, color);
}

uint32_t assign_window(int32_t argcnt, uint32_t owner, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
   int32_t window;

   printf("[graphics] assign_window: argcnt=%i owner=%u x1=%u y1=%u x2=%u y2=%u\n", argcnt, owner, x1, y1, x2, y2);

   window = GIL2VFX_assign_window(x1, y1, x2, y2);

   window_owner[window] = owner;

   return window;
}

uint32_t assign_subwindow(int32_t argcnt, uint32_t owner, uint32_t parent, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
   int32_t window;

   printf("[graphics] assign_subwindow: argcnt=%i owner=%u parent=%u x1=%u y1=%u x2=%u y2=%u\n", argcnt, owner, parent, x1, y1, x2, y2);

   window = GIL2VFX_assign_subwindow(parent, x1, y1, x2, y2);

   window_owner[window] = owner;

   return window;
}

void release_window(int32_t argcnt, uint32_t window)
{
   printf("[graphics] release_window: argcnt=%i window=%u\n", argcnt, window);

   GIL2VFX_release_window(window);
   window_owner[window] = -1;
}

uint32_t get_x1(int32_t argcnt, uint32_t window)
{
   printf("[graphics] get_x1: argcnt=%i window=%u\n", argcnt, window);

   return GIL2VFX_get_x1(window);
}

uint32_t get_x2(int32_t argcnt, uint32_t window)
{
   printf("[graphics] get_x2: argcnt=%i window=%u\n", argcnt, window);

   return GIL2VFX_get_x2(window);
}

uint32_t get_y1(int32_t argcnt, uint32_t window)
{
   printf("[graphics] get_y1: argcnt=%i window=%u\n", argcnt, window);

   return GIL2VFX_get_y1(window);
}

uint32_t get_y2(int32_t argcnt, uint32_t window)
{
   printf("[graphics] get_y2: argcnt=%i window=%u\n", argcnt, window);

   return GIL2VFX_get_y2(window);
}

void set_x1(int32_t argcnt, uint32_t window, uint32_t x1)
{
   printf("[graphics] set_x1: argcnt=%i window=%u x1=%u\n", argcnt, window, x1);

   GIL2VFX_set_x1(window, x1);
}

void set_x2(int32_t argcnt, uint32_t window, uint32_t x2)
{
   printf("[graphics] set_x2: argcnt=%i window=%u x2=%u\n", argcnt, window, x2);

   GIL2VFX_set_x2(window, x2);
}

void set_y1(int32_t argcnt, uint32_t window, uint32_t y1)
{
   printf("[graphics] set_y1: argcnt=%i window=%u y1=%u\n", argcnt, window, y1);

   GIL2VFX_set_y1(window, y1);
}

void set_y2(int32_t argcnt, uint32_t window, uint32_t y2)
{
   printf("[graphics] set_y2: argcnt=%i window=%u y2=%u\n", argcnt, window, y2);

   GIL2VFX_set_y2(window, y2);
}

void wipe_window(int32_t argcnt, uint32_t window, uint32_t color)
{
   printf("[graphics] wipe_window: argcnt=%i window=%u color=%u\n", argcnt, window, color);

   GIL2VFX_wipe_window(window, color);
}

void text_window(int32_t argcnt, uint32_t wndnum, uint32_t wnd)
{
   printf("[graphics] text_window: argcnt=%i wndnum=%u wnd=%u\n", argcnt, wndnum, wnd);

   tw[wndnum].window = wnd;
}

void text_style(int32_t argcnt, uint32_t wndnum, uint32_t font, uint32_t justify)
{
   uint32_t hfont;
   FONT *vfont;

   printf("[graphics] text_style: argcnt=%i wndnum=%u font=%u justify=%u\n", argcnt, wndnum, font, justify);

   tw[wndnum].justify = justify;

   hfont = (uint32_t)tw[wndnum].font;

   if (hfont != UINT32_MAX)
   {
      RTR_unlock(hfont);
   }

   tw[wndnum].font = (void *)RTR_get_resource_handle(RTR, font, DA_DEFAULT);

   RTR_lock(RTR, (uint32_t)tw[wndnum].font);

   vfont = (FONT *)RTR_addr((uint32_t)tw[wndnum].font);

   tw[wndnum].lookaside[vfont->font_background] = 255;
}

void text_xy(int32_t argcnt, uint32_t wndnum, uint32_t htab, uint32_t vtab)
{
   printf("[graphics] text_xy: argcnt=%i wndnum=%u htab=%u vtab=%u\n", argcnt, wndnum, htab, vtab);

   tw[wndnum].htab = htab;
   tw[wndnum].vtab = vtab;
}

int32_t get_text_x(int32_t argcnt, uint32_t wndnum)
{
   printf("[graphics] get_text_x: argcnt=%i wndnum=%u\n", argcnt, wndnum);

   return tw[wndnum].htab;
}

int32_t get_text_y(int32_t argcnt, uint32_t wndnum)
{
   printf("[graphics] get_text_y: argcnt=%i wndnum=%u\n", argcnt, wndnum);

   return tw[wndnum].vtab;
}

void home(int32_t argcnt, uint32_t wndnum)
{
   uint32_t hfont;

   printf("[graphics] home: argcnt=%i wndnum=%u\n", argcnt, wndnum);

   hfont = (uint32_t)tw[wndnum].font;
   tw[wndnum].font = RTR_addr(hfont);

   GIL2VFX_select_text_window(&tw[wndnum]);
   GIL2VFX_home();

   tw[wndnum].font = (void *)hfont;
}

void text_color(int32_t argcnt, uint32_t wndnum, uint32_t current, uint32_t new)
{
   uint32_t hfont;

   printf("[graphics] text_color: argcnt=%i wndnum=%u current=%u new=%u\n", argcnt, wndnum, current, new);

   hfont = (uint32_t)tw[wndnum].font;
   tw[wndnum].font = RTR_addr(hfont);

   GIL2VFX_select_text_window(&tw[wndnum]);
   GIL2VFX_remap_font_color(current, new);

   tw[wndnum].font = (void *)hfont;
}

void text_refresh_window(int32_t argcnt, uint32_t wndnum, int32_t wnd)
{
   printf("[graphics] text_refresh_window: argcnt=%i wndnum=%u wnd=%i\n", argcnt, wndnum, wnd);

   tw_refresh[wndnum] = wnd;
}

void vsprint(int32_t argcnt, uint32_t wndnum, int8_t *format, va_list argptr)
{
   uint32_t hfont, str;
   int8_t c, *p, *s;
   int8_t buff[32];

   printf("[graphics] vsprint: argcnt=%i wndnum=%u format=%p argprt=%p\n", argcnt, wndnum, (void *)format, (void *)argptr);

   hfont = (uint32_t)tw[wndnum].font;
   tw[wndnum].font = RTR_addr(hfont);

   GIL2VFX_select_text_window(&tw[wndnum]);

   GIL2VFX_print(BUF, "");
   txtbuf[0] = 0; // bug in GIL?  GIL_print("") above should do this!

   p = format;

   while ((c = *p++) != 0)
   {
      if (c != '%')
      {
         GIL2VFX_print(APP, "%c", c);
         continue;
      }

      switch (c = (*p++))
      {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
         GIL2VFX_remap_font_color(15, text_colors[c - '0']);
         break;

      case 'd':
         GIL2VFX_print(APP, "%s", ltoa(va_arg(argptr, int32_t), buff, 10));
         break;

      case 'u':
         GIL2VFX_print(APP, "%s", ultoa(va_arg(argptr, uint32_t), buff, 10));
         break;

      case 'x':
      case 'X':
         GIL2VFX_print(APP, "%s", ltoa(va_arg(argptr, int32_t), buff, 16));
         break;

      case 's':
         str = RTR_get_resource_handle(RTR, va_arg(argptr, uint32_t), DA_DEFAULT);

         RTR_lock(RTR, str);

         s = RTR_addr(str);

         switch (*(uint16_t *)s)
         {
         case ':S':
            GIL2VFX_print(APP, "%s", &s[2]);
            break;

         default:
            abend(MSG_SRRV);
         }

         RTR_unlock(str);

         tw[wndnum].font = RTR_addr(hfont);
         break;

      case 'a':
         GIL2VFX_print(APP, va_arg(argptr, int8_t *));
         break;

      case 'c':
         GIL2VFX_print(APP, "%c", (uint8_t)(va_arg(argptr, uint32_t) & 0xffL));
         break;
      }
   }

   if ((txtbuf[sizeof(txtbuf) - 1] != 0x69) ||
       (txtbuf[sizeof(txtbuf) - 2] != 0x77))
      abend(MSG_TBO);

   GIL2VFX_print_buffer(0);

   tw[wndnum].font = (void *)hfont;

   if (tw_refresh[wndnum] != -1)
      refresh_window(0, tw[wndnum].window, tw_refresh[wndnum]);
}

void print(int32_t argcnt, uint32_t wndnum, uint32_t format, ...)
{
   va_list argptr;
   uint32_t hstring;
   int8_t *p;
   uint16_t *w;

   printf("[graphics] print: argcnt=%i wndnum=%u format=%u\n", argcnt, wndnum, format);

   hstring = RTR_get_resource_handle(RTR, format, DA_DEFAULT);

   RTR_lock(RTR, hstring);

   p = RTR_addr(hstring);

   w = (uint16_t *)p;

   switch (*w)
   {
   case ':S':
      p += 2;
      break;

   default:
      abend(MSG_SRRP);
   }

   printf("[graphics] print: string=%s\n", p);

   va_start(argptr, format);

   if (strlen(p) < 256)
   {
      strcpy(strbuf, p);
      p = strbuf;
   }

   vsprint(argcnt, wndnum, p, argptr);

   va_end(argptr);

   RTR_unlock(hstring);
}

void sprint(int32_t argcnt, uint32_t wndnum, int8_t *format, ...)
{
   va_list argptr;
   int8_t *p;

   printf("[graphics] sprint: argcnt=%i wndnum=%u format=%p\n", argcnt, wndnum, (void *)format);

   p = format;

   va_start(argptr, format);

   if (strlen(p) < 256)
   {
      strcpy(strbuf, p);
      p = strbuf;
   }

   vsprint(argcnt, wndnum, p, argptr);

   va_end(argptr);
}

void dprint(int32_t argcnt, int8_t *format, ...)
{
   va_list argptr;
   int8_t *p;

   printf("[graphics] dprint: argcnt=%i format=%p\n", argcnt, (void *)format);

   p = format;

   va_start(argptr, format);

   text_color(0, 0, 15, DK_RED);
   text_color(0, 0, 0, 255);

   vsprint(argcnt, 0, p, argptr);

   va_end(argptr);
}

void crout(int32_t argcnt, uint32_t wndnum)
{
   printf("[graphics] crout: argcnt=%i wndnum=%u\n", argcnt, wndnum);

   sprint(argcnt, wndnum, "\n");
}

uint32_t char_width(int32_t argcnt, uint32_t wndnum, uint32_t ch)
{
   uint32_t hfont;
   uint32_t w;

   printf("[graphics] char_width: argcnt=%i wndnum=%u ch=%u\n", argcnt, wndnum, ch);

   hfont = (uint32_t)tw[wndnum].font;
   tw[wndnum].font = RTR_addr(hfont);

   GIL2VFX_select_text_window(&tw[wndnum]);
   w = GIL2VFX_char_width(ch);

   tw[wndnum].font = (void *)hfont;

   return w;
}

uint32_t font_height(int32_t argcnt, uint32_t wndnum)
{
   uint32_t hfont;
   uint32_t w;

   printf("[graphics] font_height: argcnt=%i wndnum=%u\n", argcnt, wndnum);

   hfont = (uint32_t)tw[wndnum].font;

   w = ((FONT *)(RTR_addr(hfont)))->char_height;

   return w;
}

void solid_bar_graph(int32_t argcnt, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t lb_border, uint32_t tr_border, uint32_t bkgnd, uint32_t grn, uint32_t yel, uint32_t red, int32_t val, int32_t min, int32_t crit, int32_t max)
{
   int32_t btop, bbtm, blft, brgt, grayx;
   int32_t range, point, width;
   int32_t color;

   printf("[graphics] solid_bar_graph: argcnt=%i x0=%i y0=%i x1=%i y1=%i lb_border=%u tr_border=%u bkgnd=%u grn=%u yel=%u red=%u val=%i min=%i crit=%i max=%i\n", argcnt, x0, y0, x1, y1, lb_border, tr_border, bkgnd, grn, yel, red, val, min, crit, max);

   GIL2VFX_draw_line(PAGE2, x0, y0, x0, y1, lb_border);
   GIL2VFX_draw_line(PAGE2, x0, y1, x1, y1, lb_border);
   GIL2VFX_draw_line(PAGE2, x1, y1 - 1, x1, y0, tr_border);
   GIL2VFX_draw_line(PAGE2, x1, y0, x0 + 1, y0, tr_border);

   btop = y0 + 1;
   bbtm = y1 - 1;

   blft = x0 + 1;
   brgt = x1 - 1;

   width = brgt - blft;

   if (val > max)
      val = max;
   else if (val < min)
      val = min;

   range = max - min;
   point = val - min;

   grayx = blft + (point * width / range);

   if (grayx != brgt)
      GIL2VFX_fill_rect(PAGE2, grayx, btop, brgt, bbtm, bkgnd);

   if (val <= crit)
      color = red;
   else
      color = (val * 3 >= max) ? grn : yel;

   if ((val != min) && (grayx == blft))
      grayx = blft + 1;

   if (grayx != blft)
      GIL2VFX_fill_rect(PAGE2, blft, btop, grayx, bbtm, color);
}

void aprint(int32_t argcnt, int8_t *format, ...)
{
   va_list argptr;

   printf("[graphics] aprint argcnt=%i format=%p\n", argcnt, (void *)format);

   va_start(argptr, format);

   vprintf(format, argptr);

   va_end(argptr);
}
