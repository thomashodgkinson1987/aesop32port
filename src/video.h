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

extern int32_t SCREEN_WIDTH;
extern int32_t SCREEN_HEIGHT;

extern SDL_Window *sdl_window;
extern SDL_Renderer *sdl_renderer;
extern SDL_Texture *sdl_texture;

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

#endif
