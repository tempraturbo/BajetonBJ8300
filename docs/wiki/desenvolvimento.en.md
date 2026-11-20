# 🛠️ Development

<p align="center">
  <a href="desenvolvimento.md"><img src="../imagens/botao-portugues.svg" alt="Versão em português" height="56"></a>
</p>

[📖 Wiki](README.en.md) · [🏠 Home page](../../README.en.md)

## Cable tools

In [tools/cabo/](../../tools/cabo/), PowerShell, nothing to install:

| Script | Purpose |
|---|---|
| `cabo.ps1` | Library: handshake, `Le-Bloco`, `Grava-Bloco` (the others use it) |
| `mapa_memoria.ps1` | Proves the 24-bit read, tells whether data comes raw, finds the **real chip size** and lists what lives above 64 KB |
| `ler_memoria.ps1` | Dumps the 64 KB the stock protocol reaches and checks channels and calibration (read only) |
| `teste_cabo_grava.ps1` | Proves writing to the banks area and that the firmware **refuses** to write outside it; cleans up at the end |
| `logo.ps1` | Reads (and, on a firmware that unlocks the range, writes) the boot image |

Boot image conversion: `python -I firmware/scripts/logo_bmp.py para-radio input.bmp output.bin
preview.png` (24-bit 160×138 BMP → RGB565, the same the CPS11 produces, checked byte for byte).

> **Never send anything before the magic** (not `E`, not zeros): the radio matches the magic from
> the start of the buffer and any byte in front makes every following attempt fail until the radio
> is turned off. And the magic ends in `U`, which is an UPDATE-mode command — insisting with the
> radio out of sync makes it drop into UPDATE by itself.

## Building

Anyone can build the firmware on Windows with just what is in the repository: the JieLi compiler
(clang for `pi32v2`) comes with it, in [`tools/jl-toolchain-2.5.3`](../../tools/jl-toolchain-2.5.3/LEIA-ME.md),
and only two free programs are missing.

**1. Install, once:**

| Program | Where | Detail |
|---|---|---|
| **Git for Windows** | <https://git-scm.com/download/win> | the installer with its default options will do; it brings the `bash` that runs the build |
| **Python 3.8 or newer** | <https://www.python.org/downloads/> | on the installer's first screen, tick **"Add python.exe to PATH"** |

**2. Get the repository:** `git clone https://github.com/tempraturbo/BajetonBJ8300.git`, or, on GitHub,
**Code → Download ZIP** (and unzip it).

**3. Build:** double-click **`compilar.bat`**, at the repository root. Or, in Git Bash, inside the
repository folder:

```sh
sh firmware/build.sh
```

It takes about two minutes. At the end you see `RESULTADO: tudo certo`, the four stack lines with `ok`
and the finished file, **`firmware/build/BJ8300_BANK.anc`**: that is what you flash on the radio
([how to flash](../../README.en.md#-how-to-flash-the-firmware-step-by-step)). If Git or Python is missing, `compilar.bat`
says which and where to get it.

The build compiles `firmware/src/bancos.c` and `firmware/src/tela.c`, builds the `.anc` from the original
and **checks** the result: CRCs of all 12 entries, code and menu table present and identical in all 6
flash images, menu items, every hook and patch, texts, and that no byte changed outside the expected
ranges — nowhere near the protected areas. Then it measures the **stack**
(`firmware/scripts/pilha.py`): the new code runs on its caller's stack, and its deepest path must not
go past what the stock firmware already uses there. The build is reproducible: the same source gives
the same `.anc`, byte for byte, so you can compare the sha256 with the release's. Before building it
checks the sha256 of the compiler's five files; to use another copy of the compiler, point the
`JL_TOOLCHAIN` variable at it.

**Optional:**

- **Screen preview on the PC** (below): `pip install ziglang`.
- **Morse and APRS test** (the packet, the AFSK, the decoder and the Morse reader on the radio's captures, on the PC):
  `python -I tests/teste_mensagem.py`,
  with the same zig; and the whole APRS receive loop, with the modem, the flash and the keys imitated:
  `python -I tests/teste_rx.py`.
- **CHIRP driver tests:** `python -I tests/teste_bj8300.py` needs nothing;
  `python -I tests/teste_chirp_real.py` needs CHIRP
  (`pip install https://codeload.github.com/kk7ds/chirp/tar.gz/refs/heads/master --no-deps`).
- **Studying the stock firmware:** the build leaves the whole disassembly at `firmware/build/app.dis`,
  and the scripts in `firmware/scripts` work on it: `desmonta.sh` (one stretch), `contexto.py` (who calls
  an address, with the arguments), `fluxo.py` (the paths inside a routine) and `escritas.py` (who
  reads or writes a RAM address).

**Space budget:** the new code lives in nine places, all at the same address in the 6 images:

- **the free area at the end of the image**, from `0x01E96124`, with 3260 bytes (V4.1T uses 3258);
- **the space that was the voices'**, with the spectrum, but only inside the files C0 and C1
  (1066 bytes, of which V4.1T uses 1056), because the radio's loader refuses an image that changes
  more voice files;
- **the place of the G.729 and MP3 decoders**, inside `app.bin`, with the B screens, WATCH, TRIPLE,
  the folder menu, APRS and the menu table (38884 bytes, of which V4.1T uses 38882);
- **the voice paths**, in `app.bin`, only with what our code calls (2944 bytes, of which V4.1T uses 2944);
- **the stock menu table**, dead since v43, in `app.bin` (1512 bytes, of which V4.1T uses 1510);
- **the dead part of the path table** (`.sons`), in `app.bin` (472 bytes, of which V4.1T uses 468);
- **stock item names with no pointers** (`.mortos1` to `.mortos10`), in `app.bin`, in 10 pieces (422 bytes, of which V4.1T uses 396);
- **dead stock code** (`.mortos11` to `.mortos20`), which takes the Morse and APRS routines moved there in v156, so the remote control fits (1004 bytes, of which V4.1T uses 990);
- **the stock routines of the items that left the menu** (`.mortos21` to `.mortos30`), with no caller since v43 (782 bytes, of which V4.1T uses 737).

The menu table has lived in the place of G.729 and MP3 since v43, in a 97-slot array (V4.1T uses all 97: the 17
folders and the 80 items); the original, with 54, became code space in v140. [docs/reverse-engineering.md](../reverse-engineering.md)
explains how to measure it, where space was already saved and why these places are safe.

**Screen B preview on the PC:** `python -I previa/previa.py <folder>`, from inside `firmware/`,
writes every screen scenario as PNG. It needs zig (`pip install ziglang`), which serves as the C
compiler.

## Repository layout

```
compilar.bat    double-click build (Git for Windows and Python 3)
TempraLink-BJ8300.exe  Tempra Link: programs the radio through the cable (or, experimentally, Bluetooth)
firmware/
  src/          bancos.c (banks, menus, spectrum, cable), tela.c (main screen B) and bancos.ld
  fonte/        screen B's 5x7 font and icons, as text
  previa/       screen B preview on the PC
  scripts/      build (.anc), checks, stack, disassembler, analysis
  original/     BJ8300_V0.12.anc — stock firmware
  releases/     .anc files ready to flash
  build.sh      compiles, builds and checks
chirp/          CHIRP driver (bj8300_v4.1TEMPRA.py)
tools/
  jl-toolchain-2.5.3/  the JieLi compiler (the 5 executables the build uses)
  anc-bootloader/  BinTe ANC Bootloader flasher (BinTalk V1.1)
  ch340-driver/    USB cable driver
  cabo/            PowerShell tools that talk to the radio
  ghidra-scripts/  scripts used during analysis
logo/           boot image (BMP) and converter
tests/          CHIRP driver tests
tempralink/     Tempra Link's source (Delphi), the menu generator and the tests
docs/           Tempra Link's manual, reverse engineering, changelog, screen map, images and working notes
```
