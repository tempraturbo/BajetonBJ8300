# 🔌 CHIRP with this firmware

<p align="center">
  <a href="chirp.md"><img src="../imagens/botao-portugues.svg" alt="Versão em português" height="56"></a>
</p>

[📖 Wiki](README.en.md) · [🏠 Home page](../../README.en.md)

## 📻 How to use CHIRP with this firmware

> [!CAUTION]
> **With V4.0T, use only the `bj8300_v4.0TEMPRA.py` module from this release.** It replaces the
> earlier ones (from `bj8300_v1.0TEMPRA.py` to `bj8300_v3.7TEMPRA.py`). The model in CHIRP is now
> **Bajeton → `BJ8300_v4.0TEMPRA`** (up to V2.8T it was under Baojie), and a `.img` image saved with an
> older module opens in it as usual. The earlier releases have their own module in each release.
>
> - **Do not use CHIRP's stock models** that serve the stock BJ-8300 (**Bajeton BJ7800** and
>   **Radtel RT-920**). They neither read nor write the banks, and they do not know the BEEP 2
>   roger or the new scramble frequencies: with BEEP 2 set, their settings screen does not even
>   open. RT-920 also offers "Zone or Channel", and in zone mode the radio gets stuck, because the
>   menu to leave it was removed.
> - **Do not use older versions of this module**: they do not know the new options, and a channel
>   with a new scramble frequency shows up as invalid.
> - The other way round holds too: the module **refuses** to write to a radio with the stock
>   firmware. And, like Tempra Link, it reads the firmware version and only works with V4.0T: on a radio with V3.7T
>   or an earlier one, use the module from its release, or flash V4.0T.

**1. Install the latest CHIRP.** Download **CHIRP-next** from
[chirpmyradio.com](https://chirpmyradio.com/projects/chirp/wiki/Download). The module uses the
current version's `radtel_rt900` driver; with an old CHIRP, step 3 fails with *"Invalid or
unsupported module file"*.

**2. Turn developer mode on (once).** Menu **Help** → **Developer Mode**. CHIRP warns *"Danger
Ahead"*: click **Yes**. Then it says *"Restart Required"*: click **OK**, then **close and reopen
CHIRP**. The mode stays on from then on.

**3. Load the module (every time you open CHIRP).** Menu **File** → **Load Module...** (it only
shows up in developer mode). CHIRP warns that loading a module is dangerous: click **Yes** and pick
`bj8300_v4.0TEMPRA.py`. The window turns **red** and the title shows `CHIRP **Module Loaded**`:
that is the sign the module is loaded. Closing CHIRP unloads it, so next time it has to be loaded
again.

**4. Read the radio.** With the radio on as usual and the cable plugged in, menu **Radio** →
**Download from radio...**:

| Field | Choose |
|---|---|
| **Port** | the cable's COM port |
| **Vendor** | **Bajeton** |
| **Model** | **BJ8300_v4.0TEMPRA** (not BJ7800, the stock model) |

CHIRP shows the *"Experimental driver"* notice, with the module's instructions: click **Yes**, then
**OK** to read.

> [!NOTE]
> If `BJ8300_v4.0TEMPRA` is not listed under **Bajeton**, the module is not loaded: go back to step 3.

**5. Edit.** On the right you get the list of the radio's parts:

- **Memories (VHF/UHF):** the 960 channels. The **Scramble** column and the channel's other
  properties are here.
- **Banks (VHF/UHF):** one column per bank; tick which banks each channel is in. To rename a
  bank, **double-click its column header**.
- **Settings:** the radio settings, with the 6-option roger and, in **BJ8300_v4.0TEMPRA Specific**,
  right after Noise reduction, **Expander**, **Noise gate**, **De-emphasis**, **EQ bass (RX)** and **EQ treble
  (RX)**, **Mic Gain** (with DEFAULT, in the radio's order), **Compressor**, **Mic AGC**, **EQ bass (TX)** and **EQ treble (TX)**,
  **Crystal fine** (in Hz, at 435.5 MHz), **Brightness**, **Light PWM** and **Modem gain**, plus **PF3 Key (Long Press)**, right after PF3, and
  **Scan threshold**, right after Scan Resume Method.
- **Settings → APRS (SMS) and Settings → Morse:** the APRS callsign, SSID, PATH, destination, message and DECODE, and the
  Morse message, speed and tone. Both messages are the last one sent, which the `#` boxes open with. Writing APRS and Morse needs V3.0T, and the EQ, V3.1T.
- **Settings → Banks:** one block per bank, from **BANK 1** to **BANK 16**:

  | Row | What it is |
  |---|---|
  | **Name** | the bank's name (up to 12 characters), editable here too |
  | **Channels** | how many programmed channels the bank has (read only, counted when the page opens) |
  | **Enabled** | turns the bank on or off |

  A bank with no programmed channel **cannot be turned on**: if you tick it, CHIRP unticks it right
  away. And removing a bank's last channel turns it off by itself.
- **Settings → VFO A/B/C Channel:** **WATCH** (SINGLE, DUAL or TRIPLE), **TRIPLE WATCH** and **TX SIDE** at
  the top, and a block for each side (A, B and C), each with its SHOW and scramble. **VFO C** has the same fields
  as A and B, plus Work Mode and C's channel (**CH-C**). With C in focus on the radio, blocks B and C are
  read only: change them on the radio, or press A/B on it and download again.
- **Memories (FM / AM / HF):** the other bands, as in the stock driver.

> [!NOTE]
> After you tick and untick a checkbox, CHIRP draws it grey and its label in bold. That is just
> CHIRP's way of showing "changed" (it happens with every checkbox, the stock driver's too); the
> stored value is the unticked one.

**6. Write and restart the radio.** Menu **Radio** → **Upload to radio...**. After writing,
**turn the radio off and on**: the bank filter is built at boot.

Keep a copy of the first download (**File** → **Save**) before changing anything.

## How the CHIRP driver works

The file is [chirp/bj8300_v4.0TEMPRA.py](../../chirp/bj8300_v4.0TEMPRA.py). The name is deliberate: it
only works with **this** firmware, so nobody with a stock BJ-8300 uses it by mistake. It derives
from CHIRP's `radtel_rt900.py`, which already handles this radio as part of the RT-920/BJ7800
family, and adds:

- The **Banks** tab and the **Settings → Banks** group (above). An enabled bank with no programmed
  channel never reaches the radio: the upload turns it off before writing, as the radio itself does.
- Download and upload of the banks area through the extended command, together with the channels
  and **in a single session** (the firmware answers the closing `E` with an ACK and only closes the
  session afterwards); on upload, only the sectors that changed.
- The roger with the radio's six options and the scramble with its nine, writing the same values
  the radio's menu writes.
- WATCH, CH-C, SHOW C and VFO C, written to the WATCH record (`0xD2000`) and to C's VFO sector
  (`0xD3000`). The upload only writes what changes and checks it by reading it back, and the
  radio's state (C on side B, and B's channel, mode and VFO) comes from the radio, not from the
  image. SINGLE turns TRIPLE WATCH off, as in the firmware.
- **Noise reduction** read and written at the byte the radio uses (`0x9032`; the base driver uses
  `0x9031`), and **VFO A's Voice Privacy**, which the base driver did not write.
- **Mic Gain** at byte `0x9033` of the settings block, the same one the radio's MIC GAIN item uses
  (0 HIGH, 1 MID, 2 LOW, 3 DEFAULT; the radio reads any other value as DEFAULT), with the list in the radio's order, which
  comes from the calibration in the image, and **TX SIDE** in TX-A/B's 2 bits
  (`0x9013`), with C (3).
- **No zones:** every upload writes **Full Channel** (`0x9028` = 1), because zone mode is still in
  the firmware and, without the menu item, the radio would have no way out of it — in zone mode
  up/down is stuck within 64 channels.
- The stock-firmware lock: the banks firmware answers the extended read with size `0x40`, while
  the stock one echoes the size with bit 7 set and sends more than 128 bytes. The driver reads the
  header, discards the rest (the cable does not lose sync) and stops **before writing anything**.

Three test suites, none of them needing the radio:

- `python -I tests/teste_bj8300.py` — the banks core (stored format, checksum matching the one the
  radio itself wrote, multiple membership), with CHIRP stubbed out;
- `python -I tests/teste_chirp_real.py` — **against the real installed CHIRP**: whether the class
  fits the API (bank model, `apply_to` + settings callbacks), whether an image saved by the RT-920
  driver opens without blowing up, and whether roger and scramble end up as the bytes the radio
  writes;
- `python -I tests/teste_clonagem.py` — the **cable conversation**: CHIRP's whole `sync_in` and
  `sync_out` against a simulated radio that answers like the firmware (read from the disassembly),
  with both firmwares.

The cable's electrical side and the radio's response timing can only be tested on the real radio —
that was checked by hand with the tools in `tools/cabo/`.
