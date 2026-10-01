# aesop32port

A 32-bit Linux port of the **AESOP/32** game engine (John Miles' Extensible
State-Object Processor), the bytecode runtime that shipped with
*Eye of the Beholder III* (and *Dungeon Hack*). The goal is to run the
original `EYE.RES` game data unmodified on a modern Linux system, with the
VGA-mode VFX video layer replaced by SDL2.

This is a continuation of the historical AESOP/32 modernisation effort
(mirekluza's [VOGONS project](https://www.vogons.org/viewtopic.php?t=20601),
psi29a's [thirdeye](https://github.com/psi29a/thirdeye)); see also the
companion `daesop` repository for `.RES` container tooling. Credits and
ecosystem links: [docs/architecture.md](docs/architecture.md).

> **Status: work in progress.** The interpreter core, resource manager,
> linker and SDL2 video path are in place; gameplay brings-up through
> to rendering the main menu. Large parts of the VFX, sound
> and game-side layers are still stubs (see [Status](#status) below).

## Why 32-bit?

The port is deliberately built with `gcc -m32` (i386). The reasons are
structural, not accidental:

- The bytecode value model is 32-bit (stack slots are a 4-byte value plus
  a 2-byte type tag), and the original code overlays pointers with
  segmented 16:32 halves (`VALUE` union in `src/rt.c`) — this only has
  correct layout when `sizeof(void*) == 4` and `sizeof(long) == 4`.
- Memory layout matching the original exactly makes debugging the
  interpreter far easier.
- A finished, correct 32-bit build is the regression oracle for any future
  64-bit migration; going 64-bit later is mostly mechanical
  (fixed-width types, rework of the seg:off union, struct packing audit)
  once a working reference exists.

Long term the project would like to be 64-bit native, but finishing the
32-bit port comes first.

## Building

Requirements:

- gcc with 32-bit support (`multilib-devel` on Arch/CachyOS,
  `gcc-multilib` on Debian/Ubuntu)
- a 32-bit SDL2 build:
  - Preferred: the distro's multilib package (`lib32-sdl2` /
    `lib32-sdl2-compat` on Arch, `libsdl2-dev:i386` on Debian)
  - Fallback: a vendored build installed to a `SDL2_32/` directory next
    to the repo — `tools/build-32bit-sdl2.sh` fetches SDL2 2.30.0 and
    builds it with working X11 + OpenGL + ALSA-dynamic backends
- GNU make

```sh
make build          # produces bin/aesop
```

The Makefile finds 32-bit SDL2 via `SDL2_DIR` (default: `../SDL2_32`,
i.e. a sibling directory kept out of the repo). Point it elsewhere if
your 32-bit SDL2 lives somewhere else:

```sh
make build SDL2_DIR=/usr          # distro multilib install
```

## Running

The engine needs the EOB3 game data — an `eobiii/` directory containing
`EYE.RES` and the rest of the game install — at the repo root. The game
data is **not** part of this repo (copyright); supply your own Eye of
the Beholder III files. `make run` copies the binary into `eobiii/` and
launches it there:

```sh
make run                    # build, copy bin/aesop into eobiii/, run interactively
make run 15 out.txt         # run for 15 seconds, capture output to out.txt
```

Set `AESOP_TRACE=1` to additionally log every executed opcode (very
verbose — the interpreter runs hundreds of thousands of opcodes per
second).

The binary takes the resource file and start object as arguments
(`./aesop EYE.RES start`).

### What you should see

The engine boots the resource system, links the `start` bootstrap and the
`main menu` program, dumps decoded sprites (see below), then enters the
interpreter loop. Press **ESC** or close the window for a clean shutdown.
Rendering is decoupled from opcode execution: the interpreter runs at
full speed, and a frame is presented when the game refreshes a window
(`refresh_window`) or hits the vblank stand-in
(`VFX_wait_vblank_leading`, paced to 60 Hz by `src/video.c`) — the same
point the DOS build throttled itself at. The screen shows a black window
with a single 16x16 sprite (draw call 1), and the main menu background
(319x199) paints itself (draw call 2) moments later.

Input flows too: `video.c` records pointer/keyboard state, and the
interpreter's yield hook drives `MOUSE_serve()` and the 60 Hz interface
timer (the stand-ins for the DOS INT 33h callback and the AIL timer),
so the game receives `SYS_MOUSEMOVE`/`SYS_CLICK`/`SYS_KEYDOWN`/
`SYS_TIMER` events and hit-tests the menu. The main menu renders
through the real drawing path (`VFX_shape_draw`/`VFX_shape_transform`
→ window buffers → `VFX_pane_refresh` → DAC palette → texture),
including the zoom-in intro animation. The in-game cursor follows the
mouse across the whole screen (the display scans out of video RAM at
60 Hz from the yield hook, VGA-style) and merges correctly with menu
refreshes; clicking "Abandon Quest" quits cleanly.

### Driving the game programmatically

The window (title `AESOP/32 Port`, 320x200 client) can be driven and
verified from the command line with `xdotool` and ImageMagick — move
the OS pointer (`mousemove` takes screen coordinates: window origin
from `getwindowgeometry --shell` + client offset), click with
`mousedown 1` → `sleep 0.3` → `mouseup 1` (the port polls button state
per yield, so instant synthetic clicks can be missed), and capture the
window with `import -window <id> shot.png` (inside a `timeout`, with a
non-empty window ID). PNGs can be read directly for visual checks, and
`compare -metric RMSE` diffs two captures. The full recipe — including
bounded launch/capture commands — lives in the workspace `AGENTS.md`.

Main menu item centres (window-relative; entries ~8 px tall, ~242 px
wide; the pointer position is the hit-tested point):

| Item | x | y |
|------|-----|-----|
| Introduction | 160 | 106 |
| Continue the Quest | 160 | 114 |
| Gather a New Party | 160 | 122 |
| Summon the Heroes of Darkmoon | 160 | 130 |
| Abandon the Quest | 160 | 138 |

### Sprite dumps

`VFX_shape_draw` debug-dumps every shape of a drawn shape table to
`../misc-files/extracted/image_XX_XXXX.pgm`, **relative to the run
directory** — i.e. a `misc-files/extracted/` directory next to
`eobiii/`. Create it before running; if it is missing the dumps are
silently skipped. This is temporary debug code (the hard-coded path
will want re-visiting).

> In the author's workspace `eobiii/` is a gitignored symlink to
> `../game-data/eobiii`, so the dumps land in
> `../game-data/misc-files/extracted/` instead.

### Troubleshooting

- **Audio**: the vendored SDL2 loads ALSA dynamically at runtime
  (`libasound.so.2`), so sound init normally just works. If audio init
  ever fails (`dsp: No such audio device`), run with
  `SDL_AUDIODRIVER=dummy`.
- **No window appears**: means the SDL2 in use was built without a real
  video driver (X11/Wayland) — fake windows succeed silently. Rebuild
  SDL2 with `tools/build-32bit-sdl2.sh`; an offscreen-only build caused
  exactly this symptom once.

## Layout

- `src/` — the port. Key files: `interp.c` (entry point + main loop +
  SDL2 window/render lifecycle), `rt.c` (bytecode interpreter, C rewrite
  of the original `RT.ASM`), `rtres.c` (resource cache), `rtlink.c`
  (dynamic linker binding C functions to bytecode), `rtcode.c`
  (C-function table), `gil2vfx.c` (game-to-VFX glue), `vfx.c` (VFX video
  API), `utils.c` (shape decoding, PPM dumps, debug drawing),
  `globals.h` (shared SDL objects)
- `tools/` — `build-32bit-sdl2.sh`, reproduces the vendored 32-bit SDL2
- `eobiii/` — EOB3 game data (not in git; supply your own)
- `docs/` — architecture notes

## Status

Implemented (non-stub): interpreter core, resource manager, linker,
message/event system, C-function dispatch, SDL2 window/renderer/texture
lifecycle, palette handling, EOB3 shape (RLE) decoding.

Still stubbed (grep for `[STUB]`), by file:

- `vfx.c` — 56 (stencils, pane scroll, fonts, fades)
- `eye.c` — 11 (game math/effects)
- `sound32.c` — 10 (AIL/MOD sound)
- `graphics.c` — 3, `rtsystem.c` — 2

## Related repositories

- [`daesop`](https://github.com/thomashodgkinson1987/daesop) — AESOP
  resource decompiler/disassembler (same author); used to inspect
  `EYE.RES` and generate reference dumps
- The original 1993 Watcom/DOS source tree this port derives from is
  archived out-of-repo; `docs/architecture.md` maps its components to
  this port
