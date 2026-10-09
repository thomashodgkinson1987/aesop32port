#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>

// Tom: added (SDL)
#include <SDL2/SDL.h>
#include "globals.h"

#include "vfx.h"
#include "gil2vfx.h"
#include "graphics.h"
#include "video.h"

#include "defs.h"
#include "rtmsg.h"
#include "rtsystem.h"
#include "mouse.h"
#include "rtres.h"

#define MAX_WINDOWS 50
#define MAX_PANES 250

#define FADE_INTERVALS 20

WINDOW windows[MAX_WINDOWS];
PANE panes[MAX_WINDOWS + MAX_PANES];
int8_t *bitmap_buffer;

int32_t gil2vfx_active = 0;

TEXTWINDOW *twptr;

// Capped buffer read for the print-buffer port: NUL past the
// formatted end, so scans can always probe one past the last byte.
static int32_t print_bufch(const char *buf, int32_t buflen, int32_t i)
{
   if (i < 0 || i > buflen)
      return 0;

   return (int32_t)(uint8_t)buf[i];
}

// J_FILL handler (GIL2VFXA.ASM out_f): leading spaces and the first
// word print normally, then the leftover gap distributes across the
// line's word gaps, quotient each plus one extra while the remainder
// lasts. Lines that overflow, are empty, or have no word gaps fall
// back to plain left dump.
static void print_buffer_fill(const char *buf, int32_t buflen, int32_t line_beg, int32_t line_end, int32_t x2, int32_t w_lin, int32_t w_win)
{
   int32_t si = line_beg;
   int32_t nwords = 0;
   int32_t remaining;
   int32_t quotient = 0;
   int32_t remainder = 0;

   if (w_lin < 0)
      w_lin = 0;

   // Overflowing lines fall back to a plain dump, like the original.
   if ((uint32_t)w_lin >= (uint32_t)w_win)
   {
      for (; si < line_end; si++)
      {
         int32_t ch = print_bufch(buf, buflen, si);

         if (ch == 0)
            break;

         GIL2VFX_cout(ch);
      }

      return;
   }

   // Last character NUL/LF (or empty line): nothing to distribute;
   // dump plainly. (line_beg/line_end bound every read via print_bufch.)
   if (line_end <= line_beg || print_bufch(buf, buflen, line_end - 1) == 0 ||
       print_bufch(buf, buflen, line_end - 1) == 10)
   {
      for (; si < line_end; si++)
      {
         int32_t ch = print_bufch(buf, buflen, si);

         if (ch == 0)
            break;

         GIL2VFX_cout(ch);
      }

      return;
   }

   // Leading spaces print normally, shrinking the remainder. The
   // cursor advances with them, so any fallback below resumes after
   // what is already on screen (as in the original).
   while (si < line_end && print_bufch(buf, buflen, si) == ' ')
   {
      GIL2VFX_cout(' ');
      w_lin -= VFX_character_width(twptr->font, ' ');
      si++;
   }

   // First word prints normally.
   while (si < line_end && print_bufch(buf, buflen, si) != ' ')
   {
      int32_t ch = print_bufch(buf, buflen, si);

      if (ch == 0)
         break;

      GIL2VFX_cout(ch);
      w_lin -= VFX_character_width(twptr->font, ch);
      si++;
   }

   if (w_lin < 0)
      w_lin = 0;

   remaining = x2 - twptr->htab + 1 - w_lin;

   // Word gaps in what remains to be distributed (leading spaces and
   // the first word are already on screen).
   {
      int32_t wi;

      for (wi = si; wi < line_end; wi++)
      {
         if (print_bufch(buf, buflen, wi) == ' ')
            nwords++;
      }
   }

   if (nwords == 0 || nwords > 255 || remaining < 0)
   {
      // Fall back to a plain dump of what is not already on screen
      // (the original resumes from its advanced line_beg the same way).
      for (; si < line_end; si++)
      {
         int32_t ch = print_bufch(buf, buflen, si);

         if (ch == 0)
            break;

         GIL2VFX_cout(ch);
      }

      return;
   }

   quotient = remaining / nwords;
   remainder = remaining % nwords;

   for (; si < line_end; si++)
   {
      int32_t ch = print_bufch(buf, buflen, si);

      if (ch == 0)
         break;

      GIL2VFX_cout(ch);

      if (ch == ' ')
      {
         twptr->htab += quotient;

         if (remainder > 0)
         {
            twptr->htab++;
            remainder--;
         }
      }
   }
}

// End-of-page / pending-line state for GIL2VFX_print_buffer, mirroring
// GIL2VFXA.ASM's eop_reached/pending_flag (set by cout at page end;
// print_buffer honours them when linenum != 0). No current caller
// passes a nonzero linenum, so these stay 0 for now.
int32_t eop_reached = 0;
int32_t pending_flag = 0;

extern VFX_DESC *VFX;
extern int8_t *pathname;

void GIL2VFX_copy_window(uint32_t src, uint32_t dst)
{
   printf("[gil2vfx] GIL2VFX_copy_window: src=%u dst=%u\n", src, dst);

   if (gil2vfx_active != -1)
      return;

   VFX_pane_copy(&panes[src], 0, 0, &panes[dst], 0, 0, NO_COLOR);
}

int32_t GIL2VFX_assign_window(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
   int32_t i;

   printf("[gil2vfx] GIL2VFX_assign_window: x1=%i y1=%i x2=%i y2=%i\n", x1, y1, x2, y2);

   for (i = 0; i < MAX_WINDOWS; i++)
   {
      if (windows[i].buffer == NULL)
      {
         windows[i].buffer = mem_alloc((x2 - x1 + 1) * (y2 - y1 + 1));
         // LUM WINDOW structure changes
         // windows[i].x0 = x1;
         // windows[i].y0 = y1;
         // windows[i].x1 = x2;
         // windows[i].y1 = y2;
         windows[i].x_max = x2;
         windows[i].y_max = y2;

         panes[i].window = &windows[i];
         panes[i].x0 = x1;
         panes[i].y0 = y1;
         panes[i].x1 = x2;
         panes[i].y1 = y2;

         return i;
      }
   }

   abend(MSG_TFW);
   return -1;
}

void GIL2VFX_init()
{
   int32_t dummy;

   printf("[gil2vfx] GIL2VFX_init\n");

   // Describe the video mode for VFX clients (the original asked the
   // driver via VFX_describe_driver). Without this, any VFX->
   // dereference faults on the NULL descriptor — e.g.
   // GIL2VFX_color_fade's full-screen refresh, which crashes the
   // Continue path out of the menu.
   {
      static VFX_DESC vfx_desc;

      vfx_desc.scrn_width = SCREEN_WIDTH;
      vfx_desc.scrn_height = SCREEN_HEIGHT;
      vfx_desc.bytes_per_pixel = 1;
      vfx_desc.ncolors = 256;
      vfx_desc.npages = 1;
      vfx_desc.flags = 0;
      VFX = &vfx_desc;
   }

   // VFX = VFX_describe_driver();

   //
   // Turn graphics on; set up window and pane
   //

   gil2vfx_active = -1;

   bitmap_buffer = mem_alloc(320 * 200);

   // The screen window's buffer is the video layer's "video RAM"
   // (the original pointed it at the VGA segment, 0x0A0000)
   windows[0].buffer = video_screen_buffer;
   // LUM WINDOW structure changes
   // windows[0].x0 = 0;
   // windows[0].y0 = 0;
   // windows[0].x1 = 319;
   // windows[0].y1 = 199;
   windows[0].x_max = 319;
   windows[0].y_max = 199;

   panes[0].window = &windows[0];
   panes[0].x0 = 0;
   panes[0].y0 = 0;
   panes[0].x1 = 319;
   panes[0].y1 = 199;

   dummy = GIL2VFX_assign_window(0, 0, 319, 199); // Page 2 = 1
}

void GIL2VFX_shutdown_driver()
{
   uint32_t i;

   printf("[gil2vfx] GIL2VFX_shutdown_driver\n");

   if (gil2vfx_active != -1)
      return;

   gil2vfx_active = 0;

   for (i = 0; i < MAX_WINDOWS; i++)
   {
      GIL2VFX_release_window(i);
   }

   mem_free(bitmap_buffer);

   if (VFX_shutdown_driver) // Tom: TODO
      VFX_shutdown_driver();
}

void GIL2VFX_release_window(uint32_t wnd)
{
   printf("[gil2vfx] GIL2VFX_release_window: wnd=%u\n", wnd);

   if (wnd < MAX_WINDOWS)
   {
      if (windows[wnd].buffer != NULL)
      {
         //
         // Window 0 is the screen: its buffer belongs to the video
         // layer (video.c), not the AESOP heap. The original mem_free'd
         // 0x0A0000 here, which DOS's unprotected allocator silently
         // tolerated; glibc aborts on the later double free.
         //

         if (wnd != 0)
            mem_free(windows[wnd].buffer);

         windows[wnd].buffer = NULL;
      }
   }
   GIL2VFX_release_subwindow(wnd);
}

int32_t GIL2VFX_assign_subwindow(uint32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
   uint32_t i;

   printf("[gil2vfx] GIL2VFX_assign_subwindow: wnd=%u x1=%i y1=%i x2=%i y2=%i\n", wnd, x1, y1, x2, y2);

   for (i = MAX_WINDOWS; i < MAX_WINDOWS + MAX_PANES; i++)
   {
      if (panes[i].window == NULL)
      {
         if (wnd < MAX_WINDOWS)
            panes[i].window = &windows[wnd];
         else
            panes[i].window = panes[wnd].window;

         panes[i].x0 = x1;
         panes[i].y0 = y1;
         panes[i].x1 = x2;
         panes[i].y1 = y2;

         return i;
      }
   }

   abend(MSG_TFSW);

   return -1;
}

void GIL2VFX_release_subwindow(uint32_t wnd)
{
   printf("[gil2vfx] GIL2VFX_release_subwindow: wnd=%u\n", wnd);

   panes[wnd].window = NULL;
}

int32_t GIL2VFX_get_bitmap_width(void *shape_table, int32_t shape_num)
{
   printf("[gil2vfx] GIL2VFX_get_bitmap_width: shape_table=%p shape_num=%i\n", shape_table, shape_num);

   return (VFX_shape_bounds(shape_table, shape_num) >> 16);
}

int32_t GIL2VFX_get_bitmap_height(void *shape_table, int32_t shape_num)
{
   printf("[gil2vfx] GIL2VFX_get_bitmap_height: shape_table=%p shape_num=%i\n", shape_table, shape_num);

   return ((int32_t)(int16_t)VFX_shape_bounds(shape_table, shape_num));
}

int32_t GIL2VFX_visible_bitmap_rect(int32_t x1, int32_t y1, int32_t mirror, uint8_t *shapes, int32_t shape_num, int16_t *bounds)
{
   int32_t rectangle[4];
   int32_t bm_width, bm_height;

   printf("[gil2vfx] GIL2VFX_visible_bitmap_rect: x1=%i y1=%i mirror=%i shapes=%p shape_num=%i bounds=%p\n", x1, y1, mirror, (void *)shapes, shape_num, (void *)bounds);

   VFX_shape_visible_rectangle(shapes, shape_num, x1, y1, mirror, rectangle);

   if (mirror)
   {
      bm_width = VFX_shape_bounds(shapes, shape_num);
      bm_height = (int32_t)(int16_t)bm_width;
      bm_width = bm_width >> 16;
   }

   if (mirror & X_MIRROR)
   {
      bounds[0] = (int16_t)(bm_width + rectangle[0]);
      bounds[2] = (int16_t)(bm_width + rectangle[2]);
   }
   else
   {
      bounds[0] = (int16_t)rectangle[0];
      bounds[2] = (int16_t)rectangle[2];
   }

   if (mirror & Y_MIRROR)
   {
      bounds[1] = (int16_t)(bm_height + rectangle[1]);
      bounds[3] = (int16_t)(bm_height + rectangle[3]);
   }
   else
   {
      bounds[1] = (int16_t)rectangle[1];
      bounds[3] = (int16_t)rectangle[3];
   }

   bounds[0] = (bounds[0] > panes[0].x0) ? bounds[0] : (int16_t)panes[0].x0;
   bounds[1] = (bounds[1] > panes[0].y0) ? bounds[1] : (int16_t)panes[0].y0;

   bounds[2] = (bounds[2] < panes[0].x1) ? bounds[2] : (int16_t)panes[0].x1;
   bounds[3] = (bounds[3] < panes[0].y1) ? bounds[3] : (int16_t)panes[0].y1;

   if ((bounds[0] > bounds[2]) || (bounds[1] > bounds[3]))
      return 0; // Nothing visible
   else
      return 1;
}

void GIL2VFX_draw_bitmap(
    int32_t wnd,
    int32_t x,
    int32_t y,
    int32_t mirror,
    int32_t scale,
    uint8_t *fade_table,
    uint8_t *shapes,
    int32_t shape_num)
{
   int32_t xp = x - panes[wnd].x0;
   int32_t yp = y - panes[wnd].y0;
   int32_t x_scale, y_scale;
   int32_t flags;
   int32_t xs, ys;

   printf("[gil2vfx] GIL2VFX_draw_bitmap: "
          "wnd=%i "
          "x=%i "
          "y=%i "
          "mirror=%i "
          "scale=%i "
          "fade_table=%p "
          "shapes=%p "
          "shape_num=%i"
          "\n",
          wnd, x, y, mirror, scale, (void *)fade_table, (void *)shapes, shape_num);

   if (gil2vfx_active != -1)
      return;

   if ((scale == 0) && (mirror == NO_MIRROR))
   {
      VFX_shape_draw(&panes[wnd], shapes, shape_num, xp, yp);
   }
   else
   {
      x_scale = ((scale) ? (scale << 8) : 0x10000);
      y_scale = ((scale) ? (scale << 8) : 0x10000);

      if (x_scale != 0x10000 || y_scale != 0x10000)
      {
         VFX_fixed_mul(VFX_shape_bounds(shapes, shape_num) & 0xffff0000, 0x10000 - x_scale, &xs);
         VFX_fixed_mul(VFX_shape_bounds(shapes, shape_num) << 16, 0x10000 - y_scale, &ys);

         if (mirror & X_MIRROR)
            xs = -xs;
         if (mirror & Y_MIRROR)
            ys = -ys;

         xp += xs >> 17;
         yp += ys >> 17;
      }

      switch (mirror)
      {
      case X_MIRROR:
         x_scale = -x_scale;
         xp += (VFX_shape_bounds(shapes, shape_num) >> 16);
         break;
      case Y_MIRROR:
         y_scale = -y_scale;
         yp += ((int32_t)(int16_t)VFX_shape_bounds(shapes, shape_num));
         break;
      case XY_MIRROR:
         x_scale = -x_scale;
         y_scale = -y_scale;
         xp += (VFX_shape_bounds(shapes, shape_num) >> 16) - 1;
         yp += ((int32_t)(int16_t)VFX_shape_bounds(shapes, shape_num)) - 1;
         break;
      case NO_MIRROR:
      default:
         break;
      }

      if (fade_table != NULL && scale != 0)
      {
         VFX_shape_lookaside(fade_table);
         flags = ST_XLAT;
      }
      else
      {
         // flags = NULL; // Tom: commented out
         flags = 0; // Tom: new version
      }

      VFX_shape_transform(&panes[wnd], shapes, shape_num, xp, yp, bitmap_buffer, 0, x_scale, y_scale, flags);
   }
}

int32_t GIL2VFX_get_x1(uint32_t wnd)
{
   printf("[gil2vfx] GIL2VFX_get_x1: wnd=%u\n", wnd);

   if (panes[wnd].window != NULL)
      return panes[wnd].x0;
   else
   {
      // return NULL; // Tom: commented out
      return 0; // Tom: new version
   }
}
int32_t GIL2VFX_get_y1(uint32_t wnd)
{
   printf("[gil2vfx] GIL2VFX_get_y1: wnd=%u\n", wnd);

   if (panes[wnd].window != NULL)
      return panes[wnd].y0;
   {
      // return NULL; // Tom: commented out
      return 0; // Tom: new version
   }
}
int32_t GIL2VFX_get_x2(uint32_t wnd)
{
   printf("[gil2vfx] GIL2VFX_get_x2: wnd=%u\n", wnd);

   if (panes[wnd].window != NULL)
      return panes[wnd].x1;
   {
      // return NULL; // Tom: commented out
      return 0; // Tom: new version
   }
}
int32_t GIL2VFX_get_y2(uint32_t wnd)
{
   printf("[gil2vfx] GIL2VFX_get_y2: wnd=%u\n", wnd);

   if (panes[wnd].window != NULL)
      return panes[wnd].y1;
   {
      // return NULL; // Tom: commented out
      return 0; // Tom: new version
   }
}

void GIL2VFX_set_x1(uint32_t wnd, int32_t val)
{
   printf("[gil2vfx] GIL2VFX_set_x1: wnd=%u val=%i\n", wnd, val);

   panes[wnd].x0 = val;
}
void GIL2VFX_set_y1(uint32_t wnd, int32_t val)
{
   printf("[gil2vfx] GIL2VFX_set_y1: wnd=%u val=%i\n", wnd, val);

   panes[wnd].y0 = val;
}
void GIL2VFX_set_x2(uint32_t wnd, int32_t val)
{
   printf("[gil2vfx] GIL2VFX_set_x2: wnd=%u val=%i\n", wnd, val);

   panes[wnd].x1 = val;
}
void GIL2VFX_set_y2(uint32_t wnd, int32_t val)
{
   printf("[gil2vfx] GIL2VFX_set_y2: wnd=%u val=%i\n", wnd, val);

   panes[wnd].y1 = val;
}

void GIL2VFX_wipe_window(int32_t wnd, int32_t color)
{
   printf("[gil2vfx] GIL2VFX_wipe_window: wnd=%i color=%i\n", wnd, color);

   if (gil2vfx_active != -1)
      return;

   VFX_pane_wipe(&panes[wnd], color);
}

void GIL2VFX_draw_dot(int32_t wnd, int32_t x, int32_t y, int32_t color)
{
   int32_t xp = x - panes[wnd].x0;
   int32_t yp = y - panes[wnd].y0;

   printf("[gil2vfx] GIL2VFX_draw_dot: wnd=%i x=%i y=%i color=%i\n", wnd, x, y, color);

   if (gil2vfx_active != -1)
      return;

   VFX_pixel_write(&panes[wnd], xp, yp, (uint8_t)color);
}

int32_t GIL2VFX_read_dot(int32_t wnd, int32_t x, int32_t y)
{
   int32_t xp = x - panes[wnd].x0;
   int32_t yp = y - panes[wnd].y0;

   printf("[gil2vfx] GIL2VFX_read_dot: wnd=%i x=%i y=%i\n", wnd, x, y);

   if (gil2vfx_active != -1)
   {
      // return NULL; // Tom: commented out
      return 0; // Tom: new version
   }

   return VFX_pixel_read(&panes[wnd], xp, yp);
}

void GIL2VFX_draw_line(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color)
{
   int32_t x1p = x1 - panes[wnd].x0;
   int32_t y1p = y1 - panes[wnd].y0;
   int32_t x2p = x2 - panes[wnd].x0;
   int32_t y2p = y2 - panes[wnd].y0;

   printf("[gil2vfx] GIL2VFX_draw_line: wnd=%i x1=%i y1=%i x2=%i y2=%i color=%i\n", wnd, x1, y1, x2, y2, color);

   if (gil2vfx_active != -1)
      return;

   VFX_line_draw(&panes[wnd], x1p, y1p, x2p, y2p, LD_DRAW, color);
}

void GIL2VFX_draw_rect(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color)
{
   int32_t x1p = x1 - panes[wnd].x0;
   int32_t y1p = y1 - panes[wnd].y0;
   int32_t x2p = x2 - panes[wnd].x0;
   int32_t y2p = y2 - panes[wnd].y0;

   printf("[gil2vfx] GIL2VFX_draw_rect: wnd=%i x1=%i y1=%i x2=%i y2=%i color=%i\n", wnd, x1, y1, x2, y2, color);

   if (gil2vfx_active != -1)
      return;

   VFX_line_draw(&panes[wnd], x1p, y1p, x2p, y1p, LD_DRAW, color);
   VFX_line_draw(&panes[wnd], x2p, y1p, x2p, y2p, LD_DRAW, color);
   VFX_line_draw(&panes[wnd], x2p, y2p, x1p, y2p, LD_DRAW, color);
   VFX_line_draw(&panes[wnd], x1p, y2p, x1p, y1p, LD_DRAW, color);
}

void GIL2VFX_fill_rect(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color)
{
   PANE rpane;

   printf("[gil2vfx] GIL2VFX_fill_rect: wnd=%i x1=%i y1=%i x2=%i y2=%i color=%i\n", wnd, x1, y1, x2, y2, color);

   if (gil2vfx_active != -1)
      return;

   rpane.window = panes[wnd].window;
   rpane.x0 = x1;
   rpane.y0 = y1;
   rpane.x1 = x2;
   rpane.y1 = y2;

   VFX_pane_wipe(&rpane, color);
}

void GIL2VFX_hash_rect(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color)
{
   int32_t x1p = x1 - panes[wnd].x0;
   int32_t y1p = y1 - panes[wnd].y0;
   int32_t x2p = x2 - panes[wnd].x0;
   int32_t y2p = y2 - panes[wnd].y0;

   printf("[gil2vfx] GIL2VFX_hash_rect: wnd=%i x1=%i y1=%i x2=%i y2=%i color=%i\n", wnd, x1, y1, x2, y2, color);

   if (gil2vfx_active != -1)
      return;

   VFX_rectangle_hash(&panes[wnd], x1p, y1p, x2p, y2p, color);
}

void GIL2VFX_light_fade(int32_t src_wnd, int32_t color)
{
   RGB palette[256], clr;
   int32_t i;

   printf("[gil2vfx] GIL2VFX_light_fade: src_wnd=%i color=%i\n", src_wnd, color);

   if (gil2vfx_active != -1)
      return;

   if (VFX_DAC_read) // Tom: TODO
      VFX_DAC_read(color, &clr);

   for (i = 0; i < 256; i++)
      palette[i] = clr;

   VFX_window_fade(panes[src_wnd].window, palette, FADE_INTERVALS);
}

void GIL2VFX_color_fade(int32_t src_wnd, int32_t dst_wnd)
{
   RGB palette[256], clr;
   uint32_t colors[256], num_colors;
   int32_t i;

   printf("[gil2vfx] GIL2VFX_color_fade: src_wnd=%i dst_wnd=%i\n", src_wnd, dst_wnd);

   if (gil2vfx_active != -1)
      return;

   // Transitions (e.g. menu teardown on the way to enter-game) can
   // reach here with torn-down panes; dereferencing them faults, as
   // the Continue path demonstrated. Fail the fade cleanly instead.
   if (src_wnd < 0 || src_wnd >= MAX_WINDOWS + MAX_PANES ||
       dst_wnd < 0 || dst_wnd >= MAX_WINDOWS + MAX_PANES ||
       panes[src_wnd].window == NULL || panes[src_wnd].window->buffer == NULL ||
       panes[dst_wnd].window == NULL || panes[dst_wnd].window->buffer == NULL ||
       windows[0].buffer == NULL)
   {
      printf("[gil2vfx] GIL2VFX_color_fade: bad panes, skipped\n");
      return;
   }

   num_colors = VFX_color_scan(&panes[src_wnd], colors);

   for (i = 0; i < 256; i++)
   {
      if (VFX_DAC_read) // Tom: TODO
         VFX_DAC_read(i, &palette[i]);
   }

   clr = palette[*(panes[dst_wnd].window->buffer)];
   for (i = 0; i < num_colors; i++)
   {
      if (VFX_DAC_write) // Tom: TODO
         VFX_DAC_write(colors[i], &clr);
   }

   VFX_pane_copy(&panes[src_wnd], 0, 0, &panes[dst_wnd], 0, 0, NO_COLOR);

   if (VFX_window_refresh) // Tom: TODO
      VFX_window_refresh(&windows[0], 0, 0, VFX->scrn_width - 1, VFX->scrn_height - 1);

   // VFX_window_fade(panes[dst_wnd].window, &palette, FADE_INTERVALS); // Tom: commented out
   VFX_window_fade(panes[dst_wnd].window, (RGB *)&palette, FADE_INTERVALS); // Tom: new version
}

void GIL2VFX_pixel_fade(int32_t src_wnd, int32_t dest_wnd, int32_t intervals)
{
   printf("[gil2vfx] GIL2VFX_pixel_fade: src_wnd=%i dest_wnd=%i intervals=%i\n)", src_wnd, dest_wnd, intervals);

   if (gil2vfx_active != -1)
      return;

   VFX_pixel_fade(&panes[src_wnd], &panes[dest_wnd], intervals, 0); // LUM added last parameter
}

void GIL2VFX_select_text_window(TEXTWINDOW *tw)
{
   printf("[gil2vfx] GIL2VFX_select_text_window: tw=%p\n", (void *)tw);

   twptr = tw;
}

int32_t GIL2VFX_char_width(int32_t ch)
{
   printf("[gil2vfx] GIL2VFX_char_width: ch=%i\n", ch);

   return VFX_character_width(twptr->font, ch);
}

void GIL2VFX_home(void)
{
   printf("[gil2vfx] GIL2VFX_home\n");

   if (gil2vfx_active != -1)
      return;

   VFX_pane_wipe(&panes[twptr->window], ((FONT *)(twptr->font))->font_background);

   twptr->htab = panes[twptr->window].x0;
   twptr->vtab = panes[twptr->window].y0;
}

void GIL2VFX_remap_font_color(int32_t current, int32_t new)
{
   printf("[gil2vfx] GIL2VFX_remap_font_color: current=%i new=%i\n", current, new);

   twptr->lookaside[current] = new;
}

int32_t GIL2VFX_test_overlap(int32_t wnd, int32_t x1, int32_t y1, uint8_t *shapes, int32_t shape_num)
{
   int32_t x2 = (x1 + (VFX_shape_resolution(shapes, shape_num) >> 16));
   int32_t y2 = (y1 + ((int32_t)(int16_t)VFX_shape_resolution(shapes, shape_num)));

   printf("[gil2vfx] GIL2VFX_test_overlap: wnd=%i x1=%i y1=%i shapes=%p shape_num=%i\n", wnd, x1, y1, (void *)shapes, shape_num);

   if ((x1 <= panes[wnd].x1) && (x2 >= panes[wnd].x0) &&
       (y1 <= panes[wnd].y1) && (y2 >= panes[wnd].y0))
      return 1;
   else
      return 0;
}

void GIL2VFX_print(int32_t operation, const char *format, ...)
{
   va_list arglist;
   int32_t cw;

   printf("[gil2vfx] GIL2VFX_print: operation=%i format=%p\n", operation, format);

   va_start(arglist, format);

   //
   // Bound every write to the shared txtbuf (TXTBUF_SIZE).  A runaway
   // string may still clobber the integrity canary bytes at the end of
   // the buffer (checked by graphics.c), matching the original's
   // overflow detection, but can no longer write past the array.
   //

   if (operation == BUF)
   {
      cw = vsnprintf((char *)twptr->txtbuf, TXTBUF_SIZE, format, arglist);
      if (cw < 0)
         cw = 0;
      if (cw > TXTBUF_SIZE - 1)
         cw = TXTBUF_SIZE - 1;
      twptr->txtpnt = twptr->txtbuf + cw;
   }
   else if (operation == APP)
   {
      int32_t room = (int32_t)(TXTBUF_SIZE - (twptr->txtpnt - twptr->txtbuf));

      if (room > 0)
      {
         cw = vsnprintf((char *)twptr->txtpnt, (size_t)room, format, arglist);
         if (cw < 0)
            cw = 0;
         if (cw > room - 1)
            cw = room - 1;
         twptr->txtpnt += cw;
      }
   }
}

void GIL2VFX_scroll_window(int32_t wnd, int32_t dx, int32_t dy, int32_t flags, int32_t background)
{
   printf("[gil2vfx] GIL2VFX_scroll_window: wnd=%i dx=%i dy=%i flags=%i background=%i\n", wnd, dx, dy, flags, background);

   if (gil2vfx_active != -1)
      return;

   VFX_pane_scroll(&panes[wnd], dx, dy, flags, background);
}

void GIL2VFX_print_buffer(int32_t linenum)
{
   printf("[gil2vfx] GIL2VFX_print_buffer: linenum=%i\n", linenum);

   //
   // Structured C port of GIL2VFXA_print_buffer
   // (watcom-original/asm/GIL2VFXA.ASM:277-558): word-wrap scan with
   // VFX_character_width against the remaining window width, then one
   // of four justification handlers (LEFT/RIGHT/CENTER/FILL) dumping
   // through GIL2VFX_cout, with linenum pending-line counting and
   // CR/LF handling between lines. x1/x2 below are the pane's left
   // and right edges (the ASM's x1_1/x2_1 via SETWND).
   //
   // All buffer reads below are capped at buflen (the NUL terminator
   // lives there), so malformed buffers and window overruns terminate
   // the dump instead of walking out of bounds.
   //

   char *buf;
   int32_t buflen;
   int32_t pending;
   int32_t pos;

   if (twptr == NULL || twptr->txtbuf == NULL || twptr->txtpnt == NULL || twptr->font == NULL)
      return;

   if (twptr->window < 0 || twptr->window >= MAX_WINDOWS + MAX_PANES)
      return;

   if (twptr->txtpnt < twptr->txtbuf ||
       twptr->txtpnt > twptr->txtbuf + TXTBUF_SIZE)
      return;

   buf = twptr->txtbuf;
   buflen = (int32_t)(twptr->txtpnt - twptr->txtbuf);

   pending_flag = (linenum != 0);
   eop_reached = 0;
   pending = (linenum > 0) ? linenum - 1 : 0;
   pos = 0;

   for (;;)
   {
      int32_t line_beg, line_end, w_lin, w_win;
      int32_t x1, x2;
      int32_t si;
      int32_t edi_w;

      if (pending_flag != 0 && eop_reached != 0)
         break;

      x1 = panes[twptr->window].x0;
      x2 = panes[twptr->window].x1;
      line_beg = pos;

      if (pos > buflen)
         break;

      // Word-wrap scan: accumulate widths, breaking the line at the
      // last space that fits (or at NUL/LF). edi_w tracks the running
      // right edge starting from the current htab.
      si = pos;
      line_end = pos;
      edi_w = twptr->htab;

      for (;;)
      {
         int32_t ch = (si <= buflen) ? (int32_t)(uint8_t)buf[si] : 0;

         if (ch == 0 || ch == 10)
         {
            si++;
            line_end = si;
            break;
         }

         if (ch == ' ')
            line_end = si;

         si++;
         edi_w += VFX_character_width(twptr->font, ch);

         if (edi_w > x2 + 1)
            break;
      }

      // Guarantee progress: a first word wider than the window would
      // otherwise leave line_end == line_beg forever (the original
      // hangs here); split it mid-word instead. Never fires on real
      // game text.
      if (line_end <= line_beg)
      {
         line_end = line_beg + 1;

         if (line_end > buflen + 1)
            line_end = buflen + 1;
      }

      // Pixels remaining in the window line from the cursor.
      w_win = x2 - twptr->htab + 1;

      // Total pixel width of the line.
      w_lin = 0;

      for (si = line_beg; si < line_end; si++)
         w_lin += VFX_character_width(twptr->font, (si <= buflen) ? (int32_t)(uint8_t)buf[si] : 0);

      if (pending_flag == 0 || pending <= 0)
      {
         int32_t dumped = 0;

         if (pending_flag != 0)
         {
            // Pending satisfied: this and all following lines print.
            pending = 0;
         }

         switch (twptr->justify)
         {
         case J_RIGHT:
         {
            // Right-anchor to the pane end, unless the line overflows
            // the remainder or is empty (then pane-left, like the ASM).
            int32_t first = (line_beg <= buflen) ? (int32_t)(uint8_t)buf[line_beg] : 0;

            if ((uint32_t)w_lin <= (uint32_t)w_win && first != 0)
               twptr->htab = x2 - w_lin + 1;
            else
               twptr->htab = x1;
            break;
         }
         case J_CENTER:
         {
            // Center in the remaining line width.
            int32_t first = (line_beg <= buflen) ? (int32_t)(uint8_t)buf[line_beg] : 0;

            if ((uint32_t)w_win > (uint32_t)w_lin && first != 0)
               twptr->htab += (w_win - w_lin + 1) / 2;
            break;
         }
         case J_FILL:
            print_buffer_fill(buf, buflen, line_beg, line_end, x2, w_lin, w_win);
            dumped = 1;
            break;
         case J_LEFT:
         default:
            break;
         }

         // Dump the line through the text cursor (skipped when the
         // FILL handler already emitted it).
         if (!dumped)
         {
            for (si = line_beg; si < line_end; si++)
            {
               int32_t ch = (si <= buflen) ? (int32_t)(uint8_t)buf[si] : 0;

               if (ch == 0)
                  break;

               GIL2VFX_cout(ch);
            }
         }
      }
      else
      {
         // Pending line: skip output, advance past it if another follows.
         pending--;

         if (line_end <= 0 || ((line_end - 1 <= buflen) ? buf[line_end - 1] : 0) == 0)
            break;
      }

      // Advance past this line; emit CR between lines (an LF already
      // printed needs none), skipping the next line's leading spaces.
      pos = line_end;

      if (pos <= 0 || ((pos - 1 <= buflen) ? buf[pos - 1] : 0) == 0)
         break;

      if (buf[pos - 1] != 10)
         GIL2VFX_cout(10);

      while (pos <= buflen && buf[pos] == ' ')
         pos++;
   }

   pending_flag = 0;

   // GIL2VFXA_print_buffer(&panes[twptr->window], linenum); // Tom: TODO
}

void GIL2VFX_cout(int32_t c)
{
   int32_t cvtab, nvtab, htab;

   printf("[gil2vfx] GIL2VFX_cout: c=%i\n", c);

   if (c == 10)
   {
      htab = twptr->htab = panes[twptr->window].x0; // Carriage Return

      cvtab = twptr->vtab - panes[twptr->window].y0;
      cvtab += twptr->font->char_height;

      nvtab = cvtab + twptr->font->char_height;

      if (nvtab > panes[twptr->window].y1 - panes[twptr->window].y0)
      {
         if (twptr->continueFunction != NULL)
         {
            // if ((twptr->continueFunction(twptr->htab)) == 0) // Tom: commented out
            if (twptr->continueFunction() == 0) // Tom: new version
            {
               twptr->htab = htab;
               return;
            }
         }
         twptr->htab = htab;

         VFX_pane_scroll(&panes[twptr->window], 0, -twptr->font->char_height, PS_NOWRAP, twptr->font->font_background);
      }
      else
      {
         twptr->vtab += twptr->font->char_height;
      }
   }
   else if (c == 13)
   {
      twptr->htab = panes[twptr->window].x0; // Carriage Return
   }
   else
   {
      twptr->htab += VFX_character_draw(&panes[twptr->window],
                                        twptr->htab - panes[twptr->window].x0,
                                        twptr->vtab - panes[twptr->window].y0,
                                        twptr->font, c, twptr->lookaside);
   }
}

void GIL2VFX_refresh_window(uint32_t source, uint32_t target)
{
   // mouse_pane_refresh(&panes[source], &panes[target]);
   //  LUM the function parameter has changed
   //  check whether this is correct!
   PANE *loTargetPane = &panes[target];

   printf("[gil2vfx] GIL2VFX_refresh_window: source=%u target=%u\n", source, target);

   MOUSE_pane_refresh(&panes[source], loTargetPane->x0, loTargetPane->y0, loTargetPane->x1, loTargetPane->y1);

   video_present();
}
