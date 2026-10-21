# The Catacomb Abyss for the Sega Master System

A port of Softdisk's *The Catacomb Abyss* (Gamer's Edge shareware v1.13,
1992) to the Sega Master System. It is built with SDCC 4.4.0 and devkitSMS.

The game logic comes from the GPL source release of the game. It reached
this port through haroldo-ok's Sega Genesis / Mega Drive port
([catacomb-abbyss-md](https://github.com/haroldo-ok/catacomb-abbyss-md)).
All pictures, fonts, maps, texts and sounds are converted from that port's
generated data, which itself comes from the original shareware files.

The output is `catacomb_abyss.sms`, a 3 MB ROM (188 × 16 KB banks) that uses
the standard Sega mapper and 32 KB of battery-backed cartridge RAM.

## What is in it

* **The game** (CATABYSS.EXE):
  * all 19 levels, every monster, item, scroll and key
  * the level names and level description texts
  * the in-game help (all 17 pages of HELP.TXT)
  * the debug keys
  * all 34 sound effects, played on the PSG
* **The title sequence** (INTRO.EXE): the attract screens (Mad Gamers,
  Softdisk, title, credits, trilogy), the "Prepare to enter the CATACOMB..."
  screen, the "Novice or Warrior?" challenge with its replies, the story
  screen, and "Back for more" when you restore a game.
* **The electronic catalog** (DEMOCAT.EXE): the catalog picture pages.
* **The closing screen** (LOADSCN.EXE): ARMAPC.
* **Saved games**: 3 slots in battery-backed cartridge RAM.
* **The finale picture** shown after winning.

Text that talks about PC keys was rewritten for the control pad. That
includes the key hints drawn in the status bar (`PAUSE MENU … 1 FIRE`).

### Not converted yet

* The **user's guide shell** (START.EXE: MENU, QUICK, INSTRUCT, PRODUCTS and
  PRINTFIL.TXT). On the title screen, DOWN currently opens the in-game help
  instead.
* The catalog's **LAST.CAT** closing page and the **LAST.ABS** closing
  screen. Only ARMAPC is shown after quitting.

## Controls

### Title screen

| Button | Action |
|---|---|
| 1 | New game. At "Novice or Warrior?", press 1 for Novice, 2 for Warrior, or PAUSE to go back |
| 2 | Restore a saved game |
| UP | Electronic catalog. LEFT/UP goes to the previous page, any other button to the next; 2 or PAUSE leaves |
| DOWN | Help pages |

### In the game

| Input | Action |
|---|---|
| D-pad Up / Down | Walk forward / back |
| D-pad Left / Right | Turn |
| Left or Right twice (hold the second press) | Quick turn |
| 2 + Left / Right | Sidestep |
| 1 | Magick Missile. Hold to charge the big shot |
| 2 (tap on its own) | Zapper (uses a bolt) |
| hold 2, press 1 | Xterminator (uses a nuke) |
| hold 2, press Down | Cure potion |
| PAUSE | Options menu |
| PAUSE with 1 + 2 held | Debug keys (the original's F10 menu) |

Keys, gems, scrolls, chests and other treasure are picked up by walking over
them. Keys open the matching doors when you walk into them. Each gem colour
makes that type of monster show on the radar.

The **Options** menu appears on the message line. Choose with Left/Right and
press 1 to select:

| Item | Does |
|---|---|
| Help | The 17 help pages. Left/Right changes page, Up/Down scrolls, 1 / 2 / PAUSE closes |
| Sound | Sound effects on or off |
| Scrolls | Read a scroll you have found |
| Save / Restore | Save or load one of 3 cartridge RAM slots |
| New | Start over |
| Quit | Leave the game (closing screen, then the title) |

## Building

Requirements:

* **SDCC 4.4.0**. The Makefile expects it in `~/sdcc440/sdcc-4.4.0/bin`; set
  `SDCCBIN=` to use another path. Do not use 4.5.0: build #15242 miscompiles
  `if (a != g) { g = a; … }` when `a` is held in register A.
* **devkitSMS tools** (`makesms` and the peephole rules). The Makefile
  expects them in `~/.devkitsms`; set `DK=` to use another path. `makesms`
  must be on `PATH`.
* Python 3 with Pillow, for `tools/checkrom.py` and the asset tools.

Build the ROM:

```sh
export PATH=$HOME/.devkitsms/bin:$PATH
make            # -> catacomb_abyss.sms
```

The link step runs `tools/checkrom.py`. It reports the free space in every
bank and fails if code overlaps the data banks.

### Generated data (`gen/`)

`gen/` contains all converted data, so a normal build does not need the
Genesis sources:

* `data.bin`: banks 8 and up
* `assets.h` / `assets.inc`: the asset index
* `assets_tab.c`
* `gamefont.c`
* `states.c` / `states.h`

You only need to regenerate it after changing the conversion tools. That
requires a checkout of
[catacomb-abbyss-md](https://github.com/haroldo-ok/catacomb-abbyss-md) in
`~/catacomb-abbyss-md`:

```sh
cd tools
python3 extract.py          # Genesis data -> tools/data.pkl (already included)
python3 build_assets.py     # -> gen/data.bin, assets.h/.inc, assets_tab.c, gamefont.c
python3 gen_states.py ../orig ../gen       # actor state table -> gen/states.c/.h
```

The Makefile reruns `build_assets.py` automatically when a file in
`tools/*.py` is newer than `gen/assets.h`.

## Testing and profiling

`tools/smstest/` is a headless SMS emulator that runs scripts. It is
haroldo-ok's tool from the SMS skill, with banked-call attribution added for
the profiler. Build it with `make -C tools/smstest`. Run scripts through
`tools/st.sh`:

```sh
mkdir -p /tmp/s
tools/st.sh catacomb_abyss.sms tests/smoke.txt    # title screen comes up
tools/st.sh catacomb_abyss.sms tests/play.txt     # intro -> challenge -> story -> play -> move
tools/st.sh catacomb_abyss.sms tests/text.txt     # Options menu, help pages
tools/st.sh catacomb_abyss.sms tests/steady.txt   # 400 frames of walking, profiled
python3 tools/prof2.py /tmp/s/cg_steady.txt build/game.noi build/*.rst --top 25
python3 tools/prof2.py /tmp/s/cg_steady.txt build/game.noi build/*.rst --tree ThreeDRefresh --depth 3
python3 tools/srcprof.py /tmp/s/cg_steady.txt DrawScaleds c4_draw   # cycles per source line
```

Screenshots are written as `.ppm` files to `/tmp/s/`. `tests/bench/` holds
the scripts and reference images for the renderer benchmark build. That
build uses `src/bench_main.c.txt` in place of `src/main.c`.

## How it works

### Memory and banks

| Bank | Contents |
|---|---|
| 0 (fixed) | startup, VDP/PSG layer, renderer core, bank-0 maths, asset tables, actor states |
| 1 | `c4_main`, `c4_game`, `c4_debug`, `c4_draw` |
| 2 | `c4_play`, `c4_state` |
| 3 | `c4_wiz` |
| 4 | `c4_act1` (monsters) |
| 6 | menus, save games, intro / catalog / closing screens |
| 7 | 2D graphics and text, HUD, video, input, font |
| 8 – 187 | data: screens, wall / sprite scalers and textures, maps, texts, sounds |

Banked functions use SDCC's `__banked` calls through a trampoline. The game
works in the first 16 KB half of cartridge RAM, mapped at 0x8000:

* tile map, spot visibility and actor maps (64 × 32 each)
* the two map planes
* the object list (100 objects)

The second half holds the three save slots. Each slot stores the game state,
the map planes XOR'd against the ROM copy and RLE-packed, and the object list.

### Renderer

The 3D view is 224 × 80 pixels: 280 unique background tiles, drawn at tile
(2,1). It casts 56 rays and draws 4-pixel-wide wall columns with scaler code
generated by `tools/render_gen.py`. Sprites are scaled by depth. Each frame
is composed in RAM, then copied to VRAM at the fastest rate the VDP allows
during active display (one OUT every 27 or more cycles). Sky and floor tiles
are shared.

The 70 Hz timer of the original runs from the frame interrupt. Objects move
in steps of at most 6 tics, as the collision code needs, so the game keeps
its speed when the frame rate drops.

### Speed

Walking around runs at about 4 frames per second: roughly 16 console frames
per 3D redraw, measured with `tests/steady.txt` on level 1. About half of
that is the renderer. The copy of the view to VRAM alone takes about 3
frames.

The hot paths of the game logic are in Z80 assembly:

* sprite visibility, `CalcBounds`, the actor scan in `LocationInActor`
* radar plotting
* text glyph blitting and tile painting

A new message in the status bar costs a short hitch of a few frames.

### SDCC 4.4.0 problems worked around

* `__mulsint2slong` (signed 16 × 16 → 32) in the 4.4.0 library tests the
  sign of the **low** bytes, so any operand whose low byte has bit 7 set
  gives a wrong result. `smul16()` in `src/plat0.c` is a fixed copy.
* A variable shift (`0x80 >> (x & 7)`) inside a complex loop was compiled
  without the shift. The text code now avoids variable shifts.
* Long arithmetic is slow (`__mullong` takes about 3400 cycles), so the hot
  code calls `__muluint2ulong` directly or uses 8.8 / 2.14 fixed point.

## Source layout

| Path | Contents |
|---|---|
| `src/game/` | the game (`c4_*.c`, `def.h`), ported from the Genesis sources |
| `src/` | SMS platform layer: `crt0.s`, `header.s`, `hw.c` (VDP / mapper), `render.c` + `render_a.s` (3D view), `gfx.c` (2D, text, windows), `hud.c`, `vid.c`, `input.c`, `snd.c`, `save.c`, `menu.c`, `intro.c`, `main.c`, `plat0.c` |
| `gen/` | generated data and tables (see above) |
| `tools/` | conversion tools (`build_assets.py` and friends), `checkrom.py`, the profilers, `smstest/` |
| `tests/` | smstest scripts |
| `orig/` | the Genesis port's sources this port started from (for reference) |
| `Makefile` | banked build |

## Credits and licence

* *The Catacomb Abyss* © 1992 Softdisk Publishing. Design and production by
  Greg Malone; programming by Mike Maynard, Jim Row and Nolan Martin; art by
  Steve Maines, Carol Ludden and Jerry Jones.
* The source code was released under the GNU GPL v2. The game code here
  derives from it, so this port is distributed under the GPL v2 as well.
* Genesis / Mega Drive port and the data conversion it ships: haroldo-ok
  ([catacomb-abbyss-md](https://github.com/haroldo-ok/catacomb-abbyss-md)).
* devkitSMS, the SMS skill and `smstest`: haroldo-ok
  ([sega-master-system-skill-for-claude](https://github.com/haroldo-ok/sega-master-system-skill-for-claude)).
  `smstest` is MIT-licensed.
