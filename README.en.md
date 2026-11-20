# Bajeton BJ-8300 — TEMPRA firmware V4.0T

<p align="center">
  <a href="README.md"><img src="docs/imagens/botao-portugues.svg" alt="Versão em português" height="56"></a>
</p>

New firmware for the **Baojie/Bajeton BJ-8300** radio, built by reverse engineering the stock V0.12 firmware: a
**folder menu**, **triple watch**, **APRS and Morse**, **16 channel banks**, a **new screen** across the whole radio and
**remote control from the PC**. It comes with **Tempra Link**, the program that flashes the firmware and programs the
radio over the cable. The "T" in the version stands for **Tempra**.

<p align="center">
  <a href="https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v4.0T"><img src="https://img.shields.io/badge/Download%20V4.0T-2ea44f?style=for-the-badge&logo=github&logoColor=white" alt="Download V4.0T"></a>
  <a href="docs/wiki/README.en.md"><img src="https://img.shields.io/badge/Full%20documentation-0969da?style=for-the-badge&logo=bookstack&logoColor=white" alt="Full documentation"></a>
  <a href="https://www.paypal.com/donate/?hosted_button_id=2VPWYBMKBSJX6"><img src="https://img.shields.io/badge/Donate%20with-PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white" alt="Donate with PayPal"></a>
</p>

> [!TIP]
> **💚 This project is free, and it lives on donations.** If your BJ-8300 got better with it, buy the project a coffee:
> every donation means time for new features, gear to test on and the next version coming out.
> [**Donate with PayPal**](https://www.paypal.com/donate/?hosted_button_id=2VPWYBMKBSJX6) · [the QR code](#-support-the-project)

> [!NOTE]
> **🎉 New in V4.0T:** Tempra Link, remote control from the PC, ROGER CUSTOM, a faster scan with THRESHOLD and Bluetooth
> that stays on during TX. [Everything that changed](docs/wiki/versoes.en.md)

---

## 📻 What the radio gets

| | | |
|:---:|:---:|:---:|
| <img src="docs/imagens/tela-inicio.png" alt="The new main screen" width="260"> | <img src="docs/imagens/tela-menu-pastas.png" alt="The folder menu" width="260"> | <img src="docs/imagens/tela-triple-c.png" alt="TRIPLE" width="260"> |
| **New screen**, with the S-meter | **Menu in 17 folders** | **TRIPLE:** three frequencies |
| <img src="docs/imagens/tela-aprs-rx.png" alt="APRS" width="260"> | <img src="docs/imagens/tela-morse-messages.png" alt="Morse" width="260"> | <img src="docs/imagens/tela-banks-canais.png" alt="The channel banks" width="260"> |
| **APRS:** messages and position | **Morse:** reads and sends | **16 channel banks** |
| <img src="docs/imagens/tela-espectro.png" alt="The spectrum" width="260"> | <img src="docs/imagens/tela-roger-custom.png" alt="ROGER CUSTOM" width="260"> | <img src="docs/imagens/tela-eq.png" alt="The EQ" width="260"> |
| A faster **spectrum** | **ROGER CUSTOM** | **EQ** on RX and TX |

**And more:** an S-meter from S1 to S9+60, a scramble that talks to the Baofeng UV-17L, a scan at one channel every 60 ms
with THRESHOLD, microphone compressor and AGC, crystal fine tuning, screen brightness, a full C side and remote control
over the cable and Bluetooth. **[Every feature, explained in the wiki →](docs/wiki/radio.en.md)**

---

## 💻 Tempra Link: the radio from the PC

The Windows program that comes with this firmware. It **flashes the firmware by itself** (from the stock one to TEMPRA,
with nothing to download by hand) and then programs the whole radio over the cable. A single `.exe`, nothing to install.

| | |
|:---:|:---:|
| <img src="docs/imagens/tempralink-remoto-en.png" alt="The remote control, with the radio's screen on the PC" width="430"> | <img src="docs/imagens/tempralink-canais-en.png" alt="The channels, like a spreadsheet" width="430"> |
| **Remote control:** the radio's screen, keypad and PTT on the PC | **Channels:** all 960, like a spreadsheet |

- 🔥 **Flashes the firmware:** downloads the right version from GitHub, checks it and flashes it over the cable; and goes back to stock.
- 📻 **Channels and banks:** copy and paste and CHIRP's CSV.
- ⚙️ **Settings:** the radio's menu, the VFOs, the APRS and Morse texts and DTMF.
- 🎵 **ROGER CUSTOM:** composed on a piano, or imported from MIDI and sound.
- 💾 **A backup** of every reading, and it **updates itself** from GitHub.

**[⬇️ Download Tempra Link](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v4.0T)** ·
**[📖 The manual, with every screen](docs/wiki/tempra-link.en.md)**

---

## 📥 How to flash the firmware, step by step

**From the stock firmware to TEMPRA in about 3 minutes**, over the radio's USB programming cable (the CH340 one, the
same the stock CPS uses). Two ways:

### 🚀 With Tempra Link: automatic

1. Download and open `TempraLink-BJ8300_v1.0.exe`, from the [V4.0T release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v4.0T).
2. With the radio on and the cable plugged into the PC, click **Read from radio**. Seeing the stock firmware, it offers
   to download and flash V4.0T: **Yes**.
3. When it asks, turn the radio off and turn it on holding **PTT2 + the shortcut key**, until `UPDATE` shows, and
   confirm.
4. Wait about 3 minutes and turn the radio off and on: `SYSTEM > VERSION` shows `Firmware: V4.0T`.

| | |
|:---:|:---:|
| <img src="docs/imagens/tempralink-fabrica-en.png" alt="With the stock firmware, it offers V4.0T" width="430"> | <img src="docs/imagens/tempralink-gravando-en.png" alt="Flashing in progress" width="430"> |
| It sees the stock firmware and offers V4.0T | Flashing: the red bar until the end |

### 🔧 With BinTalk: the factory flasher

1. Download `BJ8300_BANK_v4.0T.anc` and `BinTalk-V1.1.zip` from the [V4.0T release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v4.0T),
   and open `BinTe ANC Bootloader.exe`.
2. Pick the `.anc`, the cable's COM port and **115200**.
3. Turn the radio off and turn it on holding **PTT2 + the shortcut key**, until `UPDATE` shows, and click **Update**.
4. Wait for `Finish` and turn the radio off and on.

Channels, settings and calibration are not touched by flashing, and UPDATE mode runs before any modified part: you can
go back to stock whenever you want. **[The full step by step: the cable driver, what to do if it fails and going back
to stock →](docs/wiki/gravar.en.md)**

---

## 📻 How to use CHIRP with this firmware

Load the `bj8300_v4.0TEMPRA.py` module from the [V4.0T release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v4.0T)
into CHIRP (**File → Load Module**, with developer mode on) and pick **Bajeton → BJ8300_v4.0TEMPRA**. CHIRP's stock models
(Bajeton BJ7800 and Radtel RT-920) do not work with this firmware. **[The step by step →](docs/wiki/chirp.en.md)**

---

## 📖 Full documentation

> [!IMPORTANT]
> **Every feature of the radio and of Tempra Link is explained in detail, with the screens, in the
> [📖 project wiki](docs/wiki/README.en.md).**

| | |
|---|---|
| 📻 **[Radio features](docs/wiki/radio.en.md)** | the 17-folder menu, TRIPLE, APRS, Morse, the banks, the new screen and everything that changed from V0.12 |
| 💻 **[Tempra Link](docs/wiki/tempra-link.en.md)** | the manual, page by page, with the screens |
| 📥 **[Flashing the firmware](docs/wiki/gravar.en.md)** | both ways, the cable driver, errors and going back to stock |
| 🔌 **[CHIRP](docs/wiki/chirp.en.md)** | the module, the banks in CHIRP and how the driver works |
| 🗂️ **[Releases](docs/wiki/versoes.en.md)** and **[changelog](docs/changelog.md)** | every version, what changed and why |
| 🔬 **[Reverse engineering](docs/reverse-engineering.md)** | how the stock firmware was opened and modified |
| 🛠️ **[Development](docs/wiki/desenvolvimento.en.md)** | building, the cable tools, the space budget and the repository layout |

---

## 💚 Support the project

This firmware came out of many hours of reverse engineering: the stock firmware opened byte by
byte, with no vendor documentation at all, every routine mapped in the disassembly, and 49
versions, nearly all of them flashed and tested on a real radio, until the new screen and triple
watch. Everything here is, and will stay, **free**, to use, study and modify.

If your BJ-8300 got better — the banks sorted out your channels, TRIPLE watches three frequencies,
the scramble now talks to your friend's Baofeng, the spectrum got faster —, consider buying the project a coffee. Any amount
helps: it means time for new features, gear to test on, and the next version coming out.

<p align="center">
  <a href="https://www.paypal.com/donate/?hosted_button_id=2VPWYBMKBSJX6"><img src="https://img.shields.io/badge/Donate%20with-PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white" alt="Donate with PayPal"></a>
  <br><br>
  <a href="https://www.paypal.com/donate/?hosted_button_id=2VPWYBMKBSJX6"><img src="docs/imagens/doacao-paypal.png" alt="QR code to donate with PayPal" width="128"></a>
  <br>
  <sub>Point your phone's camera at the QR code, or click the button.</sub>
</p>

**Thank you!** Every donation is a sign that it is worth carrying on. 📻

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
