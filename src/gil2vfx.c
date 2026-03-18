#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include "vfx.h"
#include "gil2vfx.h"

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

extern VFX_DESC *VFX;
extern int8_t *pathname;

extern int32_t eop_reached;
extern int32_t pending_flag;

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

   // VFX = VFX_describe_driver();

   //
   // Turn graphics on; set up window and pane
   //

   gil2vfx_active = -1;

   bitmap_buffer = mem_alloc(320 * 200);

   // Tom: segfault?
   windows[0].buffer = (void *)0x0a0000; // Page 1 = 0
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
         // VFX_fixed_mul(VFX_shape_bounds(shapes, shape_num) & 0xffff0000, 0x10000 - x_scale, &xs); // Tom: commented out
         VFX_fixed_mul(VFX_shape_bounds(shapes, shape_num) & 0xffff0000, 0x10000 - x_scale, (int16_t *)&xs); // Tom: new version

         // VFX_fixed_mul(VFX_shape_bounds(shapes, shape_num) << 16, 0x10000 - y_scale, &ys); // Tom: commented out
         VFX_fixed_mul(VFX_shape_bounds(shapes, shape_num) << 16, 0x10000 - y_scale, (int16_t *)&ys); // Tom: new version

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

   if (operation == BUF)
   {
      cw = vsprintf(twptr->txtbuf, format, arglist);
      twptr->txtpnt = twptr->txtbuf + cw;
   }
   else if (operation == APP)
   {
      cw = vsprintf(twptr->txtpnt, format, arglist);
      twptr->txtpnt += cw;
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
}
