# Baojie BJ-8300 — channel banks, CHIRP driver and cable tools

> 🇧🇷 **Versão em português: [README.md](README.md)**

Modified firmware for the **Baojie/Bajeton BJ-8300** handheld (JieLi AC695X, pi32v2 CPU), adding
**16 real channel banks**, a **CHIRP driver** that edits those banks, and a set of tools that talk
to the radio over the programming cable.

Everything here came from reverse engineering the stock **V0.12** firmware, with no vendor
documentation. The findings are in [docs/reverse-engineering.md](docs/reverse-engineering.md) —
including the data-flash map, the cable protocol and the addresses of every routine used.

---

## ⚠️ Read this first

Flashing modified firmware carries risk. This project was built with two safety nets, and **both
were tested on the radio**:

1. **Recovery key combo** — powering the radio on while holding **PTT2 + the shortcut key** shows
   `UPDATE` on screen and waits for a flash. That path runs at startup, **before** anything the
   modification touches (the build script refuses to write into the routines involved; see
   `PROTEGIDAS` in [firmware/scripts/montar.py](firmware/scripts/montar.py)).
2. **Bootloader recovery** — the *"Click Here If Your Radio Can't Work"* button in BinTe ANC
   Bootloader, which does not depend on the app being healthy.

During development the radio did end up in a boot loop once (see
[docs/changelog.md](docs/changelog.md)) and came back through the key combo, without opening it.

**Keep the stock firmware**: it is in
[firmware/original/BJ8300_V0.12.anc](firmware/original/). Flashing it restores the original state —
channels and calibration live on a different chip and are never touched.

---

## What the modified firmware does

### Channel banks (the point of the project)

- **16 banks**, each with a **12-character name**.
- A channel can belong to **as many banks as you want**.
- **Several banks enabled at once**: the radio then sees their union.
- With any bank enabled, **up/down and scan only see channels from the enabled banks**; with none
  enabled the radio behaves exactly like stock.
- Banks live on the data flash (from 0xC1000), **outside** the channel area: rewriting channels
  from CHIRP or the vendor CPS never erases a bank.

### Other fixes and improvements

| What | Detail |
|---|---|
| **Selected VFO highlight** | The active side's block (name, frequency and signal bar) gets a green background; the other stays black |
| **Menu arrows inverted** | The up arrow now moves *up* the list instead of increasing the value |
| **BEEP 2 roger** | ROGER gets a fourth option: OFF, BEEP 1 (the stock one), **BEEP 2** (four tones, 1125/1857/1359/931 Hz) and TONE1200 |
| **2900 Hz scramble** | SCRAMBLE gets a third option: OFF, 3400 Hz (the stock one) and **2900 Hz**, the same inversion as the Baofeng UV-17L in mode 3 |
| **Better spectrum** | Twice as fast (160 points in ≈ 2.5 s); **10 kHz** step; **320 and 640 points**; peak hold (the bar rises at once and comes down slowly); and, stopped on a found signal, the arrow continues the sweep without changing the threshold |
| **Signal bar during scan** | The bar no longer freezes at the last channel found, and the channel number no longer drifts from the name |
| **English texts revised** | 25 strings fixed/standardised (e.g. `VOICEPRI` → `ENCRYPT`); the menu version now reads `V1.0T` |
| **Name editor** | Leaving the editor no longer leaves the keyboard's `abc` stuck in the menu header |
| **Extended cable command** | 24-bit reads (whole chip) and writes into the bank area — this is what the CHIRP driver uses |
| **Zone menus removed** | `ZONE` and `Zone Or Channel` were dropped from the menu, by explicit choice: banks replace them, and without those two the new menu table fits exactly where the original was |

The zone **function** is still in the firmware; only its two menu entries are gone. So the radio
cannot get stuck in zone mode, the CHIRP driver writes **Full Channel** on every upload.

---

## Flashing

1. Open `tools/anc-bootloader/BinTe ANC Bootloader.exe`.
2. Pick the `.anc` file, the COM port and **115200**.

   | File | Status |
   |---|---|
   | `firmware/releases/BJ8300_BANK_v12.anc` | **recommended** — tested and sound |
   | `firmware/releases/BJ8300_BANK_v24.anc` | v23 + **2900 Hz scramble** (the Baofeng UV-17L's mode 3) — **tested on the radio** with the UV-17L |
   | `firmware/releases/BJ8300_BANK_v23.anc` | v21 + **BEEP 2 roger** (100 ms per tone, like the mp3); use with this version's CHIRP driver — **tested on the radio** |
   | `firmware/releases/BJ8300_BANK_v21.anc` | v19 + **spectrum** twice as fast, 10 kHz step, 320 and 640 points and peak hold; darker highlight green (v20 without the boot loop; v20 was withdrawn) — **tested on the radio** |
   | `firmware/releases/BJ8300_BANK_v19.anc` | v18 + **the arrow continues the spectrum** when stopped on a find, without changing the threshold; the menu table now sits where the original was (code gains 1512 bytes of room) — **tested on the radio** |
   | `firmware/releases/BJ8300_BANK_v18.anc` | v17 with the green background on the RX/TX box too; **not yet tested on the radio** |
   | `firmware/releases/BJ8300_BANK_v17.anc` | **highlight of the selected VFO's block** without the black strips and with a darker green ([screen map](docs/screen-map.md)) — **tested on the radio**; only RX/TX keeps a black background |
   | `firmware/releases/BJ8300_BANK_v16.anc` | first highlight tested and working; thin black strips remain between the block's lines |
   | `firmware/releases/BJ8300_BANK_v14.anc` | earlier attempt at the highlight: it stopped v13's reset, but the green only showed in the signal bar |
   | `firmware/original/BJ8300_V0.12.anc` | stock firmware, to go back |
3. Power the radio on while **holding PTT2 + the shortcut key** until `UPDATE` shows (most
   reliable), or leave it on the normal screen and click **Update**.
4. Click **Update** and wait.

If you get `Retry1..5 / Failure!`: power-cycle the radio, make sure no other program holds the COM
port (CHIRP, the vendor CPS, scripts from this repo) and try again. The CH340 cable driver is in
[tools/ch340-driver/](tools/ch340-driver/) — install it from an **elevated** PowerShell with
`pnputil /add-driver CH341SER.INF /install`.

---

## Using banks on the radio

Three entries appear together in the menu, where the zone ones used to be:

| Entry | What it does |
|---|---|
| **BANK** | Arrows walk the 16 banks, showing `n name` plus ` ON` when enabled. **OK toggles** the bank and selects it for the other two entries |
| **BANK NAME** | Names the selected bank (12 chars), in the same text editor `CH-NAME` uses |
| **BANK CH** | Walks all 960 channels showing `bank-channel`; **OK adds/removes** the channel from the selected bank. ` IN` = in the bank, ` -` = channel empty in memory |

Behaviour notes:

- Enabling a bank with **no stored channel** filters nothing (the radio would be left with nothing
  to show) — the firmware undoes it by itself.
- Adding or removing a channel takes effect **immediately**, no restart.
- Writing banks **over the cable** (CHIRP) needs a power cycle: the filter is built at startup.
- Bank state is stored on the data flash and **survives power-off**.

The main screen looks the same: numbering stays **absolute, 1 to 960** (channel 500 is still 500,
not "the 3rd in the bank") and the enabled bank's name is not shown there. What changes is only
what **up/down and scan see** — with a bank enabled they jump straight from one marked channel to
the next.

---

## CHIRP driver

The file is [chirp/bj8300_v1.0TEMPRA.py](chirp/bj8300_v1.0TEMPRA.py). The name is deliberate: it
only works with **this** modified firmware, so nobody with a stock BJ-8300 picks it by accident.

1. In CHIRP, enable **developer mode** (`Help` → `Enable Developer Functions`) and restart.
2. `File` → `Load Module...` → pick the `.py`.
3. `Radio` → `Download From Radio`, model **Baojie BJ8300_v1.0TEMPRA**.

What it adds on top of the stock driver (it derives from `radtel_rt900.py`, which already covers
this radio as part of the RT-920/BJ7800 family):

- A **Banks** tab with one column per bank: tick channels in and out, rename banks.
- Under **Settings → Bancos**: enable/disable each bank and choose which one the radio's own menu
  entries edit.
- Bank area downloaded and uploaded through the extended command, alongside the channels; on
  upload, only the sectors that changed.
- **No zones:** banks take their place, and the driver shows nothing about zones. Every upload
  writes **Full Channel** (`0x9028` = 1), because zone mode still exists in the firmware and, with
  the menu entry gone, the radio would have no way out of it — in zone mode up/down is stuck within
  64 channels.
- It refuses to run if the radio does **not** have this firmware, so it can never write bank data
  over channels: the bank firmware answers the extended read with size `0x40`, while the stock one
  echoes the size with bit 7 set and sends more than 128 bytes — the driver reads the header,
  discards the rest (the cable does not desync) and stops with the right message.

Three test suites, all without a radio:

- `python -I tests/teste_bj8300.py` — 27 core checks (stored format, checksum matching the one the
  radio itself wrote, multi-bank membership), with CHIRP stubbed out;
- `python -I tests/teste_chirp_real.py` — 20 checks **against a real CHIRP install**: whether the
  class fits the API (bank model, `apply_to` + setting callbacks), whether an image saved by the
  RT-920 driver opens without blowing up, and whether edits end up in the image bytes;
- `python -I tests/teste_clonagem.py` — 16 checks of the **cable conversation**: CHIRP's full
  `sync_in` and `sync_out` against a simulated radio that answers like the firmware (read from the
  disassembly), for both firmwares — banks read and written with marker and checksum, only changed
  sectors, round trip, Full Channel, and the stock firmware refused with no extended write and
  without leaving the cable out of sync.

The cable's electrical side and the radio's timing can only be tested on the real radio — that was
validated by hand with the tools in `tools/cabo/`.

---

## Cable tools

In [tools/cabo/](tools/cabo/), PowerShell, nothing to install:

| Script | Purpose |
|---|---|
| `cabo.ps1` | Library: handshake, `Le-Bloco` (read), `Grava-Bloco` (write) — used by the others |
| `mapa_memoria.ps1` | Proves the 24-bit read, tells whether data comes in plain, finds the **real chip size** and lists what lives above 64 KB |
| `ler_memoria.ps1` | Dumps the 64 KB the stock protocol reaches and checks channels and calibration (read-only) |
| `teste_cabo_grava.ps1` | Proves writing into the bank area and that the firmware **refuses** writes outside it; cleans up afterwards |
| `logo.ps1` | Reads (and, on a firmware that allows that range, writes) the boot logo |

Boot logo conversion: `python -I firmware/scripts/logo_bmp.py para-radio input.bmp output.bin
preview.png` (160×138 24-bit BMP → RGB565, byte-for-byte identical to what the vendor CPS11
produces — verified against the radio).

> **Never send anything before the magic** (not `E`, not padding zeros): the radio matches the
> magic from the start of its receive buffer, so one stray byte makes every later attempt fail
> until the radio is power-cycled. And the magic ends in `U`, which is an UPDATE-mode command —
> retrying against an out-of-sync radio makes it jump into UPDATE on its own.

---

## Building

Needs the JieLi toolchain (clang targeting `pi32v2`) and Python 3:

```sh
sh firmware/build.sh
```

It compiles `firmware/src/bancos.c`, assembles the `.anc` from the stock image and **verifies** the
result: CRCs of all 12 entries, code and menu table present in all 6 flash images, menu entries,
every hook, the texts, and that no byte changed outside the expected patches — let alone near the
protected areas. The output ends with `RESULTADO: tudo certo` ("all good").

The toolchain path lives in `firmware/build.sh` (`C:/tools/jl-toolchain-2.5.3/...`).

**Space budget:** the only free area common to the 6 images is `0x1E96121..0x1E96DFF` = **3295
bytes**. Today that holds 1768 bytes of code plus 1512 bytes of the copied menu table. Anyone
hacking on this will hit that wall — [docs/reverse-engineering.md](docs/reverse-engineering.md)
explains how to measure it and where bytes were already clawed back.

---

## Repository layout

```
firmware/
  src/          bancos.c (the whole feature) and bancos.ld
  scripts/      .anc assembly, verification, disassembly, analysis
  original/     BJ8300_V0.12.anc — stock firmware
  releases/     ready-to-flash .anc files
  build.sh      compile, assemble and verify
chirp/          the CHIRP driver (bj8300_v1.0TEMPRA.py)
tools/
  anc-bootloader/  BinTe ANC Bootloader flasher
  ch340-driver/    USB cable driver
  cabo/            PowerShell tools that talk to the radio
  ghidra-scripts/  scripts used during analysis
logo/           boot image (BMP) and converter
tests/          CHIRP driver tests
docs/           documentation (reverse engineering, changelog) and working notes
```

---

## Credits and licence

- The CHIRP driver **derives** from `chirp/drivers/radtel_rt900.py` (Copyright 2025, 2026 Fred
  Trimble; 2024 Pavel Moravec, OK2MOP; 2023 Jim Unroe), distributed under **GPL-2.0-or-later** —
  the same licence applies to it.
- The stock firmware (and its part inside the `.anc` releases), the BinTe flasher, the CH340
  driver (WCH) and the `bajeton` logo belong to their makers and are kept here only for
  reproducibility and recovery.
- Everything else (bank code, scripts, tools and documentation) is by **tempraturbo**, under the
  **[0BSD](LICENSE)** licence: use, copy, modify and distribute it for any purpose, without even
  having to give credit.

No warranty of any kind. You are responsible for whatever you flash onto your radio.
