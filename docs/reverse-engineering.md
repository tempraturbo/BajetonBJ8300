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
| Radio | Beken **BK4819**, from 18 MHz up ([section 22](#22-the-mic-folder-crystal-fine-and-the-menu-while-receiving-v59)) |
| HF receiver | a Silicon Labs **Si473x**: the firmware carries the Si4735 SSB patch (15,832 B at `0x01E64D40`, 1979 8-byte commands starting with `0x15` or `0x16`, in the format the chip takes) and the LW, MW and SW bands of RX MODULATION |

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
| `0x00000–0x09FFF` | 960 channels, 32 B each (up to `0x077FF`); at `0x8000` the log of the A and B VFOs, and at `0x9000` the settings log ([section 26](#26-settings-that-got-lost-noise-gate-and-saving-on-ok-v100-and-v101)) |
| `0x0A000–0x0FFFF` | settings; `0x0F000–0x0F24F` is factory calibration (TX power at `0xF000–0xF0BF`, [section 20](#20-tx-power-map-read-only); microphone gain at `0xF210`, [section 19](#19-microphone-gain-and-mic-gain-v49); squelch, sub-tone and crystal, [section 22](#22-the-mic-folder-crystal-fine-and-the-menu-while-receiving-v59)) |
| `0x10000–0x1AFFF` | **boot image**: 160×138 RGB565 little-endian, 44160 B |
| `0x20000–0x99FFF` | fonts and Chinese text (~480 KB) |
| `0xC0000–0xC0FFF` | 64 records of 60 B used by the firmware |
| **`0xC1000–0xD1FFF`** | **the banks** (header + 16 sectors of 4 KB) |
| **`0xD2000–0xD2FFF`** | **the WATCH record** (v43; the cable writes it since v46; [section 18](#18-dual-watch-and-triple-v42-to-v51)) |
| **`0xD3000–0xD3FFF`** | **C's VFOs** (v49: C's and the saved B's; [section 18](#18-dual-watch-and-triple-v42-to-v51)) |
| **`0xD4000–0xD4FFF`** | **the Morse message**, speed and tone (v65; [section 24](#24-morse-and-aprs-v65)) |
| **`0xD5000–0xD5FFF`** | **APRS**: callsign, SSID, addressee, message and path (v65) |
| **`0xD6000–0xD7FFF`** | **the APRS RX history**: up to 62 packets, as a ring, one header per sector (v70; `0xD7000` since v74; the ring since v75) |
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
| `0x00014BE1` / `0x00014BE2` | COMPRESSOR (0 OFF, 1 to 3 = 1.33:1, 2:1 and 4:1) and MIC AGC (0 ON, 1 OFF): `0x9035` and `0x9036` of the settings (v59, section 22) |
| `0x00014BDD` / `0x00014BE0` | NOISE GATE (`0xE8` + 0 OFF, 1 DEFAULT, 2 LOW, 3 MID, 4 HIGH; v80) and EXPANDER (`0xC0` + 0 OFF to 3 HIGH); without the mark, DEFAULT and OFF: `0x9031` and `0x9034` of the settings (v79, section 25) |
| `0x00014BE3` | CRYSTAL FINE, signed 16 bits, in 10 Hz at 435.5 MHz: `0x9037` of the settings (v59, section 22) |
| `0x00007076`..`0x00007079` | the calibration's crystal correction (copy of `0xF21A`; since v59, with CRYSTAL FINE added; section 22) |
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
  is a constant: 55 since v43. Since v53 the array has 80 slots and holds the groups before the
  items, the code became "call menu_abre; r5 = r0" and the count is the groups', 13
  ([section 21](#21-the-menu-in-folders-v53-to-v57)).
- A new item goes in place of the zone ones (WATCH, CH-C and the BANK ones) or right after a stock
  one (MDF-C, after MDF-B: `ITENS_DEPOIS` in `montar.py`), and copies the rest of the item it is
  modelled on.
- **The engine has no submenus.** The low nibble of `flags` picks one of 7 screen types (0 =
  list/value, 2 = CTCSS, 3 = frequency, 4 = MEMCH/DELCH, 7 = text editor, 8 = VERSION, 10 = DCS)
  and none of them opens a list inside another. Two levels (BANK → bank → rename/channels) would be
  possible by swapping the current table from inside our own handler, the way `FUN_01E4B3A0(id)`
  does for the quick menus. That is what v53 did for the groups
  ([section 21](#21-the-menu-in-folders-v53-to-v57)).

## 7. How the bank filter works

The radio decides which channels exist by reading two RAM bitmaps built at startup while scanning
channel memory. **`FUN_01E39FCE` is their only reader.** The filter rewrites those bitmaps with the
intersection of "what exists" and the union of the enabled banks — so navigation, scan and
revalidation start respecting banks **without changing a single firmware function**.

To undo it, the bitmaps are rebuilt from scratch with the same criteria the startup uses (4 bytes
per channel, BCD, `VALIDA_FREQ`) instead of keeping a copy: that way no state of ours lives in RAM,
and a channel stored or deleted through MEMCH/DELCH counts immediately.

After the filter changes through the menu (BANK and the BANKS CHANNEL; BANK CH up to v52), **both sides** move to a valid
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

Since v54 there is no CH-C item, and in TRIPLE the key always reaches C: with its channel empty
(deleted, or never chosen), `c_alcancavel` takes it to the next channel that exists, by channel
number (stock's next one, `0x01E3C4C0`, counts inside the zone in zone mode), and with no channel at
all C goes on its VFO.

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
draw. Since v46 the cable write window includes it, and CHIRP writes WATCH, CH-C and MDF-C. The
CH-C item left the menu in v54; C's channel stays in the record, and CHIRP still writes it.

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
them, and off only to the one in focus. On the top bar, the watch when on shows as **DR** in DUAL and **TR**
in TRIPLE (up to v82, DW in both).

**Only A in SINGLE (v78)**, at the owner's request: SINGLE always shows and listens to A, and the A/B key
does not go to B or C. Up to v77 it swapped the sides in SINGLE too, and the screen then showed B.

- **The A/B key** (`gancho_ab`): in SINGLE, with A in focus, it does nothing; with B or C (WATCH set to
  SINGLE away from A), it goes back to A through the usual B → A and C → A paths.
- **WATCH set to SINGLE** with B or C in focus goes back to A when the menu closes. The item's setter
  runs with the menu open, and no stock caller of the side swap (`0x01E40E52`: the A/B key, PTT, the
  end of TX and a refused TX) runs like that. The menu exit (`0x01E3C29C`), which all 11 paths that close
  the menu go through (keys, PTT, the signal loop and power-up), clears the screen mode
  (`0x01E3C322..`) and calls the digit cleanup (`0x01E3AC52`) at `0x01E3C33A`, before redrawing. That
  call goes through `gancho_menu_fecha`: it clears the digits and, in SINGLE away from A, returns B to
  side B if C was on it and does the stock swap, which redraws (the exit redraws again).
- **The stack.** The interface and the signal loop, which close the menu, are already at the stock
  limit (740 and 620 B). The swap called from inside a routine went over by 36 B (the WATCH record on the
  stack) and, with only `rets`, by 4. So the decision is in C (`menu_fecha_decide`), and the hook, in
  assembly, pops `rets` and jumps to the swap: the path is the stock swap's, and the three roots stay at
  740, 620 and 696 B. The compiler does not emit a tail jump for a conditional call.

A radio that was already in SINGLE with B in focus powers up on B: the first A/B press, or opening and
closing the menu, takes it to A.

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
that chip (`0x36` amplifier, `0x43` filter, `0x47`/`0x48` audio, `0x71` scramble). No text in the
firmware names it, and up to v58 there was no direct proof; v59's is the table boot writes to `0x09`,
the UV-K5's BK4819 one ([section 22](#22-the-mic-folder-crystal-fine-and-the-menu-while-receiving-v59)).
That is why MIC GAIN does not go above 31. On the test radio, over the cable, `0xF210` went to 18 and then to 31,
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

## 21. The menu in folders (v53 to v57)

The menu engine (`0x01E489E0`) only knows the current table, and its state lives at `0x14BF4`:

| Address | What it is |
|---|---|
| `[0x14BF4]` | the pointer to the current table |
| `b[0x14BF8]` | how many items it has |
| `b[0x14BF9]` | 1 = editing the item |
| `b[0x14BFA]`, `h[0x14BFC]` | the item; the arrows write both (`0x01E49F78`) |
| `h[0x14BFE]` | the typed number (`0x01E4A2CE`) |
| `b[0x14C00]` | the item type (the low nibble of the flags) |
| `b[0x14C0C]` | the number timer: 15 on each digit, and the tick counts it down (`0x01E4A326`) |

The menu entry (`0x01E4CBC4..0x01E4CBEC`), the quick menus (`0x01E4B3DC..0x01E4B402`) and the arrows
(`0x01E49FA6..0x01E49FCC`) write these fields, and changing level is writing them the same way:
reload the menu exit timer (`0x01E484F8`), set the table, the count, the item and the type, announce
the new item (`0x01E4851E`, which with a null get clears the value and the cursor) and call the
engine, which redraws (`menu_vai`, in `tela.c`).

**The table** (`menu_tabela`, 80 slots of 28 B) holds the 13 groups first and then each group's
items, in group order, everything alphabetical: 68 slots used. Each group is a list item with no
get or set (like RESET), with its item count as the maximum and their names as the options; the
engine builds the lines from them, and screen B does not show them at that level. The level comes
from the pointer: the groups' table is level 0; group g's table, g + 1 (it starts after the groups
and the earlier groups' items); the radio's two, `RADIO`; and any other (the quick menus) is `FORA`,
which stays stock.

`menu_textos` (512 B) carries the rest: byte 0 the number of groups, byte 1 the number of table
positions, from byte 4 a mark per position (1 = no value in the "L": RESET, MEMCH, DELCH and the
BANKS CHANNEL; 2 = tick several: BANK and the BANKS CHANNEL), and then the pointer lists and the
group names. `montar.py` builds both from `GRUPOS` and `MENU_MARCAS`, and `conferir.py`'s `3b` reads
what was written into the image: groups and items in alphabetical order, each group a list with no
get or set and its items' names, the marks, and each stock item that stays and each of ours exactly
once, inside a group.

**The hooks:**

| Where | How | Hook | What it does |
|---|---|---|---|
| `0x01E4CBB2`, the table pointer at the entry | "r5 = menu_tabela" becomes "call menu_abre; r5 = r0" | `menu_abre` | the groups' table; if the menu was closed inside a group, it reopens there (the entry reads the item from `[+6]` right after, `0x01E4CBCA`) |
| `0x01E4CBE8`, the count | constant | — | 13, the groups |
| `0x01E4C924`, the OK call (`0x01E49996`) | call swapped | `gancho_menu_ok` | on the groups, opens the group on its first item; on VERSION, opens it read-only; elsewhere, stock OK, which opens the item editing |
| `0x01E4B9FA`, the digits call (`0x01E4A0FE`) | call swapped | `gancho_menu_digito` | outside editing, goes to the item with that number, counting from 1 (stock counts from 0, `0x01E4A2DA`); two digits before the timer runs out make one number, as stock does (`0x01E4A274..0x01E4A2CE`); with VERSION open, nothing |
| `0x01E4D54A`, EXIT outside editing ("r0 = 349", which starts the exit) | mid-routine detour | `desvio_menu_sai` → `b_menu_sai` | inside a group, goes back to the groups and on to the EXIT beep (`0x01E4D59A`); on the groups and in the radio, the stock exit |
| `0x01E49A86`, OK while editing, after the set ("r0 = 3303", which closes the edit) | mid-routine detour | `desvio_menu_marca` → `b_menu_marca` | on the tick-several items, clears the reception state (`b[0x15367]`, as stock does) and goes on to the redraw and the return (`0x01E49C7C`) without closing the edit |

The mid-routine detour (`DESVIO_MEIO`, in `tela.c`) is the usual detour with one more exit: when the
hook handles it, it does not return to just after the covered bytes but to a point further along the
routine, one the stock path also goes through (the EXIT beep, the OK redraw), with the same stack.
`4t` checks both byte by byte.

**VERSION** (type 8) is not opened by stock OK: `0x01E49A2E` only plays the voice, which no longer
exists. The hook turns editing on, which screen B draws without a cursor; OK and the digits do
nothing while it is open, and stock EXIT closes it (cancels the edit, `0x01E4D500`).

**The radio menu** (v56) is the same engine, in menu mode: the entry (`0x01E49000`) sets one of two
stock tables, `0x01E61168` (FM and AM) or `0x01E6137C` (SSB and CW, 532 B further, `0x01E49026`), and
the band's count comes from `0x01E3DE36` (FM 2, AM 5, SSB and CW 6). The memory item has `0x20` in
its flags: the title is built by a function (`0x01E3D20E`, the band prefix plus `MEMCH`), and screen
B builds it the same way. Digits count from 1, as in the main menu; OK and EXIT stay stock.

**The stack.** The folder window is drawn by the title send, at the bottom of the menu engine. With
the window's two texts at 28 B each, the interface reached 744 B (v56), 4 above stock; they became 12
and 20 B, and the three roots are back at stock's 740, 620 and 696 B.

## 22. The MIC folder, CRYSTAL FINE and the menu while receiving (v59)

**The radio chip is a BK4819.** The 16 values boot writes to register `0x09`
(`0x01E566C0..0x01E56766`, from `0x006F` to `0xF09F`) are, one by one, the BK4819 initialization of
the UV-K5, and the bandwidth register `0x43` (`0x01E4416E`: `0x3028` wide, `0x4008` narrow; the choice is bit 6
of channel byte 15, CHIRP's `narrow`, which the decode `0x01E3900C` puts in `b[0x15E2A]`) matches
its bit layout. Every write to the chip goes through `0x01E39D82(reg, value)`: 154 sites, counting
tail `goto`s (the `call`s alone give 138). The read is `0x01E407C0(reg)`, which sends the register
with bit 7 set.

**The TX compressor** comes configured and off. Boot puts `0xAA00` in `0x29` (`0x01E56602`: 2:1 ratio
in bits 15:14, 0 dB point 84) and `0x0B40` in `0x28` (`0x01E565F8`: the RX expander, with ratio 0,
off), but nothing sets bit 3 of `0x31`, which turns both on. The writes to `0x31` read it, change bit
0 (AM, not NR as it said up to v78: `0x01E44078`, `0x01E440A2`, `0x01E4B0B8`, `0x01E4B0DE`,
[section 25](#25-the-audio-folder-expander-and-noise-gate-and-what-nr-is-v79)) or bit 1 (scramble: `0x01E4090E`,
`0x01E4092A`, `0x01E4094A`) and write it back; the spectrum saves and restores the whole `0x31`, with
ten more (`0x01E3FEE4`, the list at `0x01E5FF7B`: `0x13`, `0x30`, `0x31`, `0x37`, `0x3D`, `0x40`,
`0x43`, `0x47`, `0x48`, `0x7D`, `0x7E`). The bit we set stays. **The microphone AGC** is bit 15 of
`0x19` (1 turns it off), which only boot writes, with `0x1041` (`0x01E565EE`): on. The registers only
go back to their defaults on boot's reset (`0x00`, `0x01E5653C`), in the loader `0x01E56084`, which
runs once (`0x01E56CB6`), before the boot hook.

**The MIC folder** gathers COMPRESSOR (OFF, LOW, MID and HIGH: 1.33:1, 2:1 and 4:1 in bits 15:14 of
`0x29`, the rest as boot leaves it; OFF puts `0xAA00` back and clears bit 3), MIC AGC (ON and OFF) and
MIC GAIN, which leaves TX. The levels live in `0x9035` and `0x9036` of the settings (`b[0x14BE1]` and
`b[0x14BE2]`), which the firmware does not use: no instruction reads or writes them outside the block
copies, checked across the whole disassembly (constant, `0x14680` base with offset, halfword and
word). Saving the settings (`0x01E3AB36`) takes 39 B from `0x14BCC`, so they go to the data flash with
the others, and the cable serves and receives the block's 64 B from RAM
([section 4](#4-cable-protocol)). The factory defaults (`0x01E4842E`) copy 39 B from `0x01E603C3`, but
the table ends early and runs into the Chinese text after it: 197 and 181 land there, which count as
OFF and ON, the stock ones. `aplica_mic` sets all three at boot (in `gancho_partida`, after the chip
reset), in each item's set, and when the settings change outside the menu (`gancho_ajustes`: the CHIRP
upload and RESET).

**The crystal.** Every tuning goes through `0x01E440CE(freq)`, with the frequency in 10 Hz: it adds
the calibration's correction and writes `0x38` and `0x39`. Only the spectrum writes the two directly
(`0x01E4AF1E`). The correction comes from bytes 10 to 13 of the `0xF210` block, which boot copies to
`0x7076..0x7079` (`0x01E561BE`):

| Byte | RAM | What it is |
|---|---|---|
| `0xF21A` | `b[0x7076]` | coarse index, 0 to 16, into the table at `0x01E602E0` = {40, 35, …, 5, 0, 5, …, 40} tenths of ppm, negative up to 8; 17 or more counts as 8, zero (and is written back as 8) |
| `0xF21B` | `b[0x7077]` | `0xA0` = use the fine value instead of the index |
| `0xF21C`, `0xF21D` | `0x7078` | the fine value, signed 16 bits: adds `freq / 100 × value / 435500`, i.e. 10 Hz per unit at 435.5 MHz, proportional to the frequency |

On the test radio (calibration read over the cable on Oct 26) the index is 8 and the mark `0xFF`: no
factory correction at all.

**CRYSTAL FINE** (CALIBRATION folder) adds an adjustment on top of what the factory wrote: −2000 to
+2000 Hz at 435.5 MHz, in 10 Hz steps (401 lines, a number with its own display, like BANK). +2000 Hz
is 4.6 ppm, and at 520 MHz that is 2.4 kHz, inside a 12.5 kHz channel; the stock coarse range is ±4
ppm. With the adjustment other than 0, `aplica_cristal` sets the mark and puts the adjustment plus the
factory one in the fine value (its fine value, or the index converted: 0.5 ppm = 21.775 units,
rounded); with 0, the four factory bytes go back as they were, read again from the data flash. The
adjustment lives in `0x9037` and `0x9038` (`b[0x14BE3]`, signed 16 bits), free like the MIC folder's;
the factory defaults leave `0xC4C0`, out of range, which counts as 0. It takes effect on the next
tuning, and the menu's OK asks for one: after the set the firmware clears the receive state
(`0x01E49A86`). With the item open, typed digits do nothing (`gancho_menu_digito`): they would pick
the line of that number, from −2000 Hz up.

**A one-item folder.** On EXIT while editing, after cancelling the edit, `r0 = 1400` (`0x01E4D53C`)
reads the table's item count, and with 1 the whole menu closes (`0x01E4D566`, to `0x01E3C29C`), as in a
one-item quick menu; with more it goes on to EXIT's beep (`0x01E4D59A`). `desvio_menu_um` hooks in
there: in a group it always goes to the beep, and the group's list stays, CALIBRATION included.
Nothing branches into the covered bytes, and r0 is written at `0x01E4D59A` before it is read.

**The menu while receiving.** With the menu open, on receive, the stock block of the side in focus
showed up over the folder window: its badge (which only knows A and B: C came out as B), the name and
the channel number. The signal loop calls stock's whole screen (`0x01E3C132`, through `0x01E44812`),
and only three routines paint the block and the complement (`0x01E3BD04` and `0x01E3C06E`): that one,
the side-in-focus one (`0x01E453E0`) and the one-side one (`0x01E413BE`). All three have our detour;
the last one already drew nothing in the menu, and the others handed the drawing back to stock outside
the main screen (`layout()` at 0), where it paints in the compact-A geometry, over the window's title
and first line. Since v59, `b_tela`, `b_evidencia` and `b_digitando` draw nothing in screen mode 1,
and `b_tela` clears the TX state as stock does (only with 0: with 1, the one-VFO screen, `0x01E3C152`
skips `0x01E3C1D4`). Leaving the menu (`0x01E3C29C`) clears the screen mode (`0x01E3C322`) before
redrawing (`0x01E3C33E`), so the main screen comes back whole. The menu does not close on receive: the
step into "receiving" (`0x01E47D98..0x01E47E38`) does not touch the screen mode.

Instead, the top bar shows who is receiving: `● RX`, the letter's badge (`LETRA()`, so C shows C) and
the text that side's MDF asks for, cut before the voltage, in place of the icons (VOX, DW, MO, NR, the
note, Bluetooth and the padlock), which come back when the signal goes. Who receives is always the
side in focus: entering the menu disarms dual watch (`0x01E4CBBC`, to `0x01E3FDE4`) and the signal
loop only arms it in screen mode 0 (`0x01E47C10`: `b[0x15379]`, the mode and `b[0x14950]` must be
zero). The side that reaches the meter in the menu is always A (`0x01E452E8`), which is why the
indicator does not use it. Dual watch paused in the menu is a stock choice and it stays: its trip to
the other side swaps the "current" channel, which is what the menu items read and write.

**Other calibrations mapped (read only).** `0xF0C0` and `0xF0D0` are the squelch's open and close
thresholds per level (10 B each, to `0x7052` and `0x705F`, read at `0x01E4762C` and `0x01E476A6`);
`0xF0E0`, `0xF0F0` and `0xF110` are per-band corrections read on the same path (`0x01E4765E`,
`0x01E47AE4` and `0x01E4769A`), and `0xF100` goes to `0x1496C`, with no reader checked. `0xF211`
(`b[0x706D]`) is the sub-tone level, put in bits 6:0 of `0x51` (`0x01E441A6`, `0x01E443C8`); 70 on the
test radio.

**Checked at build time:** `3b` accepts a one-item group, and `4t` checks `desvio_menu_um` byte by
byte. The stack is stock's at the three roots (740, 620 and 696 B), and the new get, set and exibe,
measured at the call site, stay at up to 320 B.

## 23. The screen backlight by PWM (v60, diagnostic)

**The pin.** The backlight is PB7 (pin 23). `0x01E001BA(pin)` returns the port base from the table at
`0x01E006D0` (`0x1E5000`, `0x1E5040`, `0x1E5080`, `0x1E50C0`: JieLi's SDK `JL_PORTA..D`), and the
backlight routine (`0x01E394D6`) only sets and clears bit 7 of port B's OUT and writes the mark
`b[0x15386]`. It is called to light up by ABR (`0x01E3951C`, which calls it at `0x01E39552`), to turn
off in the signal loop (`0x01E46578`, where v49's `gancho_luz` hooks in) and twice at boot, which
lights the logo (`0x01E55FB0`, `0x01E5607E`). Outside boot only it touches the pin: board init
(`0x01E5483E`) and the SDK's `main` (`0x01E53D2E`) configure the pin before the boot hook.

**The PWM.** The BR23 timers sit at `0x1E0400`, `0x1E0500`, `0x1E0600` and `0x1E0700` (CON, CNT, PRD
and PWM): TIMER0 does the waits, TIMER1 is the 2 ms tick (`0x01E3E44C`), and TIMER2 does not appear in
app.bin. PB7 is not a fixed PWM pin (the SDK's are PA05, PA12, PB03 and PB05), so the path is output
channel 2, which carries `CH2_T2_PWM_OUT` (46, `0x2E`). The firmware has the routine that sets up a
channel, the SDK's `gpio_output_channle` at `0x01E4370A(pin, source)`: group = source >> 4, source & 0xF
into bits 11:8 of IOMAP CON1 (`0x1E511C`) for CH0 or bits 23:20 and 27:24 of CON3 (`0x1E5124`) for CH1
and CH2, and the pin with PU = PD = 1 and {OUT, DIE} = {0, 0}, {0, 1} or {1, 0}. `luz_inicia`, at the end
of the boot hook, sets TIMER2 up as the SDK's `timer_pwm_init` does (CON = `0x18`: the 24 MHz crystal and
/4; PRD = 240, 25 kHz, above audio; PWM = 60, 25%, with the backlight on; CNT = 0; CON |= 1) and sets up
the channel (23, `0x2E`). Without CON bit 8, which drives the PWM onto TIMER2's fixed pin (PB3, which
is not the backlight's): the SDK does not set it on the channel path either.

**The backlight routine replaced.** With the pin on the channel, OUT selects the channel, and the stock
routine, turning off, would move the pin to CH0. `desvio_luz` hooks into it (8 bytes: `[--sp] = rets;
r1 = 83584`, nothing branches into them): with TIMER2 counting, it writes the mark as stock does and
sets the duty to 60 or 0; before that (the logo's light) it lets stock run.

**CH2 and the UARTs.** The channel routine has only two callers. The cable, at boot (`0x01E55AF4`, the
configuration at `0x01E5FCB6` = {62, 61, 0, 17}), is UART1 on CH1. The generic open (`0x01E6A974`,
through `0x01E4381A`, on the switch to update mode at `0x01E4C096` and at `0x01E54A92`) takes the first
free UART (bit 0 of CON0 at `0x1E2000`, `0x1E2100` and `0x1E2200`, `0x01E6A94C`): UART0 on CH0, UART1
on CH1 and UART2 on CH2. UART0 may come on from the bootloader (`main` tests it at `0x01E53E5C` for its
messages), so CH2 would only be taken with the cable and it busy. `luz_inicia` does not start the PWM
with UART2 on, so as not to take the channel from a UART; if it comes on later, the pin just echoes its
TX while it lasts.

**What only the radio can answer:** whether the backlight dims without flicker at 25 kHz, whether there
is hum on RX or TX, and how it behaves with ABR, SAVE and on the way back from flashing mode. The worst
case is the backlight stuck full (the pin off the channel goes back to an output with OUT = 1) or off
until another version is flashed; none of it touches boot before its hook or the flashing mode.

**Checked at build time:** `4m` checks `desvio_luz` byte by byte with the other 22, and the stack is
stock's at the three roots (the detour weighs 28 B where the backlight is called).

**The BRIGHTNESS item (v61).** The diagnostic worked on the radio: the backlight got dimmer, with no
flicker and no noise on RX or TX. The fixed level became the BRIGHTNESS item of the DISPLAY folder:
100%, 60%, 35%, 20% and 10% (duties 240, 144, 84, 48 and 24 of 240), at `0x9039` of the settings
(`b[0x14BE5]`; out of range, as the factory RESET leaves it, counts as 100%). `luz_ciclo` reads the level;
`luz_inicia` and `b_luz` use it, and `luz_aplica` applies it at once, on the item's set and in
`gancho_ajustes`, with the backlight's mark as it is. `0x9039` is free by the same scan as v59's bytes,
redone by script and validated on the NOISE REDUCTION byte
([section 22](#22-the-mic-folder-crystal-fine-and-the-menu-while-receiving-v59)).

**No PWM at 100% and when off (v62).** With v60 the owner heard noise on a Baofeng right next to it (and
now and then on the radio itself), which stopped with the backlight off: every PWM edge spreads a
little RF. `luz_poe` puts the pin on the channel only with the backlight on below 100% (the channel is
set up once, while PB7's PD is 0); at 100% and when off, the pin leaves the channel and is a plain
output again, with PD at 0, DIE at 1 and OUT, as board init leaves it (`0x01E54844..0x01E54854`), with no
edges at all. TIMER2 keeps counting with no pin. With UART2 on, a dimmed level stays at 100%.

**The fix for the menu's receive indicator (v62).** v59's indicator
([section 22](#22-the-mic-folder-crystal-fine-and-the-menu-while-receiving-v59)) relied on the meter,
but on the radio it never showed: the signal loop only calls the meter in screen modes 0 and 3
(`0x01E46216`, in the signal-level routine), and the end of a reception does not go through it. The
level routine (`0x01E46166`) is called in any mode: the loop has a fast counter (`b[0x14612]`, 0 to 9 on
every pass) and a slow one (`b[0x14614]`, which moves every 10), and on the pass where the fast one is
0 the slow one picks the task (the `tbh` at `0x01E46A38`); 3 and 8 go to it (`0x01E46C7A`), that is,
every 50 passes. `desvio_sinal` hooks its entry (8 bytes, `[--sp] = {rets, r7-r4}; r5 = 83584`; nothing
branches into them): outside the menu, stock; in the menu, instead of it (there it would only read RSSI
and the level kept at `b[0x7031]`), it compares the receive state (`b[0x15367]` = 4) with what the bar
shows, kept in the same `b[0x7031]` (0xFE receiving, 0xFF not), and redraws the bar when they differ.
`le_estado` sets RX in the menu from the same state, so every bar drawn there (battery, padlock)
agrees. In modes 0 and 3 no level (0 to 4) is 0xFE, and leaving the menu redraws the meter as after
stock's 0xFF.

**The end of a reception in the menu (v63).** With v62 the indicator showed and never went away: in the
menu the receive state does not leave 4 when the carrier drops (from state 4 the loop's code reaches no
write of the state; the return to waiting happens inside called routines). The indicator now also
requires the receive amplifier request: `0x01E39E2C(n)` (`PEDE_AMPLI`) keeps one byte per request from
`0x129D2` (1 and 2 the first, 3 and 4 the second, 5 and 6 the third, ...: set and clear, through the
`tbb` at `0x01E39E3A`) and turns the amplifier on with their OR. The receive one is `b[0x129D4]`:
entering reception sets it (`0x01E47E1A`, outside the QT scan) before setting the state to 4, and the
dropping signal clears it (`0x01E47A9E`), neither looking at the screen mode. `RECEBENDO_MENU` = state 4
and that byte set, used by `b_sinal` and `le_estado`.

**The indicator without a mark (v64).** The `-DDIAG_RX` diagnostic showed on the radio that the level
reading is called about once a second after the reception too, and that the state and the amplifier request
go back at once (receiving `E4 A1`, afterwards `E3 A0`). What failed was the mark in `b[0x7031]`: the firmware
resets it on its own (`b[0x7031] = 255` at `0x01E45A70`, called by the loop at `0x01E46F52`), and with it reset
`b_sinal` thought the bar was clear. Now it redraws the bar on every call, in the menu, with no mark; and
`b[0x7031]` is written by the firmware alone.

**The edge test (v66) and LIGHT PWM (v67).** To find what removes the PWM interference on a radio held
close, v66 brought two test items in the CALIBRATION folder. **LIGHT DRIVE** changed the pin's output
strength: PB7's bit in the two registers after port B's PD, `0x1E5058` and `0x1E505C`, the SDK `JL_PORT`'s
HD0 and HD (the first six, OUT to PD, are proven by the stock pin routines, `0x01E001D2` and neighbours;
these two are not). On the radio no combination changed anything in the light, and the item was removed:
either the edge depends on the backlight transistor, or these are not the strength registers. **LIGHT
PWM** changes TIMER2's PRD, and with it the PWM frequency: the edge energy that lands in a VHF/UHF channel
is proportional to it. At 1 kHz the radio held close heard no interference, and since v67 it is the
default. The choice lives in settings byte `0x903B` (`b[0x14BE7]`, free per the same v59 scan: from
`0x14BDF` to `0x14BEB` no stock access other than the block copies, checked on NOISE REDUCTION's
`0x14BDE`), 0 = 1 kHz (PRD 6000), 1 = 5 kHz (1200), 2 = 25 kHz (240) and, since v68, 3 = 240 Hz (25000) and
4 = 580 Hz (10345), which the menu shows in frequency order; out of range means 240 Hz since v69 (1 kHz in v67 and v68).
The owner still heard interference at 1 kHz: no frequency removes it (the energy landing in a channel only
falls with it), and the lower ones were there to compare. At 240 Hz there was no interference and no
flicker, and since v69 it is the default: the factory reset leaves 189 at `0x903B` (the byte of the text
after the `0x01E603C3` table), out of range, and values already saved keep their v68 meaning. Each level's duty goes on the same scale (`duty × PRD / 240`), and power-up, the set and the
settings by cable apply it (`luz_aplica`).

## 24. Morse and APRS (v65)

Two new folders, to send text in a standard any radio understands. The code is in
`firmware/src/mensagem.c`, all of it in the decoders' area (`.livre`), with no `.data` or `.bss`.

| Folder | Item | What it does |
|---|---|---|
| APRS | APRS CALLSIGN | the callsign, up to 6 letters (text editor) |
| APRS | APRS SSID | 0 (none) to 15 |
| APRS | APRS TO | the message's addressee, up to 9 (with the SSID after the `-`); empty = status |
| APRS | APRS MESSAGE | the message, up to 37 |
| APRS | APRS SEND | OK sends (an action, no value in the folder) |
| APRS | APRS PATH | WIDE1-1, WIDE1-1,WIDE2-1 or NONE |
| APRS | APRS RX | OK listens: the received packets screen, any key leaves |
| MORSE | MORSE MESSAGE | the message, up to 37 |
| MORSE | MORSE SEND | OK sends |
| MORSE | MORSE SPEED | 5 to 40 words per minute (default 15) |
| MORSE | MORSE TONE | 400 to 1200 Hz, in steps of 50 (default 700) |

**Where it is stored.** Two sectors of our own, free in the cable survey (section 3), in the same format as
the others (marker, checksum and the lock of only writing an erased sector or one that is already ours,
`grava_registro`): `0xD4000` "MRS1" (48 B: length, wpm, tone in tens of Hz, the text) and `0xD5000`
"APR1" (64 B: the lengths, the SSID, the path, the message number, the callsign, the addressee and the
text). Zeroed means the default.

### The text editor at 37 characters

The editor is CH-NAME's. The RAM map around the text (`0x14A3A`):

| Address | What it is |
|---|---|
| `b[0x14A35]` / `b[0x14A36]` | length / maximum (the maximum is the same byte as the main screen's digit-entry mode) |
| `b[0x14A37]` | **the cursor**, the insertion position (`0x01E49666`, `0x01E49F24`, `0x01E4D4BA`). The `b[0x12A74]` that v38 called the cursor is the pinyin count |
| `b[0x14A38]` / `b[0x14A39]` | mode (1 ABC, 2 123) / 0xAA marker (the first key replaces the whole text) |
| `0x14A3A..0x14A49` | the stock 16 bytes of text (the clear routine `0x01E3AC52` zeroes `0x14A34..0x14A49`) |
| `0x14A4A..0x14A5F` | the 11 chip registers the spectrum saves on entry (`0x01E4CF9C`) and restores on exit (`0x01E3FF0E`, mode 20 only) |
| `0x14A60..0x14A77` | the SDK flash file system structure (`0x01E12F1C` and neighbours): **never touched** |

Hence 37: the text and its closing zero reach `0x14A5F`. The spectrum copy is safe: the editor only exists
in the menu (mode 1), and the per-mode key dispatch (`0x01E4B988`) sends everything in mode 20 to the
spectrum handler (`0x01E4BFBE`), which does not open the menu; mode 1 is only written by the menu entry
from the main screen. The editor is never open between the spectrum saving and restoring.

The patches (`montar.py`):

- the insertion's fixed 15 limit, `0x01E4A0F0` `02 fc 04 1c` (`if (r2 > 14)`), becomes `02 fc 04 48`
  (`if (r2 > 36)`, the encoding checked by assembling the instruction with JieLi's clang). That path
  (`0x01E4A0CC`) is only called by the editor (`0x01E4A4CA`); CH-NAME and NAME stay limited to 12 by
  their own maximum;
- the three stock copies of the text with `"%15s"` (`0x01E46CCE` and `0x01E476BE`, into `sp+21` in the
  signal loop, with a `_` in the 26th byte; `0x01E48FBA`, into the 28 B `TEXTO` at `0x129B0`) call
  `texto15`: the same result up to 15 letters, and at most 24. Theme B shows none of them;
- the editor's 0 key (the pointer at `0x01E60B60`, the ABC key table) points to `" -/=+@()*#0"`: a space
  on the first press and the SSID's `-`, which no key reached (the `@$%&` and ` =+-_` sequences belong to
  the `:` and `;` keys, which the keypad never produces). The stock sequence was `"()/\*# 0"`;
- the editor drawing (`editor_cena`, `telas.c`) puts what does not fit one line on two, in the medium or
  the small font.

Each text's get fills the editor (zeroing up to the 38th byte, because the insertion does not write the
closing zero), puts the cursor at the end and the 0xAA marker; the set copies `length` letters and leaves
the editor like CH-NAME does (`LIMPA(2)` and `0x01E3C7AA`). OK on the item calls the set without
comparing values (`0x01E49A00`, after the `0x01E38BD4(61, 6)` beep): that is why SEND and RX are
single-option lists.

### Transmitting: the stock state machine

The firmware never transmits with the menu open, and TX is a state machine in the signal loop
(`0x01E4622A`, every 10 ms through the `0x01E4DEA8` `sys_timer`):

| Variable | What it is |
|---|---|
| `b[0x1533C]` | 0 idle, 1 TX, 2 refused (beeping until PTT is released) |
| `b[0x1534A]` | the step: 0 keys up (mutes RX audio, frequency, power, reg 0x30 = 0xC1FE), 1 waits one pass and arms TOT, 2 TOT and PTT (released goes to 3), 3 calls the end (`0x01E44604`), 4 tail and keys down, 8 after TOT |
| `b[0x14950]` / `b[0x14924]` | alarm / beep playing (the PTT press does not transmit with either, `0x01E43F42`) |
| PA12, `[0x1E5004]` bit 12 | the PTT: 0 = pressed (`0x01E47986`) |

The start (`0x01E41022`) closes the menu (`0x01E40768` → `0x01E3C29C`, which saves the settings),
disarms dual watch, checks the allowed band and the busy channel. The end (`0x01E44604`) plays the roger
while still transmitting, with the microphone cut (reg 0x30 = 0xC3FA through `0x01E3FF3E(4)`), and goes back
to receive.

Sending uses that whole machine:

1. SEND only arms: `b[0x14BE6] = 'M'` or `'A'` (byte `0x903A` of the settings, free per the v59 scan;
   the factory default leaves 202 in it);
2. **a hook on the PTT-press call** (`0x01E46540`, `call 0x01E43E8A`, on every loop pass): with a send
   armed, no beep playing (OK's own), no TX, no alarm and PTT released, it sets "on air" (`'m'`/`'a'`)
   and calls the TX start like the PTT press does (`0x01E39EE4`, `0x01E3951C`, `0x01E41022`); the stock
   PTT press comes last, as a jump. With PTT pressed or TX idle (refused), "on air" is undone;
3. with PTT not pressed, step 2 goes straight to 3, and the **hook on the end call** (`0x01E46D8C`, the
   only one step 3 makes) plays the content in the roger's place: the same setup and teardown as the
   BEEP roger (`0x01E43AFE(1)`, `0x01E3FF3E(4)`, `0x01E39DEA(1)`; `0x01E39E2C(10)`, `TOM(0, 0)`,
   `0x01E43AFE(0)`, `0x01E39DEA(0)`), tone 2 off (reg 0x70 = 0xE000), and the stock end with the roger
   zeroed around the call (the end does not save the settings).

Power-on and the settings that arrive by cable or by the factory reset zero `b[0x14BE6]`: nothing
transmits on its own. Before the content, the hook waits for the keys to be released (the OK that armed
it may still be held).

**Watchdog.** 4096 ms (power-on, `0x01E53BF4`: index 12 of the `0x01E5F1B4` table), cleared by the
interface on every pass (`0x01E4B41A`), by `janela()`, by the long waits and by the roger itself
(`0x01E44634`). While the content plays the interface does not run: our code calls `0x01E37E76` before
each Morse character and element and around the APRS packet.

**Stopping midway.** PTT or any key: the `0x01E43D50()` scan (255 = none) followed by
`0x01E43CF4(60)`, as the loop does. After a key, it waits for the release: the loop did not run while it
went down and, back, would see it as a new key (EXIT would become A/B on the main screen).

### Morse

Through the tone generator, like the roger: `TOM(f, 0)` on and `TOM(0, 0)` off, in FM (MCW). The unit is
1200/wpm ms (the word PARIS): dot 1, dash 3, 1 between elements, 3 between letters and 7 between words,
through `ESPERA_2MS`. The table has the 53 ITU characters from `!` to `Z` (one byte each: a marker and one
bit per element), generated and checked back by script; the ones with no code (`# % * < >`) become a
space. A tone known only at run time uses the integer formula Hz × 330382 / 32000 (= × 10.32444, the
roger's).

### APRS: the packet and the AFSK

**The packet** is APRS's AX.25 UI frame: destination `APZBJ8` (APZ = experimental software), the source
with its SSID, the path, control `0x03`, PID `0xF0`, the information field and the FCS (X.25 CRC-16:
reflected polynomial 0x8408, starts at 0xFFFF, inverted, low byte first). The field is the message
`:ADDRESSEE:text{id` (the addressee in 9 columns, the number from 1 to 99 for the other side to
acknowledge) or, with no addressee, the status `>text`.

**The AFSK.** The chip's FSK modem only does fixed pairs (FFSK 1200/1800 and 1200/2400; reg 0x72 is the bit
rate; on the UV-K5 1200/1800 was measured with an SDR), and APRS is Bell 202, **1200/2200 Hz**. So the AFSK
came from the tone generator (up to v85; since v88, through the microphone: [below](#transmitting-through-the-microphone-v86-to-v88)): on each 0 bit the frequency of tone 1 (reg 0x71) switches between 1200 and
2200 Hz (NRZI), with bit stuffing after five 1s, 40 opening flags (270 ms) and 3 closing ones. Before the flags, since
v69, there is 0.5 s of unmodulated carrier, so the receiver (battery saving, slow squelch) opens before
the packet. Each bit's
time (833.3 µs) comes from the clock: `b[0x14810]` counts milliseconds, 2 at a time on the TIMER1 tick,
and the TIMER1 counter (`0x1E0504`, from 0 to the PRD at `0x1E0508`) is the fraction of the tick, the
`0x01E4F888` formula. The target moves 833, 833 and 834 µs (2500 every 3 bits, with no accumulated
error) and wraps around 32 bits together with the clock.

**Checked on the PC** (`tests/teste_mensagem.py`, with zig): `mensagem.c` compiles with the chip and the
clock mocked (the clock crosses the 32-bit wrap in the middle of the packet); the frame matches an
independent builder; the tone switches become phase-continuous audio, and an independent demodulator
(correlation at 1200 and 2200 Hz, clock from the transitions, NRZI, destuffing, FCS) finds the right
frame, with every switch less than 10 µs off the 833.3 µs grid; the HDLC decoder finds only the good
frames in both polarities, with noise between them, one with a wrong bit and one aborted; and the Morse
comes back the same at 12 and 20 wpm. The audio is left at `firmware/build/aprs.wav`.

### APRS: receiving

We did not find the received audio on any of the processor's ADCs (the slow ADC at `0x1E3100` only reads
the battery), so receiving uses the chip's modem in raw capture, like the stock TONE1200 roger and TA1JS
on the UV-K5: reg 0x58 = 0x3FC3 (FFSK 1200/1800), 0x72 = 0x3065 (1200 bps), 0x70 = 0x00E0, a 4-byte sync =
four NRZI flags (0x5A = 0x5B = 0xFEFE), no chip CRC (0x5C = 0xAA30), a 2048-byte frame (0x5D = 0xFFE0),
almost full at 4 words (0x5E = 0x0204), sync, almost-full and end interrupts (0x3F = 0x3002), and
0x59 = 0x4008 / 0x1008 to clear and start. The loop checks the interrupt request (reg 0x0C bit 0) every
2 ms, clears and reads 0x02, and on each almost-full reads 4 words from 0x5F (the RX FIFO holds 8, 107 ms).
The bits go to `hdlc_bit`, the word's low byte first and each byte's top bit first (the chip's TX order
that TA1JS used with aprs.fi decoding). On sync, the decoder starts already "inside" a flag, with the line
level of the polarity the chip saw (reg 0x0B bit 7). A frame with a good FCS, or 2.5 s with nothing after
the sync, rearms the capture.

The loop belongs to the item's set, with the menu open: the signal loop does not run, so dual watch is
disarmed first (`0x01E3FDE4`, which goes back to the side in focus), the watchdog is cleared here,
registers 0x3F, 0x70 and 0x72 are read first and restored at the end, and any key leaves. The screen
(`tela_aprs`, `telas.c`) shows the 2 newest frames on two lines each, a message to my callsign in amber
(without the `{id`), and how many have arrived.

**What only the radio can prove:** whether the chip's FFSK 1200/1800 demodulator tells the 2200 Hz space
from the mark (on the UV-K5 there are reports both ways: TA1JS decoded a TH-D75; another project found no
frame with a good FCS); whether switching reg 0x71 mid-tone is phase continuous; the AFSK deviation with the
roger's gain; and how long one chip write takes.

### APRS: receiving in v70

On the radio, the FFSK 1200/1800 mode decoded a real beacon (the PY2ARS-14 digipeater, of the APRS-SP
network), but little of the channel's traffic. v70 goes after the causes on our side:

- **Frames of any length.** Up to v69 the decoder kept the whole frame to check the FCS and dropped what
  did not fit the 128 B buffer: digipeater beacons with a comment and already repeated frames go past
  that. Now the FCS is computed byte by byte as the frame comes in, and only the first 128 bytes are
  kept, for the screen.
- **Error correction, like Direwolf's FIX_BITS.** A frame kept whole with a bad FCS goes through a search
  for one wrong bit or two adjacent ones (one wrong tone on the air becomes two wrong bits in a row after
  NRZI), using the CRC's linearity: the remaining register differs from the right one (`0xF0B8`) by the
  error's effect, and that of one bit at position p is the CRC step applied (total − p) times to a 1, one
  step per position, without redoing the CRC. The fix only counts if the frame looks like APRS (addresses
  of letters and digits, 2 to 10 of them, control 0x03, PID 0xF0): with ~2000 tries, one in 30 would hit
  the CRC by chance.
- **Counters on the screen:** the syncs the modem found, the frames with a bad FCS (BAD) and the fixed
  ones (FIX), to see where the traffic is lost.
- **Four modem modes on the # key**, to compare with real traffic: 1 `0x3FC3`, 2 the wide RX band
  (`0x3FC9`, bits 3:1 = 100), 3 FFSK 1200/2400 (`0x33C9`) and 4 RX gain 0 (`0x3CC3`). Switching zeroes the
  counters.
- **The receiver woken up** (`0x01E39EE4`) on entry: the loop keeps battery saving from running.
- **A readable screen** (`aprs_texto`): an uncompressed position (`!`, `=`, `/` and `@`, these with the
  time) in degrees and minutes with the symbol by name (DIGI, GATE, CAR, HOME, WX…); a message to someone
  else as `SOURCE>ADDRESSEE: text`, one to me as `SOURCE> text`, and an acknowledgement as `ACK n`. The
  rest as it came, up to 48 characters, broken at a space.
- **The history in flash**, in sector `0xD6000` ("HST1"): up to 63 records of 64 B (the text and the
  colour), written without erasing until the sector fills up; full, it is erased and starts over. It is
  only used if erased or already ours. The arrows move through it, two at a time.

For the stack, the loop calls the screen directly, with no routine in between, and the screen cuts the
lines in the record itself (a zero for a moment at the break) instead of copying them: 732 B at the call
point, out of 740.

**Holding EXIT in the editor (v70).** In the text editor, EXIT deletes the last letter (`0x01E4D486`,
only reached by EXIT while editing text, from `0x01E4C7B4`). The hook in place of its first call (the MENU
EXIT TIME reload, `0x01E484F8`) waits while EXIT stays pressed: past ~0.6 s it sets the cursor
(`b[0x14A37]`) to 0, and the rest of the stock routine, which with the cursor at the start leaves editing
without saving (`0x01E4D4F4`), does the rest. Released earlier, it deletes the letter, as always. It
applies to every text editor. (On the radio it did not work: EXIT reaches that routine already released.
See "Holding EXIT, for real (v72)".)

### APRS: the preamble polarity (v71)

**What v70 showed on the radio.** Mode 1 decoded the digipeater's beacon and packets from an igate, always
from the same stations, and on ~95% of the channel's packets no counter moved at all: neither SYNC nor BAD.
Mode 2 (wide band) gave syncs and no frames; 3 (1200/2400 with the wide band) and 4 (gain 0) heard nothing.
WAVs played into a Baofeng, with the phone speaker against the microphone, told why: with the 2200 Hz tone
from −9 to +9 dB of the 1200 Hz one, none synced; with two types alternating, the same frame with the
preamble resting on 2200 Hz (type A) and on 1200 Hz (type B), B synced all five times and A never did.

**The cause.** In an NRZI flag one tone rests for 7 bits and the other shows up for 1, and which is which
depends on the transmitter's NRZI state when it starts. The chip only syncs on the pattern in `0x5A`/`0x5B`:
with `FE FE`, only on flags resting on mark. The negative sync, which bit 7 of `0x0B` signals in
OneOfEleven's MDC1200, did not happen on any type A frame.

**v71:**

- **Both polarities.** While waiting, the loop switches every 50 ms between `FE FE` and `01 01` (resting on
  space), with the 2-byte sync (`0x59` = `0x4000`/`0x1000`, bit 3 at 0) to fit the window with room to
  spare. Seven equal line bits only exist in flags (stuffing breaks them up inside the frame), so neither
  pattern shows up in the middle of one. The starting line level comes from the armed polarity
  (`pol ^ 0x0B bit 7`).
- **Capture goes on after the sync.** The chip hands over the raw bits of its whole frame, 2048 bytes
  (13.6 s), and `hdlc_bit` finds the flags of the following frames by itself, of either polarity. The loop
  no longer re-arms after a good frame or 2.5 s after the sync: only at the end of the chip's frame (`0x02`
  bit 13), or if the FIFO stops coming for 300 ms. All 4 words of each notice are read, even after a frame
  is found, so the FIFO does not fill up.
- **The counters** split the syncs of the two polarities (`SYNC mark/space`).
- **The modes**, each changing one thing only from 1: 1 `0x3FC3`; 2 FFSK 1200/2400 with mode 1's band
  (`0x33C3`); 3 and 4 the same without RX de-emphasis, bit 8 of `0x2B` (per the UV-K5 community's BK4819
  documentation; the stock firmware only writes 0 to it, at `0x01E440BE`, and the screen restores it on
  exit). De-emphasis takes about 5 dB off 2200 Hz relative to 1200 Hz.
- **Third-party frames** (`}` at the start of the info, what an igate brings from the internet): the screen
  shows the inner station and its info (position, message) instead of the raw header.
- **Where it listens.** At the top, in place of "APRS RX", the side in focus as its MDF asks (name,
  frequency or number; on VFO, the frequency), read from RAM like the main screen (`0x14DC4`, `0x14B79`,
  `0x14B86`, `0x15E0C`), without the `le_estado` chain, which would not fit the stack. With C on side B
  (TRIPLE), the format follows MDF-B.
- **Holding EXIT (~0.6 s) clears the history** and stays on the screen: sector `0xD6000` is erased and gets
  its marker again (the same routine as a full history), only if the sector is ours. A short EXIT leaves,
  as before.

The loop's stack went down to 728 B at the call point, out of 740.

### Holding EXIT, for real (v72)

**What v71 showed.** Reception started decoding about half the packets in mode 1, and nearly all the rest
went to BAD (the frame arrives whole, with wrong bits). Mode 3 (without de-emphasis) behaved like 1, and 4
like 2 (1200/2400, worse): bit 8 of `0x2B` changes nothing visible. And holding EXIT in the editor (v70) did
not leave: while held nothing happened, and on release it deleted the letter.

**The stock key machine** (the signal loop, every 10 ms) reads the key at `0x01E46280`, only accepts the
same one on three reads in a row, and counts its rounds in `b[0x14680]` (the previous one is kept in
`b[0x7030]`). Outside TX (`b[0x1533C]` other than 1), pressing sends nothing (`0x01E46372`, the table by
event type); at 100 rounds (1 s) it sends the key's long code (`0x01E6037B` + the raw code: 35 for EXIT) and
sets `b[0x146F4]`; release sends the short press (the code from `0x01E60369`, 18 for EXIT), unless the long
mark is set. In other words: the editor's EXIT runs when the key has already been released, and v70's hook
in that routine (`0x01E4D486`) never saw it held. It is gone, and the call is the stock one again.

**v72** replaces the loop's key read (`0x01E46280`, `call 0x01E43D50`) with `gancho_teclas`: with EXIT on
its 60th round (0.6 s) in the text editor (`b[0x14C00]` = 7 and `b[0x14BF9]` = 1, as screen B recognises
it), it sets the cursor (`b[0x14A37]`) to 0 and starts returning "no key". The stock code sees EXIT
released after three reads, sends the short press, and the editor's EXIT routine, with the cursor at the
start, leaves without saving (`0x01E4D4F4`), with the key still held. While EXIT stays held the read keeps
returning "no key", otherwise the real release would become another EXIT and go up the menu. The mark for
this lives in settings byte `0x903C` (`b[0x14BE8]`): the stock disassembly has no access to it, neither
through the `0x14680` base (offset 1384 is never loaded) nor by address. A value left in it is cleared on
the first read without EXIT.

On the APRS RX screen, which reads the keys itself, holding EXIT already cleared the history at 0.6 s, but
the screen was only redrawn after release: the loop now redraws before waiting for the key to be released,
which also applies to the arrows and `#`. The loop's stack is at 740 B at the call point, out of 740.

### The modem gain: CALIBRATION > MODEM GAIN (v73)

The chip's FSK modem input gain is bits 9:8 of `0x58`: 4 levels, with no public scale (the UV-K5
documentation only names it). The stock firmware uses 3 (the roger TONE1200's `0x3FC3`); 0 (v70's mode 4)
heard nothing. With 2200 Hz outside what the modem expects, too strong a signal can saturate the input.

In v73 it becomes an item of the CALIBRATION folder, **MODEM GAIN**: OFF, LOW, MEDIUM (MID since v82) and HIGH (0 to 3),
stored in settings byte `0x903D` (`b[0x14BE9]`, free per the v59 scan) as `0xA0` + the gain. Any other value
means HIGH, the stock one: that is the case of a radio that never changed the item, and of the stock RESET,
which puts in the byte whatever follows the `0x01E603C3` table (198 today). APRS RX starts with the chosen
gain, and `#` changes it there only, to compare (at the top of the screen, in place of v71's M1 to M4, the
gain's name); on leaving, the next entry goes back to the CALIBRATION one.

The gain only exists with the modem on, and only APRS RX uses it for receiving: the modem's only stock use
is the roger TONE1200 (`0x01E44676..0x01E44790`), which only transmits, programs `0x58` (`0x3FC3`), `0x59`,
`0x5A..0x5D`, `0x3F`, `0x70` and `0x72` every time, and at the end leaves `0x58` at 0, the modem off. Leaving
APRS RX writes the same 0 to `0x58` and, since v73, also restores `0x5E` (the FIFO thresholds), which the
stock firmware never writes and RX used to change without restoring. Voice reception does not go through
the modem.

With the item, the RX loop reached 744 B at the call point, out of 740: the start of the frame kept for the
screen went from 128 to 120 bytes (the FCS is still checked over the whole frame, and the screen uses up to
48 characters), and the stack is at 736 B.

### v74: the whole text, no bit left behind, and the audit

**No bit left behind.** From v71 to v73, when a frame closed correctly, reading stopped in the middle of the
FIFO's 4-word block: up to 63 bits after it were thrown away. An igate or digipeater that sends several
frames in one transmission, separated by one to three flags, lost the start of the next one. Now the good
frame becomes text and goes to the history right there (`guarda`, called inside the 64-bit loop, before the
next frame takes over `q`), and the bits go on. The screen, which took all 4 strips at once after each frame,
now goes one 32-line strip per loop round, between modem reads (`pede`): the FIFO never waits for more than
one strip.

**The whole text.** The frame as text goes up to 120 characters (`TEXTO_MAX`; it was 48, what fit in two
lines), and the history moved to 128 B records in sectors `0xD6000` and `0xD7000` (the data chip map gives
both as free), with the "HST2" marker: still 63 records. The v70 to v73 history ("HST1", 64 B records) is
erased on the first entry. The screen no longer keeps the texts on the stack: it reads each line from flash
when drawing (`aprs_linhas`, 25 bytes at a time), which freed the stack for a 160 B `q`. In the list each
frame has two lines, and the second ends in `>` when the text goes on; **OK** opens the top one whole, in up
to 6 lines, the arrows keep moving with it open, and OK or a short EXIT go back to the list. Letters from
U+00C0 to U+00FF (UTF-8 `0xC3 xx`) show without the accent, and °, º and ª as o, o and a.

**The screen.** The top frame, the one whose position is at the top, sits on the gain chip's background,
and the counters and keys on the top bar's background. The footer lists the keys: `#:GAIN OK:OPEN HOLD
EXIT:CLR` (open, `OK:BACK`).

**The audit.** An independent review of the APRS code (the code and the measured facts, without the
author's conclusions; fuzzing of the text conversion and the decoder, simulation of tone errors and noise)
found no critical bug and confirmed the FCS, the addresses, the formats, the AFSK timing, HDLC and memory
safety. Into v74 went:

- **The microphone went on air** during the 0.5 s of silent carrier before APRS (and while the SEND's OK
  stayed pressed): `AUDIO_TX(4)` (reg 0x30 = 0xC3FA) came after the waits. It now comes right at the start
  of `gancho_fim_tx`.
- **Our TX preamble rested on 2200 Hz**, the polarity a BK4819's sync does not catch (v71's "type A"): the
  AFSK started on mark, and the flag's first 0 bit switched it right away. It now starts on space, and the
  flags rest on 1200 Hz. Direwolf and TNCs do not care; a UV-K5 or an older BJ-8300 now sync.
- **The correction accepted ~1% of wrong frames** (with 2 or more errors the search hits the CRC by chance,
  and only the header was checked). The single-bit search is gone: CRC-CCITT has the (x+1) factor, so the
  syndrome of an even-weight error, which every wrong tone is, has even parity, and a single bit's has odd
  parity; they never matched, and it only accepted false positives. The fix now also checks the info (0x1C
  and up, CR and LF, like Direwolf's sanity check) and is undone when it fails. In the test, 40 frames with
  two wrong tones: none accepted.
- **BAD counts what it should:** only frames with an APRS header (`cabecalho_aprs`), aligned or not. Before,
  the noise after a packet counted, and a real frame thrown out of alignment by an error vanished uncounted.
  An AX.25 frame of another protocol (control other than UI or PID other than 0xF0) no longer reaches the
  screen.
- **In the text conversion:** "ack" is only an acknowledgement with a 1 to 5 character id and nothing after
  it ("ackermann" is not); the symbol only shows by name in the primary table (`/`); the hemisphere letter is
  checked; and a control character or one above 0x7E in the addressee, the third-party callsign or the
  symbol becomes a dot.
- The footer buffer went from 16 to 18 B ("SYNC 65535/65535" and the 0), and the PC test deletes its temp
  folder (1.4 MB every run).

**The next phase** (audit findings that change behaviour; done in v75, below):

- **automatic ack** for a message addressed to me (the sender repeats until it gets one), and the same id
  when resending the same message (today each SEND changes the id, and the other side sees it as new);
- **ring history**, one sector at a time, without erasing all 63 when it fills up, and **without the
  copies** of the same packet heard direct and through digipeaters (a digest of the text in the record's
  spare bytes);
- **reading the modem during key waits** (the full redraw after a key, the held-EXIT wait, the key
  release) and **erasing flash outside the bit loop** (the full history's two sectors are erased in the
  middle of a burst);
- on send, **turning off scramble and the subtone** (scramble uses the same reg 0x71 as the AFSK) and
  **refusing an invalid callsign** (with `-`, space or `/`, which key 0 gives);
- checking who owns `0xD7000` before erasing it, and writing the record's "used" byte last (power loss in
  the middle);
- uppercasing the callsign and addressee when saving (the editor only has ABC and 123, but RX compares
  letter by letter);
- PC tests for the RX loop, the history and the screen, which today only run on the radio.

### v75: the audit's second phase, the ack and the keys

Everything the audit left for the next phase went into v75, together with the owner's requests for the RX
screen.

**Automatic ack.** A message to me with a number (`{id`) arms the ack: the text in `aprs_texto` returns the
number (up to 5, without the `}` of an embedded reply, and found even when the text runs past the screen
limit), the history record keeps the source in 9 columns and the number, settings byte `0x903E`
(`b[0x14BEA]`, free per the v59 scan) keeps which record it is, and the send state goes to `'K'`. The loop
leaves the screen by itself, because the firmware never transmits with the menu open: the signal loop's
next round (`gancho_ida_ptt`) starts TX, which closes the menu, and `gancho_fim_tx` sends
`:SOURCE   :ackNUMBER`, with my callsign and the menu's path, after the 0.5 s of silent carrier. Every copy
of the message is acknowledged, since the previous ack may not have arrived. SEND no longer changes the
number: it only moves when the addressee or the text change (`aprs_guarda_texto`), and sending the same
message again repeats it.

**Sending.** The callsign and the addressee are saved in uppercase (the editor has an abc mode) and only
with letters, digits and, in the addressee, the SSID's `-`. During the content, scramble (bit 1 of `0x31`)
and the subtone (bit 15 of `0x51`, which the stock firmware turns on with the level at `0x01E441BE` and off
with 0) are off, and come back before the stock end: scramble uses the same `0x71` as the tone.

**The ring history** ("HST3"). Each sector (`0xD6000` and `0xD7000`) has its own header, the marker and a
generation, a byte that moves on with every new sector, and 31 records of 128 B. When the receiving sector
fills up, the other one (the oldest) is erased and starts over with the next generation: the last 31 to 62
always stay. The sector that needs erasing is erased with capture stopped, outside the bits
(`hist_prepara`), and only as a last resort, if a burst does not allow it, inside the write. The record keeps
the text (up to 104 characters), the colour, the ack's source and number and the arrival time
(`TIQUE_MS`), and the "used" byte is written last: a power loss in the middle leaves an occupied slot
without "used", which the screen skips. A copy of the same text from the same station (with the same
number) arriving within 30 s, the one heard direct and through digipeaters, does not go in again.
`0xD7000` is only used erased or ours; with something else in either sector, nothing is written. v74's
"HST2" and v70 to v73's "HST1" are erased on the first entry.

**The loop no longer stops.** Keys are read without waiting for release (a held key only counts again once
released), held EXIT is timed inside the loop (a short press counts on release) and the redraw after a key
goes through the strips, between modem reads. The loop's stack is at 736 B at the call point: to fit, the
frame buffer went from 160 to 136 B, the four registers restored on exit fit in two variables and the copy
comparison reads 12 bytes at a time.

**The screen.** `*` turns the backlight off (`0x01E394D6(0)`, which v60's detour serves through the PWM)
and the loop keeps receiving; the next key only turns it back on. A held arrow repeats (after 0.8 s, every
0.15 s), and the history wraps around at both ends. The footer got its own background, the meter's unlit
segments one (the top bar's came out almost black on the LCD), with white text and `*:OFF`.

**The loop test on the PC** (`tests/teste_rx.py`). `mensagem.c` compiles whole with `TESTE_RX`, and
`tests/teste_rx.h` imitates the BK4819 modem as mapped (it only syncs on the `0x5A` pattern, hands raw bits
through an 8-word FIFO with a notice at 4 and counts the words that did not fit), the flash, the keys, the
backlight and the screen, which spends 15 ms per strip. It checks frames separated by a single flag (the v71
to v73 bug), both polarities, copies, the ack and its frame, the ring and the sector switch, the old history
conversion, the foreign sector left untouched, the FIFO without overflow and the keys. Noise before a packet
sometimes gives a sync by chance, which opens a 13.6 s capture: that happens on the radio too, and it does
no harm.

**The CHIRP driver** shows MODEM GAIN (`0x903D`, past the base class settings struct, read and written as a
raw byte), after Light PWM.

### v76: the APRS formats decoded, and V3.0T

`aprs_texto` now decodes the formats that used to arrive unreadable, at the owner's request, always in
local units (km/h, metres, degrees Celsius, course in degrees):

- **Mic-E** (`` ` `` and `'`), used by Kenwood and Yaesu radios and many trackers: the latitude, the
  hemispheres, the longitude's 100° offset and the message come in the 6 destination characters; the
  longitude, speed, course and symbol, in the next 8 bytes; then the radio type and the `xxx}` altitude in
  base 91. A standard message shows by name (EN ROUTE, IN SERVICE, OFF DUTY, EMERGENCY...), and a custom one
  (A-K) is left out. Mic-E arriving through an igate is decoded from the inner header's text destination.
- **Compressed position** (`/YYYYXXXX$csT`, base 91): latitude and longitude, and the course and speed in
  `cs` (course in 4-degree steps, speed 1.08^s − 1 knots). With bits 4:3 of `T` at 2, `cs` is the
  altitude, which is left out.
- **After the symbol** of a plain position: course and speed (`ddd/sss`), and a digipeater's PHG
  (`PHGphgd`: p² W, 10 × 2^h feet in metres, g dB).
- **In the comment:** the `/A=feet` altitude in metres, and the extra precision `!wXY!` (DAO) is dropped.
- **Weather:** a station with the `_` symbol and the positionless report (`_MMDDHHMM`): temperature,
  humidity, pressure and wind.
- **Object** (`;`) and **item** (`)`): "OBJ NAME" or "ITEM NAME" and their position.

The compressed position maths fits in 32 bits: the whole degrees come from `y / 380926` (or `x / 190463`),
and the remainder times 6000 is below 2^32. `decimal`, which was recursive, became `numero`, without
recursion.

**APRS DECODE** (the APRS folder, which now has 8 items): OFF shows the info as it came, to check it, and
applies to frames arriving after the change. In settings byte `0x903F` (`b[0x14BEB]`, free per the v59
scan): OFF only with `0xB1`; the stock RESET leaves 229, which means ON.

**The stack.** Decoding, done inside `guarda` on top of the 128 B record, took the loop to 848 B, out of
740. The record being built moved to video memory (`0x10190`, the screen's 160×32 strip), which the stock
firmware already borrows as scratch when rewriting a channel: `guarda` runs between one strip and the next,
and the screen is redrawn whole after a new frame. It is at 720 B.

**The version** shown by the VERSION item is now V3.0T (the `V0.12` text at `0x01E5FD10`, in `montar.py`).

### The modem modes on the 0 key (v77, experimental)

**What v75 showed.** With the gains compared on air, MEDIUM was the best and the others worse; even with
it, SYNC and BAD exceed 70% of the packets, and most of what is heard on another radio never becomes a
frame. That is the modem's limit: on receive, 0x58 only picks between fixed pairs, and no register sets the
tone (0x72 is the bit rate). On transmit the tone generator gets around it (0x71 switches between 1200 and
2200 Hz on every bit); on receive there is no shortcut.

**The 0x58 fields**, from the comments of the BK4819 driver in the UV-K5 firmware (OneOfEleven's, in
`BK4819_enable_mdc1200_rx`): 15:13 the TX mode (1 = FFSK 1200/1800, 3 = 1200/2400, 5 = NOAA SAME); 12:10 the
RX mode (7 = FFSK 1200/1800, 4 = FFSK 1200/2400, 0 = direct FSK and NOAA SAME; 1-3 and 5-6 undocumented);
9:8 the gain; 7:6 undocumented; 5:4 the preamble type; 3:1 the RX band (1 = 1200/1800, 2 = NOAA SAME, 4 =
FSK 2400 and 1200/2400; 3 and 5-7 undocumented); 0 the modem on. By them, v70's "wide band" (`0x3FC9`) is
band 4, the 1200/2400 one, and v70's mode 3 (`0x33C9`) was 1200/2400 with its own band: the matched pair,
which heard nothing.

**v77** puts on the APRS RX 0 key what has not been tested yet, each changing a single thing from the
previous mode (1), with the gain chosen on `#`. On the top bar, next to the gain, the `M1` to `M10` tag;
switching zeroes the counters, and each entry starts on M1.

| Mode | 0x58 without the gain | What changes from M1 |
|---|---|---|
| M1 | `0x3CC3` | nothing: the previous one (RX 7, band 1, bits 7:6 = 3) |
| M2 | `0x3C03` | bits 7:6 at 0, like OneOfEleven's MDC1200 receiver |
| M3 | `0x3CC5` | band 2 (the NOAA SAME one) |
| M4 | `0x3CC7` | band 3 (undocumented) |
| M5 | `0x20C5` | the NOAA SAME path: RX 0 with band 2 |
| M6 to M10 | `0x24C3`, `0x28C3`, `0x2CC3`, `0x34C3`, `0x38C3` | RX mode 1, 2, 3, 5 and 6 (undocumented) |

It is an experiment: there is nothing to prove in the disassembly (the stock firmware only uses `0x3FC3`, in
the TONE1200 roger), and only the air shows what each mode does. An undocumented value in 0x58 only touches
the modem; leaving writes 0 to it, as before. Code 13 is the 0 key: in the key table (`0x01E60369`) the last
column of the matrix is `*` (9), 0 (13) and `#` (17), and the long code of 13 is 33, the action of holding
0 (the voltage box). Up to v76 it left the screen, like the other keys without a function.

**The stack.** Computing the next mode inside the loop took its frame from 256 to 268 B (the item's stack
to 732 B, out of 740), even with the 0x58 write in a separate routine: it is the loop's register
allocation. The maths and the write together in a routine that takes the key (`modem_58`) bring it back
to 720 B.

**Checked on the PC:** `teste_rx.py` presses 0 eleven times and `#` once, and checks each value written to
0x58 against the register built field by field.

### The v77 result and the on-air recordings (v78)

**The v77 modes**, tested on air with the gain at MEDIUM: none decoded more than M1, and they were removed
in v78 (the 0 key leaves the screen again, like the other keys without a function). The register stays at
the previous `0x3CC3`, with the gain from `#`.

**Four recordings** of packets on the channel, made by the owner with a phone near the speaker (HE-AAC at
48 kb/s, 0.9 to 1.2 s of signal each), went through a demodulator written for them (a 700 to 2800 Hz
filter, correlation at 1200 and 2200 Hz over a one-bit window, 13 twist compensations from −12 to +12 dB,
a PLL clock, NRZI, destuffing and FCS), checked first on our TX's WAV (`firmware/build/aprs.wav`), where it
finds the frame and the callsigns. In all four, no frame with a good FCS; 3 to 7 flags in a row show up,
and what follows does not form a callsign. The power between 1000 and 2400 Hz is only ~10 dB above that
between 2800 and 3800 Hz (19 dB in the clean WAV), with no clipping: a lot of hiss. Through the acoustic
path there is no telling what already arrives bad over the air from what the speaker, the microphone and
the phone's compression spoil.

**The next step** is to measure the signal without the acoustic path: the radio's audio by cable (the
earphone output) into an audio input, or an APRS decoder on the phone listening to the speaker (APRSdroid
has an "AFSK via microphone" mode). If a software decoder gets most of what is heard, the limit is the
chip's modem, and it is worth looking for the received audio on a JieLi input to demodulate in software;
if not even it does, the signals arrive weak, and no firmware will recover them.

### APRSdroid, Direwolf and the transmission (v84, experimental)

**Receiving.** With APRSdroid (the APK from its site, on the "AFSK via Speaker/Mic" protocol) listening to
the speaker, the owner counted all the channel's traffic through a Baofeng's next to it, and about 5% through
the Bajeton's, also with the two radios' antennas swapped. But Direwolf (`atest`) decoded, without any fix, a
packet recorded from the Bajeton's speaker with the antenna swapped: the signal arrives fine, and what brings
APRSdroid down there is the acoustic path (the speaker, the volume and where the phone sits). The limit of
the radio's APRS RX is still the chip's modem, which has no 1200/2200 Hz pair.

**A trap in the comparison.** The first measurement put the packet's 2200 Hz 5 dB above 1200 Hz on the
Bajeton's speaker and 2 to 4 dB below on the Baofeng's, which looked like a receiver without de-emphasis.
They were different stations, and the balance between the two tones belongs to each transmitter; on the
radio, the stock de-emphasis is on ([section 25](#the-de-emphasis-v85)).

**Transmitting.** Direwolf reads `firmware/build/aprs.wav` (the frame from `mensagem.c`, made on the PC): the
encoding is right. On the air, none of our packets, heard by the Baofeng next to it, was decoded by
APRSdroid, which decoded the other stations through the same path. In a recording, the tone frequencies and
the 1200 bps check out, but the lone 2200 Hz bit of each preamble flag only reaches about 1700 to 1800 Hz on
the discriminator (on the other stations it reaches 2200), and about half do not even pass the midpoint: the
decoder reads them as mark. v84, experimental and not released, used one variant per send, with " T1" to
" T6" at the end of the text: T1 as before (tone 1's gain in `0x70` at 96 of 127, the roger's); T2 and T3 a
weaker tone (64 and 40); T4 TX low-pass 2 (bits 8:6 of `0x43`, 0 from the factory) at 6, the wide one in the
UV-K5 community firmwares; T5 64, the 6 and TX low-pass 1 off (bit 1 of `0x2B`); and T6 the gain switched
with the tone, 52 at 1200 Hz and 96 at 2200 Hz (+5.3 dB, the 750 µs pre-emphasis). Over many sends,
APRSdroid read none of them, and they were removed in v85: transmitting is back to v83's.

### Transmitting through the microphone (v86 to v88)

Since v88, APRS goes out like it would from a sound-card TNC. The microphone goes into the JieLi's ADC, and
each block of 256 samples at 16 kHz goes to the DAC (`0x01E41BA0` → `0x01E321F8`, or, with NR, through the
`aec` task, `0x01E41BE8`) and from there to the BK4819's microphone input. Opening the microphone
(`0x01E41CBA`) hands the audio interrupt the address of `0x01E41C06`, which takes each block to the DAC or to
NR, in an `r0 = imm32` at `0x01E41CEC`; the value (the last 4 bytes, at `0x01E41CEE`) is now `gancho_mic`'s.
Outside a send it goes on to `0x01E41C06`; during a send (`ESTADO_MSG` at `'D'`), it puts the AFSK in place
of the samples and calls `0x01E41BA0` directly, skipping NR (that routine duplicates the block into L/R
when the output is stereo).

**The AFSK** (`dac_prepara` and `dac_bloco`, in `mensagem.c`): the frame becomes line bits (NRZI and bit
stuffing, as in `afsk_manda`: a reference bit on space, the flags resting on mark and the last bit whole),
and each sample advances a 32-bit phase accumulator by the 1200 or 2200 Hz step at 16 kHz, on a 64-point
sine at −12 dB from full scale; each bit has 13, 13 and 14 samples (40 every 3 bits), and the phase is
continuous by construction. The state (the accumulator, the current bit and up to 1280 line bits) lives in
video memory (`0x10190`), like the APRS RX record (v76): while the content plays, the interface does not
run. The send waits for the last bit with a deadline (if the blocks never come, TX ends on its own) plus
120 ms of silence for the DAC to drain. On the PC, `teste_mensagem.py` (2b) demodulates the samples, checks
the phase and writes `firmware/build/aprs_dac.wav`, which Direwolf decodes.

**What the tests showed.** In v86, SEND gave only a silent carrier. v87, a diagnostic, replaced the PTT
voice with a 1 kHz tone through the same hook and, after a send, played through the tone generator one high
beep if microphone blocks had arrived during it, or two low ones if not: the Baofeng heard the tone on PTT
(the DAC reaches the BK4819's microphone) and one high beep on SEND (the blocks did arrive). The cause: on
the way into TX step 3 (PTT released, `0x01E47996..0x01E479AC`), the stock state machine sets the step to 3
and clears bit 2 of `0x30`, the chip's microphone ADC, and the content plays exactly in step 3. v88 turns
the bit back on during the packet and restores `0x30` afterwards. With it, APRSdroid listening to a
Baofeng next to it read the packet at the first try, even with the Baofeng's volume nearly at minimum.

The packet's level on air follows the voice path (MIC GAIN and the microphone AGC). Morse still uses the
tone generator, and `afsk_manda` stays in the code, checked on the PC, as a reference.

### The modem's limit on receive (v89 to v99)

With transmitting solved, receiving went after the same path in reverse: the received audio on a JieLi input,
to demodulate in software. Its ADC has the microphone and three line-in pairs (`0x01E33EFC(line, mask)`: 4, 8
and 12 on PA0/PA1; 16, 32 and 48 on PB7/PB8; 64, 128 and 192 on PB9/PB10). The factory never opens any: the FM
radio is the JieLi's built-in one (`0x01E571E4` compares the device with `fm_inside`), and the PB7/PB8 path
with the AMUX (`0x01E32A92`) is the SDK's external-FM one, unused here (PB7 is the backlight). The factory
line-in player (`0x01E4F5F4`) picks the pair by a configuration value the factory leaves at `0xFF`, and then
skips opening it. v89, a diagnostic, showed the port A and B registers in the menu with the radio on: PB8, PB9
and PB10 are outputs at 1, and only PA0/PA1 is an analog input without pulls, which is also how the SDK leaves
an unused pin. The owner wants silent reception (the microphone listening to the speaker is out) and no
opening of the radio, so the software path was left without a proven wire to follow.

On the chip itself, v90 and v91 let the 0 key on the APRS RX screen change `0x43`: the weak-signal filter equal
to the strong one, the RF filter two steps wider, the gain after the demodulator at 0 dB, and all three, with
each of the modem gains. The sound changed; decoding did not.

v92 stored on the data chip (`0xD8000..0xDFFFF`, free in the section 3 map, in 32 slots of 1 KB) the `0x5F`
words after each sync, with the polarity, `0x0B`, the gain and the RSSI, and the cable read them with the
extended `R`. Over two rounds, about 30 captures:

- the packets arrive and the chip syncs on them: there are preambles of 30 to 41 flags in a row, and in one of
  them the header came out whole and right (destination, source, path, `03 F0` and the start of the
  information, about 24 bytes);
- but wrong bits show up in the middle of the frame. A tone's lone bit disappears, doubles or comes early: in
  the preamble, the spacing between the minority tone's bits peaks at 8, with plenty of 1, 6, 7 and 16. The
  frame with the right header does not close even with two tones swapped at any position;
- the false syncs, one every ~15 s right after each re-arm, come from noise.

In the same round, APRSdroid listening to a Baofeng next to it read about 10 packets, and the Bajeton 2. With
the bits already decided by the chip, without each one's confidence, fixing three or more tones on a 16-bit
CRC would give more false frames than true ones. It is the BK4819 modem's ceiling with Bell 202: only a packet
that arrives without errors gets through, or with the one swapped tone the v70 fix repairs.

**v93** ran the modem at 2400 and 4800 bps (`0x72`), to read each APRS bit 2 or 4 times and decide on the
processor. It does not work: `0x72` scales the tones along with the speed (the 1200/1800 pair becomes
2400/3600 and 4800/7200), and the bits come out at random. In the same version, the old send through the tone
generator (`struct afsk`, `afsk_manda` and `agora_us`), unused on the radio since v88, became PC-test only, and
about 260 B went back to `.livre`.

**v94 and v95** read each of the BK4819's 20 status and measurement registers 256 times in a row (`0x0B` to
`0x0E` and `0x60` to `0x6F`; `0x02` and the FIFO are left out, because reading changes them), with a steady DTMF
tone on air, and stored them on the data chip. v94 rebooted during the 5120 reads in a row; v95 read and stored
one register at a time, with the watchdog cleared. Each read over the chip's bus takes 1.36 ms (736 a second).
Sixteen stayed still, and the ones that moved are measurements: `0x6F` (the audio level, a VU, not the
waveform), `0x6D` (at −14 and −15), `0x6E` and `0x66` by one or two units. No register carries the audio.

**v96** read the pins with the JieLi's slow ADC, the battery one (CON at `0x1E3100`, RES at `0x1E3104`). The
stock `adc_sample` (`0x01E35EAA`) builds CON as `0xF06E | channel << 8` and sets bits 4 and 6; the diagnostic
used the same without bit 5 (the interrupt request, which would make `0x01E41B12` stop the ADC), waited for bit 7
and restarted through bit 6, as `0x01E35FCA` does. The battery scan (`0x01E41A30`, on the timer) does nothing
while the queue's preemptive slot (`w[0x1514C]`) is not −1: the diagnostic took it, waited for the scan's
conversion to end and gave it back at the end. It took 512 readings of each channel from 0 to 14 (15 is the
PMU's), at 1842 a second, in two passes: the audio as the APRS RX screen leaves it, and the chip's audio open
with the amplifier requested (the stock `0x01E47E0A..1A`). By the BR23 pin map
([kagaimiq/jielie](https://github.com/kagaimiq/jielie/blob/main/chips/br23/iomap.md)), channel 6 is PB3, the
battery one (791, like the stock reading), and PA1 is channel 0; PA0 has no channel on this ADC, only on the
audio ADC's line input. No channel carried the tone. PA1 jumps when the amplifier turns on and slowly comes
back, like a floating pin; channel 3 (PA10) goes from 0 to 1023 with the amplifier and channel 11 (PC6, the
microphone) drops to 0. The tone was on air: in the second pass, the battery wobbles with it (the speaker
drawing current), by half an ADC step.

**v97** went, on the APRS RX 0 key, through the AFC (`0x73`: bit 4 disables, bits 13:11 the range, `000` the
widest; per the BK4819 documentation, `BK4819_Registers_V1.1`, it comes on at the widest range, and the factory
never writes it), off and in three ranges, and through the RX DC filter (`0x7E` bits 2:0, 0 is no filter; the
startup writes 6), at 0 and at 7. Decoding did not change; 0 lowers the volume and 7 raises it.

**v98 and v99** put together, in a COMBO, the filters before the modem in the position the documentation points
to for APRS: the RF filter at its widest, the weak-signal one the same, and the 25 kHz mode that doubles both
(`0x43` bits 14:12 and 11:9 at 7, 4.5 kHz, and 5:4 at 2), and the DC cancellation after the demodulator (`0x2B`
bit 14, which comes on in the chip and the factory turns off by writing 0 to `0x2B` in FM, `0x01E440BE`). The
stock `0x3028` (3 kHz, and 1.7 kHz on a weak signal, both doubled) is Quansheng's original UV-K5 value, which
the community firmware opens to 3.75 kHz and 2.5 or 3.75 kHz on a weak signal
([`BK4819_SetFilterBandwidth`](https://github.com/egzumer/uv-k5-firmware-custom/blob/main/driver/bk4819.c)). v99
added the modem's band 4 (`0x3CC9`, the 1200/2400 pair's, which holds the whole 2200 Hz) and the 1200/2400 RX
with it (`0x30C9`). The owner's phone recordings of the speaker pointed at the RF filter: in the Bajeton's 7
packets the 2200 Hz came 3.5 to 6.7 dB below the 1200 Hz, and in the Baofeng's 2.7 to 5 dB above (from
different stations, so it is a hint). On air, the COMBO read one or two packets more in one round, and in the
next ones no combination stood apart from chance.

**TA1JS** ([BI4MTL/uv-k5-firmware-ta1js](https://github.com/BI4MTL/uv-k5-firmware-ta1js), `app/aprs_minimal.c`)
receives with the same recipe (`0x58` = `0x3FC3`, sync `0xFEFE`, no chip CRC, the FIFO at 4 words), without
fixing bits and without publishing a success rate; and it transmits 1200/1800, which hardware TNCs do not
decode.

**The radio's second receiver**, the Si473x ([section 1](#1-the-hardware)), does not help: it goes up to 30 MHz
in AM and SSB and 64 to 108 MHz in FM, and APRS here is on 145 MHz.

With the radio closed, the received audio reaches no pin the JieLi reads, no register carries it, and no BK4819
filter or mode takes the modem off its ceiling with the 1200/2200 pair. The diagnostics were removed, and
receiving stayed as in v88. From here, only with the BK4819's audio taken by a wire to a JieLi input and a
demodulator in software.

### The stack

The content runs in the roger's slot, in the signal loop, and the RX loop in the item's set; both within
stock (620 and 740 B). The PTT-press hook sets "on air" before the start and calls the start and the stock
PTT press as jumps (its frame already popped), and `pilha.py` now treats a `goto` from our code to
another routine as in the stock graph (a tail call: the jumping routine's frame does not add up). V2.9T
gives the same numbers with the new `pilha.py`.

## 25. The AUDIO folder: EXPANDER and NOISE GATE, and what NR is (v79)

**EXPANDER** is the BK4819's RX audio expander, the twin of the MIC folder's compressor: below the 0 dB
point, each dB less becomes 2, 3 or 4 (LOW, MID and HIGH: 1:2, 1:3 and 1:4 in bits 15:14 of `0x28`, per the
UV-K5 community documentation). It undoes the compression of a station transmitting with a compander and
quiets the hiss in the pauses; with a plain station, a weak voice gets weaker. It is enabled by the same bit
3 of `0x31` as the compressor, and with it alone the compressor stays at ratio 0 (off). When on, bits 13:9 of
`0x28` go to `10101`, the UV-K5 recipe's (`0x6B38`, the 0 dB point at 86); when off, they stay as stock.

**NOISE GATE** is the RX audio noise gate: per the official Beken table cited in issue 739 of the UV-K5 v3
firmware, bit 8 of the same `0x28` turns it on and bits 7:0 are the level; 11:9 are the DC filter (`101` on
RX), and 13:12 are not described. Power-up writes `0x0B40` (`0x01E565F2`): the expander off and the gate on
at level 64. DEFAULT is that; OFF clears bit 8; LOW, MID and HIGH use levels 32, 76 and 88. The level is the threshold: the higher, the more it cuts. On
v80 HIGH, at 160, left the radio silent, and on v81 MID and HIGH went down from 96 and 160 to 76 and 88 (on
v79 there were only LOW and HIGH, 32 and 128).

**On the radio.** The default is today's: without touching the items, `0x28`, `0x29` and `0x31` stay as the
stock power-up leaves them. Only power-up writes `0x28` and `0x29` (`0x01E565F2`, `0x01E565FC`), and the
spectrum does not save them. `aplica_audio` (formerly `aplica_mic`) applies everything at power-up, in the
items' setters and in settings arriving by cable. The levels live in two settings bytes that nothing in the
stock firmware reads or writes (the v59 scan): EXPANDER at `0x9034` (`b[0x14BE0]`), as `0xC0` + the level,
and NOISE GATE at `0x9031` (`b[0x14BDD]`), as `0xE8` + the option (0 OFF, 1 DEFAULT, 2 LOW, 3 MID, 4 HIGH; v79's `0xE0`,
in another order, counts as DEFAULT). Without the mark the stock behaviour
holds, OFF and DEFAULT: RESET leaves 208 and 0. Both join the AUDIO group, alphabetically before NOISE
REDUCTION, and the menu table goes to 92 of 96 slots. The stack does not change.

**In CHIRP**, "Expander" and "Noise gate" come right after Noise reduction, as raw bytes with the mark (the
Modem gain pattern). The base class calls `0x9031` noise reduction and `0x9034` FM interrupt, but neither
applies to the BJ-8300: its noise reduction is `0x9032`, and FM interrupt is RT-920 only. A `1` the base
may have written to `0x9031` reads DEFAULT.

**APRS.** The chip's decoder probably takes the audio before these blocks (on v71, turning off de-emphasis
changed nothing), and the APRS screen does not touch `0x28`.

### What bit 0 of `0x31` is, and what NR is

**Up to v78 this document called bit 0 of `0x31` NR. It is AM.** The four writes (`0x01E44078`,
`0x01E440A2`, `0x01E4B0B8`, `0x01E4B0DE`) belong to the two routines that put the receiver in AM or FM:
`0x01E4404E` (through the entry `0x01E440C4`: r0 = 1 is AM) and the spectrum's (`0x01E4B0A0`, right after
its AM/FM switch, `0x01E4B04C`). In AM: bit 0 of `0x31` set, `0x42` = `0x6F5C`, `0x2A` = `0x7434` and
`0x2B` = `0x0500` (no 300 Hz high-pass and no RX de-emphasis); in FM, bit 0 clear, `0x6B5A`, `0x7400` and
`0`. Power-up writes `0x6600` to `0x2A`. The signal loop sets AM when reception starts
(`0x01E47B74..0x01E47BC6`) in the airband (108 to 136 MHz) or, from 18 to 1000 MHz, with RX MODULATION
(`b[0x15E2F]`, getter `0x01E3D10C`) at 1, and FM otherwise and when tuning again (`0x01E466B6`).

**NOISE REDUCTION is the microphone noise reduction, on TX, and it runs on the JieLi.** The item writes bit
0 of `b[0x14BDE]` (`0x9032`; getter `0x01E3D0E0`), which has two readers: the bar icon (`0x01E3B154`) and
the PTT path (`0x01E478F8`, right after the microphone gain, `0x01E477D8`), which builds the microphone path
with `0x01E42080(0, NR, 4, 17)`. Proven in the disassembly:

- the microphone goes into the JieLi ADC (MIC channel, opened at `0x01E33C2A` by `0x01E41CBA`) at 16 kHz, in
  blocks of 256 16-bit samples every 16 ms, handed by the audio interrupt (`0x01E32488` → `0x01E34180`) to
  `0x01E41C06`;
- with NR at 0, each block goes straight to the DAC (`0x01E41BA0` → `0x01E321F8`); at 1, to the SDK's `aec`
  task (`0x01E24522`), whose output goes to the DAC (`0x01E41BE8`). From the DAC to the BK4819's microphone
  input is wiring (assumption);
- the suppression (`0x01E23168`, running in `0x01E240BE`) is per frequency bin, with a 512 FFT and a 256 hop:
  each bin's gain comes from the noise estimate with the floor at 0.02 (−34 dB), strength 1.8 and output gain
  2.5, constants from the initialisation (`0x01E245F8`). The ~32 KB of weights at `0x01E1C02C..` are its own;
- the item only passes 0 or 1. There is an eight-level table (`0x01E41D26`), but level 1 is always the one
  used, and nothing was found reading its two fields that looked like the strength;
- the path is torn down at the end of TX (`0x01E445AC`) and is only built with the licence byte
  `b[0x146A0]`, checked at power-up against the chip's identifier.

**Received audio does not go through the JieLi.** In the stock firmware the JieLi ADC only reads the
microphone (on TX) and the line input of its internal FM radio (`0x01E571E4` → `0x01E33EFC`); closing the
squelch (`0x01E47E8E..`) only touches the BK4819 and the amplifier. A software APRS demodulator would need
the BK4819's audio on a JieLi input, and if the board wires it to one, the firmware never reads it.
### NR with levels, in the MIC folder (v80)

Since NOISE REDUCTION belongs to the microphone, it moved to the MIC folder, with OFF, DEFAULT, LOW, MID and
HIGH in the stock item's own byte (`b[0x14BDE]`, `0x9032`; item 48 with our getter, setter and options, like
ROGER and SCRAMBLE): 0 OFF, 1 DEFAULT, 2 LOW, 3 MID and 4 HIGH. DEFAULT is the stock On. The PTT path passes
the byte to the microphone path setup, which treats it as "on" if not 0 and as an index into the preset
table at `0x01E41D06`, which skips anything above 7 (`0x01E41D08`) and whose fields nothing reads. Above 4
(RESET leaves 200), the byte counts as DEFAULT, as the stock firmware treats it (on; the stock menu, which
only looked at bit 0, showed OFF).

The levels change the two core constants that decide how much to cut. The suppression's initialisation
(`0x01E41E4E`) writes into its block `[attr+120]` = 0.02 (`0x3CA3D70A`, the floor of each bin's gain,
−34 dB), `[attr+124]` = 1.8 (`0x3FE66666`, the strength), `[attr+128]` = 2.5 and `[attr+132]` = 0.3, and
calls `0x01E245F8` with `r0` = `attr` at `0x01E42078`; from there the floor and the strength go to the core
(`0x01E23168`, called at `0x01E248AE` with both in `r2:r3`). The call goes through `gancho_nr_inicia`,
which at LOW, MID and HIGH swaps the two and carries on with a tail jump, `r0..r3` as they came:

| Level | Floor | Strength |
|---|---|---|
| DEFAULT | 0.02 (−34 dB, stock) | 1.8 |
| LOW | 0.1 (−20 dB) | 1.2 |
| MID | 0.05 (−26 dB) | 1.5 |
| HIGH | 0.01 (−40 dB) | 2.5 |

LOW and MID cut less than DEFAULT, and HIGH more. The values were picked by ear: the gain formula
(`g = s² / (strength + (1 − strength)·s²)`, limited by the floor) shows the direction, not the on-air result.

**On the top bar**, NR and NOISE GATE get tags: hidden at OFF; at DEFAULT, in the colour of the others; at
LOW, MID and HIGH, green, yellow and red. Both levels go into the `nr` byte of `struct estado` (bits 3:0 and
7:4), which sits on the deepest stack path and could not grow, and the tag is a separate routine: inlined
twice, it took the bar's frame from 44 to 52 B. Without room before the voltage, it is left out. The roots
stay at 740, 620 and 696 B.

**The MORSE and APRS folder names (v83).** Their items lost the prefix the folder already gives: CALLSIGN,
DECODE, MESSAGE, PATH, RX, SEND, SSID and TO in APRS, and MESSAGE, SEND, SPEED and TONE in MORSE. MESSAGE and
SEND exist in both, and `montar.py`, which found each item by the text the radio shows and refused two equal
ones, now finds them by the internal key (`nome='APRS MESSAGE'`), with the text in a `texto` field that it
checks against the firmware title (`rotulo`); the preview draws the text, and `conferir.py` checks each
position's marks and texts by the key.

**The menu order (v82).** The NOISE GATE and NR options go in the order of how much they cut: NOISE GATE as
OFF, LOW (32), DEFAULT (64), MID (76) and HIGH (88), and NR as OFF, LOW, MID, DEFAULT and HIGH (the stock
DEFAULT cuts more than MID). The stored code does not change (0 OFF, 1 DEFAULT, 2 LOW, 3 MID, 4 HIGH; for NR,
1 is still the stock On): the menu getter and setter (`porta_menu_le`, `nr_menu_le`) map position to code
through a table, and the top bar and `0x28` use the code. In MODEM GAIN and on the APRS screen, MEDIUM and MED
became MID, as in the other items.

**RESET (v82).** SYSTEM > RESET has VFO and ALL. Both apply the factory settings (`0x01E4842E`: the first 32
bytes of the `0x9000` block from `0x01E607B1` and the next 39 from `0x01E603C3`), and VFO also the VFOs
(`0x01E3A5C0`); ALL also erases the 32 KB block at address 0 (flash command `0x52` at `0x01E4324C`, the
channels) and writes the default channels (`0x01E48336`). Neither reaches the sectors from `0xC1000` up: the
banks, WATCH, C's VFO, Morse, APRS and the history stay. Our items' bytes go back to today's defaults through
each one's mark, and `gancho_ajustes`, right after the factory settings, disarms sending and reapplies the
audio, the crystal and the backlight. The reset turns TDR on (the table's 1), and with WATCH in SINGLE
`gancho_ajustes` clears it since v82 (the same goes for a CHIRP upload).

### The DE-EMPHASIS (v85)

The AUDIO folder gained **DE-EMPHASIS**, the FM RX audio de-emphasis: ON, the stock one, and OFF. `0x2B`
holds the RX audio filters; per the UV-K5 community documentation, its bits turn things off (10 the 300 Hz
high-pass, 9 the 3 kHz low-pass and 8 the de-emphasis). The factory only writes it when switching between AM
and FM (`0x01E4404E`: 0 in FM, at `0x01E440BE`, and `0x0500` in AM, at `0x01E44090`) and in the spectrum's
switch (`0x01E4B0A0`: the same ones, FM as a tail jump at `0x01E4B0FC`); startup does not touch it. Both FM
writes go through `gancho_2b_fm`, which puts the item's value, and `aplica_audio` also puts it when the radio
is in FM (bit 0 of `0x31` off); AM keeps the stock `0x0500`.

Checked on the radio in v84, where the item was a test (RX FILTER, with three options): with bit 8 the audio
gets much brighter, that is, the de-emphasis comes on from the factory and the bit turns it off; bit 9 makes
no audible difference and was removed (the 3 kHz cut the recordings show is in another stage).

The item lives in bits 3:2 of the EXPANDER byte (`0x9034`, 1 = OFF), with the expander level in bits 1:0: the
mark becomes the high nibble alone, `0xC`. The earlier `0xC0` to `0xC3` read ON, and the RESET's 208 (`0xD0`)
still has no mark (EXPANDER OFF and DE-EMPHASIS ON). The CHIRP driver gained De-emphasis, and the Expander and
it each write their own field without touching the other.

## 26. Settings that got lost: NOISE GATE and saving on OK (v100 and v101)

**The settings log.** Sector `0x9000` of the data chip has 6 bytes of marks and 96 B records at
`0x9010 + n × 0x60`. The save (`0x01E3AB36`) builds the record from the 32 B at `0x14B6C` and the 39 at
`0x14BCC`, with the CRC-16/XMODEM of the first 94 in bytes 94 and 95 (`0x01E3A34C`), low byte first. The record
in use is the one of the first bit still at 1 in the marks, from bit 0 of the first byte on (`0x01E3A30A`). If
the new one equals the one in use (the CRC and the first 64 bytes), nothing is written; otherwise it goes after
the one in use, and the marks lose one more bit (the `FE FC F8 … 00` table at `0x01E5A411`). Past record 40,
the sector is erased and the new one goes to `0x9010`. Startup (`0x01E55C9A`) reads the record in use and, with
a wrong CRC or no record, puts the factory ones (`0x01E4842E`).

**NOISE GATE at startup (v100).** After copying the record, startup zeroes the bytes out of range:
`0x14BD7..0x14BDA` from 9 up (PF2, PF2 LONG PRESS, PF3 and one more), `0x14BDB` and `0x14BDC` from 16 up (each
side's zone) and `0x14BDD` from 2 up. That last one, `0x9031`, is where NOISE GATE (v79 and v80) keeps `0xE8` +
the level, and it went back to the default at every startup. The v59 scan missed the check because the offset
comes from `r1 + 6` (`0x01E55D26..0x01E55D34`: `if (r1 >= 2) { r1 = 0; b[r4+r0] = r1 }`). Nothing else in the
stock firmware reads or writes the byte: the `r1 = 0` (`0x01E55D30`) becomes a nop and the write puts back the
same value. NOISE GATE stays at `0x9031`, where the CHIRP driver reads it. The other checked bytes are the
factory's and their check stays: the values the menu allows fit the limits.

**Saving on OK (v101).** The factory only saves when the menu closes (`0x01E3C29C`), and only with the screen
mode on the menu (`b[0x1533D]` = 1): the `0x9000` log and the menu's side. With it on a channel (`b[0x15212]` =
1, which `0x01E3A4D0` sets with the channel number in `h[0x1583C]`), the channel (`0x01E3AC0A`, with the number
plus the zone `b[0x15215]` × 64 when browsing by zone, and the side's 31 B at `0x14DD3 + side × 31`); on VFO,
the side's VFO (`0x01E3A4FE`, `b[0x15204]`). Switched off with the menu open, even inside a folder, whatever had
changed was lost. `salva_menu` (`bancos.c`) does the same, in the same order (`0x01E3C2B0..0x01E3C30E`), on
every OK that confirms an edit: it runs in `b_menu_marca`, which the v55 detour already called after the set
(`0x01E49A86`), and the firmware only gets there with the menu open. Every kind of item goes through there, the
text editor included (its OK is also the one at `0x01E49996`). The three routines compare with the chip and
only write what changed. On the stack, the OK detour reaches 324 B and the interface stays at the stock 740 B.

Where each menu item saves (the get and set of the built table, checked in the disassembly):

| Where | Items |
|---|---|
| the `0x9000` log | SQL, STE, EXPANDER, NOISE GATE, DE-EMPHASIS, the MIC folder, DISPLAY (except MDF-C), DTMF, KEYS, RP-STE, RPT-RL, TONE, SC-REV, SYSTEM, ROGER, TOT, VOX, VOX DELAY, TTR, TX-A/B/C, CRYSTAL FINE, LIGHT PWM, MODEM GAIN and the APRS DECODE |
| the side's channel or VFO | the CHANNEL folder (BCL, ENCRYPT, RX MODULATION, SCRAMBLE, STEP, TXP, W/N), OFFSET, SFT-D, the TONES folder, SCAN ADD and CH-NAME |
| its own sector, on set | APRS (CALLSIGN, SSID, TO, MESSAGE, PATH), MORSE (MESSAGE, SPEED, TONE), BANKS, WATCH and MDF-C |

Startup clears the APRS and Morse SEND (`0x903A`) on purpose, so nothing transmits by itself.
