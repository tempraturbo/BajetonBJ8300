# Version history

> 🇧🇷 Português: [historico.md](historico.md)

Dates are 2026. Every version was flashed and tested on the radio; whatever broke is recorded with
its cause, because that is what stops it happening twice.

## CHIRP driver — Oct 24 — no zones, and the firmware guard redone

General review of `chirp/bj8300_v1.0TEMPRA.py`, with a new test (`tests/teste_clonagem.py`) that runs
CHIRP's full `sync_in`/`sync_out` against a simulated radio, for both firmwares:

- **Zones:** the driver already showed nothing about zones (the BJ7800 base hides them, and the
  15-fixed-zone model is replaced by the banks) — surveyed setting by setting in real CHIRP: 111
  settings in 4 groups, none about zones. What was missing was the **mode**: it still exists in the
  firmware (`b[0x14BD4]`, which the cable reads and writes at `0x9028` — derived from the settings
  save routine, `0x01E3AB36`, and checked against CHIRP's format), and with the menu entry gone the
  radio could not leave it. Every upload now writes **Full Channel**. The owner's radio was already
  in Full Channel (dump from Oct 24).
- **Firmware guard:** it compared the extended read of `0x0C0000` with the normal read of `0x0000`
  — but the normal one comes back decrypted and the extended one raw, so they never matched. Against
  a stock radio it only held because the stock firmware's long answer (4 + `0x8C` bytes, read from
  the handler at `0x01E4BB6E`) desynced the cable, and the message was a generic "Failed to read
  block". The driver now reads the answer's header: `0x40` = bank firmware; anything else = stock,
  discard the rest and stop with the right message.
- Run against the old driver, the new test fails exactly at those points (Full Channel, message,
  cable desync) and passes the one that mattered: no extended write ever reached the stock firmware.

The banks themselves (format, marker, checksum, multi-bank membership, only changed sectors) passed
everything: 27 + 20 + 16 checks.

## v18 — Oct 24 — green background on RX/TX too

In v17 the **RX/TX** box at the end of the bar kept a black background (as did the thin lines between
the bar's segments, while receiving or transmitting).

When receiving or transmitting, the firmware takes another path (`0x01E40F1C` with argument 1 or 2):
it redraws the affected side's bar in **its own window** `(19, 140, y, y+9)` — segments and the
RX/TX text at x=127 — and sends it through `0x01E3B7CA`; in frequency mode it also redraws the big
line, sent through `0x01E41008`. Both sends now go through the send hook, with the same guard: only a
window inside the highlighted side's area turns green. The send at `0x01E3B7CA` sits inside a
conditional block (`if (r8 != 0)`); checked in the final file that the block prefix is intact and
only the call target changed.

No new code (1732 B), 16 sites at the send and 2 at the clear, reproducible build, no jump into a
patch. **Not yet tested on the radio.**

## v17 — Oct 24 — whole block green, no black strips, darker green ✅

**Tested on the radio:** "it turned out excellent, it's perfect" — it switches sides, gets the right
colour, no black strips. Only the RX/TX box still had a black background (v18).

Closes the thin black strips left in v16 (between the name and the frequency, and between the
frequency and the bar), mapped and checked pixel for pixel against the photos.

- **Where they came from:** no window draws there, so the send hook had nothing to recolour, and
  the strip kept whatever the screen clear left: black.
- **What clears:** `0x01E3ACBA(2)` fills the highlighted side's area with black (72 rows from 16
  with A, from 56 with B), then the other side's, and draws **the white separator line** (88 or
  55). It is the same clear the side switch performs, so no stale green from one layout ever
  survives into the other.
- **The fix:** a hook on the two calls that clear and redraw the main screen (`0x01E3C294` — boot,
  leaving the menu and 14 more callers — and `0x01E40F66`, the side switch) lets the clear run and
  fills the highlighted side's area with green. The windows come on top, and the gaps stay green.
  Entering the menu clears the same way but through another call, and is untouched; so is the
  redraw without a clear (scan's, for instance), so nothing flickers.
- **Darker green:** `0x02C0` (#005900), between v16's `0x0380` (too light on the whole block) and
  `0x0280` (which had been asked to be lighter).
- **Room:** firmware routines are now called through absolute symbols (`FIRMWARE()`), and each call
  shrank from 8 to 4 bytes — same target. The code is at 1732 of the 1782 bytes, with the new hook
  inside. The new check `4g` recomputes the target of every generated `call`/`goto` and rejects any
  that is neither an internal function nor a declared routine; tested by removing one declaration,
  it failed as it should.

Reproducible build, no jump into a patch, 16/16 sites in all six images. **Not yet tested on the
radio.**

## v16 — Oct 24 — window addresses fixed ✅ **highlight working**

**Tested on the radio:** the selected side's block turns green — `H` line, name, frequency, number
and bar — and when switching sides the previous one goes back to black and the new one turns green.
**Thin black strips** remain where no window draws (between the name and the frequency, and between
the frequency and the bar); the photos match what the map predicted pixel for pixel — see
[screen-map.md](screen-map.md), section 8.

**v15 painted nothing because of an address conversion mistake**, not a logic one. `llvm-objdump`
prints immediates in decimal: `janela()` stores y0 at `b[83466]`, y1 at `b[83470]` and the bytes per
row at `h[83472]` — which are **`0x1460A`, `0x1460E` and `0x14610`**. They had been converted as
`0x145CA`, `0x145CE` and `0x145D0` (0x40 short; the mistake came from the previous session's map), so
the hook read three unrelated variables. When "checking" the generated code I compared the hex label
with what I believed, instead of comparing 83402 with 83466.

How it was found: with v15 confirmed on the radio (no `ZONE` in the menu), every link was proven
again — the 13 sites in **all six** images of the shipped file, the guard (the stock bar only shows
because `layout()` returns 2 there), 16-bit access to video RAM (`0x01E6C18E` writes 32 bits inside
it), the DMA (`0x01E38540` reads from `0x10190` and waits for completion). What was left was the
address, and in decimal the mistake was obvious.

Changes:

- `JAN_Y0`, `JAN_Y1` and `JAN_LINHA` with the right addresses, annotated with the decimal.
- **New check (`4f`)**: the build reads the addresses from `janela()`'s own instructions in the stock
  firmware (`0x01E3AD3E`, `0x01E3AD5E`, `0x01E3AD6E`, `0x01E3AD86`) and compares them with the
  `#define`s. Tested with v15's wrong value: it fails.
- **14th site**: the highlighted side's info line (`0x01E3B2C4`, the one with the `H`), y 18 with A
  and 57 with B. It is only drawn for the expanded side, so it always comes out green.

Same size (1776 B), reproducible build, no jump into a patch, 14/14 sites in all six images.

## v15 — Oct 24 — highlight at the LCD send ⚠️ **painted nothing**

**Tested on the radio:** all black, the bar included when changing channel. Cause in v16, above:
three addresses converted wrongly from decimal. The design below (hook at the send, guard through
`layout()`) still stands in v16.


Done after mapping the entire drawing path, which explained with addresses both symptoms the user
reported on v14 (details in [screen-map.md](screen-map.md)):

1. **"the green only shows in the signal bar background, and only when changing channel with the
   arrow"** — v14's three hooks were all on **partial updates**. The full redraw, which is what runs
   when switching A to B, goes through `0x01E3C132` (the three bar rows) and the block drawer
   `0x01E3BD04`, which **had no hook at all**.
2. **"the whole box is still black"** — text backgrounds do not come from `janela()`: each text
   routine fills every character cell with its own hard-coded black (the `r8 = 0` at `0x01E3BAD8`,
   passed to the glyph drawer `0x01E388F4` and the gap fill `0x01E38978`). Swapping the window
   colour was never going to paint the block.

The hook now goes into **the send** (`envia()`, `0x01E3ADBC`) and replaces every black pixel of the
window with green before it reaches the LCD: window background, character cells and gaps all turn
green at once, while anything with a colour of its own (white text, bar segments) passes untouched.
There are **13 sites**: every `envia()` call inside the six functions that draw the two-VFO screen —
a 4-byte `call` replaced in place, so the code does not grow.

Which block is highlighted comes from **the firmware's own routine** (`0x01E3B1A2`), the same one it
uses to place every row: it returns 2 with A highlighted, 1 with B, and **0 when this is not the
two-VFO screen** (the single-VFO one writes 4 into `b[0x1533D]`). That is what keeps the green off
the other screens, which use these same functions. The `y` thresholds (89 and 57) come from three
firmware tables and have wide margins: 74 against 91 with A highlighted, 41 against 69 with B.

The hook **draws nothing** — it only reads the bounds `janela()` already stored and writes inside the
buffer, clamped to its size; v13's overflow is impossible by construction. Verified in the generated
machine code at `0x01E963FA`, instruction by instruction.

New code: 1776 B of the 1782 available. The build ends in `RESULTADO: tudo certo`, is reproducible
(same sha256 after deleting `build/`) and no jump lands inside a patch.

## v14 — Oct 24 — highlight redone from the screen map ⚠️ **did not work**

It fixed both v13 defects, each found in the disassembly:

1. **The reset** was a video-buffer overflow. v13 painted the whole block in a single call — 71
   full-width rows = **22,720 bytes** — and the buffer at 0x10190 holds **10,240** (one 160×32
   band). `janela()` has no size guard (loop at `0x01E3AD8E`), so the excess landed on the text
   buffer, the global base and the redraw flags.
2. **The highlight not showing** was the wrong variable: `tx_ab` (`0x14B84`, the setting stored in
   data flash) instead of **`b[0x15E04]`**, the screen state the firmware itself consults.

The hook moved into **three windows** (name+frequency at `0x01E4B270`, signal bar at `0x01E41468`,
channel number at `0x01E4B314`), without touching the geometry.

**Tested on the radio:** it boots, switches A to B and **no longer resets** — the overflow really was
fixed, and that part is settled. But the highlight was still wrong: green only in the signal bar
background and only when changing channel with the arrow.

**A method mistake worth recording:** those three sites were picked from a *partial* survey of the
screen, which stopped at the partial updates and never reached the full redraw or the text routines.
Both causes are in v15, above.

## v13 — Oct 24 — selected-VFO highlight ⚠️ **known problem**

- **Highlight of the selected VFO block** (hook at `0x01E41468`, the call that paints the signal
  bar row).
- **`ZONE` and `Zone Or Channel` removed** from the menu (by request), giving back 56 bytes of
  table.
- `setor_livre` dropped from the cable write path (the area is provably exclusive already).

**Observed during testing, not yet fixed:**

1. The radio boots and switches from A to B normally.
2. The highlight **does not show**: both sides stay black.
3. **The radio resets** when changing channel with the arrow on the highlighted side — same
   behaviour on A and on B.

The cause has **not been determined**. The survey of which routine draws each part of the main
screen is in [mapa-da-tela.md](mapa-da-tela.md); until that closes with evidence from the
disassembly, there is no v14.

**Fixed in v14** (above). v12 remains the last version actually tested on the radio.

## v12 — Oct 24 — 24-bit cable, read and write

- Extended cable command: **bit 7 of the size byte** turns on a 24-bit address, 64-byte block.
  Reads reach the whole chip; writes only `0xC1000..0xD1FFF`.
- A single hook serves both commands, because `'R'` and `'W'` call **the same routine**
  (`0x01E43970`, 0 reads / 1 writes).
- Validated on the radio: `BNK3` read at `0xC1000`, writes verified byte for byte, the guard
  refusing an address outside the area, and the user's own banks read back with matching
  checksums.
- Space: record markers became two `u16`s compared in two accesses, and the marker is now derived
  from the address — 70 bytes saved, stored format unchanged.

**It fixed two mistakes from v11:** the cable packet base is `0x16671` (not `0x16691`), and the
handler fires with **4** bytes, so a 5-byte packet cannot work — the extra byte also poisoned the
next command.

## v11 — Oct 24 — first attempt at extended reads ❌

Five-byte packet. It did not work (see above) and jammed the cable until the radio was
power-cycled.

## v10 — Oct 24 — leaving the text editor

Saving a bank name left the keyboard's `abc` stuck in the menu header: the two calls `CH-NAME`
makes when it finishes (`0x01E3ACBA(2)` and `0x01E3C7AA`) were missing.

## v9 — Oct 24 — signal bar during scan

While scanning, the bar froze at the level of the last channel found and the channel number drifted
from the name: the call that advances the channel (`0x01E46EB6`) never marked the screen dirty.

## v8 — Oct 23 — banks moved to `0xC1000` (end of the white band)

The boot image lives at `0x10000` — exactly where the banks were being written. Erasing the header
sector and bank 1's sector ate the top lines of the logo (erased flash reads 0xFF, which is white
in 16 bits). Banks moved to `0xC1000`, plus the `setor_livre` guard, which only writes into an
erased sector or one already ours.

## v7 and earlier — Oct 22–23 — the feature

- 16 banks, 12-character names, a channel in as many banks as you like, several enabled at once.
- Filter rewriting the two bitmaps the firmware already uses, without changing any firmware
  function.
- Hierarchical menu: `BANK`, `BANK NAME`, `BANK CH`, together where the zone entries were.
- Menu arrows inverted and 25 English strings revised (version `V1.0T`).

Mistakes fixed along the way, each with an identified cause:

| Symptom | Cause |
|---|---|
| Radio would not boot | The blob was written in plain into an area the SFC decrypts by address |
| Menu value always wrong | The menu value lives at `0x14BFC`, not at the address assumed before |
| Scratched screen, failing checksum | `0x10190` is **video memory**, not a sector buffer |
| White band on the logo | Banks written over the boot image (`0x10000`) |

## Screen diagnostics — Oct 24

Three diagnostic firmwares were flashed to find which call paints which part of the screen (the
firmware has no symbols and the coordinates change with the selected VFO):

1. **diag** — two candidate bands in different colours. Result: the signal bar row is painted by
   `0x01E41468`; the other candidate never shows on that screen.
2. **diag2** — **boot loop**. It patched 23 "colour loads" at once; at `0x01E45ED8` that register
   zeroes a 15-byte buffer before becoming the colour, and at `0x01E3C156`/`0x01E3C17E` it becomes
   the call's `x0`. Recovered through the PTT2 + shortcut key combo.
3. **diag3** — 12 sites, each checked in the disassembly. Result: on the dual-VFO screen only the
   bar row goes through `janela()`; name and frequency get their background inside the text
   routine.

**Lesson kept:** patching a constant mid-code requires looking at the register first — if the load
is not adjacent to its use, it probably serves something else too.
