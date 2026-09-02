# aesop32port

A 32-bit Linux port of the **AESOP/32** game engine (John Miles' Extensible
State-Object Processor), the bytecode runtime that shipped with
*Eye of the Beholder III* (and *Dungeon Hack*). The goal is to run the
original `EYE.RES` game data unmodified on a modern Linux system, with the
VGA-mode VFX video layer replaced by SDL2.

This is a continuation of the historical AESOP/32 modernisation effort
(mirekluza's VOGONS project, psi29a's thirdeye); see also the companion
`daesop` repository for `.RES` container tooling.

> **Status: work in progress.** The interpreter core, resource manager,
> linker and SDL2 video path are in place; gameplay brings-up to the
> point of loading the bootstrap object. Large parts of the VFX, sound
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
- 32-bit SDL2 development libraries:
  - Preferred: the distro's multilib package (`lib32-sdl2` /
    `lib32-sdl2-compat` on Arch, `libsdl2-dev:i386` on Debian)
  - Fallback: build the vendored copy — see
    `../reference/build-32bit-sdl2.sh`, which installs to `../SDL2_32`
- GNU make

```sh
make build          # produces bin/aesop
```

If using the vendored SDL2, put its `sdl2-config` first on the PATH:

```sh
PATH="../SDL2_32/bin:$PATH" make build
```

## Running

The engine needs the EOB3 game data (an `eobiii/` directory containing
`EYE.RES` et al.) next to the repo — this workspace keeps it at
`../game-data/eobiii` with a symlink `eobiii -> ../game-data/eobiii`
(gitignored).

```sh
make run                    # build, copy bin/aesop into eobiii/, run interactively
make run 15 out.txt         # run for 15 seconds, capture output to out.txt
```

The binary takes the resource file and start object as arguments
(`./aesop EYE.RES start`).

What you should see: the engine boots the resource system, links the
`start` bootstrap and the `main menu` program, dumps decoded sprites (see
below), then enters the interpreter render loop. Press **ESC** or close
the window for a clean shutdown. As of this writing the screen shows a
green-cleared window with blitted shapes — VFX shape drawing is still
stubbed, so the real menu graphics don't render yet.

### Sprite dumps

`VFX_shape_draw` debug-dumps every shape of a drawn shape table to
`../misc-files/extracted/image_XX_XXXX.pgm`, **relative to the run
directory**. In this workspace the run directory is `eobiii/` (symlink to
`../game-data/eobiii`), so the dumps land in `game-data/misc-files/extracted/`
— that directory must exist or the dumps are silently skipped. This is
temporary debug code (the hard-coded path will want re-visiting).

### Troubleshooting

- **Audio**: `SDL_Init` fails with `dsp: No such audio device` if the
  SDL2 in use has no pulse/alsa backend (the vendored `SDL2_32` build
  doesn't). Workaround: run with `SDL_AUDIODRIVER=dummy`.
- **Paths with spaces**: `$(shell sdl2-config --cflags)` breaks when the
  workspace path contains spaces (the `-I`/`-L` flags get word-split).
  Either build from a space-free path, or override with a copy of
  `SDL2_32` at a space-free location:
  `cp -a ../SDL2_32 /tmp/SDL2_32 && PATH=/tmp/SDL2_32/bin:$PATH make build`

## Layout

- `src/` — the port. Key files: `interp.c` (entry point + main loop +
  SDL2 window/render lifecycle), `rt.c` (bytecode interpreter, C rewrite
  of the original `RT.ASM`), `rtres.c` (resource cache), `rtlink.c`
  (dynamic linker binding C functions to bytecode), `rtcode.c`
  (C-function table), `gil2vfx.c` (game-to-VFX glue), `vfx.c` (VFX video
  API), `utils.c` (shape decoding, PPM dumps, debug drawing),
  `globals.h` (shared SDL objects)
- `eobiii/` — (gitignored symlink) game data; see `../game-data/`
- `docs/` — architecture notes

## Status

Implemented (non-stub): interpreter core, resource manager, linker,
message/event system, C-function dispatch, SDL2 window/renderer/texture
lifecycle, palette handling, EOB3 shape (RLE) decoding.

Still stubbed (grep for `[STUB]`), by file:

- `vfx.c` — 68 (bulk of the VFX video API: stencils, panes, refreshes)
- `eye.c` — 11 (game math/effects)
- `sound32.c` — 10 (AIL/MOD sound)
- `graphics.c` — 3, `mouse.c` — 3, `rtsystem.c` — 2, `rtcode.c` — 1
- `intrface.c` — 1

## Related repositories

- `daesop` — AESOP resource decompiler/disassembler (same author);
  used to inspect `EYE.RES` and generate the dumps in
  `../reference/decompiler-dumps/`
- `../reference/watcom-original/` — the original Watcom/DOS source tree
  this port derives from
