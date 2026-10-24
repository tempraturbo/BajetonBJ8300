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
  código novo. Medir com `firmware/scripts/espaco_livre.py`.

## 3. Mapa do chip de dados (1 MB)

Levantado pelo cabo em 2026-10-24 com `tools/cabo/mapa_memoria.ps1`, que acha o tamanho real
procurando onde o conteúdo começa a espelhar o endereço 0:

| Faixa | Conteúdo |
|---|---|
| `0x00000–0x09FFF` | 960 canais de 32 B |
| `0x0A000–0x0FFFF` | ajustes; `0x0F000–0x0F24F` é calibração de fábrica |
| `0x10000–0x1AFFF` | **imagem de abertura**: 160×138 em RGB565 little-endian, 44160 B |
| `0x20000–0x99FFF` | fontes e textos em chinês (~480 KB) |
| `0xC0000–0xC0FFF` | 64 registros de 60 B que o firmware usa |
| **`0xC1000–0xD1FFF`** | **os bancos** (cabeçalho + 16 setores de 4 KB) |
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
| Sair | `E` |

Abaixo de `0xF000` os dados trafegam **cifrados** (XOR com a chave `CO 7`, pulando 0x00, 0xFF,
espaço na chave e os bytes iguais à chave ou ao inverso dela). O comando novo entrega **cru**.

**Como o tratador funciona** (base `r13 = 0x14680`, carregada em `0x01E4B422`):

- pacote em **`0x16671`** (letra, endereço alto, endereço baixo, tamanho), dados e resposta a
  partir de **`0x16675`**; comprimento recebido em **`h[0x1666C]`**;
- `'R'` dispara com **4 bytes** no buffer (`0x01E4BB52`), `'W'` com **68** (`0x01E4C0E8`);
- os dois chamam **a mesma rotina `0x01E43970`** — 0 para ler, 1 para gravar;
- depois de responder, o contador é zerado (`0x01E4BB7C`, e `0x01E416C4` no caminho do `'W'`).

Duas armadilhas que custaram tempo:

- **Não existe pacote de 5 bytes.** O quinto byte chega depois do disparo, fica pendente e engole o
  comando seguinte — inclusive o magic. O rádio só volta desligando.
- **Nada pode ser enviado antes do magic.** A comparação é desde o início do buffer, e o magic
  termina em `U`, que é comando do modo UPDATE: insistir fora de sincronia faz o rádio entrar em
  UPDATE sozinho.

### Comando estendido (deste firmware)

Cabe nos mesmos 4 bytes: **bit 7 do tamanho ligado** = endereço de 24 bits, com os outros 7 bits
valendo o terceiro byte; o bloco é sempre de 64 B. Bit 7 em zero = comportamento de fábrica, byte a
byte — o CHIRP não vê diferença. Alcance: 8 MB.

Gravação estendida só em `0xC1000..0xD1FFF`, apagando o setor quando o bloco começa nele. Qualquer
outro endereço é ignorado sem escrever nada (provado com `tools/cabo/teste_cabo_grava.ps1`).

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

## 6. Motor de menus

A tabela do menu principal fica em **`0x01E62F98`**, com **53 itens de 28 bytes**:

```
flags(1) | máximo(u16) | get(4) | set(4) | nome_en(4) | nome_cn(4) | exibe|opções(4) | opções_cn(4) | id(1)
```

- `flags` 0x80 = item numérico com função de exibição própria; 0x07 = abre o editor de texto.
- O byte `id` **não** é o número mostrado na tela (os valores se repetem e vários são 0).
- O ponteiro da tabela é montado em `0x01E4CBB2` e a contagem vem de `FUN_01E4A81A`, fixada em
  `0x01E4CBE8`. É assim que a cópia com itens novos entra sem sacrificar os de fábrica.
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

## 10. Prova de que não brica

A inicialização do app lê as teclas (`0x01E56C70/C7E/C8C`) **antes** de carregar configuração e
canais; com PTT2 + atalho ela desenha `UPDATE` e fica esperando gravação. O script de montagem tem
uma trava (`PROTEGIDAS`) que **recusa** qualquer escrita nessas rotinas e nas do modo UPDATE, e a
conferência final refaz essa checagem byte a byte.
