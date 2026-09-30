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
      else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
      {
         video_quit = 1;
      }
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
