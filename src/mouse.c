// ############################################################################
// ##                                                                        ##
// ##  MOUSE.C   (modified by LUM for AESOP/32, 070105)                      ##
// ##                                                                        ##
// ##  Interrupt-based mouse interface for 386FX VFX drivers                 ##
// ##                                                                        ##
// ##  Version 1.00 of 09-Jul-93: Initial release for Rational DOS/4GW       ##
// ##          1.01 of 04-Sep-93: MOUSE_force_move() added                   ##
// ##                             MOUSE_status() added                       ##
// ##                             Callback function declarations fixed       ##
// ##                             May now be compiled as linkable module or  ##
// ##                              standalone demo program                   ##
// ##          1.02 of 18-Nov-93: MetaWare/Phar Lap support added            ##
// ##          1.10 of  3-Dec-93: Updated to use new WINDOW structure        ##
// ##                             MOUSE_pane_refresh() added                 ##
// ##          1.11 of 15-Feb-94: Upper-case MOUSE_ function names           ##
// ##                             Pane offsets fixed in MOUSE_pane_refresh() ##
// ##          1.12 of 11-Aug-95: Include MSS.H for AIL V3.0 compatibility   ##
// ##                                                                        ##
// ##  Project: 386FX Sound & Light(TM)                                      ##
// ##   Author: John Miles                                                   ##
// ##                                                                        ##
// ##  80386 C source compatible with WATCOM C v9.0 or later                 ##
// ##                                 MetaWare High C++ 3.1 or later         ##
// ##                                                                        ##
// ##  Compile with DEMO #defined to assemble mouse demo program             ##
// ##                                                                        ##
// ##  Example program requires Miles Sound System for timer services        ##
// ##  Must be compiled with stack-checking disabled!                        ##
// ##                                                                        ##
// ############################################################################
// ##                                                                        ##
// ##  Copyright (C) 1992-1994 Non-Linear Arts, Inc.                         ##
// ##                                                                        ##
// ##  Non-Linear Arts, Inc.                                                 ##
// ##  3415 Greystone #200                                                   ##
// ##  Austin, TX 78731                                                      ##
// ##                                                                        ##
// ##  (512) 346-9595 / FAX (512) 346-9596 / BBS (512) 454-9990              ##
// ##                                                                        ##
// ############################################################################

// LUM - this is needed for Watcom
#define DPMI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vfx.h>

#define NO_OLD_SYS_FUNCTIONS

#include "mouse.h"

//
// Mouse system variables
//

static int32_t volatile x;
static int32_t volatile y;
static int32_t volatile left;
static int32_t volatile right;
static int32_t volatile center;

static int32_t volatile last_x;
static int32_t volatile last_y;
static int32_t volatile last_left;
static int32_t volatile last_right;
static int32_t volatile last_center;

static int32_t volatile locked;
static int32_t volatile held;
static RECT saved;

// static int32_t real_event_sel;

// static int32_t real_event_seg;
static int32_t timer;
static WINDOW save;
static WINDOW work;
static PANE workp;
static int32_t buffer_size;
static void *pointer_table;
static int32_t pointer;
static int32_t ptr_width;
static int32_t ptr_height;
static int32_t hot_x;
static int32_t hot_y;
static int32_t scrn_max_x;
static int32_t scrn_max_y;
static int32_t hidecnt;
static int32_t excluded;
static RECT exclude_region;
// LUM added
static int32_t mouse_active = 0;

int32_t (*watchdog_callback)(RECT *area);
void (*MOUSE_event_callback)(int32_t x, int32_t y);
void (*button_event_callback)(int32_t left, int32_t right, int32_t center);

typedef struct // DPMI real-mode interrupt structure
{
   int32_t edi;
   int32_t esi;
   int32_t ebp;
   int32_t reserved;
   int32_t ebx;
   int32_t edx;
   int32_t ecx;
   int32_t eax;
   int16_t flags;
   int16_t es;
   int16_t ds;
   int16_t fs;
   int16_t gs;
   int16_t ip;
   int16_t cs;
   int16_t sp;
   int16_t ss;
} RMI_STRUCT;

#undef min
#undef max
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

// ############################################################################
// ##                                                                        ##
// ## Save area underneath visible portion of mouse cursor, and draw cursor  ##
// ## shape on screen                                                        ##
// ##                                                                        ##
// ############################################################################

static void MOUSE_draw(void)
{
   printf("[mouse] MOUSE_draw\n");

   int32_t shp_x, shp_y;

   // LUM added
   if (!mouse_active)
      return;

   MOUSE_lock();

   //
   // Create windows "save" and "work" based on visible cursor area, clipping
   // to the edge of the screen if necessary
   //
   // If area under cursor is entirely offscreen, exit
   // If area under cursor lies within an exclusion area, exit
   //

   saved.x0 = x + hot_x;
   saved.y0 = y + hot_y;
   saved.x1 = saved.x0 + ptr_width - 1;
   saved.y1 = saved.y0 + ptr_height - 1;

   if ((saved.x0 > scrn_max_x) ||
       (saved.y0 > scrn_max_y) ||
       (saved.x1 < 0) ||
       (saved.y1 < 0))
   {
      excluded = 1;

      MOUSE_unlock();
      return;
   }

   if (saved.x0 < 0)
      saved.x0 = 0;
   if (saved.y0 < 0)
      saved.y0 = 0;
   if (saved.x1 > scrn_max_x)
      saved.x1 = scrn_max_x;
   if (saved.y1 > scrn_max_y)
      saved.y1 = scrn_max_y;

   if (exclude_region.x0 != -1)
      if (MOUSE_shape_in_area(&exclude_region))
      {
         excluded = 1;

         MOUSE_unlock();
         return;
      }

   if (watchdog_callback != NULL)
      if (!watchdog_callback(&saved))
      {
         excluded = 1;

         MOUSE_unlock();
         return;
      }

   excluded = 0;

   workp.x1 = work.x_max = save.x_max = saved.x1 - saved.x0;
   workp.y1 = work.y_max = save.y_max = saved.y1 - saved.y0;

   //
   // Read two copies of the screen area under the cursor
   //
   // "save" will be used later to restore the background
   // "work" will be used to overlay the cursor shape on the screen
   //
   // Warning: If the compiler's "memmove" function is not re-entrant,
   // replace the memmove() call below with another VFX_window_read()
   // call for the "work" window
   //

   if (VFX_window_read) // Tom: TODO
      VFX_window_read(&save, saved.x0, saved.y0, saved.x1, saved.y1);
   memmove(work.buffer, save.buffer, buffer_size); // Tom: TODO NULL check

   //
   // Calculate the offset of the pointer in the (0,0)-based "work" window
   //
   // If "work" is clipped to screen x=0 or y=0, draw at the actual X and
   // Y locations; else -hot_x,-hot_y will align the shape with "work"'s
   // top-left corner
   //
   // Finally, copy "work" back to the physical screen
   //

   shp_x = (x + hot_x < 0) ? x : -hot_x;
   shp_y = (y + hot_y < 0) ? y : -hot_y;

   VFX_shape_draw(&workp, pointer_table, pointer, shp_x, shp_y);

   if (VFX_window_refresh) // Tom: TODO
      VFX_window_refresh(&work, saved.x0, saved.y0, saved.x1, saved.y1);

   MOUSE_unlock();
}

// ############################################################################
// ##                                                                        ##
// ## Restore area underneath mouse cursor                                   ##
// ##                                                                        ##
// ############################################################################

static void MOUSE_restore_area(void)
{
   printf("[mouse] MOUSE_restore_area\n");

   // LUM added
   if (!mouse_active)
      return;

   MOUSE_lock();

   //
   // If cursor not visible, exit
   //

   if (excluded)
   {
      MOUSE_unlock();
      return;
   }

   //
   // Copy the "save" window (written by MOUSE_draw() prior to drawing the
   // mouse cursor) to the screen
   //

   if (VFX_window_refresh) // Tom: TODO
      VFX_window_refresh(&save, saved.x0, saved.y0, saved.x1, saved.y1);

   MOUSE_unlock();
}

// ############################################################################
// ##                                                                        ##
// ## Declare an area of the screen to be off limits to any visible part of  ##
// ## the mouse cursor; the cursor will not be drawn if any portion falls    ##
// ## within this area                                                       ##
// ##                                                                        ##
// ## If x0 < 0, the previously registered exclusion area will be cancelled  ##
// ##                                                                        ##
// ## This exclusion area is internal to the mouse system; for general mouse ##
// ## area exclusion, use the MOUSE_register_watchdog_callback() function    ##
// ##                                                                        ##
// ############################################################################

static void MOUSE_exclude_area(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   printf("[mouse] MOUSE_exclude_area: x0=%i y0=%i x1=%i y1=%i\n", x0, y0, x1, y1);

   exclude_region.x0 = x0;
   exclude_region.y0 = y0;
   exclude_region.x1 = x1;
   exclude_region.y1 = y1;
}

// ############################################################################
// ##                                                                        ##
// ## Install a real-mode mouse event handler in lower 1MB of RAM            ##
// ##                                                                        ##
// ## To avoid the overhead of switching between real and protected mode, we ##
// ## don't take advantage of the DOS extender's built-in support for mouse  ##
// ## event handlers.  Instead, we make a real-mode interrupt call to the    ##
// ## Microsoft INT 33H interface to install a very short real-mode event    ##
// ## handler in the lower 1MB of memory.  Each time the mouse state changes,##
// ## the event handler records the new state in variables accessible to     ##
// ## MOUSE.C.  These variables are then monitored at regular intervals by   ##
// ## the protected-mode timer event handler MOUSE_serve(), which can then   ##
// ## simulate the action of a true protected-mode mouse event handler       ##
// ## with minimal overhead.                                                 ##
// ##                                                                        ##
// ############################################################################

static int32_t MOUSE_install_handler(void)
{
   printf("[STUB] [mouse] MOUSE_install_handler: return=1\n");

   // static unsigned char real_stub[] =
   //     {
   //         0xbe, 0x00, 0x00, // mov si,0
   //         0x8e, 0xde,       // mov ds,si

   //         0x89, 0x1e, 0xf0, 0x04, // mov [4f0],bx   (mouse button state)
   //         0x89, 0x0e, 0xf2, 0x04, // mov [4f2],cx   (pointer X coordinate)
   //         0x89, 0x16, 0xf4, 0x04, // mov [4f4],dx   (pointer Y coordinate)

   //         0xcb // retf
   //     };

   // //
   // // Rational Systems DOS/4GW version
   // //

   // union REGS inregs, outregs;
   // struct SREGS sregs;
   // static RMI_STRUCT RMI;

   // inregs.x.eax = 0x100;
   // inregs.x.ebx = ((sizeof(real_stub) + 15) / 16);

   // int386(0x31, &inregs, &outregs);

   // if (outregs.x.cflag)
   //    return 0;

   // real_event_seg = outregs.x.eax & 0xffff;
   // real_event_sel = outregs.x.edx & 0xffff;

   // memmove((void *)(real_event_seg * 16),
   //         real_stub,
   //         sizeof(real_stub));

   // memset(&RMI, 0, sizeof(RMI));

   // RMI.eax = 0x000c;
   // RMI.ecx = 0x007f;
   // RMI.es = (UWORD)(real_event_seg & 0xffff);

   // inregs.x.eax = 0x300;
   // inregs.x.ebx = 0x33;
   // inregs.x.ecx = 0;
   // inregs.x.edi = FP_OFF((void far *)&RMI);
   // sregs.es = sregs.ds = FP_SEG((void far *)&RMI);

   // int386x(0x31, &inregs, &outregs, &sregs);

   return 1;
}

// ############################################################################
// ##                                                                        ##
// ## Disable real-mode event handler                                        ##
// ##                                                                        ##
// ############################################################################

static void MOUSE_remove_handler(void)
{
   printf("[STUB] [mouse] MOUSE_remove_handler\n");

   // //
   // // Rational Systems DOS/4GW version
   // //

   // union REGS inregs, outregs;
   // struct SREGS sregs;
   // static RMI_STRUCT RMI;

   // memset(&RMI, 0, sizeof(RMI));

   // RMI.eax = 0x000c;
   // RMI.ecx = 0x0000;
   // RMI.es = 0x0000;

   // inregs.x.eax = 0x300;
   // inregs.x.ebx = 0x33;
   // inregs.x.ecx = 0;
   // inregs.x.edi = FP_OFF((void far *)&RMI);
   // sregs.es = sregs.ds = FP_SEG((void far *)&RMI);

   // int386x(0x31, &inregs, &outregs, &sregs);
}

// ############################################################################
// ##                                                                        ##
// ## Inhibit mouse service; stop tracking movement and button activity      ##
// ##                                                                        ##
// ############################################################################

void MOUSE_lock(void)
{
   printf("[mouse] MOUSE_lock\n");

   ++locked;
}

// ############################################################################
// ##                                                                        ##
// ## Enable mouse service; resume movement and button tracking              ##
// ##                                                                        ##
// ############################################################################

void MOUSE_unlock(void)
{
   printf("[mouse] MOUSE_unlock\n");

   --locked;
}

// ############################################################################
// ##                                                                        ##
// ## Inhibit mouse movement tracking (buttons still monitored)              ##
// ##                                                                        ##
// ############################################################################

void MOUSE_hold(void)
{
   printf("[mouse] MOUSE_hold\n");

   ++held;
}

// ############################################################################
// ##                                                                        ##
// ## Enable mouse movement tracking                                         ##
// ##                                                                        ##
// ############################################################################

void MOUSE_release(void)
{
   printf("[mouse] MOUSE_release\n");

   --held;
}

// ############################################################################
// ##                                                                        ##
// ## Return physical area of screen currently occupied by visible portion   ##
// ## of mouse cursor                                                        ##
// ##                                                                        ##
// ## Validates *area RECT structure; returns 0 if mouse pointer hidden,     ##
// ## excluded, or visible portion offscreen                                 ##
// ##                                                                        ##
// ############################################################################

int32_t MOUSE_visible_area(RECT *area)
{
   MOUSE_lock();

   if ((excluded) || (hidecnt < 0))
   {
      MOUSE_unlock();

      printf("[mouse] MOUSE_visible_area: area=%p return=0\n", (void *)area);

      return 0;
   }

   *area = saved;

   MOUSE_unlock();

   printf("[mouse] MOUSE_visible_area: area=%p return=1\n", (void *)area);

   return 1;
}

// ############################################################################
// ##                                                                        ##
// ## Return 1 if any visible portion of mouse cursor lies within given      ##
// ## rectangular area; 0 otherwise                                          ##
// ##                                                                        ##
// ############################################################################

int32_t MOUSE_shape_in_area(RECT *area)
{
   RECT cur;

   if (!MOUSE_visible_area(&cur))
   {
      printf("[mouse] MOUSE_shape_in_area: area=%p return=%i\n", (void *)area, 0);
      return 0;
   }

   if ((cur.x0 > area->x1) ||
       (cur.x1 < area->x0) ||
       (cur.y1 < area->y0) ||
       (cur.y0 > area->y1))
   {
      printf("[mouse] MOUSE_shape_in_area: area=%p return=%i\n", (void *)area, 0);
      return 0;
   }

   printf("[mouse] MOUSE_shape_in_area: area=%p return=%i\n", (void *)area, 1);
   return 1;
}

// ############################################################################
// ##                                                                        ##
// ## Show mouse cursor                                                      ##
// ##                                                                        ##
// ############################################################################

void MOUSE_show(void)
{
   printf("[mouse] MOUSE_show\n");

   MOUSE_lock();

   if (hidecnt)
   {
      ++hidecnt;

      if (!hidecnt)
         MOUSE_draw();
   }

   MOUSE_unlock();
}

// ############################################################################
// ##                                                                        ##
// ## Hide mouse cursor                                                      ##
// ##                                                                        ##
// ############################################################################

void MOUSE_hide(void)
{
   printf("[mouse] MOUSE_hide\n");

   MOUSE_lock();

   if (!hidecnt)
      MOUSE_restore_area();

   --hidecnt;

   MOUSE_unlock();
}

// ############################################################################
// ##                                                                        ##
// ## Set mouse pointer shape                                                ##
// ##                                                                        ##
// ## Note: This routine may be called from a background interrupt handler   ##
// ##       or mouse event callback function                                 ##
// ##                                                                        ##
// ############################################################################

void MOUSE_set_pointer(void *table, int32_t shape)
{
   int32_t hot, res;
   int32_t w, h;

   printf("[mouse] MOUSE_set_pointer: table=%p shape=%i\n", (void *)table, shape);

   if ((pointer_table == table) && (pointer == shape))
      return;

   MOUSE_lock();
   MOUSE_hide();

   pointer_table = table;
   pointer = shape;

   res = VFX_shape_resolution(table, shape);
   hot = VFX_shape_minxy(table, shape);

   ptr_width = w = res >> 16;
   ptr_height = h = res & 0xffff;

   hot_x = (int32_t)(int16_t)(hot >> 16);
   hot_y = (int32_t)(int16_t)(hot & 0xffff);

   //
   // Warning: buffer_size must be <= MAX_MOUSE_BUFFER_SIZE, or memory
   // overwrites will result!
   //

   buffer_size = w * h;

   MOUSE_show();
   MOUSE_unlock();
}

// ############################################################################
// ##                                                                        ##
// ## Return current mouse coordinates and button status                     ##
// ##                                                                        ##
// ## *mx,*my,*ml,*mr,*mc = locations to store mouse status variables for    ##
// ## X,Y,left button, right button, center button respectively              ##
// ##                                                                        ##
// ## Pass NULL pointers for unwanted variables                              ##
// ##                                                                        ##
// ############################################################################

void MOUSE_status(int32_t *mx, int32_t *my, int32_t *ml, int32_t *mr, int32_t *mc)
{
   MOUSE_lock();

   if (mx != NULL)
      *mx = x;
   if (my != NULL)
      *my = y;
   if (ml != NULL)
      *ml = left;
   if (mr != NULL)
      *mr = right;
   if (mc != NULL)
      *mc = center;

   printf("[mouse] MOUSE_status: mx=%i my=%i ml=%i mr=%i mc=%i\n", *mx, *my, *ml, *mr, *mc);

   MOUSE_unlock();
}

// ############################################################################
// ##                                                                        ##
// ## Manually alter the mouse's coordinates                                 ##
// ##                                                                        ##
// ############################################################################

void MOUSE_force_move(int32_t new_x, int32_t new_y)
{
   printf("[STUB] [mouse] MOUSE_force_move: new_x=%i new_y=%i\n", new_x, new_y);

   // union REGS inregs, outregs; // Tom: commented out

   MOUSE_lock();

   //
   // Reset mouse position manually
   //

   /* Tom: commented out
   inregs.x.eax = 4;
   inregs.x.ecx = new_x << 3;
   inregs.x.edx = new_y << 3;
   int386(0x33, &inregs, &outregs);
   */

   //
   // Initialize shared status variables for new location
   //

   /* Tom: commented out
   inregs.x.eax = 3;
   int386(0x33, &inregs, &outregs);
   *(WORD *)0x4f0 = outregs.w.bx;
   *(WORD *)0x4f2 = outregs.w.cx;
   *(WORD *)0x4f4 = outregs.w.dx;
   */

   MOUSE_unlock();
   MOUSE_serve();
}

// ############################################################################
// ##                                                                        ##
// ## Install an application event handler for mouse movement events         ##
// ##                                                                        ##
// ## This function should be passed the address of a function with the      ##
// ## following prototype:                                                   ##
// ##                                                                        ##
// ##   void MOUSE_event_callback(int32_t x, int32_t y)                      ##
// ##                                                                        ##
// ## This function will be called whenever the mouse's location changes,    ##
// ## regardless of whether the cursor is hidden or excluded.                ##
// ##                                                                        ##
// ## Passing NULL to this function will cancel further event callbacks      ##
// ##                                                                        ##
// ############################################################################

void MOUSE_register_mouse_event_callback(void (*fn)(int32_t x, int32_t y))
{
   printf("[mouse] MOUSE_register_mouse_event_callback: fn=%p\n", (void *)fn);

   MOUSE_event_callback = fn;
}

// ############################################################################
// ##                                                                        ##
// ## Install an application event handler for mouse button events           ##
// ##                                                                        ##
// ## This function should be passed the address of a function with the      ##
// ## following prototype:                                                   ##
// ##                                                                        ##
// ##void button_event_callback(int32_t left, int32_t right, int32_t center) ##
// ##                                                                        ##
// ## This function will be called whenever the mouse's button status        ##
// ## changes, regardless of whether the cursor is hidden, excluded, or held.##
// ##                                                                        ##
// ## Passing NULL to this function will cancel further event callbacks      ##
// ##                                                                        ##
// ############################################################################

void MOUSE_register_button_event_callback(void (*fn)(int32_t left, int32_t right, int32_t center))
{
   printf("[mouse] MOUSE_register_button_event_callback: fn=%p\n", (void *)fn);

   button_event_callback = fn;
}

// ############################################################################
// ##                                                                        ##
// ## Install an application "watchdog" handler for mouse pointer exclusion  ##
// ##                                                                        ##
// ## This function should be passed the address of a function with the      ##
// ## following prototype:                                                   ##
// ##                                                                        ##
// ##   int32_t watchdog_callback(RECT *area)                                ##
// ##                                                                        ##
// ## This function will be called whenever the mouse cursor is about to be  ##
// ## drawn to the screen overlaying *area. If it returns a nonzero value,   ##
// ## the cursor will be drawn normally.  Otherwise, the cursor will not be  ##
// ## drawn.                                                                 ##
// ##                                                                        ##
// ## Passing NULL to this function will cancel further watchdog callbacks   ##
// ##                                                                        ##
// ############################################################################

void MOUSE_register_watchdog_callback(int32_t (*fn)(RECT *area))
{
   printf("[mouse] void MOUSE_register_watchdog_callback: fn=%p\n", (void *)fn);

   watchdog_callback = fn;
}

// ############################################################################
// ##                                                                        ##
// ## Timer callback routine simulates protected-mode mouse event handler    ##
// ## by periodically inspecting variables maintained by real-mode handler   ##
// ##                                                                        ##
// ## The real-mode event handler shares data with this timer handler via    ##
// ## three variables defined at 0:4F0-0:4F5, an area of low memory          ##
// ## designated for interprocess communication                              ##
// ##                                                                        ##
// ## To eliminate all mouse flicker during movement, add a call to          ##
// ## VFX_wait_vblank_leading() just before the MOUSE_restore_area() call    ##
// ## below.  However, this can cause a substantial performance loss during  ##
// ## periods of rapid mouse movement, especially in EVGA.                   ##
// ##                                                                        ##
// ## Notes: This routine is called from an asynchronous interrupt handler!  ##
// ##        May also be called manually to force status update              ##
// ##                                                                        ##
// ############################################################################

// LUM this function had to be changed for AIL 2 (__cdecl not __pascal and parameter user was removed)!
// it is needed, otherwise it crashes when the function is called!

void MOUSE_timer_serve()
{
   printf("[mouse] MOUSE_timer_serve\n");

   MOUSE_serve();
}

void MOUSE_serve()
{
   printf("[mouse] MOUSE_serve\n");

   if (locked > 0)
      return;

   ++locked;

   last_left = left;
   last_right = right;
   last_center = center;

   // Tom: TODO zeroed for now
   left = 0;
   right = 0;
   center = 0;

   /* Tom: commented out
   left = ((*(int16_t *)0x4f0) & 0x01) != 0;
   right = ((*(int16_t *)0x4f0) & 0x02) != 0;
   center = ((*(int16_t *)0x4f0) & 0x04) != 0;
   */

   if ((left != last_left) || (right != last_right) || (center != last_center))
      if (button_event_callback != NULL)
         button_event_callback(left, right, center);

   if (held > 0)
   {
      --locked;
      return;
   }

   last_x = x;
   last_y = y;

   // Tom: TODO zeroed for now
   x = 0;
   y = 0;

   /* Tom: commented out
   x = (int32_t)(*(int16_t *)0x4f2) >> 3;
   y = (int32_t)(*(int16_t *)0x4f4) >> 3;
   */

   if ((x != last_x) || (y != last_y))
   {
      if (MOUSE_event_callback != NULL)
         MOUSE_event_callback(x, y);

      if (hidecnt >= 0)
      {
         MOUSE_restore_area(); // restore area at old cursor position
         MOUSE_draw();         // draw the cursor
      }
   }

   --locked;
}

// ############################################################################
// ##                                                                        ##
// ## Wrapper for VFX_window_refresh() which automatically handles mouse     ##
// ## cursor maintenance if the mouse cursor falls within the target screen  ##
// ## coordinates                                                            ##
// ##                                                                        ##
// ############################################################################

void MOUSE_window_refresh(WINDOW *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   printf("[mouse] MOUSE_window_refresh: target=%p x0=%i y0=%i x1=%i y1=%i\n", (void *)target, x0, y0, x1, y1);

   int32_t shp_x, shp_y;
   int32_t bw, bh;
   PANE bkgnd, client;
   int32_t mx, my, ml, mr, mt, mb, xr;
   uint8_t *in;

   //
   // If mouse hidden, just call the driver and return
   //

   if (hidecnt < 0)
   {
      // Tom: TODO
      if (VFX_window_refresh)
         VFX_window_refresh(target, x0, y0, x1, y1);
      return;
   }

   //
   // Freeze the mouse to ensure valid coordinates
   //

   MOUSE_hold();

   //
   // If mouse is currently outside the region to be refreshed, register the
   // region as an exclusion area, release the mouse, do the refresh,
   // cancel the exclusion area, and force an update
   //

   if ((saved.x0 > x1) ||
       (saved.x1 < x0) ||
       (saved.y1 < y0) ||
       (saved.y0 > y1))
   {
      MOUSE_exclude_area(x0, y0, x1, y1);

      MOUSE_release();

      // Tom: TODO
      if (VFX_window_refresh)
         VFX_window_refresh(target, x0, y0, x1, y1);

      MOUSE_exclude_area(-1, -1, -1, -1);

      MOUSE_serve();
      return;
   }

   //
   // If this is a scaled window refresh, do a hide/refresh/show cycle and
   // return
   //

   if (((x1 - x0) != target->x_max) || ((y1 - y0) != target->y_max))
   {
      if (VFX_wait_vblank_leading) // Tom: TODO
         VFX_wait_vblank_leading();
      MOUSE_hide();
      if (VFX_window_refresh) // Tom: TODO
         VFX_window_refresh(target, x0, y0, x1, y1);
      MOUSE_show();

      MOUSE_release();
      MOUSE_serve();
      return;
   }

   //
   // If client window has a stencil, do a hide/refresh/show cycle if
   // the mouse cursor overlaps a non-transparent area of the stencil
   //

   if (target->stencil != NULL)
   {
      //
      // Find edges of visible mouse cursor relative to
      // window being refreshed
      //

      ml = max(x0, saved.x0) - x0;
      mr = min(x1, saved.x1) - x0;
      mt = max(y0, saved.y0) - y0;
      mb = min(y1, saved.y1) - y0;

      //
      // See if a non-transparent stencil packet overlaps mouse cursor
      //

      for (my = mt; my <= mb; my++)
      {
         mx = 0;

         in = (uint8_t *)target->stencil + (((uint32_t *)target->stencil)[my]);

         while (mx <= mr)
         {
            xr = mx + (*in & 0x7f) - 1;

            if (*in < 128)
            {
               if (!((mx > mr) || (xr < ml)))
               {
                  if (VFX_wait_vblank_leading) // Tom: TODO
                     VFX_wait_vblank_leading();
                  MOUSE_hide();
                  if (VFX_window_refresh) // Tom: TODO
                     VFX_window_refresh(target, x0, y0, x1, y1);
                  MOUSE_show();

                  MOUSE_release();
                  MOUSE_serve();
                  return;
               }
            }

            in++;
            mx = xr + 1;
         }
      }
   }

   //
   // If mouse is inside the region to be refreshed, temporarily merge the
   // pointer shape with the client's window so that it will not be erased
   // during the refresh, do the refresh, and restore the client's window
   // contents
   //

   //
   // Define pane to describe part of client window which will be overlaid
   // by cursor
   //

   client.window = target;
   client.x0 = max(x0, saved.x0) - x0;
   client.x1 = min(x1, saved.x1) - x0;
   client.y0 = max(y0, saved.y0) - y0;
   client.y1 = min(y1, saved.y1) - y0;

   //
   // Define pane to describe part of saved background window which
   // lies within client screen area
   //

   bw = saved.x1 - saved.x0;
   bh = saved.y1 - saved.y0;

   bkgnd.window = &save;
   bkgnd.x0 = (saved.x0 < x0) ? x0 - saved.x0 : 0;
   bkgnd.x1 = (saved.x1 > x1) ? bw - (saved.x1 - x1) : bw;
   bkgnd.y0 = (saved.y0 < y0) ? y0 - saved.y0 : 0;
   bkgnd.y1 = (saved.y1 > y1) ? bh - (saved.y1 - y1) : bh;

   //
   // Update background preservation window by copying client source
   // pane to background pane
   //

   VFX_pane_copy(&client, 0, 0, &bkgnd, 0, 0, NO_COLOR);

   //
   // Draw the mouse pointer into the client's source window, so it will
   // appear onscreen at the correct place when the window is refreshed
   //

   shp_x = (x + hot_x < x0) ? x - x0 : -hot_x;
   shp_y = (y + hot_y < y0) ? y - y0 : -hot_y;

   VFX_shape_draw(&client, pointer_table, pointer, shp_x, shp_y);

   //
   // Copy client's window to screen, including the overlaid mouse pointer
   //

   if (VFX_window_refresh)
      VFX_window_refresh(target, x0, y0, x1, y1);

   //
   // Finally, restore the part of the client's window which we overwrote
   // with the mouse pointer (not necessary if client doesn't intend to
   // re-use contents of window)
   //

   VFX_pane_copy(&bkgnd, 0, 0, &client, 0, 0, NO_COLOR);

   //
   // Release the mouse and force service in case coordinates
   // need updating
   //

   MOUSE_release();
   MOUSE_serve();
}

// ############################################################################
// ##                                                                        ##
// ## Wrapper for VFX_pane_refresh() which automatically handles mouse       ##
// ## cursor maintenance if the mouse cursor falls within the target screen  ##
// ## coordinates                                                            ##
// ##                                                                        ##
// ############################################################################

void MOUSE_pane_refresh(PANE *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   printf("[mouse] MOUSE_pane_refresh: target=%p x0=%i y0=%i x1=%i y1=%i\n", (void *)target, x0, y0, x1, y1);

   int32_t shp_x, shp_y;
   int32_t bw, bh;
   PANE bkgnd, client;
   int32_t mx, my, ml, mr, mt, mb, xr;
   uint8_t *in;

   //
   // If mouse hidden, just call the driver and return
   //

   if (hidecnt < 0)
   {
      if (VFX_pane_refresh) // Tom: TODO
         VFX_pane_refresh(target, x0, y0, x1, y1);
      return;
   }

   //
   // Freeze the mouse to ensure valid coordinates
   //

   MOUSE_hold();

   //
   // If mouse is currently outside the region to be refreshed, register the
   // region as an exclusion area, release the mouse, do the refresh,
   // cancel the exclusion area, and force an update
   //

   if ((saved.x0 > x1) ||
       (saved.x1 < x0) ||
       (saved.y1 < y0) ||
       (saved.y0 > y1))
   {
      MOUSE_exclude_area(x0, y0, x1, y1);

      MOUSE_release();

      if (VFX_pane_refresh) // Tom: TODO
         VFX_pane_refresh(target, x0, y0, x1, y1);

      MOUSE_exclude_area(-1, -1, -1, -1);

      MOUSE_serve();
      return;
   }

   //
   // If this is a scaled pane refresh, do a hide/refresh/show cycle and
   // return
   //

   if (((x1 - x0) != (min(target->x1, target->window->x_max) - max(target->x0, 0))) ||
       ((y1 - y0) != (min(target->y1, target->window->y_max) - max(target->y0, 0))))
   {
      if (VFX_wait_vblank_leading) // Tom: TODO
         VFX_wait_vblank_leading();
      MOUSE_hide();
      if (VFX_pane_refresh) // Tom: TODO
         VFX_pane_refresh(target, x0, y0, x1, y1);
      MOUSE_show();

      MOUSE_release();
      MOUSE_serve();
      return;
   }

   //
   // If client window has a stencil, do a hide/refresh/show cycle if
   // the mouse cursor overlaps a non-transparent area of the stencil
   //

   if (target->window->stencil != NULL)
   {
      //
      // Find edges of visible mouse cursor relative to
      // window being refreshed
      //

      ml = max(x0, saved.x0) - x0 + target->x0;
      mr = min(x1, saved.x1) - x0 + target->x0;
      mt = max(y0, saved.y0) - y0 + target->y0;
      mb = min(y1, saved.y1) - y0 + target->y0;

      //
      // See if a non-transparent stencil packet overlaps mouse cursor
      //

      for (my = mt; my <= mb; my++)
      {
         mx = 0;

         in = (uint8_t *)target->window->stencil + (((uint32_t *)target->window->stencil)[my]);

         while (mx <= mr)
         {
            xr = mx + (*in & 0x7f) - 1;

            if (*in < 128)
            {
               if (!((mx > mr) || (xr < ml)))
               {
                  if (VFX_wait_vblank_leading) // Tom: TODO
                     VFX_wait_vblank_leading();
                  MOUSE_hide();
                  if (VFX_pane_refresh) // Tom: TODO
                     VFX_pane_refresh(target, x0, y0, x1, y1);
                  MOUSE_show();

                  MOUSE_release();
                  MOUSE_serve();
                  return;
               }
            }

            in++;
            mx = xr + 1;
         }
      }
   }

   //
   // If mouse is inside the region to be refreshed, temporarily merge the
   // pointer shape with the client's pane so that it will not be erased
   // during the refresh, do the refresh, and restore the client's pane
   // contents
   //

   //
   // Define pane to describe part of client window which will be overlaid
   // by cursor
   //
   // Set client pane coordinates relative to target pane
   //

   client.window = target->window;
   client.x0 = max(x0, saved.x0) - x0 + target->x0;
   client.x1 = min(x1, saved.x1) - x0 + target->x0;
   client.y0 = max(y0, saved.y0) - y0 + target->y0;
   client.y1 = min(y1, saved.y1) - y0 + target->y0;

   //
   // Define pane to describe part of saved background window which
   // lies within client screen area
   //

   bw = saved.x1 - saved.x0;
   bh = saved.y1 - saved.y0;

   bkgnd.window = &save;
   bkgnd.x0 = (saved.x0 < x0) ? x0 - saved.x0 : 0;
   bkgnd.x1 = (saved.x1 > x1) ? bw - (saved.x1 - x1) : bw;
   bkgnd.y0 = (saved.y0 < y0) ? y0 - saved.y0 : 0;
   bkgnd.y1 = (saved.y1 > y1) ? bh - (saved.y1 - y1) : bh;

   //
   // Update background preservation window by copying client source
   // pane to background pane
   //

   VFX_pane_copy(&client, 0, 0, &bkgnd, 0, 0, NO_COLOR);

   //
   // Draw the mouse pointer into the client's source window, so it will
   // appear onscreen at the correct place when the window is refreshed
   //

   shp_x = (x + hot_x < x0) ? x - x0 : -hot_x;
   shp_y = (y + hot_y < y0) ? y - y0 : -hot_y;

   VFX_shape_draw(&client, pointer_table, pointer, shp_x, shp_y);

   //
   // Copy client's pane to screen, including the overlaid mouse pointer
   //

   if (VFX_pane_refresh) // Tom: TODO
      VFX_pane_refresh(target, x0, y0, x1, y1);

   //
   // Finally, restore the part of the client's window which we overwrote
   // with the mouse pointer (not necessary if client doesn't intend to
   // re-use contents of window)
   //

   VFX_pane_copy(&bkgnd, 0, 0, &client, 0, 0, NO_COLOR);

   //
   // Release the mouse and force service in case coordinates
   // need updating
   //

   MOUSE_release();
   MOUSE_serve();
}

// ############################################################################
// ##                                                                        ##
// ## Perform MOUSE_pane_refresh() calls for all entries in a PANE_LIST      ##
// ##                                                                        ##
// ############################################################################

void MOUSE_pane_list_refresh(PANE_LIST *list)
{
   printf("[mouse] MOUSE_pane_list_refresh: list=%p\n", (void *)list);

   int32_t i;
   PANE *a;

   for (i = 0; i < list->size; i++)
   {
      if (list->flags[i] == PL_VALID)
      {
         a = &list->array[i];

         MOUSE_pane_refresh(a, a->x0, a->y0, a->x1, a->y1);
      }
   }
}

// ############################################################################
// ##                                                                        ##
// ## Initialize mouse system                                                ##
// ##                                                                        ##
// ############################################################################

int32_t MOUSE_init(int32_t xsize, int32_t ysize, int32_t background)
{
   printf("[mouse] MOUSE_init: xsize=%i ysize=%i background=%i\n", xsize, ysize, background);

   //
   // Initialize mouse system variables: by default, mouse is unlocked,
   // released, and hidden, with no valid pointer shape table and no exclusion
   // area
   //

   locked = 0;
   held = 0;
   hidecnt = -1;
   pointer_table = NULL;
   excluded = 0;

   MOUSE_exclude_area(-1, -1, -1, -1);

   MOUSE_event_callback = NULL;
   button_event_callback = NULL;
   watchdog_callback = NULL;

   x = y = left = right = center = -1;

   scrn_max_x = xsize - 1;
   scrn_max_y = ysize - 1;

   //
   // Init mouse driver, returning 0 on failure
   //

   // Tom: TODO
   // inregs.x.eax = 0;
   // int386(0x33, &inregs, &outregs);

   // if (outregs.w.ax != 0xffff)
   //    return 0;

   //
   // Set horizontal and vertical limits for mouse movement
   // Multiply limits by 8 to obtain single-pixel resolution
   //

   // Tom: hardcoded
   scrn_max_x = 320;
   scrn_max_y = 200;

   // inregs.x.eax = 7;
   // inregs.x.ecx = 0;
   // inregs.x.edx = scrn_max_x << 3;
   // int386(0x33, &inregs, &outregs);

   // inregs.x.eax = 8;
   // inregs.x.ecx = 0;
   // inregs.x.edx = scrn_max_y << 3;
   // int386(0x33, &inregs, &outregs);

   //
   // Set mouse movement rate = 1 mickey/pixel
   //

   // inregs.x.eax = 0x0f;
   // inregs.x.ecx = 1;
   // inregs.x.edx = 1;
   // int386(0x33, &inregs, &outregs);

   if (!MOUSE_install_handler())
      return 0;

   //
   // Initialize windows used by pointer rendering
   //

   // Tom: TODO NULL checking
   work.buffer = malloc(MAX_MOUSE_BUFFER_SIZE);
   save.buffer = malloc(MAX_MOUSE_BUFFER_SIZE);

   work.stencil = work.shadow = NULL;
   save.stencil = save.shadow = NULL;

   workp.x0 = 0;
   workp.y0 = 0;

   workp.window = &work;

   //
   // Force service to validate mouse status variables
   //

   MOUSE_force_move(xsize / 2, ysize / 2);

   //
   // Arrange for periodic background mouse service
   //
   // The mouse service rate should equal the vertical refresh period
   // for smoothest cursor movement.  In 200-line mode, we assume a period
   // of 70 Hz; otherwise, a 60 Hz timer rate is used.
   //
   // If AIL/32 timer services are unavailable, the application must arrange
   // for another source of periodic service.  The MOUSE_serve() routine may
   // either be polled, or called from an interrupt handler.  To eliminate
   // all mouse flicker, call MOUSE_serve() from an interrupt handler synced
   // to the actual vertical retrace timing signal.
   //

   timer = -1;

   /* Tom: commented out
   if (background)
   {
      timer = AIL_register_timer(MOUSE_timer_serve);

      if (timer != -1)
      {
         if (ysize >= 400)
            AIL_set_timer_frequency(timer, 60);
         else
            AIL_set_timer_frequency(timer, 70);

         AIL_start_timer(timer);
      }
   }
   */

   // LUM added
   mouse_active = 1;

   return 1;
}

// ############################################################################
// ##                                                                        ##
// ## Shut down mouse system                                                 ##
// ##                                                                        ##
// ############################################################################

void MOUSE_shutdown(void)
{
   printf("[mouse] MOUSE_shutdown\n");

   // LUM added
   mouse_active = 0;

   MOUSE_hide();

   // if (timer != -1)
   // {
   //    AIL_stop_timer(timer);
   //    AIL_release_timer_handle(timer);
   // }

   MOUSE_remove_handler();

   //
   // Free pointer background buffer memory
   //

   free(save.buffer);
   free(work.buffer);

   //
   // Free real-mode memory used by event handler stub
   //

   // inregs.x.eax = 0x101;
   // inregs.x.edx = real_event_sel;
   // int386(0x31, &inregs, &outregs);
}
