# Version history

> 🇧🇷 Português: [historico.md](historico.md)

Dates are 2026. Every version was flashed and tested on the radio; whatever broke is recorded with
its cause, because that is what stops it happening twice.

## CHIRP driver — Oct 29 — Light PWM

- In **BJ8300_v2.9TEMPRA Specific**, after Brightness, **Light PWM** (`0x903B`): 240 Hz, 580 Hz, 1 kHz,
  5 kHz and 25 kHz, in frequency order, as in the radio's menu. The list stores the radio's value, not the
  position (240 Hz = 3, 580 Hz = 4, 1 kHz = 0, 5 kHz = 1, 25 kHz = 2), and an out-of-range value (the
  factory reset's 189) reads 240 Hz, v69's default.
- `teste_chirp_real` got `10za` to `10zd`.

## v69 — Oct 29 — LIGHT PWM at 240 Hz by default, and half a second of carrier before APRS (to be tested)

`firmware/releases/BJ8300_BANK_v69.anc` (sha256 `5936c260…`). The v68 test: at 240 Hz, "no flicker and no
interference". It becomes the default: an out-of-range value (the factory reset leaves 189 at `0x903B`)
means 240 Hz, and values already saved stay the same. And, at the owner's request, APRS got 0.5 s of
unmodulated carrier before the flags, so the radio at the other end (battery saving, slow squelch) opens
before the packet.

## v68 — Oct 29 — LIGHT PWM also at 240 and 580 Hz ✔️ tested: 240 Hz with no flicker or interference

`firmware/releases/BJ8300_BANK_v68.anc` (sha256 `5125f62b…`). At 1 kHz the radio held close still picked up
interference. At the owner's request, LIGHT PWM got 240 Hz and 580 Hz, to compare; the menu shows all five in
frequency order, and the values already saved stay the same (1 kHz is still the default).

## v67 — Oct 29 — LIGHT PWM at 1 kHz, without the pin strength test ✔️ tested: 1 kHz still with interference

`firmware/releases/BJ8300_BANK_v67.anc` (sha256 `77d092dd…`). The v66 test: LIGHT DRIVE (the backlight
pin's strength) changed nothing in the light, and was removed; the pin stays as stock. LIGHT PWM at 1 kHz
caused no interference on the radio held close ("it got better and did not cause interference"), and stays
as a real setting in the CALIBRATION folder, with 1 kHz as the default: saved at `0x903B` (1 kHz, 5 kHz or
25 kHz). The rest is v65 (Morse and APRS), still being tested.

## v66 — Oct 29 — v65 and the backlight edge test ✔️ tested: pin strength changed nothing, 1 kHz without interference

`firmware/releases/BJ8300_BANK_v66.anc` (sha256 `44586860…`). v65 with two temporary items in the
CALIBRATION folder, to find what removes the backlight PWM interference on a radio held close: **LIGHT
PWM** (25 kHz, 5 kHz or 1 kHz) and **LIGHT DRIVE** (the backlight pin's output strength, the four HD0 and
HD combinations, showing the one on the pin when opened). Nothing is saved: on power-up it all goes back.
They only matter with the light below 100%. [Section 23](reverse-engineering.md#23-the-screen-backlight-by-pwm-v60-diagnostic).

## v65 — Oct 29 — the MORSE and APRS folders (replaced by v66 before being flashed)

`firmware/releases/BJ8300_BANK_v65.anc` (sha256 `2e297940…`). The owner asked for Morse and APRS because
they are standards: the radio can talk to any other, of any brand and firmware. Details in
[section 24](reverse-engineering.md#24-morse-and-aprs-v65) of the reverse engineering doc.

- **MORSE folder:** MORSE MESSAGE (up to 37 letters), MORSE SEND, MORSE SPEED (5 to 40 wpm) and MORSE
  TONE (400 to 1200 Hz). Morse goes out in FM through the tone generator, like the roger.
- **APRS folder:** APRS CALLSIGN, APRS SSID, APRS TO (the addressee; empty sends a status), APRS MESSAGE,
  APRS SEND, APRS PATH (WIDE1-1, WIDE1-1,WIDE2-1 or none) and APRS RX. Sending is APRS's AX.25 in Bell 202
  AFSK (1200/2200 Hz) through the tone generator, each bit timed from TIMER1; receiving uses the chip's FSK
  modem in raw capture, and the decoding (NRZI, destuffing, FCS) is ours.
- **Sending through the stock TX:** SEND only arms; a hook on the PTT press keys up like the PTT (the menu
  closes, as with the PTT in the menu) and another at the end of TX plays the message in the roger's
  place. PTT or any key stop the Morse midway.
- **The text editor goes to 37 characters** (the most its RAM holds), shows what does not fit one line on
  two, and the 0 key got a space on the first press and `- / = + @`.
- Morse, the packet, the AFSK and the decoder were checked on the PC against independent implementations
  (`tests/teste_mensagem.py`). Still to be proven on the radio: receiving through the chip's modem (APRS's
  space is 2200 Hz and the modem is 1200/1800) and the AFSK on a real receiver.
- `pilha.py` treats our code's jump to another routine like the stock one (V2.9T gives the same numbers
  with it).

## V2.9T — Oct 29 — the MIC folder, CALIBRATION, screen brightness and who is receiving in the menu ✅

v64 became **V2.9T**: `firmware/releases/BJ8300_BANK_v2.9T.anc` (sha256 `f782e4a6…`, the same file;
v64 already came out named V2.9T). Tested on the radio: "it worked perfectly, what we needed". It is
V2.8T with the MIC folder (COMPRESSOR, MIC AGC and MIC GAIN, which left TX), the CALIBRATION folder
with CRYSTAL FINE, BRIGHTNESS in DISPLAY, who is receiving in an amber box on the top bar with the menu
open (without the old block over the menu) and the CHIRP driver under Bajeton
(`bj8300_v2.9TEMPRA.py`).

Published in the [V2.9T GitHub release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.9T), together with the CHIRP module, the flasher and the cable
driver. The README points to it, shows the 15 folders and the new screens (who is receiving, the MIC
folder and CRYSTAL FINE) and keeps V2.8T among the earlier ones.

## CHIRP driver — Oct 29 — the V2.9T name, under Bajeton, and the new settings

- The module became `chirp/bj8300_v2.9TEMPRA.py`, and the model in CHIRP, `Bajeton →
  BJ8300_v2.9TEMPRA`: the owner asked for it to show up in the Bajeton list, not in Baojie's.
  `MODEL_COMPAT` takes images saved as Baojie (from v1.0T to `BJ8300_v2.8TEMPRA`) and as Bajeton, and
  all of them open in this one.
- In **BJ8300_v2.9TEMPRA Specific**, after Mic Gain: **Compressor** (`0x9035`), **Mic AGC**
  (`0x9036`), **Crystal fine** (`0x9037..0x9038`, in Hz, from −2000 to +2000 in steps of 10) and
  **Brightness** (`0x9039`). What the factory reset leaves in those bytes (197, 181, `0xC4C0` and 163)
  reads Off, On, 0 and 100%, as the firmware does.
- `teste_chirp_real` got `10v` to `10z` (the new settings) and `2c6` to `2c8` (the Baojie and Bajeton
  images).

## v64 — Oct 29 — the indicator goes away, redrawing the bar every time ✅ (it is V2.9T)

`firmware/releases/BJ8300_BANK_v64.anc` (sha256 `f782e4a6…`), named V2.9T. The diagnostic answered (the owner's photos,
menu open): idle, `E3 A0 F0 G0 28`; receiving, `● RX E4 A1 F1 G0 41`; after the other radio's PTT, `E3 A0 F0
G0 5x`. The counter kept moving, so the level reading is still called after the reception (about once a
second), and the state and the amplifier request go back to normal at once. The fault was the mark of what
the bar shows: v62 and v63 kept it in `b[0x7031]`, the meter level already drawn, and the firmware resets it
on its own (`b[0x7031] = 255` at `0x01E45A70`, which the loop calls at `0x01E46F52`). With the mark reset, the
indicator thought the bar was clear and did not redraw it. Now, in the menu, `b_sinal` redraws the bar on
every call, with no mark at all, as the diagnostic did on the radio; `b[0x7031]` belongs to the firmware
alone again.

The build passed ("tudo certo", with step 6's stack equal to stock) and is reproducible.

## Repository — Oct 29 — it builds on its own, and the end-of-reception diagnostic

- **Building without installing the compiler.** The owner asked that a layperson be able to build with
  everything at hand. The five executables of the JieLi 2.5.3 toolchain the build uses (37 MB of the
  install's 155 MB) went into `tools/jl-toolchain-2.5.3`, with `SHA256SUMS` and their origin in
  `LEIA-ME.md`; building with just them gives the same firmware, byte for byte. `build.sh` and
  `desmonta.sh` use them (or the folder in the `JL_TOOLCHAIN` variable), and the build checks their
  sha256 and Python before compiling. `compilar.bat`, at the root, builds with a double-click and says
  what is missing if Git or Python is not installed. A copy with only the repository's files, with no
  build folder and nothing in `C:\tools`, built v63 identically.
- **The stack in the build.** The scripts that measure the stack lived in old sessions' scratch
  folders; now they are `firmware/scripts/pilha_fabrica.py` and `pilha.py`, and the build runs them at
  the end (step 6: the three roots and the menus' worst get/set/exibe) and stops if any goes past
  stock. The stock disassembly (`build/app.dis`) is generated the first time.
- **This round's analysis scripts** went into `firmware/scripts`: `contexto.py` (who calls an address,
  with the arguments), `fluxo.py` (the paths inside a routine) and `escritas.py` (who reads or writes a
  RAM address, to be validated first on a known one).
- The README explains building step by step, for someone who has never built anything.

**End-of-reception diagnostic** (`firmware/releases/BJ8300_DIAG_RX.anc`, sha256 `7a81c174…`): with v63,
the amber indicator showed but still did not go away when the reception ended. With `-DDIAG_RX`
(`build.sh` takes `CFLAGS_EXTRA`), in the menu the bar shows, in place of the icons, the receive state
(E), the receive amplifier request (A), the flags `b[0x1536F]` (F) and `b[0x15368]` (G) and a counter
that moves on every `b_sinal` call (in `0x903A` of the settings, free by the validated scan), and
`b_sinal` redraws the bar on every call. Without the option, the firmware comes out as v63.

## v63 — Oct 29 — the indicator goes away when the reception ends, in amber, and the V2.9T name (tested: amber, but it still did not go away)

`firmware/releases/BJ8300_BANK_v63.anc` (sha256 `ddabab5a…`). The owner, with v62: "now it shows, but when
I let go of the other radio's PTT it does not go away ... to bring it back I have to touch the menu".

- **The indicator goes away.** v62 only looked at the receive state (`b[0x15367]` = 4), and in the menu
  it does not leave 4 when the carrier drops. From state 4 the loop's code reaches no write of the
  state: the return to waiting happens inside called routines. The mark of what the bar shows
  (`b[0x7031]`) is not to blame: a scan, validated first on the known reads of `0x706D`, only finds
  writes to it in the channel scan, the QT scan and the level reading itself. Now the indicator also
  requires the receive audio open: the receive amplifier request (`b[0x129D4]`, which `PEDE_AMPLI(5)`
  sets on entering reception, `0x01E47E1A`, before setting the state to 4, and `PEDE_AMPLI(6)` clears
  when the signal drops, `0x01E47A9E`, neither looking at the screen mode). It is the same audio you
  hear open and close. The preview got the byte on screens 57.
- **In amber**, at the owner's request: the letter and the text in a box like the receiving band on
  the main screen (dark amber background, amber badge with a dark letter and amber text), starting
  after `● RX`, which stays as it was.
- **The V2.9T name**, at the owner's request: `VERSION` shows `V2.9T`, and the CHIRP driver became
  `chirp/bj8300_v2.9TEMPRA.py` (model `Bajeton → BJ8300_v2.9TEMPRA`). Images saved under any old name,
  Baojie or Bajeton, open in it (`teste_chirp_real`: `2c` to `2c8`). The README and the GitHub release
  come once it is tested.

The build passed ("tudo certo"), the stack is stock's at the three roots and the build is reproducible.

## v62 — Oct 29 — the menu indicator for real, the backlight without PWM at 100% and when off, and CHIRP under Bajeton (tested: the indicator showed and did not go away)

`firmware/releases/BJ8300_BANK_v62.anc` (sha256 `baa4c988…`): v61 with three fixes, after the owner's
tests.

- **The menu's who-is-receiving indicator never showed** (v59): "that tag with the MDF name that sits
  on the bar in the menu when someone transmits ... is not showing, but that old-style channel
  message stopped appearing". v59 relied on the meter, but the signal loop only calls it in screen
  modes 0 and 3 (`0x01E46216`), and the end of a reception does not go through it at all. The preview
  faked the call the radio does not make, which is why it showed the indicator. Now it comes from the
  routine that reads the signal level (`0x01E46166`), which the loop calls every 50 passes in any
  screen mode: in the menu, a detour at its entry compares the receive state (4 = receiving) with what
  the bar shows and redraws it when they differ, and every bar drawn in the menu already shows who is
  receiving. The preview no longer calls the meter on those screens and uses the receive state.
- **The dimmed backlight's interference.** The owner, with v60: "when I leave 2 radios very close ...
  the Baofeng picks up noise ... and sometimes the Bajeton itself does, but rarely", and "when the
  light goes off the interference stops". Every PWM edge spreads a little RF, which a receiver right
  next to it hears. Now the PWM only exists with the backlight on below 100%: at 100%, the default,
  and with the backlight off, the pin leaves the channel and is a plain output again, exactly as
  stock, with no edges at all. The Brightness help in CHIRP warns about the interference.
- **CHIRP under Bajeton.** At the owner's request, the driver's vendor went from Baojie to Bajeton,
  the radio's brand, where CHIRP also lists the BJ7800. Images saved under Baojie, the V2.8T module's
  included, open in it through `MODEL_COMPAT` (`teste_chirp_real`: `2c6` and `2c7`). The README
  changes when the version is released: today it explains the V2.8T module, which is Baojie.

The build passed ("tudo certo", `4m` with 24 entry detours), the stack is stock's at the three roots
(the new detour reaches 388 B, within the signal loop's 620) and the build is reproducible.

## v61 — Oct 28 — BRIGHTNESS (replaced by v62 before being flashed)

`firmware/releases/BJ8300_BANK_v61.anc` (sha256 `19b9a190…`): v60 with the **BRIGHTNESS** item in the
DISPLAY folder, in place of the diagnostic's fixed level: 100%, 60%, 35%, 20% and 10%, spaced for the
eye and with no 0%, so the screen is never unreadable. 100% is the default, the stock brightness. The
level lives in `0x9039` of the settings (`b[0x14BE5]`), free like v59's; the factory RESET leaves 163
there, which counts as 100%. It takes effect at boot, on the item's set (the backlight is on in the
menu, so it changes at once) and when the settings change outside the menu. The CHIRP driver got
Brightness, right after Crystal fine (`teste_chirp_real`: `10y` and `10z`), and the preview, screen 58.

**The proof of the free bytes, redone.** v59's scan saying nothing reads or writes `0x9035` to `0x9038`
was looking at the wrong address: the offset was added by hand (`0x14680` + 2401 = `0x14FE1`; the right
one is + 1377, `0x14BE1`). The code was right, only the proof did not hold. Redone by script for
`0x9035` to `0x9039` (the constant, the offset from `0x14680`, halfword and word), it finds nothing, and
the same scan finds the four real uses of the neighbouring NOISE REDUCTION byte (`0x14BDE`), which shows
it works.

The build passed ("tudo certo"), the stack is stock's at the three roots and the build is reproducible.

## v60 — Oct 28 — PWM backlight diagnostic, level fixed at 25% ✔️ tested

`firmware/releases/BJ8300_DIAG_LUZ25.anc` (sha256 `f914160a…`): it is v59 with the screen backlight driven
by PWM, at a fixed 25% level, with no menu item. The owner asked for a brightness setting; the PWM
comes from a timer the firmware does not use and reaches the backlight pin through one of the chip's
output channels, and what the disassembly cannot answer, only the radio can: whether the backlight
dims without flicker, whether the PWM hums in the RX or TX audio, and how it behaves with ABR, SAVE and
on the way back from flashing mode. If it works, a BRIGHTNESS item goes into the DISPLAY folder. What
was proven before building is in
[reverse engineering, section 23](reverse-engineering.md#23-the-screen-backlight-by-pwm-v60-diagnostic).

The build passed ("tudo certo", `4m` with 23 entry detours), the stack is stock's at the three roots
and the build is reproducible. Tested on the radio: "the light worked, it got dimmer and there was no
noise, not on the radio, not on TX, not on RX".

## v59 — Oct 28 — the MIC folder, CALIBRATION and the menu while receiving ✔️ tested

Three of the owner's requests in one version:

- **The menu while receiving.** With the menu open, on receive, the stock block of the side in focus
  showed up over the folder window: the old badge (C came out as B), the name and the channel number.
  It was stock's whole screen, which the signal loop calls on receive; its hook and the side-in-focus
  one handed the drawing back to stock outside the main screen. In the menu they no longer draw.
  Instead, as the owner suggested, the top bar shows who is receiving: `● RX`, the letter's badge (A,
  B or C) and the name, frequency or channel number, as that side's MDF asks, in place of the icons,
  which come back when the signal goes. The menu does not close on receive, and only the side in focus
  keeps receiving, as in stock: entering the menu disarms dual watch, which only arms on the main
  screen, because its trip to the other side swaps the "current" channel, which is what the menu items
  read and write.
- **The MIC folder:** COMPRESSOR (OFF, LOW, MID and HIGH), MIC AGC (ON and OFF) and MIC GAIN, which
  leaves TX. The radio chip is a BK4819, now with proof: the table boot writes to register `0x09` is
  the UV-K5's. It has a compressor on the TX audio, which the firmware leaves set to 2:1 and off, and
  the microphone AGC on. LOW, MID and HIGH compress 1.33:1, 2:1 and 4:1.
- **The CALIBRATION folder**, with CRYSTAL FINE: the crystal's fine adjustment, −2000 to +2000 Hz at
  435.5 MHz, in 10 Hz steps, added to the factory calibration, which on the test radio is zero (index
  8, no fine value). It is proportional to the frequency, as a crystal error is: +120 Hz at 435.5 MHz
  is +40 Hz at 145 MHz. At 0, the factory one stays exactly as it is. The limit is 4.6 ppm, which at
  520 MHz is 2.4 kHz, inside the channel; the stock coarse range is ±4 ppm. Typed digits do not move
  it, only the arrows.
- **A one-item folder:** EXIT while editing would close the whole menu, as in a one-item quick menu;
  a new detour (`0x01E4D53C`) takes it back to the folder's list. The menu now has 15 folders.
- The three settings live in free bytes of the settings block (`0x9035` to `0x9038`), and what the
  stock RESET leaves there counts as OFF, ON and 0.
- **The CHIRP driver** got Compressor, Mic AGC and Crystal fine (Hz at 435.5 MHz), right after Mic
  Gain; `teste_chirp_real` got `10v` to `10x`.
- **The preview** finds folders and items by name (the numbers changed with the new folders), draws
  the menu's meter like `b_medidor` (the bar only) and got screens 53 to 57d.

The build passed ("tudo certo", with `3b` accepting the one-item folder and `4t` checking the new
detour), the stack is stock's at the three roots, the new get, set and exibe stay at up to 320 B, and
the sha256 is `d0b9ff28…`. `VERSION` still shows `V2.8T`. Tested on the radio (with v60, which contains it): "I tested
the mic and the crystal and everything worked". Details in
[reverse engineering, section 22](reverse-engineering.md#22-the-mic-folder-crystal-fine-and-the-menu-while-receiving-v59).

## V2.8T — Oct 27 — the menu in folders, with the 3rd item's card whole ✅

v58 became **V2.8T**: `firmware/releases/BJ8300_BANK_v2.8T.anc` (sha256 `11efdc26…`). Tested on the radio: "it worked
here". It is V2.7T with the amber background of the 3rd item in every folder, and in the radio menu,
whole; the `VERSION` menu shows `V2.8T` (`0x01E5FD10`). The owner asked for the new name after the
test, which is why the v58 flashed on the radio still showed `V2.7T`: apart from that text, the
firmware is the same.

Published in the [V2.8T GitHub release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.8T), together with the CHIRP module, the flasher
and the cable driver. The README points to it, describes the fix in the box at the top and keeps
V2.7T among the earlier ones, with the flaw; the screens of the items and of the radio menu had
already been redone with the card whole.

## CHIRP driver — Oct 27 — the V2.8T name

- The module became `chirp/bj8300_v2.8TEMPRA.py`, and the model in CHIRP, `Baojie →
  BJ8300_v2.8TEMPRA`. `MODEL_COMPAT` got `BJ8300_v2.7TEMPRA`, and an image saved under any of the
  five old names opens in this one. `teste_chirp_real` got `2c5`.
- Apart from the name, nothing changed: V2.8T only fixes how the menu is drawn.

## v58 — Oct 27 — the 3rd item's card whole again ✅ (it is V2.8T)

The owner, using V2.7T: "always on the third item of every menu the amber background is cut in
half", in any folder and in the radio menu too. The folder window is drawn in bands of 32 lines from
y 16 (16, 48, 80, 112), and each list line was only drawn in the band its top (y − 2 to y + 12) falls
in. The cursor card also covers the value line, down to y + 23: on the 3rd item it runs from y 63 to
87 and crosses the y 80 boundary, and the lower band did not paint what lies past it, the bottom half
of the value line. In the other positions the card fits in one band, or the lower band already drew
the line (the 4th item); with the list scrolled, the cursor sits on the 5th line. Now the cursor line
is drawn in every band its card touches. The owner confirmed the design: the amber background
covering the item and the "L" value, when there is one, groups the selection.

The preview missed it because it drew in bands from 0 (0, 32, 64, 96): there the cut fell on the 2nd
item (y 64), and went unnoticed in screens 43 and 45 and in the two README images taken from them.
The preview now splits the menu into the firmware's own bands (the folder window from 16; outside it
the summary, the title and each row separately) and got screens 47b and 51b, with the cursor on the
3rd item. The README images of the items and of the radio menu were redone.

Tested on the radio: "it worked here". The build passed ("tudo certo"), the stack is stock's at the
three roots, the menus stay at 640 B, and the sha256 is `301a399b…`.

## V2.7T — Oct 26 — the menu in folders ✅

v57 became **V2.7T**: `firmware/releases/BJ8300_BANK_v2.7T.anc` (sha256 `93bf62bb…`). v56, which
already carried everything from v52 to v55, was tested on the radio ("it worked", except for the
empty CHANNEL group), and v57 is v56 with that fix. The `VERSION` menu shows `V2.7T` (`0x01E5FD10`)
since v52: the owner asked for the new name together with the fixes.

Published in the [V2.7T GitHub release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.7T), together with the CHIRP module, the flasher
and the cable driver. The README got, as a highlight, the menu-in-folders section, with three
screens from the preview (choosing the group, a group's items with the value in the "L" and the
radio menu) and the table of the 13 groups; the two menu screens it already had (the list and
TX-A/B/C) were redone in the folder window, so the old menu no longer shows.

One unproven report: in the v56 test, a text in the old style showed up once in the title and was
gone at once. It was not found in the disassembly; the owner will say if it comes back.

## CHIRP driver — Oct 26 — the V2.7T name

- The module became `chirp/bj8300_v2.7TEMPRA.py`, and the model in CHIRP, `Baojie →
  BJ8300_v2.7TEMPRA`. `MODEL_COMPAT` got `BJ8300_v2.6TEMPRA`, and an image saved under any of the
  four old names opens in this one. `teste_chirp_real` got `2c4`.
- The help for the chosen bank names `BANKS > NAME` and `BANKS > CHANNEL`, the new names in the
  menu, and the texts name V2.7T (TX-A/B/C on C asks for "V2.6T or later").
- The CH-C field stays: the item left the radio's menu in v54, but C's channel is still where C
  starts in TRIPLE, and the cable writes it.

## v57 — Oct 26 — the CHANNEL group shows its items again ✅ (it is V2.7T)

The v56 test gave "it worked", except for group 3, CHANNEL, which showed no items while choosing the
group. While choosing the group there is no cursor (`FORA`, 255), and the scroll computation used
it: in CHANNEL, the only group with 7 items and so the only one that scrolls, the start of the list
went past its end and no line was drawn. Now the list only scrolls while choosing the item, and
while choosing the group it shows the first ones. The preview already showed screen 41 empty and it
got through, because only the screens that were meant to change were looked at: since then, every
screen that changed is looked at.

## v56 — Oct 26 — the bigger letters and the radio menu in the folder ✔️ tested: "it worked", except for the empty CHANNEL

The owner's two requests after seeing v55:

- **Bigger menu letters**, in the folder window and in the open item (the title and the options),
  without running into the scroll bar. It is the medium font with the middle row 2 px tall too (10
  in all: the bars of A, E, F, H, P and R come out like the top and bottom ones) and 1 px more
  between letters; what does not fit falls back to the old medium (NOISE REDUCTION). The window
  became 6 lines of 12 px.
- **The radio menu** (FM, AM, SSB) in the same window. It is the same engine, with one of two stock
  tables (`0x01E61168` FM and AM, `0x01E6137C` SSB and CW, chosen at the entry `0x01E49000`) and the
  number of items the band has (`0x01E3DE36`: FM 2, AM 5, SSB and CW 6). It sits in a single
  folder, with `RADIO` and the band in the title and without the group dots; the memory item, whose
  title is built by a function (`0x01E3D20E`: `FM`, `AM` or `SSB` plus `MEMCH`), has no "L". Numbers
  count from 1, as in the main menu, and EXIT from the list goes back to the radio, as stock does.
- The stack: with the window's two texts at 28 B each, the interface reached 744 B, 4 above stock's
  740. They became 12 and 20 B, and it is back at 740.

## v55 — Oct 26 — the menu the way the owner drew it

After seeing the groups ("menu approved"), the owner changed how it is used:

- **The folder window inside the group too.** The items numbered; the one under the cursor on a
  card, with the OK arrow, and its value right below, in a green "L" the cursor does not select.
  Only the item under the cursor shows its value, and the action items (RESET, MEMCH, DELCH and the
  BANKS CHANNEL) have no "L". The value comes from the lines the engine already built
  (`0x01E487BC`): the one marked as saved (`02 10`) or, with no mark (the name, OFFSET, VERSION and
  BFO), the first one.
- **OK opens the item already editing** (stock OK); saving or cancelling goes back to the window,
  on the same item; EXIT goes back to the groups, on the group.
- **VERSION is read-only**: OK opens it without a cursor, OK and the digits do nothing while it is
  open, and only EXIT leaves.
- **BANK and the BANKS CHANNEL tick several**: each OK saves and editing goes on; EXIT leaves. A
  detour right after the set (`0x01E49A86`) clears the reception state, as stock does after
  changing a setting, and goes on to the redraw without closing the edit.
- **Scrolling.** With more lines than the window, the list scrolls so the cursor and the value stay
  in view; the up and down arrows only show when there is something to scroll, and between them
  there is a thin line with the position bar, thicker than it. The group dots stay.
- DELCH still deletes one channel at a time.

## v54 — Oct 26 — without the CH-C item

The owner's request: since v49 C is a side with a VFO, like A and B, and the item that chose its
channel no longer made sense. Without it, an empty C channel (deleted, or never chosen) would leave
C out of reach of the A/B key; now, in TRIPLE, the key always reaches C: with its channel empty it
goes to the next one that exists (`canal_existe`, by channel number; stock's next channel counts
inside the zone in zone mode), and with no channel at all C goes on its VFO. The WATCH group was
left with TTR, TX-A/B/C and WATCH, and the menu with 55 items. C's channel stays in the WATCH record,
and CHIRP still writes it over the cable.

## v53 — Oct 26 — the menu groups (never flashed; went straight to v54)

The owner's request: group the items by subject, always in alphabetical order, in a way a layman
understands, and show when an item is a group.

- **13 groups**, each with its items in alphabetical order: AUDIO, BANKS, CHANNEL, DISPLAY, DTMF,
  KEYS, MEMORY, REPEATER, SCAN, SYSTEM, TONES, TX and WATCH (the table is in the README). BANK NAME
  and BANK CH became NAME and CHANNEL, inside BANKS.
- **Two levels on the same engine.** The menu table, now with 80 slots, holds the groups and then
  each group's items; entering a group puts its table in the menu state (`0x14BF4`), and going back
  puts the groups' one. Each group is a list item with no get or set, with its items' names as the
  options. The hooks sit at the menu entry (`0x01E4CBB2`, which reopens in the group it was left
  from), at OK (`0x01E4C924`), at the digits (`0x01E4B9FA`, counting from 1, as the screen shows) and
  at EXIT outside editing (`0x01E4D54A`, which goes back to the groups).
- **The folder window**, in amber, when choosing the group: the folder, the group's number and name
  in the title bar, its items in the body and a dot per group at the bottom. Inside the group, the
  item's title got the `G-k` chip (group and item).
- The map is in [reverse engineering, section 21](reverse-engineering.md#21-the-menu-in-folders-v53-to-v57)
  and in [the screen map, section 15](screen-map.md#15-the-menu-folder-window-v53-to-v57).

## v52 — Oct 26 — the name coming from CHIRP, the editor in capitals only and the voltage on 0

The owner's three requests after V2.6T:

- **A name written by CHIRP opened with `??` at the end** (the editor showed `SBST RADIO??`, 12/12).
  The name is always 12 B in memory; the radio fills what is left with 0 (`0x01E3C7BE`) and CHIRP
  with `0xFF`. The display stops at the first of the two, but the CH-NAME get (`0x01E3C732`) measured
  the name with the ROM strlen (`0x021127C0`, called at `0x01E3C784`), which only stops at 0, and
  opened the editor with the `0xFF`s as letters. The get's two ROM calls (the strlen and the copy,
  `0x01E3C792`) now go through `nome_tamanho` and `nome_para_editor`: the letters up to the first 0
  or `0xFF`, and zeros to the end of the buffer, which stock's `%15s` and the letter insertion
  (`0x01E49664`) count on. BANK NAME, the only other name editor in the firmware (the whole app was
  searched), uses the same count.
- **The name editor with capitals and digits only**, opening in `ABC`. The `#` key cycled `abc` →
  `ABC` → `123`; the "r0 = 0" at `0x01E4A5AA` became "r0 = 1", and the cycle is now `ABC` ↔ `123`.
  Both editors open in `ABC`.
- **Holding 0** (key action 33, `0x01E4CE96`) showed stock's voltage box, the old-style `VOLTAGE`.
  Its send (`0x01E4CF1C`) goes through `gancho_tensao_tecla`, which draws the start-up card over the
  screen; then the firmware waits 1 s and redraws everything, as before. The ` VOLTAGE` text left the
  translated texts, since only that action used it.
- `VERSION` now shows `V2.7T`.

## V2.6T — Oct 26 — three frequencies at once ✅

v51 became **V2.6T**, the third stable version: `firmware/releases/BJ8300_BANK_v2.6T.anc` (sha256
`9ef7cf3e…`). Tested on the radio: "now everything worked". The `VERSION` menu shows `V2.6T`
(`0x01E5FD10`). V2.5T was never released (below): the v50 test showed TX on C without C in focus,
and the fix came out under the new name.

Published in the [V2.6T GitHub release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.6T), together with the CHIRP module, the flasher
and the cable driver. The README got the TRIPLE section, with six screens from the preview (A
receiving, C receiving through the watch, C in focus, C on a VFO, TX on C and the TX-A/B/C menu),
and what changed since V0.12 now covers WATCH, side C, TX-A/B/C, MIC GAIN, the light in the spectrum
and TTR.

In use, the owner found v49's screen and commands a bit slower than on v45 (the three-side screen
and the drawing when reception starts), but acceptable. From the code, the drawing and reception
paths are the same as v45's; what weighs more is the A/B key going to C or coming back from it,
which since v49 writes the VFO sector, B's VFO record and the settings. To be watched in daily use.

## CHIRP driver — Oct 26 — the V2.6T name

- The module became `chirp/bj8300_v2.6TEMPRA.py`, and the model in CHIRP, `Baojie →
  BJ8300_v2.6TEMPRA`. `bj8300_v2.5TEMPRA.py` only existed in the repository (V2.5T was never
  released), but an image saved with it opens in this one too: `MODEL_COMPAT` registers the three
  old names. `teste_chirp_real` got `2c3`.
- The TX-A/B/C help text and the warning for a radio without the banks firmware name V2.6T.

## v51 — Oct 26 — the third in focus while PTT is pressed ✅ (it is V2.6T)

Tested on the radio: "now everything worked". The v50 test showed that, with TX-A/B/C on C, TX went
out on C's frequency, but C did not come into focus as A and B do. Now it does, still writing
nothing:

- **On PTT press**, for the third, `gancho_tx_abc` brings side B into focus with the stock swap
  (`0x01E40E52(0)`, the same as TX-A/B's), if A was in focus, and sets `b[0x15384]`: bit 7 is the
  third, bit 5 says the swap was ours.
- **At TX start**, after the disarm, `gancho_tx_desarma` puts the third in side B's current slot and
  decodes it without inverting the side, as the channel loader does with the side in focus; puts its
  power on side B and rebuilds side B's screen record with its data (`0x01E39128`: frequencies,
  tones, power and the name through the current channel). It sets bit 6 and keeps B's power in bits
  0..3. Side B's slots (the mode, the channel and the shadow) stay as they were.
- **The screen** shows what PTT put on side B: with bit 6, `watch_tela()` inverts `c_no_b` (B's card
  gets the third's letter and MDF) and gives, in bits 16..30, its channel for the card; and
  `terceira()` becomes the one that left side B, read from its slots (the VFO through the shadow of
  the side in focus, `0x15EA4`). The card shows the third transmitting, with the red meter, as A and
  B. v50's red strip is gone.
- **On the way back** (end of TX and refused TX), `gancho_tx_volta` clears the flag, gives the power
  back, brings B's shadow back to the current slot (`0x01E3FD9E`) and B's screen record, and goes back
  to A with the stock swap if that is what took it out of focus.
- The round-robin and TX read the third through the same routine (`le_terceira`); only the
  round-robin decodes with the side inverted.
- The build passed ("tudo certo"), the stack is the stock one at all three roots (`gancho_tx_abc`,
  which now does the swap, reaches 504 bytes at its site), the menus' deepest point is 640 bytes, and
  the preview's 43 screens come out the same; the TX on C one (39c) shows C on the card.

## V2.5T — Oct 26 — ❌ never released, superseded by V2.6T

v50 would have been V2.5T: it was committed under that name (`firmware/releases/BJ8300_BANK_v2.5T.anc`)
and the README already pointed to the release, but the release waited for the radio test. The test
showed that TX on C did not bring C into focus; the fix (v51) came out as V2.6T, and V2.5T's `.anc`
left `firmware/releases/`.

## v50 — Oct 26 — TX-A/B/C and the C label in the menu ✔️ tested: TX went out on C, without C in focus

The owner's two requests after using v49.

- **TX-A/B/C.** The stock TX-A/B (item 44; `b[0x14B7F]`, `0x9013` of the settings: 0 OFF, 1 A, 2 B)
  picks the side PTT transmits on, and only works with the watch on (TDR/TTR). On PTT press
  (`0x01E43E8A`), if the chosen side is not in focus, the firmware swaps the sides (`0x01E40E52(0)`,
  which saves nothing) and sets `b[0x15384] = 1`. At the end of TX (`0x01E473DC`) and on a refused
  TX (`0x01E411A6`), with the flag set, it swaps back. The item's get and set only copy the byte,
  and nothing else reads it (base + offset trace over the whole disassembly). The item gets C,
  becomes **TX-A/B/C** (four options, new title) and keeps the same byte.
- **TX on the third, in RAM only.** The letter on neither side (C, or B when C is on side B) is the
  third. Taking it to side B on every PTT, as the A/B key does, would write two sectors on the way
  and two on the way back. So TX comes out of it the way the round-robin listens to it:
  - the TX-A/B read on PTT press (`0x01E43F92`, "r0 = 1279; r0 = b[r4+r0]") becomes
    `call gancho_tx_abc` + nop, which returns to the stock code the side (1 A, 2 B) or 0 and, for
    the third, sets bit 7 of `b[0x15384]`;
  - TX start (`0x01E41022`) disarms dual watch (`0x01E3FDE4`, at `0x01E410EA`) and only then reads
    the decoded TX frequency (`b[0x15341]`, written by the decoder `0x01E38DE6`). There,
    `gancho_tx_desarma` puts the third in the current slot and its power on the side in focus,
    because TX takes the power from `b[0x15E30 + side in focus]` (`0x01E475A2`). Each channel's or
    VFO's power is the low nibble of byte 14 of the raw channel or byte 16 of the VFO
    (`0x01E3A6D0`, `0x01E3A682`). The side's own is kept in the flag;
  - both swap-backs (`0x01E411AE` and `0x01E473EE`) go through `gancho_tx_volta`: with the third in
    the current slot, they give the power back, bring the side in focus back to the current slot
    (`0x01E3FD9E`) and redraw the screen; without the flag, the stock swap-back. The firmware clears
    the flag right after them, and so does boot (`0x01E56B3A`).
- **Who wins.** Outside TRIPLE, C does nothing (it acts as OFF). If PTT already swapped sides before
  TX-A/B (the flag already set), that wins, as in the stock firmware.
- **On screen**, TX on the third left the card of the side in focus as it was and painted the
  third's strip red; the top bar showed TX. On the radio the strip did not turn red, and the owner
  asked for C in focus, as A and B do: v51 redid it that way.
- **The label of the summary at the top of the menu and QT scan** showed B with C in focus: the
  summary drew the side's letter (`'A' + side`), not the one from `LETRA()`, which the card and the
  strips already use.
- The build passed ("tudo certo", with `4r` checking the four new sites), the stack is the stock
  one at all three roots, the menus' deepest point went from 636 to 644 bytes (below the stock
  740), and the preview's 43 screens come out the same, plus two new ones (TX on C and the TX-A/B/C
  menu with C in focus).

## CHIRP driver — Oct 26 — Mic Gain, TX-A/B/C and the V2.5T name

- **The module named after the version.** `bj8300_v2.0TEMPRA.py` became
  `chirp/bj8300_v2.5TEMPRA.py`, and the model in CHIRP, `Baojie → BJ8300_v2.5TEMPRA`.
  `directory.MODEL_COMPAT` registers both old names (`BJ8300_v1.0TEMPRA` and `BJ8300_v2.0TEMPRA`),
  and the images saved with them open in this one. `teste_chirp_real` got `2c2` (the image with
  V2.0T's name).
- **Mic Gain** in BJ8300_v2.5TEMPRA Specific, right after Noise reduction: HIGH, MID and LOW, at byte
  `0x9033`, the same as the radio's MIC GAIN item. A value out of range shows as HIGH, as the
  firmware reads it. Tests 10p to 10r of `teste_chirp_real`. The help text gives the values (31, 25
  and 19), not a dB scale, which comes from the datasheet of a chip the firmware does not prove.
- **TX-A/B/C** (firmware v50) in VFO A/B/C Channel, right after TTR, which it works with: the base
  TX-A/B (`0x9013`, 2 bits) with C. Tests 10s to 10u.

## CHIRP driver — Oct 26 — VFO A/B/C Channel, VFO C, NR and VFO A's Voice Privacy

- **Settings in another order, at the owner's request.** VFO A/B Channel became **VFO A/B/C
  Channel** and, with FM/AM/SSB Modulation, moved out of BJ8300_v2.0TEMPRA Specific to the top of
  the tree. WATCH and **TTR** (TDR, which left Specific) come first. The three MDFs each sit in their
  side's block, always 2nd and with the same name (`Memory Display Format A (MDF-A)`…), and left
  Basic Settings. The Watch group is gone.
- **VFO C (firmware v49).** The VFO C Channel block has the same fields as A and B (Frequency,
  Tuning Step, TX Power, Bandwidth, the tones, Voice Privacy, Offset, Offset Direction, S-CODE,
  Scramble and RX Modulation), plus Work Mode, MDF-C and CH-C. The VFO format is the base driver's
  own (its `struct vfo`), read over the new block of the `.img`.
- **In the `.img`, the VFO C block (128 bytes, two cable blocks) goes after the WATCH one.** The
  download reads it from `0xD3000`. Without it, VFO C shows B's, which is how the radio starts C, and
  the first change creates the block. The upload only writes it when C's VFO changes and reads it
  back; with firmware v46 to v48, it warns that it needs v49.
- **With C on side B in the radio,** its mode and VFO are in B's slots, and the block keeps B's. VFO
  C shows C's and VFO B shows the saved B, both read only, and the upload leaves the block alone.
- **Noise reduction read from the right byte.** The base driver reads it at `0x9031`, a byte the
  BJ-8300 does not use, and CHIRP showed it off with NR on. The cable builds the `0x9000` block from
  RAM, and the NOISE REDUCTION item keeps it at `b[0x14BDE]`, which lands at `0x9032`. The other 30
  menu items in the block were checked: all are where the base driver puts them.
- **VFO A's Voice Privacy written.** In the base driver it is a setting with no callback, and its
  `set_settings` only writes MemSettings and callbacks: the value never reached the radio (B's is a
  MemSetting). Proven by running the base driver: with ENCRY2 applied, the field stays at 0.
- New tests: in `teste_chirp_real`, 10c to 10c6 (the tree and NR) and 10k to 10o (VFO C, the mode,
  C on side B and Voice Privacy); in `teste_clonagem`, 7 to 7f (the cable, with v49's window and
  v46-v48's).

## v49 — Oct 26 — VFO C, the light on in the spectrum and MIC GAIN ✔️ tested

Tested on the radio; with the fixes of v50 and v51, it is V2.6T. At the owner's request, C now
has the same options as A and B. Up to v48 it was only a memory channel (CH-C); since v49 it is a
side like the others: mode (VFO or channel), a 32-byte VFO and a channel.

- **Where it lives.** C's mode, and the saved B's, are in byte 9 of the WATCH record, which was
  reserved and written as 0 (channel, as up to v48). The VFOs are in sector `0xD3000` (`struct
  vfc`, "VFC1", 70 bytes: C's and the saved B's), in the format of each side's VFO record. They stay
  out of the WATCH record because screen B reads that one on every draw, and 64 more bytes on the
  stack of whoever reads it would go past the stock stack.
- **The A/B key, from B to C,** saves B's mode, channel and VFO and puts C's in side B's slots: B's
  nibble at `0x14B86`, the side's channel at `h[0x15E10]` and B's VFO record (`0x14E62`, through the
  current VFO and `0x01E3A4FE(1)`, which writes the `0x8000` log). Then it rebuilds both sides with
  the VFO/MR key's routine (`0x01E3A812`) and saves the settings (`0x01E3AB36`). From C to A, it does
  the reverse and the stock swap. While C is on side B, everything that touches it (VFO/MR, arrows,
  number entry, STEP, TXP, tones…) is the stock firmware touching C. Up to v48 it was the arrow
  path, which only worked with B and C on channels.
- **TRIPLE's round-robin**, with C on VFO, puts C's 32 VFO bytes in the current VFO (`0x15E80`), as
  the stock loader does (`0x01E3A5E8` also copies only the record's 32 bytes). C's strip shows the
  frequency with "VFO".
- **The screen light stays on in the spectrum.** The signal loop turns the light off when the ABR
  count (`h[0x14F80]`) reaches 0, through `0x01E394D6(0)` at `0x01E46578`, the only place that
  turns it off. The hook only passes that call on outside mode 20. Nothing is saved: on leaving the
  spectrum, or on powering the radio back on (it never starts in mode 20; only the key entry writes
  it, `0x01E4CF7E`), ABR applies again.
- **The cable also writes the VFO sector:** the window goes up to `0xD3FFF`.
- **MIC GAIN**, requested by the owner after hearing the microphone gain changed over the cable
  (calibration byte `0xF210` from 30 to 18 and to 31: "it changed a lot"). A new item right after
  NOISE REDUCTION, with HIGH, MID and LOW, which put 31, 25 and 19 in the gain the firmware writes
  to register `0x7D` on every transmission. The level lives at `0x9033` of the settings block, a
  byte the firmware does not use. It applies at boot (after the calibration copy, which stays
  intact in the chip), in the menu, and also on the reset to factory settings and on a CHIRP upload:
  `gancho_ajustes` sits in the settings save of those two paths (a call at `0x01E4845A` and a
  `goto` at `0x01E43A50`). Details in [section 19](reverse-engineering.md#19-microphone-gain-and-mic-gain-v49)
  of the reverse engineering notes.
- The build passed ("tudo certo"), the stack is the stock one at all three roots, the menus' deepest
  point went from 628 to 636 bytes (below the stock 740) and the preview's 85 screens come out the
  same, plus a new one (C on VFO).

## v48 — Oct 26 — TTR instead of TDR ⏳ superseded by v49, never flashed

At the owner's request, the TDR menu item became **TTR** (Transmitter Triple Receiver), because in
TRIPLE the watch is triple. The "TDR" string is unique in `app.bin` (`0x01E5A782`). Item 7 of the
menu table and the other stock item list (`0x01E5E697`) point to it, and both now say TTR.

## v47 — Oct 26 — the new DTMF box and the end stamp ⏳ superseded by v49, never flashed

- **Screen B's DTMF box had never shown, from v38 to v46.** `gancho_dtmf` asked whether the two-VFO
  screen was active, and the routine that answers (`0x01E3B1A2`) returns 0 with the screen mode
  above 5 (`0x01E3B1B4`). The # key (case 24 of the actions `tbh`, `0x01E4BE28`) writes 7 to the
  mode (`0x01E4CD06`) before opening the box. So the stock one always came up, with its Chinese
  title turned to garbage since v37. Now the hook draws when the mode is 7.
- **The paths, proven in the disassembly.** `0x01E4DA88` is only reached by case 24 (in modes 0 and
  8; 8 is the cable session) and by the radio's frequency entry (only in mode 2, which keeps the
  stock send). `0x01E4A93C` is only reached by mode 7's case. The preview gained DTMF in SINGLE and
  in TRIPLE.
- **The end stamp.** v46 did not flash over v45 (below). `build.sh` links twice and writes the CRC16
  of the code at the end of the image into `carimbo_fim`, inside `app.bin`. So any change at the end
  also changes `app.bin`'s CRC, and the loader writes it. `conferir.py` checks it (`2i`).

## CHIRP driver — Oct 26 — WATCH, CH-C and MDF-C

- **Settings → Watch**, with the radio's three menu items: WATCH (SINGLE, DUAL and TRIPLE), C's
  channel (CH-C, 1 to 960) and MDF-C (Name, Frequency and Channel, like MDF-A and MDF-B).
- **In the `.img`, the WATCH block (64 bytes) goes right after the banks.** The download reads it
  along with the banks, through the extended command. An image saved before gets the block erased,
  and uploading it leaves the radio's WATCH alone.
- **The upload writes the block only when it changes, and reads it back.** Firmware up to v45
  ignores writes outside the banks and acknowledges them anyway. In that case the upload ends with
  a warning that WATCH needs v46, after channels, settings and banks have been written.
- **What belongs to the radio comes from the radio.** Whether C is on side B, and the channel B
  had, the upload takes from the record in the radio at that moment, not from the image: an A/B
  press between the download and the upload is not undone. Only WATCH, CH-C and MDF-C come from
  the image.
- **With C in focus on the radio, CH-C is read only**, because the radio writes over it the channel
  C is on when it leaves side B.
- **SINGLE turns TDR off on upload**, the firmware's own rule.
- New tests: in `teste_chirp_real`, 10 to 10j (the record in the image and the Watch page); in
  `teste_clonagem`, 1h and 6 to 6h (the cable, against a simulated radio with v46's window and with
  the earlier one).

## v46 — Oct 26 — the cable writes the WATCH record ❌ the radio on v45 did not flash it

**The loader did not write it: in the flasher, 4 to 5% then a jump to 100%, and the radio came
back on v45.** It is the same as when v34 was flashed over itself (Oct 25). v46 only changed 1
byte of the code at the end of the image (`0x01E96124` on), which no CRC inside the image covers:
`app_area_head`'s only goes to the end of `app.bin`. All the CRCs came out the same as v45's, and
the loader only rewrites what has a new CRC (v30). Every version that flashed changed `app.bin`;
v27, which also only changed the end, was flashed over v28. v46 left `firmware/releases/`; from
v47 on, the end stamp solves it.

- **The cable write window includes the WATCH sector.** The extended write only accepted
  `0xC1000..0xD1FFF` (the header and the 16 banks), and the WATCH record sits right after it, at
  `0xD2000`: CHIRP had no way to write it. `gancho_cabo` now accepts up to `0xD2FFF`, one sector
  more in the same comparison (compiled, `if (r1 > 17)`).
- **Outside the window, nothing changes:** the block is ignored and the ACK goes out anyway,
  because the stock handler sends it, after the hook. That is why the driver reads the block back
  after writing it.
- Otherwise v46 is v45: the image is the same size, the stack is the stock one at all three roots,
  and the preview's 78 screens come out the same.

## v45 — Oct 26 — the screen follows the A/B key to C ✔️ tested

Tested on the radio: "it's fine now". Testing v44 showed that, in TRIPLE, the second A/B press in a
row left B on the screen. Inside, C was already on side B: changing the channel brought it up in
focus, on the channel after its own, and it worked.

- **The cause.** The arrow path (`0x01E40B9E`), which puts C on side B, loads the channel and does
  not redraw. Its callers redraw: the arrow (`0x01E453E0`), number entry (`0x01E4DDD0` →
  `0x01E41486`, both sides) and the return to the main screen (`0x01E4BDBE`, right after
  `0x01E40B9E(0, 1)`, calls `0x01E3C290`). The stock side swap (`0x01E40E52`) ends in the same
  `0x01E3C290` (`0x01E40EF4`), which is why going back to A showed up.
- **The fix.** After putting C on side B, `gancho_ab` calls `0x01E3C290`, which clears and redraws
  the bar and the whole screen (`0x01E3C132(0)`). It is the full redraw and not the two sides,
  because in TRIPLE having C in focus changes the layout: the card moves down to y 59 and B becomes
  a strip at 38. The same goes for the rare path where B goes back to side B after the key moved
  from A to B.
- **Why only the screen.** In v44 the key already took C to side B, changed its channel and went
  back to A with B in place. What was missing was the drawing.

## v44 — Oct 26 — C as one more side ✔️ tested, the screen did not follow the A/B key

Testing v43 showed the round-robin listening to all three frequencies, and the owner asked for
three things. The A/B key had to reach C, and C had to work "exactly like" A and B. MDF-C had to
come right after MDF-A and MDF-B. And all three had to get the same listening time.

- **The A/B key goes A → B → C → A.** The key is code 18 with no digits: case 18 of the actions
  `tbh` (`0x01E4BE28`) goes to `0x01E4CC10` and `0x01E4D44A`, which sets `r0 = 1` and calls the
  stock side swap (`0x01E40E52`) at `0x01E4D44C`. That call goes through `gancho_ab`:
  - A → B: the stock swap;
  - B → C: saves B's channel and puts C's on side B through the arrow path (`0x01E40B9E(0, 1)`:
    loads the channel through `0x01E3A650` and writes the side's channel and the flash log through
    `0x01E3A42A`). So C transmits, changes channel with the arrows and with number entry, and is
    still there after power-off;
  - C → A: saves the channel C ended on (CH-C takes that value), gives side B back to B and does
    the stock swap.

  Only with B on a channel: with B on a VFO, the key stays on A and B. Changing CH-C with C in
  focus loads the new channel into it.
- **The same time for all three.** In v43 the trip to the other side alternated between B and C,
  and the cycle was A, B, A, C: the side in focus got half the time. Now the hook sits on the way
  back (`0x01E481CC`, the call to `0x01E3FD9E` right after `b[0x15378] = 0`). Coming back from the
  other side, dual watch goes to the third one instead of returning to the side in focus, and only
  on the next return does the stock code restore it. The cycle becomes A → B → C → A, each with the
  firmware's same 50 ticks. With C in focus, the third one is B.
- **MDF-C right after MDF-B.** The build gained items that go after a stock item (`ITENS_DEPOIS` in
  `montar.py`). WATCH and CH-C stay before BANK.
- **The screen with C in focus:** A and B as strips on top and C's card below (y 59). Each one has
  its letter and its place: the card at 18, 39 or 59, by letter, and the strips 20 apart, from 18
  above the card and from 88 below.
- **The WATCH record gained B's channel and whether C is on side B** (14 bytes). v43's 10-byte
  record is no longer valid, and the radio goes back to DUAL: after flashing, WATCH, CH-C and MDF-C
  have to be set again.
- **The WATCH and TRIPLE code moved to the free range** (`.livre.bancos`), because the area at the
  end of the image (`.text`, 3260 bytes) had overflowed by 24. It now holds 2248.

## v43 — Oct 26 — TRIPLE: C watched by dual watch ✔️ tested

Tested on the radio: "it listened to all 3 frequencies". The two problems from the test (the A/B
key did not reach C, and MDF-C out of sequence) and the request for equal time went into v44.

- **TRIPLE:** C is a memory channel, picked in the new **CH-C** item (1 to 960, like BANK CH; an
  empty channel shows ` -`), and shown as **MDF-C** asks: name, frequency or number, with MDF-A's
  options.
- **The screen:** the side in focus on the usual card, and the other two as 19 px strips, each with
  its letter, its text and its channel. While receiving, the strip turns dark amber with amber
  text, like the RX chip.
- **The round-robin** sat on dual watch's trip to the other side (`0x01E4826E`, the call to the
  decoder `0x01E3900A`): once B, once C, with C's raw channel (20 B) read from memory the way the
  stock channel loader reads it. Its state lives at `b[0x14BD0]`, which nothing in the firmware
  reads or writes outside the settings block copies.
- **The WATCH record in a sector of its own**, `0xD2000`, right after the 16 banks and outside the
  window the cable writes. `WTC1` magic and checksum, like the banks. With no valid record, DUAL.
  In v42 the value lived in the reserved byte of the banks header, which had no room for C's
  channel.
- **The menu table left the original's place** for the free range, in a 64-slot array: the
  original has 54, and WATCH, CH-C and MDF-C did not fit. The pointer at `0x01E4CBB2` goes from
  "r0 = 13368; r5 = r6 + r0" to "r5 = menu_tabela", in the same 6 bytes. The original stays intact.

## v42 — Oct 26 — WATCH: SINGLE and DUAL (not flashed; v43 includes it)

- **The WATCH item** (SINGLE, DUAL and TRIPLE), modelled on TDR, before BANK. The owner chose the
  name and the options.
- **SINGLE:** only the side in focus, from the bar to the bottom of the screen. The frequency at 3x
  (2x if it does not fit) with the last two digits at 2x, the chips, a 12 px meter and, with an
  offset, the transmit frequency below.
- **Dual watch follows the owner's rule.** In SINGLE it is off, and only the frequency shown
  receives: WATCH set to SINGLE clears TDR, the TDR setter (item 7, now `tdr_grava`) keeps it at 0,
  and power-up clears it again, in case a CHIRP upload turned it back on. In DUAL and TRIPLE, TDR
  on listens to all of them, and off only to the one in focus.
- **A stack trap.** Calling the chips from inside the SINGLE card added the frames up, and the
  stack went 28 bytes past stock. They come from the scene, as in DUAL. The view went into the byte
  left over from the digits, and the state structure did not grow.

## V2.0T — Oct 26 — the new screen ✅

v41 became **V2.0T**, the second stable release. It is the same file, byte for byte (sha256
`57dcf09b…`), now under the version's name: `firmware/releases/BJ8300_BANK_v2.0T.anc`. The radio's
`VERSION` menu has shown `Firmware: V2.0T` since v38.

Published in the [V2.0T GitHub release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.0T), together with the CHIRP module, the flasher and the
cable driver. The README gained the "new look" section, with four screens taken from the preview
(the main one, the spectrum, the radio and the BANK menu), and what changed since V0.12 now covers
the new screen, the spectrum, the radio, the COIN roger and what was removed (the voices and the
Chinese language).


**The CHIRP module under the version's name.** At the owner's request, `bj8300_v1.0TEMPRA.py`
became `chirp/bj8300_v2.0TEMPRA.py`, and the model in CHIRP `Baojie → BJ8300_v2.0TEMPRA`. A `.img`
image keeps the vendor and model it was saved with, and CHIRP only opens it with a driver of the
same model (`directory.get_radio_by_image`). So the module registers the old name in
`directory.MODEL_COMPAT`, and images saved with the old module open in it with no second model in
the list. `teste_chirp_real` gained `2c` (the image with the old name opens, under the new model)
and `2d` (an unknown model is still refused). The file in the V2.0T release was replaced by the new
one, and the v1.0T one is still there.

## v41 — Oct 26 — the spectrum in theme B ✅

Tested on the radio: "it turned out excellent". It is the last screen the new theme was missing.

- **The same layout as the factory screen, in theme B.** Below the top bar:
  - line 1 has the number of points in a tag, the frequency being swept (large, with the last two
    digits small) and AM or FM;
  - line 2 has the step in a tag, the peak frequency of the sweep in green with a ▲, and the dBm
    (the number in white, the unit in grey);
  - the graph (160 × 64) sits on a dark green panel with lines at every quarter: green bars with a
    light top, the column being measured in white and the threshold line dashed in amber;
  - at the bottom, the two ends of the window, the baseline and marks at the start, middle and end.
- **Each piece through the factory routine that draws it.** The five drawing routines of the
  spectrum are replaced whole, with a `goto` at the entry: the frequency (`0x01E45D96`), the typed
  digits (`0x01E4AFC2`), AM/FM (`0x01E4B04C`), the dBm (`0x01E4ADFE`) and the ends (`0x01E4AB2A`).
  The clear at the entry (`0x01E4CFE2`), the window of each of the 5 graph slices and the threshold
  line (`0x01E4D72E`) become calls into screen B. The hooks that were already ours (labels, peak,
  bars) now draw in theme B.
- **Proven in the disassembly** that only mode 20 reaches those sites, that the five routines are
  called only from them, that no constant points at them and that nothing jumps into their middle.
  It is all in [section 11 of the reverse engineering notes](reverse-engineering.md#11-spectrum).
- **The theme B top bar in the spectrum too.** With the MO key in the spectrum, the factory routine
  would draw the meter over the graph; in screen B only the bar lights up.
- **`.vozes` went from 996 to 674 bytes**, because the drawing moved to `tela.o`. Gone are the four
  immediates that v26 and v34 changed inside the dBm and AM/FM routines, and the `4f` check of the
  format and of the 12 px font, which the peak no longer uses.
- **A stack trap.** The first build went 4 bytes over the factory maximum. `janela_b()` was always
  called with the background colour, and the compiler had dropped that argument. Called by the
  slice with another colour, the argument went back on the stack and the frame of `desenha()` grew
  8 bytes. The slice now calls the factory `JANELA()` directly, and the stack is back to the
  factory one at all three roots.
- **The preview calls the hooks themselves**, in the order the firmware calls them (scenario 32),
  and the image has to come out the same as the composed screen (30).

## v40 — Oct 26 — radio settings checked and a faster modulation switch ✔️ tested

Tested on the radio: "everything worked".

- **The freeze when switching to USB/LSB depended on what was saved in the radio, not on the
  code.** v36, v38 and v39 froze, and v25 and v34 did not. Every diagnostic build worked: builds
  with only part of the hooks, chosen by the `BJ8300_DIAG_GANCHOS` variable of `montar.py`. Then
  v39 itself, flashed again, also worked. Using SSB under v25 had rewritten the settings, and the
  value that froze it was never proven.
- **The radio settings go through a check** (`confere_radio`, in `telas.c`) on entering the radio
  and on every modulation switch, before the chip gets the values. Anything outside the limits the
  menus themselves accept (`0x01E44C68`) goes back to the default:
  - band, AM band and memory mode;
  - the memory index of FM, AM and SSB;
  - the AM and SSB steps and both LNAs;
  - the VFO BFO and, in each of the 15 SSB memories, the bandwidth and the BFO;
  - the copies the chip receives.

  A valid value does not change. The `4p` check of `conferir.py` rebuilds the new addresses from the
  factory bytes. Its `deslocamento()` read the destination register together with the offset, and
  was fixed.
- **The modulation switch got ~2.5 s faster.** The radio band switch (`0x01E3DDBA`) powers the chip
  down, waits 0.6 s and waits twice more for 1.5 s before powering it up. Each of the two became
  0.25 s (`ESPERA_2MS(750)` became 125, on both paths), checked by `4q`. On SSB, the rest of the
  wait is the I2C upload of the chip patch.
- **The S of the medium font.** Letters without a vertical stem (S, Z, 2) came out thin and
  narrower than the others. Now their two end columns are 2 px wide. The width of each column comes
  ready from `fonte.py` (`fonte_b_media`); the first version, which counted the dots for every
  letter, went 4 bytes over the stack.
- **The MEMCH item of the radio menu** (FM, AM and SSB) saves: OK stores the current frequency in
  the chosen number (on SSB, also the bandwidth and the BFO), and "CH-xx" marks a number in use. It
  does not change the tuning. Memories are recalled in memory mode (the key with code 31,
  `0x01E4C8CA`), and station names only come from the PC.

## v39 — Oct 26 — the name editor in screen B, amber and red TX ✔️ tested

What the v38 test showed, fixed. Since v38 the `VERSION` menu shows `Firmware: V2.0T`, as the
owner asked.

- **The name editor (BANK NAME and CH-NAME) in screen B.** The title sits on top, with an amber
  chip on the right showing the mode (`abc`, `ABC` or `123`, switched by `#`). The text sits in a
  green-bordered card, at 2x when it fits, with the amber cursor at the end and how many characters
  of how many. The menu engine calls the mode indicator (`0x01E48720`) on every editor pass, and
  that is what draws the whole editor: on opening, the stock clear (`0x01E49B1C`) has just wiped
  everything. The frame the firmware draws straight to the LCD (`0x01E4865C`) and the text line
  (`0x01E48FE0`) no longer reach the screen. Details in
  [section 13 of the screen map](screen-map.md#13-the-other-screens-v37-to-v39).
- **A bank that is on, and a channel in the bank, in amber** instead of the barely visible green.
  BANK CH's " IN" is gone, as BANK's " ON" already was.
- **TX:** the top bar chip got the dot, like RX, in red, and the whole signal bar turns that red
  while transmitting (it was amber).
- **`conferir.py`:** `4g` only reads instructions inside functions (each one's size comes from the
  ELF symbol table) and inside the detours. The `.rodata` of `tela.o` is interleaved with the code,
  and the string of the `abc` icons showed up as a `goto` outside.

In the first test the radio froze when switching the modulation from AM or FM to USB or LSB.
Flashed again, after the diagnostics, it worked: the freeze depended on what was saved in the radio
(see v40).

## v38 — Oct 26 — what v37 showed ✔️ tested, with defects

Tested on the radio. The fixes worked. Left over: the name editor, still the stock one; the green
of a bank that is on, hard to see; and the freeze on switching to USB/LSB.

- **Six radio RAM addresses were wrong in v37.** The disassembly writes `h[base+i<<1]` for base +
  i × 2, and they were read as base + i: the frequency is `h[0x14CA6]` (not `0x14993`), the band
  `b[0x12A1B]`, the memory mode `b[0x129F9]`, the steps `b[0x12A6F]` and `b[0x12A6E]`, and the BFO
  `h[0x14CBC]`. The preview did not catch it, because it fakes the RAM with the same addresses.
  `conferir.py` gained `4p`, which rebuilds each address from the stock bytes of the routine that
  reads it.
- **Menu:** the stock blinking (the item number at (133, 57) and, while editing, the cursor line in
  the old font) all goes out through one `envia()` in the signal loop (`0x01E476E4`), which screen B
  silences; the value being edited stays still, with the green border and the arrows. The row marker
  (`02 10`, two spaces or nothing) is read from the content: the name (type 7) and "NONE" have none,
  and BANK NAME was losing its first two letters ("MOTOROLA" showed as "TOROLA").
- **The medium font**, 9 px, in the whole menu: the same letter, with each stem or empty column
  2 px wide and the others 1 px, and rows 2 and 6 2 px tall. "NOISE REDUCTION" fits whole.
- **Radio:** typing the frequency (`0x01E49078`), "SEEK..." (`0x01E4D3F8`) and the wait during a
  modulation change (the set of the Modulation item, `0x01E3DE52`) in screen B. The two `envia()`
  calls of the stock settings routine are silenced in radio mode, and the stock row stopped
  flickering.
- **DTMF** (the `#` key, screen mode 7): an amber-bordered box with amber text in place of the card
  of the side in use.
- **Voltage at power-on:** a "VOLTAGE" card instead of "Volta 8.1V". The firmware wrote the voltage
  over the 8th position of "   Voltage" and ate the "ge"; the stock text left elsewhere became
  " VOLTAGE".
- **Both sides on a bank change:** turning a bank on or off puts both sides on a valid channel, as
  the stock DELCH does (`0x01E3C5D6..`: each side's channel that left the map jumps to the next one,
  and `0x01E3A812` reloads both). Before, only the highlighted side moved.
- **The stack of pointer calls.** The menu get/set/exibe routines are called through a pointer, and
  the stack analysis did not follow them. Measured now at the call site, they stay at up to 652 B,
  under the stock 740 B. Saving a BANK NAME went 40 B over since v35, because leaving the editor
  redraws the whole screen with the bank structure still on the stack; the write was split off.

**Freeze on switching to USB/LSB** (already in v37): the SSB patch the radio sends to the receiver
chip (`0x01E64D40..0x01E68B18`, 15,832 B, sent at `0x01E3DC42`) is the same as stock. The signal
loop runs inside the interface, so nothing competes with the upload. The cause is not proven, and
the next step is to test v36, which did not touch the radio.

## v37 — Oct 25 — the other screens in theme B, no Chinese ✔️ tested, with defects

Tested on the radio. The QT scan came out fine; the menu, the radio and the radio wait screen showed
the defects fixed in v38.

- **Menu, radio, QT scan and radio wait screen in theme B**, drawn by `firmware/src/telas.c` from
  each one's stock routines, with no hook on the general `envia()`: that way prompts like
  "Sure to Reset?" go through like the stock ones.
- **No Chinese:** the LANGUAGE item left the menu, the power-on hook sets the language to English,
  and the items' Chinese options point at the English tables. The CHIRP driver hides the language
  setting.
- **The free range grew:** the MP3 decoder goes out together with G.729
  (`0x01E257E0..0x01E2EFC4`, 38,884 B; [section 16](reverse-engineering.md#16-g729-and-mp3-out-v36-and-v37)).
- **The unlit meter is lighter**, so it does not blend into the black, on the radio's S-meter too.

## v36 — Oct 25 — screen B in place of G.729 ✅

**Tested on the radio: it flashed completely and "everything worked, perfect".** The test
checklist was v35's, below.

v35 with screen B somewhere else, because the radio refused to flash v35. The screen code is the
same, linked at another address.

- **Screen B lives in place of the G.729 decoder**, the one for the `.wtg` voices, which have not
  played since v30: `0x01E257E0..0x01E278C8`, 8,424 B inside `app.bin`, of which the screen uses
  5,636. The proof that it never runs is in [section 16](reverse-engineering.md#16-g729-and-mp3-out-v36-and-v37).
- **G.729 leaves the decoder registry:** its type goes from `0x40` to 0, which no lookup matches.
  So it is not chosen even by a path that slipped through the map.
- **Only the spectrum stays in the voices**, in C0 and C1, byte for byte the same as in v34.
  Compared with the radio on v34, only `app.bin` changes.
- **`conferir.py`** gained `2h` (the registry, the place, and no new way into the range, in the
  stock firmware and the new one) and, in `2g`, the lock that only C0 and C1 change.

## v35 — Oct 25 — main screen B ❌ **the radio did not flash it**

**The loader refused the whole image.** In the flasher it stopped at 5% and jumped to 100%, and the
radio rebooted on the previous version. Good versions go to ~78% before jumping. It is not kept
in `firmware/releases/`, because the radio does not flash it.

Five attempts, all the same:

- v35 over v34 and over V0.12;
- two test images: v34 with only the voice files C2..C11 replaced, one with the v35 code there and
  one with zeros.

v34 over V0.12 flashed completely. Reading the radio over the cable afterwards, the `tone` directory
was byte for byte v34's. The loader decides before writing, and what it refuses is an image that
changes the voice files C0..C11. The content does not matter, nor what was already on the radio.
Changing only C0 and C1 (v33 and v34) goes through, and changing `app.bin` always did.

The loader is JieLi's stock `uart_update.bin`, which comes inside `ota.bin`, built on 2023-05-19.
It is encrypted, so the exact rule cannot be read. See
[section 14](reverse-engineering.md#14-internal-flash-partitions-and-who-writes).

What v35 brought, and v36 carries:

- **highlighted side:** a card with a green border, name, channel, big frequency and a 20-segment meter;
- **tags:** power (H red, M amber, L green), FM/AM, N, +/−, CT/DCS, R/T, SC, SCR and zone;
- **the other side:** compact;
- **top bar:** only what is on (VOX, DW, MO, NR, DTMF ST, Bluetooth, lock), plus RX/TX, voltage and battery;
- **font:** our own, 5×7, doubled for the frequency.

The firmware still decides when to redraw. Screen B comes in at the entry of thirteen drawing
routines, through a detour that returns r0..r3 untouched; outside the two-VFO screen it redoes the
stock instructions and carries on with the routine. The stack never goes past what the stock
firmware already uses at the same spot. Details in sections 11 and 12 of the
[screen map](screen-map.md#12-screen-b-v35).

**To test:** the main screen in normal use: change channel, A/B, receive (the meter and RX), transmit
(TX and the TX frequency), type a frequency and a channel, change power and modulation, the top icons
(DW, lock, NR…), the battery; and go in and out of the menu and the spectrum. With the Chinese
language the screen stays the stock one.

## v34 — Oct 25 — green peak and the whole dBm on the AM/FM toggle ✅

v33 passed on the radio: the spectrum worked normally running from the space that was the voices',
and the processor executes from there.

- **Whole dBm on the AM/FM toggle.** The AM/FM routine (`0x01E4B04C`) opened the window
  `127..159 × 17..49` and cleared to black the dBm, which since v26 sits at `112..160 × 36..48`:
  only "-1" was left until the end of the pass. Its text is 12 px at y 20, so the window now ends at
  y 33, like the "x160" one (`0x01E4B066`, `r3 = 49 → 33`). It is called only by key 23 and by the
  spectrum entry.
- **Green peak.** The peak frequency is drawn in the new theme's green (`#34D399`, `0x3693` in
  RGB565), by the first colored-text routine: the 12 px ASCII path of the stock `escreve()`, with the
  letter color one asks for instead of the fixed white. `4f` checks that the font, the 12-byte
  stride and the glyph routine are the firmware's.

**To test:** in the spectrum, the peak line in green, and toggling AM/FM during the scan with the
whole dBm.

## v33 — Oct 25 — the spectrum in the voice space ✅

The spectrum's code and tables (946 B) leave the area at the end of the image for the space that
was the voices'. That area keeps 2268 of its 3260 B, leaving 992 B for the new screen. It proves the
processor **executes** from there, not just reads.

The spectrum was chosen because none of it runs without screen mode 20. In the stock disassembly,
every path to its hooks goes through case 20 of the mode handler, through the spectrum entry (the
only write of 20 to the mode) or through the main loop's "if the mode is not 20, skip". If running
from there fails, only the spectrum goes down: boot, the cable and PTT2 + shortcut never go
through it.

The canary is gone, since the code itself proves the data arrives. `conferir.py` now requires that
only the spectrum routines live in `.vozes`, that only the spectrum sites branch there and that
nothing in `.text` calls `.vozes` (`2b`, `2d`, `4g`), and all three caught the error in a build
made wrong on purpose. Details in [section 15 of the reverse engineering](reverse-engineering.md#15-voices-removed).

**To test:** enter the spectrum and use it as usual (scan, OK, #, arrows stopped on a signal,
frequency digits), then leave. The rest of the radio does not change.

## CHIRP driver — Oct 25 — COIN and no Voice Prompts

- **Roger with five options**, like the radio: OFF, BEEP 1, BEEP 2, **COIN** and TONE1200. COIN
  stores the value 4, which does not fit the base class's 2-bit roger: the module reads and writes
  3 bits, the third being the low bit of the byte above (`unused_9017`, unused by the radio).
- **No "Voice Prompts"** in Settings: since v30 the firmware plays no voice at all, and the VOICE
  item left the radio's menu.

The test gained the value-4 cases (opens as COIN, writes the right byte) and the page without the
voice setting. The module in the v1.0T release did not change: this one goes with v32.

## v32 — Oct 25 — the voice space, proven ✅

v31 with a consistent `tone` directory: `repack` redoes the CRC of every file whose content changed
and the directory's own (`jlfw.refaz_crcs_tone`), the way JieLi's tool writes them. The canary is
now `BVOZTV32`.

Read over the cable on the radio: it is there, and the `tone` header arrived with the new CRC. That
was v30's cause: **the update only rewrites a resource file whose CRC changed**. With that, the
~112 KB of the voices (`0x01E797A0..0x01E956F0`) are ours, read by the processor and written by the
update, without changing the image size or the partition table. Details in
[section 15 of the reverse engineering](reverse-engineering.md#15-voices-removed).

## v31 — Oct 25 — diagnostic: memory over the cable, and COIN

- **Memory read over the cable.** In the extended `R`, an address with bit 22 set (0x400000 and up,
  which the 1 MB data chip does not have) returns what the processor sees from `0x01E00000` on,
  through the chip ROM's `memcpy` (`0x021127A8`, 191 callers in the app). Read only; cable writes
  did not change.
- **COIN.** BEEP 3 is now called COIN in the menu, and the coin plays from the normal table again.
- **What the read showed:** the start of `app.bin`, our code, the `tone` header and `index.idx`
  equal to the `.anc`; at `0x01E797A0`, the original C0 voice, and right after the canary's 18
  bytes the radio and the `.anc` match again. The processor reads the region fine; the update is
  what did not write it.

## v30 — Oct 25 — voices removed ⚠️ **the canary did not show up**

At the owner's request, no voice plays any more (neither Chinese nor English), so their space can
become code. Every voice plays through one of three paths, and each one becomes the "no voice"
path it already has:

- the **sound queue** (`0x01E40628`): `ifs (r5 >= 118) {` becomes `if (r5 < 118) goto` the return —
  a voice leaves the queue without playing, and the beeps go on as before;
- the **mode that plays C16** (`0x01E55ABE`, chosen by `main`): the no-voice branch becomes
  unconditional;
- the **events** (C1, C2, C3 and C15, `0x01E555D6`): the go-between routine always returns without
  playing.

The **VOICE** item left the menu (53 items). The beeps (B0–B7), `index.idx` and `cfg_tool.bin`
stay. On the radio: no voice, key beeps right.

The canary — the BEEP 3 table with a signature, at the start of the voice space — failed: the roger
played BEEP 2, the safe path. v31 showed why.

## v29 — Oct 25 — back to the stock size (not flashed)

v27 without v28's extension, plus two things:

- **48 bytes less:** the spectrum's point and step labels go straight to the text routine, without
  the `SPRINTF("%s ")` copy — the window is already cleared to black, and the extra space only
  painted black.
- **Code up to 3260 bytes,** ending before the last 32 bytes of the smallest image, where the uboot
  keeps the `BOOT_START_FIRST` marker (documented for the previous family by
  [kagaimiq](https://kagaimiq.github.io/jielie/datafmt/br17-fw.html)). In the flash2/4/6 images,
  v26's and v27's code ended inside them.

Check `2f` now requires, in the 6 images, the stock size, the uboot and the partition table
identical, and the last 32 bytes erased. Run against v27, it points out exactly the three smaller
images.

## v28 — Oct 25 — image 8 KB bigger ❌ **boot loop, not kept in releases**

To gain code space, the 6 images grew 8 KB, into the unpartitioned range between the end of the
image and the VM (~224 KB in the 6 variants). The disassembly showed nothing in the firmware writes
there ([section 14](reverse-engineering.md#14-internal-flash-partitions-and-who-writes)). The
flasher finished successfully, and the radio went into a **boot loop**: logo and reset in under
1 s. It came back through PTT2 + shortcut, with v27.

The boot path was byte-for-byte the same as v27's, so the growth is what broke it. **Cause not
proven:** the uboot and the UART loader are encrypted with a cipher that is not the `enc`/`sfc` one
(all 65536 keys of both tried), so what they do with the image size cannot be seen. The suspect is
the PRCT partition, which JieLi's tool always writes with the image size (`CODE_LEN`) and which kept
the old one.

The attempt left a format lesson: the UFW entries come one after another, and three of them
(`isd_config.ini`, `script.ver`, `blimit.bin`) are encrypted **by their position in the file** —
growing an image moves them, and they have to be encrypted again. Check `1` caught it on the first
build.

## v27 — Oct 25 — step label aligned with the peak and the dBm ✅

The step label (`25.0k`) moves down from y 34 to 37, the height of the peak and the dBm on the
second line. Tested on the radio.

## v26 — Oct 25 — spectrum with the current bar and the peak; BEEP 3 roger ✅

- **Current bar in white.** The column of the point being measured is drawn white (the others stay
  blue), at least 3 px tall so it shows where there is no signal. With 80, 40 or 20 points, the
  point's 2, 4 or 8 columns.
- **Running frequency.** The big frequency on top follows the sweep, changing together with the
  white bar (every 5 loop turns; each change sends 3 KB to the LCD). While digits are being typed,
  it does not write over them.
- **Peak line.** The strongest frequency of the last pass goes to the second line, where the dBm
  used to be, and stays until the next pass ends. It is cleared when the window changes.
- **dBm on the right**, under AM/FM, with the same content as before.
- **BEEP 3 roger:** the Super Mario World coin, measured from the owner's mp3 — 2000 Hz for 70 ms
  and 2672 Hz for 330 ms. The roger menu now has five options, and BEEP 3 stores the value 4 (the
  radio keeps the roger in a whole byte). In v31 it became **COIN**.

Details in sections [11](reverse-engineering.md#11-spectrum) and
[12](reverse-engineering.md#12-roger) of the reverse engineering. Tested on the radio.

## CHIRP driver — Oct 24 — all in English, and the banks in blocks under Settings

The firmware is v1.0T's; only `bj8300_v1.0TEMPRA.py` changes.

- **All in English**, because the module is used worldwide: screen texts, notices, error
  messages, comments and internal names.
- **Settings → Banks** with one block per bank (**BANK 1** to **BANK 16**), each with **Name**
  (editable), **Channels** (how many programmed channels it has, read only) and **Enabled**. Until
  now there was only a list of "Banco N ligado", and the name could only be edited in the Banks
  tab (double-click the column header, which still works).
- **A bank with no programmed channel cannot be turned on**, the firmware's own rule. CHIRP unticks
  the box right away, and the rule is checked on the click, not from the count taken when the page
  opened. Removing a bank's last channel in the Banks tab turns it off by itself, and the upload
  turns off any enabled empty bank before writing.
- **Name and Enabled are only written when they change on this page.** CHIRP builds the page once,
  so a name given in the Banks tab afterwards is not undone by the page's old value.
- The checkbox that turns grey after you tick and untick it is CHIRP's, not the module's: the stock
  driver's "Voice Prompts" does the same, and the stored value comes out right.

Checked in the real CHIRP (with wxPython), reading the radio over the cable, read only: the page
shows the 16 blocks with the radio's names and counts. Bank 8, empty, did not turn on. Bank 1 did,
and the name typed under Settings was written to the image. All three tests pass, with the new
cases (blocks, count, refusal, check on click, turning off by itself, old value not coming back,
and the upload that turns an empty bank off).

In the [v1.0T release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v1.0T),
`bj8300_v1.0TEMPRA.py` was replaced by this one (sha256 `073c73b5…`, checked by downloading
without logging in), and the release notes got a note about it. The other release files did not
change.

## v1.0T — Oct 24 — the first stable release ✅

v25 became **v1.0T** ("T" for Tempra), the first stable release. It is the same file, byte for
byte (sha256 `e8b18e68…`), now under the version's name: `firmware/releases/BJ8300_BANK_v1.0T.anc`.
The radio's `VERSION` menu already showed `Firmware: V1.0T` since the texts were revised.

Published in the [v1.0T GitHub release](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v1.0T), together with the CHIRP module, the flasher and
the cable driver. The README now opens with everything that changed since V0.12 and the step by
step for flashing and for CHIRP (developer mode, loading the module, finding the radio under
**Baojie** → **BJ8300_v1.0TEMPRA**), with the warning not to use CHIRP's stock models or older
modules with this firmware.

## v25 — Oct 24 — scramble from 2700 to 3400 Hz ✅

- **SCRAMBLE with nine options, in ascending order: OFF and 2700 to 3400 Hz in 100 Hz steps.**
  It is meant to cover the Baofeng's other modes, not just mode 3 (2900 Hz). Values already stored
  in channels do not change: 1 is still 3400 Hz (the stock ON, the one that talks to other
  BJ-8300s) and 2 is still 2900 Hz. The new frequencies take values 3 to 8.
- **How:** the routine that applies the scramble used to pick between two frequencies by "is the
  value 1 or not". Now 14 of its bytes read the frequency from a table of ours, by the stored
  value, with the index masked to 4 bits. The item's get and set are ours and translate position
  and value, and the set writes through the stock one. The stock table is back to untouched.
  Details in [section 13 of the reverse-engineering notes](reverse-engineering.md#13-scramble).
- **CHIRP driver:** the nine options in the channel and in VFO A and B, in the radio's order. The
  test takes from `bancos.c` the value each option writes.
- **Checks:** `4k` was rewritten. It checks every frequency, the menu order, the names, the
  position ↔ value round trip and the 14 patched bytes, and it was tested by swapping two
  positions.

Code: 3012 B of 3292. Reproducible build, zero branches into patches, all three tests passing.
**Tested on the radio** (Oct 24): the UV-17L's modes 1, 2 and 3 all talk to the BJ-8300, each
on one of the menu frequencies.

## v24 — Oct 24 — a scramble that talks to the Baofeng UV-17L ✅

- **SCRAMBLE with three options: OFF, 3400 Hz, 2900 Hz.** The chip's scramble is spectrum
  inversion, and the inversion frequency comes from a two-entry table in the firmware. The first
  entry (3400 Hz) is the one the stock ON option uses. No option could reach the second one
  (3500 Hz), and it is now **2900 Hz**, where the UV-17L inverts in mode 3. Both frequencies were
  measured from the owner's recording of a 1 kHz tone. UV-17L modes 1 and 2 do not match the
  BJ-8300. The item's get and set stay stock; only the option count, the names and 2 bytes of the
  table change. Details in [section 13 of the reverse-engineering notes](reverse-engineering.md#13-scramble).
- **CHIRP driver:** the channel scramble and the VFO A and B scrambles show and write all three
  options. In the VFO the base splits the value across two fields, and the driver writes it the
  way the radio does: the byte's high nibble and nothing else.
- **Checks:** `4k` checks the scramble table. `2e` is new and covers everything: the decrypted
  app.bin must be the same in all 6 images. Until now only the code and the menu table were
  checked in all six; the roger and scramble patches only in the first one. Both checks were
  tested against the fault they must catch (v23, without the patch, and a copy with one byte
  changed in the second image only).

Code: 2804 B of 3292. Reproducible build, zero branches into patches, all three tests passing (6
new scramble checks in real CHIRP). **Tested on the radio** (Oct 24): with the scramble at 2900 Hz,
the BJ-8300 and the UV-17L in mode 3 understand each other.

## v23 — Oct 24 — BEEP 2 at the mp3's speed ✅

In v22 BEEP 2 worked, but took 0.8 to 1 s, against ~0.4 s in the mp3. The cause is in the firmware:
the wait routine the roger uses (`0x01E36286`) counts in **2 ms units**, not ms — the timer
interrupts every 48000 cycles of 24 MHz (2 ms, `0x01E3E44C`) and the wait runs until the counter
advances twice what was asked. v22's `100` gave 200 ms per tone. Each tone now waits 50 units =
**100 ms**, and the whole BEEP 2 takes ~0.4 s. (The stock BEEP, with 40 units, has always been 80 ms
per tone; it did not change.)

Only the wait value changes; code and hooks are v22's. **Tested on the radio** (Oct 24): it works.

## v22 — Oct 24 — BEEP 2 roger, and the CHIRP driver in a single session

- **ROGER with four options: OFF, BEEP 1, BEEP 2, TONE1200.** BEEP 1 is the stock BEEP (1000 Hz
  and 850 Hz, 80 ms each); **BEEP 2** plays the four tones of the owner's mp3 — **1125, 1857, 1359
  and 931 Hz, 100 ms each** —, while transmitting, right before the radio goes back to receive. The
  stored values stay the stock ones (BEEP 1, TONE1200 = 2) and BEEP 2 is 3, which fits in the
  setting's 2 bits; the menu swaps the order. The four tones play at the same volume (in the mp3 the
  second one is quieter). Details in [section 12 of the reverse-engineering notes](reverse-engineering.md#12-roger).
- **CHIRP driver:** reading failed on the radio with "Radio returned unknown identification
  string". The driver read channels and settings in one session, left with `E` and opened another
  for the banks; mapped in the firmware, `E` answers with an ACK that CHIRP does not read, and the
  session only closes on a loop turn with no byte arriving — the second session's `PROGRAM…` was
  dropped and CHIRP read the leftover ACK. Now it is **a single session** for everything, and on
  upload the firmware check comes before writing anything (nothing is written to a stock radio).
  The driver also shows and writes BEEP 2. The test's simulated radio now behaves like the firmware
  on `E`, and with it the old driver fails with exactly the message the radio showed.

Code: 2764 B of 3292. Reproducible build, zero branches into patches, all three tests passing (6 new
roger checks in real CHIRP, 4 new in the clone test). **On the radio:** BEEP 2 worked, but with
200 ms per tone instead of 100 (fixed in v23).

## v21 — Oct 24 — v20 without the boot loop ✅

Same content as v20 (spectrum 2× faster, 10 kHz step, 320/640 points, peak hold, green `0x0240`),
with the code written where the linker linked it. Code and hooks are byte-for-byte v20's; only the
block's place in flash changes.

**Why v20 boot-looped:** the linker aligns the code section to the largest alignment it finds
inside it, and v20 brought the pointer tables for the new labels (alignment 4): the code came out
linked for `0x01E96124`, but the build script wrote it at a fixed address, `0x01E96122`. Everything
sat 2 bytes off, and the first hook to run — the end-of-startup one, right after the logo — jumped
into the middle of an instruction. No check caught it because all of them used the same fixed
address on both sides.

**What prevents it now:** the code starts at the first multiple-of-4 address (`0x01E96124`);
`montar.py` reads the code address from the ELF and refuses to build if it is not where it writes;
and `conferir.py` gained `2c` (linked address = written address) and `2d`, end to end: each of the
33 firmware branches into the new code is followed into flash, and the bytes actually there,
decrypted, in all 6 images, must be the ELF's. Run against v20, both fail — `2d` points exactly at
the startup hook.

Code: 2558 B of 3292. Reproducible build, zero branches into patches, all three tests passing.
**Tested on the radio** (Oct 24): "worked perfectly"; the sweep got about 1 s faster.

## v20 — Oct 24 — faster spectrum, 10 kHz, 320/640 points, peak hold, darker green ❌ **boot loop, withdrawn**

- **Spectrum twice as fast:** each loop turn now measures one point and already tunes the next,
  instead of spending one turn on each. The chip still gets a whole turn between tuning and
  measuring, as on stock firmware, so the reading is the same: 160 points in ≈ 2.5 s (was 5 s).
- **10 kHz step:** OK goes 6.25 → 8.33 → **10** → 12.5 → 20 → 25 kHz (same chip filter as 8.33 and
  12.5).
- **320 and 640 points:** # goes 160 → 80 → 40 → 20 → **640** → **320**. The graph still has 160
  columns; each column measures 2 or 4 frequencies and shows the highest, and stopping on a signal
  happens at its exact frequency. At 25 kHz, 640 points cover 16 MHz; a sweep takes ≈ 5 s (320) and
  ≈ 10 s (640).
- **Peak hold:** the bar rises at once and, when the signal goes away, comes down half the way on
  each sweep. A short signal stays marked for a few sweeps and the noise stops flickering. Changing
  the window (typing a frequency, OK, #, entering the spectrum) clears the graph.
- **Darker highlight green:** `0x0240` (#004900, step 18), instead of `0x02C0` (#005900, 22) from
  v17 to v19.
- The new indices (640, 320 and 10 kHz) go at the end of the configuration the radio saves, so the
  stock ones keep their value. **Before going back to an earlier version**, leave # on 160/80/40/20
  and the step off 10 kHz: earlier versions do not know the new ones and would show garbage.

Two items of the plan came out differently from the proposal, for a concrete reason:
- **"Redraw the peak text only when it changes"** had nothing to gain: once mapped, the dBm text is
  drawn only once per sweep and when the sweep stops — not on every column, as I had said.
- **"Average several readings per point"** did not go in as back-to-back reads: without knowing how
  often the chip refreshes its RSSI, reading twice in a row may return the same number and clean
  nothing, and it would cost time. The peak's halfway descent is already an average between the new
  sweep and the previous one. "Always 160 points" is what # on 160, 320 and 640 already gives; with
  20/40/80 the steps are what measuring fewer points means.

Full map in [section 11 of the reverse-engineering notes](reverse-engineering.md#11-spectrum). Code:
2558 B of 3294. Reproducible build, zero branches into patches, all three tests passing. **On the radio:
boot loop** — the logo shows and the radio restarts in under 1 s, in a loop. Cause and fix in v21.

## v19 — Oct 24 — the arrow continues the spectrum, and the menu table in place ✅

- **Spectrum:** stopped on a found signal, the arrow (up or down) **continues the sweep** and, that
  time, leaves the threshold alone. While sweeping, the arrow does what it always did (raises or
  lowers the threshold). Continuing is exactly the resume the firmware itself does when the signal
  drops (`0x01E4AEFC`: closes the audio and moves to the next point), and the build checks that
  against the stock bytes (check `4i`). The hook goes into a 4-byte instruction of the spectrum key
  handler (`0x01E4BFBE`); the registers the new call clobbers were checked dead there on every path.
  Details in [section 11 of the reverse-engineering notes](reverse-engineering.md#11-spectrum).
  That key press now beeps like the keys with no function in the spectrum (code 0), not like the
  arrow. If the signal is strong, the sweep may stop again on the neighbouring point of the same
  signal: just press again.
- **Space:** the new menu table is now written **where the original was** (its 54 slots, counting
  the zero item at the end, are exactly the new table's 54 items), instead of taking 1512 bytes of
  the free area. Code now has 3294 bytes available, up from 1782. Proven first that nothing else
  reads the original table; its pointer stays as it was. Code and table came out byte-for-byte
  equal to v18's, only the table in another place.

Code: 1792 B of 3294. Reproducible build, zero branches into patches, all three tests passing.
**Tested on the radio** (Oct 24): it stops on the signal, the arrow continues without changing the threshold, and the beep is fine.

## CHIRP driver — Oct 24 — no zones, and the firmware guard redone

General review of `chirp/bj8300_v1.0TEMPRA.py`, with a new test (`tests/teste_clonagem.py`) that runs
CHIRP's full `sync_in`/`sync_out` against a simulated radio, for both firmwares:

- **Zones:** the driver already showed nothing about zones (the BJ7800 base hides them, and the
  15-fixed-zone model is replaced by the banks) — surveyed setting by setting in real CHIRP: 111
  settings in 4 groups, none about zones. What was missing was the **mode**: it still exists in the
  firmware (`b[0x14BD4]`, which the cable reads and writes at `0x9028` — derived from the settings
  save routine, `0x01E3AB36`, and checked against CHIRP's format), and with the menu entry gone the
  radio could not leave it. Every upload now writes **Full Channel**. The owner's radio was already
  in Full Channel (dump from Oct 24).
- **Firmware guard:** it compared the extended read of `0x0C0000` with the normal read of `0x0000`
  — but the normal one comes back decrypted and the extended one raw, so they never matched. Against
  a stock radio it only held because the stock firmware's long answer (4 + `0x8C` bytes, read from
  the handler at `0x01E4BB6E`) desynced the cable, and the message was a generic "Failed to read
  block". The driver now reads the answer's header: `0x40` = bank firmware; anything else = stock,
  discard the rest and stop with the right message.
- Run against the old driver, the new test fails exactly at those points (Full Channel, message,
  cable desync) and passes the one that mattered: no extended write ever reached the stock firmware.

The banks themselves (format, marker, checksum, multi-bank membership, only changed sectors) passed
everything: 27 + 20 + 16 checks.

## v18 — Oct 24 — green background on RX/TX too

In v17 the **RX/TX** box at the end of the bar kept a black background (as did the thin lines between
the bar's segments, while receiving or transmitting).

When receiving or transmitting, the firmware takes another path (`0x01E40F1C` with argument 1 or 2):
it redraws the affected side's bar in **its own window** `(19, 140, y, y+9)` — segments and the
RX/TX text at x=127 — and sends it through `0x01E3B7CA`; in frequency mode it also redraws the big
line, sent through `0x01E41008`. Both sends now go through the send hook, with the same guard: only a
window inside the highlighted side's area turns green. The send at `0x01E3B7CA` sits inside a
conditional block (`if (r8 != 0)`); checked in the final file that the block prefix is intact and
only the call target changed.

No new code (1732 B), 16 sites at the send and 2 at the clear, reproducible build, no jump into a
patch. **Not yet tested on the radio.**

## v17 — Oct 24 — whole block green, no black strips, darker green ✅

**Tested on the radio:** "it turned out excellent, it's perfect" — it switches sides, gets the right
colour, no black strips. Only the RX/TX box still had a black background (v18).

Closes the thin black strips left in v16 (between the name and the frequency, and between the
frequency and the bar), mapped and checked pixel for pixel against the photos.

- **Where they came from:** no window draws there, so the send hook had nothing to recolour, and
  the strip kept whatever the screen clear left: black.
- **What clears:** `0x01E3ACBA(2)` fills the highlighted side's area with black (72 rows from 16
  with A, from 56 with B), then the other side's, and draws **the white separator line** (88 or
  55). It is the same clear the side switch performs, so no stale green from one layout ever
  survives into the other.
- **The fix:** a hook on the two calls that clear and redraw the main screen (`0x01E3C294` — boot,
  leaving the menu and 14 more callers — and `0x01E40F66`, the side switch) lets the clear run and
  fills the highlighted side's area with green. The windows come on top, and the gaps stay green.
  Entering the menu clears the same way but through another call, and is untouched; so is the
  redraw without a clear (scan's, for instance), so nothing flickers.
- **Darker green:** `0x02C0` (#005900), between v16's `0x0380` (too light on the whole block) and
  `0x0280` (which had been asked to be lighter).
- **Room:** firmware routines are now called through absolute symbols (`FIRMWARE()`), and each call
  shrank from 8 to 4 bytes — same target. The code is at 1732 of the 1782 bytes, with the new hook
  inside. The new check `4g` recomputes the target of every generated `call`/`goto` and rejects any
  that is neither an internal function nor a declared routine; tested by removing one declaration,
  it failed as it should.

Reproducible build, no jump into a patch, 16/16 sites in all six images. **Not yet tested on the
radio.**

## v16 — Oct 24 — window addresses fixed ✅ **highlight working**

**Tested on the radio:** the selected side's block turns green — `H` line, name, frequency, number
and bar — and when switching sides the previous one goes back to black and the new one turns green.
**Thin black strips** remain where no window draws (between the name and the frequency, and between
the frequency and the bar); the photos match what the map predicted pixel for pixel — see
[screen-map.md](screen-map.md), section 8.

**v15 painted nothing because of an address conversion mistake**, not a logic one. `llvm-objdump`
prints immediates in decimal: `janela()` stores y0 at `b[83466]`, y1 at `b[83470]` and the bytes per
row at `h[83472]` — which are **`0x1460A`, `0x1460E` and `0x14610`**. They had been converted as
`0x145CA`, `0x145CE` and `0x145D0` (0x40 short; the mistake came from the previous session's map), so
the hook read three unrelated variables. When "checking" the generated code I compared the hex label
with what I believed, instead of comparing 83402 with 83466.

How it was found: with v15 confirmed on the radio (no `ZONE` in the menu), every link was proven
again — the 13 sites in **all six** images of the shipped file, the guard (the stock bar only shows
because `layout()` returns 2 there), 16-bit access to video RAM (`0x01E6C18E` writes 32 bits inside
it), the DMA (`0x01E38540` reads from `0x10190` and waits for completion). What was left was the
address, and in decimal the mistake was obvious.

Changes:

- `JAN_Y0`, `JAN_Y1` and `JAN_LINHA` with the right addresses, annotated with the decimal.
- **New check (`4f`)**: the build reads the addresses from `janela()`'s own instructions in the stock
  firmware (`0x01E3AD3E`, `0x01E3AD5E`, `0x01E3AD6E`, `0x01E3AD86`) and compares them with the
  `#define`s. Tested with v15's wrong value: it fails.
- **14th site**: the highlighted side's info line (`0x01E3B2C4`, the one with the `H`), y 18 with A
  and 57 with B. It is only drawn for the expanded side, so it always comes out green.

Same size (1776 B), reproducible build, no jump into a patch, 14/14 sites in all six images.

## v15 — Oct 24 — highlight at the LCD send ⚠️ **painted nothing**

**Tested on the radio:** all black, the bar included when changing channel. Cause in v16, above:
three addresses converted wrongly from decimal. The design below (hook at the send, guard through
`layout()`) still stands in v16.


Done after mapping the entire drawing path, which explained with addresses both symptoms the user
reported on v14 (details in [screen-map.md](screen-map.md)):

1. **"the green only shows in the signal bar background, and only when changing channel with the
   arrow"** — v14's three hooks were all on **partial updates**. The full redraw, which is what runs
   when switching A to B, goes through `0x01E3C132` (the three bar rows) and the block drawer
   `0x01E3BD04`, which **had no hook at all**.
2. **"the whole box is still black"** — text backgrounds do not come from `janela()`: each text
   routine fills every character cell with its own hard-coded black (the `r8 = 0` at `0x01E3BAD8`,
   passed to the glyph drawer `0x01E388EC` and the gap fill `0x01E38978`). Swapping the window
   colour was never going to paint the block.

The hook now goes into **the send** (`envia()`, `0x01E3ADBC`) and replaces every black pixel of the
window with green before it reaches the LCD: window background, character cells and gaps all turn
green at once, while anything with a colour of its own (white text, bar segments) passes untouched.
There are **13 sites**: every `envia()` call inside the six functions that draw the two-VFO screen —
a 4-byte `call` replaced in place, so the code does not grow.

Which block is highlighted comes from **the firmware's own routine** (`0x01E3B1A2`), the same one it
uses to place every row: it returns 2 with A highlighted, 1 with B, and **0 when this is not the
two-VFO screen** (the single-VFO one writes 4 into `b[0x1533D]`). That is what keeps the green off
the other screens, which use these same functions. The `y` thresholds (89 and 57) come from three
firmware tables and have wide margins: 74 against 91 with A highlighted, 41 against 69 with B.

The hook **draws nothing** — it only reads the bounds `janela()` already stored and writes inside the
buffer, clamped to its size; v13's overflow is impossible by construction. Verified in the generated
machine code at `0x01E963FA`, instruction by instruction.

New code: 1776 B of the 1782 available. The build ends in `RESULTADO: tudo certo`, is reproducible
(same sha256 after deleting `build/`) and no jump lands inside a patch.

## v14 — Oct 24 — highlight redone from the screen map ⚠️ **did not work**

It fixed both v13 defects, each found in the disassembly:

1. **The reset** was a video-buffer overflow. v13 painted the whole block in a single call — 71
   full-width rows = **22,720 bytes** — and the buffer at 0x10190 holds **10,240** (one 160×32
   band). `janela()` has no size guard (loop at `0x01E3AD8E`), so the excess landed on the text
   buffer, the global base and the redraw flags.
2. **The highlight not showing** was the wrong variable: `tx_ab` (`0x14B84`, the setting stored in
   data flash) instead of **`b[0x15E04]`**, the screen state the firmware itself consults.

The hook moved into **three windows** (name+frequency at `0x01E4B270`, signal bar at `0x01E41468`,
channel number at `0x01E4B314`), without touching the geometry.

**Tested on the radio:** it boots, switches A to B and **no longer resets** — the overflow really was
fixed, and that part is settled. But the highlight was still wrong: green only in the signal bar
background and only when changing channel with the arrow.

**A method mistake worth recording:** those three sites were picked from a *partial* survey of the
screen, which stopped at the partial updates and never reached the full redraw or the text routines.
Both causes are in v15, above.

## v13 — Oct 24 — selected-VFO highlight ⚠️ **known problem**

- **Highlight of the selected VFO block** (hook at `0x01E41468`, the call that paints the signal
  bar row).
- **`ZONE` and `Zone Or Channel` removed** from the menu (by request), giving back 56 bytes of
  table.
- `setor_livre` dropped from the cable write path (the area is provably exclusive already).

**Observed during testing, not yet fixed:**

1. The radio boots and switches from A to B normally.
2. The highlight **does not show**: both sides stay black.
3. **The radio resets** when changing channel with the arrow on the highlighted side — same
   behaviour on A and on B.

The cause has **not been determined**. The survey of which routine draws each part of the main
screen is in [mapa-da-tela.md](mapa-da-tela.md); until that closes with evidence from the
disassembly, there is no v14.

**Fixed in v14** (above). v12 remains the last version actually tested on the radio.

## v12 — Oct 24 — 24-bit cable, read and write

- Extended cable command: **bit 7 of the size byte** turns on a 24-bit address, 64-byte block.
  Reads reach the whole chip; writes only `0xC1000..0xD1FFF`.
- A single hook serves both commands, because `'R'` and `'W'` call **the same routine**
  (`0x01E43970`, 0 reads / 1 writes).
- Validated on the radio: `BNK3` read at `0xC1000`, writes verified byte for byte, the guard
  refusing an address outside the area, and the user's own banks read back with matching
  checksums.
- Space: record markers became two `u16`s compared in two accesses, and the marker is now derived
  from the address — 70 bytes saved, stored format unchanged.

**It fixed two mistakes from v11:** the cable packet base is `0x16671` (not `0x16691`), and the
handler fires with **4** bytes, so a 5-byte packet cannot work — the extra byte also poisoned the
next command.

## v11 — Oct 24 — first attempt at extended reads ❌

Five-byte packet. It did not work (see above) and jammed the cable until the radio was
power-cycled.

## v10 — Oct 24 — leaving the text editor

Saving a bank name left the keyboard's `abc` stuck in the menu header: the two calls `CH-NAME`
makes when it finishes (`0x01E3ACBA(2)` and `0x01E3C7AA`) were missing.

## v9 — Oct 24 — signal bar during scan

While scanning, the bar froze at the level of the last channel found and the channel number drifted
from the name: the call that advances the channel (`0x01E46EB6`) never marked the screen dirty.

## v8 — Oct 23 — banks moved to `0xC1000` (end of the white band)

The boot image lives at `0x10000` — exactly where the banks were being written. Erasing the header
sector and bank 1's sector ate the top lines of the logo (erased flash reads 0xFF, which is white
in 16 bits). Banks moved to `0xC1000`, plus the `setor_livre` guard, which only writes into an
erased sector or one already ours.

## v7 and earlier — Oct 22–23 — the feature

- 16 banks, 12-character names, a channel in as many banks as you like, several enabled at once.
- Filter rewriting the two bitmaps the firmware already uses, without changing any firmware
  function.
- Hierarchical menu: `BANK`, `BANK NAME`, `BANK CH`, together where the zone entries were.
- Menu arrows inverted and 25 English strings revised (version `V1.0T`).

Mistakes fixed along the way, each with an identified cause:

| Symptom | Cause |
|---|---|
| Radio would not boot | The blob was written in plain into an area the SFC decrypts by address |
| Menu value always wrong | The menu value lives at `0x14BFC`, not at the address assumed before |
| Scratched screen, failing checksum | `0x10190` is **video memory**, not a sector buffer |
| White band on the logo | Banks written over the boot image (`0x10000`) |

## Screen diagnostics — Oct 24

Three diagnostic firmwares were flashed to find which call paints which part of the screen (the
firmware has no symbols and the coordinates change with the selected VFO):

1. **diag** — two candidate bands in different colours. Result: the signal bar row is painted by
   `0x01E41468`; the other candidate never shows on that screen.
2. **diag2** — **boot loop**. It patched 23 "colour loads" at once; at `0x01E45ED8` that register
   zeroes a 15-byte buffer before becoming the colour, and at `0x01E3C156`/`0x01E3C17E` it becomes
   the call's `x0`. Recovered through the PTT2 + shortcut key combo.
3. **diag3** — 12 sites, each checked in the disassembly. Result: on the dual-VFO screen only the
   bar row goes through `janela()`; name and frequency get their background inside the text
   routine.

**Lesson kept:** patching a constant mid-code requires looking at the register first — if the load
is not adjacent to its use, it probably serves something else too.
