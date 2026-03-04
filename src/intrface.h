//
//  Run-time interface subs
//

#ifndef INTRFACE_H
#define INTRFACE_H

#include <stdint.h>

extern int32_t wait_ptr_state;

void init_interface(void);
void shutdown_interface(void);
void standby_cursor(void);
void resume_cursor(void);
void lock_mouse(void);
void unlock_mouse(void);
void show_mouse(void);
void hide_mouse(void);
// void copy_mouse(int window); // Tom: commented out, not used?
uint32_t mouse_in_window(int32_t argcnt, uint32_t wnd);
void refresh_window(int32_t argcnt, uint32_t src, uint32_t target);

#endif
