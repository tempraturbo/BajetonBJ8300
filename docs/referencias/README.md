# Referências

Documentos de terceiros usados no estudo do firmware, guardados aqui para não depender de link que
some, uma pasta por chip. São dos fabricantes (copyright deles). Os do BK4819 vieram de repositórios
públicos da comunidade do Quansheng UV-K5, que usa o mesmo chip de rádio do BJ-8300.

[English](README.en.md)

## BK4819 (`bk4819/`)

| Arquivo | O que é | De onde veio |
|---|---|---|
| `BK4819_Registers_V1.1.pdf` | A lista de registradores (REG-BK4819-E02 V1.1, 2022), com a tabela dos valores padrão | [BI4MTL/uv-k5-firmware-ta1js](https://github.com/BI4MTL/uv-k5-firmware-ta1js), `docs/bk4819/` |
| `BK4819_TRM_V1.0.pdf` | O manual de referência técnica (TRM-BK4819-E01 V1.0, 2022): como usar cada bloco, com as funções da Beken (`RF_SetAfResponse`, `RF_EnterFsk`...) | idem |
| `BK4819V3Registers_List_20201218.pdf` | A lista de registradores do BK4819 V3, de 2020: os mesmos campos da V1.1 | [amnemonic/Quansheng_UV-K5_Firmware](https://github.com/amnemonic/Quansheng_UV-K5_Firmware), `hardware/` |
| `BK4819_V3_Application_Note_20210428.pdf` | A nota de aplicação do BK4819 V3 (2021): áudio de TX e RX, filtros (`0x2B`), FSK, MDC, VOX, squelch, AFC. Os trechos em chinês se perdem na extração de texto | idem, com o nome `BK4819(V3) Application Note 20210428.pdf` |
| `BK4819_Datasheet_V1.0.pdf` | O datasheet (Rev. 1.0, 2020): pinos, diagrama de blocos, características elétricas. Marcado "Proprietary and Confidential" pela Beken, mas público no repositório acima, num anexo do [piotr022/UV_K5_playground](https://github.com/piotr022/UV_K5_playground) (mesmos bytes, com o nome `BK4819_Datasheet_V1.01.pdf`) e no servidor de datasheets da LCSC | idem |

Os outros PDFs desses repositórios são de peças do UV-K5 que o BJ-8300 não tem (o rádio FM BK1080,
o controlador de LCD ST7565, a EEPROM, transistores, reguladores e o amplificador de áudio) e ficaram
de fora. Os repositórios do egzumer e o do piotr022 não têm PDF, fora o anexo citado.

O que não está em nenhum deles e foi tirado de código: a tabela do equalizador (`0x54`/`0x55` e
`0x75` por dB) vem de um bloco desligado do `driver/bk4819.c` do
[OneOfEleven/uv-k5-firmware-custom](https://github.com/OneOfEleven/uv-k5-firmware-custom).

## Si4735 (`si4735/`)

O receptor de LW, MW, SW e SSB do rádio
([engenharia reversa](../engenharia-reversa.md): o remendo de SSB do Si4735 em `0x01E64D40`).

| Arquivo | O que é | De onde veio |
|---|---|---|
| `AN332_Si47xx_Programming_Guide.pdf` | O guia de programação dos Si47xx (AN332): comandos e propriedades de FM, AM, SW e LW. O SSB não está nele: é um remendo não documentado, carregado no chip | [Skyworks](https://www.skyworksinc.com/-/media/skyworks/sL/documents/public/application-notes/aN332.pdf), público |
