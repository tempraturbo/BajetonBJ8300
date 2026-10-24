# Version history

> 🇧🇷 Português: [historico.md](historico.md)

Dates are 2026. Every version was flashed and tested on the radio; whatever broke is recorded with
its cause, because that is what stops it happening twice.

## v14 — Oct 24 — highlight redone from the screen map

Done after mapping the whole screen ([screen-map.md](screen-map.md)), which explained both v13
defects with addresses:

1. **The reset** was a video-buffer overflow. v13 painted the whole block in a single call — 71
   full-width rows = **22,720 bytes** — and the buffer at 0x10190 holds **10,240** (one 160×32
   band). `janela()` has no size guard (loop at `0x01E3AD8E`), so the excess landed on the text
   buffer, the global base and the redraw flags.
2. **The highlight not showing** was the wrong variable: I used `tx_ab` (`0x14B84`, the setting
   stored in data flash) instead of **`b[0x15E04]`**, the screen state the firmware itself consults
   to place every row.

The hook now goes into the **three windows that make up a block** (name+frequency at `0x01E4B270`,
signal bar at `0x01E41468`, channel number at `0x01E4B314`) and **never touches the geometry**: it
only swaps the colour the firmware was going to fill that window with. Verified in the generated
machine code — r0 to r3 are never written — so v13's overflow is impossible by construction.

**Not yet tested on the radio.** What is proven is static: the map, the truth table of the seven
window/side combinations, and that the build ends in `RESULTADO: tudo certo`.

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
