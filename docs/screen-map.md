# Main screen map

> 🇧🇷 Português: [mapa-da-tela.md](mapa-da-tela.md)

Mapped on 2026-10-24 from the V0.12 disassembly, in two rounds: the first (v14) was incomplete and
failed on the radio, the second closed the whole drawing path. Everything here carries an address;
whatever has none is not proven, and says so.

## 1. How the firmware draws

There is no full-screen framebuffer. Drawing happens **one window at a time**:

> **Mind the addresses:** `llvm-objdump` prints immediates in **decimal** (`r4 = 83466`).
> Converting by hand has already cost a version: v15 read `0x145CA` instead of `0x1460A` (83466) —
> 0x40 short — and the highlight never painted. The addresses below were checked against the
> decimal, and the build now takes `janela()`'s straight from its own instructions (check `4f`).

```
janela(x0, x1, y0, y1, clear, colour)   0x01E3AD34
    b[0x14608] = x0   b[0x1460A] = y0   b[0x1460C] = x1   b[0x1460E] = y1
    h[0x14610] = (x1 - x0) * 2          (bytes per row)
    with clear != 0, fills the buffer with colour
<text, bar, icons are drawn inside it>
envia()                                 0x01E3ADBC
    reads the same globals and pushes the buffer to the LCD; takes no arguments
```

The buffer is video memory at **0x10190**, **10240 bytes** — one **160×32** band. Each pixel is
RGB565 stored **high byte first** (`0x01E3AD90`: `b[p] = colour >> 8`, `b[p+1] = colour`), unlike
the CPU, which is little-endian: writing a `u16` into the buffer means byte-swapping it.

And the trap that took v13 down:

> **`janela()` has no size guard.** It computes `(x1-x0) * (y1-y0) * 2` and starts writing from
> 0x10190 (loop at `0x01E3AD8E`). A window larger than 10240 bytes tramples whatever follows: the
> text buffer (0x129B0), the global base (0x14680), the redraw flags (0x15367) and the channel
> bitmaps. The result is a reset.

That is why no stock call exceeds 32 full-width rows — the largest is exactly 160×32, at
`0x01E4A78A`.

## 2. Text backgrounds do NOT come from `janela()`

Each text routine fills every character cell with its own background colour, **hard-coded black**.
In the name drawer (`0x01E3BACE`) that is the `r8 = 0` at `0x01E3BAD8`, passed as the background
both to the glyph drawer (`0x01E388F4`, sixth argument) and to the inter-character gap fill
(`0x01E38978`). The frequency routine (`0x01E3BB7A`) does the same with the `r8 = r3` it receives.

The practical consequence, and v14's second defect: **changing the window colour does not paint the
block**. The window background only shows at the edge; name and frequency stay in black boxes.

## 3. Which block is highlighted: `0x01E3B1A2`, not a loose variable

`b[0x15E04]` is the selected side (**0 = VFO A, non-zero = VFO B**). Not to be confused with
`tx_ab` (`0x14B84`), the data-flash setting `0x9018` mirrored in RAM — using the setting instead of
the screen state was one of v13's two mistakes.

But what decides the **layout** is the routine `0x01E3B1A2(side)`, which the firmware itself calls
from the block drawer and from the complement. It:

1. reads the screen mode at `b[0x1533D]` and returns **0** unless it is 0, 3 or 5;
2. otherwise returns, according to the side asked and `b[0x15E04]`:

| | `layout(0)` (side A) | `layout(1)` (side B) |
|---|---|---|
| A selected | **2** (expanded) | 3 (compact) |
| B selected | 0 (compact) | **1** (expanded) |

Called with the selected side — `layout(b[0x15E04] != 0)` — it returns **2 with A highlighted, 1
with B, and 0 when this is not the two-VFO screen**. It is the only source of truth: the single-VFO
screen writes **4** into `b[0x1533D]` (at `0x01E4CDC4`) and uses the same drawing functions.

The block drawer uses `layout(side) - 1` as an index, and `(3 >> index) & 1` tells it whether that
block is the expanded one.

## 4. Drawing paths of the two-VFO screen

```
0x01E3C278                full redraw (this is what runs when you switch A to B)
  ├─ 0x01E3B05C           status bar (y 0..15) — its own path, no hook of ours
  └─ 0x01E3C132(0)        the screen
       ├─ janela 0x01E3C176   bar row, A compact    (0,160,41,55)
       ├─ janela 0x01E3C19E   bar row, A expanded   (0,160,74,88)
       ├─ 0x01E3B2C4(side, 0) info line (the `H`)   (0,160,y,y+12), y 18 (A) / 57 (B)
       ├─ janela 0x01E3C1D0   bar row, B            (0,160,114,128)
       ├─ 0x01E3BD04(0, side, mode)   block: frequency and name line
       │    └─ windows 0x01E3BE20 (0,132,y,y+12)  0x01E3BE58 (0,160,y,y+18)
       │               0x01E3BF50 / 0x01E3BFA6 / 0x01E3C02C (0,160,y,y+24)
       └─ 0x01E3C06E(0, side)         complement: right-hand strip (132,160,y,y+22)

partial updates (run on channel change; the bar one also as the signal varies)
  0x01E4B29C   channel number (127,159,y,y+9) + bar via 0x01E413BE (0,160,y,y+14)
  0x01E4B1DE   name and frequency (7,160,y,y+24)
  0x01E453E0   redraws only the highlighted block: info line, block and complement
```

The info line (`0x01E3B2C4`) is only drawn for the **expanded** side: with `layout()` at 0 or 3 it
skips both window and send (`0x01E3B326`/`0x01E3B32A`).

`0x01E3C132` with argument **1** is the **single**-VFO screen: it skips the three bar rows and puts
the block at y 32/36. All eleven callers pass 0 or 1 — never any other value.

## 5. The geometry, by layout index

The `y` values come from two tables in the block drawer (`0x01E3BD40` = `0x5D1E45` and
`0x01E3BD4A` = `0x5D3A61`, one byte per index) and a third in the complement (`0x01E3C0BE` =
`0x5B385F`):

| index | which | frequency (24 rows) | name (12/18 rows) | right strip | bar | number |
|---|---|---|---|---|---|---|
| 1 | A expanded | 30 | 58 | 56 | 74 | 62 |
| 0 | B expanded | 69 | 97 | 95 | 114 | 101 |
| 2 | B compact | 93 | 93 | 91 | 114 | — |
| 255 | A compact | 21 | 21 | 19 | 41 | — |

Grouped by which side is highlighted:

| | `y` of the highlighted side | `y` of the other side | separating threshold |
|---|---|---|---|
| A highlighted | 18, 30, 56, 58, 62, 74 | 91, 93, 114 | `y < 89` (74 against 91) |
| B highlighted | 57, 69, 95, 97, 101, 114 | 19, 21, 41 | `y >= 57` (41 against 57) |

The info line at y 18/57 comes from `0x01E3B32E` (57 with `layout()` at 1, otherwise 18).

No window exceeds the buffer: 160×24×2 = 7680 B, 160×14×2 = 4480 B, 32×9×2 = 576 B.

## 6. Why v14 did not work

The user reported exactly two symptoms, and each has a cause with an address:

1. **"the green only shows in the signal bar background, and only when changing channel with the
   arrow"** — v14's three hooks (`0x01E4B270`, `0x01E41468`, `0x01E4B314`) are all on **partial
   updates**. The full redraw, which is what runs when switching A to B, goes through
   `0x01E3C132` and `0x01E3BD04`, which had no hook at all. Of the three, the only one that paints
   visible background area is the bar — hence green showing only there and only on channel change.
2. **"the whole box is still black"** — even with the hook in the right place, swapping the window
   colour would not be enough: name and frequency get their background from the text routines
   (section 2).

## 7. How the highlight works now

The hook goes into **the send** (`envia()`, `0x01E3ADBC`) and replaces **every black pixel of the
window** with green before it reaches the LCD. That makes the window background, every character
cell and every inter-character gap green in one go, while anything with a colour of its own (white
text, bar segments, the separator line) passes through untouched.

```c
void gancho_envia(void)
{
    unsigned y0 = JAN_Y0, lado = LAYOUT(LADO_B != 0);

    if (lado == 2 ? y0 < 89 : lado == 1 && y0 >= 57) {
        unsigned n = (JAN_LINHA >> 1) * (JAN_Y1 - y0);
        volatile u16 *p = VIDEO;

        if (n > PIXELS)             /* never past the buffer */
            n = PIXELS;
        while (n--) {
            if (*p == 0)
                *p = COR_BUF;
            p++;
        }
    }
    ENVIA();
}
```

There are **14 sites**: every `envia()` call inside the seven functions that draw the two-VFO screen
(section 4), replaced in place — a 4-byte `call`, so **the code does not grow**. The firmware's only
send by jump (`goto` at `0x01E4544E`, at the end of `0x01E453E0`) merely re-sends the complement's
last window, which the hook at `0x01E3C12A` has already processed. The status bar, the
menus and pop-up notices have their own paths and stay black. And the hook still asks the
firmware's own routine (section 3) whether this really is the two-VFO screen, so the other screens
that go through these same functions — the single-VFO one first of all — are never painted.

The hook **draws nothing**: it only reads the bounds `janela()` already stored and writes inside the
buffer, clamped to its size. v13's overflow is impossible by construction.

The colour lives in `COR_REALCE`, in `firmware/src/bancos.c` — a single line. The current value is
`0x0380` (#007100). The buffer stores the high byte first, so the hook uses `COR_BUF`, the same
value byte-swapped (`0x8003`) — verified in the generated machine code (`0x01E96442`).

## 8. What is left

### Gaps no window covers (confirmed on the radio with v16)

The hook only recolours what is sent to the LCD, and **nothing is ever sent** in these strips — they
keep whatever the screen had, black. Computed from the table in section 5 and checked in the photos:

| | A highlighted | B highlighted |
|---|---|---|
| between the big name and the frequency | y 54..56 full width; 56..58 at x 0..132 | y 93..95 full width; 95..97 at x 0..132 |
| between the frequency and the bar | y 70..74 at x 0..132 | y 109..114 at x 0..132 |

Right of x=132 there is no gap because the right-hand strip (56..78 with A, 95..117 with B) covers
the whole height. Closing the gaps means **drawing** extra strips (there is nothing to recolour), and
before that it still has to be mapped: what draws the white separator line (so as not to paint over
it), whether switching sides clears the whole screen (otherwise a gap's green would be left behind in
the other layout) and where to find room — the code takes 1776 of the 1782 bytes.

### Still unmapped

- The separator line between the blocks: it comes from none of the windows in section 5. Being
  light-coloured, the hook would not change it anyway. (The `H` indicator is now mapped: it lives in
  the info line, section 4.)
- Other screens (menu, FM, scan) use the same routines with different coordinates. The hook does not
  paint them, thanks to the guard in section 3, but the highlight was not designed for them either.
- The name window starts at x=7 on the partial update (`0x01E4B270`) and at x=0 on the full redraw
  (`0x01E3BF50`): a 7-pixel strip is left that only the full redraw repaints.
