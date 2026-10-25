# Histórico das versões

> 🇬🇧 English: [changelog.md](changelog.md)

Datas de 2026. Cada versão foi gravada e testada no rádio; o que quebrou está registrado com a
causa, porque é isso que evita repetir.

## Driver do CHIRP — 25/10 — COIN e sem Voice Prompts

- **Roger com cinco opções**, como o rádio: OFF, BEEP 1, BEEP 2, **COIN** e TONE1200. O COIN grava o
  valor 4, que não cabe nos 2 bits do roger da classe base: o módulo lê e grava 3 bits, sendo o
  terceiro o bit baixo do byte de cima (`unused_9017`, sem uso no rádio).
- **Sem "Voice Prompts"** no Settings: desde a v30 o firmware não toca voz nenhuma, e o item VOICE
  saiu do menu do rádio.

O teste ganhou os casos do valor 4 (abre como COIN, grava o byte certo) e da página sem o ajuste
de voz. O módulo do release v1.0T não mudou: este acompanha a v32.

## v32 — 25/10 — o espaço das vozes, provado ✅ (em teste)

A v31 com o diretório `tone` consistente: o `repack` refaz o CRC de cada arquivo cujo conteúdo
mudou e o do próprio diretório (`jlfw.refaz_crcs_tone`), do jeito que a ferramenta da JieLi os
grava. A canária virou `BVOZTV32`.

Lida pelo cabo no rádio: ela está lá, e o cabeçalho do `tone` chegou com o CRC novo. A causa da
v30 era essa: **a atualização só regrava arquivo de recurso cujo CRC mudou**. Com isso, os ~112 KB
das vozes (`0x01E797A0..0x01E956F0`) são espaço nosso, lido pelo processador e gravado pela
atualização, sem mudar o tamanho da imagem nem a tabela de partições. Detalhes na
[seção 15 da engenharia reversa](engenharia-reversa.md#15-vozes-fora).

## v31 — 25/10 — diagnóstico: memória pelo cabo, e o COIN

- **Leitura de memória pelo cabo.** No `R` estendido, endereço com o bit 22 ligado (0x400000 em
  diante, que o chip de dados de 1 MB não tem) devolve o que o processador enxerga a partir de
  `0x01E00000`, pelo `memcpy` da ROM do chip (`0x021127A8`, 191 chamadores no app). Só leitura; a
  gravação pelo cabo não mudou.
- **COIN.** O BEEP 3 passa a se chamar COIN no menu, e a moeda volta a tocar da tabela normal.
- **O que a leitura mostrou:** o começo do `app.bin`, o nosso código, o cabeçalho do `tone` e o
  `index.idx` iguais ao `.anc`; em `0x01E797A0`, a voz original do C0, e logo depois dos 18 bytes
  da canária o rádio e o `.anc` voltam a ser iguais. O processador lê a região certinho; quem não a
  gravou foi a atualização.

## v30 — 25/10 — vozes fora ⚠️ **a canária não apareceu**

A pedido do dono, nenhuma voz toca mais (nem em chinês nem em inglês), para o espaço delas virar
código. Toda voz toca por um de três caminhos, e cada um vira o "sem voz" que já existe nele:

- a **fila de sons** (`0x01E40628`): `ifs (r5 >= 118) {` vira `if (r5 < 118) goto` o retorno — voz
  sai da fila sem tocar, e os bipes seguem como eram;
- o **modo que toca o C16** (`0x01E55ABE`, escolhido pelo `main`): o desvio do caso sem voz vira
  incondicional;
- os **eventos** (C1, C2, C3 e C15, `0x01E555D6`): a rotina intermediária volta sempre sem tocar.

O item **VOICE** saiu do menu (53 itens). Os bipes (B0–B7), o `index.idx` e o `cfg_tool.bin` ficam.
No rádio: nenhuma voz, os bipes de tecla certos.

A canária — a tabela do BEEP 3 com uma assinatura, no começo do espaço das vozes — falhou: o roger
tocou o BEEP 2, o caminho de segurança. A v31 mostrou por quê.

## v29 — 25/10 — de volta ao tamanho de fábrica (não gravada)

A v27 sem a extensão da v28, mais duas coisas:

- **48 bytes a menos:** os rótulos de pontos e passo do espectro vão direto para a rotina de texto,
  sem a cópia com `SPRINTF("%s ")` — a janela já é limpa de preto, e o espaço a mais só pintava
  preto.
- **Código até 3260 bytes,** para terminar antes dos últimos 32 bytes da menor imagem, onde o uboot
  guarda o marcador `BOOT_START_FIRST` (documentado para a família anterior por
  [kagaimiq](https://kagaimiq.github.io/jielie/datafmt/br17-fw.html)). Nas imagens flash2/4/6 o
  código da v26 e da v27 terminava dentro deles.

A conferência `2f` passa a exigir, nas 6 imagens, o tamanho de fábrica, o uboot e a tabela de
partições idênticos e os últimos 32 bytes apagados. Rodada sobre a v27, ela aponta exatamente as
três imagens menores.

## v28 — 25/10 — imagem 8 KB maior ❌ **bootloop, não fica em releases**

Para ganhar espaço de código, as 6 imagens cresceram 8 KB, para dentro da faixa sem partição entre
o fim da imagem e a VM (~224 KB nas 6 variantes). O desmontado mostrava que nada do firmware grava
ali ([seção 14](engenharia-reversa.md#14-flash-interna-partições-e-quem-grava)). O gravador
terminou com sucesso, e o rádio ficou em **bootloop**: logo e reset em menos de 1 s. Voltou pelo
PTT2 + atalho, com a v27.

O caminho da partida era idêntico byte a byte ao da v27, então quem quebrou foi o crescimento.
**Causa não provada:** o uboot e o carregador da UART são cifrados com uma cifra que não é a do
`enc`/`sfc` (testadas as 65536 chaves dos dois), e não dá para ver o que eles fazem com o tamanho
da imagem. A suspeita é a partição PRCT, que a ferramenta da JieLi sempre grava com o tamanho da
imagem (`CODE_LEN`) e que ficou com o antigo.

Da tentativa ficou uma lição de formato: as entradas do UFW vêm uma atrás da outra, e três delas
(`isd_config.ini`, `script.ver`, `blimit.bin`) são cifradas **pela posição no arquivo** — crescer
uma imagem as muda de lugar, e elas têm de ser cifradas de novo. A conferência `1` pegou isso na
primeira montagem.

## v27 — 25/10 — passo alinhado com o pico e o dBm ✅

O rótulo do passo (`25.0k`) desce de y 34 para 37, a altura do pico e do dBm na segunda linha.
Testada no rádio.

## v26 — 25/10 — espectro com a barra atual e o pico; roger BEEP 3 ✅

- **Barra atual em branco.** A coluna do ponto que a varredura está medindo sai em branco (as outras
  continuam azuis), com 3 px no mínimo para aparecer onde não há sinal. Com 80, 40 ou 20 pontos,
  as 2, 4 ou 8 colunas do ponto.
- **Frequência que roda.** A frequência grande de cima acompanha a varredura, trocando junto com a
  barra branca (a cada 5 voltas do laço; cada troca manda 3 KB ao LCD). Com dígitos sendo
  digitados, ela não escreve por cima.
- **Linha do pico.** A frequência mais forte da última passada vai para a segunda linha, onde ficava
  o dBm, e fica até a passada seguinte terminar. Some quando a janela muda.
- **dBm à direita**, embaixo do AM/FM, com o mesmo conteúdo de antes.
- **Roger BEEP 3:** a moeda do Super Mario World, medida do mp3 do dono — 2000 Hz por 70 ms e
  2672 Hz por 330 ms. O menu do roger passa a ter cinco opções, e o BEEP 3 grava o valor 4 (o rádio
  guarda o roger num byte inteiro). Na v31 ele virou **COIN**.

Detalhes nas seções [11](engenharia-reversa.md#11-espectro) e
[12](engenharia-reversa.md#12-roger) da engenharia reversa. Testada no rádio.

## Driver do CHIRP — 24/10 — todo em inglês, e os bancos em blocos no Settings

O firmware é o mesmo da v1.0T; muda só o `bj8300_v1.0TEMPRA.py`.

- **Todo em inglês**, porque o módulo é usado por gente do mundo todo: os textos da tela, os
  avisos, as mensagens de erro, os comentários e os nomes internos.
- **Settings → Banks** com um bloco por banco (**BANK 1** a **BANK 16**), cada um com **Name**
  (editável), **Channels** (quantos canais gravados ele tem, só leitura) e **Enabled**. Até aqui
  havia só uma lista de "Banco N ligado", e o nome só se editava na aba Banks (duplo clique no
  cabeçalho da coluna, que continua valendo).
- **Banco sem canal gravado não liga**, a mesma regra do firmware. O CHIRP desmarca o checkbox na
  hora, e a regra é conferida no clique, não pela contagem de quando a página abriu. Tirar o
  último canal de um banco na aba Banks o desliga sozinho, e o upload desliga qualquer banco
  ativo e vazio antes de gravar.
- **Name e Enabled só são gravados quando mudam nesta página.** O CHIRP monta a página uma vez,
  então um nome dado na aba Banks depois disso não é desfeito pelo valor velho da página.
- O checkbox que fica cinza depois de marcar e desmarcar é do CHIRP, não do módulo: o "Voice
  Prompts" do driver de fábrica fica igual, e o valor gravado sai certo.

Conferido no CHIRP de verdade (com o wxPython), lendo o rádio pelo cabo, só leitura: a página
mostra os 16 blocos com os nomes e as contagens do rádio. O banco 8, vazio, não ligou. O banco 1
ligou, e o nome digitado no Settings foi gravado na imagem. Os três testes passam, com os casos
novos (blocos, contagem, recusa, validação no clique, desligar sozinho, valor velho que não volta,
e o upload que desliga banco vazio).

No [release v1.0T](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v1.0T), o
`bj8300_v1.0TEMPRA.py` foi trocado por este (sha256 `073c73b5…`, conferido baixando sem login) e
as notas do release ganharam um aviso da troca. Os outros arquivos do release não mudaram.

## v1.0T — 24/10 — a primeira versão estável ✅

A v25 virou a **v1.0T** ("T" de Tempra), a primeira versão estável. É o mesmo arquivo, byte a
byte (sha256 `e8b18e68…`), agora com o nome da versão: `firmware/releases/BJ8300_BANK_v1.0T.anc`.
O menu `VERSION` do rádio já mostrava `Firmware: V1.0T` desde a revisão dos textos.

Publicada no [release v1.0T do GitHub](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v1.0T), junto com o módulo do CHIRP, o gravador e o
driver do cabo. O README ganhou, logo no começo, tudo o que mudou desde a V0.12 e o passo a passo
da gravação e do CHIRP (modo desenvolvedor, carregar o módulo, achar o rádio em **Baojie** →
**BJ8300_v1.0TEMPRA**), com o aviso de não usar os modelos de fábrica do CHIRP nem módulos
antigos com este firmware.

## v25 — 24/10 — scramble de 2700 a 3400 Hz ✅

- **SCRAMBLE com nove opções, em ordem crescente: OFF e de 2700 a 3400 Hz, de 100 em 100.** É
  para cobrir os outros modos do Baofeng, e não só o 3 (2900 Hz). Os valores que já estavam
  gravados nos canais não mudam: o 1 continua 3400 Hz (o ON de fábrica, o que fala com os outros
  BJ-8300) e o 2 continua 2900 Hz. As frequências novas ficam do 3 ao 8.
- **Como:** a rotina que aplica o scramble escolhia entre duas frequências por "valor é 1 ou
  não". Agora 14 bytes dela leem a frequência de uma tabela nossa, pelo valor gravado, com o índice
  mascarado em 4 bits. O get e o set do item são nossos e traduzem posição e valor, e o set grava
  pelo de fábrica. A tabela de fábrica volta a ficar intacta. Detalhes na
  [seção 13 da engenharia reversa](engenharia-reversa.md#13-scramble).
- **Driver do CHIRP:** as nove opções no canal e nos VFO A e B, na mesma ordem do rádio. O teste
  tira do `bancos.c` o valor que cada opção grava.
- **Conferência:** a `4k` foi refeita. Ela confere cada frequência, a ordem do menu, os nomes, a
  ida e volta posição ↔ valor e os 14 bytes do remendo, e foi testada trocando duas posições.

Código: 3012 B de 3292. Build reprodutível, zero desvios em remendo, os três testes passando.
**Testada no rádio** (24/10): os modos 1, 2 e 3 do UV-17L se entendem com o BJ-8300, cada um
numa das frequências do menu.

## v24 — 24/10 — scramble que fala com o Baofeng UV-17L ✅

- **SCRAMBLE com três opções: OFF, 3400 Hz, 2900 Hz.** O scramble do chip é inversão de espectro,
  e a frequência de inversão sai de uma tabela de duas entradas no firmware. A primeira entrada
  (3400 Hz) é a que a opção ON de fábrica usa. A segunda (3500 Hz) nenhuma opção alcançava, e
  agora vale **2900 Hz**, que é onde o UV-17L inverte no modo 3. As duas frequências foram medidas
  na gravação do dono, com um tom de 1 kHz. Os modos 1 e 2 do UV-17L não batem com o BJ-8300.
  O get e o set do item continuam os de fábrica; mudam só a quantidade de opções, os nomes e os
  2 bytes da tabela. Detalhes na [seção 13 da engenharia reversa](engenharia-reversa.md#13-scramble).
- **Driver do CHIRP:** o scramble do canal e o dos VFO A e B mostram e gravam as três opções. No
  VFO, a base parte o valor em dois campos, e o driver grava como o rádio grava: o nibble alto do
  byte e mais nada.
- **Conferência:** a `4k` confere a tabela do scramble. A `2e` é nova e vale para tudo: o app.bin
  decifrado tem de ser o mesmo nas 6 imagens. Até aqui só o código e a tabela de menus eram
  conferidos nas seis; os remendos do roger e do scramble, só na primeira. As duas foram testadas
  com o defeito que devem pegar (a v23, sem o remendo, e uma cópia com um byte trocado só na
  segunda imagem).

Código: 2804 B de 3292. Build reprodutível, zero desvios em remendo, os três testes passando (6
conferências novas do scramble no CHIRP de verdade). **Testada no rádio** (24/10): com o
scramble em 2900 Hz, o BJ-8300 e o UV-17L no modo 3 se entendem.

## v23 — 24/10 — BEEP 2 na velocidade do mp3 ✅

Na v22 o BEEP 2 funcionou, mas levava 0,8 a 1 s, contra ~0,4 s do mp3. A causa está no firmware: a
rotina de espera que o roger usa (`0x01E36286`) conta em **unidades de 2 ms**, não em ms — o
temporizador interrompe a cada 48000 ciclos de 24 MHz (2 ms, `0x01E3E44C`) e a espera vai até o
contador andar o dobro do que se pede. Os `100` da v22 davam 200 ms por tom. Agora cada tom espera
50 unidades = **100 ms**, e o BEEP 2 inteiro fica em ~0,4 s. (O BEEP de fábrica, com 40 unidades,
sempre teve 80 ms por tom; ele não mudou.)

Só muda o número da espera; código e ganchos são os da v22. **Testada no rádio** (24/10): funcionou.

## v22 — 24/10 — roger BEEP 2, e o driver do CHIRP numa sessão só

- **ROGER com quatro opções: OFF, BEEP 1, BEEP 2, TONE1200.** O BEEP 1 é o BEEP de fábrica
  (1000 Hz e 850 Hz, 80 ms cada); o **BEEP 2** toca os quatro tons do mp3 que o dono mandou —
  **1125, 1857, 1359 e 931 Hz, 100 ms cada** —, transmitindo, logo antes de o rádio voltar a
  receber. Os valores gravados continuam os de fábrica (BEEP 1, TONE1200 = 2) e o BEEP 2 é o 3, que
  cabe nos 2 bits do ajuste; o menu troca a ordem. Os quatro tons saem com o mesmo volume (no mp3 o
  segundo é mais baixo). Detalhes na [seção 12 da engenharia reversa](engenharia-reversa.md#12-roger).
- **Driver do CHIRP:** a leitura falhava no rádio com "Radio returned unknown identification
  string". O driver lia canais e ajustes numa sessão, saía com `E` e abria outra para os bancos;
  mapeado no firmware, o `E` responde com um ACK que o CHIRP não lê, e a sessão só fecha numa volta
  do laço sem bytes chegando — o `PROGRAM…` da segunda sessão era descartado e o CHIRP lia o ACK que
  sobrou. Agora é **uma sessão só** para tudo, e na gravação a conferência do firmware vem antes de
  gravar qualquer coisa (num rádio de fábrica nada é gravado). O driver também mostra e grava o
  BEEP 2. O rádio simulado do teste passou a se comportar como o firmware no `E`, e com ele o driver
  antigo falha exatamente com a mensagem que apareceu no rádio.

Código: 2764 B de 3292. Build reprodutível, zero desvios em remendo, os três testes passando (6
conferências novas do roger no CHIRP de verdade, 4 novas na clonagem). **No rádio:** o BEEP 2
funcionou, mas com 200 ms por tom em vez de 100 (corrigido na v23).

## v21 — 24/10 — a v20 sem o bootloop ✅

O mesmo conteúdo da v20 (espectro 2× mais rápido, passo de 10 kHz, 320/640 pontos, pico seguro,
verde `0x0240`), com o código gravado onde o linker o ligou. Código e ganchos são byte a byte os da
v20; muda só o lugar do bloco na flash.

**Por que a v20 entrava em bootloop:** o linker alinha a seção de código pelo maior alinhamento
que encontra dentro dela, e a v20 trouxe as tabelas de ponteiros dos rótulos novos (alinhamento 4):
o código saiu ligado para `0x01E96124`, mas o montador gravava num endereço fixo, `0x01E96122`. Tudo
ficou 2 bytes fora do lugar, e o primeiro gancho a rodar — o do fim da partida, logo depois do logo
— pulava para o meio de uma instrução. Nenhuma conferência pegou porque todas usavam o mesmo
endereço fixo dos dois lados.

**O que impede de novo:** o código começa no primeiro endereço múltiplo de 4 (`0x01E96124`); o
`montar.py` lê o endereço do código no ELF e recusa montar se não for onde ele grava; e o
`conferir.py` ganhou a `2c` (endereço ligado = gravado) e a `2d`, de ponta a ponta: cada um dos 33
desvios do firmware para o código novo é seguido até a flash, e os bytes que estão lá, decifrados,
nas 6 imagens, têm de ser os do ELF. Rodadas contra a v20, as duas reprovam — a `2d` aponta
exatamente o gancho da partida.

Código: 2558 B de 3292. Build reprodutível, zero desvios em remendo, os três testes passando.
**Testada no rádio** (24/10): "funcionou perfeito"; a varredura ficou cerca de 1 s mais rápida.

## v20 — 24/10 — espectro mais rápido, 10 kHz, 320/640 pontos, pico seguro, verde mais escuro ❌ **bootloop, retirada**

- **Espectro duas vezes mais rápido:** cada volta do laço passa a medir um ponto e já sintonizar o
  seguinte, em vez de gastar uma volta para cada coisa. O chip continua tendo uma volta inteira
  entre sintonizar e medir, como de fábrica, então a medida sai igual: 160 pontos em ≈ 2,5 s
  (eram 5 s).
- **Passo de 10 kHz:** o OK passa por 6,25 → 8,33 → **10** → 12,5 → 20 → 25 kHz (filtro do chip o
  mesmo do 8,33 e do 12,5).
- **320 e 640 pontos:** o # passa por 160 → 80 → 40 → 20 → **640** → **320**. O gráfico continua
  com 160 colunas; cada coluna mede 2 ou 4 frequências e mostra a maior, e parar num sinal acontece
  na frequência exata dele. Com 25 kHz, 640 pontos cobrem 16 MHz; a varredura leva ≈ 5 s (320) e
  ≈ 10 s (640).
- **Pico seguro:** a barra sobe na hora e, quando o sinal some, desce a metade do caminho a cada
  passada. Um sinal curto fica marcado por algumas passadas e o ruído para de piscar. Trocar de
  janela (digitar frequência, OK, #, entrar no espectro) limpa o gráfico.
- **Verde do realce mais escuro:** `0x0240` (#004900, degrau 18), contra o `0x02C0` (#005900, 22)
  da v17 à v19.
- Os índices novos (640, 320 e 10 kHz) entram no fim da configuração que o rádio grava, então os de
  fábrica mantêm o valor. **Antes de voltar para uma versão anterior**, deixe o # num de 160/80/40/20
  e o passo fora do 10 kHz: as versões anteriores não conhecem os novos e mostrariam lixo.

Duas coisas do plano saíram diferentes do proposto, por motivo concreto:
- **"Redesenhar o texto do pico só quando muda"** não tinha o que ganhar: mapeado, o texto em dBm
  só é desenhado uma vez por passada e quando a varredura para — não a cada coluna, como eu tinha
  dito.
- **"Média de leituras por ponto"** não entrou como várias leituras seguidas: sem saber de quanto em
  quanto tempo o chip renova o RSSI, ler duas vezes seguidas pode devolver o mesmo número e não
  limpar nada, e custaria tempo. A descida pela metade do pico já é uma média entre a passada nova
  e a anterior. "Sempre 160 pontos" é o que o # em 160, 320 e 640 já dá; com 20/40/80 os degraus
  são o próprio significado de medir menos pontos.

Mapa completo na [seção 11 da engenharia reversa](engenharia-reversa.md#11-espectro). Código: 2558 B
de 3294. Build reprodutível, zero desvios em remendo, os três testes passando. **No rádio: bootloop**
— o logo aparece e o rádio reinicia em menos de 1 s, em laço. Causa e correção na v21.

## v19 — 24/10 — seta continua o espectro, e a tabela de menus no lugar ✅

- **Espectro:** parado num sinal achado, a seta (de cima ou de baixo) **continua a varredura** e,
  dessa vez, não mexe no limite. Enquanto varre, a seta faz o que sempre fez (sobe ou desce o
  limite). A continuação é exatamente a retomada que o próprio firmware faz quando o sinal cai
  (`0x01E4AEFC`: fecha o áudio e passa ao próximo ponto), e o build confere isso nos bytes de
  fábrica (verificação `4i`). O gancho entra numa instrução de 4 bytes do tratador de teclas do
  espectro (`0x01E4BFBE`); os registradores que a chamada nova suja foram conferidos mortos ali em
  todos os caminhos. Detalhes na [seção 11 da engenharia reversa](engenharia-reversa.md#11-espectro).
  O bipe dessa tecla passa a ser o das teclas sem função no espectro (código 0), não o da seta.
  Se o sinal for forte, a varredura pode parar de novo no ponto vizinho do mesmo sinal: aí é
  apertar de novo.
- **Espaço:** a tabela de menus nova passa a ser gravada **no lugar da original** (as 54 vagas, com
  o item zerado do fim, são exatamente os 54 itens dela), em vez de ocupar 1512 bytes da área livre.
  O código tem agora 3294 bytes disponíveis, contra 1782. Provado antes que mais nada lê a tabela
  original; o ponteiro dela fica como era. O código e a tabela saíram byte a byte iguais aos da
  v18, só a tabela em outro lugar.

Código: 1792 B de 3294. Build reprodutível, zero desvios em remendo, os três testes passando.
**Testada no rádio** (24/10): para no sinal, a seta continua sem mexer no limite, e o bipe ficou bom.

## Driver do CHIRP — 24/10 — sem zonas, e a trava do firmware refeita

Revisão geral do `chirp/bj8300_v1.0TEMPRA.py`, com um teste novo (`tests/teste_clonagem.py`) que
roda o `sync_in`/`sync_out` inteiros do CHIRP contra um rádio simulado, nos dois firmwares:

- **Zonas:** o driver já não mostrava nada de zona (a base BJ7800 esconde, e o modelo de 15 zonas
  fixas é trocado pelos bancos) — levantado ajuste por ajuste no CHIRP real: 111 ajustes em 4
  grupos, nenhum de zona. O que faltava era o **modo**: ele continua no firmware (`b[0x14BD4]`,
  que o cabo lê e grava em `0x9028` — deduzido da rotina que salva os ajustes, `0x01E3AB36`, e
  conferido no formato do CHIRP), e sem o item de menu o rádio não sairia dele. Agora todo upload
  grava **Full Channel**. O rádio do dono já estava em Full Channel (dump de 24/10).
- **Trava do firmware:** comparava a leitura estendida de `0x0C0000` com a leitura normal de
  `0x0000` — mas a normal volta decifrada e a estendida crua, então nunca coincidiam. Contra um
  rádio de fábrica ela só segurava porque a resposta longa dele (4 + `0x8C` bytes, lido no tratador
  `0x01E4BB6E`) dessincronizava o cabo, e a mensagem era um "Failed to read block" genérico. Agora
  o driver lê o cabeçalho da resposta: `0x40` = firmware dos bancos; outra coisa = fábrica,
  descarta o resto e para com a mensagem certa.
- Rodado contra o driver antigo, o teste novo falha exatamente nesses pontos (Full Channel,
  mensagem, cabo dessincronizado) e passa no que importava: nenhuma gravação estendida chegava ao
  firmware de fábrica.

Os bancos em si (formato, marca, soma, pertinência múltipla, só os setores mudados) passaram em
tudo: 27 + 20 + 16 conferências.

## v18 — 24/10 — fundo verde também no RX/TX

Na v17 a caixinha do **RX/TX**, na ponta da barra, ficava com fundo preto (e os fiozinhos entre os
segmentos da barra também, enquanto recebe ou transmite).

Ao receber ou transmitir, o firmware passa por outro caminho (`0x01E40F1C` com argumento 1 ou 2):
redesenha a barra do lado afetado numa **janela própria** `(19, 140, y, y+9)` — segmentos e o texto
RX/TX em x=127 — e manda por `0x01E3B7CA`; no modo frequência, redesenha ainda a linha grande, que
sai por `0x01E41008`. Os dois envios entraram no gancho do envio, com a mesma trava: só fica verde a
janela que cai na área do lado em evidência. O envio de `0x01E3B7CA` fica dentro de um bloco
condicional (`if (r8 != 0)`); conferido no arquivo final que o prefixo do bloco ficou intacto e só
o destino da chamada mudou.

Sem código novo (1732 B), 16 sítios no envio e 2 na limpeza, build reprodutível, zero desvios em
remendo. **Ainda não testada no rádio.**

## v17 — 24/10 — bloco inteiro verde, sem faixas pretas, e verde mais escuro ✅

**Testada no rádio:** "ficou excelente, tá perfeito" — muda de lado, fica da cor certa, sem as faixas
pretas. Só a caixinha do RX/TX continuou com fundo preto (v18).

Fecha as faixas finas pretas que sobravam na v16 (entre o nome e a frequência, e entre a frequência
e a barra), mapeadas e conferidas pixel a pixel nas fotos.

- **De onde vinham:** nenhuma janela desenha ali, então o gancho do envio não tinha o que
  recolorir, e a faixa ficava com o que a limpeza da tela deixou: preto.
- **Quem limpa:** `0x01E3ACBA(2)` enche de preto a área do lado em evidência (72 linhas a partir
  de 16 com o A, de 56 com o B), a do outro lado, e desenha **a linha separadora branca** (88 ou
  55). É a mesma limpeza que a troca de lado faz, então nunca sobra verde velho de um layout no
  outro.
- **O conserto:** um gancho nas duas chamadas que limpam e redesenham a tela principal
  (`0x01E3C294` — partida, saída do menu e mais 14 chamadores — e `0x01E40F66`, a troca de lado)
  deixa a limpeza correr e enche de verde a área do lado em evidência. As janelas vêm por cima, e
  nos vãos fica o verde. A entrada do menu também limpa assim, mas por outra chamada, e não é
  tocada; o redesenho sem limpeza (o da varredura, por exemplo) também não, para não piscar.
- **Verde mais escuro:** `0x02C0` (#005900), entre o `0x0380` da v16 (claro demais no bloco
  inteiro) e o `0x0280` (que tinha sido pedido mais claro).
- **Espaço:** as rotinas do firmware passaram a ser chamadas por símbolo absoluto (`FIRMWARE()`),
  e cada chamada caiu de 8 para 4 bytes — mesmo destino. O código ficou em 1732 dos 1782 bytes,
  com o gancho novo dentro. A verificação nova `4g` recalcula o destino de cada `call`/`goto`
  gerado e reprova qualquer um que não seja uma função interna ou uma rotina declarada; testada
  tirando uma declaração, acusou.

Build reprodutível, zero desvios em remendo, 16/16 sítios nas seis imagens. **Ainda não testada
no rádio.**

## v16 — 24/10 — endereços da janela corrigidos ✅ **realce funcionando**

**Testado no rádio:** o bloco do lado escolhido fica verde — linha do `H`, nome, frequência, número
e barra — e, ao trocar de lado, o anterior volta a preto e o novo fica verde. Ficam **faixas finas
pretas** onde nenhuma janela desenha (entre o nome e a frequência, e entre a frequência e a barra);
as fotos batem pixel a pixel com o que o mapa previa — ver [mapa-da-tela.md](mapa-da-tela.md),
seção 8.

**A v15 não pintava nada por um erro de conversão de endereço**, não de lógica. O `llvm-objdump`
mostra os imediatos em decimal: a `janela()` grava y0 em `b[83466]`, y1 em `b[83470]` e os bytes por
linha em `h[83472]` — que são **`0x1460A`, `0x1460E` e `0x14610`**. Eles tinham sido convertidos
como `0x145CA`, `0x145CE` e `0x145D0` (0x40 a menos; o erro veio do mapa da sessão anterior), e o
gancho lia três variáveis alheias. Na "conferência" do código gerado comparei o rótulo hex com o que
eu acreditava, em vez de comparar 83402 com 83466.

Como foi achado: com a v15 confirmada no rádio (sem `ZONE` no menu), cada elo foi provado de novo —
os 13 sítios nas **seis** imagens do arquivo gravado, a trava (a barra de fábrica só aparece porque
o `layout()` devolve 2 ali), o acesso de 16 bits à RAM de vídeo (`0x01E6C18E` grava 32 bits dentro
dela), o DMA (`0x01E38540` lê de `0x10190` e espera terminar). Sobrou o endereço, e em decimal o
erro ficou óbvio.

Mudou:

- `JAN_Y0`, `JAN_Y1` e `JAN_LINHA` com os endereços certos, anotados com o decimal ao lado.
- **Verificação nova (`4f`)**: a montagem lê os endereços das próprias instruções da `janela()` no
  firmware de fábrica (`0x01E3AD3E`, `0x01E3AD5E`, `0x01E3AD6E`, `0x01E3AD86`) e compara com os
  `#define`. Testada com o valor errado da v15: reprova.
- **14º sítio**: a linha de informações do lado em evidência (`0x01E3B2C4`, a do `H`), y 18 com o
  A e 57 com o B. Ela só é desenhada para o lado expandido, então entra sempre verde.

Mesmo tamanho (1776 B), build reprodutível, zero desvios em remendo, 14/14 sítios nas seis imagens.

## v15 — 24/10 — realce no envio ao LCD ⚠️ **não pintou nada**

**Testado no rádio:** tudo preto, inclusive a barra ao trocar de canal. Causa na v16, acima: três
endereços convertidos errado do decimal. O desenho abaixo (gancho no envio, trava pelo `layout()`)
continua valendo na v16.


Feito depois de mapear o caminho de desenho inteiro, o que explicou com endereço os dois sintomas
que o usuário relatou na v14 (detalhes em [mapa-da-tela.md](mapa-da-tela.md)):

1. **«o verde só sai no fundo da barra de sinal, e só ao mudar de canal com a seta»** — os três
   ganchos da v14 estavam todos em **atualizações parciais**. O redesenho completo, que é o que
   corre ao trocar de A para B, passa por `0x01E3C132` (as três faixas de barra) e pelo desenhador
   de bloco `0x01E3BD04`, que **não tinham gancho nenhum**.
2. **«o quadrado completo continua preto»** — o fundo do texto não vem de `janela()`: cada rotina de
   texto enche a célula de cada caractere com uma cor própria, preta fixa (o `r8 = 0` de
   `0x01E3BAD8`, que vai para o desenhador de glifo `0x01E388F4` e para o vão `0x01E38978`). Trocar
   a cor da janela nunca ia pintar o bloco.

Agora o gancho entra **no envio** (`envia()`, `0x01E3ADBC`) e troca por verde todo pixel preto do
pedaço antes de ele ir para o LCD: fundo da janela, célula de caractere e vão ficam verdes de uma
vez, e o que tem cor própria (texto branco, segmentos da barra) passa intacto. São **13 sítios**,
todas as chamadas de `envia()` dentro das seis funções que desenham a tela de dois VFOs — `call` de
4 bytes trocada no lugar, sem crescer o código.

Qual bloco está em evidência sai da **própria rotina do firmware** (`0x01E3B1A2`), a mesma que ele
usa para posicionar cada linha: ela devolve 2 com o A em evidência, 1 com o B e **0 quando a tela
não é a de dois VFOs** (a de um VFO só grava 4 em `b[0x1533D]`). É isso que mantém o verde fora das
outras telas, que usam essas mesmas funções. Os limites de `y` (89 e 57) saem de três tabelas do
firmware e têm folga larga: 74 contra 91 com o A em evidência, 41 contra 69 com o B.

O gancho **não desenha nada** — só lê os limites que `janela()` já gravou e escreve dentro do
buffer, com trava no tamanho dele; o estouro da v13 é impossível por construção. Conferido no
código de máquina gerado em `0x01E963FA`, instrução por instrução.

Código novo: 1776 B dos 1782 disponíveis. Montagem fecha em `RESULTADO: tudo certo`, build
reprodutível (mesmo sha256 depois de apagar `build/`) e zero desvios caindo dentro de remendo.

## v14 — 24/10 — realce refeito a partir do mapa da tela ⚠️ **não funcionou**

Consertou os dois defeitos da v13, cada um achado no desassemblado:

1. **O reset** era estouro do buffer de vídeo. A v13 pintava o bloco inteiro numa chamada só — 71
   linhas em largura cheia = **22.720 bytes** — e o buffer em 0x10190 tem **10.240** (uma banda de
   160×32). `janela()` não tem trava de tamanho (laço em `0x01E3AD8E`), então o excedente caía em
   cima do buffer de texto, da base global e das flags de redesenho.
2. **O realce não aparecer** era variável errada: `tx_ab` (`0x14B84`, o ajuste gravado na memória de
   dados) em vez de **`b[0x15E04]`**, que é o estado da tela que o próprio firmware consulta.

O gancho passou a entrar em **três janelas** (nome+frequência em `0x01E4B270`, barra de sinal em
`0x01E41468`, número do canal em `0x01E4B314`), sem mexer na geometria.

**Testado no rádio:** liga, troca de A para B e **não reinicia mais** — o estouro estava mesmo
resolvido, e essa parte é definitiva. Mas o realce continuou errado: verde só no fundo da barra de
sinal e só ao mudar de canal com a seta.

**Erro de método, e vale registrar:** os três sítios foram escolhidos a partir de um levantamento
*parcial* da tela, que parou nas atualizações parciais e não chegou ao redesenho completo nem às
rotinas de texto. As duas causas estão na v15, acima.

## v13 — 24/10 — realce do VFO ⚠️ **com problema conhecido**

- **Realce do bloco do VFO selecionado** (gancho em `0x01E41468`, a chamada que pinta a faixa da
  barra de sinal).
- **`ZONE` e `Zone Or Channel` removidos** do menu (a pedido), devolvendo 56 bytes de tabela.
- `setor_livre` saiu do caminho de gravação pelo cabo (a área já é comprovadamente exclusiva).

**Observado no teste, ainda não corrigido:**

1. O rádio liga e troca de A para B normalmente.
2. O realce **não aparece**: os dois lados continuam com fundo preto.
3. **O rádio reinicia** ao trocar de canal com a seta no lado em evidência — em A ou em B, mesmo
   comportamento.

Causa ainda **não determinada**. O levantamento de qual rotina desenha cada parte da tela principal
está em [mapa-da-tela.md](mapa-da-tela.md); enquanto ele não fechar com evidência no
desassemblado, não sai v14.

**Corrigido na v14** (acima). A v12 continua sendo a última versão testada no rádio.

## v12 — 24/10 — cabo de 24 bits, para ler e gravar

- Comando estendido no cabo: **bit 7 do tamanho** liga endereço de 24 bits, bloco de 64 B. Leitura
  alcança o chip inteiro; gravação só em `0xC1000..0xD1FFF`.
- Um gancho só atende os dois comandos, porque `'R'` e `'W'` chamam **a mesma rotina**
  (`0x01E43970`, 0 lê / 1 grava).
- Validado no rádio: `BNK3` lido em `0xC1000`, gravação conferida byte a byte, trava recusando
  endereço fora da área, e os bancos do próprio usuário lidos com as somas batendo.
- Espaço: as marcas dos registros viraram dois `u16` comparados em dois acessos e a marca passou a
  ser deduzida do endereço — 70 bytes economizados, formato gravado inalterado.

**Corrigiu dois erros do v11:** a base do pacote do cabo era `0x16671` (e não `0x16691`), e o
tratador dispara com **4** bytes, então pacote de 5 não funciona — o byte extra ainda envenenava o
comando seguinte.

## v11 — 24/10 — primeira tentativa de leitura estendida ❌

Pacote de 5 bytes. Não funcionou (ver acima) e travava o cabo até desligar o rádio.

## v10 — 24/10 — saída do editor de texto

Gravar nome de banco deixava o `abc` do teclado preso no cabeçalho do menu: faltavam as duas
chamadas que o `CH-NAME` faz ao terminar (`0x01E3ACBA(2)` e `0x01E3C7AA`).

## v9 — 24/10 — barra de sinal no scan

Durante a varredura a barra ficava congelada no nível do último canal achado e o número do canal
descasava do nome: a chamada que avança o canal (`0x01E46EB6`) não marcava a tela como suja.

## v8 — 23/10 — bancos em `0xC1000` (fim da faixa branca)

A imagem de abertura mora em `0x10000` — exatamente onde os bancos estavam sendo gravados. Apagar o
setor do cabeçalho e o do banco 1 comia as primeiras linhas do logo (flash apagada lê 0xFF, que em
16 bits é branco). Bancos movidos para `0xC1000`, mais a trava `setor_livre`, que só grava em setor
apagado ou que já seja nosso.

## v7 e anteriores — 22 a 23/10 — a feature

- 16 bancos, nome de 12 caracteres, canal em quantos bancos quiser, vários ligados ao mesmo tempo.
- Filtro reescrevendo os dois mapas de bits que o firmware já usa, sem alterar nenhuma função.
- Menu hierárquico: `BANK`, `BANK NAME`, `BANK CH`, juntos no lugar dos itens de zona.
- Setas do menu invertidas e 25 textos em inglês revisados (versão `V1.0T`).

Erros corrigidos no caminho, todos com causa identificada:

| Sintoma | Causa |
|---|---|
| Rádio não ligava | O blob foi gravado em claro numa área que o SFC decifra por endereço |
| Valor do item sempre errado | O valor do menu está em `0x14BFC`, não no endereço suposto antes |
| Tela riscada e checksum falhando | `0x10190` é a **memória de vídeo**, não um buffer de setor |
| Faixa branca no logo | Bancos gravados em cima da imagem de abertura (`0x10000`) |

## Diagnósticos de tela — 24/10

Três firmwares de diagnóstico foram gravados para descobrir qual chamada pinta qual pedaço da tela
(o firmware não tem símbolos e as coordenadas mudam conforme o VFO selecionado):

1. **diag** — duas faixas candidatas em cores diferentes. Resultado: a faixa da barra de sinal é
   pintada por `0x01E41468`; a outra candidata não aparece nessa tela.
2. **diag2** — **bootloop**. Pintou 23 "cargas de cor" de uma vez; em `0x01E45ED8` aquele
   registrador zera um buffer de 15 bytes antes de virar cor, e em `0x01E3C156`/`0x01E3C17E` ele
   vira o `x0` da chamada. Recuperado pelo combo PTT2 + atalho.
3. **diag3** — 12 sítios, cada um conferido no desassemblado. Resultado: na tela de dois VFOs, só a
   faixa da barra passa por `janela()`; nome e frequência recebem o fundo dentro da rotina de
   texto.

**Lição que ficou:** mexer em constante no meio do código exige olhar o registrador antes — se a
carga não está colada no uso, ela provavelmente serve para outra coisa.
