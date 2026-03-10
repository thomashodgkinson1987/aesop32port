// ############################################################################
// ##                                                                        ##
// ##  EYE.C                                                                 ##
// ##                                                                        ##
// ##  Eye III engine support functions                                      ##
// ##                                                                        ##
// ##  Version: 1.00 of 28-Oct-92 -- Initial version                         ##
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
#include <sys/types.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "vfx.h"

#include "defs.h"
#include "shared.h"
#include "rtcode.h"
#include "rtsystem.h"
#include "rtmsg.h"
#include "rtobject.h"
#include "graphics.h"
#include "event.h"
#include "intrface.h"
#include "rtres.h"
#include "rt.h"
#include "sound.h"

#define SAVETYPE (diag_flag ? SF_TXT : SF_BIN)

#define SAVEDIR_FN "SAVEGAME/SAVEGAME.DIR"

int8_t savegame_dir[NUM_SAVEGAMES][SAVE_LEN + 1];

static int8_t items_bin[] = "SAVEGAME/ITEMS_yy.BIN";
static int8_t items_txt[] = "SAVEGAME/ITEMS_yy.TXT";
static int8_t lvl_bin[] = "SAVEGAME/LVLxx_yy.BIN";
static int8_t lvl_txt[] = "SAVEGAME/LVLxx_yy.TXT";

static int8_t lvl_tmp[] = "SAVEGAME/LVLxx.TMP";
static int8_t itm_tmp[] = "SAVEGAME/ITEMS.TMP";

int8_t DX_offset[6][4] = {{0, 0, 0, 0},
                          {0, 1, 0, -1},
                          {0, 0, 0, 0},
                          {-1, 0, 1, 0},
                          {0, -1, 0, 1},
                          {1, 0, -1, 0}};

int8_t DY_offset[6][4] = {{0, 0, 0, 0},
                          {-1, 0, 1, 0},
                          {0, 0, 0, 0},
                          {0, -1, 0, 1},
                          {1, 0, -1, 0},
                          {0, 1, 0, -1}};

extern int8_t txtbuf[2400]; // used as dot buffer -- needs 8 * MAXDOTS words

/*********************************************************/
int32_t step_X(int32_t argcnt, uint32_t x, uint32_t fdir, uint32_t mtype, uint32_t distance)
{
   int8_t xx = (int8_t)x;
   (void)argcnt;

   printf("[eye] step_X\n");

   if (!distance)
      return x;

   if (mtype == MTYP_ML)
   {
      xx += DX_offset[MTYP_F - 1][(uint8_t)fdir];
      xx += DX_offset[MTYP_L - 1][(uint8_t)fdir];
   }
   else if (mtype == MTYP_MR)
   {
      xx += DX_offset[MTYP_F - 1][(uint8_t)fdir];
      xx += DX_offset[MTYP_R - 1][(uint8_t)fdir];
   }
   else if (mtype == MTYP_MM)
   {
      xx += 2 * DX_offset[MTYP_F - 1][(uint8_t)fdir];
   }
   else if (mtype != MTYP_INIT)
   {
      xx += (int8_t)distance * DX_offset[(uint8_t)mtype - 1][(uint8_t)fdir];
   }

   xx &= (LVL_X - 1);

   return xx;
}

int32_t step_Y(int32_t argcnt, uint32_t y, uint32_t fdir, uint32_t mtype, uint32_t distance)
{
   int8_t yy = (int8_t)y;
   (void)argcnt;

   printf("[eye] step_Y\n");

   if (!distance)
      return y;

   if (mtype == MTYP_ML)
   {
      yy += DY_offset[MTYP_F - 1][(uint8_t)fdir];
      yy += DY_offset[MTYP_L - 1][(uint8_t)fdir];
   }
   else if (mtype == MTYP_MR)
   {
      yy += DY_offset[MTYP_F - 1][(uint8_t)fdir];
      yy += DY_offset[MTYP_R - 1][(uint8_t)fdir];
   }
   else if (mtype == MTYP_MM)
   {
      yy += 2 * DY_offset[MTYP_F - 1][(uint8_t)fdir];
   }
   else if (mtype != MTYP_INIT)
   {
      yy += (int8_t)distance * DY_offset[(uint8_t)mtype - 1][(uint8_t)fdir];
   }

   yy &= (LVL_Y - 1);

   return yy;
}

uint32_t step_FDIR(int32_t argcnt, uint32_t fdir, uint32_t mtype)
{
   uint8_t f = (uint8_t)fdir;
   (void)argcnt;

   printf("[eye] step_FDIR\n");

   switch (mtype)
   {
   case MTYP_TL:
      return (f) ? (uint32_t)f - 1L : 3L;

   case MTYP_TR:
      return (f == 3) ? 0L : (uint32_t)f + 1L;
   }

   return (uint32_t)f;
}

/*********************************************************/
int32_t step_square_X(int32_t argcnt, uint32_t x, uint32_t r, uint32_t dir)
{
   (void)argcnt;

   printf("[eye] step_square_X\n");

   switch (dir)
   {
   case DIR_E:
      x = step_X(0, x, dir, MTYP_F, r & 0x01);
      break;
   case DIR_W:
      x = step_X(0, x, dir, MTYP_F, !(r & 0x01));
      break;
   }

   return x;
}

int32_t step_square_Y(int32_t argcnt, uint32_t y, uint32_t r, uint32_t dir)
{
   (void)argcnt;

   printf("[eye] step_square_Y\n");

   switch (dir)
   {
   case DIR_N:
      y = step_Y(0, y, dir, MTYP_F, r < 2);
      break;
   case DIR_S:
      y = step_Y(0, y, dir, MTYP_F, r >= 2);
      break;
   }

   return y;
}

int32_t step_region(int32_t argcnt, uint32_t r, uint32_t dir)
{
   (void)argcnt;

   printf("[eye] step_region\n");

   switch (dir)
   {
   case DIR_N:
   case DIR_S:
      r ^= 2;
      break;

   case DIR_E:
   case DIR_W:
      r ^= 1;
      break;
   }

   return r;
}

/*********************************************************/
uint32_t distance(int32_t argcnt, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
   uint32_t dx, dy, root;
   int32_t num;
   static int16_t square_root[32] =
       {
           0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144, 169, 196, 225, 256,
           289, 324, 361, 400, 441, 484, 529, 576, 625, 676, 729, 784, 841, 900, 961};
   (void)argcnt;

   printf("[eye] distance\n");

   dx = absv(0, x1 - x2);
   dy = absv(0, y1 - y2);

   num = (dx * dx) + (dy * dy);

   for (root = 0; root < 31; root++)
   {
      if (((int32_t)square_root[root]) >= num)
         break;
   }

   return root;
}

/*********************************************************/
//
// Return octal direction in which entity at cur_x,y should
// move in order to approach dest_x,y, or -1 if already there
//
// Direction is cardinal if bit 0 is clear, else diagonal
//
// Directions: N (NE) E (SE) S (SW) W (NW)
//             0  1   2  3   4  5   6  7
//
// Note that opposite direction may be obtained by XOR'ing with 4
//
/*********************************************************/

uint32_t seek_direction(int32_t argcnt, uint32_t cur_x, uint32_t cur_y, uint32_t dest_x, uint32_t dest_y)
{
   int32_t dx, dy;
   (void)argcnt;

   printf("[eye] seek_direction\n");

   dx = dest_x - cur_x;
   dy = dest_y - cur_y;

   if (dx < 0)
   { // move west (- in X)
      if (dy > 0)
         return 5; //  (southwest)
      else if (dy < 0)
         return 7; //  (northwest)
      else
         return 6; //  (due west)
   }
   else if (dx > 0)
   { // move east (+ in X)
      if (dy > 0)
         return 3; //  (southeast)
      else if (dy < 0)
         return 1; //  (northeast)
      else
         return 2; //  (due east)
   }

   if (dy > 0)
      return 4; // move south (+ in Y)
   else if (dy < 0)
      return 0; // move north (- in Y)

   return -1; // cur == dest, return -1
}

/*********************************************************/
//
// Fast status check for spell-request arrays
//
// Returns 1 if valid spells requested > spells known
//
// Assumes array = [2][10][10] = [ST] [LVL] [#], and that
// spell #s begin indexing the array at level 1 (not 0)
//
// This code resource is needed to speed up resting in camp, since the
// players' spell arrays must be searched once per 10-minute interval
//
/*********************************************************/

uint32_t spell_request(int32_t argcnt, int8_t *stat, int8_t *cnt, uint32_t typ, uint32_t num)
{
   uint32_t i, toff;
   int n, h; // LUM changed to int (original BYTE) lead to a warning in on condition below
   (void)argcnt;

   printf("[eye] spell_request\n");

   toff = typ ? 110 : 10;

   stat += toff;
   cnt += toff;

   for (i = 0; i < num; i++)
   {
      n = *stat++;
      h = *cnt++;

      if ((n != -1) && (h < n))
         return 1;
   }

   return 0;
}

/*********************************************************/
//
// Fast list creation for spell menu arrays
//
// Fills list with spells from cnt[typ 0-1][lvl 1-9][# 0-9]
//
// Returns # of spells placed in list (<= max)
//
/*********************************************************/

uint32_t spell_list(int32_t argcnt, int8_t *cnt, uint32_t typ, uint32_t lvl, int8_t *list, uint32_t max)
{
   uint32_t i, l, num;
   int m, n, j; // LUM changed to int (original BYTE) lead to a warning in on condition below
   (void)argcnt;

   printf("[eye] spell_list\n");

   l = (10 * (lvl - 1));
   num = 0;

   cnt = cnt + (typ ? 110 : 10) + l;

   for (i = 0; i < 10; i++)
   {
      m = i + l;
      n = *cnt++;

      if (n < 0)
         continue;

      for (j = 0; j < n; j++)
      {
         *list++ = m;

         if (++num == max)
            return num;
      }
   }

   return num;
}

/*=========================================================================*/
/*      MAGIC_FIELD:                                                                                                                                                            */
/*                                                                                                                                                                                                      */
/*              This routine draws a magic field around the desired character. */
/*              (Munged from Eye II)                                           */
/*                                                                                                                                                                                                      */
/*      INPUTS: pl, red, yel                                                   */
/*      RETURNS:        none                                                                                                                                                            */
/*=========================================================================*/

void magic_field(int32_t argcnt, uint32_t p, uint32_t redfield, uint32_t yelfield, int32_t sparkle)
{
   static uint8_t _x[] = {8, 80};
   static int8_t _y[] = {2, 54, 106};
   int16_t red, yel /*, color //Tom: commented out, not currently used */;
   int16_t x, y, lp, save;
   (void)argcnt;

   printf("[STUB] [eye] magic_field\n");

   red = 0x23;
   yel = 0x37;

   if (sparkle != -1L)
   {
      red += sparkle;
      yel += sparkle;
   }

   // color = red; // Tom: commented out, not currently used

   x = _x[p & 1];
   y = _y[p >> 1];

   x += 176;

   if ((redfield) && (!yelfield))
   {
      // GIL2VFX_draw_rect(PAGE2, x, y, x + 63, y + 49, color); // Tom: commented out
   }
   else if ((yelfield) && (!redfield))
   {
      // GIL2VFX_draw_rect(PAGE2, x, y, x + 63, y + 49, yel); // Tom: commented out
   }
   else
   {
      save = x;
      for (lp = 0; lp < 64; lp += 16)
      {
         x = save + lp;
         if (redfield)
         {
            // GIL2VFX_draw_line(PAGE2, x, y, x + 7, y, color); // Tom: commented out
            // GIL2VFX_draw_line(PAGE2, x + 8, y + 49, x + 15, y + 49, color); // Tom: commented out
         }

         if (yelfield)
         {
            // GIL2VFX_draw_line(PAGE2, x + 8, y, x + 15, y, yel); // Tom: commented out
            // GIL2VFX_draw_line(PAGE2, x, y + 49, x + 7, y + 49, yel); // Tom: commented out
         }
      }

      x = save;
      save = y;

      for (lp = 1; lp < 48; lp += 12)
      {
         y = save + lp - 1;
         if (yelfield)
         {
            // GIL2VFX_draw_line(PAGE2, x, y + 1, x, y + 6, yel); // Tom: commented out
            // GIL2VFX_draw_line(PAGE2, x + 63, y + 7, x + 63, y + 12, yel); // Tom: commented out
         }

         if (redfield)
         {
            // GIL2VFX_draw_line(PAGE2, x, y + 7, x, y + 12, color); // Tom: commented out
            // GIL2VFX_draw_line(PAGE2, x + 63, y + 1, x + 63, y + 6, color); // Tom: commented out
         }
      }
   }
}

/*=========================================================================*/
/*      COORD_IN_REGION:                                                                                                                                                        */
/*                                                                                                                                                                                                      */
/*              This routine tests to see if two coordinates passed to it are in the      */
/*      region passed to it.                                                                                                                                            */
/*                                                                                                                                                                                                      */
/*      INPUTS: int16_t testx,int16_t testy,int16_t left x,int16_t top y,int16_t right x, */
/*                              int16_t bottom y                                                                                                                                   */
/*      RETURNS:        none                                                                                                                                                            */
/*=========================================================================*/
int32_t Coord_In_Region(int32_t x, int32_t y, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
   printf("[eye] Coord_In_Region\n");

   if ((x < x1) || (x > x2))
      return (0); /* if it exceeds x bound then false     */
   if ((y < y1) || (y > y2))
      return (0); /* if it exceeds y bound then false     */
   return (1);    /* otherwise its true                   */
}

/*********************************************************/
//
// Explosion effect boosted from Eye II
//
/*********************************************************/

void do_dots(int32_t argcnt, int32_t view, int32_t scrn, int32_t exp_x, int32_t exp_y, int32_t scale, int32_t power, int32_t dots, int32_t life, int32_t upval, int8_t *colors)
{
   printf("[STUB] [eye] do_dots\n");

   static int16_t _floor[] =
       {
           119,
           103,
           79,
           63,
       };
   static int i, pixcol, active, cx, cy, px, py, mask;
   int16_t *xpos, *ypos, *xvel, *yvel, *color, *colcnt, *colidx, *dotbuffer;
   int16_t lside, rside, top, bottom;
   int16_t roof, floor, lwall, rwall;
   (void)argcnt;
   (void)view; // Tom: not currently used
   (void)scrn; // Tom: not currently used

   hide_mouse();

   floor = _floor[scale];
   if (scale)
      scale--;

   dotbuffer = (int16_t *)txtbuf;
   roof = 0;
   lwall = -100;
   rwall = 276;

   // Tom: added zeroed version
   top = 0;
   bottom = 0;
   lside = 0;
   rside = 0;

   // Tom: commented out, zeroed version above
   // top = GIL2VFX_get_y1(view);
   // bottom = GIL2VFX_get_y2(view);
   // lside = GIL2VFX_get_x1(view);
   // rside = GIL2VFX_get_x2(view);

   xpos = dotbuffer;
   ypos = dotbuffer + MAXDOTS;
   xvel = dotbuffer + (MAXDOTS * 2);
   yvel = dotbuffer + (MAXDOTS * 3);
   color = dotbuffer + (MAXDOTS * 4);
   colcnt = dotbuffer + (MAXDOTS * 5);
   colidx = dotbuffer + (MAXDOTS * 6);

   if (dots > MAXDOTS)
      dots = MAXDOTS;

   cx = exp_x;
   cy = exp_y;

   for (i = 0; i < dots; i++)
   {
      xpos[i] = ypos[i] = 0;
      xvel[i] = rnd(0, 0, power) - (power >> 1);
      yvel[i] = rnd(0, 0, power) - (power >> 1) - (power >> (8 - upval));
      colcnt[i] = rnd(0, (4 << 8) / life, (8 << 8) / life);
      colidx[i] = scale << 8;
   }

   active = 2;
   while (active)
   {
      if (active != 2)
         for (i = dots - 1; i >= 0; i--)
         {
            px = ((xpos[i] >> ACCUR) >> scale) + cx;
            py = ((ypos[i] >> ACCUR) >> scale) + cy;
            if (py > floor)
               py = floor;

            if (Coord_In_Region(px, py, lside, top, rside, bottom))
            {
               // GIL2VFX_draw_dot(scrn, px, py, color[i]); // Tom: commented out
            }
         }

      active = 0;

      for (i = 0; i < dots; i++)
      {
         if (xvel[i] > 0)
            xvel[i] -= FRICTION;
         else
            xvel[i] += FRICTION;

         xpos[i] += xvel[i];
         yvel[i] += GRAVITY;
         ypos[i] += yvel[i];

         colidx[i] += colcnt[i];

         px = ((xpos[i] >> ACCUR) >> scale) + cx;
         py = ((ypos[i] >> ACCUR) >> scale) + cy;

         if ((py >= floor) || (py < roof))
            yvel[i] = 0 - (yvel[i] >> 1);
         if ((px >= rwall) || (px < lwall))
            xvel[i] = 0 - (xvel[i] >> 1);
         if (py > floor)
            py = floor;

         // Tom: added zeroed version
         mask = 0;
         color[i] = 0;

         // Tom: commented out, zeroed version above
         // mask = GIL2VFX_read_dot(view, px, py);
         // color[i] = GIL2VFX_read_dot(scrn, px, py);

         pixcol = colors[colidx[i] >> 8];

         if (pixcol != XCOLOR)
         {
            active = 1;

            if ((mask == XCOLOR) && Coord_In_Region(px, py, lside, top, rside, bottom))
            {
               // GIL2VFX_draw_dot(scrn, px, py, pixcol); // Tom: commented out
            }
         }
         else
            colcnt[i] = 0;
      }

      // PollMod(); // Tom: commented out

      // Tom: TODO
      if (VFX_wait_vblank_leading)
      {
         VFX_wait_vblank_leading();
         VFX_wait_vblank_leading();
      }
   }

   show_mouse();
}

/*=========================================================================*/
/*      DO_ICE:                                                                                                                                                                         */
/*                                                                                                                                                                                                      */
/*              Ice special effect for the cone of cold spell.  It is way cool. */
/*              (at least according to its original Westwood implementer)       */
/*                                                                                                                                                                                                      */
/*      INPUTS: as below                                                                                                                                                        */
/*      RETURNS:        none                                                                                                                                                            */
/*                                                                                                                                                                                                      */
/*=========================================================================*/

void do_ice(int32_t argcnt, int32_t view, int32_t scrn, int32_t dots, int32_t mag, int32_t grav, int32_t life, int8_t *colors)
{
   int16_t i, pixcol, active, cx, cy, px, py, mask, count;
   int16_t *xpos, *ypos, *xvel, *yvel, *color, *colcnt, *colidx, *delay, *dotbuffer;
   int16_t m, v, grav78, t;
   (void)argcnt;
   (void)px;   // Tom: not currently used
   (void)py;   // Tom: not currently used
   (void)view; // Tom: not currently used
   (void)scrn; // Tom: not currently used

   printf("[STUB] [eye] do_ice\n");

   hide_mouse();

   dotbuffer = (int16_t *)txtbuf;

   xpos = dotbuffer;
   ypos = dotbuffer + MAXDOTS;
   xvel = dotbuffer + (MAXDOTS * 2);
   yvel = dotbuffer + (MAXDOTS * 3);
   color = dotbuffer + (MAXDOTS * 4);
   colcnt = dotbuffer + (MAXDOTS * 5);
   colidx = dotbuffer + (MAXDOTS * 6);
   delay = dotbuffer + (MAXDOTS * 7);

   mag <<= ACCUR;

   if (dots > MAXDOTS)
      dots = MAXDOTS;

   cx = 88;
   cy = 48;

   for (i = 0; i < dots; i++)
   {
      m = rnd(0, mag >> 2, mag);
      v = t = 0;

      while (t < m)
      {
         v += grav;
         t += v;
      }

      switch (rand() & 3)
      {
      case 0:
         xpos[i] = 1 << (ACCUR - 1);
         ypos[i] = t;
         xvel[i] = v;
         yvel[i] = 0;
         break;

      case 1:
         xpos[i] = t;
         ypos[i] = 1 << (ACCUR - 1);
         xvel[i] = 0;
         yvel[i] = v;
         break;

      case 2:
         xpos[i] = 1 << (ACCUR - 1);
         ypos[i] = -t;
         xvel[i] = v;
         yvel[i] = 0;
         break;

      case 3:
         xpos[i] = -t;
         ypos[i] = 1 << (ACCUR - 1);
         xvel[i] = 0;
         yvel[i] = v;
         break;
      }

      if (rand() & 1)
      {
         xvel[i] *= -1;
         yvel[i] *= -1;
      }

      colcnt[i] = rnd(0, (4 << 8) / life, (8 << 8) / life);
      colidx[i] = 0;
      delay[i] = rnd(0, 0, life >> 2);
   }

   active = 2;
   count = 0;

   while (active)
   {
      if (active != 2)
         for (i = dots - 1; i >= 0; i--)
         {
            px = (xpos[i] >> ACCUR) + cx;
            py = (ypos[i] >> ACCUR) + cy;
            // GIL2VFX_draw_dot(scrn, px, py, color[i]); // Tom: commented out
         }

      active = 0;
      grav78 = (grav >> 1) + (grav >> 2) + (grav >> 3);

      for (i = 0; i < dots; i++)
      {
         if (delay[i])
            delay[i]--;
         else
         {
            if (xpos[i] > 0)
               if (xvel[i] > 0)
                  xvel[i] -= grav;
               else
                  xvel[i] -= grav78;
            else if (xvel[i] < 0)
               xvel[i] += grav;
            else
               xvel[i] += grav78;
            if (ypos[i] > 0)
               if (yvel[i] > 0)
                  yvel[i] -= grav;
               else
                  yvel[i] -= grav78;
            else if (yvel[i] < 0)
               yvel[i] += grav;
            else
               yvel[i] += grav78;

            xpos[i] += xvel[i];
            ypos[i] += yvel[i];
            colidx[i] += colcnt[i];
         }

         px = (xpos[i] >> ACCUR) + cx;
         py = (ypos[i] >> ACCUR) + cy;

         if (count < (life >> 2))
            mask = 0;
         else
            mask = 0; // Tom: added zeroed version
         // mask = GIL2VFX_read_dot(view, px, py); // Tom: commented out

         color[i] = 0; // Tom: added zeroed version
         // color[i] = GIL2VFX_read_dot(scrn, px, py); // Tom: commented out
         pixcol = colors[colidx[i] >> 8];

         if (pixcol)
         {
            active = 1;

            if (mask == XCOLOR && !delay[i])
            {
               // GIL2VFX_draw_dot(scrn, px, py, pixcol); // Tom: commented out
            }
         }
         else
            colcnt[i] = 0;
      }

      // PollMod(); // Tom: commented out

      if (VFX_wait_vblank_leading) // Tom: TODO
         VFX_wait_vblank_leading();

      count++;
   }

   show_mouse();
}

/*********************************************************/
//
// Read savegame directory file into string array
//
/*********************************************************/

void read_save_directory(void)
{
   int32_t i;
   TF_class *TF;

   printf("[eye] read_save_directory\n");

   TF = TF_construct((int8_t *)SAVEDIR_FN, TF_READ);

   if (TF == NULL)
      abend(MSG_COSDR);

   for (i = 0; i < NUM_SAVEGAMES; i++)
      TF_readln(TF, savegame_dir[i], SAVE_LEN + 1);

   TF_destroy(TF);
}

/*********************************************************/
//
// Return the address ('string' override type) of string in
// savegame directory
//
/*********************************************************/

int8_t *savegame_title(int32_t argcnt, uint32_t num)
{
   (void)argcnt;

   printf("[eye] savegame_title\n");

   return savegame_dir[num];
}

/*********************************************************/
//
// Copy the AESOP string 'string' to savegame directory entry
// savegame directory
//
/*********************************************************/

void set_savegame_title(int32_t argcnt, int8_t *string, uint32_t num)
{
   (void)argcnt;

   printf("[eye] set_savegame_title\n");

   strcpy((char *)savegame_dir[num], (char *)string);
}

/*********************************************************/
//
// Write string array as savegame directory file
//
/*********************************************************/

void write_save_directory(void)
{
   int32_t i;
   TF_class *TF;

   printf("[eye] write_save_directory\n");

   TF = TF_construct((int8_t *)SAVEDIR_FN, TF_WRITE);

   if (TF == NULL)
      abend(MSG_COSDW);

   for (i = 0; i < NUM_SAVEGAMES; i++)
      if (!TF_writeln(TF, savegame_dir[i]))
         abend(MSG_CWSD);

   TF_destroy(TF);
}

/*********************************************************/
//
// Update savegame filename strings with slot #
//
/*********************************************************/

void set_save_slotnum(uint32_t slot)
{
   int8_t num[3];

   printf("[eye] set_save_slotnum\n");

   sprintf((char *)num, "%02u", slot);

   strncpy((char *)&items_bin[15], (char *)num, 2);
   strncpy((char *)&items_txt[15], (char *)num, 2);
   strncpy((char *)&lvl_bin[15], (char *)num, 2);
   strncpy((char *)&lvl_txt[15], (char *)num, 2);
}

/*********************************************************/
//
// Update savegame filename strings with level #
//
// Use directory specified in TEMP environment variable for
// temporary file storage; use current working directory if
// TEMP undefined
//
/*********************************************************/

void set_save_lvlnum(uint32_t lvl)
{
   int8_t num[3];

   printf("[eye] set_save_lvlnum\n");

   sprintf((char *)num, "%02u", lvl);

   strncpy((char *)&lvl_bin[12], (char *)num, 2);
   strncpy((char *)&lvl_tmp[12], (char *)num, 2);
   strncpy((char *)&lvl_txt[12], (char *)num, 2);
}

/*********************************************************/
//
// Remove all temporary savegame files
//
// Called before restoring or leaving game
//
/*********************************************************/

void remove_temporary_save_files(void)
{
   int32_t lvl;

   printf("[eye] remove_temporary_save_files\n");

   for (lvl = 1; lvl <= NUM_LEVELS; lvl++)
   {
      set_save_lvlnum(lvl);
      delete_file(lvl_tmp);
   }
}

/*********************************************************/
//
// Save game at slotnum on level lvlnum
//
// Return 0 if save failed due to lack of disk space or other
// system error
//
/*********************************************************/

uint32_t save_game(int32_t argcnt, uint32_t slotnum, uint32_t lvlnum)
{
   uint32_t lvl;
   (void)argcnt;

   printf("[eye] save_game\n");

   if (slotnum == 0)
      abend(MSG_IASS0);

   set_save_slotnum(slotnum);
   set_save_lvlnum(lvlnum);

   if (!save_range(items_bin, SAVETYPE, FIRST_ITEM, LAST_ITEM))
      return 0;

   if (!save_range(lvl_bin, SAVETYPE, FIRST_LVL_OBJ, LAST_LVL_OBJ))
      return 0;

   for (lvl = 1; lvl <= NUM_LEVELS; lvl++)
   {
      if (lvl == lvlnum)
         continue;

      set_save_lvlnum(lvl);

      if (copy_file(lvl_tmp, lvl_bin) == -1)
         return 0;
   }

   return 1;
}

/*********************************************************/
//
// Save current level context to temporary file
//
// This function assumes that the non-current-level tempfiles
// are already present and valid
//
// Used before spawning a process via the launch() function handler
//
/*********************************************************/

void suspend_game(int32_t argcnt, uint32_t cur_lvl)
{
   (void)argcnt;

   printf("[eye] suspend_game\n");

   if (!save_range(itm_tmp, SAVETYPE, FIRST_ITEM, LAST_ITEM))
      abend(MSG_CNSI);

   set_save_lvlnum(cur_lvl);

   if (!save_range(lvl_tmp, SAVETYPE, FIRST_LVL_OBJ, LAST_LVL_OBJ))
      abend(MSG_CNSCL);
}

/*********************************************************/
//
// Resume current level context from temporary file
//
// This function assumes that the non-current-level tempfiles
// are already present and valid
//
// Used when resuming game after launched process terminates
//
/*********************************************************/

void resume_level(int32_t argcnt, uint32_t cur_lvl)
{
   (void)argcnt;

   printf("[eye] resume_level\n");

   set_save_lvlnum(cur_lvl);

   restore_range(lvl_tmp, FIRST_LVL_OBJ, LAST_LVL_OBJ, 1);
}

/*********************************************************/
//
// Resume global items (including kernel, PCs, etc.)
//
// Release any windows owned by entities
// Cancel notification requests issued by all entities
// Restore items from temporary context file
//
// This function must be called before restoring any level-specific
// objects
//
// Failure results in unrecoverable system error; function will not
// return unless successful
//
/*********************************************************/

void resume_items(int32_t argcnt, uint32_t first, uint32_t last, uint32_t restoring)
{
   (void)argcnt;

   printf("[eye] resume_items\n");

   release_owned_windows(-1);
   cancel_entity_requests(-1);

   restore_range(itm_tmp, first, last, restoring);
}

/*********************************************************/
//
// Leave level old_lvl; enter level new_lvl
//
// Save level objects to temporary file; restore level objects
// from previously saved temporary file
//
// Failure results in unrecoverable system error; function will not
// return unless successful
//
/*********************************************************/

void change_level(int32_t argcnt, uint32_t old_lvl, uint32_t new_lvl)
{
   (void)argcnt;

   printf("[eye] change_level\n");

   set_save_lvlnum(old_lvl);

   if (!save_range(lvl_tmp, SAVETYPE, FIRST_LVL_OBJ, LAST_LVL_OBJ))
      abend(MSG_CNSLT);

   set_save_lvlnum(new_lvl);

   restore_range(lvl_tmp, FIRST_LVL_OBJ, LAST_LVL_OBJ, 1);
}

/*********************************************************/
//
// Restore global items (including kernel, PCs, etc.)
//
// Release any windows owned by entities
// Cancel notification requests issued by all entities
// Restore items from context file
//
// This function must be called before restoring any level-specific
// objects
//
// Failure results in unrecoverable system error; function will not
// return unless successful
//
/*********************************************************/

void restore_items(int32_t argcnt, uint32_t slotnum)
{
   (void)argcnt;

   printf("[eye] restore_items\n");

   set_save_slotnum(slotnum);

   release_owned_windows(-1);
   cancel_entity_requests(-1);

   //   aprint(0,"restore_range(%s, %d, %d, 1)\n",items_bin,FIRST_ITEM,LAST_ITEM);
   restore_range(items_bin, FIRST_ITEM, LAST_ITEM, 1);
}

/*********************************************************/
//
// Restore level-specific objects (features & NPCs)
//
// Restore current level objects from context file
// Copy non-current level context files to temporary files
//
// Failure results in unrecoverable system error; function will not
// return unless successful
//
/*********************************************************/

void restore_level_objects(int32_t argcnt, uint32_t slotnum, uint32_t lvlnum)
{
   uint32_t lvl;
   (void)argcnt;

   printf("[eye] restore_level_objects\n");

   set_save_slotnum(slotnum);
   set_save_lvlnum(lvlnum);

   restore_range(lvl_bin, FIRST_LVL_OBJ, LAST_LVL_OBJ, 1);

   for (lvl = 1; lvl <= NUM_LEVELS; lvl++)
   {
      if (lvl == lvlnum)
         continue;

      set_save_lvlnum(lvl);

      if (copy_file(lvl_bin, lvl_tmp) == -1)
         abend(MSG_CNCLT);
   }
}

/*********************************************************/
//
// Read initial (slot 0) items (including kernel, PCs, etc.) into
// object list
//
// Used during transfer/create process
//
// Failure results in unrecoverable system error; function will not
// return unless successful
//
/*********************************************************/

void read_initial_items(void)
{
   printf("[eye] read_initial_items\n");

   set_save_slotnum(0);

   restore_range(items_bin, FIRST_ITEM, LAST_ITEM, 0);
}

/*********************************************************/
//
// Write initial temporary files based on slot 0 binaries
//
// Used during transfer/create process
//
// Failure results in unrecoverable system error; function will not
// return unless successful
//
/*********************************************************/

void write_initial_tempfiles(void)
{
   uint32_t lvl;

   printf("[eye] write_initial_tempfiles\n");

   if (!save_range(itm_tmp, SAVETYPE /* SF_TXT */, FIRST_ITEM, LAST_ITEM))
      abend(MSG_CNSI);

   set_save_slotnum(0);

   for (lvl = 1; lvl <= NUM_LEVELS; lvl++)
   {
      set_save_lvlnum(lvl);

      if (copy_file(lvl_bin, lvl_tmp) == -1)
         abend(MSG_CNCLT);
   }
}

/*********************************************************/
//
// Translate level-specific objects from text to binary format
//
/*********************************************************/

void create_initial_binary_files(void)
{
   uint32_t lvl;

   printf("[eye] create_initial_binary_files\n");

   set_save_slotnum(0);

   //   if (copy_file(items_txt,items_bin) == -1)
   //      abend(MSG_CNTI);

   if (file_time(items_txt) >= file_time(items_bin))
   {
      printf("Translating %s to %s\n", items_txt, items_bin);
      translate_file(items_txt, items_bin, FIRST_ITEM, LAST_ITEM);
   }

   for (lvl = 1; lvl <= NUM_LEVELS; lvl++)
   {
      set_save_lvlnum(lvl);

      if (file_time(lvl_txt) < file_time(lvl_bin))
         continue;

      printf("Translating %s to %s\n", lvl_txt, lvl_bin);

      translate_file(lvl_txt, lvl_bin, FIRST_LVL_OBJ, LAST_LVL_OBJ);
   }
}

void mono_on(void)
{
   printf("[STUB] [eye] mono_on\n");
}

void mono_off(void)
{
   printf("[STUB] [eye] mono_off\n");
}

void *open_transfer_file(int32_t argcnt, int8_t *filename)
{
   (void)argcnt;
   (void)filename;

   printf("[STUB] [eye] open_transfer_file\n");

   return NULL;
}

void close_transfer_file(void)
{
   printf("[STUB] [eye] close_transfer_file\n");
}

int32_t player_attrib(int32_t argcnt, uint32_t plrnum, uint32_t offset, uint32_t size)
{
   (void)argcnt;
   (void)plrnum;
   (void)offset;
   (void)size;

   printf("[STUB] [eye] player_attrib\n");

   return 0;
}

int32_t item_attrib(int32_t argcnt, uint32_t plrnum, uint32_t invslot, uint32_t attrib)
{
   (void)argcnt;
   (void)plrnum;
   (void)invslot;
   (void)attrib;

   printf("[STUB] [eye] item_attrib\n");

   return 0;
}

int32_t arrow_count(int32_t argcnt, uint32_t plrnum)
{
   (void)argcnt;
   (void)plrnum;

   printf("[STUB] [eye] arrow_count\n");

   return 0;
}

/*********************************************************/
//
// Launch a secondary process
//
// Application is responsible for preserving its state and arranging
// for its resurrection
//
// Failure results in unrecoverable system error; function will not
// return under any circumstances
//
/*********************************************************/

void launch(int32_t argcnt, int8_t *dirname, int8_t *prgname, int8_t *argn1, int8_t *argn2)
{
   typedef struct
   {
      char prg[128];
      char arg1[128];
      char arg2[128];
   } stag;

   stag *s;
   int8_t dir[128];

   printf("[STUB] [eye] launch: argcnt=%i dirname=%s prgname=%s argn1=%s argn2=%s\n", argcnt, dirname, prgname, argn1, argn2);

   s = *(stag **)0x4fa;

   strcpy((char *)dir, (char *)dirname);
   strcpy(s->prg, (char *)prgname);

   if (argn1 != NULL)
      strcpy(s->arg1, (char *)argn1);
   else
      s->arg1[0] = 0;

   if (argn1 != NULL)
      strcpy(s->arg2, (char *)argn2);
   else
      s->arg2[0] = 0;

   RT_execute(bootstrap, MSG_DESTROY, UINT32_MAX);

   shutdown_sound();
   RTR_destroy(RTR, RTR_FREEBASE);

   locate(0, 51);
   // chdir(dir); // Tom: commented out

   exit(127);
}
