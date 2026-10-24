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
| `0x00015E2C` / `0x000129A4` | scramble do canal ou VFO atual / o da sintonia: 0 OFF, 1 3400 Hz, 2 2900 Hz (v24) |
| `0x01E68B18` | frequências de inversão do scramble (u16, Hz × 10,32444): 35103 = 3400 Hz e, desde a v24, 29941 = 2900 Hz |
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
  `0x01E4CBE8`. A tabela nova é gravada por cima da original (que tem as mesmas 54 vagas, contando
  o item zerado) e a contagem vira a constante 54, porque a vaga do item zerado passa a ter item.
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

## 10. Prova de que não brica

A inicialização do app lê as teclas (`0x01E56C70/C7E/C8C`) **antes** de carregar configuração e
canais; com PTT2 + atalho ela desenha `UPDATE` e fica esperando gravação. O script de montagem tem
uma trava (`PROTEGIDAS`) que **recusa** qualquer escrita nessas rotinas e nas do modo UPDATE, e a
conferência final refaz essa checagem byte a byte.

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

**v24:** a segunda entrada da tabela passa a **29941 = 2900 Hz**, e o item SCRAMBLE ganha a
terceira opção: **OFF, 3400 Hz, 2900 Hz**. Valor 2 = 2900 Hz.
- Só mudam a quantidade de opções (3) e as duas tabelas de nomes. O get e o set continuam os de
  fábrica, porque o valor gravado já é a posição no menu. O OFF em chinês é o de fábrica.
- No CHIRP, o campo scramble do canal pega os bits 7–2 do byte 14, então a opção 2900 Hz é o
  `0x08`. No VFO, o formato da base parte o nibble em `unknown3` (bits 7–5) e `scramble` (bits
  4–2), e por isso o driver troca esse ajuste pelo dele.
- A verificação `4k` do `conferir.py` tira a tabela da instrução de `0x01E40936`. Ela confere que
  a primeira entrada não mudou e calcula a frequência da segunda pela conta da primeira (3400 Hz).
