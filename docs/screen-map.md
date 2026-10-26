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
both to the glyph drawer (`0x01E388EC`, sixth argument) and to the inter-character gap fill
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
0x01E3C290                clear and redraw: boot, leaving the menu and 14 more callers
  ├─ 0x01E3ACBA(2)        clear (see below)               ← gancho_limpa at 0x01E3C294
  └─ 0x01E3C278           full redraw
0x01E4DC9E                A/B key: switches side (0x01E3A812) and calls 0x01E41486, which goes to
  └─ 0x01E40F1C(0)        ├─ 0x01E3ACBA(2) clear        ← gancho_limpa at 0x01E40F66
                          └─ 0x01E3C132(0) the screen

0x01E3C278                full redraw without clearing (also called directly, during scan)
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

receive and transmit (0x01E40F1C with argument 1 or 2)
  0x01E3B592(1, 2, side, 1)   bar of the side receiving/transmitting in ITS OWN WINDOW
                              (19,140,y,y+9), segments and the RX/TX text at x=127 → send 0x01E3B7CA
  window 0x01E40FD4           frequency mode only: the big line (0,160,y,y+24) → send 0x01E41008
```

The info line (`0x01E3B2C4`) is only drawn for the **expanded** side: with `layout()` at 0 or 3 it
skips both window and send (`0x01E3B326`/`0x01E3B32A`).

**The clear `0x01E3ACBA(2)` draws the separator line.** It fills whole rows with
`0x01E3AC68(y, height, colour)`, which sends straight to the LCD, bypassing `janela()`/`envia()`:

| | A highlighted | B highlighted |
|---|---|---|
| highlighted side's area (black) | y 16, 72 rows (16..87) | y 56, 72 rows (56..127) |
| other side's area (black) | y 89, 39 rows | y 16, 39 rows |
| **separator line** (white) | y **88** | y **55** |

The same clear runs when entering the menu (`0x01E4CBAE`), so the hook does not go into it, but
into the two calls that clear **and redraw the main screen**.

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

There are two hooks, sharing the same arithmetic: the highlighted side's area is the clear's, 72
rows starting at 16 (A) or 56 (B).

```c
static unsigned area_realce(void)         /* 16 with A, 56 with B, 0 off this screen */
{
    unsigned lado = LAYOUT(LADO_B);        /* 2 = A highlighted, 1 = B, 0 = other screen */
    return lado - 1 <= 1 ? 96 - 40 * lado : 0;
}
```

**1. At the send** (`envia()`, `0x01E3ADBC`): replaces **every black pixel of the window** with
green before it reaches the LCD. That makes the window background, every character
cell and every inter-character gap green in one go, while anything with a colour of its own (white
text, bar segments, the separator line) passes through untouched.

```c
void gancho_envia(void)
{
    unsigned y0 = JAN_Y0, inicio = area_realce();

    if (inicio && y0 - inicio < AREA) {    /* unsigned: y0 above the start becomes huge */
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

There are **16 sites**: every `envia()` call inside the functions that draw the two-VFO screen,
including those on the receive and transmit path (section 4), replaced in place — a 4-byte `call`, so **the code does not grow**. The firmware's only
send by jump (`goto` at `0x01E4544E`, at the end of `0x01E453E0`) merely re-sends the complement's
last window, which the hook at `0x01E3C12A` has already processed. The status bar, the
menus and pop-up notices have their own paths and stay black. And the hook still asks the
firmware's own routine (section 3) whether this really is the two-VFO screen, so the other screens
that go through these same functions — the single-VFO one first of all — are never painted.

This hook **draws nothing**: it only reads the bounds `janela()` already stored and writes inside
the buffer, clamped to its size. v13's overflow is impossible by construction.

**2. At the clear** (`gancho_limpa`, at the calls `0x01E3C294` and `0x01E40F66`): lets the stock
clear run — black and the separator — and fills the highlighted side's area with green. The windows
come on top, recoloured by the first hook, and **the gaps between them stay green**. Nothing else
paints those gaps, so they stay green until the next clear, which is always followed by this same
fill. The separator (88 or 55) lies outside the area and is untouched.

```c
void gancho_limpa(unsigned modo)
{
    unsigned inicio;

    LIMPA(modo);
    if ((inicio = area_realce()) != 0)
        ENCHE(inicio, AREA, COR_REALCE);
}
```

The colour lives in `COR_REALCE`, in `firmware/src/bancos.c` — a single line. The current value is
`0x0240` (#004900, step 18 of the 64 green steps, since v20; v17 to v19 used `0x02C0`). Video memory stores the high byte first, so the send hook uses `COR_BUF`, the
same value byte-swapped (`0x4002`); the fill takes the plain colour, because the routine splits the
bytes itself. Both verified in the generated machine code.

## 8. Gaps no window covers

The send hook only recolours what is sent to the LCD, and **nothing is ever sent** in these strips.
In v16 they kept whatever the clear left, black; since v17 the clear hook leaves them green.
Computed from the table in section 5 and checked in the v16 photos:

| | A highlighted | B highlighted |
|---|---|---|
| between the big name and the frequency | y 54..56 full width; 56..58 at x 0..132 | y 93..95 full width; 95..97 at x 0..132 |
| between the frequency and the bar | y 70..74 at x 0..132 | y 109..114 at x 0..132 |

Right of x=132 there is no gap because the right-hand strip (56..78 with A, 95..117 with B) covers
the whole height. Also inside the area, and now green: rows 16..17 between the status bar and the
`H` line (with A) and row 56 between the separator and the `H` line (with B).

## 9. Still unmapped

- Row 15, between the status bar (window 0..15, rows 0..14) and A's area (16): nothing draws it, not
  even the clear. It stays black, as a border under the status bar.
- Other screens (menu, FM, scan) use the same routines with different coordinates. The hook does not
  paint them, thanks to the guard in section 3, but the highlight was not designed for them either.
- The name window starts at x=7 on the partial update (`0x01E4B270`) and at x=0 on the full redraw
  (`0x01E3BF50`): a 7-pixel strip is left that only the full redraw repaints.

## 10. Text: `escreve()`, the fonts and the glyph

Mapped on 2026-10-25 for the new theme, which needs colored text.

```
escreve(y, x, text, size, background)                0x01E3B834
    size 12, 16 or 24; x advances by each letter's width
    the letter color is fixed: r10 = 0xFFFF (0x01E3B842), white
glyph(y, x, width, height, bits, background, foreground)   0x01E388EC (14 callers)
    draws nothing with height 0, width 0 or x > 160; clips the width to 160 - x
    writes into the open window's buffer, through the cursor b[0x14604] (x) and b[0x14606] (y),
    which 0x01E38724 clamps to 159 and 127 and converts through the window (b[0x14608],
    b[0x1460A], h[0x14610]); a pixel that would fall outside the 10240 B buffer is not written
```

**The bits** come in bands of 8 rows. Each byte is a column, and bit k is row band × 8 + k, bit 0
on top. The first band takes the first `width` bytes and the next band the following ones (loop at
`0x01E38944..0x01E38974`). A set bit is drawn in the foreground color, a clear one in the
background color.

**The ASCII** (32 to 126) of each size:

| Size | Letter | Bytes | From |
|---|---|---|---|
| 12 | 6 × 12 | 12 | data chip, `0x86BC0 + c × 12` (`0x01E3B9E6`) |
| 16 | 8 × 16 | 16 | data chip, `0x5B5C0 + c × 16`, and each column moves down one row (`0x01E3B8D0..0x01E3B906`) |
| 24 | 12 × 24 | 36, 48 apart | internal flash, `0x01E63B70 + (c − 32) × 48` (`0x01E3B868`) |

Sizes 12 and 16 are read by `LE_FLASH` (`0x01E390AC`), and 24 by the ROM's `memcpy`
(`0x021127A8`). Bytes from 161 up are Chinese (GB2312, two bytes), through other paths.

To write in color it is enough to repeat the ASCII path with any foreground: `janela()`,
`glyph()` letter by letter, and `envia()`. That is what `escreve_cor` in `bancos.c` does, in
the 12 px font (v34, the spectrum's peak line).

## 11. The data of the two-VFO screen

Mapped on 2026-10-25 in the stock disassembly, for screen B (section 12). Global base `0x14680`.

**Each side** (A = 0, B = 1) has a record at `0x14DC4 + side × 31` (`0x01E3BD88..0x01E3BD8E`):

| Field | Where | Note |
|---|---|---|
| RX frequency | u32 at +0 | in 10 Hz; `%03d.%05d` with /100000 |
| tone type | `b[+4]` | 0 none, 1 CT, other DCS |
| TX frequency | u32 at +5 | the one the big line shows while transmitting (`0x01E40FE6`) |
| shift | `b[+10]` | 1 "+", 2 "−" |
| bandwidth | `b[+11]` | non-zero: "N" |
| AM | `b[+12]` | 1 = AM, from 18 to 999.99999 MHz; 108 to 135.99999 MHz is always AM (`0x01E3B438..0x01E3B47E`) |
| SC | `b[+13]` | non-zero and not AM: "SC" (meaning unproven) |
| SCR | `b[+14] & 3` | non-zero: "SCR" (meaning unproven) |
| name | 12 B at +15 | stops at 0 or 0xFF (`0x01E3BDCC..0x01E3BDE6`); empty: "Unnamed CH" |

Outside the record:

| Data | Where |
|---|---|
| highlighted side | `b[0x15E04]` |
| work mode | nibble of `b[0x14B86]` (A low, B high): 1 = channel |
| display | `b[0x14B79]` (A), `b[0x14B7A]` (B): 0 name, 1 frequency, 2 number |
| channel number | `h[0x15E0C]` (A), `h[0x15E10]` (B), base 0 |
| power | `b[0x15E30 + side]`: 0 H, 1 M, 2 L |
| R/T | `b[0x15E28 + side]`: 1 R, 2 T (meaning unproven) |
| language | `b[0x14B74]`: 1 = Chinese |
| zone | with `b[0x14BD4]` = 0 it browses by zone; the zone is `b[0x14BDB + side]` |
| TX state | `b[0x14685]`: 2 = transmitting (`0x01E40F1C`) |
| typed digits | how many in `b[0x14A35]`, in ASCII from `0x14A3A` (`0x01E4B29C`, `0x01E4B1DE`) |

**Top bar** (`0x01E3B05C`, background `0x2965`). Every icon is always drawn, and an off one gets a
foreground in the background color:

| Icon | Turned on by |
|---|---|
| VOX | `b[0x14B6E]` |
| DW | `b[0x14B70]` (TDR) |
| MO | `b[0x15370]` |
| musical note | `b[0x14B75]`, the DTMFST item |
| waves | `b[0x14BDE]`, the NOISE REDUCTION item (its get/set, `0x01E3D0E0`/`0x01E3D0F4`, is the only writer) |
| Bluetooth | `b[0x14B89]` = 1 |
| padlock | `b[0x14B87]` |
| voltage | `b[0x149BC] × 444 / 1000`, in tenths of a volt (`0x01E3AFBE`) |
| battery | level from `[0x149C0]`: above 6 counts as 3, 4 to 6 as 4, otherwise itself (`0x01E3B03E`); 0..3 = 1..4 bars, 4 = red outline only; with 5 and 6 the loop blinks the outline (`0x01E460B2..0x01E460F2`) |

**Meter.** It is redrawn only when the signal step changes (`0x01E46166`: 5 steps, level
2 × step + 2) or by the monitor, with level 10 (`0x01E4532C`). The full redraw leaves the bars empty,
and the end of reception does not call the meter. While transmitting it is full by power
(H 10, M 5, L 2).

**Who redraws what.**

- **The whole screen:** `0x01E3C132`, through the 17 callers of `0x01E3C290`; during a scan, through `0x01E3C278` on every channel.
- **The highlighted side on a channel change:** `0x01E453E0`.
- **One side and the info line:** `0x01E413BE` and `0x01E3B2C4`. The A/B key goes through `0x01E41486`.
- **The meter:** `0x01E3B592`.
- **Transmission:** `0x01E40F1C`.
- **The numbers being typed:** `0x01E4B29C` and `0x01E4B1DE`.
- **The top bar, the battery, MO and the padlock:** each has its own routine and window.

## 12. Screen B (v35)

Drawn by `firmware/src/tela.c`. Since v36 the code lives in place of the G.729 decoder, inside
`app.bin` ([reverse engineering, section 16](reverse-engineering.md#16-g729-and-mp3-out-v36-and-v37)); in v35 it was
in the voice space, and the loader refused to flash it. From the firmware it only uses
`janela()` and `envia()`; rectangles and letters it writes into the video buffer of the open window
itself. The whole scene is drawn in bands of up to 32 rows, and whatever falls outside the window is
clipped.

**The firmware still decides when to draw.** Thirteen routines from section 11 get, at their entry,
a `goto` to a detour in `tela.c`. The detour:

1. saves `rets` and r0..r3, with the same instructions the firmware uses (`0x01E3C290` and `0x01E51358`);
2. calls the C hook with the routine's arguments;
3. if the hook drew screen B, returns to the caller with r0..r3 untouched;
4. if not, redoes the stock instructions the `goto` covered and carries on with the routine.

Screen B only draws when `layout()` is not 0. Up to v36 it also stayed out of the Chinese language,
which the font does not have; since v37 the radio is always in English. The other screens are in
section 13. On the single-VFO screen and the spectrum, the routines run as always.

| Routine | Hook | What screen B redraws |
|---|---|---|
| `0x01E3C132(0)`, the whole screen | `b_tela` | everything, and clears the TX state like it does |
| `0x01E3B05C`, `0x01E3B03E`, `0x01E3AE50` | `b_barra` | the top bar |
| `0x01E3AF1E(level)`, the battery | `b_bateria` | the bar, with the incoming level (the low-battery blink) |
| `0x01E3AEB0(on)`, MO | `b_monitor` | the bar, with the incoming MO |
| `0x01E453E0` | `b_evidencia` | the highlighted side |
| `0x01E4B29C`, `0x01E4B1DE` | `b_digitando` | the highlighted side with the digits; only these show them, as in stock |
| `0x01E413BE`, `0x01E3B2C4` | `b_lado` | both sides, since on the A/B key the card moves |
| `0x01E3B592`, the meter | `b_medidor` | the side's meter with the incoming level, and the bar (RX) |
| `0x01E40F1C`, transmission | `b_transmissao` | redoes its logic: writes the state before drawing, and draws the side and the bar (2) or everything (end) |

**Proven in the disassembly before flashing:**

- no branch or constant lands inside the covered bytes;
- none of the thirteen reads arguments from the caller's stack;
- they only change state in what the hook repeats (`b[0x14685]`);
- stack: the maximum depth with screen B equals stock in the three contexts it is called from (the
  UI routine `0x01E4B416`, 740 B; the signal loop `0x01E4622A`, 620 B; boot, 696 B). Screen B's
  worst path, 376 B from the meter detour, fits below the deepest path of each.

The stock screen's send and clear hooks (section 7) moved to `tela.c` too. With screen B active, the
send hook redraws from screen B the side the window belongs to, and the clear does not run, because
the whole screen follows right after. Without screen B (`layout()` at 0) both do what the stock call
did; the old green highlight, which only showed in the Chinese language, left in v37.

**PC preview.** `firmware/previa/` compiles the same `tela.c` with the RAM, `janela()` and `envia()`
imitated and saves the screen as PNG: `python -I previa/previa.py <folder>` (needs zig, from
`pip install ziglang`). The font (5×7, doubled for the frequency) lives in
`firmware/fonte/fonte_b.txt`, drawn in `#` and `.`; `scripts/fonte.py` generates the table.

**Checked in the build** (`conferir.py`): 2b, 2d and 4g as before, now with the screen sites; 4m
checks the 13 detours byte by byte (stock bytes, `goto`, detour and return); 4n, that there is no
RAM section.

## 13. The other screens (v37 to v39)

Drawn by `firmware/src/telas.c`, which `tela.c` includes (the same `tela.o`, in the free range). The
screen mode is `b[0x1533D]`: 0, 3 and 5 the two-VFO screen, 1 the menu, 2 the radio, 7 DTMF, 17 the
QT scan and 20 the spectrum.

**Each part comes in through the stock routine that draws it, never through the general
`envia()`.** Prompts and confirmations ("Sure to Reset?") go through like the stock ones, and screen
B's deep drawing only runs at those points. Where the stock routine both draws and stores state, the
hook runs it whole and silences its `envia()`, instead of repeating the logic.

| Where | How | Hook | What screen B does |
|---|---|---|---|
| `0x01E48E82`, `0x01E48ECA` (menu engine `0x01E489E0`) | `envia()` replaced | `gancho_menu_titulo`, `gancho_menu_linha` | the band of the title or of the row the engine built |
| `0x01E476E4` (signal loop) | `envia()` replaced | `gancho_menu_pisca` | nothing in the menu: all of the stock blinking goes out through it |
| `0x01E3B2C4`, the info line | detour | `b_lado` | in the menu and the QT scan, the summary of the highlighted side |
| `0x01E48720`, the editor's mode indicator | detour | `b_editor_modo` | the whole name editor |
| `0x01E4865C`, a frame straight to the LCD | detour | `b_editor_quadro` | nothing in the menu (it reads the color from the caller's stack: only the stock routine, with the stack restored, reads it) |
| `0x01E48FE0`, the editor's text line | `envia()` replaced | `gancho_editor_linha` | nothing: the indicator detour already drew the text |
| `0x01E44FB0`, the whole radio | detour | `b_radio` | the radio screen, after setting band > 4 and AM band > 3 to 0, as it does |
| `0x01E449BA`, `0x01E44B28`, `0x01E45198` | detour | `b_radio_freq`, `b_radio_nome`, `b_radio_sinal` | the frequency, the memory name and the S-meter |
| `0x01E44C68(item, value)`, the settings | detour | `b_radio_ajustes` | runs the stock one (it stores the value) and redraws; its two `envia()` calls (`0x01E44CA8`, `0x01E44FA8`) are silenced in mode 2 |
| `0x01E49078`, the typed frequency | detour | `b_radio_digita` | the mask with the digits in place of the frequency |
| `0x01E4D3F8`, "SEEK..." | `envia()` replaced | `gancho_radio_seek` | "SEEK..." in amber in place of the frequency |
| `0x01E4A7D4` (entering the radio), `0x01E3DEBE` and `0x01E3DEEA` (modulation change) | `envia()` replaced | `gancho_radio_inicio` | the wait screen, full screen; the change's "Please Wait..." (`0x01E3DF10`) becomes a nop |
| `0x01E45A82(running)`, the QT scan | detour | `b_scanqt` | the title and the state |
| `0x01E4DA88`, `0x01E4A93C` (DTMF) | `envia()` replaced | `gancho_dtmf` | the DTMF box, in place of the card of the side in use |
| `0x01E55F40` (power-on) | `envia()` replaced | `gancho_tensao` | the voltage card |
| `0x01E4CFE2`, the clear on entering the spectrum (v41) | replaced call | `gancho_espectro_limpa` | the background and the top bar |
| `0x01E45D96`, `0x01E4AFC2`, `0x01E4B04C`, `0x01E4ADFE`, `0x01E4AB2A` (spectrum) | routine replaced whole | `b_esp_freq`, `b_esp_digitos`, `b_esp_modo`, `b_esp_dbm`, `b_esp_eixo` | the frequency or the digits, AM/FM, the dBm and the ends |
| the 5 windows of the graph slices and the threshold line (`0x01E4D72E`) | replaced call | `gancho_fatia`, `gancho_limite` | the panel with the grid and the line dashed in amber |

**Spectrum** (v41). Routine map and proof in [section 11 of the reverse engineering
notes](reverse-engineering.md#11-spectrum). The top bar, battery and MO hooks draw the B bar in mode
20 too, and the meter, which in the spectrum only the MO key reaches, lights only the bar (the
factory one would draw it over the graph).

**Menu.** The engine `0x01E489E0` builds the title at `0x15494` (26 B) and three rows at `0x154AF`
(27 B each). Each row goes through `0x01E487BC(row, text, marker)`, which puts in front `02 10` (the
stored value, marker 2), two spaces (another option, 1) or nothing (0: the type-7 name and "NONE");
type 8 never has a marker. Screen B recognizes the marker by the content. The other data: item
number `b[0x14BFA]`, cursor `b[0x14C10]`, editing `b[0x14BF9]` and type `b[0x14C00]` (0 and 4 to 6
list, 2 tone with typing, 3 frequency, 7 name, 8 no marker, 13 signed number). BANK and BANK CH mark
the option that is on with a `0x01` byte at the end, which screen B paints amber.

**The blinking.** In the menu, the signal loop (`0x01E4622A`) counts `b[0x14C1A]` from 0 to 7, once
per pass. At 4 it draws, at 0 it erases. Outside editing it blinks the item number at (133, 57)
(`0x01E47062`, `0x01E47290`); while editing, the cursor row in the stock font (`0x01E472EA`,
`0x01E47332`); in the name editor, the text line with a `_` (`0x01E476B2`). All of it goes out
through the same `envia()` at `0x01E476E4`.

**The name editor** (type 7 being edited). The text is at `0x14A3A`, the length at `b[0x14A35]`, the
maximum at `b[0x14A36]` (written by the item's get) and the mode at `b[0x14A38]`: 0 `abc`, 1 `ABC`,
2 `123`. 3, pinyin, exists only in Chinese (`0x01E4A59C`). On opening, the OK handler calls the stock
clear (`0x01E49B1C`), and after every key the engine runs whole (`0x01E4A0FE` ends in
`goto 0x01E489E0`). The already-drawn mode (`b[0x14686]`) does not go back to `0xFF` on that path, so
it cannot tell whether the editor just opened: the detour draws the whole editor on every pass.

**Radio.** The addresses, each one rebuilt from the stock bytes by `conferir.py`'s `4p`:

| Data | Where | Read at |
|---|---|---|
| band: 0 FM, 1 AM, 2 LSB, 3 USB, 4 CW | `b[0x12A1B]` | `0x01E44FCE` |
| AM band: LW, MW, SW, USW | `b[0x14CA5]` | `0x01E45016` |
| frequency: FM in 10 kHz, the rest in kHz | `h[0x14CA6]` | `0x01E449DA` |
| browsing memories (1) and the memory | `b[0x129F9]`, `b[0x14CB4]` | `0x01E44AD2`, `0x01E44AE2` |
| signal and the level already drawn (signal / 6) | `b[0x14CCF]`, `b[0x14CCE]` | `0x01E451A6` |
| AM and SSB step | `b[0x12A6F]`, `b[0x12A6E]` | `0x01E44D22`, `0x01E44D30` |
| bandwidth, LNA | `b[0x14CBA]`, `b[0x14CBB]` | `0x01E44DC6`, `0x01E44E4E` |
| BFO, signed | `h[0x14CBC]` | `0x01E44F16` |
| setting being edited (0 = the frequency) | `[0x14CD4]` | `0x01E4934A` |
| zeros in front of what is typed | `b[0x14A36]` | `0x01E490A0` |

The modulation change goes through the radio's own menu: the tables `0x01E61168` (FM and AM) and
`0x01E6137C` (SSB), and the Modulation item's set is `0x01E3DE52`. It redraws the stock single-VFO
screen and writes "Radio Initialization / Please Wait..." before starting the chip (`0x01E3DDBA`);
in SSB, the receiver chip's patch runs from `0x01E64D40` to `0x01E68B18` (`0x01E3DC42`). The band
switch powers the chip down, waits 0.6 s (`0x01E3DBCC`) and twice more 1.5 s before powering it
up; since v40 each of those two is 0.25 s. During the screen B wait (entering the radio and
switching modulation) the saved settings go through `confere_radio`: anything outside the limits of
the factory setters (`0x01E44C68`) goes back to the default (v40, [changelog](changelog.md)).

**DTMF** (the `#` key). The mode becomes 7 (`0x01E4CD06`), and the stock box opens at `0x01E4DA54`
(on side A at y 28..68, on B at y 83..124); the digits are redrawn at `0x01E4A8D4`, from `0x14A3A`,
how many in `b[0x14A35]`, up to 10. In screen B the box is part of the scene while the mode is 7: the
exit zeroes the mode before the full redraw (`0x01E3C29C`, `0x01E40D68`). `gancho_dtmf` (on the sends
`0x01E4DA88` and `0x01E4A93C`) draws when the mode is 7; up to v46 it asked whether the two-VFO
screen was active, and `0x01E3B1A2` answers 0 with the mode above 5, so the stock box always came
up.

**Voltage at power-on** (`0x01E55E9C`, with the power-on message set to voltage): boot reads the
ADC at `0x01E45FB0` and does × 444 / 1000, in tenths of a volt. The stock text is "   Voltage" with
the voltage written over the 8th position, and the "ge" was lost.

**The medium font.** The same 5×7 letter, with each stem column (4 or more lit dots) and each empty
column 2 px wide and the others 1 px, and rows 2 and 6 2 px tall: 9 tall, the vertical stroke of 2x
and the horizontal stroke of 1x, 1 px between letters. A letter with no stem (S, Z, 2) gets the
2 px in both end columns, otherwise it comes out thin (v40). The width of each column comes ready
from `fonte.py`, one bit per column in `fonte_b_media`. It lives in a separate `letra_media()`,
because next to `letra()` it made that function's frame grow, and `letra()` is at the end of the
main screen's deepest path.

**Two traps that cost versions:**

- in the disassembly, `h[r13+r0<<1]` and `[r13+r0<<2]` are base + index × 2 and × 4. Read as base +
  index, six radio addresses came out wrong in v37, and the preview does not catch it, because it
  fakes the RAM with the same addresses;
- the stack analysis does not follow `call rN`. The menu get/set/exibe routines are called through a
  pointer: the set on OK (`0x01E49996`) at 144 B, the get and exibe in the engine at 296 B. A set
  that redraws the screen (Modulation's, CH-NAME's through the editor exit `0x01E3C7AA`) falls into
  that; measured at the call site, they all stay at up to 652 B, under the stock 740 B.

## 14. The WATCH views (v42 to v45)

WATCH picks how many frequencies the main screen shows. Screen B reads its record
([reverse engineering, section 18](reverse-engineering.md#18-dual-watch-and-triple-v42-to-v45)) on
every draw and keeps the view in the byte left over from the digits of the state structure: the
digits went from 8 to 7 bytes (up to 6 digits and the zero), and the structure did not grow,
because it sits on the deepest stack path.

| View | Value | What the screen shows |
|---|---|---|
| DUAL | 0 | the usual screen: the side in focus on the card and the other one compact |
| SINGLE | 1 | only the side in focus, on a card from the bar to the bottom of the screen |
| TRIPLE | 2 | A or B in focus on the card, and the other two as thin strips |
| TRIPLE with C on side B | 3 | C in focus on the card, and A and B as strips |

**SINGLE.** The card runs from y 18 to 126. The big line (the frequency, or the name with the name
MDF) at 3x, at 2x when it does not fit, with the frequency's last two digits at 2x; the chips
below, drawn by the scene, as in DUAL (called from inside the card, the frames added up and the
stack went 28 B past stock); the meter at y 88, 12 px tall; and, with an offset, the transmit
frequency at the bottom (`TX 147.12000`). The other side does not show, and its meter is not even
drawn.

**TRIPLE.** Each side has a letter: A, B and C, with side B showing C in view 3. The position comes
from the letter:

- the card of the one in focus at y 18 (A), 39 (B) or 59 (C), with the usual 68 px;
- the strips, 19 px tall, 20 apart: the ones before the card from 18, and the ones after it from
  88 (letter 1 at 88, letter 2 at 108).

| In focus | y 18 | y 38/39 | y 58/59 | y 88 | y 108 |
|---|---|---|---|---|---|
| A | card A | | | strip B | strip C |
| B | strip A | card B | | | strip C |
| C | strip A | strip B | card C | | |

Each strip has the letter badge, the text its MDF asks for (the name, the frequency or the number)
and the channel on a chip (`VFO` on the VFO). The third one, the one on neither side, is read from
the data chip: the channel record, with MDF-C (or MDF-B, when it is B). An empty channel shows as
`EMPTY CH`. Since v49, a third one on VFO shows its VFO's frequency (read from sector `0xD3000`)
with `VFO` on the chip. **While receiving**, the strip turns dark amber (`#3b2a0b`, the amber chip's
background) with the badge and the text in amber (`#fbbf24`). The side out of focus is receiving
when it has signal, or when dual watch is on it with state 4, and the one receiving is the third
one when the round-robin is on it.

Only the card has a meter. The meter redraw (`b_medidor`) draws the card's meter and, for the side
out of focus, the two strips that may be receiving; the redraw of one side (`desenha_lado`) and the
DTMF box follow the same geometry. The strips check by themselves whether they cross the open
window and whether they are receiving, and the strip drawing is inlined into both callers: it is
the end of the deepest path of the screen in TRIPLE, and one more frame went past the stock stack.

**In the preview**, scenes 33 to 35 are SINGLE (channel receiving, VFO with an offset, and
transmitting) and 36 to 39 are TRIPLE (A receiving, C receiving, B in focus with A receiving, and C
in focus).
