// ############################################################################
// ##                                                                        ##
// ##  INTRFACE.C                                                            ##
// ##                                                                        ##
// ##  AESOP user interface code resource handlers for Eye III engine        ##
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

extern VFX_DESC *VFX;

#include "defs.h"
#include "shared.h"
#include "rtsystem.h"
#include "rtmsg.h"
#include "rtres.h"
#include "rtlink.h"
#include "rtcode.h"
#include "rt.h"
#include "intrface.h"
#include "event.h"
#include "graphics.h"

int32_t interface_active = 0;

int32_t htimer;
uint32_t volatile heartbeat;
uint32_t volatile in_BIOS;

uint32_t cur_table;
uint32_t cur_number;
int32_t cur_hot_X;
int32_t cur_hot_Y;

uint32_t wait_number;
int32_t wait_hot_X;
int32_t wait_hot_Y;

uint32_t save_number;
int32_t save_hot_X;
int32_t save_hot_Y;

int32_t ptr_valid;
int32_t wait_ptr_valid;
int32_t wait_ptr_state;

uint32_t pointer_set;
uint32_t pointer_set_entry;
uint32_t pointer_num;
uint32_t pointer_fade_table;
uint32_t pointer_fade_level;
uint32_t pointer_scale;
int32_t last_cursor_X, last_cursor_Y;
int32_t volatile point_X, point_Y;
int32_t volatile btn_left, btn_right;
int32_t volatile last_X, last_Y;
int32_t volatile last_left, last_right;

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
} DPMI_RMI;

/*********************************************************/
void getkey(void)
{
   printf("[intrface] getkey\n");

   while (!find_event(SYS_KEYDOWN, -1L))
      ;

   remove_event(SYS_KEYDOWN, -1L, -1);
}

/*********************************************************/
void add_region_event(int32_t type, int32_t owner)
{
   int16_t nxt;
   uint32_t r;
   NREQ *NR;

   printf("[intrface] add_region_event: type=%i owner=%i\n", type, owner);

   DISABLE();

   nxt = NR_first[type];
   while (nxt != -1)
   {
      NR = &NR_list[nxt];
      nxt = NR->next;
      r = NR->parameter;

      if (mouse_in_window(0, r))
         add_event(type, r, owner);
   }

   ENABLE();
}

/*********************************************************/
//
// AIL timer callback routine; check the keyboard buffer
// for incoming characters 60 times per second, and post SYS_TIMER
// events to the event queue 30 times per second
//
// If a SYS_TIMER message is already in the queue, update its
// "heartbeat" parameter instead of posting a new message
//
// Don't update system heartbeat count while hourglass cursor is
// active
//
// If the keyboard buffer's head and tail pointers do not match,
// at least one character has been typed in the last 1/60 second.  Read
// the scan code and ASCII/extended code directly from the buffer, post
// a SYS_KEYDOWN event, and flush the buffer by manually equating its
// head and tail pointers
//
// Avoid manipulating keyboard buffer during execution of INT 9 handler
// (possible since IRQ 0/INT 8 has higher priority than IRQ 1/INT 9)
//
/*********************************************************/

static void timer_callback(void) // Warning: called during IRQ 0
{
   EVENT *EV;
   unsigned key, scan, ascii;
   static uint16_t *head = (uint16_t *)0x41aL;
   static uint16_t *tail = (uint16_t *)0x41cL;
   static uint16_t *buffer = (uint16_t *)0x41eL;
   (void)scan; // Tom: added, not used?

   printf("[intrface] timer_callback\n");

   if (ENABLED <= 0)
      return;

   if (wait_ptr_state <= 0)
   {
      ++heartbeat;

      if (!(heartbeat & 1L))
      {
         if ((EV = find_event(SYS_TIMER, -1)) == NULL)
            add_event(SYS_TIMER, heartbeat >> 1, -1);
         else
            EV->parameter = heartbeat >> 1;
      }
   }

   if ((*head != *tail) && (!in_BIOS))
   {
      key = buffer[((*head) - 0x1e) / 2];
      *head = *tail;

      ascii = key & 0xff;
      scan = key >> 8;

      if ((key == KP_5) || (ascii == 0) || (ascii == 0xe0))
         add_event(SYS_KEYDOWN, key, -1);
      else
         add_event(SYS_KEYDOWN, ascii, -1);
   }
}

/*********************************************************/
static void mouse_event_handler(int32_t px, int32_t py)
{
   static int32_t entry = 0;
   int16_t nxt;
   int32_t r;
   NREQ *NR;
   EVENT *EV;

   printf("[intrface] mouse_event_handler: px=%i py=%i\n", px, py);

   if (entry)
      return;
   entry = 1;

   point_X = px;
   point_Y = py;

   if ((EV = find_event(SYS_MOUSEMOVE, -1L)) == NULL)
      add_event(SYS_MOUSEMOVE, ((uint32_t)point_Y << 16) | (uint32_t)point_X, -1);
   else
      EV->parameter = ((uint32_t)point_Y << 16) | (uint32_t)point_X;

   nxt = NR_first[SYS_ENTER_REGION];
   while (nxt != -1)
   {
      NR = &NR_list[nxt];
      nxt = NR->next;
      r = NR->parameter;

      if (mouse_in_window(0, r))
      {
         if (NR->status & NSX_IN_REGION)
            continue;

         NR->status |= NSX_IN_REGION;

         add_event(SYS_ENTER_REGION, r, -1);
      }
      else
         NR->status &= (~NSX_IN_REGION);
   }

   nxt = NR_first[SYS_LEAVE_REGION];
   while (nxt != -1)
   {
      NR = &NR_list[nxt];
      nxt = NR->next;
      r = NR->parameter;

      if (!mouse_in_window(0, r))
      {
         if (!(NR->status & NSX_OUT_REGION))
            continue;

         NR->status &= (~NSX_OUT_REGION);

         add_event(SYS_LEAVE_REGION, r, -1);
      }
      else
         NR->status |= NSX_OUT_REGION;
   }

   entry = 0;
}
/*********************************************************/
static void mouse_button_event_handler(int32_t left, int32_t right, int32_t center)
{
   static int32_t entry = 0;
   (void)center; // Tom: added

   printf("[intrface] mouse_button_event_handler: left=%i right=%i center=%i\n", left, right, center);

   if (entry)
      return;
   entry = 1;

   btn_left = left;
   btn_right = right;

   if (btn_left != last_left)
   {
      add_event(btn_left ? SYS_CLICK : SYS_RELEASE, 0, -1);
      add_event(btn_left ? SYS_LEFT_CLICK : SYS_LEFT_RELEASE, 0, -1);
      add_region_event(btn_left ? SYS_LEFT_CLICK_REGION : SYS_LEFT_RELEASE_REGION, -1);
      add_region_event(btn_left ? SYS_CLICK_REGION : SYS_RELEASE_REGION, -1);

      last_left = btn_left;
   }

   if (btn_right != last_right)
   {
      add_event(btn_right ? SYS_CLICK : SYS_RELEASE, 0, -1);
      add_event(btn_right ? SYS_RIGHT_CLICK : SYS_RIGHT_RELEASE, 0, -1);
      add_region_event(btn_right ? SYS_RIGHT_CLICK_REGION : SYS_RIGHT_RELEASE_REGION, -1);
      add_region_event(btn_right ? SYS_CLICK_REGION : SYS_RELEASE_REGION, -1);

      last_right = btn_right;
   }

   entry = 0;
}

/*********************************************************/
void init_interface(void) // Tom: TODO
{
   // Tom: added new version

   printf("[intrface] init_interface\n");

   in_BIOS = 0;
   heartbeat = 0L;

   pointer_set = UINT32_MAX;
   pointer_set_entry = UINT32_MAX;
   ptr_valid = 0;
   wait_ptr_valid = 0;
   wait_ptr_state = 0;

   // LUM the parameters have changed ("background" added)
   // MOUSE_init(VFX->scrn_width, VFX->scrn_height, 1); // Tom: original version
   MOUSE_init(320, 200, 1); // Tom: hardcoded version

   MOUSE_register_mouse_event_callback(mouse_event_handler);
   MOUSE_register_button_event_callback(mouse_button_event_handler);

   point_X = last_X = last_cursor_X = 0;
   point_Y = last_Y = last_cursor_Y = 0;
   btn_left = last_left = 0;
   btn_right = last_right = 0;

   // htimer = AIL_register_timer(timer_callback); // Tom: commented out
   // AIL_set_timer_frequency(htimer, 60); // Tom: commented out
   // AIL_start_timer(htimer); // Tom: commented out

   htimer = 0; // Tom: hardcoded
   interface_active = 1;
}

/*********************************************************/
void shutdown_interface(void) // Tom: TODO
{
   printf("[intrface] shutdown_interface\n");

   if (!interface_active)
      return;
   interface_active = 0;

   hide_mouse();

   MOUSE_shutdown();

   if (pointer_set != UINT32_MAX)
   {
      RTR_unlock(pointer_set);
   }
}

/*********************************************************/
void set_mouse_pointer(int32_t argcnt, uint32_t table, uint32_t number, int32_t hot_X, int32_t hot_Y, uint32_t scale, uint32_t fade_table, uint32_t fade_level)
{
   ND_entry *entry;

   printf("[intrface] set_mouse_pointer: argcnt=%i table=%u number=%u hot_X=%i hot_Y=%i scale=%u fade_table=%u fade_level=%u\n", argcnt, table, number, hot_X, hot_Y, scale, fade_table, fade_level);

   if ((wait_ptr_state != 0) && (argcnt != 0))
   {
      save_number = number;
      save_hot_X = hot_X;
      save_hot_Y = hot_Y;

      return;
   }

   cur_table = table;
   cur_number = number;
   cur_hot_X = hot_X;
   cur_hot_Y = hot_Y;

   if ((table == pointer_set_entry) &&
       (number == pointer_num) &&
       (scale == pointer_scale) &&
       (fade_table == pointer_fade_table) &&
       (fade_level == pointer_fade_level))
      return;

   if (table != pointer_set_entry)
   {
      if (pointer_set != -1U)
      {
         RTR_unlock(pointer_set);
      }

      if ((entry = RTR_search_name_dir(RTR, table)) == NULL)
         pointer_set = RTR_get_resource_handle(RTR, table, DA_DEFAULT);
      else
         pointer_set = entry->handle;

      RTR_lock(RTR, pointer_set);

      pointer_set_entry = table;
   }

   pointer_num = number;
   pointer_scale = scale;
   pointer_fade_table = fade_table;
   pointer_fade_level = fade_level;

   ptr_valid = 1;

   // LUM the parameters have changed
   MOUSE_set_pointer(RTR_addr(pointer_set), pointer_num); //,hot_X,hot_Y);
}

/*********************************************************/
void set_wait_pointer(int32_t argcnt, uint32_t number, int32_t hot_X, int32_t hot_Y)
{
   (void)argcnt; // Tom: added

   printf("[intrface] set_wait_pointer: argcnt=%i number=%u hot_X=%i hot_Y=%i\n", argcnt, number, hot_X, hot_Y);

   if (number == UINT32_MAX)
   {
      wait_ptr_valid = 0;
      return;
   }

   wait_number = number;
   wait_hot_X = hot_X;
   wait_hot_Y = hot_Y;

   wait_ptr_valid = 1;
}

/*********************************************************/
void standby_cursor(void)
{
   printf("[intrface] standby_cursor\n");

   if (!wait_ptr_valid)
      return;
   if (!ptr_valid)
      return;

   ++wait_ptr_state;

   if (wait_ptr_state == 1)
   {
      save_number = cur_number;
      save_hot_X = cur_hot_X;
      save_hot_Y = cur_hot_Y;

      set_mouse_pointer(0, cur_table, wait_number, wait_hot_X, wait_hot_Y, pointer_scale, pointer_fade_table, pointer_fade_level);
   }
}

/*********************************************************/
//
// Turn off hourglass cursor
//
/*********************************************************/

void resume_cursor(void)
{
   printf("[intrface] resume_cursor\n");

   if (!wait_ptr_valid)
      return;
   if (!ptr_valid)
      return;

   --wait_ptr_state;

   if (wait_ptr_state == 0)
      set_mouse_pointer(0, cur_table, save_number, save_hot_X, save_hot_Y, pointer_scale, pointer_fade_table, pointer_fade_level);
}

/*********************************************************/
//
// Disable mouse tracking
//
/*********************************************************/

void lock_mouse(void)
{
   printf("[intrface] lock_mouse\n");

   MOUSE_lock();
}

/*********************************************************/
//
// Enable normal mouse tracking
//
// Do a hide/show cycle in case the mouse was moved while locked
//
/*********************************************************/

void unlock_mouse(void)
{
   printf("[intrface] unlock_mouse\n");

   MOUSE_unlock();
}

/*********************************************************/
void show_mouse(void)
{
   printf("[intrface] show_mouse\n");

   MOUSE_show();
}

/*********************************************************/
void hide_mouse(void)
{
   printf("[intrface] hide_mouse\n");

   MOUSE_hide();
}

/*********************************************************/
uint32_t mouse_XY(void)
{
   uint32_t xy;

   DISABLE();

   xy = ((uint32_t)point_Y << 16) + point_X;

   ENABLE();

   printf("[intrface] mouse_XY: return=%u\n", xy);

   return xy;
}

/*********************************************************/
//
// Warning: re-entrant
//
/*********************************************************/

uint32_t mouse_in_window(int32_t argcnt, uint32_t wnd)
{
   uint32_t stat;

   printf("[STUB] [intrface] mouse_in_window: argcnt=%i wnd=%u\n", argcnt, wnd);

   // Tom: commented out, stubbed version above
   // stat = ((point_X >= GIL2VFX_get_x1(wnd)) &&
   //         (point_X <= GIL2VFX_get_x2(wnd)) &&
   //         (point_Y >= GIL2VFX_get_y1(wnd)) &&
   //         (point_Y <= GIL2VFX_get_y2(wnd)));

   stat = 0; // Tom: added, stubbed version

   return stat;
}

/*********************************************************/
//
// Wrapper for GIL_copy_window() which handles cursor save/restore
//
// Target must be a subwindow of PAGE1
//
/*********************************************************/

void refresh_window(int32_t argcnt, uint32_t src, uint32_t target)
{
   (void)argcnt; // Tom: added
   (void)src;    // Tom: added
   (void)target; // Tom: added

   printf("[STUB] [intrface] refresh_window: argcnt=%i src=%u target=%u\n", argcnt, src, target);

   // GIL2VFX_refresh_window(src, target); // Tom: commented out
}

void intrface_entry()
{
   printf("[intrface] intrface_entry\n");

   // wvideo
}
