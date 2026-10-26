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
| `0x0A000–0x0FFFF` | settings; `0x0F000–0x0F24F` is factory calibration (TX power at `0xF000–0xF0BF`, [section 20](#20-tx-power-map-read-only); microphone gain at `0xF210`, [section 19](#19-microphone-gain-and-mic-gain-v49)) |
| `0x10000–0x1AFFF` | **boot image**: 160×138 RGB565 little-endian, 44160 B |
| `0x20000–0x99FFF` | fonts and Chinese text (~480 KB) |
| `0xC0000–0xC0FFF` | 64 records of 60 B used by the firmware |
| **`0xC1000–0xD1FFF`** | **the banks** (header + 16 sectors of 4 KB) |
| **`0xD2000–0xD2FFF`** | **the WATCH record** (v43; the cable writes it since v46; [section 18](#18-dual-watch-and-triple-v42-to-v51)) |
| **`0xD3000–0xD3FFF`** | **C's VFOs** (v49: C's and the saved B's; [section 18](#18-dual-watch-and-triple-v42-to-v51)) |
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

Extended writes are only accepted in `0xC1000..0xD3FFF` (up to v45, `0xD1FFF`; v46 added the
WATCH record and v49 C's VFOs), erasing the sector when the block starts it. Any other address is ignored without
writing anything (proven with `tools/cabo/teste_cabo_grava.ps1`), and the ACK goes out anyway.

**The `0x9000` settings block comes from RAM.** The cable handler treats `0x9000` apart
(`0x01E439B4`): it builds the 64 bytes from RAM (`0x01E43A94`: bytes 0..31 from `0x14B6C`, 32..63
from `0x14BCC`) and, on a write, puts them back there (`0x01E43A32`). In the chip, the settings are
a log of 96-byte records (`0x9010 + n × 0x60`, written by `0x01E3AB36`). NOISE REDUCTION
(`b[0x14BDE]`, get `0x01E3D0E0`) lands at `0x9032`; the base driver read it at `0x9031`. The other 30
menu items in the block match. `0x8000` is similar: the cable reloads A's and B's VFOs from the
log (`0x01E3A5E8`) and sends `0x14E42`.

**Memory read (v31, diagnostic).** In the extended `R`, an address with bit 22 set (`0x400000` and
up, which the 1 MB chip does not have) returns 64 B of the processor's memory at
`0x01E00000 + (address − 0x400000)`: what the firmware sees of the internal flash (and of the RAM
at `0x02000000`). The copy is the chip ROM's `memcpy` (`0x021127A8`, 191 callers in the app,
destination in r0, source in r1, size in r2). Read only. It is what showed the voice case
([section 15](#15-voices-removed)).

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
| `0x00014BDF` | MIC GAIN: 0 HIGH, 1 MID, 2 LOW (v49; `0x9033` of the settings, section 19) |
| `0x0000706C` | microphone gain put into register `0x7D` on every TX (copy of `0xF210`; since v49, of MIC GAIN) |
| `0x00014B7F` | TX-A/B (`0x9013` of the settings): 0 OFF, 1 A, 2 B; since v50, 3 = C (section 18) |
| `0x00015384` | PTT side-swap flag: 1 = swapped (swaps back at the end of TX); since v51, bit 7 = TX on the third, bit 6 = it is already on side B, bit 5 = the swap to B was TX-A/B/C's, bits 0..3 = B's power |
| `0x00015E2C` / `0x000129A4` | scramble of the current channel or VFO / the tuned one: 0 OFF, 1 3400 Hz, 2 2900 Hz, 3–8 = 2700, 2800, 3000…3300 Hz (v25) |
| `0x01E68B18` | stock inversion frequencies (u16, Hz × 10.32444): 35103 = 3400 Hz and 36135 = 3500 Hz; since v25 nothing reads it (section 13) |
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
  `0x01E4CBE8`. Up to v42 the new table was written over the original (which has 54 slots,
  counting the zero item). Since v43 it lives in the free range, in the 64-slot `menu_tabela`
  array of `tela.c`, because WATCH, CH-C and MDF-C no longer fitted. The pointer computation at
  `0x01E4CBB2` ("r0 = 13368; r5 = r6 + r0") becomes "r5 = menu_tabela" (`c5 ff` + 32 bits), in the
  same 6 bytes. The leftover r0 is not read (the next call, `0x01E3AC52`, writes it before reading
  it), and r6 is overwritten at `0x01E4CBC0`. The rest of the firmware finds the table through the
  pointer this code stores at `[0x14BF4]` (`0x01E4CBC4`), and the original stays intact. The count
  is a constant: 55 since v43.
- A new item goes in place of the zone ones (WATCH, CH-C and the BANK ones) or right after a stock
  one (MDF-C, after MDF-B: `ITENS_DEPOIS` in `montar.py`), and copies the rest of the item it is
  modelled on.
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

After the filter changes through the menu (BANK and BANK CH), **both sides** move to a valid
channel (v38), as the stock DELCH does after deleting a channel (`0x01E3C5D6..0x01E3C6C4`): in full
channel mode, each side's channel (`h[0x15E0C]`, `h[0x15E10]`) that left the bitmap jumps to the
next valid one (`0x01E3C4C0`), and `0x01E3A812` reloads both sides, revalidating each. Up to v37
only `REVALIDA()` ran, and it looks only at the highlighted side. At power-on it is still only
that, because boot already reloaded both sides (`0x01E56A9E`) before the hook, and a second reload
would request a redraw over the logo. In zone mode, which the CHIRP driver turns off on every
upload, it also stays `REVALIDA()` only.

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
8. **Growing the image breaks the radio** (v28, boot loop). The 6 images grew 8 KB into the
   unpartitioned range after them, where the disassembly proved nothing in the firmware writes
   ([section 14](#14-internal-flash-partitions-and-who-writes)). The flasher finished
   successfully, and the radio kept resetting right after the logo. The uboot and the loader use
   another cipher, and the cause was not proven; the suspect is PRCT, which holds the image size
   and kept the old one. Since then `2f` requires the stock size and the uboot and the partition
   table identical.
9. **The update skips a resource file whose CRC did not change** (v30). New data inside a file of
   the `tone` directory, with the old CRC in its entry, never reaches the radio: it keeps the old
   content. `repack` redoes the directory's CRCs (`refaz_crcs_tone`, v32).
10. **Three UFW entries are encrypted by their position in the file** (`isd_config.ini`,
    `script.ver`, `blimit.bin`: the CRC only matches when decrypted with their offset). Changing
    the size of an earlier entry moves them, and they have to be encrypted again.
11. **The loader also skips a whole image whose inner CRCs do not change** (v46). v46 only changed
    the code at the end of the image, outside every CRC, and the radio on v45 stopped at 5% and
    came back on it, like v34 flashed over v34. Since v47 `carimbo_fim` (the CRC16 of the end code)
    lives inside `app.bin`, and `conferir.py` checks it (`2i`).

## 10. Why it cannot brick

The app's startup reads the keys (`0x01E56C70/C7E/C8C`) **before** loading configuration and
channels; with PTT2 + the shortcut key it draws `UPDATE` and waits for a flash. The build script
has a guard (`PROTEGIDAS`) that **refuses** any write into those routines and into the UPDATE-mode
ones, and the final verification repeats that check byte by byte.

Since v29, check `2f` also requires, in the 6 images: the stock size, the uboot and the partition
table identical, and the last 32 bytes erased (the uboot's `BOOT_START_FIRST` marker). The voice
patches (section 15) stay off the UPDATE path: its screen is drawn from the key read
(`0x01E56C9A`), which goes on to `0x01E55AF4`.

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

**Current bar, running frequency, peak and dBm (v26–v27).**

- **Current bar in white.** `gancho_coluna` takes the five calls that draw the bars
  (`0x01E4D600`, `0x01E4D644`, `0x01E4D688`, `0x01E4D6CC`, `0x01E4D710`), in place of
  `0x01E4AAA8`, which draws an x0..x1 × y0..y1 rectangle with the color as the 5th argument (on the
  stack, as the firmware passes it). The loop redraws one 32-column strip per turn, at
  `b[0x14697]` (`4f` takes the address from the `r13 = 0x14680` at `0x01E4B422`), and each bar
  arrives with x relative to the strip. The column is strip × 32 + x, against
  `ESP_INDICE × (160 / points)`; the point's columns come out `0xFFFF`, with the top clamped to 61
  (3 px at least). The threshold line's call (`0x01E4D72E`) does not go through the hook.
- **Running frequency.** In `gancho_espectro`, the turn that tuned a new point calls
  `MOSTRA_FREQ(ESP_FREQ)` only when the strip the loop is about to redraw is the point's: the
  frequency and the white bar change together, every 5 turns. With `b[0x14A35]` non-zero (digits
  being typed, the same check the firmware does before showing the peak) it does not write.
- **Peak on the second line.** The end-of-pass call that showed the peak on the top line
  (`0x01E4AEA4`, to `MOSTRA_FREQ`) goes to `mostra_pico`: window 40..112 × 36..48, 12 px font, in
  the firmware's format (`"%03d.%05d"` at `0x01E5FF26`, checked by `4f` against the one at
  `0x01E45DB2`). `gancho_janela` clears it when the window changes.
- **dBm on the right.** Three immediates of its routine (`0x01E4ADFE`): `r0 = 40 → 112` at
  `0x01E4AE12` and `r1 = 120 → 160` at `0x01E4AE14` (the window), `r1 = 56 → 116` at `0x01E4AE30`
  (the text's x). AM/FM has the window 127..159 × 17..49 with its text at y 20, and the dBm starts
  at y 36.
- **Step (v27).** The step label is drawn at y 37, the height of the peak and the dBm.
- **AM/FM without erasing the dBm (v34).** The AM/FM routine (`0x01E4B04C`, called only by key 23
  and by the entry) opens the window `127..159 × 17..49`, clears it and writes in 12 px at y 20; its
  end went from y 49 to 33 (`r3` at `0x01E4B066`), because the dBm lives at `112..160 × 36..48`.
- **Green peak (v34).** `mostra_pico` writes through `escreve_cor`, the 12 px ASCII path of
  `escreve()` with the letter color instead of the fixed white (map in
  [screen-map.md](screen-map.md#10-text-escreve-the-fonts-and-the-glyph)).

**How the spectrum is drawn, and theme B (v41).** The firmware draws the spectrum in pieces, each
by its own routine and in its own window:

| Routine | Factory window | Called by |
|---|---|---|
| clear `LIMPA(0)` (`0x01E3ACBA`) | top band `0x2965` (y 0..17) and the rest black | entry (`0x01E4CFE2`) |
| frequency (`0x01E45D96`, `"%03d.%05d"` in the 16 px font) | 42..128 × 20..38 | entry, end of sweep, find, keys 16 and 18, OK and #, and the end of typing in the signal loop |
| typed digits (`0x01E4AFC2`, mask `"---.-----"`) | the same | keys 16 and 18 |
| AM/FM (`0x01E4B04C(m)`, text `0x01E5EBB4[m & 1]`) | 127..159 × 17..49 (33 since v34) | entry and key 23 |
| dBm (`0x01E4ADFE(v)`, `"%03ddBm"`) | 40..120 × 36..48 (112..160 since v26) | end of sweep, find and every pass while stopped on a find |
| ends (`0x01E4AB2A(start, end)`) | 0..160 × 113..128: line, marks at x 14, 80 and 145, and the two frequencies | entry, key 16, OK and # |
| graph, in the main loop | 5 windows of 32 × 64 at y 49..113, one per pass, filled black | `0x01E4D5DA`, `0x01E4D61E`, `0x01E4D662`, `0x01E4D6A6`, `0x01E4D6EA` |
| bars (`0x01E4AAA8`) and the threshold line (`0x07E0`) | x relative to the slice, top = 64 − height | `gancho_coluna` and `0x01E4D72E` |

In v41 the five routines for the frequency, the digits, AM/FM, the dBm and the ends are replaced
whole by a `goto` at the entry (`ESPECTRO_B_FUNCOES` in `montar.py`). The clear at the entry, the
five slice windows and the threshold line become replaced calls (`ESPECTRO_B_CHAMADAS`). The
targets live in `tela.o`, and the `.vozes` hooks (labels, peak, new window and bars) only call the
pieces there. Each piece opens its own window and draws only what belongs to it: the peak and the
dBm change at different moments, and there is no RAM to keep one in order to redraw it along with
the other. The proof:

- **The new sites are reached only through the mode 20 doors.** The backward closure
  (`so_por.py`) of each one stops at case 20 of the modes `tbh`, at case 36 of the key actions or
  at the fall-through of the main loop.
- **The signal loop site** (`0x01E46E5C`) has a door of its own, the `if (r0 == 20)` at
  `0x01E46B8E`, which reads `b[r6 + r9]`. The routine runs from `0x01E4622A` to `0x01E48336`, and
  nothing outside jumps into its middle. In the routine's graph, built with the `tbb`/`tbh` tables,
  every path from the entry to that point goes through `r9 = 3261` (`0x01E46544`) and
  `r6 = 83584` (`0x01E46268`). `r9` is written nowhere else, and from the `r6 += 3264` at
  `0x01E4726E` there is no way back there without passing `r6 = 83584` again.
- **The five routines are called only from those sites** (the 4-byte calls, scanned across the
  whole `app.bin`, and the 2-byte ones of the dBm, in the disassembly). No 32-bit constant points
  at them, no branch lands in their middle, and they only draw: window, text in a local buffer,
  `envia()`.

The four immediates that v26 and v34 changed inside the dBm and AM/FM routines are gone, because
the whole routines no longer run.

**The light on in the spectrum (v49).** The ABR count is at `h[0x14F80]`: whoever turns the
light on (`0x01E3951C`) loads it from ABR and calls `0x01E394D6(1)`, and the tick counts it down in
modes 0, 2, 3 and 11 (mask 2061 at `0x01E41876`) and in 20 (`0x01E41A0C`). The signal loop turns
it off, at its head (`0x01E4655A`, which every mode goes through on every turn): with ABR on, out
of mode 4 and the count at 0, `0x01E394D6(0)` at `0x01E46578`. It is the only caller that passes 0
(the light flag, `b[0x15386]`, only goes to 0 inside it; the other three turn it on). `gancho_luz`
takes that call and does not pass it on in mode 20. Nothing is saved: on leaving the spectrum, or
on powering the radio back on (power-up does not write 20 to the mode; only the key entry does,
`0x01E4CF7E`), ABR applies again.

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

**COIN (v26; called BEEP 3 up to v30).** Value 4. The menu has five options (OFF, BEEP 1, BEEP 2,
COIN, TONE1200) and two tables translate position ↔ value (`roger_valor`, `roger_posicao`). The
branch at `0x01E4461E` (if 0, skip) already takes 4 to the BEEP path, and 2 branches off earlier,
at `0x01E4461A` (`if (r0 == 2)`, stock, checked by `4j`). The tones: the Super Mario World coin
measured from the owner's mp3, 2000 Hz for 70 ms and 2672 Hz for 330 ms (`TOM(20649)`,
`ESPERA_2MS(35)`, `TOM(27587)`, `ESPERA_2MS(165)`). In CHIRP the value has 3 bits: the roger's 2
and the low bit of `unused_9017`.

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

**v24:** the table's second entry became 29941 = 2900 Hz, and the item gained a third option (OFF,
3400 Hz, 2900 Hz), with the stock get and set. Tested with the UV-17L in mode 3: the two radios
understand each other.

**v25: 2700 to 3400 Hz, in 100 Hz steps**, to cover the Baofeng's other modes. There are nine
options in ascending order: OFF, 2700, 2800, …, 3400 Hz. Tested: the UV-17L's modes 1, 2 and 3
all talk to the BJ-8300, each on one of these frequencies.
- **Frequency by value.** In the apply routine, the 14 bytes that picked the entry (`0x01E4092E`:
  `r0 = 1; if r4 == 1, r0 = 0; r1 = 0x01E68B18`) became `r0 = r4 & 0xF; nop; r1 =
  scramble_freqs`. The next stock instruction (`r1 = h[r1 + r0<<1]`) now reads the frequency from
  our 16-entry table, by the stored value. The encodings come from the firmware itself (`40 16` at
  `0x01E005E6`, `60 e1 0f 00` at `0x01E00350`, `00 00` at `0x01E0019A`), and the stretch was
  checked with `llvm-objdump` on v25. r4 is the value the routine received in r0, and the call at
  `0x01E4092A` preserves it. With the mask, any byte lands inside the table, so there was no need
  to prove where each path into here gets its value. The stock table stays intact, and nothing
  reads it anymore.
- **Values that do not change.** 1 is still 3400 Hz (the stock ON, the one that talks to other
  BJ-8300s) and 2 is still 2900 Hz (v24's). The new ones are 3 = 2700, 4 = 2800, 5 = 3000,
  6 = 3100, 7 = 3200 and 8 = 3300 Hz. For 9 to 15, values no menu writes, the radio inverts at
  3400 Hz and the menu shows 3400 Hz.
- **Sorted menu.** The get (`scramble_le`) and set (`scramble_grava`) are ours and translate
  position ↔ value. The set puts the value in `h[0x14BFC]`, calls the stock set and restores the
  position. So the channel or VFO nibble and `b[0x15E2C]` are still written by the firmware. The
  stock get and set were only used by item 47 of the menu table.
- **CHIRP.** The channel's scramble field takes bits 7–2 of byte 14, so each option is the value
  << 2. In the VFO, the base format splits the nibble into `unknown3` (bits 7–5) and `scramble`
  (bits 4–2), and the driver replaces that setting with its own. The test reads from `bancos.c`
  the value each option writes.
- **Check `4k`.** Takes the Hz → register ratio from the stock table (3400 Hz) and checks, in the
  blob, every frequency, the menu from 2700 to 3400 Hz in 100 Hz steps, the names, the position ↔
  value round trip, that every value from 0 to 15 shows in the menu with the frequency the radio
  really inverts at, and the 14 patched bytes. It was tested by swapping two positions in the
  blob, and four of those checks failed.

## 14. Internal flash: partitions and who writes

Surveyed on 2026-10-25 from the app directory of each of the 6 images and from the disassembly.

| Range (`flash.bin`, 1 MB) | What it is |
|---|---|
| `0x00A0–0x2088` | uboot (encrypted with a cipher that is not the `enc`/`sfc` one) |
| `0x2088–0x210D` | embedded `isd_config.ini` (SPI, uboot UART, RESET, EOFFSET) |
| `0x3000` | app directory (`app_dir_head`) |
| `0x3100–0x7B1DC` | `app.bin` |
| `0x7B1DC–0x9886C` | `tone` directory: voices and beeps ([section 15](#15-voices-removed)) |
| `0x9886C–0x99121` | `cfg_tool.bin` and SDK data |
| `0x99121–0x9A000` | erased slack: our code, up to 32 B before the end |
| `0x9A000–0xD2000` | no partition (~224 KB) |
| `0xD2000–0xEE000` | VM (the settings) |
| `0xEF000` and up | BTIF, USER, USERLOGO and `key_mac` (`0xFF000`) |

The other variants follow the same design: the app at `0x2300` (flash2/4/6) and 2 MB flash
(flash3/4). For the processor, the address is `0x01E00000 + (position − start of app_dir)`, which
is why it is the same in all 6. The **PRCT** partition has the image's size (0x9A000 or 0x99000:
the tool's `CODE_LEN`). The last 32 bytes of each image belong to the uboot's `BOOT_START_FIRST`
marker (documented for the previous family by kagaimiq), and our code ends before them.

**Who writes to the internal flash.** Every call of the write primitives (`0x01E37A9A`, and
`0x01E37A28` directly) lands in:

- the VM driver (`0x01E37AB8..`, `0x01E514E0..`): only in the VM's two banks;
- the **UART update**, which writes a loader (`uart_update.bin`, 21.5 KB, from inside `ota.bin`) as
  `LOADER.BIN` before rebooting (`0x01E37FB2`, called only from `0x01E69780`). The position comes
  from the current VM's start and size (read at boot, `0x01E130E4`), from the VM of the incoming
  firmware (its header, `0x01E696D2`) and from the flash size, aligned to 128 KB: the top of the
  VM, and the routine refuses (error 255) any position below its start;
- an 80-byte record at the end of the flash (`0x01E381E4`);
- the `exif` block (`0x01E50B7A`): the partition does not exist, the base stays 0 and the offsets
  are 16-bit;
- the generic file write (`0x01E13492`): only inside the open partition.

Nothing writes to the unpartitioned range — and **still, growing the image into it broke the
radio** (v28): what the uboot and the loader do with the image size does not show in the app's
disassembly. The image sizes and the partition table stay as JieLi made them (`2f`).

**The loader refuses too much change in the voices (v35).** When flashing through BinTalk, the radio
jumps to the loader, JieLi's stock `uart_update.bin` (from 2023-05-19, inside `ota.bin`). It is
the loader that asks for the pieces of the `.anc` and writes them. It is encrypted and cannot be
read. What was measured on the radio ([changelog](changelog.md), v35):

- an image that changes the voice files C0..C11 is refused before any write. Read over the cable
  afterwards, `tone` was byte for byte v34's. The content does not matter, nor what was on the
  radio;
- changing only C0 and C1 goes through;
- changing `app.bin` always went through.

Why (number of files, of sectors or of bytes) cannot be known without reading the loader. The large
new code went inside `app.bin` ([section 16](#16-g729-and-mp3-out-v36-and-v37)), and `2g` refuses to build if any
voice file other than C0 and C1 changes.

**The loader skips an image whose inner CRCs do not change (v46).** Flashing v34 over v34 itself
stopped at 5% and came back (Oct 25), and v46 over v45 did the same (Oct 26): it only changed 1
byte of the code at the end of the image, which `app_area_head`'s CRC does not cover (it only goes
to the end of `app.bin`). Every version that flashed changed `app.bin`. Since v47 the build writes
the CRC16 of the end code into `carimbo_fim`, inside `app.bin`, and `2i` checks it.

## 15. Voices removed

The `tone` directory comes right after `app.bin` (for the processor, `0x01E781DC`), with 127 files:
`index.idx` (signature `TIDX`, at `+0x1000`), the voices **C0–C49** (Chinese, 47.5 KB) and
**E0–E67** (English, 66 KB), contiguous from `+0x15C0` to `+0x1D514`, and the beeps **B0–B7**
(272 B). Offsets count from the directory header, and the data is SFC-encrypted like the rest.
The `md5.bin` named in the strings is read by nobody.

**Who plays them.** Checked against every address load that lands in the path strings, including
the ones used as a base for an offset:

| Path | What it plays |
|---|---|
| sound queue (`0x01E405E2`), the only reader of the 126-path table (`0x01E5E8FC`, read only at `0x01E40634`) | numbers 0–117 = voices, 118–125 = beeps |
| terminal mode chosen by `main` (`0x01E55AAE`, called at `0x01E57A2E`) | C16, if `b[0x153F3]` |
| event handler (`0x01E55740`), through the go-between `0x01E555C6` | C1, C2, C3 and C15 (base + 0, 25, 50 and 100) |
| another terminal mode (`0x01E55A60`) and the end of transmission | B4 and B7 |

**The three patches (v30).** Encodings checked in `llvm-objdump`, and no branch lands inside the
patched spans:

| Site | Stock | New |
|---|---|---|
| `0x01E40628` | `35 e9 76 80` `ifs (r5 >= 118) {` | `85 f9 1c ec` `if (r5 < 118) goto 56` (the return) |
| `0x01E55ABE` | `01 53` `if (r1 == 0) goto 38` | `04 93` `goto 38` |
| `0x01E555D6` | `03 50` `if (r3 == 0) goto 32` | `04 90` `goto 32` |

Each one becomes the "no voice" path that already existed there. The VOICE setting lives at
`b[0x14B73]` (the "say number" routine, `0x01E409A2`, already checks it), and the item left the
menu.

**The space:** `0x01E797A0..0x01E956F0` (114,512 B), the linker's `.vozes` section, at the same
address in all 6 images, inside the stock image.

**The update skips a resource whose CRC did not change.** Each entry keeps the CRC16 of the file's
decrypted content, and the directory header keeps the one of everything after it. In v30 the C0
content changed with the old CRC in its entry, and the radio kept the original voice: read through
the cable memory read in v31, only the replaced bytes differed, and their neighbors matched the
`.anc`. In v32 `repack` redoes the CRCs (`refaz_crcs_tone`), and the data arrived. `2g` checks the
patches, that the path table is read in one place only, that the `.vozes` block lands inside the
voices in the 6 images, that the rest of the directory is the stock one except the CRCs, and that
every CRC matches the content.

**Code in the voice space (v33).** The spectrum lives there: the 10 routines the firmware calls
(the hooks for the keys, the scan, the point record, the next point, the window, the bars and the
peak, and the three routines replaced whole), the helpers and the tables, 946 B (`.vozes.text` and
`.vozes.rodata`). The area at the end of the image keeps the rest, 2268 of its 3260 B.

None of the spectrum runs without screen mode 20 (`b[0x1533D]`). The proof is a backward closure,
over the stock disassembly, from each hook site. It gathers every instruction that can reach it:

- the previous one, when it falls through (inside an `if (...) {` block it counts as always
  falling through);
- the source of every branch;
- every `tbb`/`tbh` case, with the table read from `app.bin` and the case count from the guard
  (the table bytes do not count as instructions).

It fails if it reaches a routine entry, a constant equal to the address, a `tbb`/`tbh` without a
guard or an instruction nothing leads to. Every site stops at one of these three gates:

| Gate | Leads to |
|---|---|
| case 20 of the mode `tbh` (`0x01E4B988`) | the spectrum key handler (`0x01E4BFBE`) |
| case 36 of the key-action `tbh` (`0x01E4BE28`) | the spectrum entry (`0x01E4CF78`), which writes 20 to the mode |
| fall-through of the main loop's `if (r0 != 20) goto` (`0x01E4D5AC`) | the scan and the bars (`0x01E4D5B0`) |

The stock routines along the way (`0x01E4AEAC` scan, `0x01E4AE44` next point, `0x01E4AD7C` record,
`0x01E4AA3C` window, `0x01E4A9E6`, and the replaced `0x01E4A99C`, `0x01E4A9C2` and `0x01E4ABDC`)
are called only from those gates or by one another, always at the entry, and no 32-bit constant
points to them. The mode was checked too:

- of the 25 byte writes to it, only the one at `0x01E4CF7E` writes 20; the others write 0, 1, 2,
  3, 4, 7, 8, 11 and 17;
- no 16- or 32-bit write covers the byte;
- it lies outside the RAM's initial data, which ends at `0xD4D0`.

If code there did not run, the radio would go down on entering the spectrum. Boot, the cable and
PTT2 + shortcut never go through it.

In the build:

- `2b` requires those 10 in `.vozes` and the others in `.text`;
- `2d` requires that only the spectrum sites branch to `.vozes`;
- `4g` requires that nothing in `.text` calls `.vozes`.

All three failed on a build with boot and `zera` in `.vozes`. `2d` now counts only the branches we
wrote, the bytes that differ from stock. Read at every even offset, it also finds "branches" in the
middle of an instruction: the middle of the call at `0x01E16A10` "goes" to `0x01E79878`. No real
stock branch leads to the new code:

- the end of the image was free space;
- the only "branches" into the voice space are 56 at the end of `app.bin`
  (`0x01E734C6..0x01E77CBA`), the copy of the code that runs in RAM and the initial data, read away
  from their own address;
- the RAM code, disassembled at its own address, does not branch there.

**Main screen B (v35, moved out in v36).** In v35 the whole `tela.o` lived here too (6.6 KB counting
the spectrum), and the loader refused the image, because it changed the files C0..C11
([section 14](#14-internal-flash-partitions-and-who-writes)). Since v36 the screen lives in place of
G.729 ([section 16](#16-g729-and-mp3-out-v36-and-v37)), and only the spectrum stays here, inside C0 and C1.

## 16. G.729 and MP3 out (v36 and v37)

Screen B (5,636 B) went inside `app.bin`, in place of the G.729 decoder, the one for the `.wtg`
voices, which have not played since v30.

**The range.** `0x01E257E0..0x01E278C8`, 8,424 B, with:

- the G.729 code;
- the standard's tables: the cosine table at `0x01E2686C` and the LSP codebook at `0x01E2696C`;
- its library routines, pointed to by the operations table `0x01E26730`.

Just before, at `0x01E257C8..0x01E257DF`, there are two 12 B structures of the player, read at
`0x01E3000A` and `0x01E30012`, which stay. Just after, at `0x01E278C8`, the MP3 decoder starts: it
is called from `0x01E2ECAA` and pointed to by the table `0x01E2C084`.

**How a decoder is chosen.** The registry at `0x01E34838` holds three decoders of 68 B each, with
the type and the routines:

| Type | What it is | Routines |
|---|---|---|
| `0x40` | G.729 (`wtg`) | `0x01E257E0..`, in the range |
| `0x01` | MP3 | `0x01E2EC52..` |
| `0x100` | sine, for the `.sin` beeps | in RAM (`0x472C..`) |

Only two lookups read the registry; they are the only loads of its address:

- `0x01E2F1D2`, by exact type;
- `0x01E2F2CE`, by mask: the type must share a bit with the requested mask.

The decoder start (`0x01E30646`, called only from `0x01E308BE`) uses the exact lookup when the
requested mask has a single bit, and the mask lookup otherwise. The copy that runs in RAM calls the
exact one with `0x100`.

**Who asks for type `0x40`.** A decoder is only created by `0x01E3022E`. It is called by three
routines, and all three are called only by the sound player (`0x01E40318`):

- `0x01E3041C`, for a file that is not `.sin`. The type comes from the extension through the table
  `0x01E60D40` (`wtg` `0x40`, `msbc` `0x20`, `sbc` `0x10`, `mty` `0x400`, `aac` `0x200`,
  `mp3` `0x01`, `wma` `0x02`, `wav` `0x04`). An unknown extension gives 0, and then the default
  input `0x01E30BE4` applies, which is `0x40` again;
- `0x01E3039A`, for `.sin`, and `0x01E302E0`, for the sine by index: both use the input
  `0x01E30C18`, of type `0x100`.

The player decides by the extension of the opened file, comparing it with `"sin"` (`0x01E5FBCE`) in
`0x01E1A608` (3 letters, case-insensitive). In the sound directory, the files that are not `.sin`
are the voices and `index.idx`, which nobody plays, and every voice is cut since v30
([section 15](#15-voices-removed)). The `ring.*` in the strings does not exist in the directory:
the file does not open, and the player returns before creating the decoder.

**Nothing from outside gets in.** In the stock disassembly, of the flash and of the RAM copy, what
points into the range from outside it is:

- the registry's 6 pointers;
- ten "branches" the disassembler shows, which are table. Eight fall in the middle of the 4 KB of
  noise-reduction weights (`0x01E21C78..0x01E22788`, inside the block `0x01E21A2C` that its
  descriptor points to at `0x01E77FA0`); the other two, `0x01E257D0` and `0x01E257D8`, fall in the
  12 B structures just before the range. Not even the most conservative reachability, whose roots
  are the entry and every 32-bit value that lands in code, reaches any of them.

Apart from the registry, no `rX = imm` immediate and no loose 32-bit value, in the whole `app.bin`,
points into the range.

**The lock.** G.729's type in the registry becomes 0, and neither lookup matches 0:

- the exact lookup always gets a single-bit type (or the `0x100` of the RAM copy);
- the mask lookup needs at least one bit in common.

A `.wtg` request that slipped through the map takes the same stock path that a format without a
decoder takes, instead of running screen B as if it were audio.

**Noise reduction stays.** The ~32 KB at `0x01E1C02C..0x01E22B2C`, which looked free because they
are not code, are the noise-reduction weights. Their descriptors are at `0x01E77EC0..`, which goes
to RAM at `0xD1B4..`, and the routine that reads them (`0x01E23168`) works at 16 kHz with 512-sample
frames. It is in use.

**In the build:**

- `2b` requires `tela.o` in `.livre` (`.g729` up to v36), and only it;
- `2h` checks the registry and the place, and that nothing from outside points into the range, in
  the stock firmware and the new one, apart from screen B's sites. That covers 32-bit values and
  4-byte `call`/`goto`, the latter by the flash address and by the RAM one. It was tested with the
  range 8 B wider, when it flags the neighboring structures and MP3, and without excluding the
  registry, when it flags the 6 pointers.

**MP3 too (v37).** The MP3 decoder starts right after G.729, at `0x01E278C8`, and has no file to
play: in the sound directory, what is not `.sin` is the `.wtg` voices and `index.idx`. It goes out
the same way, and the range becomes `0x01E257E0..0x01E2EFC4`, 38,884 B, with the code and tables of
both side by side:

- its type in the registry (`0x01E3487C`, the second 68 B entry) also becomes 0;
- the only thing pointing into MP3 from outside the new range is the registry. The call from
  `0x01E2ECAA` and the table `0x01E2C084` are inside the range;
- the one exception is `0x01E05AD4`: the bytes `87 e8 e2 01`, read as 32 bits, fall in the range, but
  they are the instruction `if (r0 != r7) goto -60`. `conferir.py` only accepts the exception with
  those exact bytes.

In v39 screen B uses 11,650 B of the 38,884.

## 17. No Chinese (v37)

The radio always runs in English, as the owner asked:

- **the LANGUAGE item leaves the menu** (stock item 22, get `0x01E3C9A8` and set `0x01E3C9B8`),
  together with ZONE, "Zone Or Channel" and VOICE;
- **the power-on hook zeroes the language** (`b[0x14B74]`, which LANGUAGE's get reads) right after the
  settings come from the data chip. A Chinese language written by another program turns into
  English before the home screen, and the first saved setting (`0x01E3AB36`) writes the 0 to the
  chip. Elsewhere only LANGUAGE's set wrote the language, and the reset to factory settings
  (`0x01E4842E`) keeps it;
- **the Chinese options of the changed items** (the `+0x17` field of ROGER and SCRAMBLE) point at the
  English tables;
- **the name editor's pinyin mode is gone**: the mode switch (`0x01E4A59C`) cycles 3 modes in English
  and 4 in Chinese.

The CHIRP driver hides the language setting. `conferir.py` checks in `4o` that the byte the hook
zeroes is the one LANGUAGE's get reads and that the item left the table.

## 18. Dual watch and TRIPLE (v42 to v51)

The firmware only has two sides, A and B. WATCH picks how many frequencies the main screen shows
(SINGLE, DUAL or TRIPLE), and TRIPLE adds a third one, C, without touching the dual watch engine:
hooks on three calls and a record of its own in the data chip.

**The stock engine** is the signal loop (`0x01E4622A`):

| Address | What it is |
|---|---|
| `b[0x14B70]` | TDR: dual watch on. The loop reads it live, and the item's stock setter (`0x01E3957C`) only writes the byte |
| `b[0x15367]` | the receive state: 0 and 1 tune, 2 wait, 3 watch, 4 receiving, 5 MO (the `tbh` at `0x01E467A6`) |
| `b[0x15378]` | 1 = dual watch is off the side in focus |
| `b[0x1537A]` | ticks left before switching: 50 with no signal on a side and it moves to the next |
| `b[0x15E04]` | the side in focus (0 A, 1 B) |
| `0x01E48220` | the trip to the other side: copies the "other" shadow into the "current" one (raw channel `0x15E6C` → `0x15E44`, 20 B; raw VFO `0x15EC8` → `0x15E80`, 36 B; mode `b[0x15E14]` → `b[0x15E12]`; channel `h[0x15E40]` → `h[0x15E3C]`; zone `b[0x15E17]` → `b[0x15E15]`), sets `b[0x15378] = 1` and decodes (`0x01E3900A`, called at `0x01E4826E`) with the side in focus inverted around the call. Tuning reads what was decoded |
| `0x01E481C6` | the way back: `b[0x15378] = 0` and `0x01E3FD9E` (the shadow of the side in focus → current, and decode), called at `0x01E481CC` |
| `0x01E3FDE4` | disarms dual watch on the side in focus. Transmit calls it first (`0x01E41022`), which is why TX always goes out on the side in focus |

None of this writes to flash, or touches the screen record (`0x14DC4` + side × 31).

**The round-robin** (`gancho_volta_dw`, on the call at `0x01E481CC`). In TRIPLE, coming back from
the other side, dual watch goes to the third one instead of returning to the side in focus: it sets
`b[0x15378]` back to 1, reads the third one's raw channel from memory (20 B at channel × 32, as the
stock channel loader `0x01E3A650` reads it), marks channel in the mode `b[0x15E12]` and the number
`h[0x15E3C]`, and decodes like the trip out. Decoding writes the power of the inverted side
(`0x01E38F34`, with `b[0x153A1] = 165`), and the hook puts back the one that was there. On the next
return, the stock code restores the side in focus. The cycle is A → B → C → A, each with the 50
ticks. The third one is C, or B when C is on side B. The trip out (`gancho_troca_dw`, on the call
at `0x01E4826E`) only marks that it is not on the third one and decodes.

The round-robin state lives at `b[0x14BD0]` (2 = on the third one), the `0x9024` of the settings
block, which on the RT-900 BT is "Single Mode" and which this firmware does not use: apart from the
block copies (save settings, defaults, power-up and cable), nothing reads or writes it, checked
over the whole disassembly. Saving settings takes it along to the data chip, and a CHIRP upload
overwrites it: both only change who comes first in the round-robin.

**The A/B key** is code 18 with no digits: case 18 of the actions `tbh` (`0x01E4BE28`) goes to
`0x01E4CC10` and `0x01E4D44A`, which sets `r0 = 1` and calls the stock side swap (`0x01E40E52`) at
`0x01E4D44C`. After the call the code only calls `0x01E3AC52`, which writes r0 before reading it.
`gancho_ab` takes its place:

- A → B: the stock swap;
- B → C (TRIPLE, with B on a channel and C on a programmed channel): saves B's channel in the
  record and puts C's on side B through the arrow path, `0x01E40B9E(0, 1)`. It copies the channel
  to `h[0x15E3E]`, clears the digits (`0x01E3AC52`), loads the channel (`0x01E3A650`: the record
  from flash, the power `b[0x15E30 + side]`, the shadow of the side in focus, the decode, the
  screen record and the mute) and writes the side's channel and the flash log at `0xE000`
  (`0x01E3A42A`). It **does not redraw**: its callers do (the arrow at `0x01E453E0`, number entry
  through `0x01E41486`, the return to the main screen at `0x01E4BDBE` with `0x01E3C290`). The hook
  calls `0x01E3C290`, which clears and redraws the bar and the whole screen (`0x01E3C132(0)`), as
  the stock swap does at its end (`0x01E40EF4`). Without it, in v44, the screen stayed on B until
  the next arrow;
- C → A: saves the channel C ended on (the arrows and number entry change it as on any side), gives
  side B back to B the same way and does the stock swap, which redraws.

The stock side swap (`0x01E40E52`) disarms dual watch, flips `b[0x15E04]`, swaps the two sides'
shadows, decodes, rebuilds the screen record (`0x01E39128`) and the bar (`0x01E3B05C`), saves the
settings (`0x01E3AB36`, with r0 ≠ 0), redraws (`0x01E3C290`) and mutes (`0x01E39F20`).

**The WATCH record** lives in a sector of its own, `0xD2000`, right after the 16 banks, 14 bytes:

| Bytes | Field |
|---|---|
| 0–3 | the `WTC1` magic |
| 4 | the mode: 0 DUAL, 1 SINGLE, 2 TRIPLE |
| 5 | MDF-C: 0 name, 1 frequency, 2 number |
| 6–7 | C's channel (0 to 959) |
| 8 | 1 = C is on side B |
| 9 | since v49: bit 0 = C on VFO; bit 1 = the saved B on VFO (up to v48, reserved and 0) |
| 10–11 | B's channel, kept while C is on side B |
| 12–13 | the checksum of bytes 0 to 11, the same as the banks |

With no valid record (wrong magic or checksum), the radio stays in DUAL. v43's record had 10
bytes, without fields 8 to 11, and stopped being valid in v44. Screen B reads the record on every
draw. Since v46 the cable write window includes it, and CHIRP writes WATCH, CH-C and MDF-C.

**VFO C (v49).** C gets a mode (VFO or channel) and a VFO, like A and B. The VFOs are in sector
`0xD3000` (`struct vfc`, "VFC1", 70 bytes: C's, the saved B's and the checksum), in the format of
each side's VFO record (`0x14E42` A's, `0x14E62` B's, the 32 bytes of the `0x8000` log). Each
side's state, which the stock swap shows, is by "in focus" and "other", not by A and B:

| What | current | in focus | other |
|---|---|---|---|
| mode (0 VFO, 1 channel) | `b[0x15E12]` | `b[0x15E13]` | `b[0x15E14]` |
| channel | `h[0x15E3C]` | `h[0x15E3E]` | `h[0x15E40]` |
| zone | `b[0x15E15]` | `b[0x15E16]` | `b[0x15E17]` |
| raw VFO, 36 bytes | `0x15E80` | `0x15EA4` | `0x15EC8` |
| raw channel, 20 bytes | `0x15E44` | `0x15E58` | `0x15E6C` |

What is saved is by A and B: each side's mode in a nibble of `b[0x14B86]` (the settings'
`0x901A`, 1 = channel), each side's channel at `h[0x15E0C + side × 4]` and the VFO record.
`0x01E3A4FE(side)` copies the current VFO's 32 bytes to the record and, if it changed, writes the
log. `0x01E3A812` (the VFO/MR key's routine, which only flips the nibble and calls it) rebuilds both
sides from what is saved: each side's power-up (`0x01E3A486` A, `0x01E3A796` B) and `0x01E3A700`,
which on a channel writes the side's channel and the log (`0x01E3A42A`) and loads through the
stock loader (`0x01E3A650`, which on VFO copies the record's 32 bytes). The A/B key from B to C
swaps what is saved for side B with C's and calls that rebuild; from C to A it does the reverse
and the stock swap. In the round-robin, C on VFO goes in with its 32 bytes in the current VFO and
mode 0.

**The TDR rule**, the owner's: in SINGLE dual watch is off, and only the frequency shown receives.
WATCH set to SINGLE clears TDR, the TDR item's setter (item 7, `tdr_grava` instead of
`0x01E3957C`) keeps it at 0, and power-up clears it again. In DUAL and TRIPLE, on listens to all of
them, and off only to the one in focus.

**TX-A/B/C (v50).** The stock TX-A/B (item 44, `b[0x14B7F]`: 0 OFF, 1 A, 2 B) picks the PTT side and
only works with the watch on. PTT press (`0x01E43E8A`) tests TDR (`0x01E43F86`), reads the byte
(`0x01E43F92`) and, if the side (value − 1) is not in focus, swaps the sides (`0x01E40E52(0)`,
saving nothing) and sets `b[0x15384] = 1`, which swaps back at the end of TX (`0x01E473DC`) and on a
refused TX (`0x01E411A6`); boot clears the flag (`0x01E56B3A`). An earlier part of the same routine
does the same with the side of the PTT pressed (`b[0x15383]`), and that one wins. The item's get and
set only copy the byte, and nothing else reads it. With C (3), the stock arithmetic would swap the
sides on every PTT, so the read becomes `call gancho_tx_abc` + nop:

| With TX-A/B/C on | In DUAL / SINGLE | In TRIPLE |
|---|---|---|
| OFF, A | as stock | as stock |
| B | as stock | side B, or the third if C is on side B |
| C | nothing (as OFF) | side B if C is on it, else the third |

The third comes into focus while PTT is pressed, as A and B do, and nothing is written (v51; in
v50 TX went out on it without it showing). `gancho_tx_abc` brings side B into focus with the stock
swap (`0x01E40E52(0)`) if A was in focus, sets bit 7 of `b[0x15384]` (and bit 5, if it swapped; only
if the flag was clear) and returns 0. TX start (`0x01E41022`) disarms dual watch (`0x01E3FDE4`, at
`0x01E410EA`) and then reads the decoded TX frequency (`b[0x15341]`, written by the decoder
`0x01E38DE6`); there `gancho_tx_desarma` puts the third in the current slot (`le_terceira`, the
round-robin's own read) and decodes it without inverting the side, as the channel loader does with
the side in focus (`0x01E3A650`). TX takes the power from `b[0x15E30 + side in focus]`
(`0x01E475A2`, stored in `b[0x1299C]` by `0x01E43FCA`), and the decoder only writes it in a special
case (`b[0x153A1] = 165`); a channel's or VFO's power is the low nibble of byte 14 of the raw channel
(`0x15E52`) or byte 16 of the VFO (`0x15E90`), as the stock loaders take it (`0x01E3A6D0`,
`0x01E3A682`). The hook puts that power on side B, keeps B's own in bits 0..3 of the flag with bit
6, rebuilds B's screen record with the third's data (`0x01E39128`: the decoder's frequencies and
tones and the name read from the chip through the current channel) and redraws. Side B's slots (the
nibble of `b[0x14B86]`, `h[0x15E10]` and the shadow) do not change. With bits 7 and 6,
`watch_tela()` inverts `c_no_b` and gives the third's channel to the card (bits 16..30, with bit
31), and `terceira()` becomes the one that left side B, read from those slots (the VFO from the
shadow of the side in focus, `0x15EA4`). Both swap-backs (`0x01E411AE` and `0x01E473EE`) go through
`gancho_tx_volta`: with bit 7, they clear the flag, give the power back, bring B's shadow back to
the current slot (`0x01E3FD9E`) and B's screen record, and go back to A with the stock swap if bit 5
says that is what took it out; without bit 7, the stock swap-back.

**Checked at build time** (`conferir.py`): `4r` checks the three swapped call sites byte by byte
(`0x01E4826E`, `0x01E481CC` and `0x01E4D44C`: the stock call before, the hook's after; since v49,
the light's, `0x01E46578`; and since v50 the TX-A/B/C ones: `0x01E410EA`, `0x01E411AE`,
`0x01E473EE` and the read replaced by a call and nop at `0x01E43F92`); `4b` checks the menu table
pointer; the stack with the hooks is the stock one at all three roots.

## 19. Microphone gain and MIC GAIN (v49)

**Stock.** The microphone gain is the low 6 bits of the radio chip's register `0x7D`, written by
the register routine `0x01E39D82(reg, value)`. Boot writes `0xE912` to it (`0x01E566A4`; 18 in the
low bits), but that only lasts until the first transmission: on every TX (`0x01E477D8`) the
firmware reads the register, replaces the low 6 bits with `b[0x706C]` and writes it back. `0x706C`
is a copy of the factory calibration: boot reads 16 B from `0xF210` in the data chip into it
(`0x01E561BE`..`0x01E561CC`). On the test radio `0xF210` was **30**. Neither the menu nor the stock
CHIRP touches it (CHIRP downloads the calibration, `0xF000..0xF24F`, and does not write it).

**The scale.** The firmware writes 6 bits, and the BK4819 datasheet puts the microphone
sensitivity in bits 4 to 0 (0 to 31, in 0.5 dB steps). The other registers the firmware uses match
that chip (`0x36` amplifier, `0x43` filter, `0x47`/`0x48` audio, `0x71` scramble), but there is no
direct proof of the chip in the firmware: no text with its name, no read of its ID. That is why MIC
GAIN does not go above 31. On the test radio, over the cable, `0xF210` went to 18 and then to 31,
and the difference heard (18 → 31: "it changed a lot") fits that scale.

**The MIC GAIN item** (v49, right after NOISE REDUCTION, modelled on TDR): HIGH, MID and LOW, which
put **31, 25 and 19** in `b[0x706C]`. The level lives in `b[0x14BDF]`, `0x9033` of the settings
block (saved with them by `0x01E3AB36` and served by the cable from RAM, section 4). Nothing in the
firmware reads or writes that byte outside the block copies, checked across the whole
disassembly. The default settings table leaves it at 171, and any value above 2 counts as HIGH
(`mic_le`). The gain is set by `aplica_mic`, called:

- at boot, by `gancho_partida`, which runs after the calibration copy (every path to it goes
  through `0x01E561CC`): MIC GAIN replaces `0xF210`, which stays intact in the chip;
- in the item's set (`mic_grava`): it takes effect on the next transmission, without a restart;
- on the reset to factory settings (`0x01E4842E`), which saves the settings with a call at
  `0x01E4845A`, and on the cable `W` of the `0x9000` block, which ends in a `goto` to the same save
  at `0x01E43A50` (only the `0x9000` branch gets there, `0x01E439F2`). Both go through
  `gancho_ajustes`, which sets the gain for the new level and calls the stock save (`0x01E3AB36`
  reads no input register). A CHIRP upload takes effect at once.

**Checked at build time** (`conferir.py`): `4r` checks the call at `0x01E4845A` and the `goto` at
`0x01E43A50` byte by byte, the stock ones before and the hook's after. `gancho_ajustes` uses 296 B
of stack at its deepest point, below the interface's stock 740 B.

## 20. TX power (map, read only)

Mapped at the owner's request, to find out whether H and L sit within the limits; the modified
firmware changes none of it.

- **The register.** `0x01E44526(bias)` writes the radio chip's `0x36`: with bias 0 (end of TX,
  `0x01E44810`) it writes 127; otherwise `(bias << 8) | gain`, with the gain per level from the
  table at `0x01E5BE4A` = {`0xFF` H, `0xD7` M, `0xE8` L}, indexed by `b[0x1299C] % 3` (0 H, 1 M,
  2 L).
- **The bias** comes from 16 B tables per band and level, picked at TX (`0x01E47800`..`0x01E478BA`)
  by the frequency, through pointers from `0x01E5FB60`:

  | Band | Index | H, M, L tables in RAM |
  |---|---|---|
  | below 200 MHz | (MHz − 130) / 3, up to 15 (below 130, 0) | `0x710C`, `0x711C`, `0x712C` |
  | 200 to 299 MHz | (MHz − 200) / 5, up to 12 | `0x70DC`, `0x70EC`, `0x70FC` |
  | 300 to 399 MHz | below 350, 0; otherwise (MHz − 350) / 5 | `0x707C`, `0x708C`, `0x709C` |
  | 400 MHz and up | (MHz − 400) / 10, up to 12 | `0x70AC`, `0x70BC`, `0x70CC` |

- **The tables are the factory calibration**: boot (`0x01E5609A` on) copies the 16 B rows of
  `0xF000..0xF0BF` from the data chip to RAM (`0xF000` → `0x70AC`, `0xF010` → `0x710C`, `0xF020` →
  `0x70DC`, `0xF030` → `0x707C`, `0xF040` → `0x70BC`, `0xF050` → `0x711C`, `0xF060` → `0x70EC`,
  `0xF070` → `0x708C`, `0xF080` → `0x70CC`, `0xF090` → `0x712C`, `0xF0A0` → `0x70FC`, `0xF0B0` →
  `0x709C`).
- **On the test radio** (calibration read over the cable): H between 218 and 250, M between 186 and
  201, L between 158 and 166, on a 0 to 255 bias (255 only in the steps the index never reaches).
  The firmware does not know how many watts that gives: the output in W depends on the amplifier,
  and only a wattmeter measures it.
