//
//  VIDEO.H - SDL2 video layer for the AESOP/32 port
//
//  Stands in for the original VFX video device driver. All SDL window,
//  renderer and texture lifecycle, event pumping, frame presentation
//  and frame pacing live here so the interpreter (rt.c) stays
//  video-agnostic, as RT.ASM was in the original.
//

#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include <SDL2/SDL.h>

#include "vfx.h" // RGB / WINDOW / PANE types + driver pointer signatures

extern int32_t SCREEN_WIDTH;
extern int32_t SCREEN_HEIGHT;

extern SDL_Window *sdl_window;
extern SDL_Renderer *sdl_renderer;
extern SDL_Texture *sdl_texture;

//
// The "video RAM": a 320x200 8-bit palettized framebuffer. All VFX
// drawing targets ultimately refresh into this buffer, and
// video_present() converts it through the DAC palette to the display
// texture. GIL2VFX_init() points windows[0].buffer (the original VGA
// segment 0xA0000) at it.
//
extern uint8_t *video_screen_buffer;

//
// The current DAC palette, as 8-bit RGB components
//
extern RGB video_dac[256];

// Set by video_poll_events() when the user requests shutdown
// (window close or ESC).
extern volatile int video_quit;

// Create the window, renderer and streaming texture, and show an
// initial black frame. Returns 0 on success, -1 on failure (SDL state
// is cleaned up before returning).
int32_t video_init(const char *title, int32_t width, int32_t height);

// Destroy the texture/renderer/window and shut SDL down. Safe to call
// even if video_init() failed.
void video_shutdown(void);

// Pump the SDL event queue; sets video_quit on window close or ESC.
void video_poll_events(void);

// Copy the texture to the screen.
void video_present(void);

// Host yield hook for the interpreter loop: pump events (rate-limited
// internally) and return nonzero if execution should abort. Suitable
// for assigning to RT_yield.
int32_t video_yield(void);

// VFX driver stand-ins: assign to VFX_wait_vblank /
// VFX_wait_vblank_leading. These pace execution to the 60Hz frame
// cadence, exactly where the DOS build throttled itself.
void video_wait_vblank(void);
void video_wait_vblank_leading(void);

//
// Driver implementations for the remaining VFX function pointers
// (assigned in video_init). Contracts derived from the original
// MOUSE.C call sites:
//
//   VFX_window_refresh(src, x0,y0,x1,y1) — blit src->buffer from local
//     (0,0) (stride src->x_max+1) to the screen at (x0,y0), sized by
//     the rect. A window whose buffer IS the screen buffer refreshes
//     as a no-op.
//   VFX_window_read(dest, x0,y0,x1,y1) — copy the screen rect into
//     dest->buffer at local (0,0), rows packed at dest->x_max+1.
//   VFX_pane_refresh(target, x0,y0,x1,y1) — blit target's window
//     buffer, pane-local coords (rect minus the pane origin), to the
//     screen at (x0,y0).
//   VFX_area_wipe(x0,y0,x1,y1,color) — fill the screen rect.
//   VFX_DAC_write/read — palette; write takes raw 6-bit VGA components
//     (see set_palette()) and scales to 8-bit, as the hardware did.
//
void video_window_refresh(WINDOW *src, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
void video_window_read(WINDOW *dest, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
void video_pane_refresh(PANE *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
void video_area_wipe(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color);
void video_DAC_write(int32_t color_number, RGB *triplet);
void video_DAC_read(int32_t color_number, RGB *triplet);

//
// Pointer state, recorded by the event pump. This is the SDL stand-in
// for the DOS real-mode mouse callback stashing button/X/Y words in
// the BIOS Data Area at 0x4F0: MOUSE_serve() polls it and fires the
// game's event callbacks. Coordinates are logical pixels
// (0..SCREEN_WIDTH-1 etc.); buttons: bit0 = left, bit1 = right,
// bit2 = center.
//
extern volatile int32_t video_mouse_x;
extern volatile int32_t video_mouse_y;
extern volatile int32_t video_mouse_buttons;

// Warp the pointer and update the recorded state (MOUSE_force_move).
void video_warp_mouse(int32_t x, int32_t y);

//
// Keyboard ring buffer — the stand-in for the BIOS keyboard buffer
// (0x41A/0x41C/0x41E). Keys are DOS-style words: ASCII in the low
// byte, or set-1 scan code in the high byte (low byte 0) for extended
// keys.
//
int32_t video_key_pending(void);
uint16_t video_key_read(void);

#endif
