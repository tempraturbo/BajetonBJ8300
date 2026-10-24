# Revisão dos textos em inglês do menu (BJ-8300)

Levantado em 24/10/2026 com `scripts/textos.py`, que lista cada texto do menu com endereço,
tamanho e folga.

## A regra que manda em tudo

As cadeias ficam **coladas umas nas outras na flash, sem um único byte de folga**. Trocar no lugar
só pode usar **tamanho igual ou menor** (sobra vai a zero; o texto termina no zero). Esticar
passaria por cima da cadeia seguinte.

Isso decide o padrão: **CAIXA ALTA**, que é o que 90% dos textos já usa — e, por sorte, é
exatamente o que encurta os casos fora do padrão ("Normal Mode" → "NORMAL MODE" cabe igual,
"Super  Mode" → "SUPER MODE" sobra um byte).

Duas cadeias do STEP são **sufixo de outras** (economia do fabricante): `"2.5 K"` mora dentro de
`"12.5 K"` (um byte adiante) e `"5.0 K"` dentro de `"25.0 K"`. Por isso só a maior entra na lista
de trocas — a menor sai corrigida junto, automaticamente.

## O padrão

1. **Nome de item e valor de opção em CAIXA ALTA.** É o que SQL, STEP, TXP, BEEP PROMPT, SCAN ADD,
   POWER ON MSG… já fazem.
2. **Unidade física mantém a grafia correta**, mesmo em meio a maiúsculas: `Hz`, não `HZ` nem `hz`.
3. **Nada de espaço duplo** e nada de espaço antes de unidade (`2.5K`, não `2.5 K`).
4. **Abreviação consagrada de rádio fica como está**: SQL, TOT, TXP, W/N, TDR, ABR, BCL, STE,
   DTMFST, SC-REV, SFT-D, RP-STE, RPT-RL, PTT-LT, MDF-A/B, TX-A/B, AL-MOD, S-CODE, R/T-CTCSS,
   R/T-DCS, TO/CO/SE, BOT/EOT/BOTH, DT-ST/ANI-ST/DT+ANI.

## As trocas (todas cabem no espaço original)

| Item | Endereço | Era | Fica | Por quê |
|---|---|---|---|---|
| TXP | `0x1E5A560` | `Middle` | `MIDDLE` | fora do padrão entre HIGH e LOW |
| SAVE | `0x1E5A4A0` | `Normal Mode` | `NORMAL MODE` | padrão |
| SAVE | `0x1E5A4AC` | `Super  Mode` | `SUPER MODE` | **espaço duplo** |
| SAVE | `0x1E5A4B8` | `Deep   Mode` | `DEEP MODE` | **espaço triplo** |
| STEP | `0x1E5A581` | `6.25 K` | `6.25K` | espaço antes da unidade |
| STEP | `0x1E5A588` | `8.33 K` | `8.33K` | idem |
| STEP | `0x1E5A58F` | `10.0 K` | `10.0K` | idem |
| STEP | `0x1E5A596` | `12.5 K` | `12.5K` | idem (corrige `2.5 K` junto) |
| STEP | `0x1E5A59D` | `20.0 K` | `20.0K` | idem |
| STEP | `0x1E5A5A4` | `25.0 K` | `25.0K` | idem (corrige `5.0 K` junto) |
| STEP | `0x1E5A5AB` | `50.0 K` | `50.0K` | idem |
| Zone Or Channel | `0x1E5A7B3` | `Zone Or Channel` | `ZONE OR CHANNEL` | padrão |
| idem (opção) | `0x1E5A71A` | `Zone Mode` | `ZONE MODE` | padrão |
| idem (opção) | `0x1E5A724` | `Full Channel` | `FULL CHANNEL` | padrão |
| POWER ON MSG | `0x1E5A8F4` | `POWER ON MSG` | `POWER-ON MSG` | hifenização correta |
| SCAN ADD | `0x1E5A6A1` | `Delet` | `DEL` | **erro de grafia** (DELETE não caberia) |
| SCAN ADD | `0x1E5A6A7` | `Add` | `ADD` | padrão |
| TONE | `0x1E5A613` | `1000hz` | `1000Hz` | unidade |
| TONE | `0x1E5A61A` | `1450hz` | `1450Hz` | unidade |
| TONE | `0x1E5A621` | `1750hz` | `1750Hz` | unidade |
| TONE | `0x1E5A628` | `2100hz` | `2100Hz` | unidade |
| RESET | `0x1E5A60F` | `All` | `ALL` | padrão (ao lado de VFO) |
| RX Modulation | `0x1E5AA81` | `RX Modulation` | `RX MODULATION` | padrão |

São 23 trocas, 0 bytes a mais no firmware.

## Levantado e deixado de fora (precisa da sua opinião)

- **`VOICEPRI`** (item 18): o nome em chinês do mesmo item é 语音加密 = *criptografia de voz*, ou
  seja, o inglês está enganoso (parece "voice priority"). `ENCRYPT` caberia (7 de 8 bytes).
  Não mexi porque é mudança de significado, não de forma.
- **`PTTB`** (opção de PF2/PF3): críptico; sem saber o que faz, não dá para melhorar.
- **`AUTOLOCK`**: `AUTO LOCK` (9) não cabe em 8 bytes.
- **`BEEP PROMPT`**: correto, só comprido; `BEEP` encurtaria.
- **Cadeias compartilhadas**: `OFF` e `ON` são as mesmas para uns dez itens (já em caixa alta) —
  mexer nelas mexe em todos de uma vez.
- O resto das 584 cadeias ASCII do `app.bin` são de depuração do SDK (`BT_IDLE_CLOCK`,
  `MUSIC_IDLE_CLOCK`…), não aparecem na tela.

## Como é aplicado

`scripts/montar.py`, lista `TEXTOS` com `TEXTOS_REVISADOS`. Cada troca **confere o texto antigo
byte a byte antes de escrever** e recusa se o novo for maior. O `conferir.py` relê cada cadeia no
firmware montado. Para voltar ao original, é pôr `TEXTOS_REVISADOS = False` e recompilar.
