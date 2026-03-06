// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  MOUSE.C   (modified by LUM for AESOP/32, 070105)                      ��
// ��                                                                        ��
// ��  Interrupt-based mouse interface for 386FX VFX drivers                 ��
// ��                                                                        ��
// ��  Version 1.00 of 09-Jul-93: Initial release for Rational DOS/4GW       ��
// ��          1.01 of 04-Sep-93: MOUSE_force_move() added                   ��
// ��                             MOUSE_status() added                       ��
// ��                             Callback function declarations fixed       ��
// ��                             May now be compiled as linkable module or  ��
// ��                              standalone demo program                   ��
// ��          1.02 of 18-Nov-93: MetaWare/Phar Lap support added            ��
// ��          1.10 of  3-Dec-93: Updated to use new WINDOW structure        ��
// ��                             MOUSE_pane_refresh() added                 ��
// ��          1.11 of 15-Feb-94: Upper-case MOUSE_ function names           ��
// ��                             Pane offsets fixed in MOUSE_pane_refresh() ��
// ��          1.12 of 11-Aug-95: Include MSS.H for AIL V3.0 compatibility   ��
// ��                                                                        ��
// ��  Project: 386FX Sound & Light(TM)                                      ��
// ��   Author: John Miles                                                   ��
// ��                                                                        ��
// ��  80386 C source compatible with WATCOM C v9.0 or later                 ��
// ��                                 MetaWare High C++ 3.1 or later         ��
// ��                                                                        ��
// ��  Compile with DEMO #defined to assemble mouse demo program             ��
// ��                                                                        ��
// ��  Example program requires Miles Sound System for timer services        ��
// ��  Must be compiled with stack-checking disabled!                        ��
// ��                                                                        ��
// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  Copyright (C) 1992-1994 Non-Linear Arts, Inc.                         ��
// ��                                                                        ��
// ��  Non-Linear Arts, Inc.                                                 ��
// ��  3415 Greystone #200                                                   ��
// ��  Austin, TX 78731                                                      ��
// ��                                                                        ��
// ��  (512) 346-9595 / FAX (512) 346-9596 / BBS (512) 454-9990              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// LUM - this is needed for Watcom
#define DPMI

#include <stdio.h>
#include <stdlib.h>
// #include <dos.h> // Tom: commented out
// #include <conio.h> // Tom: commented out
#include <string.h>

// #include "vfx.h" // Tom: commented out
// #include "dll.h" // Tom: commented out

#define NO_OLD_SYS_FUNCTIONS
// LUM include ail32.h instead of mss.h
// #include "ail32.h" // Tom: commented out

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
// static RECT saved; // Tom: commented out

#ifdef DPMI
static int32_t real_event_sel;
#endif

static int32_t real_event_seg;
// static HTIMER timer; // Tom: commented out
// static WINDOW save; // Tom: commented out
// static WINDOW work; // Tom: commented out
// static PANE workp; // Tom: commented out
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
// static RECT exclude_region; // Tom: commented out
// LUM added
static int32_t mouse_active = 0;

// int32_t (*watchdog_callback)(RECT *area); // Tom: commented out
void (*MOUSE_event_callback)(int32_t x, int32_t y);
void (*button_event_callback)(int32_t left, int32_t right, int32_t center);

#ifdef DPMI

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

#else

typedef struct // INT21 (Phar Lap) real-mode interrupt structure
{
   int16_t intnum;
   int16_t ds;
   int16_t es;
   int16_t fs;
   int16_t gs;
   int32_t eax;
   int32_t edx;
} RMI_STRUCT;

#endif

#undef min
#undef max
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Save area underneath visible portion of mouse cursor, and draw cursor  ��
// �� shape on screen                                                        ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

static void MOUSE_draw(void)
{
   printf("[STUB] MOUSE_draw\n");
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Restore area underneath mouse cursor                                   ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

static void MOUSE_restore_area(void)
{
   printf("[STUB] MOUSE_restore_area\n");
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Declare an area of the screen to be off limits to any visible part of  ��
// �� the mouse cursor; the cursor will not be drawn if any portion falls    ��
// �� within this area                                                       ��
// ��                                                                        ��
// �� If x0 < 0, the previously registered exclusion area will be cancelled  ��
// ��                                                                        ��
// �� This exclusion area is internal to the mouse system; for general mouse ��
// �� area exclusion, use the MOUSE_register_watchdog_callback() function    ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

static void MOUSE_exclude_area(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   // Tom: commented out
   // exclude_region.x0 = x0;
   // exclude_region.y0 = y0;
   // exclude_region.x1 = x1;
   // exclude_region.y1 = y1;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Install a real-mode mouse event handler in lower 1MB of RAM            ��
// ��                                                                        ��
// �� To avoid the overhead of switching between real and protected mode, we ��
// �� don't take advantage of the DOS extender's built-in support for mouse  ��
// �� event handlers.  Instead, we make a real-mode interrupt call to the    ��
// �� Microsoft INT 33H interface to install a very short real-mode event    ��
// �� handler in the lower 1MB of memory.  Each time the mouse state changes,��
// �� the event handler records the new state in variables accessible to     ��
// �� MOUSE.C.  These variables are then monitored at regular intervals by   ��
// �� the protected-mode timer event handler MOUSE_serve(), which can then   ��
// �� simulate the action of a true protected-mode mouse event handler       ��
// �� with minimal overhead.                                                 ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

static int32_t MOUSE_install_handler(void)
{
   static unsigned char real_stub[] =
       {
           0xbe, 0x00, 0x00, // mov si,0
           0x8e, 0xde,       // mov ds,si

           0x89, 0x1e, 0xf0, 0x04, // mov [4f0],bx   (mouse button state)
           0x89, 0x0e, 0xf2, 0x04, // mov [4f2],cx   (pointer X coordinate)
           0x89, 0x16, 0xf4, 0x04, // mov [4f4],dx   (pointer Y coordinate)

           0xcb // retf
       };

   //
   // Rational Systems DOS/4GW version
   //

   {
#ifdef DPMI
      // Tom: commented out
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
      // RMI.es = (uint16_t)(real_event_seg & 0xffff);

      // inregs.x.eax = 0x300;
      // inregs.x.ebx = 0x33;
      // inregs.x.ecx = 0;
      // inregs.x.edi = FP_OFF((void far *)&RMI);
      // sregs.es = sregs.ds = FP_SEG((void far *)&RMI);

      // int386x(0x31, &inregs, &outregs, &sregs);

#else

      //
      // MetaWare / Phar Lap version
      //

      uint32_t i;
      FARPTR stub;
      union REGS inregs, outregs;
      static RMI_STRUCT RMI;

      inregs.w.ebx = ((sizeof(real_stub) + 15) / 16);

      inregs.w.eax = 0x25c0;
      int86(0x21, &inregs, &outregs);

      if (outregs.w.cflag)
         return 0;

      real_event_seg = outregs.w.eax & 0xffff;

      stub.part.seg = 0x34;
      stub.part.off = real_event_seg * 16;

      for (i = 0; i < sizeof(real_stub); i++)
      {
         ((_Far uint8_t *)stub.ptr)[i] = real_stub[i];
      }

      inregs.w.edx = (uint32_t)&RMI;
      inregs.w.ecx = 0x007f;
      RMI.intnum = 0x33;
      RMI.eax = 0x000c;
      RMI.edx = 0;
      RMI.es = (uint16_t)(real_event_seg & 0xffff);

      inregs.w.eax = 0x2511;
      int86(0x21, &inregs, &outregs);

#endif

      return 1;
   }
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Disable real-mode event handler                                        ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

static void MOUSE_remove_handler(void)
{
   //
   // Rational Systems DOS/4GW version
   //

#ifdef DPMI
   // Tom: commented out
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

#else

   union REGS inregs, outregs;
   static RMI_STRUCT RMI;

   inregs.w.edx = (uint32_t)&RMI;
   inregs.w.ecx = 0;
   RMI.intnum = 0x33;
   RMI.eax = 0x000c;
   RMI.edx = 0;
   RMI.es = 0;

   inregs.w.eax = 0x2511;
   int86(0x21, &inregs, &outregs);

#endif
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Inhibit mouse service; stop tracking movement and button activity      ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_lock(void)
{
   ++locked;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Enable mouse service; resume movement and button tracking              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_unlock(void)
{
   --locked;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Inhibit mouse movement tracking (buttons still monitored)              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_hold(void)
{
   ++held;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Enable mouse movement tracking                                         ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_release(void)
{
   --held;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Return physical area of screen currently occupied by visible portion   ��
// �� of mouse cursor                                                        ��
// ��                                                                        ��
// �� Validates *area RECT structure; returns 0 if mouse pointer hidden,     ��
// �� excluded, or visible portion offscreen                                 ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// Tom: commented out
// int32_t MOUSE_visible_area(RECT *area)
// {
//    MOUSE_lock();

//    if ((excluded) || (hidecnt < 0))
//    {
//       MOUSE_unlock();
//       return 0;
//    }

//    *area = saved;

//    MOUSE_unlock();

//    return 1;
// }

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Return 1 if any visible portion of mouse cursor lies within given      ��
// �� rectangular area; 0 otherwise                                          ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// Tom: commented out
// int32_t MOUSE_shape_in_area(RECT *area)
// {
//    RECT cur;

//    if (!MOUSE_visible_area(&cur))
//       return 0;

//    if ((cur.x0 > area->x1) ||
//        (cur.x1 < area->x0) ||
//        (cur.y1 < area->y0) ||
//        (cur.y0 > area->y1))
//       return 0;

//    return 1;
// }

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Show mouse cursor                                                      ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_show(void)
{
   MOUSE_lock();

   if (hidecnt)
   {
      ++hidecnt;

      if (!hidecnt)
         MOUSE_draw();
   }

   MOUSE_unlock();
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Hide mouse cursor                                                      ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_hide(void)
{
   MOUSE_lock();

   if (!hidecnt)
      MOUSE_restore_area();

   --hidecnt;

   MOUSE_unlock();
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Set mouse pointer shape                                                ��
// ��                                                                        ��
// �� Note: This routine may be called from a background interrupt handler   ��
// ��       or mouse event callback function                                 ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_set_pointer(void *table, int32_t shape)
{
   int32_t hot, res;
   int32_t w, h;

   if ((pointer_table == table) &&
       (pointer == shape))
      return;

   MOUSE_lock();
   MOUSE_hide();

   pointer_table = table;
   pointer = shape;

   // res = VFX_shape_resolution(table, shape);
   // hot = VFX_shape_minxy(table, shape);

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

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Return current mouse coordinates and button status                     ��
// ��                                                                        ��
// �� *mx,*my,*ml,*mr,*mc = locations to store mouse status variables for    ��
// �� X,Y,left button, right button, center button respectively              ��
// ��                                                                        ��
// �� Pass NULL pointers for unwanted variables                              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

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

   MOUSE_unlock();
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Manually alter the mouse's coordinates                                 ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_force_move(int32_t new_x, int32_t new_y)
{
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Install an application event handler for mouse movement events         ��
// ��                                                                        ��
// �� This function should be passed the address of a function with the      ��
// �� following prototype:                                                   ��
// ��                                                                        ��
// ��   void MOUSE_event_callback(int32_t x, int32_t y)                      ��
// ��                                                                        ��
// �� This function will be called whenever the mouse's location changes,    ��
// �� regardless of whether the cursor is hidden or excluded.                ��
// ��                                                                        ��
// �� Passing NULL to this function will cancel further event callbacks      ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_register_mouse_event_callback(void (*fn)(int32_t x, int32_t y))
{
   MOUSE_event_callback = fn;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Install an application event handler for mouse button events           ��
// ��                                                                        ��
// �� This function should be passed the address of a function with the      ��
// �� following prototype:                                                   ��
// ��                                                                        ��
// ��   void button_event_callback(int32_t left, int32_t right, int32_t center) ��
// ��                                                                        ��
// �� This function will be called whenever the mouse's button status        ��
// �� changes, regardless of whether the cursor is hidden, excluded, or held.��
// ��                                                                        ��
// �� Passing NULL to this function will cancel further event callbacks      ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_register_button_event_callback(void (*fn)(int32_t left, int32_t right, int32_t center))
{
   button_event_callback = fn;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Install an application "watchdog" handler for mouse pointer exclusion  ��
// ��                                                                        ��
// �� This function should be passed the address of a function with the      ��
// �� following prototype:                                                   ��
// ��                                                                        ��
// ��   int32_t watchdog_callback(RECT *area)                             ��
// ��                                                                        ��
// �� This function will be called whenever the mouse cursor is about to be  ��
// �� drawn to the screen overlaying *area. If it returns a nonzero value,   ��
// �� the cursor will be drawn normally.  Otherwise, the cursor will not be  ��
// �� drawn.                                                                 ��
// ��                                                                        ��
// �� Passing NULL to this function will cancel further watchdog callbacks   ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// Tom: commented out
// void MOUSE_register_watchdog_callback(int32_t (*fn)(RECT *area))
// {
//    watchdog_callback = fn;
// }

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Timer callback routine simulates protected-mode mouse event handler    ��
// �� by periodically inspecting variables maintained by real-mode handler   ��
// ��                                                                        ��
// �� The real-mode event handler shares data with this timer handler via    ��
// �� three variables defined at 0:4F0-0:4F5, an area of low memory          ��
// �� designated for interprocess communication                              ��
// ��                                                                        ��
// �� To eliminate all mouse flicker during movement, add a call to          ��
// �� VFX_wait_vblank_leading() just before the MOUSE_restore_area() call    ��
// �� below.  However, this can cause a substantial performance loss during  ��
// �� periods of rapid mouse movement, especially in EVGA.                   ��
// ��                                                                        ��
// �� Notes: This routine is called from an asynchronous interrupt handler!  ��
// ��        May also be called manually to force status update              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// LUM this function had to be changed for AIL 2 (__cdecl not __pascal and parameter user was removed)!
// it is needed, otherwise it crashes when the function is called!

void MOUSE_timer_serve()
{
   MOUSE_serve();
}

void MOUSE_serve()
{
#ifdef __HIGHC__
   FARPTR loc;
#endif

   if (locked > 0)
      return;

   ++locked;

   last_left = left;
   last_right = right;
   last_center = center;

#ifdef DPMI
   left = ((*(int16_t *)0x4f0) & 0x01) != 0;
   right = ((*(int16_t *)0x4f0) & 0x02) != 0;
   center = ((*(int16_t *)0x4f0) & 0x04) != 0;
#else
#ifdef PHARLAP
   loc.part.seg = 0x34;
   loc.part.off = 0x4f0;

   left = ((*(_Far uint8_t *)loc.ptr) & 0x01) != 0;
   right = ((*(_Far uint8_t *)loc.ptr) & 0x02) != 0;
   center = ((*(_Far uint8_t *)loc.ptr) & 0x04) != 0;
#endif
#endif

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

#ifdef DPMI
   x = (int32_t)(*(int16_t *)0x4f2) >> 3;
   y = (int32_t)(*(int16_t *)0x4f4) >> 3;
#else
#ifdef PHARLAP
   loc.part.seg = 0x34;
   loc.part.off = 0x4f2;
   x = (int32_t)(*(_Far uint16_t *)loc.ptr) >> 3;

   loc.part.seg = 0x34;
   loc.part.off = 0x4f4;
   y = (int32_t)(*(_Far uint16_t *)loc.ptr) >> 3;
#endif
#endif

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

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Wrapper for VFX_window_refresh() which automatically handles mouse     ��
// �� cursor maintenance if the mouse cursor falls within the target screen  ��
// �� coordinates                                                            ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// void MOUSE_window_refresh(WINDOW *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
// {
//    int32_t shp_x, shp_y;
//    int32_t bw, bh;
//    PANE bkgnd, client;
//    int32_t mx, my, ml, mr, mt, mb, xr;
//    uint8_t *in;

//    //
//    // If mouse hidden, just call the driver and return
//    //

//    if (hidecnt < 0)
//    {
//       VFX_window_refresh(target, x0, y0, x1, y1);
//       return;
//    }

//    //
//    // Freeze the mouse to ensure valid coordinates
//    //

//    MOUSE_hold();

//    //
//    // If mouse is currently outside the region to be refreshed, register the
//    // region as an exclusion area, release the mouse, do the refresh,
//    // cancel the exclusion area, and force an update
//    //

//    if ((saved.x0 > x1) ||
//        (saved.x1 < x0) ||
//        (saved.y1 < y0) ||
//        (saved.y0 > y1))
//    {
//       MOUSE_exclude_area(x0, y0, x1, y1);

//       MOUSE_release();

//       VFX_window_refresh(target, x0, y0, x1, y1);

//       MOUSE_exclude_area(-1, -1, -1, -1);

//       MOUSE_serve();
//       return;
//    }

//    //
//    // If this is a scaled window refresh, do a hide/refresh/show cycle and
//    // return
//    //

//    if (((x1 - x0) != target->x_max) ||
//        ((y1 - y0) != target->y_max))
//    {
//       VFX_wait_vblank_leading();
//       MOUSE_hide();
//       VFX_window_refresh(target, x0, y0, x1, y1);
//       MOUSE_show();

//       MOUSE_release();
//       MOUSE_serve();
//       return;
//    }

//    //
//    // If client window has a stencil, do a hide/refresh/show cycle if
//    // the mouse cursor overlaps a non-transparent area of the stencil
//    //

//    if (target->stencil != NULL)
//    {
//       //
//       // Find edges of visible mouse cursor relative to
//       // window being refreshed
//       //

//       ml = max(x0, saved.x0) - x0;
//       mr = min(x1, saved.x1) - x0;
//       mt = max(y0, saved.y0) - y0;
//       mb = min(y1, saved.y1) - y0;

//       //
//       // See if a non-transparent stencil packet overlaps mouse cursor
//       //

//       for (my = mt; my <= mb; my++)
//       {
//          mx = 0;

//          in = (uint8_t *)target->stencil +
//               (((uint32_t *)target->stencil)[my]);

//          while (mx <= mr)
//          {
//             xr = mx + (*in & 0x7f) - 1;

//             if (*in < 128)
//             {
//                if (!((mx > mr) || (xr < ml)))
//                {
//                   VFX_wait_vblank_leading();
//                   MOUSE_hide();
//                   VFX_window_refresh(target, x0, y0, x1, y1);
//                   MOUSE_show();

//                   MOUSE_release();
//                   MOUSE_serve();
//                   return;
//                }
//             }

//             in++;
//             mx = xr + 1;
//          }
//       }
//    }

//    //
//    // If mouse is inside the region to be refreshed, temporarily merge the
//    // pointer shape with the client's window so that it will not be erased
//    // during the refresh, do the refresh, and restore the client's window
//    // contents
//    //

//    //
//    // Define pane to describe part of client window which will be overlaid
//    // by cursor
//    //

//    client.window = target;
//    client.x0 = max(x0, saved.x0) - x0;
//    client.x1 = min(x1, saved.x1) - x0;
//    client.y0 = max(y0, saved.y0) - y0;
//    client.y1 = min(y1, saved.y1) - y0;

//    //
//    // Define pane to describe part of saved background window which
//    // lies within client screen area
//    //

//    bw = saved.x1 - saved.x0;
//    bh = saved.y1 - saved.y0;

//    bkgnd.window = &save;
//    bkgnd.x0 = (saved.x0 < x0) ? x0 - saved.x0 : 0;
//    bkgnd.x1 = (saved.x1 > x1) ? bw - (saved.x1 - x1) : bw;
//    bkgnd.y0 = (saved.y0 < y0) ? y0 - saved.y0 : 0;
//    bkgnd.y1 = (saved.y1 > y1) ? bh - (saved.y1 - y1) : bh;

//    //
//    // Update background preservation window by copying client source
//    // pane to background pane
//    //

//    VFX_pane_copy(&client, 0, 0, &bkgnd, 0, 0, NO_COLOR);

//    //
//    // Draw the mouse pointer into the client's source window, so it will
//    // appear onscreen at the correct place when the window is refreshed
//    //

//    shp_x = (x + hot_x < x0) ? x - x0 : -hot_x;
//    shp_y = (y + hot_y < y0) ? y - y0 : -hot_y;

//    VFX_shape_draw(&client, pointer_table, pointer, shp_x, shp_y);

//    //
//    // Copy client's window to screen, including the overlaid mouse pointer
//    //

//    VFX_window_refresh(target, x0, y0, x1, y1);

//    //
//    // Finally, restore the part of the client's window which we overwrote
//    // with the mouse pointer (not necessary if client doesn't intend to
//    // re-use contents of window)
//    //

//    VFX_pane_copy(&bkgnd, 0, 0, &client, 0, 0, NO_COLOR);

//    //
//    // Release the mouse and force service in case coordinates
//    // need updating
//    //

//    MOUSE_release();
//    MOUSE_serve();
// }

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Wrapper for VFX_pane_refresh() which automatically handles mouse       ��
// �� cursor maintenance if the mouse cursor falls within the target screen  ��
// �� coordinates                                                            ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// void MOUSE_pane_refresh(PANE *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
// {
//    int32_t shp_x, shp_y;
//    int32_t bw, bh;
//    PANE bkgnd, client;
//    int32_t mx, my, ml, mr, mt, mb, xr;
//    uint8_t *in;

//    //
//    // If mouse hidden, just call the driver and return
//    //

//    if (hidecnt < 0)
//    {
//       VFX_pane_refresh(target, x0, y0, x1, y1);
//       return;
//    }

//    //
//    // Freeze the mouse to ensure valid coordinates
//    //

//    MOUSE_hold();

//    //
//    // If mouse is currently outside the region to be refreshed, register the
//    // region as an exclusion area, release the mouse, do the refresh,
//    // cancel the exclusion area, and force an update
//    //

//    if ((saved.x0 > x1) ||
//        (saved.x1 < x0) ||
//        (saved.y1 < y0) ||
//        (saved.y0 > y1))
//    {
//       MOUSE_exclude_area(x0, y0, x1, y1);

//       MOUSE_release();

//       VFX_pane_refresh(target, x0, y0, x1, y1);

//       MOUSE_exclude_area(-1, -1, -1, -1);

//       MOUSE_serve();
//       return;
//    }

//    //
//    // If this is a scaled pane refresh, do a hide/refresh/show cycle and
//    // return
//    //

//    if (((x1 - x0) !=
//         (min(target->x1, target->window->x_max) - max(target->x0, 0))) ||
//        ((y1 - y0) !=
//         (min(target->y1, target->window->y_max) - max(target->y0, 0))))
//    {
//       VFX_wait_vblank_leading();
//       MOUSE_hide();
//       VFX_pane_refresh(target, x0, y0, x1, y1);
//       MOUSE_show();

//       MOUSE_release();
//       MOUSE_serve();
//       return;
//    }

//    //
//    // If client window has a stencil, do a hide/refresh/show cycle if
//    // the mouse cursor overlaps a non-transparent area of the stencil
//    //

//    if (target->window->stencil != NULL)
//    {
//       //
//       // Find edges of visible mouse cursor relative to
//       // window being refreshed
//       //

//       ml = max(x0, saved.x0) - x0 + target->x0;
//       mr = min(x1, saved.x1) - x0 + target->x0;
//       mt = max(y0, saved.y0) - y0 + target->y0;
//       mb = min(y1, saved.y1) - y0 + target->y0;

//       //
//       // See if a non-transparent stencil packet overlaps mouse cursor
//       //

//       for (my = mt; my <= mb; my++)
//       {
//          mx = 0;

//          in = (uint8_t *)target->window->stencil +
//               (((uint32_t *)target->window->stencil)[my]);

//          while (mx <= mr)
//          {
//             xr = mx + (*in & 0x7f) - 1;

//             if (*in < 128)
//             {
//                if (!((mx > mr) || (xr < ml)))
//                {
//                   VFX_wait_vblank_leading();
//                   MOUSE_hide();
//                   VFX_pane_refresh(target, x0, y0, x1, y1);
//                   MOUSE_show();

//                   MOUSE_release();
//                   MOUSE_serve();
//                   return;
//                }
//             }

//             in++;
//             mx = xr + 1;
//          }
//       }
//    }

//    //
//    // If mouse is inside the region to be refreshed, temporarily merge the
//    // pointer shape with the client's pane so that it will not be erased
//    // during the refresh, do the refresh, and restore the client's pane
//    // contents
//    //

//    //
//    // Define pane to describe part of client window which will be overlaid
//    // by cursor
//    //
//    // Set client pane coordinates relative to target pane
//    //

//    client.window = target->window;
//    client.x0 = max(x0, saved.x0) - x0 + target->x0;
//    client.x1 = min(x1, saved.x1) - x0 + target->x0;
//    client.y0 = max(y0, saved.y0) - y0 + target->y0;
//    client.y1 = min(y1, saved.y1) - y0 + target->y0;

//    //
//    // Define pane to describe part of saved background window which
//    // lies within client screen area
//    //

//    bw = saved.x1 - saved.x0;
//    bh = saved.y1 - saved.y0;

//    bkgnd.window = &save;
//    bkgnd.x0 = (saved.x0 < x0) ? x0 - saved.x0 : 0;
//    bkgnd.x1 = (saved.x1 > x1) ? bw - (saved.x1 - x1) : bw;
//    bkgnd.y0 = (saved.y0 < y0) ? y0 - saved.y0 : 0;
//    bkgnd.y1 = (saved.y1 > y1) ? bh - (saved.y1 - y1) : bh;

//    //
//    // Update background preservation window by copying client source
//    // pane to background pane
//    //

//    VFX_pane_copy(&client, 0, 0, &bkgnd, 0, 0, NO_COLOR);

//    //
//    // Draw the mouse pointer into the client's source window, so it will
//    // appear onscreen at the correct place when the window is refreshed
//    //

//    shp_x = (x + hot_x < x0) ? x - x0 : -hot_x;
//    shp_y = (y + hot_y < y0) ? y - y0 : -hot_y;

//    VFX_shape_draw(&client, pointer_table, pointer, shp_x, shp_y);

//    //
//    // Copy client's pane to screen, including the overlaid mouse pointer
//    //

//    VFX_pane_refresh(target, x0, y0, x1, y1);

//    //
//    // Finally, restore the part of the client's window which we overwrote
//    // with the mouse pointer (not necessary if client doesn't intend to
//    // re-use contents of window)
//    //

//    VFX_pane_copy(&bkgnd, 0, 0, &client, 0, 0, NO_COLOR);

//    //
//    // Release the mouse and force service in case coordinates
//    // need updating
//    //

//    MOUSE_release();
//    MOUSE_serve();
// }

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Perform MOUSE_pane_refresh() calls for all entries in a PANE_LIST      ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// void MOUSE_pane_list_refresh(PANE_LIST *list)
// {
//    uint32_t i;
//    PANE *a;

//    for (i = 0; i < list->size; i++)
//    {
//       if (list->flags[i] == PL_VALID)
//       {
//          a = &list->array[i];

//          MOUSE_pane_refresh(a, a->x0, a->y0, a->x1, a->y1);
//       }
//    }
// }

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Initialize mouse system                                                ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

int32_t MOUSE_init(int32_t xsize, int32_t ysize, int32_t background)
{
   //    union REGS inregs, outregs;

   //    //
   //    // Initialize mouse system variables: by default, mouse is unlocked,
   //    // released, and hidden, with no valid pointer shape table and no exclusion
   //    // area
   //    //

   //    locked = 0;
   //    held = 0;
   //    hidecnt = -1;
   //    pointer_table = NULL;
   //    excluded = 0;

   //    MOUSE_exclude_area(-1, -1, -1, -1);

   //    MOUSE_event_callback = NULL;
   //    button_event_callback = NULL;
   //    watchdog_callback = NULL;

   //    x = y = left = right = center = -1;

   //    scrn_max_x = xsize - 1;
   //    scrn_max_y = ysize - 1;

   // #ifdef DPMI

   //    //
   //    // Init mouse driver, returning 0 on failure
   //    //

   //    inregs.x.eax = 0;
   //    int386(0x33, &inregs, &outregs);

   //    if (outregs.w.ax != 0xffff)
   //       return 0;

   //    //
   //    // Set horizontal and vertical limits for mouse movement
   //    // Multiply limits by 8 to obtain single-pixel resolution
   //    //

   //    inregs.x.eax = 7;
   //    inregs.x.ecx = 0;
   //    inregs.x.edx = scrn_max_x << 3;
   //    int386(0x33, &inregs, &outregs);

   //    inregs.x.eax = 8;
   //    inregs.x.ecx = 0;
   //    inregs.x.edx = scrn_max_y << 3;
   //    int386(0x33, &inregs, &outregs);

   //    //
   //    // Set mouse movement rate = 1 mickey/pixel
   //    //

   //    inregs.x.eax = 0x0f;
   //    inregs.x.ecx = 1;
   //    inregs.x.edx = 1;
   //    int386(0x33, &inregs, &outregs);

   // #else
   // #ifdef __HIGHC__

   //    //
   //    // Init mouse driver, returning 0 on failure
   //    //

   //    inregs.w.eax = 0;
   //    int86(0x33, &inregs, &outregs);

   //    if (outregs.x.ax != 0xffff)
   //       return 0;

   //    //
   //    // Set horizontal and vertical limits for mouse movement
   //    // Multiply limits by 8 to obtain single-pixel resolution
   //    //

   //    inregs.w.eax = 7;
   //    inregs.w.ecx = 0;
   //    inregs.w.edx = scrn_max_x << 3;
   //    int86(0x33, &inregs, &outregs);

   //    inregs.w.eax = 8;
   //    inregs.w.ecx = 0;
   //    inregs.w.edx = scrn_max_y << 3;
   //    int86(0x33, &inregs, &outregs);

   //    //
   //    // Set mouse movement rate = 1 mickey/pixel
   //    //

   //    inregs.w.eax = 0x0f;
   //    inregs.w.ecx = 1;
   //    inregs.w.edx = 1;
   //    int86(0x33, &inregs, &outregs);

   // #endif
   // #endif

   //    if (!MOUSE_install_handler())
   //       return 0;

   //    //
   //    // Initialize windows used by pointer rendering
   //    //

   //    work.buffer = malloc(MAX_MOUSE_BUFFER_SIZE);
   //    save.buffer = malloc(MAX_MOUSE_BUFFER_SIZE);

   //    work.stencil = work.shadow = NULL;
   //    save.stencil = save.shadow = NULL;

   //    workp.x0 = 0;
   //    workp.y0 = 0;

   //    workp.window = &work;

   //    //
   //    // Force service to validate mouse status variables
   //    //

   //    MOUSE_force_move(xsize / 2, ysize / 2);

   //    //
   //    // Arrange for periodic background mouse service
   //    //
   //    // The mouse service rate should equal the vertical refresh period
   //    // for smoothest cursor movement.  In 200-line mode, we assume a period
   //    // of 70 Hz; otherwise, a 60 Hz timer rate is used.
   //    //
   //    // If AIL/32 timer services are unavailable, the application must arrange
   //    // for another source of periodic service.  The MOUSE_serve() routine may
   //    // either be polled, or called from an interrupt handler.  To eliminate
   //    // all mouse flicker, call MOUSE_serve() from an interrupt handler synced
   //    // to the actual vertical retrace timing signal.
   //    //

   //    timer = -1;

   //    if (background)
   //    {
   //       timer = AIL_register_timer(MOUSE_timer_serve);

   //       if (timer != -1)
   //       {
   //          if (ysize >= 400)
   //             AIL_set_timer_frequency(timer, 60);
   //          else
   //             AIL_set_timer_frequency(timer, 70);

   //          AIL_start_timer(timer);
   //       }
   //    }

   //    // LUM added
   mouse_active = 1;

   return 1;
}

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Shut down mouse system                                                 ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

void MOUSE_shutdown(void)
{
   //    union REGS inregs, outregs;

   //    // LUM added
   //    mouse_active = 0;

   //    MOUSE_hide();

   //    if (timer != -1)
   //    {
   //       AIL_stop_timer(timer);
   //       AIL_release_timer_handle(timer);
   //    }

   //    MOUSE_remove_handler();

   //    //
   //    // Free pointer background buffer memory
   //    //

   //    free(save.buffer);
   //    free(work.buffer);

   //    //
   //    // Free real-mode memory used by event handler stub
   //    //

   // #ifdef DPMI
   //    inregs.x.eax = 0x101;
   //    inregs.x.edx = real_event_sel;
   //    int386(0x31, &inregs, &outregs);

   // #else
   // #ifdef PHARLAP

   //    inregs.w.eax = 0x25c1;
   //    inregs.w.ecx = real_event_seg;
   //    int86(0x21, &inregs, &outregs);

   // #endif
   // #endif
}

#ifdef DEMO

// ����������������������������������������������������������������������������
// ��                                                                        ��
// �� Exercise mouse system (compiled only if DEMO defined)                  ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

int32_t volatile local_X;
int32_t volatile local_Y;
int32_t volatile local_left;
int32_t volatile local_right;

//
// Callback functions for simulated mouse event handler
//
// Note: These functions are called from within an AIL/32 timer
// interrupt handler
//

void MOUSE_event_fn(int32_t x, int32_t y)
{
   local_X = x;
   local_Y = y;
}

void button_event_fn(int32_t l, int32_t r, int32_t c)
{
   c++; // avoid warning
   local_left = l;
   local_right = r;
}

void main(int argc, char *argv[])
{
   VFX_DESC *VFX;
   int32_t w, h;
   void *DLL, *drvr;

   WINDOW window, twnd;
   PANE pane, text;

   void *shapes;
   FONT *font;
   int32_t i;

   static uint8_t colors[256];
   static int8_t strbuf[256];

   //
   // Get name of driver to use
   //

   if (argc != 2)
   {
      printf("\nUsage example: MOUSE drv\\vesa480.dll\n");
      exit(1);
   }

   //
   // Load the driver DLL
   //

   DLL = FILE_read(argv[1], NULL);

   if (DLL == NULL)
   {
      printf("Missing or invalid 386FX driver\n");
      exit(1);
   }

   drvr = DLL_load(DLL, DLLMEM_ALLOC | DLLSRC_MEM, NULL);

   if (drvr == NULL)
   {
      printf("Invalid DLL image\n");
      exit(1);
   }

   free(DLL);

   //
   // Register the driver with the API
   //

   VFX_register_driver(drvr);

   VFX = VFX_describe_driver();

   w = VFX->scrn_width;
   h = VFX->scrn_height;

   printf("\n  Driver name: %s\n", argv[1]);
   printf(" Screen width: %d\n", w);
   printf("Screen height: %d\n", h);
   getch();

   //
   // Turn graphics on; set up window and pane
   //

   VFX_init_driver();

   window.buffer = calloc(w * h, 1);
   window.stencil = window.shadow = NULL;
   pane.x0 = 0;
   pane.y0 = 0;
   pane.x1 = window.x_max = w - 1;
   pane.y1 = window.y_max = h - 1;
   pane.window = &window;

   for (i = 0; i < h / 4; i++)
   {
      VFX_line_draw(&pane, i, i, w - 1 - i, i, LD_DRAW, i);
      VFX_line_draw(&pane, i, i, i, h - 1 - i, LD_DRAW, i);

      VFX_line_draw(&pane, i, h - 1 - i, w - 1 - i, h - 1 - i, LD_DRAW, i);
      VFX_line_draw(&pane, w - 1 - i, i, w - 1 - i, h - 1 - i, LD_DRAW, i);
   }

   VFX_window_refresh(&window, 0, 0, w - 1, h - 1);

   //
   // Init AIL (for timer services)
   //

   AIL_startup();

   //
   // Init mouse system
   //

   MOUSE_init(w, h, 1);

   shapes = FILE_read("mouse.shp", NULL);
   MOUSE_set_pointer(shapes, 0);

   MOUSE_register_mouse_event_callback(MOUSE_event_fn);
   MOUSE_register_button_event_callback(button_event_fn);

   MOUSE_show();

   //
   // Test MOUSE_refresh_window()
   //

   font = FILE_read("demo.fnt", NULL);
   for (x = 0; x < 256; x++)
      colors[x] = x;
   colors[font->font_background] = 15;

   text.window = &twnd;
   text.x0 = 0;
   text.y0 = 0;
   text.x1 = w - 1 - (i - 1) - (i - 1);
   text.y1 = h - 1 - (i - 1) - (i - 1);

   twnd.stencil = twnd.shadow = NULL;
   twnd.x_max = w - 1 - (i - 1) - (i - 1);
   twnd.y_max = h - 1 - (i - 1) - (i - 1);
   twnd.buffer = calloc((twnd.x_max + 1) * (twnd.y_max + 1), 1);

   VFX_pane_wipe(&text, 15);

   //
   // Demonstrate mouse system
   //

   while (!kbhit())
   {
      colors[1] = (++colors[1]) & 0x0f;

      VFX_pane_scroll(&text, 0, -(font->char_height), PS_NOWRAP, NO_COLOR);

      sprintf(strbuf, "(%d,%d) (%d,%d)           ",
              local_X, local_Y, local_left, local_right);

      VFX_string_draw(&text, 4, text.y1 - font->char_height + 1, font, strbuf, colors);

      MOUSE_window_refresh(&twnd, i - 1, i - 1, w - 1 - (i - 1), h - 1 - (i - 1));
   }
   getch();

   //
   // Shut everything down
   //

   MOUSE_shutdown();
   VFX_shutdown_driver();
   AIL_shutdown();
}

#endif // #ifdef DEMO
