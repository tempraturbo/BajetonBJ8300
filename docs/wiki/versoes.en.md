# 🗂️ The releases

<p align="center">
  <a href="versoes.md"><img src="../imagens/botao-portugues.svg" alt="Versão em português" height="56"></a>
</p>

[📖 Wiki](README.en.md) · [🏠 Home page](../../README.en.md)

## Older releases

The latest one is in [firmware/releases/](../../firmware/releases/). The released ones are on the
[Releases page](https://github.com/tempraturbo/BajetonBJ8300/releases), and all of them, test versions included, in the
git history. The full story, with what broke and why, is in the [changelog](../changelog.md).

| File | Status |
|---|---|
| `BJ8300_BANK_v4.1T.anc` | **stable, the recommended one** — v172: V3.7T + the remote control over the cable and Bluetooth (with the radio's screen on the PC and the screen turned off on request), ROGER CUSTOM, the scan every 60 ms with THRESHOLD, the channel number while scanning and short `*` stopping it, Bluetooth that stays on during TX, the cable at 460800, the version over the cable and Bluetooth, SCR on every scramble frequency and VERSION opening with the firmware and hardware, and the spectrum line that always goes up and comes back to the default on entry; use it with Tempra Link and the CHIRP driver from the same release |
| `BJ8300_BANK_v4.0T.anc` | never released — v171 with the name; the spectrum line fix (v172) came in before the release, as V4.1T |
| `BJ8300_BANK_v3.7T.anc` | previous stable — v154: V3.6T + the Morse MESSAGES screen (what comes in, the history and the SEND box), the SENDING screen, the boxes opening with the last message, the silent screens, MESSAGES in one click, MONITOR on the side in focus, PF3 LONG and the flashlight screen; use with the CHIRP driver from the same release |
| `BJ8300_BANK_v3.6T.anc` | previous stable — v142: V3.5T + CH MEMORY, the rebuilt BANKS, the mode warnings, the lists with the cursor in the middle, the clear names, TRIPLE WATCH with DW and TW on the bar and the APRS (SMS) folder; use with the CHIRP driver from the same release |
| `BJ8300_BANK_v3.5T.anc` | previous stable — v139: V3.2T + SAVE that asks for the name, the TX tone and AIR labels, MIC GAIN with DEFAULT, the 90% PA cap, CALLSIGN before sending, the OK that saves the channel and a single EXIT on text items; use with the CHIRP driver from the same release |
| `BJ8300_BANK_v3.2T.anc` | previous stable — v115: V3.1T + the MESSAGES screen in place of APRS's RX, MESSAGE and SEND (`#` opens the message box and held `#` the TO one, and the screen comes back by itself after sending), the footer with P and RX GAIN in CALIBRATION; use it with the CHIRP driver from the same release |
| `BJ8300_BANK_v3.1T.anc` | previous stable — v111: V3.0T + the EQ in CHIRP (which V3.0T reset to DEFAULT on every cable upload), APRS RX with the FILTER setting the bass to −3 dB, the treble to +4 dB and SAVE off, key 0 turning the FILTER on and off, the footer with S, B and F, MORE below the message and the firmware cleanup; use it with the CHIRP driver from the same release |
| `BJ8300_BANK_v3.0T.anc` | previous stable — v110: V2.9T + the APRS folder (sending through the microphone input, receiving with the FILTER, the history and the ack) and MORSE, the bass and treble EQ on receive and transmit, the 24-segment S-meter, the AUDIO folder (EXPANDER, NOISE GATE and DE-EMPHASIS), NOISE REDUCTION with levels, LIGHT PWM, MODEM GAIN, NO TX and every setting saved on OK; use it with the CHIRP driver from the same release |
| `BJ8300_BANK_v2.9T.anc` | previous stable — v64: V2.8T + the MIC folder (COMPRESSOR, MIC AGC and MIC GAIN, which left TX), the CALIBRATION folder with CRYSTAL FINE, BRIGHTNESS in DISPLAY, who is receiving on the top bar with the menu open (without the old block over the menu) and the CHIRP driver under Bajeton; use with the CHIRP driver from the same release |
| `BJ8300_BANK_v2.8T.anc` | the previous stable one — v58: V2.7T with the amber background of the 3rd item in every folder and in the radio menu whole; use with the CHIRP driver from the same release |
| `BJ8300_BANK_v2.7T.anc` | the menu in folders with the 3rd item's background cut in half — v57: V2.6T + the menu in 13 folders (with the item's value, scrolling, OK opening straight into editing, BANK and CHANNEL taking several picks and VERSION read only), a bigger menu font, the radio menu in the same window, no CH-C item, the name editor counting only the letters and with only ABC and 123, and the voltage when holding `0` in the new theme, but the 3rd item's amber background comes out cut in half; use with the CHIRP driver from the same release |
| `BJ8300_BANK_v2.6T.anc` | triple watch with the menu as a single list — v51: v41 + WATCH (SINGLE, DUAL and TRIPLE), the full side C (VFO or channel), TX-A/B/C, MIC GAIN, the light on in the spectrum, TTR and the DTMF box in the new theme; use with the CHIRP driver from the same release |
| `BJ8300_BANK_v2.0T.anc` | the new screen without WATCH — v41 under the version's name (same file); use with the CHIRP driver from the same release |
| `BJ8300_BANK_v45.anc` | v41 + the `WATCH` menu (SINGLE: one side full screen; DUAL: the usual A/B; TRIPLE: a third frequency, C, picked in `CH-C` and shown as `MDF-C` asks, which dual watch watches with the same time as the others and the A/B key brings into focus) — tested on the radio |
| `BJ8300_BANK_v41.anc` | v39 + the spectrum in theme B, the radio settings checked on entry and on every modulation switch, a modulation switch ~2.5 s faster and the S of the medium font — tested on the radio |
| `BJ8300_BANK_v39.anc` | v36 + the other screens in theme B (menu with the medium font, name editor, radio, QT scan, DTMF and the power-on voltage), no Chinese language, both sides on a bank change and the `VERSION` menu at `V2.0T` — tested on the radio (a freeze when switching to USB/LSB came from what was saved in it: [changelog](../changelog.md), v40) |
| `BJ8300_BANK_v36.anc` | v34 + the new main screen (theme B): a card for the highlighted side with the big frequency and the meter, colored tags for power, modulation and tones, and a top bar with only what is on; in the Chinese language the screen stays the stock one — tested on the radio |
| `BJ8300_BANK_v34.anc` | v32 + the spectrum in the voice space, the green peak and the whole dBm on the AM/FM toggle; the last one with the stock main screen — tested on the radio |
| `BJ8300_BANK_v32.anc` | v27 without the voices (and without the VOICE item), COIN roger (the coin), memory read over the cable; frees ~112 KB for the new screen theme — tested on the radio |
| `BJ8300_BANK_v27.anc` | v1.0T + spectrum with the current bar in white, the running frequency, the peak line and the dBm on the right; BEEP 3 roger (the coin, called COIN since v31) — tested on the radio |
| `BJ8300_BANK_v1.0T.anc` | the first stable one, with the stock screen — v25 under the version's name (same file) |
| `BJ8300_BANK_v24.anc` | v23 + 2900 Hz scramble (the Baofeng UV-17L's mode 3) — tested on the radio |
| `BJ8300_BANK_v23.anc` | v21 + BEEP 2 roger — tested on the radio |
| `BJ8300_BANK_v21.anc` | v19 + spectrum twice as fast, 10 kHz step, 320 and 640 points and peak hold; darker highlight green (v20 without the boot loop; v20 was withdrawn) — tested on the radio |
| `BJ8300_BANK_v19.anc` | v18 + the arrow resumes the spectrum when stopped on a find; the menu table where the original was — tested on the radio |
| `BJ8300_BANK_v18.anc` | v17 with the green background on the RX/TX box too — not tested on the radio |
| `BJ8300_BANK_v17.anc` | VFO block highlight without the black stripes ([screen map](../screen-map.md)) — tested on the radio |
| `BJ8300_BANK_v16.anc` | first working highlight; thin black stripes remain between the block's lines |
| `BJ8300_BANK_v14.anc` | earlier highlight attempt: the green only showed on the signal bar |
| `BJ8300_BANK_v12.anc` | banks, arrows, texts, the scan fix and the 24-bit cable — tested on the radio |
| `firmware/original/BJ8300_V0.12.anc` | stock firmware, to go back |
