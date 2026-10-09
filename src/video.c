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
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "video.h"
#include "globals.h" // test_palette (initial DAC contents)

int32_t SCREEN_WIDTH = 0;
int32_t SCREEN_HEIGHT = 0;

SDL_Window *sdl_window = NULL;
SDL_Renderer *sdl_renderer = NULL;
SDL_Texture *sdl_texture = NULL;

uint8_t *video_screen_buffer = NULL;

RGB video_dac[256];

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
// Frame cadence, matching the 60Hz VGA refresh the DOS build synced to.
// A 60 Hz period is 1000/60 = 16.666... ms, which has no exact integer
// form: the old (1000 / 60) truncated to 16 ms and ran everything at
// ~62.5 Hz. Step 17,17,16 ms in rotation instead (50 ms per 3 frames),
// so the long-term average is exactly 60 Hz with no accumulated error.
//

#define VIDEO_FRAME_RATE 60

//
// Minimum interval between event pumps from the interpreter yield hook
//

#define VIDEO_POLL_MIN_MS 5

// Next whole-ms step of the 17,17,16 rotation, advancing *phase.
static uint32_t video_frame_step(uint32_t *phase)
{
   uint32_t step = (*phase % 3u == 2u) ? 16u : 17u;

   ++(*phase);

   return step;
}

static uint32_t next_frame_tick = 0;
static uint32_t last_poll_tick = 0;
static uint32_t next_scan_tick = 0;
static uint32_t scan_phase = 0;
static uint32_t frame_phase = 0;
static uint32_t present_count = 0;

//
// 8-bit ARGB palette lookup, kept in sync with video_dac by
// video_DAC_write(); video_present() converts the screen buffer
// through this table
//

static uint32_t dac_argb[256];

static void video_dac_init_from_test_palette(void)
{
   for (int32_t i = 0; i < 256; i++)
   {
      video_dac[i] = test_palette.colors[i];

      dac_argb[i] = (255u << 24) |
                    ((uint32_t)video_dac[i].r << 16) |
                    ((uint32_t)video_dac[i].g << 8) |
                    (uint32_t)video_dac[i].b;
   }
}

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

   //
   // Allocate the "video RAM" and seed the DAC palette with the
   // test palette (the game's set_palette() calls overwrite it via
   // VFX_DAC_write as soon as it runs)
   //

   video_screen_buffer = malloc((size_t)(SCREEN_WIDTH * SCREEN_HEIGHT));
   if (video_screen_buffer == NULL)
   {
      printf("[video] video_init: could not allocate screen buffer\n");
      video_shutdown();
      return -1;
   }

   memset(video_screen_buffer, 0, (size_t)(SCREEN_WIDTH * SCREEN_HEIGHT));

   video_dac_init_from_test_palette();

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
   // TEMP DEBUG: dump final screen buffer state
   if (video_screen_buffer != NULL)
   {
      FILE *f = fopen("../misc-files/extracted/final_screen.ppm", "wb");

      if (f != NULL)
      {
         int32_t nonzero = 0;

         for (int32_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
            if (video_screen_buffer[i])
               nonzero++;

         fprintf(f, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);

         for (int32_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
         {
            RGB *c = &video_dac[video_screen_buffer[i]];
            fputc(c->r, f);
            fputc(c->g, f);
            fputc(c->b, f);
         }

         fclose(f);

         printf("[video] shutdown: final buffer nonzero=%i presents=%u\n", nonzero, present_count);
      }
   }

   if (video_screen_buffer != NULL)
   {
      free(video_screen_buffer);
      video_screen_buffer = NULL;
   }

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

      SDL_RenderWindowToLogical(sdl_renderer, mx, my, &fx, &fy);

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
   if (sdl_renderer == NULL || sdl_texture == NULL || video_screen_buffer == NULL)
      return;

   //
   // Convert the 8-bit palettized screen buffer to the display texture
   // through the current DAC palette
   //

   void *pixels;
   int pitch;

   if (SDL_LockTexture(sdl_texture, NULL, &pixels, &pitch) == 0)
   {
      uint32_t *dst = (uint32_t *)pixels;
      int32_t dst_stride = pitch / 4;

      for (int32_t y = 0; y < SCREEN_HEIGHT; y++)
      {
         const uint8_t *src_row = video_screen_buffer + (y * SCREEN_WIDTH);
         uint32_t *dst_row = dst + (y * dst_stride);

         for (int32_t x = 0; x < SCREEN_WIDTH; x++)
            dst_row[x] = dac_argb[src_row[x]];
      }

      SDL_UnlockTexture(sdl_texture);
   }

   present_count++;

   // TEMP DEBUG: report first non-empty present
   {
      static int reported = 0;

      if (!reported)
      {
         int32_t nonzero = 0;

         for (int32_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
            if (video_screen_buffer[i])
               nonzero++;

         if (nonzero > 0)
         {
            printf("[video] present: first non-empty frame, nonzero=%i\n", nonzero);
            reported = 1;
         }
      }
   }

   SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
   SDL_RenderPresent(sdl_renderer);
}

//
// VFX driver implementations (see video.h for the contracts)
//

static void video_clip_to_screen(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
   if (*x0 < 0)
      *x0 = 0;
   if (*y0 < 0)
      *y0 = 0;
   if (*x1 > SCREEN_WIDTH - 1)
      *x1 = SCREEN_WIDTH - 1;
   if (*y1 > SCREEN_HEIGHT - 1)
      *y1 = SCREEN_HEIGHT - 1;
}

void video_window_refresh(WINDOW *src, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   printf("[video] video_window_refresh: src=%p x0=%i y0=%i x1=%i y1=%i\n",
          (void *)src, x0, y0, x1, y1);

   if (video_screen_buffer == NULL || src == NULL || src->buffer == NULL)
      return;

   //
   // A window whose buffer IS the screen buffer is already on screen
   //

   if (src->buffer == video_screen_buffer)
      return;

   video_clip_to_screen(&x0, &y0, &x1, &y1);

   //
   // Source offset rule: when the rect fits inside the window bounds,
   // the buffer is screen-aligned and the rect coords are buffer
   // coords; otherwise the window is rect-sized (the mouse work/save
   // windows) and its content starts at local (0,0)
   //

   int32_t sx, sy;

   if (x1 <= src->x_max && y1 <= src->y_max)
   {
      sx = x0;
      sy = y0;
   }
   else
   {
      sx = 0;
      sy = 0;
   }

   int32_t w_width = src->x_max + 1;
   int32_t w_height = src->y_max + 1;
   int32_t copy_w = x1 - x0 + 1;
   int32_t copy_h = y1 - y0 + 1;

   if (sx + copy_w > w_width)
      copy_w = w_width - sx;
   if (sy + copy_h > w_height)
      copy_h = w_height - sy;

   if (copy_w <= 0 || copy_h <= 0)
      return;

   for (int32_t row = 0; row < copy_h; row++)
   {
      memcpy(video_screen_buffer + ((y0 + row) * SCREEN_WIDTH) + x0,
             src->buffer + ((sy + row) * w_width) + sx,
             (size_t)copy_w);
   }
}

void video_window_read(WINDOW *dest, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   printf("[video] video_window_read: dest=%p x0=%i y0=%i x1=%i y1=%i\n",
          (void *)dest, x0, y0, x1, y1);

   if (video_screen_buffer == NULL || dest == NULL || dest->buffer == NULL)
      return;

   video_clip_to_screen(&x0, &y0, &x1, &y1);

   int32_t copy_w = x1 - x0 + 1;
   int32_t copy_h = y1 - y0 + 1;

   if (copy_w <= 0 || copy_h <= 0)
      return;

   //
   // Same source rule as video_window_refresh: a rect that fits the
   // destination window reads at the rect coords (screen-aligned
   // buffer); a rect-sized window (mouse save/work) stores at (0,0)
   //

   int32_t dx, dy;

   if (x1 <= dest->x_max && y1 <= dest->y_max)
   {
      dx = x0;
      dy = y0;
   }
   else
   {
      dx = 0;
      dy = 0;
   }

   int32_t d_width = dest->x_max + 1;

   for (int32_t row = 0; row < copy_h; row++)
   {
      memcpy(dest->buffer + ((dy + row) * d_width) + dx,
             video_screen_buffer + ((y0 + row) * SCREEN_WIDTH) + x0,
             (size_t)copy_w);
   }
}

void video_pane_refresh(PANE *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   printf("[video] video_pane_refresh: target=%p x0=%i y0=%i x1=%i y1=%i\n",
          (void *)target, x0, y0, x1, y1);

   if (video_screen_buffer == NULL || target == NULL || target->window == NULL)
      return;

   WINDOW *w = target->window;

   //
   // The screen window's buffer IS the screen buffer
   //

   if (w->buffer == video_screen_buffer)
      return;

   //
   // The pane rect is the window's placement on screen: clip the
   // refresh region to it
   //

   if (x0 < target->x0)
      x0 = target->x0;
   if (y0 < target->y0)
      y0 = target->y0;
   if (x1 > target->x1)
      x1 = target->x1;
   if (y1 > target->y1)
      y1 = target->y1;

   video_clip_to_screen(&x0, &y0, &x1, &y1);

   //
   // Source offset rule (see video_window_refresh): subwindow panes
   // share their owner's screen-aligned buffer, so a rect that fits
   // the window bounds reads at the rect coords; a pane-sized buffer
   // (assign_window windows) reads pane-locally. For the assign_window
   // case the rect is the whole window, so both give (0,0).
   //

   int32_t sx, sy;

   if (x1 <= w->x_max && y1 <= w->y_max)
   {
      sx = x0;
      sy = y0;
   }
   else
   {
      sx = x0 - target->x0;
      sy = y0 - target->y0;
   }

   int32_t w_width = w->x_max + 1;
   int32_t w_height = w->y_max + 1;
   int32_t copy_w = x1 - x0 + 1;
   int32_t copy_h = y1 - y0 + 1;

   if (sx + copy_w > w_width)
      copy_w = w_width - sx;
   if (sy + copy_h > w_height)
      copy_h = w_height - sy;

   if (copy_w <= 0 || copy_h <= 0)
      return;

   for (int32_t row = 0; row < copy_h; row++)
   {
      memcpy(video_screen_buffer + ((y0 + row) * SCREEN_WIDTH) + x0,
             w->buffer + ((sy + row) * w_width) + sx,
             (size_t)copy_w);
   }
}

void video_area_wipe(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color)
{
   printf("[video] video_area_wipe: x0=%i y0=%i x1=%i y1=%i color=%i\n",
          x0, y0, x1, y1, color);

   if (video_screen_buffer == NULL)
      return;

   video_clip_to_screen(&x0, &y0, &x1, &y1);

   int32_t copy_w = x1 - x0 + 1;
   int32_t copy_h = y1 - y0 + 1;

   if (copy_w <= 0 || copy_h <= 0)
      return;

   for (int32_t row = 0; row < copy_h; row++)
   {
      memset(video_screen_buffer + ((y0 + row) * SCREEN_WIDTH) + x0,
             (uint8_t)color,
             (size_t)copy_w);
   }
}

void video_DAC_write(int32_t color_number, RGB *triplet)
{
   if (color_number < 0 || color_number > 255 || triplet == NULL)
      return;

   //
   // The game sends raw 6-bit VGA DAC components (see set_palette());
   // scale to 8-bit for the display palette, as the VGA hardware did
   //

   video_dac[color_number].r = (uint8_t)((triplet->r << 2) | (triplet->r >> 4));
   video_dac[color_number].g = (uint8_t)((triplet->g << 2) | (triplet->g >> 4));
   video_dac[color_number].b = (uint8_t)((triplet->b << 2) | (triplet->b >> 4));

   dac_argb[color_number] = (255u << 24) |
                            ((uint32_t)video_dac[color_number].r << 16) |
                            ((uint32_t)video_dac[color_number].g << 8) |
                            (uint32_t)video_dac[color_number].b;
}

void video_DAC_read(int32_t color_number, RGB *triplet)
{
   if (color_number < 0 || color_number > 255 || triplet == NULL)
      return;

   //
   // Deviation from the original: the VGA hardware read back 6-bit
   // components, this returns the 8-bit display palette. Only matters
   // for the (dormant) fade round-trips; revisit with those.
   //

   *triplet = video_dac[color_number];
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

   //
   // Continuous scan-out: the DOS VGA signal displayed video RAM at
   // 60Hz regardless of what the game did; the display must too, or
   // direct-to-video-RAM drawing (the mouse cursor) is invisible
   // until the next game-driven present.
   //

   if (next_scan_tick == 0 || (int32_t)(now - next_scan_tick) >= 0)
   {
      if (next_scan_tick == 0)
      {
         next_scan_tick = now;
         scan_phase = 0;
      }
      else
         next_scan_tick += video_frame_step(&scan_phase);

      if ((int32_t)(now - next_scan_tick) >= 0)
      {
         next_scan_tick = now; // fell behind: re-anchor
         scan_phase = 0;
      }

      video_present();
   }

   return video_quit;
}

void video_warp_mouse(int32_t x, int32_t y)
{
   if (sdl_window == NULL)
      return;

   SDL_WarpMouseInWindow(sdl_window, x, y);

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
   {
      next_frame_tick = SDL_GetTicks();
      frame_phase = 0;
   }

   uint32_t now = SDL_GetTicks();
   if ((int32_t)(next_frame_tick - now) > 0)
      SDL_Delay(next_frame_tick - now);

   video_present();

   next_frame_tick += video_frame_step(&frame_phase);

   // If we fell behind by a whole frame or more (long stall without a
   // vblank call), re-anchor instead of bursting through the backlog.
   if ((int32_t)(SDL_GetTicks() - next_frame_tick) >= 0)
      next_frame_tick = 0;
}

void video_wait_vblank(void)
{
   video_wait_vblank_leading();
}
