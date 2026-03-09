// ############################################################################
// ##                                                                        ##
// ##  MOUSE.H: C type definitions & mouse API prototypes                    ##
// ##                                                                        ##
// ##  Source compatible with 32-bit 80386 C/C++                             ##
// ##                                                                        ##
// ##  V1.00 of  9-Jul-93: Initial release                                   ##
// ##   1.10 of 24-Jan-94: Added MOUSE_pane_refresh()                        ##
// ##   1.11 of 15-Feb-94: Upper-case MOUSE_ function names                  ##
// ##                                                                        ##
// ##  Project: 386FX Sound & Light(TM)                                      ##
// ##   Author: John Miles                                                   ##
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

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#include "vfx.h"

//
// MAX_MOUSE_BUFFER_SIZE should be set to the size of the visible area
// in bytes of the largest mouse cursor to be used
//

#define MAX_MOUSE_BUFFER_SIZE 16384

extern int32_t MOUSE_init(int32_t xsize, int32_t ysize, int32_t background);
extern void MOUSE_shutdown(void);

extern void MOUSE_show(void);
extern void MOUSE_hide(void);
extern void MOUSE_set_pointer(void *table, int32_t shape);
extern void MOUSE_status(int32_t *mx, int32_t *my, int32_t *ml, int32_t *mr, int32_t *mc);
extern void MOUSE_force_move(int32_t new_x, int32_t new_y);

extern void MOUSE_register_mouse_event_callback(void (*fn)(int32_t x, int32_t y));
extern void MOUSE_register_button_event_callback(void (*fn)(int32_t left, int32_t right, int32_t center));
extern void MOUSE_register_watchdog_callback(int32_t (*fn)(RECT *area));

extern void MOUSE_lock(void);
extern void MOUSE_unlock(void);
extern void MOUSE_hold(void);
extern void MOUSE_release(void);

extern int32_t MOUSE_visible_area(RECT *area);
extern int32_t MOUSE_shape_in_area(RECT *area);

// extern void __cdecl MOUSE_serve(void); // Tom: commented out, new version below
extern void MOUSE_serve(void);

extern void MOUSE_window_refresh(WINDOW *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
extern void MOUSE_pane_refresh(PANE *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1);

extern void MOUSE_pane_list_refresh(PANE_LIST *list);

#endif
