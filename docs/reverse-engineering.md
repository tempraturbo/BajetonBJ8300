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
  for new code. Measure it with `firmware/scripts/espaco_livre.py`. Since v19 it is all for code
  (since v21, 3292 bytes from the first multiple-of-4 address, `0x01E96124` — see "traps", 7); up to v18 the menu table copy took 1512 of them and
  code could only go up to 1782.
- **Call the firmware through absolute symbols, not pointers.** `((void (*)(void))0x01E3ADBC)()`
  compiles to "load the address into a register (6 bytes) and call through it (2)"; declared with
  `FIRMWARE(ENVIA, 0x01E3ADBC)` (a `.set` in `bancos.c`), it comes out as a 4-byte relative `call`,
  resolved by the linker to the exact address. That is what made room for the clear hook (v17), and
  check `4g` in `conferir.py` recomputes the target of every generated `call`/`goto`.
- **The new menu table sits where the original was (v19).** The original (`0x01E62F98`, 53 items)
  is followed by the all-zero item that ends it: 53 + 1 = 54 slots, exactly the 54 items of the new
  table. Proven before changing it that nothing else reads the original: the only access is the
  pointer computation at `0x01E4CBB2` (the only load of 13368 in the firmware); no data pointer and
  no 32-bit constant falls inside it; the other two callers of the count (`FUN_01E4A81A`, at
  `0x01E4B3FE` and `0x01E4C3C4`) point the menu structure at **other** tables before counting; and
  the three constants valued in the offset range (13720, 13912, 14357) are arithmetic and radio-chip
  registers. The pointer stays as it was (`conferir.py` checks it), and v19 came out with code and
  table byte-for-byte equal to v18's, only the table in another place.

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
| Exit | `E` → ACK `0x06` |

Below `0xF000` data travels **encrypted** (XOR with the key `CO 7`, skipping 0x00, 0xFF, a space in
the key, and bytes equal to the key or its complement). The new command delivers it **plain**.

**How the handler works** (base `r13 = 0x14680`, loaded at `0x01E4B422`):

- packet at **`0x16671`** (letter, address high, address low, size), data and response from
  **`0x16675`**; received length in **`h[0x1666C]`**;
- `'R'` fires with **4 bytes** in the buffer (`0x01E4BB52`), `'W'` with **68** (`0x01E4C0E8`);
- both call **the same routine `0x01E43970`** — 0 to read, 1 to write;
- after answering, the counter is cleared (`0x01E4BB7C`, and `0x01E416C4` on the `'W'` path).

Three traps that cost time:

- **There is no 5-byte packet.** The fifth byte arrives after dispatch, stays pending and swallows
  the next command — including the magic. Only a power cycle clears it.
- **Nothing may be sent before the magic.** Matching starts at the beginning of the buffer, and the
  magic ends in `U`, which is an UPDATE-mode command: retrying against an out-of-sync radio makes
  it jump into UPDATE by itself.
- **One session per operation.** `E` answers with an ACK (`0x01E4C0D0` → `0x01E416C4`) and only
  puts the session in state 3; it closes on a loop turn with no byte arriving (`0x01E4BA96`). A
  `PROGRAM…` sent right after `E` lands in state 3 and is dropped — and CHIRP, which does not
  read the `E` ACK, takes that leftover byte as its answer. That was the driver's "Radio returned
  unknown identification string" up to v22, which opened a second session for the banks.

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
| `0x01E4AE44` | spectrum: next sweep point (or restart the window) |
| `0x01E39E22` | radio chip audio output (registers `0x47`/`0x48`): 1 opens, 0 closes |
| `0x01E39E2C` | amplifier request, per source (six flags at `0x129D2`): odd requests, even releases; the spectrum uses 5 and 6 |
| `0x01E39CFC` | the other audio pin (flag at `0x129D1`): 1 on, 0 off |
| `0x01E38B4C` | key beep: queues tone `134 + r0` (the code each key passes only picks the tone) |
| `0x01E44604` | end of transmission: plays the roger and switches back to receive |
| `0x01E43B4E` | frequencies of the two tone generators (registers `0x71`/`0x72`, Hz × 10.32444) |
| `0x01E43AFE` | turns the chip's tone mode on (1) / off (0) |
| `0x01E408E2` | applies the scramble: bit 1 of register `0x31` and the inversion frequency in `0x71` (section 13) |
| `0x01E36286` | busy wait in **2 ms units**: the timer interrupts every 48000 cycles of 24 MHz = 2 ms (`0x01E3E44C`) and adds 2 to the counter `[0x14810]` (`0x01E51356`), which advances 1 per ms; the wait runs until it advances `2n` |

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
| `0x00014B83` | roger: 0 OFF, 1 BEEP, 2 TONE1200; 3 = BEEP 2 (v22) |
| `0x00015E2C` / `0x000129A4` | scramble of the current channel or VFO / the tuned one: 0 OFF, 1 3400 Hz, 2 2900 Hz (v24) |
| `0x01E68B18` | scramble inversion frequencies (u16, Hz × 10.32444): 35103 = 3400 Hz and, since v24, 29941 = 2900 Hz |
| `0x0001533D` | screen mode (20 = spectrum) |
| `0x00014654` | spectrum sweep state: 0 tune, 1 measure, 2 stopped on a find |
| `0x00014648` / `0x0001464C` | measured level / threshold that stops the sweep, both in graph height (0..64) |
| `0x000148DC`..`0x000148EC` | spectrum: tuned frequency, the peak's, centre, end and start of the window (u32) |
| `0x0001551B` | spectrum graph heights, 160 columns of 0..64 (83584 + 3739) |
| `0x00006EC0` | spectrum settings: `+4` points (160 >> n), `+8` step index, `+0x14` modulation (0 FM, 1 AM, 2 USB; key 23 toggles FM and AM), `+0x15` threshold, `+0x19`/`+0x1D` its minimum and maximum |

## 6. Menu engine

The main menu table is at **`0x01E62F98`**, **53 entries of 28 bytes**:

```
flags(1) | max(u16) | get(4) | set(4) | name_en(4) | name_cn(4) | show|options(4) | options_cn(4) | id(1)
```

- `flags` 0x80 = numeric entry with its own display function; 0x07 = opens the text editor.
- The `id` byte is **not** the number shown on screen (values repeat and several are 0).
- The table pointer is built at `0x01E4CBB2` and the count comes from `FUN_01E4A81A`, pinned at
  `0x01E4CBE8`. The new table is written over the original (which has the same 54 slots, counting
  the zero item) and the count becomes the constant 54, because the zero item's slot now holds an
  item.
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
7. **Code must be written where the linker linked it** (v20, boot loop). The linker aligns the
   `.text` section to the largest alignment it finds inside it: v20 brought pointer tables
   (alignment 4) and `.text` came out linked for `0x01E96124`, while the build script wrote it at
   a fixed address, `0x01E96122`. All the code sat 2 bytes off, and the first hook to run (the
   startup one, right after the logo) jumped into the middle of an instruction: reset in under
   1 s, in a loop. No check caught it because all of them used the same fixed address on both
   sides. Since v21 the code starts at the first multiple of 4, `montar.py` reads the `.text`
   address from the ELF and refuses to build if it does not match, and `conferir.py` checks end
   to end (`2c` and `2d`: every firmware branch into the new code, against the bytes actually at
   the target, decrypted, in all 6 images).

## 10. Why it cannot brick

The app's startup reads the keys (`0x01E56C70/C7E/C8C`) **before** loading configuration and
channels; with PTT2 + the shortcut key it draws `UPDATE` and waits for a flash. The build script
has a guard (`PROTEGIDAS`) that **refuses** any write into those routines and into the UPDATE-mode
ones, and the final verification repeats that check byte by byte.

## 11. Spectrum

SPECTRUM is screen mode 20 (`b[0x1533D]`). On every turn of the main loop, after handling the key,
`0x01E4D5A0` calls the sweep state machine, `0x01E4AEAC(255)`, and draws one of the 5 chunks of the
160×64 graph (32 columns per turn, from the heights at `0x1551B`).

The main loop (`0x01E4DEA8`) is a 10 ms software timer, registered at `0x01E56D20`. On stock
firmware each point costs two turns (one tunes, the next measures): with the drawing, ~31 ms per
point, and the 5 s the owner timed for 160 points.

**Sweep** (state in `b[0x14654]`):

- **0, tune:** writes the point's frequency (`[0x148DC]`) to the radio chip's registers `0x38`/`0x39`,
  toggles `0x30` and moves to 1.
- **1, measure:** `0x01E4ACFC` reads the RSSI (register `0x67`, 9 bits) and returns 1 once the
  reading is ready (non-zero); the 10 frequencies in the internal-noise list (`0x6EE1`) count as 1.
  It keeps the raw reading in `h[0x1463C]` and in the list at `0x15944` (u16 × 160, by point index)
  and the matching height in `h[0x14648]`. `0x01E4AD7C` records the point: the sweep's highest and
  lowest reading (`h[0x14644]`/`h[0x14640]`), where the highest is (`[0x148E0]`) and the height in
  the point's columns (160 / points columns per point). If the level (`h[0x14648]`) reaches the
  threshold (`h[0x1464C]`), it **stops**: opens the audio (`0x01E39E22(1)`, `0x01E39E2C(5)`,
  `0x01E39CFC(1)`), shows the frequency and moves to 2. Otherwise, next point (`0x01E4AE44`) and
  back to 0.
- **2, stopped on a find:** keeps measuring the same point. When the level drops below the
  threshold it **resumes** (`0x01E4AEFC`): state 0, closes the audio (`0x01E39E22(0)`,
  `0x01E39E2C(6)`, `0x01E39CFC(0)`) and moves to the next point.

**Window** (`0x01E4AA3C(centre)`): step from the table at `0x01E5EBC0` by the index `[0x6EC8]`
(routine `0x01E4A99C`) — 625, 833, 1250, 2000, 2500 (6.25 / 8.33 / 12.5 / 20 / 25 kHz);
160 >> `[0x6EC4]` points (routine `0x01E4A9C2`: 160, 80, 40, 20); width = points × step. It writes
start `[0x148EC]`, end `[0x148E8]`, centre `[0x148E4]` and the step between points `h[0x14650]`.
The threshold (`[0x6ED5]`, between `[0x6ED9]` and `[0x6EDD]`) is turned into graph height (0..64)
by `0x01E4A9F2`.

The points and step labels are drawn by `0x01E4ABDC(points, step)`, with the pointer tables at
`0x01E601E0` ("x160 ", " x80 ", " x40 ", " x20 ", format `"%s "`) and `0x01E6044C` ("6.25k" …
"25.0k"); each step's filter comes from `0x01E60460` (2, 1, 1, 0, 0), which is followed right away
by text — none of the three can grow in place. The whole configuration (73 bytes at `0x6EC0`) is
**saved** when leaving the spectrum (`0x01E3FE38`) and **loaded** at startup (`0x01E55E8A`), checked
only by its checksum.

The dBm text (`0x01E4ADFE`) is drawn when the window starts over (the sweep's peak, with its
frequency) and when the sweep stops on a signal — not on every point.

**Keys** (the code comes from `b[sp+13]`, read at `0x01E4B946`; the spectrum handler is
`0x01E4BFBE`, with a `tbh` for codes 16..24):

| Code | What it does in the spectrum |
|---|---|
| 16 | frequency digit; with 6 digits it re-centres the window (`0x01E4AA3C`) |
| 17 (OK) | next step (index + 1 mod 5) and the matching filter (register `0x43`, `0x01E4A980`); since v20, in our order with 10 kHz |
| 18 | deletes a digit; with no digit, calls `0x01E3FEE4` |
| 19 | **up arrow**: threshold + 2, up to the maximum |
| 21 | **down arrow**: threshold − 2, down to the minimum |
| 23 | toggles FM and AM (`[0x6ED4]`, registers `0x31`/`0x42`/`0x2A`/`0x2B`) |
| 24 (#) | next number of points (`[0x6EC4]` + 1 mod 4); since v20, with 640 and 320 |
| 20, 22 | nothing, just the beep |

19 and 21 are the arrows in the main mode too: there they lead to the menu's `AVANCA` (19) and
`RETROCEDE` (21). Every exit of the handler goes through the beep `0x01E38B4C` with 0 (keys with no
function), 1 (up arrow) or 2 (down arrow) — except 17 and 24, which go straight back to
`0x01E4D5A0` and do not beep.

**The arrow continues the sweep (v19).** On stock firmware, stopped on a find, the arrow only
changes the threshold: the way to move on was to raise the threshold past the signal. The hook
replaces the `r1 = r0 − 16` with which the handler picks the `tbh` case (`0x01E4BFBE`, 4 bytes,
reached only by case 20 of the mode `tbh`): it returns the key in r0 and the case in r1. When
stopped (state 2), the up or down arrow does exactly the firmware's resume and becomes case 20,
which does nothing but beep; the threshold stays as it was. In any other state, and for any other
key, the case is the stock one. Three things checked first:

- **Two return values from a C function:** the pi32v2 clang returns a `u64` with the low half in r0
  and the high half in r1 (`r1_r0 = r5_r4` in the disassembly). That is what lets an instruction
  that leaves two registers be replaced by a call.
- **r2 and r3 dead at the hook point.** In this ABI only r0..r3 are free for the callee (r4..r15
  are saved in prologues, `{rets, r15-r4}` included). The new call clobbers r2 and r3 where there
  used to be no call, so they must be dead there: checked path by path from `0x01E4BFC2` on,
  following the `tbh` cases and stepping into the called routines before any write to them. No
  path reads r2 or r3 before writing them; the only one that leaves the app is `0x020015C2`, the
  critical-section entry (`cli`, counter, `sti`), which does not read them.
- **The resume is the firmware's, checked at build time:** check `4i` in `conferir.py` takes the
  resume's four calls (`0x01E4AF00/06/0C` and `0x01E4AF86`) and each one's r0 from the stock bytes,
  and requires the hook to make the same calls, in the same order, and clear the same state
  variable.

If the signal is strong, neighbouring points of the same signal may also pass the threshold — the
graph shows that as a hill several points wide — and the sweep stops again right next to it. Making
the arrow skip the whole hill needs to remember "I am skipping" from one turn to the next, i.e. a
byte of RAM of our own, and this firmware has no proven-free RAM yet.

**10 kHz step, 320 and 640 points, speed and peak (v20).**

- **Compatible indices:** since the configuration is saved, the stock indices keep their meaning
  (points 0..3, step 0..4) and the new ones go at the end: points 4 = 640 and 5 = 320, step 5 =
  10 kHz (filter 1, the same as 8.33 and 12.5). Only the key order changes: # goes 160 → 80 → 40 →
  20 → 640 → 320, and OK goes 6.25 → 8.33 → 10 → 12.5 → 20 → 25. An out-of-range value counts as the
  first one.
- **Routines replaced whole** by a 4-byte `goto` at the entry (the `goto` leaves `rets` alone, so
  ours returns straight to the caller): `0x01E4A9C2` (points), `0x01E4A99C` (step) and `0x01E4ABDC`
  (labels). No firmware branch lands in the middle of them — all go to the entry. Keys 17 and 24 are
  now handled in the key hook, which does what the firmware does next (`0x01E4C628..0x01E4C664`:
  filter, window, frequency, labels, sweep from the start).
- **320 and 640 points:** the point index is a byte and the graph has 160 columns, so the
  firmware's state machine keeps sweeping 160 columns, with the step between columns × 2 or × 4,
  and each column measures 2 or 4 sub-points at the real step and keeps the highest. Which
  sub-point is tuned comes from the frequency itself (`(freq − start) / step % sub-points`), with no
  new memory; the next point (calls at `0x01E4AF86` and `0x01E4C6B8`) goes through `gancho_avanca`,
  which moves inside the column and only on the last sub-point lets the firmware move to the next
  column. Simulated for all 36 combinations: every frequency of the window once per sweep, in order
  and in the right column.
- **One point per loop turn:** at the sweep call (`0x01E4D5B2`), when the turn measured and moved to
  the next point, the same turn already tunes it. It is only measured on the next turn, as in the
  firmware: the chip gets the same time to settle, and the reading is taken under the same
  conditions. That is twice the speed (≈ 2.5 s for 160 points). Going beyond that means shortening
  the chip's wait between tuning and reading, and nothing in the firmware says how short it can be.
- **Peak hold:** recording the point (calls at `0x01E4AEE0` and `0x01E4AF7A`) goes through
  `gancho_registra`, which does what `0x01E4AD7C` did with one difference: the bar rises at once
  and, when the signal drops, comes down half the way on each sweep. A new window (typed frequency,
  `0x01E4C054`; entry, `0x01E4CFBC`; and OK/#) clears the heights first, so another window's bars
  do not linger on top.
- **Registers:** this compiler tracks, between functions of the same file, which registers each one
  clobbers. The replaced calls and routines clobbered all of r0 to r3 (directly or through the ones
  they call), and ours clobber nothing beyond that; so no firmware caller can rely on r0..r3 after
  them.
- **Checked at build time:** every replaced call (where it went and where it goes now) and every
  entry `goto` (`4h`), the variable addresses taken from the firmware instructions that use them
  (`4f`), and the arrow's resume (`4i`, now through `gancho_avanca`).

## 12. Roger

The ROGER menu entry (stock index 20) keeps its value at **`b[0x14B83]`** (offset `0x17` of the
settings block; `0x9017` over the cable, 2 bits in CHIRP): 0 OFF, 1 BEEP, 2 TONE1200. The option
name tables live in RAM (`0x6E3C` in English, `0x6E30` in Chinese), with other data right after
them. Startup (`0x01E55C9A`) range-checks six settings fields, and roger is not one of them.

The roger is played at **end of transmission**, `0x01E44604`, called on PTT release
(`0x01E46D8C`), on a second end-of-TX path (`0x01E47A12`) and on a power-off path (`0x01E4D806`).
It plays the tone while still transmitting and then switches back to receive:

- **BEEP:** turns the tone generator on (`0x01E43AFE(1)`: registers `0x24`, `0x70` = `0xE0E0`,
  `0x3F`) and plays two frequency pairs from a table (`0x01E60DC8`), 80 ms each (a wait of 40 units of 2 ms), through
  `0x01E43B4E(f1, f2)` (registers `0x71` and `0x72`): **1000 Hz and 850 Hz**. Then turns it off.
- **TONE1200:** not a tone but an FSK data burst — it programs the chip's modulator (`0x58`–`0x5F`)
  and sends 14 words at 1200 Hz.
- The register value is **Hz × 10.32444**: 10324 = 1000 Hz, 8776 = 850 Hz and 12389 = 1200 Hz (the
  TONE1200), all read from the stock bytes.

**BEEP 2 (v22):** the new option, with value 3 (fits in the 2 bits). The menu shows OFF, BEEP 1,
BEEP 2, TONE1200; the entry's get and set are ours and swap position and value (2 ↔ 3), so BEEP and
TONE1200 keep their stock values, and the CHIRP driver does the same swap. In the end-of-TX routine,
the branch `0x01E4461E` "if not 1, skip" (`80 f8 c2 02`) became "if 0, skip" (`00 f8 c2 00`,
checked with `llvm-objdump`): value 3 takes the BEEP path. And the tone loop (`0x01E44638`) became a
call to our `gancho_roger_tons` plus a branch to its end (`0x01E44656`): with BEEP 1 it does what the
loop did; with BEEP 2 it plays **1125, 1857, 1359 and 931 Hz, 100 ms each** (the owner's mp3,
measured in 1 ms windows; v22 played 200 ms each, because the wait counts in 2 ms units — fixed in
v23). Checked first: nothing outside branches into the loop, r5 and r6 are
written before being read from there on, and r0..r3 already arrived there as call garbage. Check
`4j` in `conferir.py` takes the variable, the BEEP table and the tone and delay routines from the
stock bytes, and verifies both patches.

## 13. Scramble

The chip's SCRAMBLE is voice **spectrum inversion**, not encryption. The audio is mirrored around
a frequency, and the receiver has to mirror it around the same one to understand it. It is bit 1
of the radio chip's register `0x31`, and the inversion frequency goes into register `0x71`, the
same one the tone generator uses (Hz × 10.32444, the roger arithmetic).

The value's path, from the menu to the chip:

1. **SCRAMBLE item** (stock index 47, 2 options: OFF and ON). The get `0x01E3D058` returns
   `b[0x15E2C] & 3`. The set `0x01E3D06C` writes the value into the **high nibble** of byte 14 of
   the channel record (`0x15E52`) or of byte 16 of the VFO (`0x15E90`), keeping the low nibble.
2. **Channel load** (`0x01E3900C`): `b[0x15E2C] = byte 14 >> 4`, the whole nibble (byte 16 for the
   VFO).
3. **Tuning** (`0x01E43FCA`): stores the value in `b[0x129A4]`.
4. **Apply** (`0x01E408E2`): with 0, or in the airband (108–136 MHz), it clears bit 1 of `0x31`.
   Otherwise it sets the bit and writes into `0x71` an entry from the table **`0x01E68B18`**.
   Value 1 uses the first entry, **35103 = 3400 Hz**. Any higher value uses the second one,
   36135 = 3500 Hz, which no menu option could reach.

Every reader of `b[0x129A4]` (`0x01E43B40`, `0x01E4431A`, `0x01E443D2`) only tests zero against
non-zero. The only instruction that points to the table is `0x01E40936`. The next byte,
`0x01E68B1C`, is already another table, read by `0x01E1440C`.

**Measured** from a recording made by the owner: a 1 kHz tone transmitted with scramble on and
received with it off. The BJ-8300 inverts at **3400 Hz**, which confirms the table. The Baofeng
UV-17L inverts at **2900 Hz** in mode 3, with the residual carrier visible at 2900 Hz (−45 dB).
Same method, different frequency. The owner also tested UV-17L modes 1 and 2, and neither matches
the BJ-8300. The TONE menu's call tone (1000/1450/1750/2100 Hz, side key, `0x01E412D4` →
`0x01E408CA`) uses the same register `0x71`, so it cannot be used to measure the scramble.

**v24:** the table's second entry becomes **29941 = 2900 Hz**, and the SCRAMBLE item gains a third
option: **OFF, 3400 Hz, 2900 Hz**. Value 2 = 2900 Hz.
- Only the option count (3) and the two name tables change. The get and set stay stock, because
  the stored value already is the menu position. The Chinese OFF is the stock one.
- In CHIRP, the channel's scramble field takes bits 7–2 of byte 14, so the 2900 Hz option is
  `0x08`. In the VFO, the base format splits the nibble into `unknown3` (bits 7–5) and `scramble`
  (bits 4–2), so the driver replaces that setting with its own.
- Check `4k` in `conferir.py` takes the table from the instruction at `0x01E40936`. It confirms the
  first entry is unchanged and computes the second entry's frequency from the first one's ratio
  (3400 Hz).
