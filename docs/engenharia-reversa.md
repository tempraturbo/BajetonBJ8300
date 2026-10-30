# O que foi descoberto no BJ-8300

> 🇬🇧 English: [reverse-engineering.md](reverse-engineering.md)

Tudo abaixo saiu do firmware de fábrica **V0.12** e de medições no próprio rádio. Cada endereço
foi conferido no desassemblado (`firmware/scripts/desmonta.sh`) ou provado pelo cabo. Onde a
conclusão veio de experimento, está dito qual.

## 1. O aparelho

| | |
|---|---|
| CPU | JieLi **AC695X**, núcleo **pi32v2** (toolchain JieLi 2.5.3, clang) |
| Tela | 160×128, RGB565, memória de vídeo em **0x10190** (10240 B por banda de 160×32) |
| Canais | 960, registros de 32 bytes |
| Família no CHIRP | Radtel **RT-920** / Bajeton **BJ7800** (mesmo protocolo e formato) |

São **duas memórias diferentes**, e confundi-las custa caro:

- **Flash do firmware** — onde o `.anc` é gravado. É ela que limita o tamanho do código.
- **Chip de dados (1 MB)** — canais, ajustes, logo, fontes e os bancos. O firmware fala com ele por
  `LE_FLASH`/`GRAVA_FLASH`; o PC, pelo cabo.

## 2. O arquivo `.anc`

Formato UFW da JieLi com embaralhamento KDH. São 12 entradas:

```
flash.bin  630784 B   flash2.bin 626688 B   flash3.bin 630784 B
flash4.bin 626688 B   flash5.bin 630784 B   flash6.bin 626688 B
ota.bin    285590 B   isd_config.ini 4514 B  blimit.bin 144 B  tail.bin 64 B
info.log, script.ver
```

As seis imagens de flash são variantes para chips diferentes; **todas** têm que ser alteradas, com
CRCs refeitos (`firmware/scripts/jlfw.py`).

- O `app.bin` tem 491.740 bytes e roda em **0x01E00100**. Os últimos 0xD4A8 bytes são copiados para
  a RAM na partida (rodam em 0x0200xxxx).
- Tudo que fica **depois do diretório do app** é lido pelo **SFC**, que decifra por endereço em
  blocos de 32 bytes. Gravar código em claro ali faz o processador executar lixo — foi o primeiro
  erro grave do projeto.
- **Área livre comum às 6 imagens: `0x1E96121..0x1E96DFF` = 3295 bytes.** É o único espaço para
  código novo. Medir com `firmware/scripts/espaco_livre.py`. Desde a v19 é todo do código: desde a v21, 3292
  bytes a partir do primeiro endereço múltiplo de 4, `0x01E96124` (ver "pegadinhas", 7); até a v18 a cópia da tabela de menus ocupava 1512
  deles e o código só ia até 1782.
- **Chamar o firmware por símbolo absoluto, não por ponteiro.** `((void (*)(void))0x01E3ADBC)()`
  compila como "carrega o endereço num registrador (6 bytes) e chama por ele (2)"; declarado com
  `FIRMWARE(ENVIA, 0x01E3ADBC)` (um `.set` no `bancos.c`), sai como `call` relativo de 4 bytes,
  resolvido pelo linker no endereço exato. Foi o que abriu espaço para o gancho da limpeza (v17), e a
  verificação `4g` do `conferir.py` recalcula o destino de cada `call`/`goto` gerado.
- **A tabela de menus nova fica no lugar da original (v19).** A original (`0x01E62F98`, 53 itens)
  vem seguida do item zerado que a encerra: 53 + 1 = 54 vagas, exatamente os 54 itens da tabela
  nova. Provado antes de mudar que mais nada lê a original: o único acesso é o cálculo do ponteiro
  em `0x01E4CBB2` (a única carga de 13368 no firmware); nenhum ponteiro de dados nem constante de
  32 bits cai dentro dela; os outros dois chamadores da contagem (`FUN_01E4A81A`, em `0x01E4B3FE` e
  `0x01E4C3C4`) apontam a estrutura do menu para **outras** tabelas antes de contar; e as três
  constantes com valor na faixa dos deslocamentos (13720, 13912, 14357) são conta e registradores do
  chip de rádio. O ponteiro fica como era (o `conferir.py` confere), e a v19 saiu com código e
  tabela byte a byte iguais aos da v18, só a tabela em outro lugar.

## 3. Mapa do chip de dados (1 MB)

Levantado pelo cabo em 2026-10-24 com `tools/cabo/mapa_memoria.ps1`, que acha o tamanho real
procurando onde o conteúdo começa a espelhar o endereço 0:

| Faixa | Conteúdo |
|---|---|
| `0x00000–0x09FFF` | 960 canais de 32 B |
| `0x0A000–0x0FFFF` | ajustes; `0x0F000–0x0F24F` é calibração de fábrica (potência de TX em `0xF000–0xF0BF`, [seção 20](#20-a-potência-de-tx-mapa-só-leitura); ganho do microfone em `0xF210`, [seção 19](#19-o-ganho-do-microfone-e-o-mic-gain-v49); squelch, subtom e cristal, [seção 22](#22-a-pasta-mic-o-crystal-fine-e-o-menu-recebendo-v59)) |
| `0x10000–0x1AFFF` | **imagem de abertura**: 160×138 em RGB565 little-endian, 44160 B |
| `0x20000–0x99FFF` | fontes e textos em chinês (~480 KB) |
| `0xC0000–0xC0FFF` | 64 registros de 60 B que o firmware usa |
| **`0xC1000–0xD1FFF`** | **os bancos** (cabeçalho + 16 setores de 4 KB) |
| **`0xD2000–0xD2FFF`** | **o registro do WATCH** (v43; o cabo o grava desde a v46; [seção 18](#18-a-dupla-escuta-e-o-triple-v42-a-v51)) |
| **`0xD3000–0xD3FFF`** | **os VFOs da C** (v49: o da C e o do B guardado; [seção 18](#18-a-dupla-escuta-e-o-triple-v42-a-v51)) |
| **`0xD4000–0xD4FFF`** | **a mensagem do Morse**, a velocidade e o tom (v65; [seção 24](#24-morse-e-aprs-v65)) |
| **`0xD5000–0xD5FFF`** | **o APRS**: indicativo, SSID, destinatário, mensagem e caminho (v65) |
| **`0xD6000–0xD7FFF`** | **o histórico do APRS RX**: até 62 pacotes, em anel, um cabeçalho por setor (v70; o `0xD7000` desde a v74; o anel desde a v75) |
| `0xFA000` | 4 KB de uso do firmware |
| livre | `0x1B000–0x1FFFF`, `0x9A000–0xBFFFF`, `0xC5000–0xF9FFF`, `0xFB000–0xFFFFF` ≈ **370 KB** |

O firmware **não** mora nesse chip: nenhuma amostra bate com as imagens do `.anc`, nem cruas nem
decifradas. Ou seja, sobra espaço de **dados** à vontade, mas espaço de **código** continua sendo
os 3295 bytes da imagem.

## 4. Protocolo do cabo

57600 8N1. Igual ao driver `radtel_rt900.py` do CHIRP:

| Passo | Bytes |
|---|---|
| Entrar | `PROGRAMBT11U` → ACK `0x06` |
| Identificar | `F` → 8 B (`01 36 01 80 04 00 05 20`) + 8 B (`02 00 02 60 01 03 30 04`) |
| Cifra | `SEND \x01\x01` + 18 zeros → ACK |
| Ler | `R` + endereço **16 bits BE** + tamanho → eco de 4 B + dados |
| Gravar | `W` + endereço + tamanho + **64 B** → ACK |
| Sair | `E` → ACK `0x06` |

Abaixo de `0xF000` os dados trafegam **cifrados** (XOR com a chave `CO 7`, pulando 0x00, 0xFF,
espaço na chave e os bytes iguais à chave ou ao inverso dela). O comando novo entrega **cru**.

**Como o tratador funciona** (base `r13 = 0x14680`, carregada em `0x01E4B422`):

- pacote em **`0x16671`** (letra, endereço alto, endereço baixo, tamanho), dados e resposta a
  partir de **`0x16675`**; comprimento recebido em **`h[0x1666C]`**;
- `'R'` dispara com **4 bytes** no buffer (`0x01E4BB52`), `'W'` com **68** (`0x01E4C0E8`);
- os dois chamam **a mesma rotina `0x01E43970`** — 0 para ler, 1 para gravar;
- depois de responder, o contador é zerado (`0x01E4BB7C`, e `0x01E416C4` no caminho do `'W'`).

Três armadilhas que custaram tempo:

- **Não existe pacote de 5 bytes.** O quinto byte chega depois do disparo, fica pendente e engole o
  comando seguinte — inclusive o magic. O rádio só volta desligando.
- **Nada pode ser enviado antes do magic.** A comparação é desde o início do buffer, e o magic
  termina em `U`, que é comando do modo UPDATE: insistir fora de sincronia faz o rádio entrar em
  UPDATE sozinho.
- **Uma sessão só por operação.** O `E` responde com um ACK (`0x01E4C0D0` → `0x01E416C4`) e só
  põe a sessão no estado 3; ela fecha numa volta do laço em que nenhum byte chegue
  (`0x01E4BA96`). Um `PROGRAM…` mandado logo depois do `E` cai no estado 3 e é descartado — e o
  CHIRP, que não lê o ACK do `E`, toma esse byte que sobrou como a resposta. Era o "Radio
  returned unknown identification string" do driver até a v22, que abria uma segunda sessão
  para os bancos.

### Comando estendido (deste firmware)

Cabe nos mesmos 4 bytes: **bit 7 do tamanho ligado** = endereço de 24 bits, com os outros 7 bits
valendo o terceiro byte; o bloco é sempre de 64 B. Bit 7 em zero = comportamento de fábrica, byte a
byte — o CHIRP não vê diferença. Alcance: 8 MB.

Gravação estendida só em `0xC1000..0xD3FFF` (até a v45, `0xD1FFF`; a v46 incluiu o registro do
WATCH e a v49 os VFOs da C), apagando o setor quando o bloco começa nele. Qualquer outro endereço é ignorado sem escrever
nada (provado com `tools/cabo/teste_cabo_grava.ps1`), e o ACK sai do mesmo jeito.

**O bloco de ajustes `0x9000` vem da RAM.** O tratador do cabo trata o `0x9000` à parte
(`0x01E439B4`): monta os 64 B da RAM (`0x01E43A94`: bytes 0..31 de `0x14B6C`, 32..63 de `0x14BCC`) e,
na gravação, devolve para lá (`0x01E43A32`). No chip, os ajustes são um log de registros de 96 B
(`0x9010 + n × 0x60`, gravado por `0x01E3AB36`). O NOISE REDUCTION (`b[0x14BDE]`, get `0x01E3D0E0`)
cai em `0x9032`; o driver de base o lia em `0x9031`. Os outros 30 itens do menu do bloco batem.
O `0x8000` é parecido: o cabo recarrega os VFOs de A e B do log (`0x01E3A5E8`) e manda `0x14E42`.

**Leitura de memória (v31, diagnóstico).** No `R` estendido, endereço com o bit 22 ligado
(`0x400000` em diante, que o chip de 1 MB não tem) devolve 64 B da memória do processador em
`0x01E00000 + (endereço − 0x400000)`: o que o firmware enxerga da flash interna (e da RAM em
`0x02000000`). A cópia é o `memcpy` da ROM do chip (`0x021127A8`, 191 chamadores no app, destino
em r0, origem em r1, tamanho em r2). Só leitura. Foi ela que mostrou o caso das vozes
([seção 15](#15-vozes-fora)).

## 5. Rotinas do firmware usadas

| Endereço | O que é |
|---|---|
| `0x01E19DCC` | `sprintf` (no máximo dois valores, como os itens de fábrica usam) |
| `0x01E390AC` | `LE_FLASH(endereço 24 bits, destino, tamanho)` — chip de dados |
| `0x01E3A126` | `APAGA_SETOR(endereço)` — 4 KB |
| `0x01E3A25A` | `GRAVA_FLASH(endereço, origem, tamanho)` — fatia em páginas de 256 B |
| `0x01E38C3A` | `VALIDA_FREQ(freq)` — 1 se cai numa faixa permitida |
| `0x01E3A700` | `REVALIDA()` — pula para o próximo canal válido |
| `0x01E3AD34` | **`janela(x0, x1, y0, y1, limpar, cor)`** — define a área de desenho e, com `limpar != 0`, enche com `cor` (RGB565) |
| `0x01E43970` | dados do cabo: 0 lê, 1 grava |
| `0x01E489E0` | motor de menus |
| `0x01E458E4` | avança o canal (usada pelo scan) |
| `0x01E3951C` | fim do carregamento de configuração e canais (onde o filtro entra) |
| `0x01E4AE44` | espectro: próximo ponto da varredura (ou recomeça a janela) |
| `0x01E39E22` | saída de áudio do chip de rádio (registradores `0x47`/`0x48`): 1 abre, 0 fecha |
| `0x01E39E2C` | pedido ao amplificador, por origem (seis flags em `0x129D2`): ímpar pede, par solta; o espectro usa 5 e 6 |
| `0x01E39CFC` | o outro pino do áudio (flag em `0x129D1`): 1 liga, 0 desliga |
| `0x01E38B4C` | bipe de tecla: enfileira o tom `134 + r0` (o código que cada tecla passa só escolhe o tom) |
| `0x01E44604` | fim da transmissão: toca o roger e volta para a recepção |
| `0x01E43B4E` | frequências dos dois geradores de tom (registradores `0x71`/`0x72`, Hz × 10,32444) |
| `0x01E43AFE` | liga (1) / desliga (0) o modo de tom do chip |
| `0x01E408E2` | aplica o scramble: bit 1 do registrador `0x31` e a frequência de inversão no `0x71` (seção 13) |
| `0x01E36286` | espera ocupada em **unidades de 2 ms**: o temporizador interrompe a cada 48000 ciclos de 24 MHz = 2 ms (`0x01E3E44C`) e soma 2 ao contador `[0x14810]` (`0x01E51356`), que anda 1 por ms; a espera vai até ele andar `2n` |

### Memória usada

| Endereço | O que é |
|---|---|
| `0x00010190` | **memória de vídeo** (não é buffer de setor — ver "pegadinhas") |
| `0x000129B0` | buffer de texto que vai para a tela |
| `0x00014BFC` | valor do item de menu em edição |
| `0x00014B6C` | espelho do bloco de ajustes `0x9000`; **`0x14B84` = `tx_ab`** (VFO ativo) |
| `0x00015D8B` | mapa de bits "canal existe" (120 B) |
| `0x00015D12` | mapa de bits "canal entra na busca" (120 B) |
| `0x00015D10`, `0x00015E06`, `0x00015E08` | máscaras por zona |
| `0x00015E3C` | canal atual |
| `0x00015367`, `0x0001536F` | flags de "redesenhar" |
| `0x00016671` / `0x0001666C` | pacote do cabo / comprimento recebido |
| `0x00014B83` | roger: 0 OFF, 1 BEEP, 2 TONE1200; 3 = BEEP 2 (v22) |
| `0x00014BDF` | MIC GAIN: 0 HIGH, 1 MID, 2 LOW (v49; o `0x9033` dos ajustes, seção 19) |
| `0x0000706C` | ganho do microfone posto no registrador `0x7D` a cada TX (cópia do `0xF210`; desde a v49, do MIC GAIN) |
| `0x00014BE1` / `0x00014BE2` | COMPRESSOR (0 OFF, 1 a 3 = 1,33:1, 2:1 e 4:1) e MIC AGC (0 ON, 1 OFF): o `0x9035` e o `0x9036` dos ajustes (v59, seção 22) |
| `0x00014BDD` / `0x00014BE0` | NOISE GATE (`0xE8` + 0 OFF, 1 DEFAULT, 2 LOW, 3 MID, 4 HIGH; v80) e EXPANDER (`0xC0` + 0 OFF a 3 HIGH); sem a marca, DEFAULT e OFF: o `0x9031` e o `0x9034` dos ajustes (v79, seção 25) |
| `0x00014BE3` | CRYSTAL FINE, 16 bits com sinal, em 10 Hz a 435,5 MHz: o `0x9037` dos ajustes (v59, seção 22) |
| `0x00007076`..`0x00007079` | a correção do cristal da calibração (cópia de `0xF21A`; desde a v59, com o CRYSTAL FINE somado; seção 22) |
| `0x00014B7F` | TX-A/B (`0x9013` dos ajustes): 0 OFF, 1 A, 2 B; desde a v50, 3 = C (seção 18) |
| `0x00015384` | marca da troca de lado do PTT: 1 = trocou (destroca no fim da TX); desde a v51, bit 7 = TX na terceira, bit 6 = ela já está no lado B, bit 5 = a troca para o B foi do TX-A/B/C, bits 0..3 = a potência do B |
| `0x00015E2C` / `0x000129A4` | scramble do canal ou VFO atual / o da sintonia: 0 OFF, 1 3400 Hz, 2 2900 Hz, 3–8 = 2700, 2800, 3000…3300 Hz (v25) |
| `0x01E68B18` | frequências de inversão de fábrica (u16, Hz × 10,32444): 35103 = 3400 Hz e 36135 = 3500 Hz; desde a v25 ninguém a lê (seção 13) |
| `0x0001533D` | modo da tela (20 = espectro) |
| `0x00014654` | estado da varredura do espectro: 0 sintoniza, 1 mede, 2 parado num achado |
| `0x00014648` / `0x0001464C` | nível medido / limite que faz parar, os dois em altura do gráfico (0..64) |
| `0x000148DC`..`0x000148EC` | espectro: frequência sintonizada, a do pico, centro, fim e início da janela (u32) |
| `0x0001551B` | alturas do gráfico do espectro, 160 colunas de 0..64 (83584 + 3739) |
| `0x00006EC0` | configuração do espectro: `+4` pontos (160 >> n), `+8` índice do passo, `+0x14` modulação (0 FM, 1 AM, 2 USB; a tecla 23 alterna FM e AM), `+0x15` limite, `+0x19`/`+0x1D` mínimo e máximo dele |

## 6. Motor de menus

A tabela do menu principal fica em **`0x01E62F98`**, com **53 itens de 28 bytes**:

```
flags(1) | máximo(u16) | get(4) | set(4) | nome_en(4) | nome_cn(4) | exibe|opções(4) | opções_cn(4) | id(1)
```

- `flags` 0x80 = item numérico com função de exibição própria; 0x07 = abre o editor de texto.
- O byte `id` **não** é o número mostrado na tela (os valores se repetem e vários são 0).
- O ponteiro da tabela é montado em `0x01E4CBB2` e a contagem vem de `FUN_01E4A81A`, fixada em
  `0x01E4CBE8`. Até a v42 a tabela nova era gravada por cima da original (que tem 54 vagas,
  contando o item zerado). Desde a v43 ela fica na faixa livre, no vetor `menu_tabela` de 64 vagas
  do `tela.c`, porque o WATCH, o CH-C e o MDF-C não cabiam mais. O cálculo do ponteiro em
  `0x01E4CBB2` ("r0 = 13368; r5 = r6 + r0") vira "r5 = menu_tabela" (`c5 ff` + 32 bits), nos mesmos
  6 bytes. O r0 que sobrava não é lido (a chamada seguinte, `0x01E3AC52`, o escreve antes de ler),
  e o r6 é sobrescrito em `0x01E4CBC0`. O resto do firmware acha a tabela pelo ponteiro que esse
  trecho guarda em `[0x14BF4]` (`0x01E4CBC4`), e a original fica intacta. A contagem é uma
  constante: 55 desde a v43. Desde a v53 o vetor tem 80 vagas e traz os grupos antes dos itens, o
  trecho virou "call menu_abre; r5 = r0" e a contagem é a dos grupos, 13
  ([seção 21](#21-o-menu-em-pastas-v53-a-v57)).
- Um item novo entra no lugar dos de zona (o WATCH, o CH-C e os do BANK) ou logo depois de um de
  fábrica (o MDF-C, depois do MDF-B: `ITENS_DEPOIS` do `montar.py`), e copia o resto do item que
  serve de molde.
- **O motor não tem submenu.** O nibble baixo das `flags` escolhe entre 7 tipos de tela (0 =
  lista/valor, 2 = CTCSS, 3 = frequência, 4 = MEMCH/DELCH, 7 = editor de texto, 8 = VERSION, 10 =
  DCS) e nenhum abre uma lista dentro de outra. Dois níveis (BANK → banco → renomear/canais) seriam
  possíveis trocando a tabela corrente de dentro do nosso próprio handler, como `FUN_01E4B3A0(id)`
  faz nos menus rápidos. Foi o que a v53 fez para os grupos ([seção 21](#21-o-menu-em-pastas-v53-a-v57)).

## 7. Como o filtro dos bancos funciona

O rádio decide quais canais existem lendo dois mapas de bits na RAM, montados na partida varrendo a
memória de canais. **`FUN_01E39FCE` é o único leitor deles.** O filtro reescreve esses mapas com a
interseção entre "o que existe" e a união dos bancos ligados — por isso navegação, busca e
revalidação passam a respeitar os bancos **sem alterar nenhuma função do firmware**.

Para desfazer, os mapas são remontados do zero com o mesmo critério da partida (4 bytes por canal,
BCD, `VALIDA_FREQ`), em vez de guardar cópia: assim não existe estado nosso em RAM e canal gravado
ou apagado pelo MEMCH/DELCH entra na conta na hora.

Depois de o filtro mudar pelo menu (o BANK e o CHANNEL dos BANKS; o BANK CH até a v52), **os dois lados** vão para um canal que
vale (v38), como o DELCH de fábrica faz depois de apagar um canal (`0x01E3C5D6..0x01E3C6C4`): em
canal cheio, o canal de cada lado (`h[0x15E0C]`, `h[0x15E10]`) que saiu do mapa pula para o
próximo que vale (`0x01E3C4C0`), e `0x01E3A812` recarrega os dois lados, revalidando cada um.
Até a v37 só o `REVALIDA()` rodava, e ele olha só o lado em evidência. Na partida continua só ele,
porque a partida já recarregou os dois lados (`0x01E56A9E`) antes do gancho, e uma segunda recarga
pediria redesenho por cima do logo. No modo por zona, que o driver do CHIRP desliga em todo
upload, também fica só o `REVALIDA()`.

### Formato gravado

```
0xC1000  cabeçalho: "BNK3" | ativos(u16) | escolhido(u8) | reservado | soma(u16)
0xC2000 + n*0x1000  banco n: "BCO1" | nome(12) | mapa(120 B = 960 bits) | soma(u16)
```

`soma_de`: começa em `0x1234` e faz `soma = (soma << 1) + byte`. **Atenção:** registro que termina
em 16 bytes zerados sempre dá soma 0 — ou seja, nos bancos quem protege de verdade é a marca.

## 8. Codificação de instrução útil

`rX = imm8` em **2 bytes** (achada comparando casos conhecidos, conferida contra o `llvm-objdump`):

```
byte1 & 0xE0 == 0x20   e   byte0 & 0xC0 == 0x40
imm = (byte1 & 0x1F) | ((byte0 >> 3) & 7) << 5        reg = byte0 & 7
```

É o que permite trocar uma cor de preenchimento **sem crescer o código** — e é por isso que cores
de realce ficam limitadas a 0..255, que em RGB565 são só verdes e azuis escuros (o vermelho mora
nos bits altos).

## 9. Pegadinhas que custaram caro

1. **`0x10190` é a memória de vídeo**, não um buffer de setor. O firmware só a empresta ao regravar
   canal, aceitando sujar a tela. Guardar a tabela dos bancos ali fazia a interface pintar por cima
   dela.
2. **O logo fica em `0x10000`** — e a primeira versão dos bancos gravava exatamente ali. Apagar
   dois setores comeu as primeiras linhas da imagem: era a "faixa branca" no logo. Daí a trava
   `setor_livre`, que só grava em setor apagado ou que já seja nosso.
3. **Espelhamento**: em chip de 64/128/256 KB, `0xC1000` cairia em `0x1000`, em cima de canal. O
   dump provou que não acontece neste rádio (1 MB, sem marca `BNK3` nos 64 KB baixos).
4. **Medir tamanho de código**: somar `.text` + `.rodata` + `.rodata.str1.1` do `.o`; **não**
   incluir `.rela.text`.
5. **Trocar uma constante no meio do código exige olhar o registrador.** Um diagnóstico que trocou
   23 "cargas de cor" de uma vez derrubou o rádio em bootloop: em uma delas o mesmo registrador
   zerava um buffer de 15 bytes antes de virar cor, e em outras duas ele vira o `x0` da chamada
   seguinte. Regra: só mexer quando a carga está colada no uso e o registrador não serve para mais
   nada até a chamada.
6. **`janela()` não confere sinal**: ela faz `(x1-x0) * (y1-y0) * 2` e sai escrevendo. Altura ou
   largura negativa vira um tamanho enorme e corrompe memória.
7. **O código tem de ser gravado onde o linker o ligou** (v20, bootloop). O linker alinha a seção
   `.text` pelo maior alinhamento que encontra dentro dela: a v20 trouxe tabelas de ponteiros
   (alinhamento 4) e o `.text` saiu ligado para `0x01E96124`, enquanto o montador gravava num
   endereço fixo, `0x01E96122`. Todo o código ficou 2 bytes fora do lugar, e o primeiro gancho a
   rodar (o da partida, logo depois do logo) pulava para o meio de uma instrução: reset em menos de
   1 s, em laço. Nenhuma conferência pegou porque todas usavam o mesmo endereço fixo dos dois
   lados. Desde a v21 o código começa no primeiro múltiplo de 4, o `montar.py` lê o endereço do
   `.text` no ELF e recusa montar se não bater, e o `conferir.py` confere de ponta a ponta
   (`2c` e `2d`: cada desvio do firmware para o código novo, com os bytes que estão de fato no
   destino, decifrados, nas 6 imagens).
8. **Crescer a imagem derruba o rádio** (v28, bootloop). As 6 imagens cresceram 8 KB para dentro
   da faixa sem partição depois delas, onde o desmontado provava que nada do firmware grava
   ([seção 14](#14-flash-interna-partições-e-quem-grava)). O gravador terminou com sucesso, e o
   rádio ficou em reset contínuo logo depois do logo. O uboot e o carregador são cifrados com outra
   cifra, e a causa não foi provada; a suspeita é a PRCT, que guarda o tamanho da imagem e ficou
   com o antigo. Desde então a `2f` exige o tamanho de fábrica e o uboot e a tabela de partições
   idênticos.
9. **A atualização pula arquivo de recurso cujo CRC não mudou** (v30). Dado novo dentro de um
   arquivo do diretório `tone`, com o CRC velho na entrada, não chega ao rádio: ele fica com o
   conteúdo antigo. O `repack` refaz os CRCs do diretório (`refaz_crcs_tone`, v32).
10. **Três entradas do UFW são cifradas pela posição no arquivo** (`isd_config.ini`, `script.ver`,
    `blimit.bin`: o CRC só bate decifrando com o deslocamento delas). Mudar o tamanho de uma
    entrada anterior as move, e elas têm de ser cifradas de novo.
11. **O carregador também pula a imagem inteira cujos CRCs de dentro não mudam** (v46). A v46 só
    mudava o código do fim da imagem, fora de todo CRC, e o rádio com a v45 parou nos 5% e voltou
    nela, como a v34 gravada por cima da própria v34. Desde a v47 o `carimbo_fim` (o CRC16 do
    código do fim) mora dentro do `app.bin`, e o `conferir.py` o confere (`2i`).

## 10. Prova de que não brica

A inicialização do app lê as teclas (`0x01E56C70/C7E/C8C`) **antes** de carregar configuração e
canais; com PTT2 + atalho ela desenha `UPDATE` e fica esperando gravação. O script de montagem tem
uma trava (`PROTEGIDAS`) que **recusa** qualquer escrita nessas rotinas e nas do modo UPDATE, e a
conferência final refaz essa checagem byte a byte.

Desde a v29, a conferência `2f` exige ainda, nas 6 imagens: o tamanho de fábrica, o uboot e a
tabela de partições idênticos e os últimos 32 bytes apagados (o marcador `BOOT_START_FIRST` do
uboot). Os remendos das vozes (seção 15) ficam fora do caminho do UPDATE: a tela dele é desenhada a
partir da leitura das teclas (`0x01E56C9A`), que segue para `0x01E55AF4`.

## 11. Espectro

O SPECTRUM é o modo de tela 20 (`b[0x1533D]`). A cada volta do laço principal, depois de tratar a
tecla, `0x01E4D5A0` chama a máquina de estados da varredura, `0x01E4AEAC(255)`, e desenha um dos 5
pedaços do gráfico de 160×64 (32 colunas por volta, a partir das alturas em `0x1551B`).

O laço principal (`0x01E4DEA8`) é um temporizador de software de 10 ms, registrado em `0x01E56D20`.
De fábrica cada ponto custa duas voltas (uma sintoniza, a seguinte mede): com o desenho, ~31 ms por
ponto, e os 5 s que o dono cronometrou para 160 pontos.

**Varredura** (estado em `b[0x14654]`):

- **0, sintoniza:** grava a frequência do ponto (`[0x148DC]`) nos registradores `0x38`/`0x39` do
  chip de rádio, alterna o `0x30` e vai para 1.
- **1, mede:** `0x01E4ACFC` lê o RSSI (registrador `0x67`, 9 bits) e devolve 1 quando a medida está
  pronta (leitura diferente de 0); as 10 frequências da lista de ruídos internos (`0x6EE1`) valem
  1. Guarda a leitura crua em `h[0x1463C]` e na lista `0x15944` (u16 × 160, pelo índice do ponto) e
  a altura correspondente em `h[0x14648]`. `0x01E4AD7C` registra o ponto: maior e menor leitura da
  passada (`h[0x14644]`/`h[0x14640]`), onde está a maior (`[0x148E0]`) e a altura nas colunas do
  ponto (160 / pontos colunas por ponto). Se o nível (`h[0x14648]`) chega ao limite
  (`h[0x1464C]`), **para**: abre o áudio (`0x01E39E22(1)`, `0x01E39E2C(5)`, `0x01E39CFC(1)`), mostra
  a frequência e vai para 2. Senão, próximo ponto (`0x01E4AE44`) e volta a 0.
- **2, parado num achado:** continua medindo o mesmo ponto. Quando o nível cai abaixo do limite,
  faz a **retomada** (`0x01E4AEFC`): estado 0, fecha o áudio (`0x01E39E22(0)`, `0x01E39E2C(6)`,
  `0x01E39CFC(0)`) e passa ao próximo ponto.

**Janela** (`0x01E4AA3C(centro)`): passo da tabela `0x01E5EBC0` pelo índice `[0x6EC8]` (rotina
`0x01E4A99C`) — 625, 833, 1250, 2000, 2500 (6,25 / 8,33 / 12,5 / 20 / 25 kHz); 160 >> `[0x6EC4]`
pontos (rotina `0x01E4A9C2`: 160, 80, 40, 20); largura = pontos × passo. Grava início
`[0x148EC]`, fim `[0x148E8]`, centro `[0x148E4]` e o passo entre pontos `h[0x14650]`. O limite
(`[0x6ED5]`, entre `[0x6ED9]` e `[0x6EDD]`) vira altura de gráfico (0..64) em `0x01E4A9F2`.

Os rótulos de pontos e passo são desenhados por `0x01E4ABDC(pontos, passo)`, com as tabelas de
ponteiros `0x01E601E0` ("x160 ", " x80 ", " x40 ", " x20 ", formato `"%s "`) e `0x01E6044C`
("6.25k" … "25.0k"); o filtro de cada passo vem de `0x01E60460` (2, 1, 1, 0, 0), e logo depois dela
já há texto — nenhuma das três cresce no lugar. A configuração inteira (73 bytes em `0x6EC0`) é
**gravada** ao sair do espectro (`0x01E3FE38`) e **lida** na partida (`0x01E55E8A`), conferida só
pela soma.

O texto em dBm (`0x01E4ADFE`) é desenhado na volta ao começo da janela (o pico da passada, junto
com a frequência dele) e quando a varredura para num sinal — não a cada ponto.

**Teclas** (o código vem de `b[sp+13]`, lido em `0x01E4B946`; o tratador do espectro é
`0x01E4BFBE`, com `tbh` para os códigos 16..24):

| Código | O que faz no espectro |
|---|---|
| 16 | dígito da frequência; com 6 dígitos, recentra a janela (`0x01E4AA3C`) |
| 17 (OK) | passo seguinte (índice + 1 mod 5) e o filtro correspondente (registrador `0x43`, `0x01E4A980`); desde a v20, pela nossa ordem com o 10 kHz |
| 18 | apaga um dígito; sem dígito, chama `0x01E3FEE4` |
| 19 | **seta de cima**: limite + 2, até o máximo |
| 21 | **seta de baixo**: limite − 2, até o mínimo |
| 23 | alterna FM e AM (`[0x6ED4]`, registradores `0x31`/`0x42`/`0x2A`/`0x2B`) |
| 24 (#) | quantidade de pontos seguinte (`[0x6EC4]` + 1 mod 4); desde a v20, com 640 e 320 |
| 20, 22 | nada, só o bipe |

19 e 21 são as setas também no modo principal: lá elas levam ao `AVANCA` (19) e ao `RETROCEDE`
(21) do menu. Toda saída do tratador passa pelo bipe `0x01E38B4C` com 0 (teclas sem função), 1
(seta de cima) ou 2 (seta de baixo) — menos 17 e 24, que voltam direto para `0x01E4D5A0` e não
bipam.

**A seta continua a varredura (v19).** De fábrica, parado num achado, a seta só mexe no limite: o
jeito de seguir era subir o limite até passar do sinal. O gancho entra no lugar do `r1 = r0 − 16`
com que o tratador escolhe o caso do `tbh` (`0x01E4BFBE`, 4 bytes, alcançado só pelo caso 20 do
`tbh` dos modos): devolve a tecla em r0 e o caso em r1. Parado (estado 2), seta de cima ou de
baixo fazem exatamente a retomada do firmware e viram o caso 20, que não faz nada além do bipe; o
limite fica como estava. Em qualquer outro estado, e para qualquer outra tecla, o caso é o de
fábrica. Três coisas conferidas antes:

- **Dois valores de volta de uma função C:** o clang do pi32v2 devolve `u64` com a metade de baixo
  em r0 e a de cima em r1 (`r1_r0 = r5_r4` no desmontado). É o que permite trocar uma instrução que
  deixa dois registradores por uma chamada.
- **r2 e r3 mortos no ponto do gancho.** Nesta ABI só r0..r3 são livres para o chamado (r4..r15 são
  salvos nos prólogos, `{rets, r15-r4}` inclusive). A chamada nova suja r2 e r3 onde antes não
  havia chamada, então eles têm de estar mortos ali: conferido caminho a caminho, de `0x01E4BFC2`
  em diante, seguindo os casos dos `tbh` e entrando nas rotinas chamadas antes de qualquer escrita
  deles. Nenhum caminho lê r2 ou r3 antes de escrevê-los; o único que sai do app é
  `0x020015C2`, a entrada de seção crítica (`cli`, contador, `sti`), que não os lê.
- **A retomada é a do firmware, conferida no build:** a verificação `4i` do `conferir.py` tira dos
  bytes de fábrica as quatro chamadas da retomada (`0x01E4AF00/06/0C` e `0x01E4AF86`) e o r0 de cada
  uma, e exige que o gancho faça as mesmas, na mesma ordem, e zere a mesma variável de estado.

Se o sinal for forte, os pontos vizinhos do mesmo sinal também podem passar do limite — é o que o
gráfico mostra como um morro de vários pontos — e a varredura para de novo logo ao lado. Fazer a
seta pular o morro inteiro precisa guardar "estou pulando" entre uma volta e outra, ou seja, um
byte de RAM nosso, e este firmware ainda não tem RAM comprovadamente livre.

**Passo de 10 kHz, 320 e 640 pontos, velocidade e pico (v20).**

- **Índices compatíveis:** como a configuração é gravada, os índices de fábrica mantêm o
  significado (pontos 0..3, passo 0..4) e os novos entram no fim: pontos 4 = 640 e 5 = 320, passo
  5 = 10 kHz (filtro 1, o mesmo do 8,33 e do 12,5). Só a ordem das teclas muda: o # faz 160 → 80 →
  40 → 20 → 640 → 320, e o OK faz 6,25 → 8,33 → 10 → 12,5 → 20 → 25. Valor fora da faixa vale como
  o primeiro.
- **Rotinas trocadas inteiras** por um `goto` de 4 bytes na entrada (o `goto` não mexe no `rets`,
  então a nossa volta direto para quem chamou): `0x01E4A9C2` (pontos), `0x01E4A99C` (passo) e
  `0x01E4ABDC` (rótulos). Nenhum desvio do firmware cai no meio delas — todos vão à entrada. As
  teclas 17 e 24 passam a ser tratadas no gancho das teclas, que faz o mesmo que o firmware faz em
  seguida (`0x01E4C628..0x01E4C664`: filtro, janela, frequência, rótulos, varredura do começo).
- **320 e 640 pontos:** o índice do ponto é um byte e o gráfico tem 160 colunas, então a máquina
  do firmware continua varrendo 160 colunas, com o passo entre colunas × 2 ou × 4, e cada coluna
  mede 2 ou 4 sub-pontos no passo de verdade e fica com o maior. Qual sub-ponto está sintonizado
  sai da própria frequência (`(freq − início) / passo % sub-pontos`), sem memória nova; o próximo
  ponto (chamadas `0x01E4AF86` e `0x01E4C6B8`) passa pelo `gancho_avanca`, que anda dentro da
  coluna e só no último sub-ponto deixa o firmware passar para a coluna seguinte. Simulado para as
  36 combinações: toda frequência da janela uma vez por passada, na ordem e na coluna certa.
- **Um ponto por volta do laço:** na chamada da varredura (`0x01E4D5B2`), quando a volta mediu e
  passou ao ponto seguinte, a mesma volta já o sintoniza. Ele só é medido na volta seguinte, como
  no firmware: o chip tem o mesmo tempo para estabilizar, e a medida sai nas mesmas condições. É o
  dobro da velocidade (≈ 2,5 s para 160 pontos). Ir além disso exige encurtar a espera do chip
  entre sintonizar e ler, e nenhum ponto do firmware diz quanto ela pode ser.
- **Pico seguro:** o registro do ponto (chamadas `0x01E4AEE0` e `0x01E4AF7A`) passa pelo
  `gancho_registra`, que faz o mesmo que `0x01E4AD7C` com uma diferença: a barra sobe na hora e,
  quando o sinal cai, desce a metade do caminho a cada passada. A janela nova (frequência digitada,
  `0x01E4C054`; entrada, `0x01E4CFBC`; e OK/#) zera as alturas antes, para as barras de outra
  janela não ficarem descendo por cima.
- **Registradores:** este compilador aproveita, entre funções do mesmo arquivo, quais registradores
  cada uma suja. As chamadas e rotinas trocadas sujavam todos de r0 a r3 (direto ou pelas que
  chamam), e as nossas não sujam nada além disso; então nenhum chamador do firmware pode contar com
  r0..r3 depois delas.
- **Conferido no build:** cada chamada trocada (de onde vinha e para onde vai) e cada `goto` de
  entrada (`4h`), os endereços das variáveis tirados das instruções do firmware que as usam
  (`4f`), e a retomada da seta (`4i`, agora pelo `gancho_avanca`).

**Barra atual, frequência que roda, pico e dBm (v26–v27).**

- **Barra atual em branco.** O `gancho_coluna` entra nas cinco chamadas que desenham as barras
  (`0x01E4D600`, `0x01E4D644`, `0x01E4D688`, `0x01E4D6CC`, `0x01E4D710`), no lugar de
  `0x01E4AAA8`, que desenha um retângulo x0..x1 × y0..y1 com a cor no 5º argumento (na pilha, como
  o firmware a passa). O laço redesenha uma faixa de 32 colunas por volta, em `b[0x14697]`
  (a `4f` tira o endereço do `r13 = 0x14680` de `0x01E4B422`), e cada barra chega com x relativo à
  faixa. A coluna é faixa × 32 + x, contra `ESP_INDICE × (160 / pontos)`; as colunas do ponto
  saem em `0xFFFF`, com o topo limitado a 61 (3 px no mínimo). A chamada da linha do limite
  (`0x01E4D72E`) não passa pelo gancho.
- **Frequência que roda.** No `gancho_espectro`, a volta que sintonizou um ponto novo chama
  `MOSTRA_FREQ(ESP_FREQ)` só quando a faixa que o laço vai redesenhar é a do ponto: a frequência e
  a barra branca trocam juntas, a cada 5 voltas. Com `b[0x14A35]` diferente de zero (dígitos
  sendo digitados, a mesma checagem que o firmware faz antes de mostrar o pico) não escreve.
- **Pico na segunda linha.** A chamada do fim da passada que mostrava o pico na linha de cima
  (`0x01E4AEA4`, para `MOSTRA_FREQ`) vai para o `mostra_pico`: janela 40..112 × 36..48, fonte de 12,
  no formato do firmware (`"%03d.%05d"` em `0x01E5FF26`, conferido pela `4f` contra o de
  `0x01E45DB2`). O `gancho_janela` a apaga quando a janela muda.
- **dBm à direita.** Três imediatos da rotina dele (`0x01E4ADFE`): `r0 = 40 → 112` em
  `0x01E4AE12` e `r1 = 120 → 160` em `0x01E4AE14` (a janela), `r1 = 56 → 116` em `0x01E4AE30` (o
  x do texto). O AM/FM tem a janela 127..159 × 17..49 com o texto em y 20, e o dBm começa em y 36.
- **Passo (v27).** O rótulo do passo é desenhado em y 37, a altura do pico e do dBm.
- **AM/FM sem apagar o dBm (v34).** A rotina do AM/FM (`0x01E4B04C`, chamada só pela tecla 23 e pela
  entrada) abre a janela `127..159 × 17..49`, limpa e escreve em 12 px em y 20; o fim dela passou de
  y 49 para 33 (`r3` em `0x01E4B066`), porque o dBm mora em `112..160 × 36..48`.
- **Pico em verde (v34).** O `mostra_pico` escreve pelo `escreve_cor`, o caminho ASCII de 12 px do
  `escreve()` com a cor da letra no lugar do branco fixo (mapa em
  [mapa-da-tela.md](mapa-da-tela.md#10-texto-escreve-as-fontes-e-o-glifo)).

**O desenho do espectro e o tema B (v41).** O firmware desenha o espectro em pedaços, cada um pela
sua rotina e na sua janela:

| Rotina | Janela de fábrica | Quem chama |
|---|---|---|
| limpeza `LIMPA(0)` (`0x01E3ACBA`) | faixa de cima `0x2965` (y 0..17) e o resto preto | entrada (`0x01E4CFE2`) |
| frequência (`0x01E45D96`, `"%03d.%05d"` na fonte de 16) | 42..128 × 20..38 | entrada, fim da passada, achado, teclas 16 e 18, OK e #, e o fim da digitação no laço do sinal |
| dígitos digitados (`0x01E4AFC2`, máscara `"---.-----"`) | a mesma | teclas 16 e 18 |
| AM/FM (`0x01E4B04C(m)`, texto `0x01E5EBB4[m & 1]`) | 127..159 × 17..49 (33 desde a v34) | entrada e tecla 23 |
| dBm (`0x01E4ADFE(v)`, `"%03ddBm"`) | 40..120 × 36..48 (112..160 desde a v26) | fim da passada, achado e cada volta parado num achado |
| pontas (`0x01E4AB2A(início, fim)`) | 0..160 × 113..128: linha, marcas em x 14, 80 e 145, e as duas frequências | entrada, tecla 16, OK e # |
| gráfico, no laço principal | 5 janelas de 32 × 64 em y 49..113, uma por volta, cheias de preto | `0x01E4D5DA`, `0x01E4D61E`, `0x01E4D662`, `0x01E4D6A6`, `0x01E4D6EA` |
| barras (`0x01E4AAA8`) e a linha do limite (`0x07E0`) | x relativo à fatia, topo = 64 − altura | o `gancho_coluna` e `0x01E4D72E` |

Na v41 as cinco rotinas da frequência, dos dígitos, do AM/FM, do dBm e das pontas são trocadas
inteiras por um `goto` na entrada (`ESPECTRO_B_FUNCOES` no `montar.py`). A limpeza da entrada, as
cinco janelas das fatias e a linha do limite viram chamadas trocadas (`ESPECTRO_B_CHAMADAS`). Os
destinos ficam no `tela.o`, e os ganchos do `.vozes` (rótulos, pico, janela nova e barras) só
chamam as peças de lá. Cada peça abre a sua janela e só desenha o que é dela: o pico e o dBm mudam
em momentos diferentes, e não há RAM onde guardar um para redesenhar junto com o outro. A prova:

- **Os sítios novos só são alcançados pelas portas do modo 20.** O fecho para trás (`so_por.py`)
  de cada um para no caso 20 do `tbh` dos modos, no caso 36 das ações de tecla ou na queda do laço
  principal.
- **O sítio do laço do sinal** (`0x01E46E5C`) tem uma porta própria, o `if (r0 == 20)` de
  `0x01E46B8E`, que lê `b[r6 + r9]`. A rotina vai de `0x01E4622A` a `0x01E48336`, e nada de fora
  desvia para o meio dela. No grafo da rotina, montado com as tabelas dos `tbb`/`tbh`, todo caminho
  da entrada até ali passa por `r9 = 3261` (`0x01E46544`) e por `r6 = 83584` (`0x01E46268`). O
  `r9` não é escrito em outro lugar, e do `r6 += 3264` de `0x01E4726E` não se volta ali sem passar
  de novo pelo `r6 = 83584`.
- **As cinco rotinas só são chamadas desses sítios** (as chamadas de 4 bytes, varridas no `app.bin`
  inteiro, e as de 2 bytes do dBm, no desmontado). Nenhuma constante de 32 bits aponta para elas,
  nenhum desvio cai no meio delas e elas só desenham: janela, texto num buffer local, `envia()`.

Os quatro imediatos que a v26 e a v34 mudavam dentro do dBm e do AM/FM saíram, porque as rotinas
inteiras não rodam mais.

**A luz acesa no espectro (v49).** A contagem do ABR fica em `h[0x14F80]`: quem acende a luz
(`0x01E3951C`) a carrega pelo ABR e chama `0x01E394D6(1)`, e o tique a desce nos modos 0, 2, 3 e
11 (a máscara 2061 em `0x01E41876`) e no 20 (`0x01E41A0C`). Quem apaga é o laço do sinal, na
cabeça dele (`0x01E4655A`, por onde todos os modos passam a cada volta): com o ABR ligado, fora do
modo 4 e com a contagem em 0, `0x01E394D6(0)` em `0x01E46578`. É o único chamador que passa 0 (a
marca da luz, `b[0x15386]`, só vai a 0 dentro dela; os outros três acendem). O `gancho_luz` entra
nessa chamada e não a repassa no modo 20. Nada é gravado: ao sair do espectro, ou ao religar o
rádio (a partida não grava 20 no modo; só a entrada por tecla, `0x01E4CF7E`), o ABR volta a valer.

## 12. Roger

O item ROGER do menu (índice 20 de fábrica) guarda o valor em **`b[0x14B83]`** (offset `0x17` do
bloco de ajustes; `0x9017` no cabo, 2 bits no CHIRP): 0 OFF, 1 BEEP, 2 TONE1200. As tabelas de
nomes das opções moram na RAM (`0x6E3C` em inglês, `0x6E30` em chinês), com outros dados colados
depois. A partida (`0x01E55C9A`) corrige a faixa de seis campos dos ajustes, e o roger não está
entre eles.

O roger é tocado no **fim da transmissão**, `0x01E44604`, chamada ao soltar o PTT
(`0x01E46D8C`), num segundo fim de TX (`0x01E47A12`) e num caminho de desligamento
(`0x01E4D806`). Ela toca o tom ainda transmitindo e depois volta para a recepção:

- **BEEP:** liga o gerador de tom (`0x01E43AFE(1)`: registradores `0x24`, `0x70` = `0xE0E0`,
  `0x3F`) e toca dois pares de frequências de uma tabela (`0x01E60DC8`), 80 ms cada (espera de 40 unidades de 2 ms), por
  `0x01E43B4E(f1, f2)` (registradores `0x71` e `0x72`): **1000 Hz e 850 Hz**. Depois desliga.
- **TONE1200:** não é tom, é um pacote de dados FSK — programa o modulador do chip (`0x58`–`0x5F`)
  e manda 14 palavras a 1200 Hz.
- A frequência no registrador é **Hz × 10,32444**: 10324 = 1000 Hz, 8776 = 850 Hz e 12389 = 1200 Hz
  (o TONE1200), tudo lido dos bytes de fábrica.

**BEEP 2 (v22):** a opção nova, com o valor 3 (cabe nos 2 bits). O menu mostra OFF, BEEP 1,
BEEP 2, TONE1200; o get e o set do item são nossos e trocam posição e valor (2 ↔ 3), então BEEP e
TONE1200 continuam com os valores de fábrica, e o driver do CHIRP faz a mesma troca. Na rotina de
fim de TX, o desvio `0x01E4461E` "se não for 1, pula" (`80 f8 c2 02`) virou "se for 0, pula"
(`00 f8 c2 00`, conferido no `llvm-objdump`): o valor 3 entra no caminho do BEEP. E o laço dos tons
(`0x01E44638`) virou uma chamada para o nosso `gancho_roger_tons` e um desvio para o fim dele
(`0x01E44656`): com o BEEP 1 ele faz o mesmo que o laço fazia; com o BEEP 2 toca **1125, 1857, 1359
e 931 Hz, 100 ms cada** (o mp3 que o dono mandou, medido em janelas de 1 ms; na v22 saíam 200 ms
cada, porque a espera conta em unidades de 2 ms — corrigido na v23). Conferido antes:
ninguém de fora desvia para dentro do laço, r5 e r6 são escritos antes de lidos dali em diante, e
r0..r3 já chegavam ali como lixo de chamada. A verificação `4j` do `conferir.py` tira dos bytes de
fábrica a variável, a tabela do BEEP e as rotinas de tom e de espera, e confere os dois remendos.

**COIN (v26; chamado de BEEP 3 até a v30).** Valor 4. O menu tem cinco opções (OFF, BEEP 1,
BEEP 2, COIN, TONE1200) e duas tabelas traduzem posição ↔ valor (`roger_valor`,
`roger_posicao`). O desvio `0x01E4461E` (se for 0, pula) já leva o 4 ao caminho do BEEP, e o 2
desvia antes, em `0x01E4461A` (`if (r0 == 2)`, de fábrica, conferido pela `4j`). Os tons: a moeda
do Super Mario World medida do mp3 do dono, 2000 Hz por 70 ms e 2672 Hz por 330 ms (`TOM(20649)`,
`ESPERA_2MS(35)`, `TOM(27587)`, `ESPERA_2MS(165)`). No CHIRP o valor tem 3 bits: os 2 do roger e
o bit baixo do `unused_9017`.

## 13. Scramble

O SCRAMBLE do chip é **inversão de espectro** da voz, não criptografia: o áudio é espelhado em
torno de uma frequência, e quem recebe precisa espelhar na mesma para entender. É o bit 1 do
registrador `0x31` do chip de rádio, e a frequência de inversão vai no registrador `0x71`, o mesmo
que o gerador de tom usa (Hz × 10,32444, a conta do roger).

O caminho do valor, do menu até o chip:

1. **Item SCRAMBLE** (índice 47 de fábrica, 2 opções: OFF e ON). O get `0x01E3D058` devolve
   `b[0x15E2C] & 3`. O set `0x01E3D06C` grava o valor no **nibble alto** do byte 14 do registro do
   canal (`0x15E52`) ou do byte 16 do VFO (`0x15E90`), mantendo o nibble baixo.
2. **Carga do canal** (`0x01E3900C`): `b[0x15E2C] = byte 14 >> 4`, o nibble inteiro (no VFO, o byte
   16).
3. **Sintonia** (`0x01E43FCA`): guarda o valor em `b[0x129A4]`.
4. **Aplicação** (`0x01E408E2`): com 0, ou na faixa aérea (108–136 MHz), apaga o bit 1 do `0x31`.
   Senão, liga o bit e escreve no `0x71` uma entrada da tabela **`0x01E68B18`**: o valor 1 usa a
   primeira, **35103 = 3400 Hz**, e qualquer valor maior usa a segunda, 36135 = 3500 Hz, que
   nenhuma opção do menu alcançava.

Todos os leitores de `b[0x129A4]` (`0x01E43B40`, `0x01E4431A`, `0x01E443D2`) só testam zero ou não
zero, e a única instrução que aponta a tabela é `0x01E40936` (o byte seguinte, `0x01E68B1C`, já é
outra tabela, lida por `0x01E1440C`).

**Medido** numa gravação do dono, com um tom de 1 kHz transmitido com scramble e recebido sem: o
BJ-8300 inverte em **3400 Hz**, o que confirma a tabela. O Baofeng UV-17L inverte em **2900 Hz** no
modo 3, com a portadora residual visível em 2900 Hz (−45 dB). É o mesmo método, só que em outra
frequência. Os modos 1 e 2 do UV-17L também não batem com o BJ-8300 (testado pelo dono). O tom
de chamada do menu TONE (1000/1450/1750/2100 Hz, tecla lateral, `0x01E412D4` → `0x01E408CA`) usa o
mesmo registrador `0x71`, e por isso não serve para medir o scramble.

**v24:** a segunda entrada da tabela passou a 29941 = 2900 Hz, e o item ganhou a terceira opção
(OFF, 3400 Hz, 2900 Hz), com o get e o set de fábrica. Testada com o UV-17L no modo 3: os dois se
entendem.

**v25: de 2700 a 3400 Hz, de 100 em 100**, para cobrir os outros modos do Baofeng. São nove
opções em ordem crescente: OFF, 2700, 2800, …, 3400 Hz. Testada: os modos 1, 2 e 3 do UV-17L
se entendem com o BJ-8300, cada um numa dessas frequências.
- **Frequência pelo valor.** Na aplicação, os 14 bytes que escolhiam a entrada (`0x01E4092E`:
  `r0 = 1; se r4 == 1, r0 = 0; r1 = 0x01E68B18`) viraram `r0 = r4 & 0xF; nop; r1 =
  scramble_freqs`. A instrução seguinte, de fábrica (`r1 = h[r1 + r0<<1]`), passa a ler a
  frequência da nossa tabela de 16 entradas, pelo valor gravado. As codificações saíram do próprio
  firmware (`40 16` em `0x01E005E6`, `60 e1 0f 00` em `0x01E00350`, `00 00` em `0x01E0019A`), e o
  trecho foi conferido no `llvm-objdump` da v25. O r4 é o valor que a rotina recebeu em r0, e a
  chamada de `0x01E4092A` o preserva. Com a máscara, qualquer byte cai dentro da tabela, e por isso
  não foi preciso provar a origem de cada caminho até aqui. A tabela de fábrica fica intacta, e
  ninguém mais a lê.
- **Valores que não mudam.** O 1 continua 3400 Hz (o ON de fábrica, o que fala com os outros
  BJ-8300) e o 2 continua 2900 Hz (o da v24). Os novos são 3 = 2700, 4 = 2800, 5 = 3000, 6 = 3100,
  7 = 3200 e 8 = 3300 Hz. De 9 a 15, valores que nenhum menu grava, o rádio inverte em 3400 Hz e o
  menu mostra 3400 Hz.
- **Menu em ordem.** O get (`scramble_le`) e o set (`scramble_grava`) são nossos e traduzem
  posição ↔ valor. O set põe o valor em `h[0x14BFC]`, chama o set de fábrica e devolve a posição.
  Assim o nibble do canal ou do VFO e o `b[0x15E2C]` continuam gravados pelo firmware. O get e o set
  de fábrica só eram usados pelo item 47 da tabela de menus.
- **CHIRP.** O campo scramble do canal pega os bits 7–2 do byte 14, então cada opção é o valor
  << 2. No VFO, o formato da base parte o nibble em `unknown3` (bits 7–5) e `scramble` (bits 4–2),
  e o driver troca esse ajuste pelo dele. O teste lê do `bancos.c` o valor que cada opção grava.
- **Conferência `4k`.** Tira a conta Hz → registro da tabela de fábrica (3400 Hz) e confere no
  blob cada frequência, o menu de 2700 a 3400 Hz de 100 em 100, os nomes, a ida e volta
  posição ↔ valor, que todo valor de 0 a 15 aparece no menu com a frequência em que o rádio
  inverte, e os 14 bytes do remendo. Foi testada trocando duas posições no blob, e quatro dessas
  conferências acusaram.

## 14. Flash interna: partições e quem grava

Levantado em 2026-10-25 no diretório do app de cada uma das 6 imagens e no desmontado.

| Faixa (`flash.bin`, 1 MB) | O que é |
|---|---|
| `0x00A0–0x2088` | uboot (cifrado com uma cifra que não é a do `enc`/`sfc`) |
| `0x2088–0x210D` | `isd_config.ini` embutido (SPI, UART do uboot, RESET, EOFFSET) |
| `0x3000` | diretório do app (`app_dir_head`) |
| `0x3100–0x7B1DC` | `app.bin` |
| `0x7B1DC–0x9886C` | diretório `tone`: vozes e bipes ([seção 15](#15-vozes-fora)) |
| `0x9886C–0x99121` | `cfg_tool.bin` e dados do SDK |
| `0x99121–0x9A000` | sobra apagada: o nosso código, até 32 B antes do fim |
| `0x9A000–0xD2000` | sem partição (~224 KB) |
| `0xD2000–0xEE000` | VM (os ajustes) |
| `0xEF000` em diante | BTIF, USER, USERLOGO e `key_mac` (`0xFF000`) |

As outras variantes seguem o mesmo desenho: o app em `0x2300` (flash2/4/6) e flash de 2 MB
(flash3/4). Para o processador, o endereço é `0x01E00000 + (posição − início do app_dir)`, e por
isso é o mesmo nas 6. A partição **PRCT** tem o tamanho da imagem (0x9A000 ou 0x99000: o
`CODE_LEN` da ferramenta). Os últimos 32 bytes de cada imagem são do marcador `BOOT_START_FIRST` do
uboot (documentado para a família anterior por kagaimiq), e o nosso código termina antes deles.

**Quem grava na flash interna.** Todas as chamadas das primitivas de escrita (`0x01E37A9A`, e
`0x01E37A28` direto) caem em:

- o driver da VM (`0x01E37AB8..`, `0x01E514E0..`): só nos dois bancos da VM;
- a **atualização pela UART**, que grava um carregador (`uart_update.bin`, 21,5 KB, de dentro do
  `ota.bin`) como `LOADER.BIN` antes de reiniciar (`0x01E37FB2`, chamada só de `0x01E69780`). A
  posição sai do início e do tamanho da VM atual (lidos na partida, `0x01E130E4`), da VM do
  firmware que está chegando (o cabeçalho dele, `0x01E696D2`) e do tamanho da flash, alinhada a
  128 KB: o topo da VM, e a rotina recusa (erro 255) qualquer posição abaixo do início dela;
- um registro de 80 bytes no fim da flash (`0x01E381E4`);
- o bloco `exif` (`0x01E50B7A`): a partição não existe, a base fica 0 e os deslocamentos são de
  16 bits;
- a escrita genérica de arquivo (`0x01E13492`): só dentro da partição aberta.

Nada grava na faixa sem partição — e **mesmo assim crescer a imagem para dentro dela derrubou o
rádio** (v28): o que o uboot e o carregador fazem com o tamanho da imagem não aparece no
desmontado do app. O tamanho das imagens e a tabela de partições ficam como a JieLi fez (`2f`).

**O carregador recusa mudança demais nas vozes (v35).** Na gravação pelo BinTalk, o rádio pula para
o carregador, o `uart_update.bin` padrão da JieLi (de 19/05/2023, dentro do `ota.bin`). É ele que
pede os pedaços do `.anc` e grava. Ele é cifrado e não se lê. O que foi medido no rádio
([histórico](historico.md), v35):

- uma imagem que muda os arquivos de voz C0..C11 é recusada antes de qualquer escrita. Lido pelo
  cabo depois, o `tone` estava byte a byte o da v34. Não importa o conteúdo, nem o que havia no
  rádio;
- mudar só o C0 e o C1 passa;
- mudar o `app.bin` sempre passou.

O porquê (quantidade de arquivos, de setores ou de bytes) não se sabe sem ler o carregador. O
código novo grande foi para dentro do `app.bin` ([seção 16](#16-o-g729-e-o-mp3-fora-v36-e-v37)), e a `2g` recusa
montar se mudar qualquer arquivo de voz além do C0 e do C1.

**O carregador pula a imagem cujos CRCs de dentro não mudam (v46).** Gravar a v34 por cima da
própria v34 parava nos 5% e voltava (25/10), e a v46 por cima da v45 fez o mesmo (26/10): ela só
mudava 1 byte do código do fim da imagem, que o CRC do `app_area_head` não cobre (ele vai só até o
fim do `app.bin`). Todas as versões que gravaram mudaram o `app.bin`. Desde a v47 o build grava o
CRC16 do código do fim no `carimbo_fim`, dentro do `app.bin`, e a `2i` confere.

## 15. Vozes fora

O diretório `tone` vem logo depois do `app.bin` (para o processador, `0x01E781DC`), com 127
arquivos: o `index.idx` (assinatura `TIDX`, em `+0x1000`), as vozes **C0–C49** (chinês, 47,5 KB) e
**E0–E67** (inglês, 66 KB), contínuas de `+0x15C0` a `+0x1D514`, e os bipes **B0–B7** (272 B). Os
deslocamentos contam do cabeçalho do diretório, e os dados são cifrados em SFC como o resto. O
`md5.bin` citado nas strings não é lido por ninguém.

**Quem toca.** Conferido em toda carga de endereço que cai nas strings dos caminhos, inclusive as
que servem de base para um deslocamento:

| Caminho | O que toca |
|---|---|
| fila de sons (`0x01E405E2`), a única que lê a tabela de 126 caminhos (`0x01E5E8FC`, lida só em `0x01E40634`) | números 0–117 = vozes, 118–125 = bipes |
| modo terminal que o `main` escolhe (`0x01E55AAE`, chamado em `0x01E57A2E`) | C16, se `b[0x153F3]` |
| tratador de eventos (`0x01E55740`), pela intermediária `0x01E555C6` | C1, C2, C3 e C15 (base + 0, 25, 50 e 100) |
| outro modo terminal (`0x01E55A60`) e o fim de transmissão | B4 e B7 |

**Os três remendos (v30).** Codificações conferidas no `llvm-objdump`, e nenhum desvio cai no meio
dos trechos:

| Sítio | De fábrica | Novo |
|---|---|---|
| `0x01E40628` | `35 e9 76 80` `ifs (r5 >= 118) {` | `85 f9 1c ec` `if (r5 < 118) goto 56` (o retorno) |
| `0x01E55ABE` | `01 53` `if (r1 == 0) goto 38` | `04 93` `goto 38` |
| `0x01E555D6` | `03 50` `if (r3 == 0) goto 32` | `04 90` `goto 32` |

Cada um vira o caminho "sem voz" que já existia ali. O ajuste VOICE fica em `b[0x14B73]` (o "falar
número", `0x01E409A2`, já o confere), e o item saiu do menu.

**O espaço:** `0x01E797A0..0x01E956F0` (114.512 B), a seção `.vozes` do linker, no mesmo endereço
nas 6 imagens, dentro da imagem de fábrica.

**A atualização pula recurso com CRC inalterado.** Cada entrada guarda o CRC16 do conteúdo
decifrado do arquivo, e o cabeçalho do diretório guarda o de tudo depois dele. Na v30 o conteúdo do
C0 mudou com o CRC velho na entrada, e o rádio ficou com a voz original: lido pela memória do cabo
na v31, só os bytes trocados diferiam, e os vizinhos batiam com o `.anc`. Na v32 o `repack` refaz
os CRCs (`refaz_crcs_tone`), e o dado chegou. A `2g` confere os remendos, que a tabela de caminhos
é lida num lugar só, que o bloco `.vozes` cai dentro das vozes nas 6 imagens, que o resto do
diretório é o de fábrica menos os CRCs, e que todo CRC confere com o conteúdo.

**Código no espaço das vozes (v33).** O espectro mora ali: as 10 rotinas que o firmware chama (os
ganchos das teclas, da varredura, do registro, do próximo ponto, da janela, das barras e do pico, e
as três rotinas trocadas inteiras), as ajudantes e as tabelas, 946 B (`.vozes.text` e
`.vozes.rodata`). A área do fim da imagem fica com o resto, 2268 dos 3260 B.

Nada do espectro roda sem o modo de tela 20 (`b[0x1533D]`). A prova é um fecho para trás, no
desmontado de fábrica, a partir de cada sítio de gancho. Ele junta toda instrução que pode chegar
ali:

- a anterior, quando cai nela (dentro de um bloco `if (...) {` conta como se caísse sempre);
- a origem de todo desvio;
- todo caso de `tbb`/`tbh`, com a tabela lida do `app.bin` e a quantidade de casos pela guarda (os
  bytes da tabela não contam como instrução).

Falha se chegar a uma entrada de rotina, a uma constante igual ao endereço, a um `tbb`/`tbh` sem
guarda ou a uma instrução a que nada leva. Todo sítio para numa destas três portas:

| Porta | Leva a |
|---|---|
| caso 20 do `tbh` dos modos (`0x01E4B988`) | tratador das teclas do espectro (`0x01E4BFBE`) |
| caso 36 do `tbh` das ações de tecla (`0x01E4BE28`) | entrada do espectro (`0x01E4CF78`), que grava o 20 no modo |
| queda do `if (r0 != 20) goto` do laço principal (`0x01E4D5AC`) | varredura e barras (`0x01E4D5B0`) |

As rotinas de fábrica do meio do caminho (`0x01E4AEAC` varredura, `0x01E4AE44` próximo ponto,
`0x01E4AD7C` registro, `0x01E4AA3C` janela, `0x01E4A9E6`, e as trocadas `0x01E4A99C`, `0x01E4A9C2`
e `0x01E4ABDC`) só são chamadas dessas portas ou umas pelas outras, sempre pela entrada, e nenhuma
constante de 32 bits aponta para elas. O modo também foi conferido:

- das 25 escritas de byte nele, só a de `0x01E4CF7E` grava 20; as outras gravam 0, 1, 2, 3, 4, 7,
  8, 11 e 17;
- nenhuma escrita de 16 ou 32 bits cobre o byte;
- ele fica fora dos dados iniciais da RAM, que vão até `0xD4D0`.

Se o código dali não rodasse, o rádio cairia ao entrar no espectro. A partida, o cabo e o PTT2 +
atalho não passam por ele.

No build:

- a `2b` exige essas 10 no `.vozes` e as outras no `.text`;
- a `2d` exige que só os sítios do espectro desviem para o `.vozes`;
- a `4g` exige que nada do `.text` chame o `.vozes`.

As três falharam numa montagem com a partida e o `zera` no `.vozes`. A `2d` passou a contar só os
desvios que nós gravamos, os bytes diferentes dos de fábrica. Lida a cada deslocamento par, ela
também acha "desvios" no meio de uma instrução: o meio da chamada em `0x01E16A10` "vai" para
`0x01E79878`. Desvio de fábrica de verdade para o código novo não existe:

- o fim da imagem era espaço livre;
- no espaço das vozes só caem 56 "desvios" do fim do `app.bin` (`0x01E734C6..0x01E77CBA`), a
  cópia do código que roda na RAM e os dados iniciais, lidos fora do endereço deles;
- o código da RAM, desmontado no endereço dele, não desvia para lá.

**Tela principal B (v35, daqui para fora na v36).** Na v35 o `tela.o` inteiro morava aqui também
(6,6 KB contando o espectro), e o carregador recusou a imagem, porque ela mudava os arquivos C0..C11
([seção 14](#14-flash-interna-partições-e-quem-grava)). Desde a v36 a tela mora no lugar do G.729
([seção 16](#16-o-g729-e-o-mp3-fora-v36-e-v37)), e aqui fica só o espectro, dentro do C0 e do C1.

## 16. O G.729 e o MP3 fora (v36 e v37)

A tela B (5.636 B) foi para dentro do `app.bin`, no lugar do decodificador G.729, o das vozes
`.wtg`, que não tocam desde a v30.

**A faixa.** `0x01E257E0..0x01E278C8`, 8.424 B, com:

- o código do G.729;
- as tabelas da norma: a de cossenos em `0x01E2686C` e o dicionário LSP em `0x01E2696C`;
- as rotinas da biblioteca dele, apontadas pela tabela de operações `0x01E26730`.

Logo antes, em `0x01E257C8..0x01E257DF`, há duas estruturas de 12 B do tocador, lidas em `0x01E3000A`
e `0x01E30012`, que ficam. Logo depois, em `0x01E278C8`, começa o decodificador de MP3: ele é chamado
de `0x01E2ECAA` e apontado pela tabela `0x01E2C084`.

**Como um decodificador é escolhido.** O registro `0x01E34838` tem três decodificadores de 68 B
cada, com o tipo e as rotinas:

| Tipo | O que é | Rotinas |
|---|---|---|
| `0x40` | G.729 (`wtg`) | `0x01E257E0..`, na faixa |
| `0x01` | MP3 | `0x01E2EC52..` |
| `0x100` | seno, para os bipes `.sin` | na RAM (`0x472C..`) |

Só duas buscas leem o registro; são as únicas cargas do endereço dele:

- `0x01E2F1D2`, por tipo exato;
- `0x01E2F2CE`, por máscara: o tipo tem de ter algum bit em comum com a máscara pedida.

A partida do decodificador (`0x01E30646`, chamada só de `0x01E308BE`) usa a busca exata quando a
máscara pedida tem um bit só, e a de máscara nos outros casos. A cópia que roda na RAM chama a
exata com `0x100`.

**Quem pede o tipo `0x40`.** Decodificador só é criado por `0x01E3022E`. Ela é chamada por três
rotinas, e as três só são chamadas pelo tocador de sons (`0x01E40318`):

- `0x01E3041C`, para o arquivo que não é `.sin`. O tipo sai da extensão pela tabela `0x01E60D40`
  (`wtg` `0x40`, `msbc` `0x20`, `sbc` `0x10`, `mty` `0x400`, `aac` `0x200`, `mp3` `0x01`,
  `wma` `0x02`, `wav` `0x04`). Extensão desconhecida dá 0, e aí vale a entrada padrão
  `0x01E30BE4`, que é `0x40` de novo;
- `0x01E3039A`, para o `.sin`, e `0x01E302E0`, para o seno pelo índice: as duas usam a entrada
  `0x01E30C18`, de tipo `0x100`.

O tocador decide pela extensão do arquivo aberto, comparando-a com `"sin"` (`0x01E5FBCE`) em
`0x01E1A608` (3 letras, sem diferenciar caixa). No diretório de sons, os arquivos que não são
`.sin` são as vozes e o `index.idx`, que ninguém toca, e toda voz está cortada desde a v30
([seção 15](#15-vozes-fora)). O `ring.*` das strings não existe no diretório: o arquivo não abre,
e o tocador volta antes de criar o decodificador.

**Nada de fora entra.** No desmontado de fábrica, da flash e da cópia da RAM, o que aponta para
dentro da faixa vindo de fora dela é:

- os 6 ponteiros do registro;
- dez "desvios" que o desmontador mostra, e que são tabela. Oito caem no meio dos 4 KB de pesos da
  redução de ruído (`0x01E21C78..0x01E22788`, dentro do bloco `0x01E21A2C` que o descritor dela
  aponta em `0x01E77FA0`); os outros dois, `0x01E257D0` e `0x01E257D8`, caem nas estruturas de 12 B
  logo antes da faixa. Nem o alcance mais conservador, cujas raízes são a entrada e todo valor de
  32 bits que caia em código, chega a algum deles.

Fora o registro, nenhum imediato `rX = imm` e nenhum valor de 32 bits solto, em todo o `app.bin`,
aponta para a faixa.

**A trava.** O tipo do G.729 no registro vira 0, e nenhuma das duas buscas casa o 0:

- a busca exata sempre recebe um tipo com um bit só (ou o `0x100` da cópia da RAM);
- a de máscara exige pelo menos um bit em comum.

Um pedido de `.wtg` que tivesse escapado do mapa sai pelo mesmo caminho de fábrica que um formato
sem decodificador toma, em vez de executar a tela B como se fosse áudio.

**A redução de ruído fica.** Os ~32 KB de `0x01E1C02C..0x01E22B2C`, que pareciam livres por não
serem código, são os pesos da redução de ruído. Os descritores deles estão em `0x01E77EC0..`, que vai
para a RAM em `0xD1B4..`, e a rotina que os lê (`0x01E23168`) trabalha a 16 kHz com quadros de 512.
Ela está em uso.

**No build:**

- a `2b` exige o `tela.o` no `.livre` (no `.g729` até a v36), e só ele;
- a `2h` confere o registro e o lugar, e que nada de fora aponte para a faixa, no de fábrica e no
  novo, fora os sítios da tela B. Vale para valor de 32 bits e para `call`/`goto` de 4 bytes, estes
  pelo endereço da flash e pelo da RAM. Ela foi testada com a faixa 8 B mais larga, quando acusa
  as estruturas vizinhas e o MP3, e sem excluir o registro, quando acusa os 6 ponteiros.

**O MP3 também (v37).** O decodificador de MP3 começa logo depois do G.729, em `0x01E278C8`, e não
tem arquivo nenhum para tocar: no diretório de sons, o que não é `.sin` são as vozes `.wtg` e o
`index.idx`. Ele sai pelo mesmo caminho, e a faixa passa a ser `0x01E257E0..0x01E2EFC4`, 38.884 B,
com o código e as tabelas dos dois colados:

- o tipo dele no registro (`0x01E3487C`, o segundo de 68 B) também vira 0;
- o que aponta para dentro do MP3 vindo de fora da faixa nova é só o registro. A chamada de
  `0x01E2ECAA` e a tabela `0x01E2C084` ficam dentro da faixa;
- a única exceção é `0x01E05AD4`: os bytes `87 e8 e2 01`, lidos como 32 bits, caem na faixa, mas são
  a instrução `if (r0 != r7) goto -60`. O `conferir.py` só aceita a exceção com esses bytes exatos.

Na v39 a tela B usa 11.650 B dos 38.884.

## 17. Sem chinês (v37)

O rádio fica sempre em inglês, a pedido do dono:

- **o item LANGUAGE sai do menu** (o 22 de fábrica, get `0x01E3C9A8` e set `0x01E3C9B8`), junto com
  o ZONE, o "Zone Or Channel" e o VOICE;
- **o gancho da partida zera o idioma** (`b[0x14B74]`, que o get do LANGUAGE lê) logo depois de os
  ajustes virem do chip de dados. Um idioma chinês gravado por outro programa vira inglês antes da
  tela inicial, e o primeiro ajuste salvo (`0x01E3AB36`) grava o 0 no chip. Fora dali, só o set do
  LANGUAGE gravava o idioma, e o retorno aos ajustes de fábrica (`0x01E4842E`) o preserva;
- **as opções em chinês dos itens mudados** (o campo `+0x17` do ROGER e do SCRAMBLE) apontam as
  tabelas em inglês;
- **o modo pinyin do editor de nome some**: a troca de modo (`0x01E4A59C`) cicla 3 modos em inglês e
  4 em chinês.

O driver do CHIRP esconde o ajuste de idioma. O `conferir.py` confere na `4o` que o byte que o
gancho zera é o que o get do LANGUAGE lê e que o item saiu da tabela.

## 18. A dupla escuta e o TRIPLE (v42 a v51)

O firmware só tem dois lados, A e B. O WATCH escolhe quantas frequências a tela principal mostra
(SINGLE, DUAL ou TRIPLE), e o TRIPLE põe uma terceira, a C, sem mexer no motor da dupla escuta:
são ganchos em três chamadas e um registro próprio no chip de dados.

**O motor de fábrica** é o laço do sinal (`0x01E4622A`):

| Endereço | O que é |
|---|---|
| `b[0x14B70]` | o TDR: a dupla escuta ligada. O laço o lê na hora, e o set de fábrica do item (`0x01E3957C`) só grava o byte |
| `b[0x15367]` | o estado da recepção: 0 e 1 sintoniza, 2 espera, 3 vigia, 4 recebendo, 5 MO (o `tbh` de `0x01E467A6`) |
| `b[0x15378]` | 1 = a dupla escuta está fora do lado em evidência |
| `b[0x1537A]` | os tiques que faltam para trocar: 50 sem sinal num lado e ela vai ao próximo |
| `b[0x15E04]` | o lado em evidência (0 A, 1 B) |
| `0x01E48220` | a ida ao outro lado: copia a sombra do "outro" para a "corrente" (canal cru `0x15E6C` → `0x15E44`, 20 B; VFO cru `0x15EC8` → `0x15E80`, 36 B; modo `b[0x15E14]` → `b[0x15E12]`; canal `h[0x15E40]` → `h[0x15E3C]`; zona `b[0x15E17]` → `b[0x15E15]`), põe `b[0x15378] = 1` e decodifica (`0x01E3900A`, chamada em `0x01E4826E`) com o lado em evidência invertido em volta da chamada. A sintonia lê o que foi decodificado |
| `0x01E481C6` | a volta: `b[0x15378] = 0` e `0x01E3FD9E` (a sombra do lado em evidência → corrente, e decodifica), chamada em `0x01E481CC` |
| `0x01E3FDE4` | desarma a dupla escuta no lado em evidência. A transmissão a chama antes (`0x01E41022`), e por isso o TX sai sempre do lado em evidência |

Nada disso grava na flash, nem mexe no registro da tela (`0x14DC4` + lado × 31).

**O rodízio** (`gancho_volta_dw`, na chamada de `0x01E481CC`). No TRIPLE, voltando do outro lado,
a dupla escuta vai à terceira em vez de voltar ao lado em evidência: põe `b[0x15378]` de novo em
1, lê o canal cru da terceira da memória (20 B de canal × 32, como o carregador de canal de fábrica
`0x01E3A650` lê), marca canal no modo `b[0x15E12]` e no número `h[0x15E3C]`, e decodifica como a
ida. A decodificação grava a potência do lado invertido (`0x01E38F34`, com `b[0x153A1] = 165`), e o
gancho devolve a que estava. Na volta seguinte, a de fábrica restaura o lado em evidência. O ciclo
é A → B → C → A, cada um com os 50 tiques. A terceira é a C, ou o B quando a C está no lado B. A
ida (`gancho_troca_dw`, na chamada de `0x01E4826E`) só marca que não está na terceira e decodifica.

O estado do rodízio fica em `b[0x14BD0]` (2 = na terceira), o `0x9024` do bloco de ajustes, que no
RT-900 BT é o "Single Mode" e que este firmware não usa: fora as cópias do bloco (gravar ajustes,
padrão, partida e cabo), nada o lê nem o grava, conferido no desmontado inteiro. Gravar ajustes o
leva junto para o chip de dados, e um upload do CHIRP o sobrescreve: os dois só mudam quem vem
primeiro no rodízio.

**A tecla A/B** é o código 18 sem dígitos: o caso 18 do `tbh` das ações (`0x01E4BE28`) vai a
`0x01E4CC10` e a `0x01E4D44A`, que faz `r0 = 1` e chama a troca de lados de fábrica (`0x01E40E52`)
em `0x01E4D44C`. Depois da chamada o trecho só chama `0x01E3AC52`, que escreve o r0 antes de ler.
O `gancho_ab` entra no lugar:

- A → B: a troca de fábrica;
- B → C (TRIPLE, com o B num canal e a C num canal gravado): guarda o canal do B no registro e põe o
  da C no lado B pelo caminho das setas, `0x01E40B9E(0, 1)`. Ele copia o canal para `h[0x15E3E]`,
  limpa os dígitos (`0x01E3AC52`), carrega o canal (`0x01E3A650`: o registro do flash, a potência
  `b[0x15E30 + lado]`, a sombra do lado em evidência, a decodificação, o registro da tela e o
  silêncio) e grava o canal do lado e o log da flash em `0xE000` (`0x01E3A42A`). Ele **não
  redesenha**: quem o chama é que redesenha (a seta em `0x01E453E0`, a digitação do número por
  `0x01E41486`, o retorno à tela principal em `0x01E4BDBE` com `0x01E3C290`). O gancho chama
  `0x01E3C290`, que limpa e refaz a barra e a tela inteira (`0x01E3C132(0)`), como a troca de
  fábrica faz no fim (`0x01E40EF4`). Sem isso, na v44, a tela ficava no B até a próxima seta;
- C → A: guarda o canal em que a C ficou (as setas e a digitação a mudam como a qualquer lado),
  devolve o B ao lado B pelo mesmo caminho e faz a troca de fábrica, que redesenha.

Desde a v54 não há o item CH-C, e no TRIPLE a tecla sempre chega à C: com o canal dela vazio (apagado,
ou nunca escolhido), `c_alcancavel` a leva ao próximo canal que existe, pelo número do canal (o
próximo de fábrica, `0x01E3C4C0`, conta dentro da zona no modo zona), e sem nenhum canal a C vai em VFO.

A troca de lados de fábrica (`0x01E40E52`) desarma a dupla escuta, inverte `b[0x15E04]`, troca as
sombras dos dois lados, decodifica, refaz o registro da tela (`0x01E39128`) e a barra
(`0x01E3B05C`), grava os ajustes (`0x01E3AB36`, com r0 ≠ 0), redesenha (`0x01E3C290`) e silencia
(`0x01E39F20`).

**O registro do WATCH** fica num setor só dele, `0xD2000`, logo depois dos 16 bancos, com 14 bytes:

| Bytes | Campo |
|---|---|
| 0–3 | a marca `WTC1` |
| 4 | o modo: 0 DUAL, 1 SINGLE, 2 TRIPLE |
| 5 | o MDF-C: 0 nome, 1 frequência, 2 número |
| 6–7 | o canal da C (0 a 959) |
| 8 | 1 = a C está no lado B |
| 9 | desde a v49: bit 0 = a C em VFO; bit 1 = o B guardado em VFO (até a v48, reservado e 0) |
| 10–11 | o canal do B, guardado enquanto a C está no lado B |
| 12–13 | a soma dos bytes 0 a 11, a mesma dos bancos |

Sem registro válido (marca ou soma errada), o rádio fica em DUAL. A v43 tinha 10 bytes, sem os
campos 8 a 11, e o registro dela deixou de valer na v44. A tela B lê o registro a cada desenho.
Desde a v46 a janela de gravação do cabo o inclui, e o CHIRP grava o WATCH, o CH-C e o MDF-C. O
item CH-C saiu do menu na v54; o canal da C continua no registro, e o CHIRP ainda o grava.

**O VFO C (v49).** A C ganha modo (VFO ou canal) e VFO, como o A e o B. Os VFOs ficam no setor
`0xD3000` (`struct vfc`, "VFC1", 70 B: o da C, o do B guardado e a soma), no formato do registro
de VFO de cada lado (`0x14E42` o do A, `0x14E62` o do B, os 32 B do log `0x8000`). O estado de cada
lado, que a troca de fábrica mostra, é por "em evidência" e "outro", e não por A e B:

| O quê | corrente | em evidência | outro |
|---|---|---|---|
| modo (0 VFO, 1 canal) | `b[0x15E12]` | `b[0x15E13]` | `b[0x15E14]` |
| canal | `h[0x15E3C]` | `h[0x15E3E]` | `h[0x15E40]` |
| zona | `b[0x15E15]` | `b[0x15E16]` | `b[0x15E17]` |
| VFO cru, 36 B | `0x15E80` | `0x15EA4` | `0x15EC8` |
| canal cru, 20 B | `0x15E44` | `0x15E58` | `0x15E6C` |

O que é guardado é por A e B: o modo de cada lado num nibble de `b[0x14B86]` (o `0x901A` dos
ajustes, 1 = canal), o canal de cada lado em `h[0x15E0C + lado × 4]` e o registro de VFO.
`0x01E3A4FE(lado)` copia os 32 B do VFO corrente para o registro e, se mudou, grava o log.
`0x01E3A812` (a rotina da tecla VFO/MR, que só inverte o nibble e a chama) remonta os dois lados a
partir do que está guardado: a partida de cada lado (`0x01E3A486` o A, `0x01E3A796` o B) e
`0x01E3A700`, que em canal grava o canal do lado e o log (`0x01E3A42A`) e carrega pelo carregador
de fábrica (`0x01E3A650`, que em VFO copia os 32 B do registro). A tecla A/B do B para a C troca
o que está guardado no lado B pelo da C e chama essa remontagem; da C para o A faz o contrário e a
troca de fábrica. No rodízio, a C em VFO entra com os 32 B dela no VFO corrente e o modo 0.

**A regra do TDR**, do dono: no SINGLE a dupla escuta fica desligada, e só a frequência mostrada
recebe. O WATCH em SINGLE zera o TDR, o set do item TDR (o 7, `tdr_grava` no lugar de
`0x01E3957C`) o mantém em 0, e a partida o zera de novo. No DUAL e no TRIPLE, ligada escuta todas,
e desligada só a que está em evidência. Na barra de cima, a escuta ligada aparece como **DR** no DUAL e
**TR** no TRIPLE (até a v82, DW nos dois).

**Só o A no SINGLE (v78)**, a pedido do dono: o SINGLE mostra e escuta sempre o A, e a tecla A/B não leva
ao B nem à C. Até a v77 ela trocava os lados também no SINGLE, e a tela passava a mostrar o B.

- **A tecla A/B** (`gancho_ab`): no SINGLE, com o A em evidência, não faz nada; com o B ou a C (o WATCH
  posto em SINGLE fora do A), volta ao A pelos caminhos B → A e C → A de sempre.
- **O WATCH posto em SINGLE** com o B ou a C em evidência volta ao A quando o menu fecha. O set do item
  roda com o menu aberto, e nenhum chamador de fábrica da troca de lados (`0x01E40E52`: a tecla A/B, o
  PTT, o fim da TX e a TX recusada) roda assim. A saída do menu (`0x01E3C29C`), por onde passam os 11
  caminhos que o fecham (teclas, PTT, laço do sinal e partida), zera o modo de tela (`0x01E3C322..`) e
  chama a limpeza dos dígitos (`0x01E3AC52`) em `0x01E3C33A`, antes de redesenhar. Essa chamada passa pelo
  `gancho_menu_fecha`: ele limpa os dígitos e, no SINGLE fora do A, devolve o B ao lado B se a C estava
  nele e faz a troca de fábrica, que redesenha (a saída redesenha de novo).
- **A pilha.** A interface e o laço do sinal, que fecham o menu, já estão no limite de fábrica (740 e
  620 B). A troca chamada de dentro de uma rotina os passava em 36 B (o registro do WATCH na pilha) e,
  só com o `rets`, em 4. Por isso a decisão fica em C (`menu_fecha_decide`), e o gancho, em assembly,
  desempilha o `rets` e salta para a troca: o caminho fica o da troca de fábrica, e as três raízes
  ficam em 740, 620 e 696 B. O compilador não faz o salto de cauda numa chamada condicional.

Um rádio que já estava no SINGLE com o B em evidência liga no B: a primeira tecla A/B, ou o menu
aberto e fechado, o leva ao A.

**O TX-A/B/C (v50).** O TX-A/B de fábrica (item 44, `b[0x14B7F]`: 0 OFF, 1 A, 2 B) escolhe o lado do
PTT e só vale com a escuta ligada. A ida do PTT (`0x01E43E8A`) testa o TDR (`0x01E43F86`), lê o
byte (`0x01E43F92`) e, se o lado (valor − 1) não está em evidência, troca os lados
(`0x01E40E52(0)`, sem gravar) e marca `b[0x15384] = 1`, que no fim da TX (`0x01E473DC`) e na TX
recusada (`0x01E411A6`) faz a destroca; a partida zera a marca (`0x01E56B3A`). Um trecho anterior
da mesma rotina faz o mesmo com o lado do PTT apertado (`b[0x15383]`), e esse ganha. O get e o set
do item só copiam o byte, e nada mais o lê. Com o C (3), a conta de fábrica trocaria os lados a
cada PTT, então a leitura vira `call gancho_tx_abc` + nop:

| Com o TX-A/B/C em | No DUAL / SINGLE | No TRIPLE |
|---|---|---|
| OFF, A | como de fábrica | como de fábrica |
| B | como de fábrica | lado B, ou a terceira se a C está no lado B |
| C | nada (como OFF) | lado B se a C está nele, senão a terceira |

A terceira fica em evidência enquanto o PTT está apertado, como o A e o B, e nada é gravado (v51;
na v50 a TX saía nela sem ela aparecer). `gancho_tx_abc` põe o lado B em evidência com a troca de
fábrica (`0x01E40E52(0)`) se o A estava nela, marca `b[0x15384]` com o bit 7 (e o 5, se trocou; só se
a marca estava zerada) e devolve 0. O início da TX (`0x01E41022`) desarma a dupla escuta
(`0x01E3FDE4`, em `0x01E410EA`) e depois lê a frequência de TX decodificada (`b[0x15341]`, escrita
pela decodificação `0x01E38DE6`); ali `gancho_tx_desarma` põe a terceira na corrente (`le_terceira`,
a mesma leitura do rodízio) e a decodifica sem inverter o lado, como o carregador de canal faz com o
lado em evidência (`0x01E3A650`). A TX tira a potência de `b[0x15E30 + lado em evidência]`
(`0x01E475A2`, gravada em `b[0x1299C]` por `0x01E43FCA`), e a decodificação só a escreve num caso
especial (`b[0x153A1] = 165`); a de um canal ou VFO é o nibble baixo do byte 14 do canal cru
(`0x15E52`) ou do 16 do VFO (`0x15E90`), como os carregadores de fábrica a tiram (`0x01E3A6D0`,
`0x01E3A682`). O gancho põe essa potência no lado B, guarda a dele nos bits 0..3 da marca com o bit
6, refaz o registro de tela do B com os dados da terceira (`0x01E39128`: as frequências e os tons
da decodificação e o nome lido do chip pelo canal corrente) e redesenha. As vagas do lado B (o
nibble de `b[0x14B86]`, `h[0x15E10]` e a sombra) não mudam. Com os bits 7 e 6, `watch_tela()`
inverte o `c_no_b` e dá o canal da terceira para o cartão (bits 16..30, com o bit 31), e
`terceira()` passa a ser a que saiu do lado B, lida dessas vagas (o VFO, da sombra do lado em
evidência, `0x15EA4`). As duas destrocas (`0x01E411AE` e `0x01E473EE`) passam por
`gancho_tx_volta`: com o bit 7, zeram a marca, devolvem a potência, a sombra do B à corrente
(`0x01E3FD9E`) e o registro de tela do B, e voltam ao A com a troca de fábrica se o bit 5 diz que
foi ela que o tirou; sem o bit 7, a destroca de fábrica.

**Conferido no build** (`conferir.py`): a `4r` confere os três pontos de troca byte a byte
(`0x01E4826E`, `0x01E481CC` e `0x01E4D44C`: a chamada de fábrica antes, a do gancho depois; desde a
v49, a da luz, `0x01E46578`; e desde a v50 os do TX-A/B/C: `0x01E410EA`, `0x01E411AE`, `0x01E473EE`
e a leitura trocada por chamada e nop em `0x01E43F92`); a `4b` confere o ponteiro da tabela do menu;
a pilha com os ganchos é a de fábrica nas três raízes.

## 19. O ganho do microfone e o MIC GAIN (v49)

**De fábrica.** O ganho do microfone fica nos 6 bits de baixo do registrador `0x7D` do chip de
rádio, escrito pela rotina de registrador `0x01E39D82(reg, valor)`. A partida escreve `0xE912` nele
(`0x01E566A4`; 18 nos bits de baixo), mas isso só vale até a primeira transmissão: a cada TX
(`0x01E477D8`), o firmware lê o registrador, troca os 6 bits de baixo por `b[0x706C]` e o grava de
volta. O `0x706C` é uma cópia da calibração de fábrica: a partida lê 16 B de `0xF210` do chip de
dados para lá (`0x01E561BE`..`0x01E561CC`). No rádio de teste o `0xF210` valia **30**. Nem o menu
nem o CHIRP de fábrica mexem nele (o CHIRP baixa a calibração, `0xF000..0xF24F`, e não a grava).

**A escala.** O firmware escreve 6 bits, e a folha de dados do BK4819 dá a sensibilidade do
microfone nos bits 4 a 0 (0 a 31, em degraus de 0,5 dB). Os outros registradores que o firmware
usa batem com esse chip (`0x36` amplificador, `0x43` filtro, `0x47`/`0x48` áudio, `0x71` scramble).
Nenhum texto do firmware diz o nome, e até a v58 não havia prova direta; a da v59 é a tabela que a
partida escreve no `0x09`, a do BK4819 do UV-K5 ([seção 22](#22-a-pasta-mic-o-crystal-fine-e-o-menu-recebendo-v59)).
Por isso o MIC GAIN não passa de 31. No rádio de teste, pelo cabo, o `0xF210` foi a 18
e depois a 31, e a diferença ouvida (18 → 31: "mudou bastante") combina com essa escala.

**O item MIC GAIN** (v49, logo depois do NOISE REDUCTION, no molde do TDR): HIGH, MID e LOW, que
põem **31, 25 e 19** em `b[0x706C]`. O nível fica em `b[0x14BDF]`, o `0x9033` do bloco de ajustes
(gravado com eles por `0x01E3AB36` e servido pelo cabo da RAM, seção 4). Nada no firmware lê ou
grava esse byte fora das cópias do bloco, conferido no desmontado inteiro. A tabela de ajustes
padrão o deixa em 171, e qualquer valor acima de 2 vale HIGH (`mic_le`). Quem põe o ganho é o
`aplica_mic`, chamado:

- na partida, pelo `gancho_partida`, que roda depois da cópia da calibração (todo caminho até ele
  passa por `0x01E561CC`): o MIC GAIN substitui o `0xF210`, que fica intacto no chip;
- no set do item (`mic_grava`): vale na próxima transmissão, sem desligar;
- no retorno aos ajustes de fábrica (`0x01E4842E`), que grava os ajustes com uma chamada em
  `0x01E4845A`, e no `W` do bloco `0x9000` pelo cabo, que termina num `goto` para a mesma gravação
  em `0x01E43A50` (só o ramo do `0x9000` chega ali, `0x01E439F2`). As duas passam pelo
  `gancho_ajustes`, que põe o ganho do nível novo e chama a gravação de fábrica (`0x01E3AB36` não
  lê registrador de entrada). Um upload do CHIRP vale na hora.

**Conferido no build** (`conferir.py`): a `4r` confere a chamada de `0x01E4845A` e o `goto` de
`0x01E43A50` byte a byte, os de fábrica antes e os do gancho depois. O `gancho_ajustes` usa 296 B de
pilha no ponto mais fundo, abaixo dos 740 B de fábrica da interface.

## 20. A potência de TX (mapa, só leitura)

Mapeada a pedido do dono para saber se o H e o L ficam dentro dos limites; nada disso é mudado pelo
firmware modificado.

- **O registrador.** `0x01E44526(bias)` escreve o `0x36` do chip de rádio: com bias 0 (o fim da TX,
  `0x01E44810`) põe 127; senão, `(bias << 8) | ganho`, com o ganho por nível na tabela
  `0x01E5BE4A` = {`0xFF` H, `0xD7` M, `0xE8` L}, indexada por `b[0x1299C] % 3` (0 H, 1 M, 2 L).
- **O bias** vem de tabelas de 16 B por faixa e nível, escolhidas na TX (`0x01E47800`..`0x01E478BA`)
  pela frequência, com ponteiros a partir de `0x01E5FB60`:

  | Faixa | Índice | Tabelas H, M, L na RAM |
  |---|---|---|
  | abaixo de 200 MHz | (MHz − 130) / 3, até 15 (abaixo de 130, 0) | `0x710C`, `0x711C`, `0x712C` |
  | 200 a 299 MHz | (MHz − 200) / 5, até 12 | `0x70DC`, `0x70EC`, `0x70FC` |
  | 300 a 399 MHz | abaixo de 350, 0; senão (MHz − 350) / 5 | `0x707C`, `0x708C`, `0x709C` |
  | 400 MHz ou mais | (MHz − 400) / 10, até 12 | `0x70AC`, `0x70BC`, `0x70CC` |

- **As tabelas são a calibração de fábrica**: a partida (`0x01E5609A` em diante) copia as linhas de
  16 B de `0xF000..0xF0BF` do chip de dados para a RAM (`0xF000` → `0x70AC`, `0xF010` → `0x710C`,
  `0xF020` → `0x70DC`, `0xF030` → `0x707C`, `0xF040` → `0x70BC`, `0xF050` → `0x711C`, `0xF060` →
  `0x70EC`, `0xF070` → `0x708C`, `0xF080` → `0x70CC`, `0xF090` → `0x712C`, `0xF0A0` → `0x70FC`,
  `0xF0B0` → `0x709C`).
- **No rádio de teste** (calibração lida pelo cabo): H entre 218 e 250, M entre 186 e 201, L entre
  158 e 166, num bias de 0 a 255 (255 só nos degraus que o índice nunca alcança). O firmware não
  sabe quantos watts isso dá: a saída em W depende do amplificador e só um wattímetro mede.

## 21. O menu em pastas (v53 a v57)

O motor de menus (`0x01E489E0`) só conhece a tabela corrente, e o estado dele fica em `0x14BF4`:

| Endereço | O que é |
|---|---|
| `[0x14BF4]` | o ponteiro da tabela corrente |
| `b[0x14BF8]` | quantos itens ela tem |
| `b[0x14BF9]` | 1 = editando o item |
| `b[0x14BFA]`, `h[0x14BFC]` | o item; as setas escrevem os dois (`0x01E49F78`) |
| `h[0x14BFE]` | o número digitado (`0x01E4A2CE`) |
| `b[0x14C00]` | o tipo do item (o nibble baixo das flags) |
| `b[0x14C0C]` | o tempo do número: 15 a cada dígito, e o tique o desce (`0x01E4A326`) |

A entrada do menu (`0x01E4CBC4..0x01E4CBEC`), os menus rápidos (`0x01E4B3DC..0x01E4B402`) e as setas
(`0x01E49FA6..0x01E49FCC`) escrevem esses campos, e mudar de nível é escrevê-los do mesmo jeito:
recarregar o tempo de saída do menu (`0x01E484F8`), pôr a tabela, a conta, o item e o tipo, avisar o
item novo (`0x01E4851E`, que com o get nulo zera o valor e o cursor) e chamar o motor, que redesenha
(`menu_vai`, no `tela.c`).

**A tabela** (`menu_tabela`, 80 vagas de 28 B) traz primeiro os 13 grupos e depois os itens de cada
um, na ordem dos grupos, tudo em ordem alfabética: 68 vagas usadas. Cada grupo é um item de lista sem
get nem set (como o RESET), com a quantidade de itens no máximo e os nomes deles nas opções; o motor
monta as linhas com eles, e a tela B não as mostra nesse nível. O nível sai do ponteiro: a tabela dos
grupos é o nível 0; a do grupo g, o g + 1 (ela começa depois dos grupos e dos itens dos anteriores);
as duas do rádio, o `RADIO`; e qualquer outra (os menus rápidos) é `FORA`, que fica como de fábrica.

O `menu_textos` (512 B) leva o resto: no byte 0 a quantidade de grupos, no 1 a de posições da tabela,
a partir do 4 uma marca por posição (1 = sem valor no "L": RESET, MEMCH, DELCH e o CHANNEL dos BANKS;
2 = marcar vários: o BANK e o CHANNEL dos BANKS), e depois as listas de ponteiros e os nomes dos
grupos. O `montar.py` monta os dois a partir do `GRUPOS` e do `MENU_MARCAS`, e a `3b` do `conferir.py`
lê o que ficou gravado na imagem: os grupos e os itens em ordem alfabética, cada grupo uma lista sem
get nem set com os nomes dos itens dele, as marcas, e cada item de fábrica que fica e cada um dos
nossos uma vez só, dentro de um grupo.

**Os ganchos:**

| Onde | Como | Gancho | O que faz |
|---|---|---|---|
| `0x01E4CBB2`, o ponteiro da tabela na entrada | "r5 = menu_tabela" vira "call menu_abre; r5 = r0" | `menu_abre` | a tabela dos grupos; se o menu foi fechado dentro de um grupo, reabre nele (a entrada lê o item de `[+6]` logo depois, `0x01E4CBCA`) |
| `0x01E4CBE8`, a contagem | constante | — | 13, os grupos |
| `0x01E4C924`, a chamada do OK (`0x01E49996`) | chamada trocada | `gancho_menu_ok` | nos grupos, abre o grupo no primeiro item; no VERSION, abre só para ler; no resto, o OK de fábrica, que abre o item editando |
| `0x01E4B9FA`, a chamada dos números (`0x01E4A0FE`) | chamada trocada | `gancho_menu_digito` | fora da edição, vai ao item de mesmo número, contando de 1 (o de fábrica conta de 0, `0x01E4A2DA`); dois dígitos antes do tempo acabar fazem um número só, como no de fábrica (`0x01E4A274..0x01E4A2CE`); com o VERSION aberto, nada |
| `0x01E4D54A`, o EXIT fora da edição ("r0 = 349", que começa a saída) | desvio no meio | `desvio_menu_sai` → `b_menu_sai` | dentro de um grupo, volta aos grupos e segue para o bipe do EXIT (`0x01E4D59A`); nos grupos e no rádio, a saída de fábrica |
| `0x01E49A86`, o OK editando depois do set ("r0 = 3303", que fecha a edição) | desvio no meio | `desvio_menu_marca` → `b_menu_marca` | nos itens de marcar vários, zera o estado da recepção (`b[0x15367]`, como o de fábrica) e segue para o redesenho e a volta (`0x01E49C7C`) sem fechar a edição |

O desvio no meio (`DESVIO_MEIO`, no `tela.c`) é o desvio de sempre com mais uma saída: quando o
gancho resolve, ele não volta para depois dos bytes cobertos, e sim para um ponto adiante da rotina,
onde o caminho de fábrica também passa (o bipe do EXIT, o redesenho do OK), com a mesma pilha. A `4t`
confere os dois byte a byte.

**O VERSION** (tipo 8) o OK de fábrica não abre: `0x01E49A2E` só toca a voz, que não há mais. O
gancho liga a edição, que a tela B desenha sem cursor; o OK e os números não fazem nada com ele
aberto, e o EXIT de fábrica o fecha (cancela a edição, `0x01E4D500`).

**O menu do rádio** (v56) é o mesmo motor, no modo de menu: a entrada (`0x01E49000`) põe uma de duas
tabelas de fábrica, `0x01E61168` (FM e AM) ou `0x01E6137C` (SSB e CW, 532 B adiante, `0x01E49026`), e
a conta da banda vem de `0x01E3DE36` (FM 2, AM 5, SSB e CW 6). O item de memória tem `0x20` nas
flags: o título é montado por função (`0x01E3D20E`, o prefixo da banda e `MEMCH`), e a tela B o monta
igual. Os números contam de 1, como no principal; o OK e o EXIT ficam os de fábrica.

**A pilha.** A janela de pasta é desenhada pelo envio do título, no fundo do motor de menus. Com os
dois textos da janela em 28 B cada, a interface ia a 744 B (v56), 4 acima da de fábrica; eles ficaram
com 12 e 20 B, e as três raízes voltaram aos 740, 620 e 696 B de fábrica.

## 22. A pasta MIC, o CRYSTAL FINE e o menu recebendo (v59)

**O chip de rádio é um BK4819.** Os 16 valores que a partida escreve no registrador `0x09`
(`0x01E566C0..0x01E56766`, de `0x006F` a `0xF09F`) são, um a um, os da inicialização do BK4819 no
UV-K5, e o `0x43` da largura de banda (`0x01E4416E`: `0x4008` no largo, `0x3028` no estreito) bate
com o desenho dos bits dele. Toda escrita no chip passa por `0x01E39D82(reg, valor)`: são 154 pontos,
contando os `goto` de cauda (só os `call` dão 138). A leitura é `0x01E407C0(reg)`, que manda o
registrador com o bit 7 ligado.

**O compressor de TX** vem configurado e desligado. A partida põe `0xAA00` no `0x29` (`0x01E56602`:
razão 2:1 nos bits 15:14, ponto de 0 dB 84) e `0x0B40` no `0x28` (`0x01E565F8`: o expansor do RX, com
a razão 0, desligado), mas nada liga o bit 3 do `0x31`, que liga os dois. As escritas do `0x31` leem,
mudam o bit 0 (o AM, e não o NR, como dizia até a v78: `0x01E44078`, `0x01E440A2`, `0x01E4B0B8`,
`0x01E4B0DE`, [seção 25](#25-a-pasta-audio-o-expander-e-a-noise-gate-e-o-que-é-a-nr-v79)) ou o 1 (o scramble:
`0x01E4090E`, `0x01E4092A`, `0x01E4094A`) e gravam; o espectro guarda e devolve o `0x31` inteiro, com
mais dez (`0x01E3FEE4`, a lista de `0x01E5FF7B`: `0x13`, `0x30`, `0x31`, `0x37`, `0x3D`, `0x40`, `0x43`,
`0x47`, `0x48`, `0x7D`, `0x7E`). O bit posto por nós fica. **O AGC do microfone** é o bit 15 do `0x19`
(1 desliga), que só a partida escreve, com `0x1041` (`0x01E565EE`): ligado. Os registradores só voltam
ao padrão no reset da partida (`0x00`, `0x01E5653C`), na carga `0x01E56084`, que roda uma vez
(`0x01E56CB6`), antes do gancho da partida.

**A pasta MIC** junta COMPRESSOR (OFF, LOW, MID e HIGH: 1,33:1, 2:1 e 4:1 nos bits 15:14 do `0x29`, o
resto como a partida deixa; OFF devolve o `0xAA00` e desliga o bit 3), MIC AGC (ON e OFF) e o MIC
GAIN, que sai do TX. Os níveis ficam no `0x9035` e no `0x9036` dos ajustes (`b[0x14BE1]` e
`b[0x14BE2]`), que o firmware não usa: nenhuma instrução os lê nem grava fora das cópias do bloco,
conferido no desmontado inteiro (constante, base `0x14680` com deslocamento, meia palavra e palavra).
A gravação dos ajustes (`0x01E3AB36`) leva 39 B a partir de `0x14BCC`, então eles vão para o chip de
dados com os outros, e o cabo serve e recebe os 64 B do bloco da RAM ([seção 4](#4-protocolo-do-cabo)).
O padrão de fábrica (`0x01E4842E`) copia 39 B de `0x01E603C3`, mas a tabela acaba antes e segue com
o texto em chinês que vem depois: ficam 197 e 181, que valem OFF e ON, os de fábrica. O `aplica_mic`
põe os três na partida (no `gancho_partida`, depois do reset do chip), no set de cada item e quando
os ajustes mudam por fora do menu (`gancho_ajustes`: o upload do CHIRP e o RESET).

**O cristal.** Toda sintonia passa por `0x01E440CE(freq)`, com a frequência em 10 Hz: ela soma a
correção da calibração e grava o `0x38` e o `0x39`. Só o espectro escreve os dois direto
(`0x01E4AF1E`). A correção vem dos bytes 10 a 13 do bloco `0xF210`, que a partida copia para
`0x7076..0x7079` (`0x01E561BE`):

| Byte | RAM | O que é |
|---|---|---|
| `0xF21A` | `b[0x7076]` | índice grosso, de 0 a 16, na tabela `0x01E602E0` = {40, 35, …, 5, 0, 5, …, 40} décimos de ppm, negativos até o 8; 17 ou mais vale 8, o zero (e é regravado como 8) |
| `0xF21B` | `b[0x7077]` | `0xA0` = usa o fino no lugar do índice |
| `0xF21C`, `0xF21D` | `0x7078` | o fino, 16 bits com sinal: soma `freq / 100 × valor / 435500`, ou seja, 10 Hz por unidade em 435,5 MHz, proporcional à frequência |

No rádio de teste (calibração lida pelo cabo em 26/10) o índice é 8 e a marca `0xFF`: nenhuma
correção de fábrica.

**O CRYSTAL FINE** (pasta CALIBRATION) soma um ajuste ao que a fábrica gravou: de −2000 a +2000 Hz em
435,5 MHz, de 10 em 10 (401 linhas, um número com exibição própria, como o BANK). +2000 Hz são
4,6 ppm, e em 520 MHz dão 2,4 kHz, dentro do canal de 12,5 kHz; a faixa grossa de fábrica é ±4 ppm.
Com o ajuste diferente de 0, o `aplica_cristal` põe a marca e, no fino, o ajuste mais o de fábrica (o
fino dela, ou o índice convertido: 0,5 ppm = 21,775 unidades, arredondado); com 0, os quatro bytes de
fábrica voltam como estavam, relidos do chip de dados. O ajuste fica no `0x9037` e no `0x9038`
(`b[0x14BE3]`, 16 bits com sinal), livres como os da pasta MIC; o padrão de fábrica deixa `0xC4C0`,
fora da faixa, que vale 0. Vale na próxima sintonia, e o OK do menu a pede: depois do set o firmware
zera o estado da recepção (`0x01E49A86`). Com o item aberto, os números digitados não fazem nada
(`gancho_menu_digito`): escolheriam a linha do número, de −2000 Hz em diante.

**A pasta de um item só.** No EXIT editando, depois de cancelar a edição, `r0 = 1400` (`0x01E4D53C`)
lê a quantidade de itens da tabela, e com 1 o menu fecha inteiro (`0x01E4D566`, para `0x01E3C29C`),
como num menu rápido de um item; com mais, segue ao bipe do EXIT (`0x01E4D59A`). O `desvio_menu_um`
entra ali: num grupo segue sempre ao bipe, e a lista do grupo fica, também no CALIBRATION. Nada
desvia para os bytes cobertos, e o r0 é escrito em `0x01E4D59A` antes de ser lido.

**O menu recebendo.** Com o menu aberto, ao receber, aparecia por cima da janela de pasta o bloco de
fábrica do lado em evidência: o selo dela (que só conhece A e B: a C saía B), o nome e o número do
canal. O laço do sinal chama a tela inteira de fábrica (`0x01E3C132`, por `0x01E44812`), e só três
rotinas pintam o bloco e o complemento (`0x01E3BD04` e `0x01E3C06E`): ela, a do lado em evidência
(`0x01E453E0`) e a de um lado (`0x01E413BE`). As três têm desvio nosso; o da última já não desenhava
no menu, e os outros devolviam o desenho à de fábrica fora da tela principal (`layout()` em 0), onde
ela pinta na geometria do A compacto, por cima do título e da primeira linha da janela. Desde a v59,
`b_tela`, `b_evidencia` e `b_digitando` não desenham nada no modo de tela 1, e `b_tela` zera o estado
de TX como a de fábrica (só com 0: com 1, a tela de um VFO, `0x01E3C152` pula `0x01E3C1D4`). A saída do
menu (`0x01E3C29C`) zera o modo de tela (`0x01E3C322`) antes de redesenhar (`0x01E3C33E`), então a tela
principal volta inteira. O menu não fecha ao receber: a passagem para "recebendo"
(`0x01E47D98..0x01E47E38`) não mexe no modo de tela.

No lugar, a barra de cima mostra quem recebe: `● RX`, o selo da letra (`LETRA()`, então a C sai C) e o
texto que o MDF daquele lado pede, cortado antes da tensão, no lugar dos ícones (VOX, DW, MO, NR, a
nota, o Bluetooth e o cadeado), que voltam quando o sinal some. Quem recebe é sempre o lado em
evidência: a entrada do menu desarma a dupla escuta (`0x01E4CBBC`, para `0x01E3FDE4`) e o laço do sinal
só a arma no modo de tela 0 (`0x01E47C10`: `b[0x15379]`, o modo e `b[0x14950]` têm de estar zerados).
O lado que chega ao medidor no menu é sempre o A (`0x01E452E8`), e por isso o indicador não o usa. A
dupla escuta parada no menu é escolha de fábrica e ficou: a ida dela ao outro lado troca o canal
"corrente", que é o que os itens do menu leem e gravam.

**Outras calibrações mapeadas (só leitura).** O `0xF0C0` e o `0xF0D0` são os limiares de abrir e de
fechar do squelch por nível (10 B cada, para `0x7052` e `0x705F`, lidos em `0x01E4762C` e
`0x01E476A6`); o `0xF0E0`, o `0xF0F0` e o `0xF110` são correções por faixa lidas no mesmo caminho
(`0x01E4765E`, `0x01E47AE4` e `0x01E4769A`), e o `0xF100` vai para `0x1496C`, sem leitor conferido. O
`0xF211` (`b[0x706D]`) é o nível do subtom, posto nos bits 6:0 do `0x51` (`0x01E441A6`, `0x01E443C8`); no
rádio de teste, 70.

**Conferido no build:** a `3b` aceita grupo de um item, e a `4t` confere o `desvio_menu_um` byte a
byte. A pilha é a de fábrica nas três raízes (740, 620 e 696 B), e os get, set e exibe novos, medidos
no ponto da chamada, ficam em até 320 B.

## 23. A luz da tela por PWM (v60, diagnóstico)

**O pino.** A luz é o PB7 (pino 23). `0x01E001BA(pino)` devolve a base da porta pela tabela de
`0x01E006D0` (`0x1E5000`, `0x1E5040`, `0x1E5080`, `0x1E50C0`: as `JL_PORTA..D` do SDK da JieLi), e a
rotina da luz (`0x01E394D6`) só liga e desliga o bit 7 do OUT da porta B e grava a marca `b[0x15386]`.
Ela é chamada ao acender pelo ABR (`0x01E3951C`, que a chama em `0x01E39552`), ao apagar no laço do
sinal (`0x01E46578`, onde entra o `gancho_luz` da v49) e duas vezes na partida, que acende o logo
(`0x01E55FB0`, `0x01E5607E`). Fora da partida, só ela mexe no pino: a iniciação da placa
(`0x01E5483E`) e o `main` do SDK (`0x01E53D2E`) configuram o pino antes do gancho da partida.

**O PWM.** Os temporizadores do BR23 ficam em `0x1E0400`, `0x1E0500`, `0x1E0600` e `0x1E0700` (CON,
CNT, PRD e PWM): o TIMER0 faz as esperas, o TIMER1 é o tique de 2 ms (`0x01E3E44C`), e o TIMER2 não
aparece no app.bin. O PB7 não é pino fixo de PWM (os do SDK são PA05, PA12, PB03 e PB05), e o caminho
é o canal de saída 2, que leva o `CH2_T2_PWM_OUT` (46, `0x2E`). O firmware tem a rotina que liga um
canal, o `gpio_output_channle` do SDK em `0x01E4370A(pino, fonte)`: grupo = fonte >> 4, a fonte & 0xF
nos bits 11:8 do CON1 do IOMAP (`0x1E511C`) no CH0 ou nos bits 23:20 e 27:24 do CON3 (`0x1E5124`) no
CH1 e no CH2, e o pino com PU = PD = 1 e {OUT, DIE} = {0, 0}, {0, 1} ou {1, 0}. O `luz_inicia`, no fim
do gancho da partida, configura o TIMER2 como o `timer_pwm_init` do SDK (CON = `0x18`: o cristal de
24 MHz e /4; PRD = 240, 25 kHz, fora do áudio; o PWM = 60, 25%, com a luz acesa; CNT = 0; CON |= 1) e
liga o canal (23, `0x2E`). Sem o bit 8 do CON, que leva o PWM ao pino fixo do TIMER2 (o PB3, que não
é o da luz): o SDK também não o liga no caminho pelo canal.

**A rotina da luz trocada.** Com o pino no canal, o OUT escolhe o canal, e a rotina de fábrica, ao
apagar, passaria o pino para o CH0. O `desvio_luz` entra nela (8 bytes: `[--sp] = rets; r1 = 83584`,
nada desvia para dentro deles): com o TIMER2 contando, grava a marca como ela e põe o ciclo em 60 ou em
0; antes disso (a luz do logo), deixa a de fábrica rodar.

**O CH2 e as seriais.** A rotina do canal só tem dois chamadores. O cabo, na partida (`0x01E55AF4`, a
configuração de `0x01E5FCB6` = {62, 61, 0, 17}), é a UART1 no CH1. A abertura genérica (`0x01E6A974`,
pelo `0x01E4381A`, na troca para o modo de atualização em `0x01E4C096` e em `0x01E54A92`) pega a
primeira serial livre (o bit 0 do CON0 de `0x1E2000`, `0x1E2100` e `0x1E2200`, `0x01E6A94C`): a UART0
no CH0, a UART1 no CH1 e a UART2 no CH2. A UART0 pode vir ligada do carregador (o `main` a testa em
`0x01E53E5C` para as mensagens), então o CH2 só seria tomado com o cabo e ela ocupados. O
`luz_inicia` não liga o PWM com a UART2 ligada, para não tirar o canal de uma serial; se ela for
ligada depois, o pino só repete o TX dela enquanto durar.

**O que só o rádio responde:** se a luz escurece sem piscar a 25 kHz, se há zumbido no RX ou no TX, e
o comportamento no ABR, no SAVE e na volta do modo de gravação. O pior caso é a luz ficar cheia (o
pino fora do canal volta a ser saída com OUT = 1) ou apagada até gravar outra versão; nada disso toca
a partida antes do gancho dela nem o modo de gravação.

**Conferido no build:** a `4m` confere o `desvio_luz` byte a byte com os outros 22, e a pilha é a de
fábrica nas três raízes (o desvio pesa 28 B onde a luz é chamada).

**O item BRIGHTNESS (v61).** O diagnóstico funcionou no rádio: a luz ficou mais fraca, sem piscar e sem
ruído no RX nem no TX. O nível fixo virou o item BRIGHTNESS da pasta DISPLAY: 100%, 60%, 35%, 20% e
10% (os ciclos 240, 144, 84, 48 e 24 de 240), no `0x9039` dos ajustes (`b[0x14BE5]`; fora da faixa,
como o RESET de fábrica o deixa, vale 100%). O `luz_ciclo` lê o nível; o `luz_inicia` e o `b_luz` o
usam, e o `luz_aplica` o põe na hora, no set do item e no `gancho_ajustes`, com a marca da luz como
está. O `0x9039` é livre pela mesma varredura dos bytes da v59, refeita por script e validada no byte
do NOISE REDUCTION ([seção 22](#22-a-pasta-mic-o-crystal-fine-e-o-menu-recebendo-v59)).

**Sem PWM a 100% e apagada (v62).** Com a v60, o dono viu ruído num Baofeng encostado (e às vezes no
próprio rádio), que sumia com a luz apagada: cada borda do PWM espalha um pouco de RF. O `luz_poe`
leva o pino ao canal só com a luz acesa abaixo de 100% (o canal liga uma vez, enquanto o PD do PB7
estiver em 0); a 100% e apagada, o pino sai do canal e volta a ser saída comum, com o PD em 0, o DIE
em 1 e o OUT, como a iniciação da placa o deixa (`0x01E54844..0x01E54854`), sem borda nenhuma. O TIMER2
fica contando sem pino. Com a UART2 ligada, o nível escurecido fica em 100%.

**A correção do indicador de recepção no menu (v62).** O indicador da v59
([seção 22](#22-a-pasta-mic-o-crystal-fine-e-o-menu-recebendo-v59)) contava com o medidor, mas no rádio
ele não aparecia: o laço do sinal só chama o medidor nos modos de tela 0 e 3 (`0x01E46216`, na rotina
do nível do sinal), e o fim da recepção não passa por ele. A rotina do nível (`0x01E46166`) é chamada
em qualquer modo: o laço tem um contador rápido (`b[0x14612]`, de 0 a 9 a cada volta) e um lento
(`b[0x14614]`, que anda a cada 10), e na volta em que o rápido dá 0 o lento escolhe a tarefa (o `tbh` de
`0x01E46A38`); o 3 e o 8 vão a ela (`0x01E46C7A`), ou seja, a cada 50 voltas. O `desvio_sinal` entra na
entrada dela (8 bytes, `[--sp] = {rets, r7-r4}; r5 = 83584`; nada desvia para dentro deles): fora do
menu, a de fábrica; no menu, no lugar dela (que ali só leria o RSSI e o nível guardado em `b[0x7031]`),
compara o estado da recepção (`b[0x15367]` = 4) com o que a barra mostra, guardado no mesmo
`b[0x7031]` (0xFE recebendo, 0xFF não), e redesenha a barra quando muda. O `le_estado` põe o RX no
menu pelo mesmo estado, então toda barra desenhada ali (a bateria, o cadeado) concorda. Nos modos 0 e
3 nenhum nível (0 a 4) é 0xFE, e a volta do menu redesenha o medidor como depois do 0xFF de fábrica.

**O fim da recepção no menu (v63).** Com a v62 o indicador aparecia e não sumia: no menu o estado da
recepção não sai do 4 quando a portadora cai (do estado 4, o código do laço não chega a nenhuma
escrita do estado; a volta para a espera acontece dentro de rotinas chamadas). O indicador passou a
exigir também o pedido do amplificador da recepção: `0x01E39E2C(n)` (o `PEDE_AMPLI`) guarda um byte por
pedido a partir de `0x129D2` (1 e 2 o primeiro, 3 e 4 o segundo, 5 e 6 o terceiro, ...: liga e solta,
pelo `tbb` de `0x01E39E3A`) e liga o amplificador com o OU deles. O da recepção é `b[0x129D4]`: a entrada
da recepção o liga (`0x01E47E1A`, fora do scan QT) antes de pôr o estado em 4, e o sinal que cai o solta
(`0x01E47A9E`), nos dois sem olhar o modo de tela. `RECEBENDO_MENU` = estado 4 e esse byte ligado,
usado pelo `b_sinal` e pelo `le_estado`.

**O indicador sem marca (v64).** O diagnóstico com `-DDIAG_RX` mostrou no rádio que a leitura do nível é
chamada perto de uma vez por segundo também depois da recepção, e que o estado e o pedido do amplificador
voltam na hora (recebendo `E4 A1`, depois `E3 A0`). O que falhava era a marca em `b[0x7031]`: o firmware a
zera por conta própria (`b[0x7031] = 255` em `0x01E45A70`, chamada pelo laço em `0x01E46F52`), e com ela
zerada o `b_sinal` achava a barra limpa. Agora ele redesenha a barra em toda chamada, no menu, sem marca; e o
`b[0x7031]` só é escrito pelo firmware.

**O teste das bordas (v66) e o LIGHT PWM (v67).** Para achar o que tira a interferência do PWM num
rádio encostado, a v66 trouxe dois itens de teste na pasta CALIBRATION. O **LIGHT DRIVE** trocava a força
de saída do pino: o bit do PB7 nos dois registradores depois do PD da porta B, `0x1E5058` e `0x1E505C`, o
HD0 e o HD do `JL_PORT` do SDK (os seis primeiros, OUT a PD, estão provados pelas rotinas de pino de
fábrica, `0x01E001D2` e vizinhas; estes dois não). No rádio, nenhuma combinação mudou nada na luz, e o
item saiu: ou a borda depende do transistor da luz, ou esses não são os registradores de força. O
**LIGHT PWM** troca o PRD do TIMER2, e com ele a frequência do PWM: a energia das bordas que cai num canal
de VHF/UHF é proporcional a ela. A 1 kHz o rádio encostado não ouviu interferência, e desde a v67 é o
padrão. A escolha fica no `0x903B` dos ajustes (`b[0x14BE7]`, livre pela mesma varredura da v59: de
`0x14BDF` a `0x14BEB` nenhum acesso de fábrica fora as cópias do bloco, conferida no `0x14BDE` do NOISE
REDUCTION), 0 = 1 kHz (PRD 6000), 1 = 5 kHz (1200), 2 = 25 kHz (240) e, desde a v68, 3 = 240 Hz (25000) e
4 = 580 Hz (10345), que o menu mostra em ordem de frequência; fora da faixa vale 240 Hz desde a v69 (1 kHz na v67 e na v68).
O dono ainda ouviu interferência a 1 kHz: nenhuma frequência a elimina (a energia que cai num canal só
diminui com ela), e as mais baixas eram para comparar. Em 240 Hz não houve interferência nem
cintilação, e desde a v69 é o padrão: o reset de fábrica deixa 189 no `0x903B` (o byte do texto que vem
depois da tabela `0x01E603C3`), fora da faixa, e os valores já gravados continuam os da v68. O ciclo de cada nível vai na mesma escala (`ciclo × PRD / 240`), e a partida, o set e os ajustes
pelo cabo a aplicam (`luz_aplica`).

## 24. Morse e APRS (v65)

Duas pastas novas, para mandar texto num padrão que qualquer rádio entende. O código está no
`firmware/src/mensagem.c`, inteiro na faixa dos decodificadores (`.livre`), sem `.data` nem `.bss`.

| Pasta | Item | O que faz |
|---|---|---|
| APRS | APRS CALLSIGN | o indicativo, até 6 letras (editor de texto) |
| APRS | APRS SSID | 0 (nenhum) a 15 |
| APRS | APRS TO | o destinatário da mensagem, até 9 (com o SSID depois do `-`); vazio = status |
| APRS | APRS MESSAGE | a mensagem, até 37 |
| APRS | APRS SEND | OK manda (ação, sem valor na pasta) |
| APRS | APRS PATH | WIDE1-1, WIDE1-1,WIDE2-1 ou NONE |
| APRS | APRS RX | OK escuta: a tela dos pacotes recebidos, qualquer tecla sai |
| MORSE | MORSE MESSAGE | a mensagem, até 37 |
| MORSE | MORSE SEND | OK manda |
| MORSE | MORSE SPEED | 5 a 40 palavras por minuto (padrão 15) |
| MORSE | MORSE TONE | 400 a 1200 Hz, de 50 em 50 (padrão 700) |

**Onde fica gravado.** Dois setores próprios, livres no levantamento pelo cabo (seção 3), no formato dos
outros (marca, soma e a trava de só gravar em setor apagado ou que já seja nosso, `grava_registro`):
`0xD4000` "MRS1" (48 B: tamanho, wpm, tom em dezenas de Hz, o texto) e `0xD5000` "APR1" (64 B: os
tamanhos, o SSID, o caminho, o número da mensagem, o indicativo, o destinatário e o texto). Zerado vale o
padrão.

### O editor de texto a 37 caracteres

O editor é o do CH-NAME. O mapa da RAM em volta do texto (`0x14A3A`):

| Endereço | O que é |
|---|---|
| `b[0x14A35]` / `b[0x14A36]` | tamanho / máximo (o máximo é o mesmo byte do modo de digitação da tela principal) |
| `b[0x14A37]` | **o cursor**, a posição de inserção (`0x01E49666`, `0x01E49F24`, `0x01E4D4BA`). O `b[0x12A74]` que a v38 chamava de cursor é a contagem do pinyin |
| `b[0x14A38]` / `b[0x14A39]` | modo (1 ABC, 2 123) / marca 0xAA (a primeira tecla troca o texto todo) |
| `0x14A3A..0x14A49` | os 16 bytes de fábrica do texto (a limpeza `0x01E3AC52` zera `0x14A34..0x14A49`) |
| `0x14A4A..0x14A5F` | os 11 registradores do chip que o espectro guarda ao entrar (`0x01E4CF9C`) e devolve ao sair (`0x01E3FF0E`, só no modo 20) |
| `0x14A60..0x14A77` | a estrutura do sistema de arquivos da flash do SDK (`0x01E12F1C` e vizinhas): **não se toca** |

Daí os 37: o texto e o zero do fim vão até `0x14A5F`. A cópia do espectro é segura: o editor só existe
no menu (modo 1), e o despacho das teclas por modo (`0x01E4B988`) manda tudo do modo 20 ao tratador do
espectro (`0x01E4BFBE`), que não abre o menu; o modo 1 só é gravado pela entrada do menu na tela
principal. O editor nunca fica aberto entre o espectro guardar e devolver.

Os remendos (`montar.py`):

- o limite fixo de 15 da inserção, `0x01E4A0F0` `02 fc 04 1c` (`if (r2 > 14)`), vira `02 fc 04 48`
  (`if (r2 > 36)`, a codificação conferida montando a instrução com o clang da JieLi). Esse caminho
  (`0x01E4A0CC`) só é chamado pelo editor (`0x01E4A4CA`); o CH-NAME e o NAME seguem limitados a 12 pelo
  máximo deles;
- as três cópias de fábrica do texto com `"%15s"` (`0x01E46CCE` e `0x01E476BE`, para `sp+21` no laço do
  sinal, com um `_` no 26º byte; `0x01E48FBA`, para o `TEXTO` de 28 B em `0x129B0`) chamam o `texto15`:
  o mesmo resultado até 15 letras, e no máximo 24. O tema B não mostra nenhuma delas;
- a tecla 0 do editor (o ponteiro em `0x01E60B60`, tabela das teclas no ABC) aponta para
  `" -/=+@()*#0"`: o espaço no primeiro toque e o `-` do SSID, que nenhuma tecla alcançava (as sequências
  `@$%&` e ` =+-_` são das teclas `:` e `;`, que o teclado não gera). A sequência de fábrica era
  `"()/\*# 0"`;
- o desenho do editor (`editor_cena`, `telas.c`) põe o que não cabe numa linha em duas, no médio ou no
  pequeno.

O get de cada texto preenche o editor (zera até o 38º byte, porque a inserção não escreve o zero do fim),
põe o cursor no fim e a marca 0xAA; o set copia `tamanho` letras e sai do editor como o CH-NAME
(`LIMPA(2)` e `0x01E3C7AA`). O OK no item chama o set sem comparar valor (`0x01E49A00`, depois do bipe
`0x01E38BD4(61, 6)`): por isso os SEND e o RX são listas de uma opção.

### A transmissão: a máquina de estados de fábrica

O firmware nunca transmite com o menu aberto, e a TX é uma máquina de estados no laço do sinal
(`0x01E4622A`, a cada 10 ms pelo `sys_timer` `0x01E4DEA8`):

| Variável | O que é |
|---|---|
| `b[0x1533C]` | 0 parado, 1 TX, 2 recusada (bipando até soltar o PTT) |
| `b[0x1534A]` | o passo: 0 liga (corta o áudio de RX, frequência, potência, reg 0x30 = 0xC1FE), 1 espera uma volta e arma o TOT, 2 TOT e PTT (solto vai ao 3), 3 chama o fim (`0x01E44604`), 4 cauda e desliga, 8 depois do TOT |
| `b[0x14950]` / `b[0x14924]` | alarme / bipe tocando (a ida do PTT não transmite com nenhum dos dois, `0x01E43F42`) |
| PA12, `[0x1E5004]` bit 12 | o PTT: 0 = apertado (`0x01E47986`) |

O início (`0x01E41022`) fecha o menu (`0x01E40768` → `0x01E3C29C`, que grava os ajustes), desarma a
dupla escuta, confere a faixa permitida e o canal ocupado. O fim (`0x01E44604`) toca o roger ainda
transmitindo, com o microfone cortado (reg 0x30 = 0xC3FA por `0x01E3FF3E(4)`), e volta à recepção.

O envio usa essa máquina inteira:

1. o SEND só arma: `b[0x14BE6] = 'M'` ou `'A'` (o byte `0x903A` dos ajustes, livre pela varredura da
   v59; o padrão de fábrica deixa 202 nele);
2. **gancho na chamada da ida do PTT** (`0x01E46540`, `call 0x01E43E8A`, em toda volta do laço): com o
   envio armado, sem bipe tocando (o do próprio OK), sem TX, sem alarme e com o PTT solto, põe o "no ar"
   (`'m'`/`'a'`) e chama o início da TX como a ida do PTT (`0x01E39EE4`, `0x01E3951C`, `0x01E41022`); a ida
   de fábrica vem por último, como salto. Com o PTT apertado ou a TX parada (recusada), o "no ar" é
   desfeito;
3. sem o PTT apertado, o passo 2 vai direto ao 3, e o **gancho na chamada do fim** (`0x01E46D8C`, a
   única que o passo 3 faz) toca o conteúdo no lugar do roger: o mesmo preparo e o mesmo desligar do roger
   BEEP (`0x01E43AFE(1)`, `0x01E3FF3E(4)`, `0x01E39DEA(1)`; `0x01E39E2C(10)`, `TOM(0, 0)`,
   `0x01E43AFE(0)`, `0x01E39DEA(0)`), o tom 2 desligado (reg 0x70 = 0xE000), e o fim de fábrica com o roger
   zerado em volta da chamada (o fim não grava os ajustes).

A partida e os ajustes que chegam pelo cabo ou pelo retorno aos de fábrica zeram o `b[0x14BE6]`: nada
transmite sozinho. Antes do conteúdo, o gancho espera as teclas soltarem (o OK que armou ainda pode estar
apertado).

**Watchdog.** De 4096 ms (a partida, `0x01E53BF4`: o índice 12 da tabela `0x01E5F1B4`), limpo pela
interface a cada volta (`0x01E4B41A`), pela `janela()`, pelas esperas longas e pelo próprio roger
(`0x01E44634`). Enquanto o conteúdo toca, a interface não roda: o nosso código chama `0x01E37E76` antes
de cada caractere e de cada elemento do Morse e em volta do pacote do APRS.

**Parar no meio.** O PTT ou qualquer tecla: a varredura `0x01E43D50()` (255 = nenhuma) seguida de
`0x01E43CF4(60)`, como o laço faz. Depois de uma tecla, espera soltar: o laço não rodou enquanto ela
descia e, ao voltar, a veria como tecla nova (o EXIT viraria o A/B na tela principal).

### Morse

Pelo gerador de tom, como o roger: `TOM(f, 0)` liga e `TOM(0, 0)` desliga, em FM (MCW). A unidade é
1200/wpm ms (a palavra PARIS): ponto 1, traço 3, 1 entre elementos, 3 entre letras e 7 entre palavras, pela
`ESPERA_2MS`. A tabela tem os 53 caracteres da ITU de `!` a `Z` (um byte cada: uma marca e um bit por
elemento), gerada e conferida na volta por script; os sem código (`# % * < >`) viram espaço. O tom de um
valor que só se sabe na hora usa a conta inteira Hz × 330382 / 32000 (= × 10,32444, a do roger).

### APRS: o pacote e o AFSK

**O pacote** é o AX.25 de UI do APRS: destino `APZBJ8` (APZ = software experimental), a origem com o SSID,
o caminho, controle `0x03`, PID `0xF0`, o campo de informação e o FCS (CRC-16 do X.25: polinômio 0x8408
refletido, começa em 0xFFFF, sai invertido, byte baixo primeiro). O campo é a mensagem
`:DESTINO  :texto{id` (o destinatário em 9 colunas, o número de 1 a 99 para o outro lado confirmar) ou,
sem destinatário, o status `>texto`.

**O AFSK.** O modem FSK do chip só faz pares fixos (FFSK 1200/1800 e 1200/2400; o reg 0x72 é a taxa de
bits; no UV-K5 mediram 1200/1800 no SDR), e o APRS é o Bell 202, **1200/2200 Hz**. Então o AFSK sai pelo
gerador de tom: a cada bit 0 a frequência do tom 1 (reg 0x71) troca entre 1200 e 2200 Hz (NRZI), com bit
stuffing depois de cinco 1s, 40 flags de abertura (270 ms) e 3 de fecho. Antes das flags, desde a v69,
vão 0,5 s de portadora sem modulação, para o receptor (economia de bateria, squelch lento) abrir antes
do pacote. O tempo de cada bit (833,3 µs) sai
do relógio: `b[0x14810]` conta milissegundos, de 2 em 2 no tique do TIMER1, e o contador do TIMER1
(`0x1E0504`, de 0 ao PRD em `0x1E0508`) é a fração do tique, a conta de `0x01E4F888`. O alvo anda 833, 833
e 834 µs (2500 a cada 3 bits, sem acumular erro) e dá a volta nos 32 bits junto com o relógio.

**Conferido no PC** (`tests/teste_mensagem.py`, com o zig): o `mensagem.c` compila com o chip e o relógio
imitados (o relógio atravessa a volta dos 32 bits no meio do pacote); o quadro bate com um montador
independente; as trocas de tom viram áudio de fase contínua, e um demodulador independente (correlação em
1200 e 2200 Hz, relógio pelas transições, NRZI, destuffing, FCS) acha o quadro certo, com cada troca a
menos de 10 µs da grade de 833,3 µs; o decodificador HDLC acha só os quadros bons nas duas polaridades,
com ruído entre eles, um com um bit errado e um abortado; e o Morse volta igual a 12 e 20 wpm. O áudio
fica em `firmware/build/aprs.wav`.

### APRS: a recepção

Não achamos o áudio recebido num ADC do processador (o ADC lento em `0x1E3100` só lê a bateria), então a
recepção usa o modem do chip em captura crua, como o roger TONE1200 de fábrica e o TA1JS no UV-K5:
reg 0x58 = 0x3FC3 (FFSK 1200/1800), 0x72 = 0x3065 (1200 bps), 0x70 = 0x00E0, sync de 4 bytes = quatro flags
em NRZI (0x5A = 0x5B = 0xFEFE), sem o CRC do chip (0x5C = 0xAA30), quadro de 2048 bytes (0x5D = 0xFFE0),
quase cheia com 4 palavras (0x5E = 0x0204), avisos de sync, quase cheia e fim (0x3F = 0x3002), e
0x59 = 0x4008 / 0x1008 para limpar e ligar. O laço olha o pedido de interrupção (reg 0x0C bit 0) a cada
2 ms, limpa e lê o 0x02, e a cada quase cheia lê 4 palavras do 0x5F (a FIFO de RX tem 8, 107 ms). Os bits
vão ao `hdlc_bit`, o byte baixo da palavra primeiro e o bit alto de cada byte primeiro (a ordem do TX do
chip que o TA1JS usou com o aprs.fi decodificando). No sync, o decodificador começa já "dentro" de uma
flag, com o nível de linha da polaridade que o chip viu (reg 0x0B bit 7). Um quadro com o FCS certo, ou
2,5 s sem nada depois do sync, rearma a captura.

O laço é do set do item, com o menu aberto: o laço do sinal não roda, então a dupla escuta é desarmada
antes (`0x01E3FDE4`, que volta ao lado em evidência), o watchdog é limpo aqui, os registradores 0x3F, 0x70 e
0x72 são lidos antes e devolvidos no fim, e qualquer tecla sai. A tela (`tela_aprs`, `telas.c`) mostra os
2 quadros mais novos em duas linhas cada, a mensagem para o meu indicativo em âmbar (sem o `{id`), e
quantos chegaram.

**O que só o rádio prova:** se o demodulador FFSK 1200/1800 do chip separa o espaço de 2200 Hz da marca
(no UV-K5 há relato dos dois lados: o TA1JS decodificou um TH-D75; outro projeto não achou nenhum quadro
com FCS certo); se trocar o reg 0x71 no meio do tom é contínuo em fase; o desvio do AFSK com o ganho do
roger; e o tempo de uma escrita no chip.

### APRS: a recepção na v70

No rádio, o modo FFSK 1200/1800 decodificou um beacon real (o digipeater PY2ARS-14, da rede APRS-SP),
mas pouco do tráfego do canal. A v70 ataca as causas do nosso lado:

- **Quadro de qualquer tamanho.** Até a v69 o decodificador guardava o quadro inteiro para conferir o FCS
  e descartava o que passasse do buffer de 128 B: beacons de digipeater com comentário e quadros já
  repetidos passam disso. Agora o FCS é calculado byte a byte enquanto o quadro chega, e só os primeiros
  128 bytes ficam guardados, para a tela.
- **Correção de erro, como o FIX_BITS do Direwolf.** Quadro guardado inteiro com o FCS errado passa por
  uma busca de um bit errado ou de dois vizinhos (um tom trocado no ar vira dois bits errados seguidos
  depois do NRZI), pela linearidade do CRC: o registro que sobrou difere do certo (`0xF0B8`) pelo efeito
  do erro, e o de um bit na posição p é o passo do CRC aplicado (total − p) vezes a um 1, uma conta por
  posição, sem refazer o CRC. O conserto só vale se o quadro tiver forma de APRS (endereços de letras e
  números, de 2 a 10, controle 0x03, PID 0xF0): com ~2000 tentativas, uma em 30 acertaria o CRC por acaso.
- **Contadores na tela:** os syncs que o modem achou, os quadros com o FCS errado (BAD) e os consertados
  (FIX), para ver onde o tráfego se perde.
- **Quatro modos do modem na tecla #**, para comparar com o tráfego real: 1 o `0x3FC3`, 2 a banda de RX
  larga (`0x3FC9`, bits 3:1 = 100), 3 o FFSK 1200/2400 (`0x33C9`) e 4 o ganho de RX 0 (`0x3CC3`). A troca
  zera os contadores.
- **O receptor acordado** (`0x01E39EE4`) ao entrar: o laço não deixa a economia de bateria rodar.
- **A tela legível** (`aprs_texto`): a posição sem compressão (`!`, `=`, `/` e `@`, estes com a hora) em
  graus e minutos com o símbolo pelo nome (DIGI, GATE, CAR, HOME, WX…); a mensagem para outro como
  `ORIGEM>DESTINO: texto`, a para mim como `ORIGEM> texto`, e a confirmação como `ACK n`. O resto, como
  veio, até 48 caracteres, quebrados num espaço.
- **O histórico na flash**, no setor `0xD6000` ("HST1"): até 63 registros de 64 B (o texto e a cor), gravados
  sem apagar até o setor encher; cheio, ele é apagado e recomeça. Só é usado se estiver apagado ou já for
  nosso. As setas andam por ele, dois por vez.

Para a pilha, o laço chama a tela direto, sem rotina no meio, e a tela corta as linhas no próprio registro
(um zero por um instante no lugar da quebra) em vez de copiá-las: 732 B no ponto da chamada, de 740.

**O EXIT segurado no editor (v70).** No editor de texto, o EXIT apaga a última letra (`0x01E4D486`, só
alcançada pelo EXIT editando texto, de `0x01E4C7B4`). O gancho no lugar da primeira chamada dela (a recarga
do MENU EXIT TIME, `0x01E484F8`) espera enquanto o EXIT continua apertado: passando de ~0,6 s, põe o cursor
(`b[0x14A37]`) em 0, e o resto da rotina de fábrica, que com o cursor no começo sai da edição sem salvar
(`0x01E4D4F4`), faz o resto. Solto antes, apaga a letra, como sempre. Vale em todos os editores de texto. (No rádio não funcionou: o
EXIT chega a essa rotina já solto. Ver "O EXIT segurado, de verdade (v72)".)

### APRS: a polaridade do preâmbulo (v71)

**O que a v70 mostrou no rádio.** O modo 1 decodificou o beacon do digipeater e pacotes de um igate, sempre
das mesmas estações, e em ~95% dos pacotes do canal nenhum contador se mexia: nem SYNC, nem BAD. O modo 2
(banda larga) dava sync e nenhum quadro; o 3 (1200/2400 com banda larga) e o 4 (ganho 0) não ouviam nada.
Os WAVs tocados num Baofeng, com o alto-falante colado no microfone, contaram o porquê: com o tom de 2200 Hz
de −9 a +9 dB do de 1200 Hz, nenhum fez sync; com dois tipos alternados, o mesmo quadro com o preâmbulo
parado em 2200 Hz (tipo A) e em 1200 Hz (tipo B), o B fez sync todas as cinco vezes e o A nenhuma.

**A causa.** Numa flag em NRZI um tom fica 7 bits parado e o outro aparece 1 bit, e qual é qual depende do
estado do NRZI do transmissor ao começar. O chip só sincroniza no padrão do `0x5A`/`0x5B`: com `FE FE`, só
nas flags paradas na marca. O sync negativo, que o `0x0B` bit 7 indica no MDC1200 do OneOfEleven, não
aconteceu em nenhum quadro do tipo A.

**A v71:**

- **As duas polaridades.** Esperando, o laço troca a cada 50 ms entre `FE FE` e `01 01` (parado no espaço),
  com o sync de 2 bytes (`0x59` = `0x4000`/`0x1000`, bit 3 em 0) para caber folgado na janela. Sete bits de
  linha iguais só existem nas flags (o stuffing os corta dentro do quadro), então nenhum dos dois padrões
  aparece no meio de um. O nível de linha do começo vem da polaridade armada (`pol ^ 0x0B bit 7`).
- **A captura segue depois do sync.** O chip passa os bits crus do quadro dele inteiro, 2048 bytes
  (13,6 s), e o `hdlc_bit` acha sozinho as flags dos quadros seguintes, de qualquer polaridade. O laço não
  rearma mais depois de um quadro bom nem 2,5 s depois do sync: só no fim do quadro do chip (`0x02` bit
  13), ou se a FIFO parar de chegar por 300 ms. As 4 palavras de cada aviso saem todas, mesmo depois de um
  quadro achado, para a FIFO não encher.
- **Os contadores** separam os syncs das duas polaridades (`SYNC marca/espaço`).
- **Os modos**, cada um mudando uma coisa só do 1: 1 o `0x3FC3`; 2 o FFSK 1200/2400 com a banda do 1
  (`0x33C3`); 3 e 4 os mesmos sem o de-ênfase do RX, o bit 8 do `0x2B` (pela documentação do BK4819 da
  comunidade do UV-K5; a fábrica só grava 0 nele, em `0x01E440BE`, e a tela o devolve ao sair). O de-ênfase
  tira uns 5 dB do 2200 Hz em relação ao 1200 Hz.
- **Quadro de terceiros** (`}` no começo da info, o que um igate traz da internet): a tela mostra a estação
  de dentro e a info dela (posição, mensagem), em vez do cabeçalho cru.
- **Onde se ouve.** No alto, no lugar do "APRS RX", o lado em evidência como o MDF dele pede (nome,
  frequência ou número; no VFO, a frequência), lido da RAM como a tela principal (`0x14DC4`, `0x14B79`,
  `0x14B86`, `0x15E0C`), sem a cadeia do `le_estado`, que não caberia na pilha. Com a C no lado B (TRIPLE),
  o formato segue o MDF-B.
- **O EXIT segurado (~0,6 s) apaga o histórico** e fica na tela: o setor `0xD6000` é apagado e recebe a
  marca de novo (a mesma rotina do histórico cheio), só se o setor for nosso. O EXIT curto sai, como antes.

A pilha do laço caiu para 728 B no ponto da chamada, de 740.

### O EXIT segurado, de verdade (v72)

**O que a v71 mostrou.** A recepção passou a decodificar uns metade dos pacotes no modo 1, e o resto foi
quase todo para o BAD (o quadro chega inteiro, com bits errados). O modo 3 (sem o de-ênfase) ficou igual ao
1, e o 4 igual ao 2 (1200/2400, pior): o bit 8 do `0x2B` não muda nada que se veja. E o EXIT segurado no
editor (v70) não saía: segurando, nada acontecia, e ao soltar apagava a letra.

**A máquina de teclas de fábrica** (o laço do sinal, a cada 10 ms) lê a tecla em `0x01E46280`, só aceita a
mesma em três leituras seguidas, e conta as voltas dela em `b[0x14680]` (a anterior fica em `b[0x7030]`).
Fora da TX (`b[0x1533C]` diferente de 1), apertar não manda nada (`0x01E46372`, a tabela por tipo de
evento); com 100 voltas (1 s) manda o código longo da tecla (`0x01E6037B` + o código cru: 35 no EXIT) e marca
`b[0x146F4]`; o solto manda o toque curto (o código de `0x01E60369`, 18 no EXIT), a não ser que a marca do
longo esteja ligada. Ou seja: o EXIT do editor roda quando a tecla já foi solta, e o gancho da v70 na
rotina dele (`0x01E4D486`) nunca a via apertada. Ele saiu, e a chamada voltou a ser a de fábrica.

**A v72** troca a leitura das teclas do laço (`0x01E46280`, `call 0x01E43D50`) pelo `gancho_teclas`: com o
EXIT na 60ª volta (0,6 s) no editor de texto (`b[0x14C00]` = 7 e `b[0x14BF9]` = 1, como a tela B o
reconhece), põe o cursor (`b[0x14A37]`) em 0 e passa a devolver "nenhuma tecla". A fábrica vê o EXIT solto
depois de três leituras, manda o toque curto, e a rotina do EXIT do editor, com o cursor no começo, sai sem
salvar (`0x01E4D4F4`), com a tecla ainda apertada. Enquanto o EXIT continuar apertado a leitura segue
devolvendo "nenhuma", senão o solto de verdade viraria outro EXIT e subiria o menu. A marca disso fica no
byte `0x903C` dos ajustes (`b[0x14BE8]`): no desmontado de fábrica não há acesso a ele, nem pela base
`0x14680` (o deslocamento 1384 nunca é carregado), nem pelo endereço. Um valor que sobre nele se apaga na
primeira leitura sem o EXIT.

Na tela do APRS RX, que lê as teclas por conta própria, o EXIT segurado já apagava o histórico aos 0,6 s,
mas a tela só se redesenhava depois de soltar: agora o laço redesenha antes de esperar a tecla soltar, e
isso vale também para as setas e o `#`. A pilha do laço ficou em 740 B no ponto da chamada, de 740.

### O ganho do modem: CALIBRATION > MODEM GAIN (v73)

O ganho de entrada do modem FSK do chip são os bits 9:8 do `0x58`: 4 níveis, sem escala pública (a
documentação do UV-K5 só lhe dá o nome). A fábrica usa o 3 (o `0x3FC3` do roger TONE1200); o 0 (o modo 4 da
v70) não ouvia nada. Com o 2200 Hz fora do que o modem espera, um sinal forte demais pode saturar a entrada.

Na v73 ele vira um item da pasta CALIBRATION, **MODEM GAIN**: OFF, LOW, MEDIUM (MID desde a v82) e HIGH (0 a 3), gravado no
`0x903D` dos ajustes (`b[0x14BE9]`, livre pela varredura da v59) como `0xA0` + o ganho. Qualquer outro
valor vale HIGH, o de fábrica: é o caso do rádio que nunca mudou o item, e do RESET de fábrica, que põe no
byte o que vier depois da tabela `0x01E603C3` (hoje 198). O APRS RX entra com o ganho escolhido, e o `#` o
troca só ali, para comparar (no alto da tela, no lugar do M1 a M4 da v71, o nome do ganho); ao sair, a
próxima entrada volta ao da CALIBRATION.

O ganho só existe com o modem ligado, e só o APRS RX o usa na recepção: o único uso de fábrica do modem é o
roger TONE1200 (`0x01E44676..0x01E44790`), que só transmite, programa `0x58` (`0x3FC3`), `0x59`,
`0x5A..0x5D`, `0x3F`, `0x70` e `0x72` a cada vez, e no fim deixa o `0x58` em 0, o modem desligado. A saída do
APRS RX grava o mesmo 0 no `0x58` e, desde a v73, devolve também o `0x5E` (os limites da FIFO), que a
fábrica nunca grava e o RX mudava sem devolver. A recepção de voz não passa pelo modem.

Com o item, o laço do RX chegava a 744 B no ponto da chamada, de 740: o começo do quadro guardado para a
tela foi de 128 a 120 bytes (o FCS continua conferido no quadro inteiro, e a tela usa até 48 caracteres),
e a pilha ficou em 736 B.

### A v74: o texto inteiro, nenhum bit para trás e a auditoria

**Nenhum bit para trás.** Da v71 à v73, quando um quadro fechava certo, a leitura parava no meio do bloco
de 4 palavras da FIFO: até 63 bits depois dele iam fora. Um igate ou um digipeater que manda vários quadros
na mesma transmissão, separados por uma a três flags, perdia o começo do seguinte. Agora o quadro bom vira
texto e vai para o histórico ali mesmo (`guarda`, chamada no meio do laço dos 64 bits, antes de o seguinte
ocupar o `q`), e os bits seguem. A tela, que levava as 4 faixas de uma vez depois de cada quadro, vai uma
faixa de 32 linhas por volta do laço, entre as leituras do modem (`pede`): a FIFO nunca espera mais que uma
faixa.

**O texto inteiro.** O quadro em texto vai até 120 caracteres (`TEXTO_MAX`; eram 48, o que cabia em duas
linhas), e o histórico passou a registros de 128 B nos setores `0xD6000` e `0xD7000` (o mapa do chip de
dados dá os dois como livres), com a marca "HST2": ainda 63 registros. O histórico das v70 a v73 ("HST1",
registros de 64 B) é apagado na primeira entrada. A tela não guarda mais os textos na pilha: lê cada linha
da flash na hora de desenhar (`aprs_linhas`, de 25 em 25 bytes), e isso liberou a pilha para o `q` de 160 B.
Na lista, cada quadro tem duas linhas, e a segunda termina em `>` quando o texto continua; o **OK** abre o de
cima inteiro, em até 6 linhas, as setas seguem andando com ele aberto, e o OK ou o EXIT curto voltam à
lista. As letras de U+00C0 a U+00FF (UTF-8 `0xC3 xx`) aparecem sem o acento, e °, º e ª como o, o e a.

**A tela.** O quadro de cima, o da posição no alto, fica no fundo do selo do ganho, e os contadores e as
teclas, no fundo da barra de cima. O rodapé diz as teclas: `#:GAIN OK:OPEN HOLD EXIT:CLR` (aberto,
`OK:BACK`).

**A auditoria.** Uma revisão independente do código do APRS (o código e os fatos medidos, sem as
conclusões do autor; fuzz da conversão em texto e do decodificador, simulação de erros de tom e de ruído)
não achou erro crítico e confirmou o FCS, os endereços, os formatos, o tempo do AFSK, o HDLC e a memória.
Entraram na v74:

- **O microfone ia ao ar** nos 0,5 s de portadora muda antes do APRS (e enquanto o OK do SEND continuava
  apertado): o `AUDIO_TX(4)` (reg 0x30 = 0xC3FA) vinha depois das esperas. Agora vem logo no começo do
  `gancho_fim_tx`.
- **O preâmbulo do nosso TX parava em 2200 Hz**, a polaridade que o sync de um BK4819 não pega (o "tipo A"
  da v71): o AFSK começava na marca, e o primeiro bit 0 da flag já o trocava. Agora começa no espaço, e as
  flags param em 1200 Hz. Para o Direwolf e as TNCs tanto faz; um UV-K5 ou um BJ-8300 antigo passam a
  sincronizar.
- **A correção aceitava ~1% de quadros errados** (com 2 ou mais erros, a busca acerta o CRC por acaso, e o
  conferido era só o cabeçalho). A busca de um bit só saiu: o CRC-CCITT tem o fator (x+1), então a síndrome
  de um erro de peso par, que é todo tom trocado, tem paridade par, e a de um bit, ímpar; nunca casavam, e
  ela só aceitava falso positivo. O conserto agora também confere a info (de 0x1C em diante, CR e LF, como o
  sanity check do Direwolf) e é desfeito quando não passa. No teste, 40 quadros com dois tons trocados:
  nenhum aceito.
- **O BAD conta o que deve:** só o quadro com cabeçalho de APRS (`cabecalho_aprs`), alinhado ou não. Antes o
  ruído depois do pacote contava, e o quadro real que se desalinhava por um erro sumia sem contar. O quadro
  AX.25 de outro protocolo (controle diferente de UI ou PID diferente de 0xF0) não vai mais para a tela.
- **Na conversão em texto:** "ack" só é confirmação com o id de 1 a 5 caracteres e nada depois ("ackermann"
  não é); o símbolo só sai pelo nome na tabela primária (`/`); a letra do hemisfério é conferida; e
  caractere de controle ou acima de 0x7E no destinatário, no indicativo de terceiros ou no símbolo vira
  ponto.
- O buffer do rodapé foi de 16 a 18 B ("SYNC 65535/65535" e o 0), e o teste do PC apaga a pasta temporária
  (eram 1,4 MB a cada execução).

**A próxima fase** (achados da auditoria que mudam comportamento; feita na v75, abaixo):

- **ack automático** para a mensagem dirigida a mim (o remetente repete até receber) e o mesmo id ao
  reenviar a mesma mensagem (hoje cada SEND troca o id, e o outro lado a vê como nova);
- **histórico em anel**, um setor por vez, sem apagar os 63 de uma vez quando enche, e **sem as cópias** do
  mesmo pacote ouvido direto e pelos digipeaters (um resumo do texto nos bytes livres do registro);
- **ler o modem durante as esperas** de tecla (o redesenho inteiro depois de uma tecla, a espera do EXIT
  segurado, o soltar da tecla) e **apagar a flash fora do laço dos bits** (os dois setores do histórico
  cheio são apagados no meio de uma rajada);
- no envio, **desligar o scramble e o subtom** (o scramble usa o mesmo reg 0x71 do AFSK) e **recusar
  indicativo inválido** (com `-`, espaço ou `/`, que a tecla 0 dá);
- conferir de quem é o `0xD7000` antes de apagá-lo, e gravar o byte "usado" do registro por último (queda
  de energia no meio);
- passar o indicativo e o destinatário para maiúsculas ao gravar (o editor só tem ABC e 123, mas o RX
  compara letra a letra);
- testes no PC do laço do RX, do histórico e da tela, que hoje só rodam no rádio.

### A v75: a segunda fase da auditoria, o ack e as teclas

Tudo o que a auditoria deixou para a próxima fase entrou na v75, junto com pedidos do dono para a tela do RX.

**O ack automático.** A mensagem para mim com número (`{id`) arma o ack: o texto em `aprs_texto` devolve o
número (até 5, sem o `}` da resposta embutida, e achado mesmo quando o texto passa do limite da tela), o
registro do histórico guarda a origem em 9 colunas e o número, o `0x903E` dos ajustes (`b[0x14BEA]`, livre
pela varredura da v59) guarda qual registro é, e o estado do envio vai a `'K'`. O laço sai da tela sozinho,
porque o firmware nunca transmite com o menu aberto: a volta seguinte do laço do sinal (`gancho_ida_ptt`)
inicia a TX, o que fecha o menu, e o `gancho_fim_tx` manda `:ORIGEM   :ackNUMERO`, com o meu indicativo e o
caminho do menu, depois dos 0,5 s de portadora muda. Cada cópia da mensagem é confirmada, porque o ack
anterior pode não ter chegado. O SEND não troca mais o número: ele só anda quando o destinatário ou o texto
mudam (`aprs_guarda_texto`), e mandar de novo a mesma mensagem é repeti-la.

**O envio.** O indicativo e o destinatário são gravados em maiúsculas (o editor tem o modo abc) e só com
letras, números e, no destinatário, o `-` do SSID. Durante o conteúdo, o scramble (bit 1 do `0x31`) e o
subtom (bit 15 do `0x51`, que a fábrica liga com o nível em `0x01E441BE` e desliga com 0) ficam desligados,
e voltam antes do fim de fábrica: o scramble usa o mesmo `0x71` do tom.

**O histórico em anel** ("HST3"). Cada setor (`0xD6000` e `0xD7000`) tem o seu cabeçalho, a marca e uma
geração, um byte que anda a cada setor novo, e 31 registros de 128 B. Quando o setor que recebe enche, o
outro (o dos mais velhos) é apagado e recomeça com a geração seguinte: ficam sempre os últimos 31 a 62. O
setor que precisa ser apagado é apagado com a captura parada, fora dos bits (`hist_prepara`), e só por
último, se uma rajada não deixar, dentro da gravação. O registro guarda o texto (até 104 caracteres), a cor,
a origem e o número do ack e a hora da chegada (`TIQUE_MS`), e o byte "usado" é gravado por último: uma
queda de energia no meio deixa um lugar ocupado sem o "usado", que a tela pula. A cópia do mesmo texto da
mesma estação (com o mesmo número) chegada em até 30 s, a que vem direto e pelos digipeaters, não entra de
novo. O `0xD7000` só é usado apagado ou nosso; com outra coisa num dos dois, nada é gravado. O "HST2" da
v74 e o "HST1" das v70 a v73 são apagados na primeira entrada.

**O laço não para mais.** As teclas são lidas sem esperar soltar (a apertada só conta de novo depois de
solta), o EXIT segurado é medido no próprio laço (o toque curto vale ao soltar) e o redesenho depois de uma
tecla vai pelas faixas, entre as leituras do modem. A pilha do laço ficou em 736 B no ponto da chamada:
para caber, o buffer do quadro foi de 160 a 136 B, os quatro registradores devolvidos na saída cabem em
duas variáveis e a comparação das cópias lê 12 bytes por vez.

**A tela.** O `*` apaga a luz (`0x01E394D6(0)`, que o desvio da v60 atende pelo PWM) e o laço continua
recebendo; a tecla seguinte só a acende. A seta segurada repete (depois de 0,8 s, a cada 0,15 s), e o
histórico dá a volta nas duas pontas. O rodapé ganhou um fundo próprio, o dos segmentos apagados do
medidor (o da barra de cima saía quase preto no LCD), com o texto branco e o `*:OFF`.

**O teste do laço no PC** (`tests/teste_rx.py`). O `mensagem.c` compila inteiro com `TESTE_RX`, e o
`tests/teste_rx.h` imita o modem do BK4819 como está mapeado (só sincroniza no padrão do `0x5A`, passa os
bits crus por uma FIFO de 8 palavras com aviso em 4 e conta as que não couberam), a flash, as teclas, a
luz e a tela, que gasta 15 ms por faixa. Ele confere quadros separados por uma flag só (o defeito da v71 à
v73), as duas polaridades, as cópias, o ack e o quadro dele, o anel e a troca de setor, a conversão do
histórico velho, o setor alheio intocado, a FIFO sem transbordar e as teclas. O ruído antes de um pacote às
vezes dá um sync por acaso, que abre uma captura de 13,6 s: no rádio isso também acontece e não faz mal.

**O driver do CHIRP** mostra o MODEM GAIN (`0x903D`, fora do struct de ajustes da classe base, lido e
gravado como byte cru), depois do Light PWM.

### A v76: os formatos do APRS decodificados e a V3.0T

O `aprs_texto` passou a decodificar os formatos que chegavam ilegíveis, a pedido do dono, sempre em
unidades daqui (km/h, metros, graus Celsius, rumo em graus):

- **Mic-E** (`` ` `` e `'`), o dos rádios Kenwood e Yaesu e de muitos rastreadores: a latitude, os
  hemisférios, o deslocamento de 100° da longitude e a mensagem vêm nos 6 caracteres do destino; a
  longitude, a velocidade, o rumo e o símbolo, nos 8 bytes seguintes; depois, o tipo do rádio e a altitude
  `xxx}` em base 91. A mensagem padrão sai pelo nome (EN ROUTE, IN SERVICE, OFF DUTY, EMERGENCY...), e a
  própria (A-K) fica de fora. O Mic-E que chega por igate é decodificado pelo destino em texto do
  cabeçalho de dentro.
- **Posição comprimida** (`/YYYYXXXX$csT`, base 91): a latitude e a longitude, e o rumo e a velocidade do
  `cs` (o rumo de 4 em 4 graus, a velocidade 1,08^s − 1 nós). Com os bits 4:3 do `T` em 2, o `cs` é a
  altitude, que fica de fora.
- **Depois do símbolo** de uma posição comum: o rumo e a velocidade (`ddd/sss`), e o PHG do digipeater
  (`PHGphgd`: p² W, 10 × 2^h pés em metros, g dB).
- **No comentário:** a altitude `/A=pés` em metros, e o `!wXY!` da precisão extra (DAO) sai.
- **Tempo:** a estação com o símbolo `_` e o relatório sem posição (`_MMDDHHMM`): a temperatura, a umidade,
  a pressão e o vento.
- **Objeto** (`;`) e **item** (`)`): "OBJ NOME" ou "ITEM NOME" e a posição deles.

A conta da posição comprimida cabe em 32 bits: a parte inteira dos graus sai de `y / 380926` (ou
`x / 190463`), e o resto vezes 6000 é menor que 2^32. O `decimal`, que era recursivo, virou o `numero`, sem
recursão.

**APRS DECODE** (a pasta APRS, que passou a ter 8 itens): OFF mostra a info como veio, para conferir, e
vale para os quadros que chegam depois de mudar. No `0x903F` dos ajustes (`b[0x14BEB]`, livre pela
varredura da v59): OFF só com `0xB1`; o RESET de fábrica deixa 229, que vale ON.

**A pilha.** A decodificação, montada dentro do `guarda` em cima do registro de 128 B, levou o laço a
848 B, de 740. O registro em montagem passou para a memória de vídeo (`0x10190`, a faixa de 160×32 da tela),
que o firmware de fábrica já empresta como rascunho ao regravar canal: o `guarda` roda entre uma faixa e
outra, e a tela é redesenhada inteira depois de um quadro novo. Ficou em 720 B.

**A versão** que o item VERSION mostra passou a V3.0T (o texto `V0.12` em `0x01E5FD10`, no `montar.py`).

### Os modos do modem na tecla 0 (v77, experimental)

**O que a v75 mostrou.** Com os ganhos comparados no ar, o MEDIUM foi o melhor e os outros, piores; mesmo
nele, SYNC e BAD passam de 70% dos pacotes, e a maioria do que se ouve num outro rádio não vira quadro. O
limite é o do modem: na recepção o 0x58 só escolhe entre pares fixos, e nenhum registrador dá o tom (o
0x72 é a velocidade). Na transmissão o gerador de tom contorna isso (o 0x71 troca entre 1200 e 2200 Hz a
cada bit); na recepção não há atalho.

**Os campos do 0x58**, pelos comentários do driver do BK4819 no firmware do UV-K5 (o do OneOfEleven, no
`BK4819_enable_mdc1200_rx`): 15:13 o modo de TX (1 = FFSK 1200/1800, 3 = 1200/2400, 5 = NOAA SAME); 12:10 o
modo de RX (7 = FFSK 1200/1800, 4 = FFSK 1200/2400, 0 = FSK direto e NOAA SAME; 1-3 e 5-6 sem
documentação); 9:8 o ganho; 7:6 sem documentação; 5:4 o tipo de preâmbulo; 3:1 a banda de RX (1 = 1200/1800,
2 = NOAA SAME, 4 = FSK 2400 e 1200/2400; 3 e 5-7 sem documentação); 0 o modem ligado. Por eles, a "banda
larga" da v70 (`0x3FC9`) é a 4, a do 1200/2400, e o modo 3 da v70 (`0x33C9`) era o 1200/2400 com a banda
dele: o par casado, que não ouviu nada.

**A v77** põe na tecla 0 do APRS RX o que ainda não foi testado, cada um mudando uma coisa só do modo de
antes (o 1), com o ganho escolhido no `#`. Na barra de cima, ao lado do ganho, a etiqueta `M1` a `M10`; a
troca zera os contadores, e cada entrada começa no M1.

| Modo | 0x58 sem o ganho | O que muda do M1 |
|---|---|---|
| M1 | `0x3CC3` | nada: o de antes (RX 7, banda 1, bits 7:6 = 3) |
| M2 | `0x3C03` | os bits 7:6 em 0, como o receptor de MDC1200 do OneOfEleven |
| M3 | `0x3CC5` | a banda 2 (a do NOAA SAME) |
| M4 | `0x3CC7` | a banda 3 (sem documentação) |
| M5 | `0x20C5` | o caminho do NOAA SAME: RX 0 com a banda 2 |
| M6 a M10 | `0x24C3`, `0x28C3`, `0x2CC3`, `0x34C3`, `0x38C3` | o modo de RX 1, 2, 3, 5 e 6 (sem documentação) |

É um experimento: não há o que provar no desmontado (a fábrica só usa o `0x3FC3`, no roger TONE1200), e
o que cada modo faz só o ar mostra. Um valor sem documentação no 0x58 só mexe no modem; a saída grava 0
nele, como antes. O código 13 é a tecla 0: na tabela das teclas (`0x01E60369`) a última coluna da matriz
é `*` (9), 0 (13) e `#` (17), e o código longo do 13 é o 33, a ação do 0 segurado (a caixa de tensão). Até
a v76 ele saía da tela, como as outras teclas sem função.

**A pilha.** A conta do modo seguinte dentro do laço levava o quadro dele de 256 para 268 B (a pilha do
item a 732 B, de 740), mesmo com a escrita do 0x58 numa rotina à parte: é a alocação de registradores do
laço. A conta e a escrita juntas numa rotina que recebe a tecla (`modem_58`) devolvem os 720 B.

**Conferido no PC:** o `teste_rx.py` aperta o 0 onze vezes e o `#` uma, e confere cada valor gravado no
0x58 contra o registrador montado campo a campo.

### O resultado da v77 e as gravações do ar (v78)

**Os modos da v77**, testados no ar com o ganho em MEDIUM: nenhum decodificou mais que o M1, e eles saíram
na v78 (a tecla 0 volta a sair da tela, como as outras sem função). O registrador fica no `0x3CC3` de
antes, com o ganho do `#`.

**Quatro gravações** de pacotes do canal, feitas pelo dono com o celular perto do alto-falante (HE-AAC de
48 kb/s, de 0,9 a 1,2 s de sinal cada), passaram por um demodulador escrito para elas (filtro de 700 a
2800 Hz, correlação em 1200 e 2200 Hz numa janela de um bit, 13 compensações de torção de −12 a +12 dB,
relógio por PLL, NRZI, destuffing e FCS), conferido antes no WAV do nosso TX (`firmware/build/aprs.wav`):
ali ele acha o quadro e os indicativos. Nas quatro, nenhum quadro com o FCS certo; aparecem de 3 a 7
flags seguidas, e o que vem depois não forma indicativo. A potência entre 1000 e 2400 Hz fica só ~10 dB
acima da de 2800 a 3800 Hz (19 dB no WAV limpo), sem saturação: muito chiado. Pelo caminho acústico não
dá para separar o que já chega ruim pelo ar do que o alto-falante, o microfone e a compressão do celular
estragam.

**O próximo passo** é medir o sinal sem o caminho acústico: o áudio do rádio por cabo (a saída de fone)
numa entrada de áudio, ou um decodificador de APRS no celular ouvindo o alto-falante (o APRSdroid tem o
modo "AFSK via microfone"). Se um decodificador em software pega a maior parte do que se ouve, o limite é
o modem do chip, e vale procurar o áudio recebido numa entrada do JieLi para demodular em software; se
nem ele pega, os sinais chegam fracos, e nenhum firmware os recupera.

### A pilha

O conteúdo roda no slot do roger, no laço do sinal, e o laço do RX no set do item; os dois dentro do de
fábrica (620 e 740 B). O gancho da ida do PTT põe o "no ar" antes do início e chama o início e a ida de
fábrica como saltos (o quadro dele já desfeito), e o `pilha.py` passou a tratar o `goto` do nosso código
para outra rotina como no grafo de fábrica (salto de cauda: o quadro de quem salta não soma). A V2.9T dá
os mesmos números com o `pilha.py` novo.

## 25. A pasta AUDIO: o EXPANDER e a NOISE GATE, e o que é a NR (v79)

**O EXPANDER** é o expansor do áudio de RX do BK4819, o gêmeo do compressor da pasta MIC: abaixo do ponto
de 0 dB, cada 1 dB a menos vira 2, 3 ou 4 (LOW, MID e HIGH: 1:2, 1:3 e 1:4 nos bits 15:14 do `0x28`, pela
documentação da comunidade do UV-K5). Ele desfaz a compressão de quem transmite com compander e cala o
chiado nas pausas; com uma estação comum, a voz fraca fica mais fraca. Liga pelo mesmo bit 3 do `0x31` que
o compressor, e só com ele o compressor fica na razão 0 (desligado). Ligado, os bits 13:9 do `0x28` vão a
`10101`, os da receita do UV-K5 (`0x6B38`, o ponto de 0 dB em 86); desligado, ficam os de fábrica.

**A NOISE GATE** é a porta de ruído do áudio de RX: pela tabela oficial da Beken citada na issue 739 do
firmware do UV-K5 v3, o bit 8 do mesmo `0x28` a liga e os bits 7:0 são o nível; os 11:9 são o filtro de
DC (`101` no RX), e os 13:12 não têm descrição. A partida grava `0x0B40` (`0x01E565F2`): o expansor
desligado e a porta ligada no nível 64. DEFAULT é isso; OFF desliga o bit 8; LOW, MID e HIGH usam os
níveis 32, 76 e 88. O nível é o limiar: quanto maior, mais corta. Na v80 o HIGH, em 160, deixou o rádio
mudo, e na v81 o MID e o HIGH baixaram de 96 e 160 para 76 e 88 (na v79 eram só LOW e HIGH, 32 e 128).

**No rádio.** O padrão é o de hoje: sem mexer nos itens, o `0x28`, o `0x29` e o `0x31` ficam como a partida
de fábrica os deixa. Só a partida escreve no `0x28` e no `0x29` (`0x01E565F2`, `0x01E565FC`), e o
espectro não os guarda. O `aplica_audio` (o antigo `aplica_mic`) põe tudo na partida, no set dos itens e
nos ajustes que chegam pelo cabo. Os níveis ficam em dois bytes dos ajustes que nada de fábrica lê nem
grava (a varredura da v59): o EXPANDER no `0x9034` (`b[0x14BE0]`), como `0xC0` + o nível, e a NOISE GATE no
`0x9031` (`b[0x14BDD]`), como `0xE8` + a opção (0 OFF, 1 DEFAULT, 2 LOW, 3 MID, 4 HIGH; o `0xE0` da v79,
com outra ordem, vale DEFAULT). Sem a marca vale o de fábrica, OFF e DEFAULT: o RESET deixa
208 e 0. Os dois entram no grupo AUDIO, em ordem alfabética antes do NOISE REDUCTION, e a tabela do menu
passa a 92 de 96 vagas. A pilha não muda.

**No CHIRP**, "Expander" e "Noise gate" vêm logo depois da Noise reduction, como bytes crus com a marca (o
molde do Modem gain). A classe base chama o `0x9031` de noise reduction e o `0x9034` de FM interrupt, mas
nenhum dos dois vale no BJ-8300: a noise reduction dele é o `0x9032`, e o FM interrupt é só do RT-920. Um
`1` que a base tenha gravado no `0x9031` lê DEFAULT.

**O APRS.** O decodificador do chip provavelmente pega o áudio antes desses blocos (na v71, desligar o
de-ênfase não mudou nada), e a tela do APRS não mexe no `0x28`.

### O que é o bit 0 do `0x31`, e o que é a NR

**Até a v78 esta documentação chamava de NR o bit 0 do `0x31`. Ele é do AM.** As quatro escritas
(`0x01E44078`, `0x01E440A2`, `0x01E4B0B8`, `0x01E4B0DE`) são das duas rotinas que põem o receptor em AM ou
em FM: `0x01E4404E` (pela entrada `0x01E440C4`: r0 = 1 é AM) e a do espectro (`0x01E4B0A0`, logo depois da
troca de AM/FM dele, `0x01E4B04C`). Em AM: o bit 0 do `0x31` ligado, `0x42` = `0x6F5C`, `0x2A` = `0x7434` e
`0x2B` = `0x0500` (sem o passa-alta de 300 Hz e sem o de-ênfase do RX); em FM, o bit 0 desligado,
`0x6B5A`, `0x7400` e `0`. A partida grava `0x6600` no `0x2A`. O laço do sinal põe o AM ao começar a
receber (`0x01E47B74..0x01E47BC6`) na faixa aeronáutica (108 a 136 MHz) ou, de 18 a 1000 MHz, com o RX
MODULATION (`b[0x15E2F]`, o get `0x01E3D10C`) em 1, e põe o FM no resto e ao voltar a sintonizar
(`0x01E466B6`).

**A NOISE REDUCTION é a redução de ruído do microfone, no TX, e roda no JieLi.** O item grava o bit 0 de
`b[0x14BDE]` (o `0x9032`; o get `0x01E3D0E0`), que tem dois leitores: o ícone da barra (`0x01E3B154`) e a
ida do PTT (`0x01E478F8`, logo depois do ganho do microfone, `0x01E477D8`), que monta o caminho do
microfone com `0x01E42080(0, NR, 4, 17)`. Provado no desmontado:

- o microfone entra no ADC do JieLi (canal MIC, aberto em `0x01E33C2A` por `0x01E41CBA`) a 16 kHz, em
  blocos de 256 amostras de 16 bits a cada 16 ms, entregues pela interrupção de áudio (`0x01E32488` →
  `0x01E34180`) ao `0x01E41C06`;
- com a NR em 0, cada bloco vai direto ao DAC (`0x01E41BA0` → `0x01E321F8`); em 1, à tarefa `aec` do SDK
  (`0x01E24522`), cuja saída vai ao DAC (`0x01E41BE8`). Do DAC à entrada de microfone do BK4819 é a fiação
  (suposição);
- a supressão (`0x01E23168`, rodando em `0x01E240BE`) é por faixa de frequência, com FFT de 512 e passo
  de 256: o ganho de cada faixa sai da estimativa de ruído com o piso em 0,02 (−34 dB), a força em 1,8 e o
  ganho de saída em 2,5, constantes da inicialização (`0x01E245F8`). Os pesos de ~32 KB em
  `0x01E1C02C..` são dela;
- o item só passa 0 ou 1. Há uma tabela de oito níveis (`0x01E41D26`), mas o 1 é sempre o usado, e não se
  achou quem leia os dois campos dela que pareciam a força;
- o caminho é desmontado no fim do TX (`0x01E445AC`) e só é montado com a licença `b[0x146A0]`, conferida
  na partida contra o identificador do chip.

**O áudio recebido não passa pelo JieLi.** No firmware de fábrica, o ADC do JieLi só lê o microfone (no
TX) e a entrada de linha do rádio FM interno dele (`0x01E571E4` → `0x01E33EFC`); o fechamento do squelch
(`0x01E47E8E..`) só mexe no BK4819 e no amplificador. Um demodulador de APRS em software precisaria do
áudio do BK4819 numa entrada do JieLi, e se a placa o liga a alguma, o firmware nunca a lê.
### A NR com níveis, na pasta MIC (v80)

Como a NOISE REDUCTION é do microfone, ela passou à pasta MIC, com OFF, DEFAULT, LOW, MID e HIGH no mesmo
byte do item de fábrica (`b[0x14BDE]`, o `0x9032`; o item 48 com o get, o set e as opções nossos, como o
ROGER e o SCRAMBLE): 0 OFF, 1 DEFAULT, 2 LOW, 3 MID e 4 HIGH. DEFAULT é o ligado de fábrica. A ida do PTT
passa o byte à montagem do caminho do microfone, que o trata como "ligada" se não for 0 e como índice da
tabela de presets de `0x01E41D06`, que pula o que passa de 7 (`0x01E41D08`) e cujos campos nada lê. Acima
de 4 (o RESET deixa 200), o byte vale DEFAULT, como a fábrica o trata (ligada; o menu de fábrica, que só
olhava o bit 0, mostrava OFF).

Os níveis mexem nas duas constantes do núcleo que decidem o quanto cortar. A inicialização da supressão
(`0x01E41E4E`) grava no bloco dela `[attr+120]` = 0,02 (`0x3CA3D70A`, o piso do ganho de cada faixa, −34 dB),
`[attr+124]` = 1,8 (`0x3FE66666`, a força), `[attr+128]` = 2,5 e `[attr+132]` = 0,3, e chama `0x01E245F8`
com `r0` = `attr` em `0x01E42078`; dali o piso e a força vão ao núcleo (`0x01E23168`, chamado em
`0x01E248AE` com os dois em `r2:r3`). A chamada passa pelo `gancho_nr_inicia`, que em LOW, MID e HIGH troca
os dois e segue em salto de cauda, com `r0..r3` como vieram:

| Nível | Piso | Força |
|---|---|---|
| DEFAULT | 0,02 (−34 dB, o de fábrica) | 1,8 |
| LOW | 0,1 (−20 dB) | 1,2 |
| MID | 0,05 (−26 dB) | 1,5 |
| HIGH | 0,01 (−40 dB) | 2,5 |

LOW e MID cortam menos que o DEFAULT, e HIGH mais. A escolha dos valores é de ouvido: a fórmula do ganho
(`g = s² / (força + (1 − força)·s²)`, limitado pelo piso) mostra o sentido, não o resultado no ar.

**Na barra de cima**, a NR e a NOISE GATE ganham etiquetas: fora em OFF; no DEFAULT, na cor das outras;
em LOW, MID e HIGH, verde, amarela e vermelha. Os dois níveis vão no byte `nr` da `struct estado`
(bits 3:0 e 7:4), que está no caminho mais fundo da pilha e não podia crescer, e a etiqueta é uma rotina
separada: embutida duas vezes, levava o quadro da barra de 44 a 52 B. Sem lugar antes da tensão, ela não
entra. As raízes continuam em 740, 620 e 696 B.

**Os nomes das pastas MORSE e APRS (v83).** Os itens delas perderam o prefixo, que a pasta já diz: CALLSIGN,
DECODE, MESSAGE, PATH, RX, SEND, SSID e TO na APRS, e MESSAGE, SEND, SPEED e TONE na MORSE. MESSAGE e SEND
existem nas duas, e o `montar.py`, que achava cada item pelo texto que o rádio mostra e recusava dois iguais,
passou a achá-los pela chave interna (`nome='APRS MESSAGE'`), com o texto num campo `texto` que ele confere
contra o título do firmware (`rotulo`); a prévia desenha o texto, e o `conferir.py` confere as marcas e os
textos de cada posição pela chave.

**A ordem no menu (v82).** As opções da NOISE GATE e da NR vão em ordem de quanto cortam: a NOISE GATE em
OFF, LOW (32), DEFAULT (64), MID (76) e HIGH (88), e a NR em OFF, LOW, MID, DEFAULT e HIGH (o DEFAULT de
fábrica corta mais que o MID). O código gravado não muda (0 OFF, 1 DEFAULT, 2 LOW, 3 MID, 4 HIGH; na NR, o
1 continua sendo o ligado de fábrica): o get e o set do menu (`porta_menu_le`, `nr_menu_le`) passam da posição
ao código por uma tabela, e a barra de cima e o `0x28` usam o código. No MODEM GAIN e na tela do APRS, o MEDIUM
e o MED viraram MID, como nos outros itens.

**O RESET (v82).** SYSTEM > RESET tem VFO e ALL. Os dois põem os ajustes de fábrica (`0x01E4842E`: os 32
primeiros bytes do bloco `0x9000` de `0x01E607B1` e os 39 seguintes de `0x01E603C3`), e o VFO também os VFOs
(`0x01E3A5C0`); o ALL, além disso, apaga o bloco de 32 KB do endereço 0 (o comando `0x52` da flash em
`0x01E4324C`, os canais) e grava os canais padrão (`0x01E48336`). Nenhum dos dois chega aos setores do
`0xC1000` em diante: os bancos, o WATCH, o VFO da C, o Morse, o APRS e o histórico ficam. Os bytes dos
nossos itens voltam ao padrão de hoje pela marca de cada um, e o `gancho_ajustes`, logo depois dos ajustes de
fábrica, desarma o envio e reaplica o áudio, o cristal e a luz. O reset liga o TDR (o 1 da tabela), e com o
WATCH em SINGLE o `gancho_ajustes` o zera desde a v82 (o mesmo vale para um upload do CHIRP).
