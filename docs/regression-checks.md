# Regression checks — menu-path behavioral tripwire

The main-menu idle path is sensitive to sub-percent perturbations in
the interpreter/event code: a change with no semantic effect (and no
log errors) can flip it from "sits on menu" to "141 event-pump
iterations, then an unprompted quit-to-black". This documents the
cheap checks that catch that class of regression, and the episode
that motivated them (item 4, below).

## Quick tripwire (every `rt.c` / `event.c` change)

Prerequisites: 32-bit build works (`make build`), Xvfb on `:99`
(`Xvfb :99 -screen 0 1024x768x24 &`), game data in `eobiii/`.

```sh
cp bin/aesop eobiii/
cd eobiii
DISPLAY=:99 SDL_AUDIODRIVER=dummy timeout --foreground 5s ./aesop EYE.RES start > check.log 2>&1
```

Pass criteria (all must hold; values from the verified baseline):

| Check | Command | Expected |
|---|---|---|
| Menu renders | `grep "first non-empty frame" check.log` | `nonzero=85` |
| Menu intact at shutdown | `grep "final buffer" check.log` | `nonzero=63999` |
| No new stubs hit | `grep -c "STUB" check.log` | `1` (`sound32 init_sound`) |
| No fault diagnostics | `grep -c -E "cache exhausted\|stk_off mismatch\|underflow\|overflow\|abend\|opcode_fault" check.log` | `0` |
| Pump runs once | `grep -c "Message=9" check.log` | `1` |
| Shutdown shape | `tail -1 check.log` | `exit - return=-1` (TERM-killed while idling) |

`Message=9` is the main-menu object's modal **"get choice"** pump
(`while(!flag) dispatch_event()`); exactly 1 dispatch in 5 s means
the menu is idling. `141` + `nonzero=0` + `return=0` means the game
walked its abandon/quit path with no input — a regression (or a
behavior change requiring explicit justification).

## Acceptance tests (rendering-path changes)

Beyond the idle tripwire, two interactive proofs cover the paths
the tripwire cannot reach (neither fires on the untouched menu):

- **Summon transfer dialog**: click true-Summon (aim at the bottom
  of its box — hit zones sit lower than the visuals, see below) or
  keyboard-select it (Down ×2 from boot, then Enter). Expect the
  `xfer` dialog with panel border (`VFX_line_draw`) and legible
  CHARCOPY text; second click dismisses back to a full menu repaint.
  Log markers: `open_transfer_file`, 4× `draw_line`, dialog `print`
  strings.
- **Hover highlight**: park the pointer on an item, move it fully
  away (off-window corner), screenshot. The last-hovered item must
  recolor in place (same pixels, highlight palette — verified as a
  300 px gold→bright swap with zero positional shift), with no
  cursor overlap to confuse the diff.

Visual confirmation (optional but recommended for rendering-path
changes): capture the window and diff against a known-good shot —

```sh
export DISPLAY=:99
./aesop EYE.RES start & sleep 5
ID=$(xdotool search --name "AESOP/32 Port" | head -1)
[ -n "$ID" ] && timeout 3 import -window "$ID" shot.png
compare -metric RMSE known-good.png shot.png null:
```

`0 (0)` is a pass; a diff confined to the cursor sprite is a pass
(the cursor sits wherever the OS pointer was left). Never run
`import -window` with an empty ID (falls into interactive mode).

## A/B method (when the tripwire fails or a change is high-risk)

Build the pre-change binary without touching the repo: copy `src/`
to `/tmp`, restore the file(s) under test from git
(`git show HEAD:src/rt.c > ...`), compile with the Makefile flags,
link against `SDL2_32`, and run both binaries back-to-back from the
same `eobiii/` directory. Same environment + same 5 s window makes
any behavioral delta attributable to the change.

## Known episode: item 4 (`static bool quit`) — parked

`RT_execute`'s `static bool quit` (chatgpt-report item 4) is a real
bug — one transient nonzero yield would poison all future
executions — but the fix was reverted: removing the BSS static
deterministically flips the menu 1 → 141 message-9 dispatches plus
quit-to-black, with no input and no new diagnostics.

Bisection (per-variant `/tmp` binaries, identical link structure):

| Variant | Change vs HEAD | Result |
|---|---|---|
| trace bound / CALL default / JSR / RTS guard alone | never-taken branches | stays |
| timing-only `volatile` NOP per loop | +~2 cycles/iter | stays |
| quit hunk (static removed) | — | quits |
| static→local, per-iter test kept | static removed | quits |
| dummy static elsewhere (BSS size restored) | static removed | quits |
| HEAD or v4 + `-ftrivial-auto-var-init={zero,pattern}` (all files) | zeroed/patterned automatics | no change either way |
| quitting binary + TEMP prints / under gdb | perturbed | stays |

Ruled out, in order: semantic effect (all new branches
trace-verified never-taken; yield provably always 0 —
`host_yield` returns latched `video_quit`), uninitialized C locals,
per-iteration timing, BSS size. What remains: exact static layout
(and, independently, gross timing) moves the pump's drain dynamics
between attractors. ASLR is on (`randomize_va_space=2`), so absolute
addresses vary per run — the sensitivity is to relative layout or
deterministic leftover values, not fixed addresses.

Collateral established during the hunt (verified, reusable):

- Menu message map (`main menu` resource 1322): M:9 "get choice"
  (modal pump), M:6 "menu option selected" (sets pump-exit flag),
  M:1 "destroy" (`hide_mouse, light_fade, wipe, wipe` — the quit
  sequence), M:7/M:8 enable/disable, M:3 "print menu", M:4/M:5
  mouse/key handlers (SEND M:3 only). Disassemble with daesop:
  `daesop -j EYE.RES "main menu" out.txt` — requires
  `bytecode.def` beside the binary (`make package`, see daesop
  README).
- `mem_alloc` checksums are exit-only accounting, not a suspect.
- `host_yield` returns `video_yield()` verbatim.

## Verified game facts (don't re-derive these)

- **No menu fade-in exists in bytecode.** The show/enable paths
  call no fade; `start` imports `light_fade` but never calls it.
  The "reveal" is the shape_transform zoom series (9 scaled draws,
  unpaced — completes in microseconds on modern hardware, hence
  invisible; leave alone). Do not invent a fade-in.
- **Menu quit fade-out is requested but stubbed.** M:1 destroy
  calls `light_fade(color=0)` → `VFX_window_fade(..., 20)` (DDA
  palette cross-fade per VFXA.ASM:9823-9985), then wipes. The port
  drops it (stub + NULL DAC hooks): current behavior is a hard cut
  to black.
- **Menu item text is dual-rendered.** The background shape carries
  baked item text; M:3 re-prints the same strings through fonts
  (base colors normally, highlight remap on hover). Both must land
  pixel-on-pixel (CENTER justification does this); any positional
  mismatch shows as doubling.
- **Dialog text needs fonts + line fills.** `print` output that
  looks "missing" is usually glyph pixels (font rasterizer) or line
  background clears — check both before blaming layout.
- **Pointer hit zones sit lower than the visuals** (~8 px; click
  bottom-of-box, e.g. y≈133 for Summon). Keyboard nav avoids this
  entirely: arrows + Enter (selection starts at Continue, so Down
  ×2 + Enter reaches Summon).
- **Open text-layout question**: text background fills span
  x=3–318 and cover the dialog's right border pixel column;
  re-examine fill extent vs border geometry if dialogs ever look
  off by one pixel on the right.

Sharpest next experiments: `gdb` hardware watchpoint on the menu
statics (`staticVar0/2` in the RTR cache segment — traps only on
writes, likely below the perturbation threshold that breakpoints
cross), or BSS padding cartography (`static uint8_t pad[N]` at
various points) to localize the sensitive address region. Re-attempt
item 4 only after the sensitivity is understood.
