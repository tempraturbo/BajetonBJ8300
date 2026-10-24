# Version history

> 🇧🇷 Português: [historico.md](historico.md)

Dates are 2026. Every version was flashed and tested on the radio; whatever broke is recorded with
its cause, because that is what stops it happening twice.

## v24 — Oct 24 — a scramble that talks to the Baofeng UV-17L ✅

- **SCRAMBLE with three options: OFF, 3400 Hz, 2900 Hz.** The chip's scramble is spectrum
  inversion, and the inversion frequency comes from a two-entry table in the firmware. The first
  entry (3400 Hz) is the one the stock ON option uses. No option could reach the second one
  (3500 Hz), and it is now **2900 Hz**, where the UV-17L inverts in mode 3. Both frequencies were
  measured from the owner's recording of a 1 kHz tone. UV-17L modes 1 and 2 do not match the
  BJ-8300. The item's get and set stay stock; only the option count, the names and 2 bytes of the
  table change. Details in [section 13 of the reverse-engineering notes](reverse-engineering.md#13-scramble).
- **CHIRP driver:** the channel scramble and the VFO A and B scrambles show and write all three
  options. In the VFO the base splits the value across two fields, and the driver writes it the
  way the radio does: the byte's high nibble and nothing else.
- **Checks:** `4k` checks the scramble table. `2e` is new and covers everything: the decrypted
  app.bin must be the same in all 6 images. Until now only the code and the menu table were
  checked in all six; the roger and scramble patches only in the first one. Both checks were
  tested against the fault they must catch (v23, without the patch, and a copy with one byte
  changed in the second image only).

Code: 2804 B of 3292. Reproducible build, zero branches into patches, all three tests passing (6
new scramble checks in real CHIRP). **Tested on the radio** (Oct 24): with the scramble at 2900 Hz,
the BJ-8300 and the UV-17L in mode 3 understand each other.

## v23 — Oct 24 — BEEP 2 at the mp3's speed ✅

In v22 BEEP 2 worked, but took 0.8 to 1 s, against ~0.4 s in the mp3. The cause is in the firmware:
the wait routine the roger uses (`0x01E36286`) counts in **2 ms units**, not ms — the timer
interrupts every 48000 cycles of 24 MHz (2 ms, `0x01E3E44C`) and the wait runs until the counter
advances twice what was asked. v22's `100` gave 200 ms per tone. Each tone now waits 50 units =
**100 ms**, and the whole BEEP 2 takes ~0.4 s. (The stock BEEP, with 40 units, has always been 80 ms
per tone; it did not change.)

Only the wait value changes; code and hooks are v22's. **Tested on the radio** (Oct 24): it works.

## v22 — Oct 24 — BEEP 2 roger, and the CHIRP driver in a single session

- **ROGER with four options: OFF, BEEP 1, BEEP 2, TONE1200.** BEEP 1 is the stock BEEP (1000 Hz
  and 850 Hz, 80 ms each); **BEEP 2** plays the four tones of the owner's mp3 — **1125, 1857, 1359
  and 931 Hz, 100 ms each** —, while transmitting, right before the radio goes back to receive. The
  stored values stay the stock ones (BEEP 1, TONE1200 = 2) and BEEP 2 is 3, which fits in the
  setting's 2 bits; the menu swaps the order. The four tones play at the same volume (in the mp3 the
  second one is quieter). Details in [section 12 of the reverse-engineering notes](reverse-engineering.md#12-roger).
- **CHIRP driver:** reading failed on the radio with "Radio returned unknown identification
  string". The driver read channels and settings in one session, left with `E` and opened another
  for the banks; mapped in the firmware, `E` answers with an ACK that CHIRP does not read, and the
  session only closes on a loop turn with no byte arriving — the second session's `PROGRAM…` was
  dropped and CHIRP read the leftover ACK. Now it is **a single session** for everything, and on
  upload the firmware check comes before writing anything (nothing is written to a stock radio).
  The driver also shows and writes BEEP 2. The test's simulated radio now behaves like the firmware
  on `E`, and with it the old driver fails with exactly the message the radio showed.

Code: 2764 B of 3292. Reproducible build, zero branches into patches, all three tests passing (6 new
roger checks in real CHIRP, 4 new in the clone test). **On the radio:** BEEP 2 worked, but with
200 ms per tone instead of 100 (fixed in v23).

## v21 — Oct 24 — v20 without the boot loop ✅

Same content as v20 (spectrum 2× faster, 10 kHz step, 320/640 points, peak hold, green `0x0240`),
with the code written where the linker linked it. Code and hooks are byte-for-byte v20's; only the
block's place in flash changes.

**Why v20 boot-looped:** the linker aligns the code section to the largest alignment it finds
inside it, and v20 brought the pointer tables for the new labels (alignment 4): the code came out
linked for `0x01E96124`, but the build script wrote it at a fixed address, `0x01E96122`. Everything
sat 2 bytes off, and the first hook to run — the end-of-startup one, right after the logo — jumped
into the middle of an instruction. No check caught it because all of them used the same fixed
address on both sides.

**What prevents it now:** the code starts at the first multiple-of-4 address (`0x01E96124`);
`montar.py` reads the code address from the ELF and refuses to build if it is not where it writes;
and `conferir.py` gained `2c` (linked address = written address) and `2d`, end to end: each of the
33 firmware branches into the new code is followed into flash, and the bytes actually there,
decrypted, in all 6 images, must be the ELF's. Run against v20, both fail — `2d` points exactly at
the startup hook.

Code: 2558 B of 3292. Reproducible build, zero branches into patches, all three tests passing.
**Tested on the radio** (Oct 24): "worked perfectly"; the sweep got about 1 s faster.

## v20 — Oct 24 — faster spectrum, 10 kHz, 320/640 points, peak hold, darker green ❌ **boot loop, withdrawn**

- **Spectrum twice as fast:** each loop turn now measures one point and already tunes the next,
  instead of spending one turn on each. The chip still gets a whole turn between tuning and
  measuring, as on stock firmware, so the reading is the same: 160 points in ≈ 2.5 s (was 5 s).
- **10 kHz step:** OK goes 6.25 → 8.33 → **10** → 12.5 → 20 → 25 kHz (same chip filter as 8.33 and
  12.5).
- **320 and 640 points:** # goes 160 → 80 → 40 → 20 → **640** → **320**. The graph still has 160
  columns; each column measures 2 or 4 frequencies and shows the highest, and stopping on a signal
  happens at its exact frequency. At 25 kHz, 640 points cover 16 MHz; a sweep takes ≈ 5 s (320) and
  ≈ 10 s (640).
- **Peak hold:** the bar rises at once and, when the signal goes away, comes down half the way on
  each sweep. A short signal stays marked for a few sweeps and the noise stops flickering. Changing
  the window (typing a frequency, OK, #, entering the spectrum) clears the graph.
- **Darker highlight green:** `0x0240` (#004900, step 18), instead of `0x02C0` (#005900, 22) from
  v17 to v19.
- The new indices (640, 320 and 10 kHz) go at the end of the configuration the radio saves, so the
  stock ones keep their value. **Before going back to an earlier version**, leave # on 160/80/40/20
  and the step off 10 kHz: earlier versions do not know the new ones and would show garbage.

Two items of the plan came out differently from the proposal, for a concrete reason:
- **"Redraw the peak text only when it changes"** had nothing to gain: once mapped, the dBm text is
  drawn only once per sweep and when the sweep stops — not on every column, as I had said.
- **"Average several readings per point"** did not go in as back-to-back reads: without knowing how
  often the chip refreshes its RSSI, reading twice in a row may return the same number and clean
  nothing, and it would cost time. The peak's halfway descent is already an average between the new
  sweep and the previous one. "Always 160 points" is what # on 160, 320 and 640 already gives; with
  20/40/80 the steps are what measuring fewer points means.

Full map in [section 11 of the reverse-engineering notes](reverse-engineering.md#11-spectrum). Code:
2558 B of 3294. Reproducible build, zero branches into patches, all three tests passing. **On the radio:
boot loop** — the logo shows and the radio restarts in under 1 s, in a loop. Cause and fix in v21.

## v19 — Oct 24 — the arrow continues the spectrum, and the menu table in place ✅

- **Spectrum:** stopped on a found signal, the arrow (up or down) **continues the sweep** and, that
  time, leaves the threshold alone. While sweeping, the arrow does what it always did (raises or
  lowers the threshold). Continuing is exactly the resume the firmware itself does when the signal
  drops (`0x01E4AEFC`: closes the audio and moves to the next point), and the build checks that
  against the stock bytes (check `4i`). The hook goes into a 4-byte instruction of the spectrum key
  handler (`0x01E4BFBE`); the registers the new call clobbers were checked dead there on every path.
  Details in [section 11 of the reverse-engineering notes](reverse-engineering.md#11-spectrum).
  That key press now beeps like the keys with no function in the spectrum (code 0), not like the
  arrow. If the signal is strong, the sweep may stop again on the neighbouring point of the same
  signal: just press again.
- **Space:** the new menu table is now written **where the original was** (its 54 slots, counting
  the zero item at the end, are exactly the new table's 54 items), instead of taking 1512 bytes of
  the free area. Code now has 3294 bytes available, up from 1782. Proven first that nothing else
  reads the original table; its pointer stays as it was. Code and table came out byte-for-byte
  equal to v18's, only the table in another place.

Code: 1792 B of 3294. Reproducible build, zero branches into patches, all three tests passing.
**Tested on the radio** (Oct 24): it stops on the signal, the arrow continues without changing the threshold, and the beep is fine.

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
