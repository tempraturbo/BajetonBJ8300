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
| `0x0A000–0x0FFFF` | ajustes; `0x0F000–0x0F24F` é calibração de fábrica (potência de TX em `0xF000–0xF0BF`, [seção 20](#20-a-potência-de-tx-mapa-só-leitura); ganho do microfone em `0xF210`, [seção 19](#19-o-ganho-do-microfone-e-o-mic-gain-v49)) |
| `0x10000–0x1AFFF` | **imagem de abertura**: 160×138 em RGB565 little-endian, 44160 B |
| `0x20000–0x99FFF` | fontes e textos em chinês (~480 KB) |
| `0xC0000–0xC0FFF` | 64 registros de 60 B que o firmware usa |
| **`0xC1000–0xD1FFF`** | **os bancos** (cabeçalho + 16 setores de 4 KB) |
| **`0xD2000–0xD2FFF`** | **o registro do WATCH** (v43; o cabo o grava desde a v46; [seção 18](#18-a-dupla-escuta-e-o-triple-v42-a-v51)) |
| **`0xD3000–0xD3FFF`** | **os VFOs da C** (v49: o da C e o do B guardado; [seção 18](#18-a-dupla-escuta-e-o-triple-v42-a-v51)) |
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
  constante: 55 desde a v43.
- Um item novo entra no lugar dos de zona (o WATCH, o CH-C e os do BANK) ou logo depois de um de
  fábrica (o MDF-C, depois do MDF-B: `ITENS_DEPOIS` do `montar.py`), e copia o resto do item que
  serve de molde.
- **O motor não tem submenu.** O nibble baixo das `flags` escolhe entre 7 tipos de tela (0 =
  lista/valor, 2 = CTCSS, 3 = frequência, 4 = MEMCH/DELCH, 7 = editor de texto, 8 = VERSION, 10 =
  DCS) e nenhum abre uma lista dentro de outra. Dois níveis (BANK → banco → renomear/canais) seriam
  possíveis trocando a tabela corrente de dentro do nosso próprio handler, como `FUN_01E4B3A0(id)`
  faz nos menus rápidos — é interface nova, não foi feito.

## 7. Como o filtro dos bancos funciona

O rádio decide quais canais existem lendo dois mapas de bits na RAM, montados na partida varrendo a
memória de canais. **`FUN_01E39FCE` é o único leitor deles.** O filtro reescreve esses mapas com a
interseção entre "o que existe" e a união dos bancos ligados — por isso navegação, busca e
revalidação passam a respeitar os bancos **sem alterar nenhuma função do firmware**.

Para desfazer, os mapas são remontados do zero com o mesmo critério da partida (4 bytes por canal,
BCD, `VALIDA_FREQ`), em vez de guardar cópia: assim não existe estado nosso em RAM e canal gravado
ou apagado pelo MEMCH/DELCH entra na conta na hora.

Depois de o filtro mudar pelo menu (o BANK e o BANK CH), **os dois lados** vão para um canal que
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
Desde a v46 a janela de gravação do cabo o inclui, e o CHIRP grava o WATCH, o CH-C e o MDF-C.

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
e desligada só a que está em evidência.

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
usa batem com esse chip (`0x36` amplificador, `0x43` filtro, `0x47`/`0x48` áudio, `0x71` scramble),
mas não há prova direta do chip no firmware: nenhum texto com o nome, nenhuma leitura da
identificação. Por isso o MIC GAIN não passa de 31. No rádio de teste, pelo cabo, o `0xF210` foi a 18
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
