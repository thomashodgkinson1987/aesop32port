# AESOP/32 port — architecture notes

Corrected and expanded from the old GEMINI.md notes (which described only
the original Watcom tree).

## What the engine is

AESOP (Extensible State-Object Processor) is a state-machine bytecode
engine by John Miles (Miles Design, 1992). Game logic, AI and UI are not
compiled into the runtime: they live as "state objects" — AESOP bytecode —
inside a `.RES` resource container (`EYE.RES` for Eye of the Beholder III).
The runtime host loads the container, links the objects and interprets the
bytecode, calling back into a library of C "kernel functions" (low-level
services: file IO, input, video, sound, random numbers, etc.).

Two eras:

- **AESOP/16** — 16-bit DOS runtime (INTERP.EXE), 1992
- **AESOP/32** — 32-bit DOS/4G runtime (AESOP.EXE + A32*.DLLs), used by
  EOB3 and Dungeon Hack. EYE.RES containers keep the "AESOP/16 V1.00"
  signature string in both eras; the bytecode executed by the 32-bit
  runtime is the 32-bit flavour.

## Component map: Watcom original → this port

| Original (watcom-original/) | Port (src/) | Notes |
|---|---|---|
| `INTERP.C` | `interp.c` | entry point, arg parsing, program create/destroy, main loop |
| `asm/RT.ASM` | `rt.c` | the bytecode interpreter, rewritten in C. Stack values are the `VALUE` union: 4-byte value overlaid with 16:32 seg:off halves and 8-bit `STKVAL` type tag — LP32 layout, hence `-m32`. Video-agnostic like the original; yields to the host via the `RT_yield` hook |
| `RTRES.C` | `rtres.c` | resource manager / cache (`RTR_class`), directory blocks, resource lookup |
| `RTLINK.C` | `rtlink.c` | dynamic linker: binds import lists to C functions / other bytecode objects |
| `RTCODE.C` | `rtcode.c` | maps kernel C functions to bytecode IDs |
| `RTOBJECT.C` | `rtobject.c` | object/class instantiation, LESA structures |
| `EVENT.C` | `event.c` | event queue |
| `INTRFACE.C` | `intrface.c` | game/user interface glue |
| `EYE.C` | `eye.c` | game-specific math, effects |
| `GRAPHICS.C` | `graphics.c` | core graphics routines |
| `SOUND32.C`, `MODSND32.C`, `ail/` | `sound32.c` | AIL 2.0 audio; stubbed in the port |
| `GIL2VFX.C` | `gil2vfx.c` | glue between the game's GIL layer and the VFX video library |
| `vfx/` (VFX 1.19 library, DLL-based) | `vfx.c`, `mouse.c` | video API; the port replaces the VFX DLL system with SDL2, implemented in `video.c` (the "driver": window/renderer/texture lifecycle, event pump, present, 60 Hz frame pacing at `VFX_wait_vblank*`). The complete VFX 1.19 source release is archived at `../../reference/vfx119/` — `VFX_115/VFXA.ASM` is the ground truth for shape drawing: RLE decoded straight to the destination, **no transparent color anywhere** (skip packets are the only transparency), inclusive `xmax`/`ymax` (`linecount = ymax+1-ymin`); the color-255 skip rule exists only inside `VFX_shape_transform`'s intermediate buffer (VFXA.ASM:3917) and texture mapping (`XCOLOR`) |
| `xfer/` | — | file-transfer library, not ported |

Supporting code in the port that has no Watcom counterpart: `video.c/h`
(SDL2 video driver stand-in), `utils.c/h` (EOB3 shape-table access, RLE
decode — used only by `VFX_shape_transform` and the debug PGM dumps since
shape drawing went RLE-direct —, PPM dumps, debug shape drawing),
`trace.c/h` (shared hot-path trace gating, `AESOP_TRACE`), `globals.h`.

## Resource format essentials

- `.RES` = sequence of resources, each with a 12-byte header; a chain of
  directory blocks points at them; special tables 0–4 hold resource names,
  low-level function table and message names.
- Code resources have `.IMPT`/`.EXPT` sibling resources (import/export
  dictionaries) consumed by the linker.
- Special resources: `kernel` (the kernel state object), `start` (program
  entry), `menu`, `dungeon`, `PC`, `entities`, `items`, etc.
- The two known EYE.RES revisions are documented in
  `../../game-data/EYE-RES-notes.md`.

## Tooling

- `daesop` (companion repo) — container info/extract/replace, bytecode
  disassembly, EOB3 bitmap/font conversion, `-eob3menupatch`.
- The kernel's C-function inventory and bytecode opcode table live in
  `../../reference/decompiler-dumps/`. Caveat: those dumps were generated
  against the Apr 14 1993 EYE.RES revision, not the Jul 6 revision the
  port runs (`../game-data/eobiii/EYE.RES`). The `kernel` and `menu`
  bytecode is byte-identical across revisions (see
  `../../game-data/EYE-RES-notes.md`), but regenerate anything else
  with the current daesop: `cd ../daesop && ./bin/daesop -i
  ../game-data/eobiii/EYE.RES out.txt`.
- VFX 1.19 documentation and sources: `../../reference/vfx119/` (see the
  component map above for what to read first).

## Ecosystem & credits

The AESOP engine (AESOP/16 and AESOP/32) was created by **John Miles**
(Miles Design, 1992), who released the engine's compiler and
documentation into the public domain in 2009; **John Ratcliff**
co-developed the `MOD32SND` sound mixer. The game data of interest
ships with *Eye of the Beholder III* (`EYE.RES`) and *Dungeon Hack*
(`HACK.RES`, `OPEN.RES`).

Community context this project builds on:

- The **AESOP/32 modernisation project** led by **mirekluza**
  ([VOGONS thread](https://www.vogons.org/viewtopic.php?t=20601)),
  which produced the original `daesop` decompiler.
- **Bret Curtis (psi29a)'s
  [thirdeye](https://github.com/psi29a/thirdeye/)** — a from-scratch
  C++ AESOP engine reimplementation running these titles natively on
  modern systems, and a valuable reference.
- [AESOP engine discussion on the ScummVM
  forums](https://forums.scummvm.org/viewtopic.php?t=2397).

## Porting conventions

- C17, `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2`; fixed
  width types (`int32_t` etc.) preferred over `long`/`int`.
- The original Hungarian-ish prefixes remain: `HRES` (handle to resource),
  `UBYTE/UWORD/ULONG`.
- Stubbed functions print `[STUB]` markers to stdout — grep for `[STUB]`
  to find remaining work.
