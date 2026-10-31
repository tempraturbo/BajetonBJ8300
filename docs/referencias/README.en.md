# References

Third-party documents used to study the firmware, kept here so we do not depend on links that may
disappear, one folder per chip. They belong to their manufacturers (their copyright). The BK4819 ones
came from public repositories of the Quansheng UV-K5 community, which uses the same radio chip as the
BJ-8300.

[Português](README.md)

## BK4819 (`bk4819/`)

| File | What it is | Source |
|---|---|---|
| `BK4819_Registers_V1.1.pdf` | The register list (REG-BK4819-E02 V1.1, 2022), with the table of default values | [BI4MTL/uv-k5-firmware-ta1js](https://github.com/BI4MTL/uv-k5-firmware-ta1js), `docs/bk4819/` |
| `BK4819_TRM_V1.0.pdf` | The technical reference manual (TRM-BK4819-E01 V1.0, 2022): how to use each block, with Beken's functions (`RF_SetAfResponse`, `RF_EnterFsk`...) | same |
| `BK4819V3Registers_List_20201218.pdf` | The BK4819 V3 register list, from 2020: the same fields as V1.1 | [amnemonic/Quansheng_UV-K5_Firmware](https://github.com/amnemonic/Quansheng_UV-K5_Firmware), `hardware/` |
| `BK4819_V3_Application_Note_20210428.pdf` | The BK4819 V3 application note (2021): TX and RX audio, filters (`0x2B`), FSK, MDC, VOX, squelch, AFC. The Chinese passages are lost in text extraction | same, named `BK4819(V3) Application Note 20210428.pdf` |
| `BK4819_Datasheet_V1.0.pdf` | The datasheet (Rev. 1.0, 2020): pins, block diagram, electrical characteristics. Marked "Proprietary and Confidential" by Beken, but public in the repository above, in an attachment of [piotr022/UV_K5_playground](https://github.com/piotr022/UV_K5_playground) (same bytes, named `BK4819_Datasheet_V1.01.pdf`) and on LCSC's datasheet server | same |

The other PDFs in those repositories cover UV-K5 parts the BJ-8300 does not have (the BK1080 FM radio,
the ST7565 LCD controller, the EEPROM, transistors, regulators and the audio amplifier) and were left
out. The egzumer repositories and piotr022's have no PDFs, apart from the attachment above.

Not in any of them, and taken from code: the equalizer table (`0x54`/`0x55` and `0x75` per dB) comes
from a disabled block of `driver/bk4819.c` in
[OneOfEleven/uv-k5-firmware-custom](https://github.com/OneOfEleven/uv-k5-firmware-custom).

## Si4735 (`si4735/`)

The radio's LW, MW, SW and SSB receiver ([reverse engineering](../reverse-engineering.md): the Si4735
SSB patch at `0x01E64D40`).

| File | What it is | Source |
|---|---|---|
| `AN332_Si47xx_Programming_Guide.pdf` | The Si47xx programming guide (AN332): FM, AM, SW and LW commands and properties. SSB is not in it: it is an undocumented patch loaded into the chip | [Skyworks](https://www.skyworksinc.com/-/media/skyworks/sL/documents/public/application-notes/aN332.pdf), public |
