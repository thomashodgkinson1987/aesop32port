#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include <SDL2/SDL.h>

#include "defs.h"
#include "utils.h"

extern int32_t SCREEN_WIDTH;
extern int32_t SCREEN_HEIGHT;

extern SDL_Window *sdl_window;
extern SDL_Renderer *sdl_renderer;
extern SDL_Texture *sdl_texture;

extern AESOP_Palette test_palette;

#endif
