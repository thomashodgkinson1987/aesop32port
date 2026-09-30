//
//  VIDEO.C - SDL2 video layer for the AESOP/32 port
//
//  Plays the role of the original VFX video device driver. The DOS
//  build throttled itself against the VGA vblank interrupt at
//  VFX_wait_vblank_leading(); video_wait_vblank_leading() reproduces
//  that here with a fixed frame cadence, so the interpreter can run
//  at full speed and frames appear when the game asks for them.
//

#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "video.h"

int32_t SCREEN_WIDTH = 0;
int32_t SCREEN_HEIGHT = 0;

SDL_Window *sdl_window = NULL;
SDL_Renderer *sdl_renderer = NULL;
SDL_Texture *sdl_texture = NULL;

volatile int video_quit = 0;

//
// Pointer state (see video.h)
//

volatile int32_t video_mouse_x = 0;
volatile int32_t video_mouse_y = 0;
volatile int32_t video_mouse_buttons = 0;

//
// Keyboard ring buffer (see video.h)
//

#define VIDEO_KEY_QUEUE_SIZE 16

static uint16_t key_queue[VIDEO_KEY_QUEUE_SIZE];
static uint32_t key_head = 0;
static uint32_t key_tail = 0;

//
// Frame cadence, matching the 60Hz VGA refresh the DOS build synced to
//

#define VIDEO_FRAME_RATE 60
#define VIDEO_FRAME_MS (1000 / VIDEO_FRAME_RATE)

//
// Minimum interval between event pumps from the interpreter yield hook
//

#define VIDEO_POLL_MIN_MS 5

static uint32_t next_frame_tick = 0;
static uint32_t last_poll_tick = 0;

//
// Map an SDL keycode to a DOS-style key word (ASCII in the low byte,
// or set-1 scan code in the high byte for extended keys). Returns 0
// for keys the game never sees (modifiers etc.).
//
// The original posted whatever the BIOS keyboard buffer contained,
// with extended keys (arrows, F-keys) identified by a zero ASCII byte.
// Only a minimal subset is mapped so far — enough for menus.
//

static uint16_t keysym_to_key(SDL_Keycode sym)
{
   switch (sym)
   {
   case SDLK_UP:       return 0x4800;
   case SDLK_DOWN:     return 0x5000;
   case SDLK_LEFT:     return 0x4B00;
   case SDLK_RIGHT:    return 0x4D00;
   case SDLK_HOME:     return 0x4700;
   case SDLK_END:      return 0x4F00;
   case SDLK_PAGEUP:   return 0x4900;
   case SDLK_PAGEDOWN: return 0x5100;
   case SDLK_INSERT:   return 0x5200;
   case SDLK_DELETE:   return 0x5300;
   case SDLK_F1:       return 0x3B00;
   case SDLK_F2:       return 0x3C00;
   case SDLK_F3:       return 0x3D00;
   case SDLK_F4:       return 0x3E00;
   case SDLK_F5:       return 0x3F00;
   case SDLK_F6:       return 0x4000;
   case SDLK_F7:       return 0x4100;
   case SDLK_F8:       return 0x4200;
   case SDLK_F9:       return 0x4300;
   case SDLK_F10:      return 0x4400;
   default:
      break;
   }

   if (sym >= 32 && sym <= 126)
   {
      if (sym >= 'a' && sym <= 'z' && (SDL_GetModState() & KMOD_SHIFT))
         sym -= 32;

      return (uint16_t)sym;
   }

   return 0;
}

static void video_key_push(const SDL_KeyboardEvent *key)
{
   uint16_t key_word = keysym_to_key(key->keysym.sym);

   if (key_word == 0)
      return;

   // Buffer full: drop the key (the BIOS beeped instead)
   if ((key_tail + 1) % VIDEO_KEY_QUEUE_SIZE == key_head)
      return;

   key_queue[key_tail] = key_word;
   key_tail = (key_tail + 1) % VIDEO_KEY_QUEUE_SIZE;
}

int32_t video_init(const char *title, int32_t width, int32_t height)
{
   SCREEN_WIDTH = width;
   SCREEN_HEIGHT = height;

   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
   {
      printf("[video] video_init: could not initialise! SDL_Error: %s\n", SDL_GetError());
      return -1;
   }

   sdl_window = SDL_CreateWindow(title,
                                 SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED,
                                 SCREEN_WIDTH,
                                 SCREEN_HEIGHT,
                                 SDL_WINDOW_SHOWN);

   if (sdl_window == NULL)
   {
      printf("[video] video_init: window could not be created! SDL_Error: %s\n", SDL_GetError());
      SDL_Quit();
      return -1;
   }

   // No SDL_RENDERER_PRESENTVSYNC: frame pacing is done manually in
   // video_wait_vblank_leading(). With vsync enabled every present
   // blocks for a frame, which previously throttled the interpreter
   // to one opcode per frame when presents were issued per-opcode.
   sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
   if (sdl_renderer == NULL)
   {
      printf("[video] video_init: renderer could not be created! SDL_Error: %s\n", SDL_GetError());
      SDL_DestroyWindow(sdl_window);
      sdl_window = NULL;
      SDL_Quit();
      return -1;
   }

   SDL_RenderSetLogicalSize(sdl_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

   sdl_texture = SDL_CreateTexture(sdl_renderer,
                                   SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   SCREEN_WIDTH,
                                   SCREEN_HEIGHT);
   if (sdl_texture == NULL)
   {
      printf("[video] video_init: texture could not be created! SDL_Error: %s\n", SDL_GetError());
      SDL_DestroyRenderer(sdl_renderer);
      sdl_renderer = NULL;
      SDL_DestroyWindow(sdl_window);
      sdl_window = NULL;
      SDL_Quit();
      return -1;
   }

   // Start with a black screen
   {
      void *pixels;
      int pitch;

      if (SDL_LockTexture(sdl_texture, NULL, &pixels, &pitch) == 0)
      {
         memset(pixels, 0, (size_t)(pitch * SCREEN_HEIGHT));
         SDL_UnlockTexture(sdl_texture);
      }
   }

   video_present();

   printf("[video] video_init: %ix%i window created\n", SCREEN_WIDTH, SCREEN_HEIGHT);

   return 0;
}

void video_shutdown(void)
{
   if (sdl_texture != NULL)
   {
      SDL_DestroyTexture(sdl_texture);
      sdl_texture = NULL;
   }

   if (sdl_renderer != NULL)
   {
      SDL_DestroyRenderer(sdl_renderer);
      sdl_renderer = NULL;
   }

   if (sdl_window != NULL)
   {
      SDL_DestroyWindow(sdl_window);
      sdl_window = NULL;
   }

   SDL_Quit();
}

void video_poll_events(void)
{
   SDL_Event e;

   while (SDL_PollEvent(&e) != 0)
   {
      if (e.type == SDL_QUIT)
      {
         video_quit = 1;
      }
      else if (e.type == SDL_KEYDOWN)
      {
         if (e.key.keysym.sym == SDLK_ESCAPE)
         {
            video_quit = 1;
         }
         else
         {
            video_key_push(&e.key);
         }
      }
   }

   //
   // Record pointer state — the SDL stand-in for the DOS mouse callback
   // writing the button/X/Y words to the BIOS Data Area at 0x4F0, 0x4F2
   // and 0x4F4. Coordinates are converted through the renderer so a
   // future resizable window stays correct, and clamped to the screen
   // (the DOS driver clamped at the screen edges too).
   //

   if (sdl_renderer != NULL)
   {
      int mx, my;
      float fx, fy;
      uint32_t state = SDL_GetMouseState(&mx, &my);

      SDL_RenderWindowToLogical(sdl_renderer, (float)mx, (float)my, &fx, &fy);

      if (fx < 0.0f)
         fx = 0.0f;
      if (fy < 0.0f)
         fy = 0.0f;
      if (fx > (float)(SCREEN_WIDTH - 1))
         fx = (float)(SCREEN_WIDTH - 1);
      if (fy > (float)(SCREEN_HEIGHT - 1))
         fy = (float)(SCREEN_HEIGHT - 1);

      video_mouse_x = (int32_t)fx;
      video_mouse_y = (int32_t)fy;
      video_mouse_buttons = ((state & SDL_BUTTON_LMASK) ? 0x01 : 0) |
                            ((state & SDL_BUTTON_RMASK) ? 0x02 : 0) |
                            ((state & SDL_BUTTON_MMASK) ? 0x04 : 0);
   }
}

void video_present(void)
{
   if (sdl_renderer == NULL || sdl_texture == NULL)
      return;

   SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
   SDL_RenderPresent(sdl_renderer);
}

int32_t video_yield(void)
{
   uint32_t now = SDL_GetTicks();

   // Rate-limit the pump: at full interpreter speed the yield hook is
   // called many thousands of times per second, and an empty
   // SDL_PollEvent() is not free.
   if ((int32_t)(now - last_poll_tick) >= VIDEO_POLL_MIN_MS)
   {
      last_poll_tick = now;
      video_poll_events();
   }

   return video_quit;
}

void video_warp_mouse(int32_t x, int32_t y)
{
   if (sdl_window == NULL)
      return;

   SDL_WarpMouseInWindow(sdl_window, (float)x, (float)y);

   // Update the recorded state immediately so MOUSE_serve() sees the
   // new position before the next event pump
   video_mouse_x = x;
   video_mouse_y = y;
}

int32_t video_key_pending(void)
{
   return (key_head != key_tail);
}

uint16_t video_key_read(void)
{
   uint16_t key = 0;

   if (key_head != key_tail)
   {
      key = key_queue[key_head];
      key_head = (key_head + 1) % VIDEO_KEY_QUEUE_SIZE;
   }

   return key;
}

//
// The DOS VFX driver waited for the start of the vblank interval here,
// and the game used it as its frame-sync point (fades, animation
// delays, cursor restores). The SDL equivalent: pump events, pace to
// the next 60Hz frame boundary, then put the frame on screen.
//

void video_wait_vblank_leading(void)
{
   video_poll_events();

   if (next_frame_tick == 0)
      next_frame_tick = SDL_GetTicks() + VIDEO_FRAME_MS;

   uint32_t now = SDL_GetTicks();
   if ((int32_t)(next_frame_tick - now) > 0)
      SDL_Delay(next_frame_tick - now);

   video_present();

   next_frame_tick += VIDEO_FRAME_MS;

   // If we fell behind by a whole frame or more (long stall without a
   // vblank call), re-anchor instead of bursting through the backlog.
   if ((int32_t)(SDL_GetTicks() - next_frame_tick) >= 0)
      next_frame_tick = 0;
}

void video_wait_vblank(void)
{
   video_wait_vblank_leading();
}
