# Main screen map

> 🇧🇷 Português: [mapa-da-tela.md](mapa-da-tela.md)

Mapped on 2026-10-24 from the V0.12 disassembly, after two wrong versions. Everything here carries
an address: whatever has none is not proven, and says so.

## 1. How the firmware draws

There is no full-screen framebuffer. Drawing happens **one window at a time**:

```
janela(x0, x1, y0, y1, clear, colour)   0x01E3AD34
    stores the bounds in globals (0x145CA, 0x145CE, 0x145D0) and, with clear != 0,
    fills the window with colour
<text, bar, icons are drawn inside it>
send(...)                               0x01E3ADBC
    reads the same globals and pushes the buffer to the LCD
```

The buffer is video memory at **0x10190**, **10240 bytes** — one **160×32** band in RGB565. And
here is the trap that took v13 down:

> **`janela()` has no size guard.** It computes `(x1-x0) * (y1-y0) * 2` and starts writing from
> 0x10190 (loop at `0x01E3AD8E`). A window larger than 10240 bytes tramples whatever follows: the
> text buffer (0x129B0), the global base (0x14680), the redraw flags (0x15367) and the channel
> bitmaps. The result is a reset.

That is why no stock call exceeds 32 full-width rows — the largest is exactly 160×32, at
`0x01E4A78A`.

## 2. Which side is selected: `b[0x15E04]`

**0 = VFO A, non-zero = VFO B.** It is the variable the firmware itself consults to lay out the
screen:

| Where | What it decides |
|---|---|
| `0x01E41432` | A's bar row sits at **74** (expanded) or **41** (compact) |
| `0x01E4B308` | the channel number sits at **62** or **101** |
| `0x01E4B264` | the name+frequency window sits at **30** or **69** |

Not to be confused with **`tx_ab` (`0x14B84`)**, which is data-flash setting `0x9018` mirrored in
RAM. Using the setting instead of the screen state was one of v13's two mistakes.

## 3. The three windows of a block

| Window | Call site | Geometry | y on VFO A | y on VFO B |
|---|---|---|---|---|
| name + frequency | `0x01E4B270` | `(7, 160, y, y+24)` | 30 | 69 |
| signal bar | `0x01E41468` | `(0, 160, y, y+14)` | 41 (compact) or 74 (expanded) | 114 |
| channel number | `0x01E4B314` | `(127, 159, y, y+9)` | 62 | 101 |

All well under the buffer limit: 153×24×2 = 7344 B, 160×14×2 = 4480 B, 32×9×2 = 576 B.

The channel-number window **follows the selection** (62 when A is selected, 101 when B), i.e. it is
always the highlighted side's; the other side's number is drawn by another path.

### Draw sequence

```
0x01E4CB72 / 0x01E4CC54   UI loop, when b[0x15E13] == 1
  └─ 0x01E4B29C           draws one block
       ├─ 0x01E413BE      signal bar row  → window at 0x01E41468
       ├─ janela          channel number  → 0x01E4B314
       ├─ 0x01E3BC68      number contents
       └─ 0x01E3ADBC      send
0x01E4B1DE                name and frequency → window at 0x01E4B270
  ├─ 0x01E3BACE           name (text)
  ├─ 0x01E3BB7A           frequency
  └─ 0x01E3ADBC           send
```

## 4. Still unmapped

- Where the **non-selected side's channel number** is drawn (it shows in the photos but comes from
  none of the three windows above).
- The white separator line between the blocks and the `H` power indicator: they do not go through
  `janela()` with arguments the scanner recognises.
- Other screens (menu, single VFO, FM) use the same routines with different coordinates — the
  highlight was not tested there.

## 5. How the highlight was done, and why it is safe

The hook goes into the **three window calls** in the table above and **never changes the
geometry**: it only picks the colour the firmware was going to fill that window with.

```c
void gancho_realce(unsigned x0, unsigned x1, unsigned y0, unsigned y1,
                   unsigned clear, unsigned colour)
{
    unsigned limit = y1 - y0 > 14 ? 60 : 100;   /* 24 rows = name+frequency */

    if ((y0 >= limit) == (LADO_B != 0))
        colour = COR_REALCE;
    JANELA(x0, x1, y0, y1, clear, colour);
}
```

The height tells which of the three windows it is, and that gives the `y` threshold separating the
sides. Verified in the generated machine code (`0x01E963FA`): **r0–r3 are never touched**, so no
call can request a larger area than the original — v13's overflow is impossible by construction.

Truth table for the real combinations:

| Window | height → limit | y | side | painted when |
|---|---|---|---|---|
| name+freq A | 24 → 60 | 30 | A | A selected |
| name+freq B | 24 → 60 | 69 | B | B selected |
| bar, A expanded | 14 → 100 | 74 | A | A selected |
| bar, A compact | 14 → 100 | 41 | A | never (it only exists while B is selected) |
| bar B | 14 → 100 | 114 | B | B selected |
| channel number | 9 → 100 | 62 or 101 | follows the selection | always the highlighted side |

The colour lives in `COR_REALCE`, in `firmware/src/bancos.c` — a single line. The current value is
`0x0380` (#007100). Because the hook passes the colour as a 16-bit value, the 8-bit limit of the
direct colour patch (which only reaches greens too dark to see) does **not** apply here.
