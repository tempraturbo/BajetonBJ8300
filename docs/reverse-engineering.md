# What was found inside the BJ-8300

> 🇧🇷 Português: [engenharia-reversa.md](engenharia-reversa.md)

Everything below came out of the stock **V0.12** firmware and from measurements on the radio
itself. Every address was checked in the disassembly (`firmware/scripts/desmonta.sh`) or proven
over the cable. Where a conclusion came from an experiment, it says so.

## 1. The hardware

| | |
|---|---|
| CPU | JieLi **AC695X**, **pi32v2** core (JieLi toolchain 2.5.3, clang) |
| Display | 160×128, RGB565, video memory at **0x10190** (10240 B per 160×32 band) |
| Channels | 960, 32-byte records |
| CHIRP family | Radtel **RT-920** / Bajeton **BJ7800** (same protocol and layout) |

There are **two separate memories**, and mixing them up is expensive:

- **Firmware flash** — where the `.anc` is written. This is what limits code size.
- **Data flash (1 MB)** — channels, settings, boot logo, fonts and the banks. The firmware reaches
  it through `LE_FLASH`/`GRAVA_FLASH`; the PC reaches it over the cable.

## 2. The `.anc` file

JieLi UFW format with KDH scrambling. Twelve entries:

```
flash.bin  630784 B   flash2.bin 626688 B   flash3.bin 630784 B
flash4.bin 626688 B   flash5.bin 630784 B   flash6.bin 626688 B
ota.bin    285590 B   isd_config.ini 4514 B  blimit.bin 144 B  tail.bin 64 B
info.log, script.ver
```

The six flash images are variants for different chips; **all** of them must be patched, with CRCs
recomputed (`firmware/scripts/jlfw.py`).

- `app.bin` is 491,740 bytes and runs at **0x01E00100**. Its last 0xD4A8 bytes are copied to RAM at
  startup (they run at 0x0200xxxx).
- Everything **after the app directory** is read through the **SFC**, which decrypts by address in
  32-byte blocks. Writing plain code there makes the CPU execute garbage — the project's first
  serious mistake.
- **Free area common to all 6 images: `0x1E96121..0x1E96DFF` = 3295 bytes.** That is the only room
  for new code. Measure it with `firmware/scripts/espaco_livre.py`. The menu table copy takes 1512
  of them, so code can go up to 1782.
- **Call the firmware through absolute symbols, not pointers.** `((void (*)(void))0x01E3ADBC)()`
  compiles to "load the address into a register (6 bytes) and call through it (2)"; declared with
  `FIRMWARE(ENVIA, 0x01E3ADBC)` (a `.set` in `bancos.c`), it comes out as a 4-byte relative `call`,
  resolved by the linker to the exact address. That is what made room for the clear hook (v17), and
  check `4g` in `conferir.py` recomputes the target of every generated `call`/`goto`.
- **Reserve not used yet:** the original menu table (`0x01E62F98`, 53 items) is followed by the
  all-zero item that ends it, and 53 + 1 = 54 items = exactly the size of our copy. Writing the new
  table over the original would give back the 1512 bytes of the free area. First it must be proven
  that the other two callers of the count (`FUN_01E4A81A`, at `0x01E4B3FE` and `0x01E4C3C4`) do not
  count the main table — without the zero item they would run past the end.

## 3. Data flash map (1 MB)

Mapped over the cable on 2026-10-24 with `tools/cabo/mapa_memoria.ps1`, which finds the real size
by looking for where content starts mirroring address 0:

| Range | Contents |
|---|---|
| `0x00000–0x09FFF` | 960 channels, 32 B each |
| `0x0A000–0x0FFFF` | settings; `0x0F000–0x0F24F` is factory calibration |
| `0x10000–0x1AFFF` | **boot image**: 160×138 RGB565 little-endian, 44160 B |
| `0x20000–0x99FFF` | fonts and Chinese text (~480 KB) |
| `0xC0000–0xC0FFF` | 64 records of 60 B used by the firmware |
| **`0xC1000–0xD1FFF`** | **the banks** (header + 16 sectors of 4 KB) |
| `0xFA000` | 4 KB used by the firmware |
| free | `0x1B000–0x1FFFF`, `0x9A000–0xBFFFF`, `0xC5000–0xF9FFF`, `0xFB000–0xFFFFF` ≈ **370 KB** |

The firmware does **not** live on this chip: no sample matches the `.anc` images, raw or decrypted.
So there is plenty of room for **data**, while **code** space remains the 3295 bytes in the image.

## 4. Cable protocol

57600 8N1. Same as CHIRP's `radtel_rt900.py` driver:

| Step | Bytes |
|---|---|
| Enter | `PROGRAMBT11U` → ACK `0x06` |
| Identify | `F` → 8 B (`01 36 01 80 04 00 05 20`) + 8 B (`02 00 02 60 01 03 30 04`) |
| Crypto setup | `SEND \x01\x01` + 18 zeros → ACK |
| Read | `R` + **16-bit BE** address + size → 4-byte echo + data |
| Write | `W` + address + size + **64 B** → ACK |
| Exit | `E` |

Below `0xF000` data travels **encrypted** (XOR with the key `CO 7`, skipping 0x00, 0xFF, a space in
the key, and bytes equal to the key or its complement). The new command delivers it **plain**.

**How the handler works** (base `r13 = 0x14680`, loaded at `0x01E4B422`):

- packet at **`0x16671`** (letter, address high, address low, size), data and response from
  **`0x16675`**; received length in **`h[0x1666C]`**;
- `'R'` fires with **4 bytes** in the buffer (`0x01E4BB52`), `'W'` with **68** (`0x01E4C0E8`);
- both call **the same routine `0x01E43970`** — 0 to read, 1 to write;
- after answering, the counter is cleared (`0x01E4BB7C`, and `0x01E416C4` on the `'W'` path).

Two traps that cost time:

- **There is no 5-byte packet.** The fifth byte arrives after dispatch, stays pending and swallows
  the next command — including the magic. Only a power cycle clears it.
- **Nothing may be sent before the magic.** Matching starts at the beginning of the buffer, and the
  magic ends in `U`, which is an UPDATE-mode command: retrying against an out-of-sync radio makes
  it jump into UPDATE by itself.

### Extended command (this firmware)

It fits in the same 4 bytes: **bit 7 of the size byte set** means a 24-bit address, with the other
7 bits carrying the third address byte; the block is always 64 B. Bit 7 clear is byte-for-byte
stock behaviour — CHIRP sees no difference. Reach: 8 MB.

Extended writes are only accepted in `0xC1000..0xD1FFF`, erasing the sector when the block starts
it. Any other address is ignored without writing anything (proven with
`tools/cabo/teste_cabo_grava.ps1`).

## 5. Firmware routines used

| Address | What it is |
|---|---|
| `0x01E19DCC` | `sprintf` (at most two values, like the stock menu entries use) |
| `0x01E390AC` | `LE_FLASH(24-bit address, dest, size)` — data flash |
| `0x01E3A126` | `APAGA_SETOR(address)` — erase 4 KB |
| `0x01E3A25A` | `GRAVA_FLASH(address, src, size)` — splits into 256-byte pages |
| `0x01E38C3A` | `VALIDA_FREQ(freq)` — 1 if it falls in an allowed band |
| `0x01E3A700` | `REVALIDA()` — jump to the next valid channel |
| `0x01E3AD34` | **`janela(x0, x1, y0, y1, clear, colour)`** — sets the drawing window and, with `clear != 0`, fills it with `colour` (RGB565) |
| `0x01E43970` | cable data: 0 reads, 1 writes |
| `0x01E489E0` | menu engine |
| `0x01E458E4` | advance channel (used by scan) |
| `0x01E3951C` | end of config/channel loading (where the filter hooks in) |

### Memory used

| Address | What it is |
|---|---|
| `0x00010190` | **video memory** (not a sector buffer — see "traps") |
| `0x000129B0` | text buffer that goes to the screen |
| `0x00014BFC` | value of the menu entry being edited |
| `0x00014B6C` | mirror of settings block `0x9000`; **`0x14B84` = `tx_ab`** (active VFO) |
| `0x00015D8B` | "channel exists" bitmap (120 B) |
| `0x00015D12` | "channel is in scan" bitmap (120 B) |
| `0x00015D10`, `0x00015E06`, `0x00015E08` | per-zone masks |
| `0x00015E3C` | current channel |
| `0x00015367`, `0x0001536F` | redraw flags |
| `0x00016671` / `0x0001666C` | cable packet / received length |

## 6. Menu engine

The main menu table is at **`0x01E62F98`**, **53 entries of 28 bytes**:

```
flags(1) | max(u16) | get(4) | set(4) | name_en(4) | name_cn(4) | show|options(4) | options_cn(4) | id(1)
```

- `flags` 0x80 = numeric entry with its own display function; 0x07 = opens the text editor.
- The `id` byte is **not** the number shown on screen (values repeat and several are 0).
- The table pointer is built at `0x01E4CBB2` and the count comes from `FUN_01E4A81A`, pinned at
  `0x01E4CBE8`. That is how a copy with extra entries goes in without sacrificing stock ones.
- **The engine has no submenus.** The low nibble of `flags` picks one of 7 screen types (0 =
  list/value, 2 = CTCSS, 3 = frequency, 4 = MEMCH/DELCH, 7 = text editor, 8 = VERSION, 10 = DCS)
  and none of them opens a list inside another. Two levels (BANK → bank → rename/channels) would be
  possible by swapping the current table from inside our own handler, the way `FUN_01E4B3A0(id)`
  does for the quick menus — that is new UI work, and was not done.

## 7. How the bank filter works

The radio decides which channels exist by reading two RAM bitmaps built at startup while scanning
channel memory. **`FUN_01E39FCE` is their only reader.** The filter rewrites those bitmaps with the
intersection of "what exists" and the union of the enabled banks — so navigation, scan and
revalidation start respecting banks **without changing a single firmware function**.

To undo it, the bitmaps are rebuilt from scratch with the same criteria the startup uses (4 bytes
per channel, BCD, `VALIDA_FREQ`) instead of keeping a copy: that way no state of ours lives in RAM,
and a channel stored or deleted through MEMCH/DELCH counts immediately.

### Stored format

```
0xC1000  header: "BNK3" | enabled(u16) | selected(u8) | reserved | checksum(u16)
0xC2000 + n*0x1000  bank n: "BCO1" | name(12) | bitmap(120 B = 960 bits) | checksum(u16)
```

`soma_de`: starts at `0x1234` and does `sum = (sum << 1) + byte`. **Careful:** any record ending in
16 zero bytes always checksums to 0 — so for bank records it is really the marker that protects.

## 8. A useful instruction encoding

`rX = imm8` in **2 bytes** (found by comparing known cases, verified against `llvm-objdump`):

```
byte1 & 0xE0 == 0x20   and   byte0 & 0xC0 == 0x40
imm = (byte1 & 0x1F) | ((byte0 >> 3) & 7) << 5        reg = byte0 & 7
```

This is what allows swapping a fill colour **without growing the code** — and why highlight colours
are limited to 0..255, which in RGB565 means only dark greens and blues (red lives in the high
bits).

## 9. Traps that cost dearly

1. **`0x10190` is video memory**, not a sector buffer. The firmware only borrows it when rewriting
   a channel, accepting a dirty screen. Keeping the bank table there made the UI paint over it.
2. **The boot logo lives at `0x10000`** — exactly where the first bank version wrote. Erasing two
   sectors ate the top lines of the image: that was the "white band" on the logo. Hence the
   `setor_livre` guard, which only writes into an erased sector or one already ours.
3. **Mirroring**: on a 64/128/256 KB chip, `0xC1000` would land on `0x1000`, on top of channels.
   The dump proved it does not happen on this radio (1 MB, no `BNK3` marker in the low 64 KB).
4. **Measuring code size**: add `.text` + `.rodata` + `.rodata.str1.1` from the `.o`; do **not**
   include `.rela.text`.
5. **Swapping a constant mid-code requires looking at the register.** A diagnostic that changed 23
   "colour loads" at once put the radio in a boot loop: in one of them the same register zeroed a
   15-byte buffer before becoming the colour, and in two others it becomes the `x0` of the call
   that follows. Rule: only patch when the load sits right next to its use and the register serves
   nothing else until the call.
6. **`janela()` does not check signs**: it computes `(x1-x0) * (y1-y0) * 2` and starts writing. A
   negative height or width becomes a huge size and corrupts memory.

## 10. Why it cannot brick

The app's startup reads the keys (`0x01E56C70/C7E/C8C`) **before** loading configuration and
channels; with PTT2 + the shortcut key it draws `UPDATE` and waits for a flash. The build script
has a guard (`PROTEGIDAS`) that **refuses** any write into those routines and into the UPDATE-mode
ones, and the final verification repeats that check byte by byte.
