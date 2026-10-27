# Histórico das versões

> 🇬🇧 English: [changelog.md](changelog.md)

Datas de 2026. Cada versão foi gravada e testada no rádio; o que quebrou está registrado com a
causa, porque é isso que evita repetir.

## V2.8T — 27/10 — o menu em pastas, com o cartão do 3º item inteiro ✅

A v58 virou a **V2.8T**: `firmware/releases/BJ8300_BANK_v2.8T.anc` (sha256 `11efdc26…`). Testada no rádio: "deu certo
aqui". É a V2.7T com o fundo âmbar do 3º item de cada pasta, e do menu do rádio, inteiro; o menu
`VERSION` mostra `V2.8T` (`0x01E5FD10`). O dono pediu o nome novo depois do teste, e por isso a v58
gravada no rádio ainda mostrava `V2.7T`: fora esse texto, o firmware é o mesmo.

Publicada no [release V2.8T do GitHub](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.8T), junto com o módulo do CHIRP, o gravador e o
driver do cabo. O README aponta para ela, conta a correção no quadro do topo e manteve a V2.7T entre
as anteriores, com o defeito; as telas dos itens e do menu do rádio já tinham sido refeitas com o
cartão inteiro.

## Driver do CHIRP — 27/10 — o nome da V2.8T

- O módulo virou `chirp/bj8300_v2.8TEMPRA.py`, e o modelo no CHIRP, `Baojie → BJ8300_v2.8TEMPRA`. O
  `MODEL_COMPAT` ganhou o `BJ8300_v2.7TEMPRA`, e uma imagem salva com qualquer um dos cinco nomes
  antigos abre neste. O `teste_chirp_real` ganhou a `2c5`.
- Fora o nome, nada mudou: a V2.8T só corrige o desenho do menu.

## v58 — 27/10 — o cartão do 3º item inteiro ✅ (é a V2.8T)

O dono, usando a V2.7T: "sempre no terceiro item de cada menu o fundo âmbar fica pela metade", em
qualquer pasta e também no menu do rádio. A janela de pasta é desenhada em faixas de 32 linhas a
partir de y 16 (16, 48, 80, 112), e cada linha da lista só era desenhada na faixa em que cai o topo
dela (y − 2 a y + 12). O cartão do cursor cobre também a linha do valor, até y + 23: no 3º item ele
vai de y 63 a 87 e atravessa a divisa de y 80, e a faixa de baixo não pintava o que passa dela, a
metade de baixo da linha do valor. Nas outras posições o cartão cabe numa faixa só, ou a faixa de
baixo já desenhava a linha (o 4º item); com a lista rolada, o cursor fica na 5ª linha. Agora a linha
do cursor é desenhada em toda faixa que o cartão toca. O dono confirmou o desenho: o fundo âmbar
cobrindo o item e o valor do "L", quando há valor, agrupa a seleção.

A prévia não pegou porque desenhava em faixas a partir de 0 (0, 32, 64, 96): nela o corte caía no 2º
item (y 64), e passou despercebido nas telas 43 e 45 e nas duas imagens do README tiradas delas. A
prévia agora divide o menu nas mesmas faixas do firmware (a janela de pasta a partir de 16; fora dela
o resumo, o título e cada linha à parte) e ganhou as telas 47b e 51b, com o cursor no 3º item. As
imagens dos itens e do menu do rádio no README foram refeitas.

Testada no rádio: "deu certo aqui". O build passou ("tudo certo"), a pilha é a de fábrica nas três
raízes, os menus ficam em 640 B, e o sha256 é `301a399b…`.

## V2.7T — 26/10 — o menu em pastas ✅

A v57 virou a **V2.7T**: `firmware/releases/BJ8300_BANK_v2.7T.anc` (sha256 `93bf62bb…`). A v56, que já
trazia tudo das v52 a v55, foi testada no rádio ("funcionou", menos o grupo CHANNEL vazio), e a v57 é
ela com essa correção. O menu `VERSION` mostra `V2.7T` (`0x01E5FD10`) desde a v52: o dono pediu o
nome novo junto com as correções.

Publicada no [release V2.7T do GitHub](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.7T), junto com o módulo do CHIRP, o gravador e o
driver do cabo. O README ganhou, em destaque, a seção do menu em pastas, com três telas da prévia (a
escolha do grupo, os itens de um grupo com o valor no "L" e o menu do rádio) e a tabela dos 13
grupos; as duas telas do menu que já estavam lá (a lista e o TX-A/B/C) foram refeitas na janela de
pasta, para não mostrar o menu antigo.

Um relato sem prova: no teste da v56, um texto no formato antigo apareceu uma vez no título e apagou
logo. Não foi achado no desmontado; o dono vai avisar se voltar.

## Driver do CHIRP — 26/10 — o nome da V2.7T

- O módulo virou `chirp/bj8300_v2.7TEMPRA.py`, e o modelo no CHIRP, `Baojie → BJ8300_v2.7TEMPRA`. O
  `MODEL_COMPAT` ganhou o `BJ8300_v2.6TEMPRA`, e uma imagem salva com qualquer um dos quatro nomes
  antigos abre neste. O `teste_chirp_real` ganhou a `2c4`.
- A ajuda do banco escolhido cita `BANKS > NAME` e `BANKS > CHANNEL`, os nomes novos no menu, e os
  textos citam a V2.7T (o TX-A/B/C com o C pede "a V2.6T ou depois").
- O campo CH-C continua: o item saiu do menu do rádio na v54, mas o canal da C ainda é por onde ela
  começa no TRIPLE, e o cabo o grava.

## v57 — 26/10 — o grupo CHANNEL de novo com os itens ✅ (é a V2.7T)

O teste da v56 deu "funcionou", menos o grupo 3, CHANNEL, que na escolha do grupo aparecia sem
nenhum item. Escolhendo o grupo não há cursor (`FORA`, 255), e a conta da rolagem o usava: no
CHANNEL, o único grupo com 7 itens e por isso o único que rola, o início da lista ia além do fim e
nenhuma linha era desenhada. Agora a lista só rola escolhendo o item, e escolhendo o grupo mostra
os primeiros. A prévia já mostrava a tela 41 vazia e passou, porque só se olharam as telas que
deviam mudar: desde então, olha-se toda tela que mudou.

## v56 — 26/10 — a letra maior e o menu do rádio na pasta ✔️ testada: "funcionou", menos o CHANNEL vazio

Os dois pedidos do dono depois de ver a v55:

- **A letra do menu maior**, na janela de pasta e no item aberto (o título e as opções), sem passar
  da barra de rolagem. É a média com a linha do meio também de 2 px (10 de altura: as barras do A,
  E, F, H, P e R saem iguais às de cima e de baixo) e 1 px a mais entre as letras; o que não cabe
  sai na média de antes (o NOISE REDUCTION). A janela passou a 6 linhas de 12 px.
- **O menu do rádio** (FM, AM, SSB) na mesma janela. É o mesmo motor, com uma de duas tabelas de
  fábrica (`0x01E61168` FM e AM, `0x01E6137C` SSB e CW, escolhidas na entrada `0x01E49000`) e quantos
  itens a banda tem (`0x01E3DE36`: FM 2, AM 5, SSB e CW 6). Fica numa pasta só, com `RADIO` e a
  banda no título e sem os pontos dos grupos; o item de memória, que tem o título montado por função
  (`0x01E3D20E`: `FM`, `AM` ou `SSB` e `MEMCH`), não tem o "L". Os números contam de 1, como no menu
  principal, e o EXIT da lista volta ao rádio, como de fábrica.
- A pilha: com os dois textos da janela em 28 B cada, a interface ia a 744 B, 4 acima dos 740 de
  fábrica. Eles ficaram com 12 e 20 B, e ela voltou aos 740.

## v55 — 26/10 — o menu do jeito que o dono desenhou

Depois de ver os grupos ("menu aprovado"), o dono mudou o uso:

- **A janela de pasta também dentro do grupo.** Os itens numerados; o do cursor num cartão, com a
  seta do OK, e o valor dele logo embaixo, num "L" verde que o cursor não seleciona. Só o item do
  cursor mostra o valor, e os de ação (RESET, MEMCH, DELCH e o CHANNEL dos BANKS) não têm "L". O
  valor sai das linhas que o motor já montou (`0x01E487BC`): a marcada como gravada (`02 10`) ou,
  sem marca (o nome, o OFFSET, o VERSION e o BFO), a primeira.
- **OK abre o item já editando** (o OK de fábrica); gravar ou cancelar volta à janela, no mesmo
  item; EXIT volta aos grupos, no grupo.
- **O VERSION só para ler**: o OK o abre sem cursor, o OK e os números não fazem nada com ele aberto,
  e só o EXIT sai.
- **O BANK e o CHANNEL dos BANKS marcam vários**: cada OK grava e a edição continua; o EXIT sai. Um
  desvio logo depois do set (`0x01E49A86`) zera o estado da recepção, como o de fábrica faz depois
  de mudar um ajuste, e segue para o redesenho sem fechar a edição.
- **A rolagem.** Com mais linhas que a janela, a lista rola para o cursor e o valor ficarem à vista;
  as setas de cima e de baixo só aparecem quando há o que rolar, e entre elas fica um risco com a
  barra da posição, mais grossa que ele. Os pontos dos grupos ficam.
- O DELCH continua apagando um canal por vez.

## v54 — 26/10 — sem o item CH-C

Pedido do dono: desde a v49 a C é um lado com VFO, como o A e o B, e o item que escolhia o canal
dela não fazia mais sentido. Sem ele, um canal da C vazio (apagado, ou nunca escolhido) a deixaria
fora do alcance da tecla A/B; agora, no TRIPLE, a tecla sempre chega à C: com o canal dela vazio,
vai ao próximo que existe (`canal_existe`, pelo número do canal; o próximo canal de fábrica conta
dentro da zona no modo zona), e sem nenhum canal a C vai em VFO. O grupo WATCH ficou com TTR,
TX-A/B/C e WATCH, e o menu com 55 itens. O canal da C continua no registro do WATCH, e o CHIRP ainda
o grava pelo cabo.

## v53 — 26/10 — os grupos do menu (nunca gravada; foi direto para a v54)

Pedido do dono: agrupar os itens por assunto, sempre em ordem alfabética, de um jeito que leigo
entenda, e mostrar quando o item é um grupo.

- **13 grupos**, cada um com os itens em ordem alfabética: AUDIO, BANKS, CHANNEL, DISPLAY, DTMF,
  KEYS, MEMORY, REPEATER, SCAN, SYSTEM, TONES, TX e WATCH (a tabela está no README). O BANK NAME e o
  BANK CH viraram NAME e CHANNEL, dentro de BANKS.
- **Dois níveis no mesmo motor.** A tabela do menu, agora com 80 vagas, traz os grupos e depois os
  itens de cada um; entrar num grupo é pôr a tabela dele no estado do menu (`0x14BF4`), e voltar é
  pôr a dos grupos. Cada grupo é um item de lista sem get nem set, com os nomes dos itens nas
  opções. Os ganchos ficam na entrada do menu (`0x01E4CBB2`, que reabre no grupo de onde se saiu),
  no OK (`0x01E4C924`), nos números (`0x01E4B9FA`, contando de 1, como a tela mostra) e no EXIT fora
  da edição (`0x01E4D54A`, que volta aos grupos).
- **A janela de pasta**, em âmbar, na escolha do grupo: a pasta, o número e o nome do grupo na barra
  de título, os itens dele no corpo e um ponto por grupo embaixo. Dentro do grupo, o título do item
  ganhou o chip `G-k` (grupo e item).
- O mapa está na [engenharia reversa, seção 21](engenharia-reversa.md#21-o-menu-em-pastas-v53-a-v57)
  e no [mapa da tela, seção 15](mapa-da-tela.md#15-a-janela-de-pasta-do-menu-v53-a-v57).

## v52 — 26/10 — o nome vindo do CHIRP, o editor só em maiúsculas e a tensão do 0

Os três pedidos do dono depois da V2.6T:

- **O nome gravado pelo CHIRP abria com `??` no fim** (o editor mostrava `SBST RADIO??`, 12/12). O
  nome tem sempre 12 B na memória; o que sobra, o rádio completa com 0 (`0x01E3C7BE`) e o CHIRP com
  `0xFF`. A exibição para no primeiro dos dois, mas o get do CH-NAME (`0x01E3C732`) media o nome com
  o strlen da ROM (`0x021127C0`, chamado em `0x01E3C784`), que só para no 0, e abria o editor com os
  `0xFF` como letras. As duas chamadas da ROM no get (o strlen e a cópia, `0x01E3C792`) agora passam
  por `nome_tamanho` e `nome_para_editor`: as letras até o primeiro 0 ou `0xFF`, e zero até o fim do
  buffer, com que o `%15s` de fábrica e a inserção de letra (`0x01E49664`) contam. O BANK NAME, o
  único outro editor de nome do firmware (varrido o app inteiro), usa a mesma conta.
- **O editor de nome só com maiúsculas e números**, abrindo em `ABC`. A tecla `#` ciclava `abc` →
  `ABC` → `123`; o "r0 = 0" de `0x01E4A5AA` virou "r0 = 1", e o ciclo ficou `ABC` ↔ `123`. Os dois
  editores abrem em `ABC`.
- **O 0 segurado** (a ação 33 das teclas, `0x01E4CE96`) mostrava a caixa de tensão de fábrica, o
  `VOLTAGE` no estilo antigo. O envio dela (`0x01E4CF1C`) passa pelo `gancho_tensao_tecla`, que
  desenha o cartão da partida por cima da tela; depois o firmware espera 1 s e redesenha tudo, como
  antes. O texto ` VOLTAGE` saiu dos textos traduzidos, porque só essa ação o usava.
- O `VERSION` passou a mostrar `V2.7T`.

## V2.6T — 26/10 — três frequências de uma vez ✅

A v51 virou a **V2.6T**, a terceira versão estável: `firmware/releases/BJ8300_BANK_v2.6T.anc` (sha256
`9ef7cf3e…`). Testada no rádio: "agora funcionou tudo". O menu `VERSION` mostra `V2.6T`
(`0x01E5FD10`). A V2.5T não chegou a ser lançada (abaixo): o teste da v50 mostrou a TX na C sem a C em
evidência, e a correção saiu com o nome novo.

Publicada no [release V2.6T do GitHub](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.6T), junto com o módulo do CHIRP, o gravador e o
driver do cabo. O README ganhou a seção do TRIPLE, com seis telas da prévia (o A recebendo, a C
recebendo pela escuta, a C em evidência, a C em VFO, a TX na C e o menu TX-A/B/C), e o que mudou
desde a V0.12 passou a contar o WATCH, o lado C, o TX-A/B/C, o MIC GAIN, a luz no espectro e o TTR.

No uso, o dono achou a tela e os comandos da v49 um pouco mais lentos que na v45 (a tela de três
lados e o desenho ao começar a receber), mas aceitável. Pelo código, os caminhos do desenho e da
recepção são os mesmos da v45; o que pesa mais é a tecla A/B indo à C ou voltando dela, que desde a
v49 grava o setor dos VFOs, o registro de VFO do B e os ajustes. Fica para observar no dia a dia.

## Driver do CHIRP — 26/10 — o nome da V2.6T

- O módulo virou `chirp/bj8300_v2.6TEMPRA.py`, e o modelo no CHIRP, `Baojie → BJ8300_v2.6TEMPRA`. O
  `bj8300_v2.5TEMPRA.py` só existiu no repositório (a V2.5T não foi lançada), mas uma imagem salva com
  ele abre neste também: o `MODEL_COMPAT` registra os três nomes antigos. O `teste_chirp_real` ganhou
  a `2c3`.
- O texto de ajuda do TX-A/B/C e o aviso de rádio sem o firmware dos bancos citam a V2.6T.

## v51 — 26/10 — a terceira em evidência enquanto o PTT está apertado ✅ (é a V2.6T)

Testada no rádio: "agora funcionou tudo". O teste da v50 mostrou que, com o TX-A/B/C em C, a TX saía
na frequência da C, mas a C não ia para evidência como o A e o B vão. Agora vai, e continua sem
gravar nada:

- **Na ida do PTT**, para a terceira, `gancho_tx_abc` põe o lado B em evidência com a troca de
  fábrica (`0x01E40E52(0)`, a mesma do TX-A/B), se o A estava nela, e marca `b[0x15384]`: o bit 7 é a
  terceira, o bit 5 diz que a troca foi nossa.
- **No início da TX**, depois do desarme, `gancho_tx_desarma` põe a terceira na corrente do lado B e a
  decodifica sem inverter o lado, como o carregador de canal faz com o lado em evidência; põe a
  potência dela no lado B e refaz o registro de tela do B com os dados dela (`0x01E39128`:
  frequências, tons, potência e o nome pelo canal corrente). Marca o bit 6 e guarda a potência do B
  nos bits 0..3. As vagas do lado B (o modo, o canal e a sombra) ficam como estavam.
- **A tela** mostra o que o PTT pôs no lado B: com o bit 6, `watch_tela()` inverte o `c_no_b` (o
  cartão do B ganha a letra e o MDF da terceira) e dá, nos bits 16..30, o canal dela para o cartão; e
  `terceira()` passa a ser quem saiu do lado B, lida das vagas dele (o VFO pela sombra do lado em
  evidência, `0x15EA4`). O cartão mostra a terceira transmitindo, com o medidor vermelho, como o A e o
  B. A faixa vermelha da v50 saiu.
- **Na volta** (fim da TX e TX recusada), `gancho_tx_volta` zera a marca, devolve a potência, a sombra
  do B à corrente (`0x01E3FD9E`) e o registro de tela do B, e volta ao A com a troca de fábrica se foi
  ela que o tirou de evidência.
- O rodízio e a TX leem a terceira pela mesma rotina (`le_terceira`); só o rodízio decodifica com o
  lado invertido.
- O build passou ("tudo certo"), a pilha é a de fábrica nas três raízes (o `gancho_tx_abc`, que agora
  faz a troca, chega a 504 B no sítio), o ponto mais fundo dos menus ficou em 640 B, e as 43 telas da
  prévia saem iguais; a da TX na C (39c) mostra a C no cartão.

## V2.5T — 26/10 — ❌ nunca lançada, superada pela V2.6T

A v50 seria a V2.5T: foi commitada com o nome (`firmware/releases/BJ8300_BANK_v2.5T.anc`) e o README
já apontava para o release, mas o release ficou esperando o teste no rádio. O teste mostrou que a TX
na C não punha a C em evidência; a correção (v51) saiu como V2.6T, e o `.anc` da V2.5T saiu de
`firmware/releases/`.

## v50 — 26/10 — o TX-A/B/C e o selo C no menu ✔️ testada: a TX saía na C, sem a C em evidência

Os dois pedidos do dono depois de usar a v49.

- **TX-A/B/C.** O TX-A/B de fábrica (item 44; `b[0x14B7F]`, o `0x9013` dos ajustes: 0 OFF, 1 A,
  2 B) escolhe em que lado o PTT transmite, e só vale com a escuta ligada (TDR/TTR). Na ida do PTT
  (`0x01E43E8A`), se o lado escolhido não está em evidência, o firmware troca os lados
  (`0x01E40E52(0)`, que não grava nada) e marca `b[0x15384] = 1`. No fim da TX (`0x01E473DC`) e na TX
  recusada (`0x01E411A6`), com a marca, ele destroca. O get e o set do item só copiam o byte, e nada
  mais o lê (rastreio de base + deslocamento no desmontado inteiro). O item ganha o C, vira
  **TX-A/B/C** (quatro opções, título novo) e continua no mesmo byte.
- **A TX na terceira, só pela RAM.** A letra que não está em nenhum dos dois lados (a C, ou o B
  quando a C está no lado B) é a terceira. Levá-la ao lado B a cada PTT, como a tecla A/B faz,
  gravaria dois setores na ida e dois na volta. Por isso a TX sai dela como o rodízio a ouve:
  - a leitura do TX-A/B na ida do PTT (`0x01E43F92`, "r0 = 1279; r0 = b[r4+r0]") vira
    `call gancho_tx_abc` + nop, que devolve ao trecho de fábrica o lado (1 A, 2 B) ou 0 e, para a
    terceira, marca `b[0x15384]` com o bit 7;
  - o início da TX (`0x01E41022`) desarma a dupla escuta (`0x01E3FDE4`, em `0x01E410EA`) e só
    depois lê a frequência de TX decodificada (`b[0x15341]`, escrita pela decodificação
    `0x01E38DE6`). Ali, `gancho_tx_desarma` põe a terceira na corrente e a potência dela no lado em
    evidência, porque a TX tira a potência de `b[0x15E30 + lado em evidência]` (`0x01E475A2`). A de
    cada canal ou VFO é o nibble baixo do byte 14 do canal cru ou do 16 do VFO (`0x01E3A6D0`,
    `0x01E3A682`). A do lado fica guardada na marca;
  - as duas destrocas (`0x01E411AE` e `0x01E473EE`) passam por `gancho_tx_volta`: com a terceira na
    corrente, devolvem a potência e o lado em evidência à corrente (`0x01E3FD9E`) e refazem a tela;
    sem a marca, a destroca de fábrica. O firmware zera a marca logo depois delas, e a partida
    também (`0x01E56B3A`).
- **Quem ganha.** Fora do TRIPLE o C não vale (fica como OFF). Se o PTT já trocou de lado antes do
  TX-A/B (a marca já posta), ele ganha, como no firmware de fábrica.
- **Na tela**, a TX na terceira deixava o cartão do lado em evidência como estava e pintava a faixa
  da terceira de vermelho; na barra, TX. No rádio, a faixa não apareceu vermelha, e o dono pediu a C
  em evidência, como o A e o B: a v51 refez assim.
- **O selo do resumo no topo do menu e do scan QT** mostrava B com a C em evidência: o resumo
  desenhava a letra do lado (`'A' + lado`), e não a de `LETRA()`, que o cartão e as faixas já usam.
- O build passou ("tudo certo", com a `4r` conferindo os quatro sítios novos), a pilha é a de
  fábrica nas três raízes, o ponto mais fundo dos menus foi de 636 para 644 B (abaixo dos 740 B de
  fábrica), e as 43 telas da prévia saem iguais, com duas novas (a TX na C e o menu TX-A/B/C com a
  C em evidência).

## Driver do CHIRP — 26/10 — o Mic Gain, o TX-A/B/C e o nome da V2.5T

- **O módulo com o nome da versão.** O `bj8300_v2.0TEMPRA.py` virou `chirp/bj8300_v2.5TEMPRA.py`, e o
  modelo no CHIRP, `Baojie → BJ8300_v2.5TEMPRA`. O `directory.MODEL_COMPAT` registra os dois nomes
  antigos (`BJ8300_v1.0TEMPRA` e `BJ8300_v2.0TEMPRA`), e as imagens salvas com eles abrem neste. O
  `teste_chirp_real` ganhou a `2c2` (a imagem com o nome da V2.0T).
- **Mic Gain** no BJ8300_v2.5TEMPRA Specific, logo depois do Noise reduction: HIGH, MID e LOW, no
  byte `0x9033`, o mesmo do item MIC GAIN do rádio. Um valor fora da faixa aparece como HIGH, como o
  firmware o lê. Testes 10p a 10r do `teste_chirp_real`. O texto de ajuda diz os valores (31, 25 e
  19) e não a escala em dB, que vem da folha de dados de um chip que o firmware não prova.
- **TX-A/B/C** (firmware v50) no VFO A/B/C Channel, logo depois do TTR, com que ele funciona: o TX-A/B
  da base (`0x9013`, 2 bits) com o C. Testes 10s a 10u.

## Driver do CHIRP — 26/10 — VFO A/B/C Channel, o VFO C, o NR e a Voice Privacy do A

- **Settings em outra ordem, a pedido do dono.** O VFO A/B Channel virou **VFO A/B/C Channel** e,
  com o FM/AM/SSB Modulation, saiu de dentro do BJ8300_v2.0TEMPRA Specific para o topo da árvore. O
  WATCH e o **TTR** (o TDR, que saiu do Specific) vêm primeiro. Os três MDF ficam cada um no bloco do
  seu lado, sempre em 2º e com o mesmo nome (`Memory Display Format A (MDF-A)`…), e saíram do Basic
  Settings. O grupo Watch deixou de existir.
- **VFO C (firmware v49).** O bloco VFO C Channel tem os mesmos campos do A e do B (Frequency,
  Tuning Step, TX Power, Bandwidth, os tons, Voice Privacy, Offset, Offset Direction, S-CODE,
  Scramble e RX Modulation), mais o Work Mode, o MDF-C e o CH-C. O formato do VFO é o do próprio
  driver de base (o `struct vfo` dele), lido sobre o bloco novo do `.img`.
- **No `.img`, o bloco do VFO C (128 B, dois blocos do cabo) vai depois do do WATCH.** O download o
  lê de `0xD3000`. Sem ele, o VFO C mostra o do B, que é como o rádio começa a C, e a primeira
  mudança cria o bloco. O upload só o grava quando o VFO da C muda e o lê de volta; com o firmware
  da v46 à v48, avisa que precisa da v49.
- **Com a C no lado B no rádio,** o modo e o VFO dela estão nas vagas do B, e o bloco guarda os do B.
  O VFO C mostra os da C e o VFO B mostra o B guardado, os dois só de leitura, e o upload não toca no
  bloco.
- **Noise reduction lido do byte certo.** O driver de base o lê em `0x9031`, um byte que o BJ-8300
  não usa, e o CHIRP mostrava desligado com o NR ligado. O cabo monta o bloco `0x9000` da RAM, e o
  item NOISE REDUCTION guarda em `b[0x14BDE]`, que cai em `0x9032`. Conferidos os outros 30 itens do
  menu que moram no bloco: todos estão onde o driver de base os põe.
- **Voice Privacy do VFO A gravada.** No driver de base ela é um ajuste sem callback, e o
  `set_settings` dele só grava MemSettings e callbacks: o valor nunca chegava ao rádio (o do B é um
  MemSetting). Provado rodando o driver de base: com ENCRY2 aplicado, o campo fica em 0.
- Testes novos: no `teste_chirp_real`, do 10c ao 10c6 (a árvore e o NR) e do 10k ao 10o (o VFO C, o
  modo, a C no lado B e a Voice Privacy); no `teste_clonagem`, do 7 ao 7f (o cabo, com a janela da
  v49 e a da v46 à v48).

## v49 — 26/10 — o VFO C, a luz acesa no espectro e o MIC GAIN ✔️ testada

Testada no rádio; com os consertos da v50 e da v51, é a V2.6T. A pedido do dono, a C passa a ter as
mesmas opções do A e do B. Até a v48 ela era só um canal da memória (o CH-C); desde a v49 é um lado
como os outros: modo (VFO ou canal), VFO de 32 B e canal.

- **Onde mora.** O modo da C, e o do B guardado, ficam no byte 9 do registro do WATCH, que era
  reservado e gravado em 0 (canal, como até a v48). Os VFOs ficam no setor `0xD3000` (`struct vfc`,
  "VFC1", 70 B: o da C e o do B guardado), no formato do registro de VFO de cada lado. Ficam fora do
  registro do WATCH porque a tela B lê aquele a cada desenho, e 64 B a mais na pilha de quem o lê
  passariam da de fábrica.
- **A tecla A/B, do B para a C,** guarda o modo, o canal e o VFO do B e põe os da C nas vagas do
  lado B: o nibble do B em `0x14B86`, o canal do lado em `h[0x15E10]` e o registro de VFO do B
  (`0x14E62`, pelo VFO corrente e `0x01E3A4FE(1)`, que grava o log `0x8000`). Depois remonta os dois
  lados pela rotina da tecla VFO/MR (`0x01E3A812`) e grava os ajustes (`0x01E3AB36`). Da C para o A,
  faz o contrário e a troca de fábrica. Enquanto a C está no lado B, tudo o que mexe nele (VFO/MR,
  setas, digitação, STEP, TXP, tons…) é o firmware de fábrica mexendo nela. Até a v48 era o caminho
  das setas, que só servia com o B e a C em canal.
- **O rodízio do TRIPLE**, com a C em VFO, põe os 32 B do VFO dela no VFO corrente (`0x15E80`), como
  o carregador de fábrica faz (`0x01E3A5E8` também só copia os 32 B do registro). A faixa da C mostra
  a frequência com "VFO".
- **A luz da tela fica acesa no espectro.** O laço do sinal apaga a luz quando a contagem do ABR
  (`h[0x14F80]`) chega a 0, por `0x01E394D6(0)` em `0x01E46578`, o único lugar que a apaga. O gancho
  só repassa essa chamada fora do modo 20. Nada é gravado: ao sair do espectro, ou ao religar o rádio
  (que nunca parte no modo 20; só a entrada por tecla o grava, `0x01E4CF7E`), o ABR volta a valer.
- **O cabo grava também o setor dos VFOs:** a janela vai até `0xD3FFF`.
- **O MIC GAIN**, pedido do dono depois de ouvir o ganho do microfone mudado pelo cabo (o `0xF210`
  da calibração de 30 para 18 e para 31: "mudou bastante"). Item novo logo depois do NOISE
  REDUCTION, com HIGH, MID e LOW, que põem 31, 25 e 19 no ganho que o firmware escreve no
  registrador `0x7D` a cada transmissão. O nível fica no `0x9033` do bloco de ajustes, um byte que o
  firmware não usa. Vale na partida (depois da cópia da calibração, que fica intacta no chip), no
  menu, e também no retorno aos ajustes de fábrica e no upload do CHIRP: o `gancho_ajustes` entra na
  gravação dos ajustes desses dois caminhos (uma chamada em `0x01E4845A` e um `goto` em
  `0x01E43A50`). Detalhes na [seção 19](engenharia-reversa.md#19-o-ganho-do-microfone-e-o-mic-gain-v49)
  da engenharia reversa.
- O build passou ("tudo certo"), a pilha é a de fábrica nas três raízes, o ponto mais fundo dos
  menus foi de 628 para 636 B (abaixo dos 740 B de fábrica) e as 85 telas da prévia saem iguais, com
  uma nova (a C em VFO).

## v48 — 26/10 — TTR no lugar de TDR ⏳ superada pela v49, nunca gravada

A pedido do dono, o item TDR do menu virou **TTR** (Transmitter Triple Receiver), porque no TRIPLE a
escuta é tripla. A cadeia "TDR" é única no `app.bin` (`0x01E5A782`). Apontam para ela o item 7 da
tabela de menus e a outra lista de itens de fábrica (`0x01E5E697`), e as duas passam a dizer TTR.

## v47 — 26/10 — a caixa do DTMF nova e o carimbo do fim ⏳ superada pela v49, nunca gravada

- **A caixa do DTMF da tela B nunca tinha aparecido, da v38 à v46.** O `gancho_dtmf` perguntava se
  a tela de dois VFOs estava ativa, e a rotina que responde (`0x01E3B1A2`) devolve 0 com o modo da
  tela acima de 5 (`0x01E3B1B4`). A tecla # (caso 24 do `tbh` das ações, `0x01E4BE28`) grava o 7 no
  modo (`0x01E4CD06`) antes de abrir a caixa. Por isso saía sempre a de fábrica, com o título em
  chinês virado lixo desde a v37. Agora o gancho desenha quando o modo é 7.
- **Os caminhos, provados no desmontado.** O `0x01E4DA88` só é alcançado pelo caso 24 (nos modos 0
  e 8; o 8 é a sessão do cabo) e pela digitação da frequência do rádio (só no modo 2, que segue com o
  envio de fábrica). O `0x01E4A93C` só é alcançado pelo caso do modo 7. A prévia ganhou o DTMF em
  SINGLE e no TRIPLE.
- **O carimbo do fim.** A v46 não gravava por cima da v45 (abaixo). O `build.sh` liga duas vezes e
  grava o CRC16 do código do fim da imagem no `carimbo_fim`, dentro do `app.bin`. Assim toda
  mudança no fim muda também o CRC do `app.bin`, e o carregador a grava. O `conferir.py` confere
  (`2i`).

## Driver do CHIRP — 26/10 — WATCH, CH-C e MDF-C

- **Settings → Watch**, com os três itens do menu do rádio: o WATCH (SINGLE, DUAL e TRIPLE), o
  canal da C (CH-C, de 1 a 960) e o MDF-C (Name, Frequency e Channel, como o MDF-A e o MDF-B).
- **No `.img`, o bloco do WATCH (64 bytes) vai logo depois dos bancos.** O download o lê junto com
  os bancos, pelo comando estendido. Uma imagem salva antes ganha o bloco apagado, e o upload dela
  não toca no WATCH do rádio.
- **O upload grava o bloco só quando ele muda, e o lê de volta.** O firmware até a v45 ignora a
  gravação fora dos bancos e responde ACK assim mesmo. Nesse caso o upload termina com um aviso de
  que o WATCH precisa da v46, depois de já ter gravado canais, ajustes e bancos.
- **O que é do rádio vem do rádio.** Se a C está no lado B, e o canal que o B tinha, o upload tira
  do registro que está no rádio na hora, e não da imagem: uma tecla A/B apertada entre o download
  e o upload não é desfeita. Da imagem vêm só o WATCH, o CH-C e o MDF-C.
- **Com a C em evidência no rádio, o CH-C fica só de leitura**, porque o rádio grava por cima dele o
  canal em que a C estiver quando sair do lado B.
- **SINGLE desliga o TDR no upload**, a mesma regra do firmware.
- Testes novos: no `teste_chirp_real`, do 10 ao 10j (o registro na imagem e a página Watch); no
  `teste_clonagem`, o 1h e do 6 ao 6h (o cabo, com um rádio simulado com a janela da v46 e com a
  de antes).

## v46 — 26/10 — o cabo grava o registro do WATCH ❌ o rádio com a v45 não a gravou

**O carregador não a gravou: no gravador, 4 a 5% e pulava para 100%, e o rádio voltava na v45.**
É o mesmo de quando a v34 foi gravada por cima dela mesma (25/10). A v46 só mudava 1 byte do
código do fim da imagem (`0x01E96124` em diante), que nenhum CRC de dentro da imagem cobre: o do
`app_area_head` vai só até o fim do `app.bin`. Todos os CRCs saíram iguais aos da v45, e o
carregador só regrava o que tem CRC novo (v30). Todas as versões que gravaram mudaram o
`app.bin`; a v27, que também só mudava o fim, foi gravada por cima da v28. A v46 saiu de
`firmware/releases/`; da v47 em diante, o carimbo do fim resolve.

- **A janela de gravação do cabo inclui o setor do WATCH.** A gravação estendida só aceitava
  `0xC1000..0xD1FFF` (o cabeçalho e os 16 bancos), e o registro do WATCH fica logo depois, em
  `0xD2000`: o CHIRP não tinha como gravá-lo. O `gancho_cabo` passa a aceitar até `0xD2FFF`, um
  setor a mais na mesma comparação (no compilado, `if (r1 > 17)`).
- **Fora da janela, nada muda:** o bloco é ignorado e o ACK sai do mesmo jeito, porque quem o manda
  é o tratador de fábrica, depois do gancho. É por isso que o driver lê o bloco de volta depois de
  gravar.
- Do resto, a v46 é a v45: a imagem tem o mesmo tamanho, a pilha é a de fábrica nas três raízes, e
  as 78 telas da prévia saem iguais.

## v45 — 26/10 — a tela acompanha a tecla A/B até a C ✔️ testada

Testada no rádio: "agora tá ok". O teste da v44 mostrou que, no TRIPLE, a segunda tecla A/B
seguida deixava o B na tela. Por dentro a C já estava no lado B: mudando o canal, ela aparecia em
evidência, no canal seguinte ao dela, e funcionava.

- **A causa.** O caminho das setas (`0x01E40B9E`), que põe a C no lado B, carrega o canal e não
  redesenha. Quem o chama é que redesenha: a seta (`0x01E453E0`), a digitação do número
  (`0x01E4DDD0` → `0x01E41486`, os dois lados) e o retorno à tela principal (`0x01E4BDBE`, logo
  depois de `0x01E40B9E(0, 1)`, chama `0x01E3C290`). A troca de lados de fábrica (`0x01E40E52`)
  termina na mesma `0x01E3C290` (`0x01E40EF4`), e por isso a volta ao A aparecia.
- **A correção.** Depois de pôr a C no lado B, o `gancho_ab` chama `0x01E3C290`, que limpa, refaz a
  barra e a tela inteira (`0x01E3C132(0)`). O redesenho é o inteiro, e não o dos dois lados, porque
  no TRIPLE a C em evidência muda a disposição: o cartão desce para y 59 e o B vira faixa em 38. O
  mesmo vale no caminho raro em que o B volta ao lado B depois de a tecla ir do A para o B.
- **Por que só a tela.** Na v44 a tecla já levava a C ao lado B, mudava o canal dela e voltava ao A
  com o B no lugar. O que faltava era o desenho.

## v44 — 26/10 — a C como um lado a mais ✔️ testada, a tela não acompanhava a tecla A/B

O teste da v43 mostrou o rodízio ouvindo as três frequências, e o dono pediu três coisas. A tecla
A/B tinha de chegar à C, e a C tinha de funcionar "igualzinho" à A e à B. O MDF-C tinha de vir na
sequência do MDF-A e do MDF-B. E as três tinham de ter o mesmo tempo de escuta.

- **A tecla A/B passa por A → B → C → A.** A tecla é o código 18 sem dígitos: o caso 18 do `tbh`
  das ações (`0x01E4BE28`) vai a `0x01E4CC10` e a `0x01E4D44A`, que faz `r0 = 1` e chama a troca
  de lados de fábrica (`0x01E40E52`) em `0x01E4D44C`. Essa chamada passa pelo `gancho_ab`:
  - A → B: a troca de fábrica;
  - B → C: guarda o canal do B e põe o da C no lado B pelo caminho das setas (`0x01E40B9E(0, 1)`:
    carrega o canal por `0x01E3A650` e grava o canal do lado e o log da flash por `0x01E3A42A`).
    Assim a C transmite, muda de canal pelas setas e pela digitação, e continua lá depois de
    desligar;
  - C → A: guarda o canal em que a C ficou (o CH-C passa a valer esse), devolve o B ao lado B e faz
    a troca de fábrica.

  Só com o B num canal: com o B num VFO, a tecla fica no A e no B. Mudar o CH-C com a C em
  evidência carrega o canal novo nela.
- **O mesmo tempo para as três.** Na v43 a ida ao outro lado alternava entre o B e a C, e o ciclo
  era A, B, A, C: o lado em evidência ficava com metade do tempo. Agora o gancho entra na volta
  (`0x01E481CC`, a chamada de `0x01E3FD9E` logo depois de `b[0x15378] = 0`). Voltando do outro lado,
  a dupla escuta vai à terceira em vez de voltar ao lado em evidência, e só na volta seguinte a de
  fábrica o restaura. O ciclo vira A → B → C → A, cada um com os mesmos 50 tiques do firmware. Com a
  C em evidência, a terceira é o B.
- **O MDF-C logo depois do MDF-B.** A montagem ganhou os itens que entram depois de um item de
  fábrica (`ITENS_DEPOIS` do `montar.py`). O WATCH e o CH-C continuam antes do BANK.
- **A tela com a C em evidência:** A e B em faixas em cima e o cartão da C embaixo (y 59). Cada um
  tem a letra e o lugar dele: o cartão em 18, 39 ou 59, conforme a letra, e as faixas de 20 em 20,
  a partir de 18 acima do cartão e de 88 abaixo.
- **O registro do WATCH ganhou o canal do B e se a C está no lado B** (14 bytes). O de 10 bytes da
  v43 deixa de valer, e o rádio volta ao DUAL: depois de gravar, o WATCH, o CH-C e o MDF-C se
  escolhem de novo.
- **O código do WATCH e do TRIPLE foi para a faixa livre** (`.livre.bancos`), porque a área do fim
  da imagem (`.text`, 3260 bytes) tinha estourado por 24. Ela fica com 2248.

## v43 — 26/10 — TRIPLE: a C vigiada pela dupla escuta ✔️ testada

Testada no rádio: "ele ouviu as 3 frequências". Os dois problemas do teste (a tecla A/B não chegava
à C, e o MDF-C fora da sequência) e o pedido de tempo igual foram para a v44.

- **TRIPLE:** a C é um canal da memória, escolhido no item novo **CH-C** (de 1 a 960, como o
  BANK CH; canal vazio com ` -`), e aparece como o **MDF-C** pede: nome, frequência ou número, com
  as opções do MDF-A.
- **A tela:** o lado em evidência no cartão de sempre, e os outros dois em faixas de 19 px, cada uma
  com a letra, o texto e o canal. Recebendo, a faixa fica em âmbar escuro com o texto âmbar, como a
  etiqueta do RX.
- **O rodízio** entrava na ida da dupla escuta ao outro lado (`0x01E4826E`, a chamada da
  decodificação `0x01E3900A`): uma vez o B, outra a C, com o canal cru da C (20 B) lido da memória
  como o carregador de canal de fábrica lê. O estado fica em `b[0x14BD0]`, que nada no firmware lê
  ou grava fora das cópias do bloco de ajustes.
- **O registro do WATCH num setor só dele**, `0xD2000`, logo depois dos 16 bancos e fora da janela
  que o cabo grava. Marca `WTC1` e soma, como os bancos. Sem registro válido, DUAL. Na v42 o valor
  ficava no byte reservado do cabeçalho dos bancos, que não cabia o canal da C.
- **A tabela do menu saiu do lugar da original** para a faixa livre, num vetor de 64 vagas: a
  original tem 54, e o WATCH, o CH-C e o MDF-C não cabiam. O ponteiro em `0x01E4CBB2` passa de
  "r0 = 13368; r5 = r6 + r0" para "r5 = menu_tabela", nos mesmos 6 bytes. A original fica intacta.

## v42 — 26/10 — WATCH: SINGLE e DUAL (não gravada; a v43 a inclui)

- **O item WATCH** (SINGLE, DUAL e TRIPLE), no molde do TDR, antes do BANK. O nome e as opções
  foram escolhidos pelo dono.
- **SINGLE:** só o lado em evidência, da barra ao pé da tela. A frequência em 3x (em 2x se não
  couber) com os dois últimos dígitos em 2x, as etiquetas, um medidor de 12 px e, com deslocamento,
  a frequência de transmissão embaixo.
- **A dupla escuta segue a regra do dono.** No SINGLE fica desligada, e só a frequência mostrada
  recebe: o WATCH em SINGLE zera o TDR, o set do TDR (item 7, agora `tdr_grava`) o mantém em 0 e a
  partida o zera de novo, para o caso de um upload do CHIRP o ter religado. No DUAL e no TRIPLE, o
  TDR ligado escuta todas, e desligado só a que está em evidência.
- **Uma armadilha de pilha.** Chamar as etiquetas de dentro do cartão do SINGLE somava os quadros, e
  a pilha passava 28 bytes da de fábrica. Elas vêm da cena, como no DUAL. A vista entrou no byte que
  sobrava dos dígitos, e a estrutura do estado não cresceu.

## V2.0T — 26/10 — a tela nova ✅

A v41 virou a **V2.0T**, a segunda versão estável. É o mesmo arquivo, byte a byte (sha256
`57dcf09b…`), agora com o nome da versão: `firmware/releases/BJ8300_BANK_v2.0T.anc`. O menu
`VERSION` do rádio já mostra `Firmware: V2.0T` desde a v38.

Publicada no [release V2.0T do GitHub](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v2.0T), junto com o módulo do CHIRP, o gravador e o driver
do cabo. O README ganhou a seção "A cara nova", com quatro telas tiradas da prévia (a principal, o
espectro, o rádio e o menu BANK), e o que mudou desde a V0.12 passou a contar a tela nova, o
espectro, o rádio, o roger COIN e o que saiu (as vozes e o idioma chinês).


**O módulo do CHIRP com o nome da versão.** A pedido do dono, o `bj8300_v1.0TEMPRA.py` virou
`chirp/bj8300_v2.0TEMPRA.py`, e o modelo no CHIRP, `Baojie → BJ8300_v2.0TEMPRA`. Uma imagem `.img`
guarda o fabricante e o modelo com que foi salva, e o CHIRP só a abre com um driver do mesmo modelo
(`directory.get_radio_by_image`). Por isso o módulo registra o nome antigo em
`directory.MODEL_COMPAT`, e as imagens salvas com o módulo antigo abrem nele sem um segundo modelo
na lista. O `teste_chirp_real` ganhou a `2c` (a imagem com o nome antigo abre, com o modelo novo) e
a `2d` (um modelo desconhecido continua recusado). O arquivo do release V2.0T foi trocado pelo
novo, e o da v1.0T continua lá.

## v41 — 26/10 — o espectro no tema B ✅

Testada no rádio: "ficou excelente". É a última tela que faltava no tema novo.

- **A mesma disposição da tela de fábrica, no tema B.** Embaixo da barra de cima:
  - a linha 1 tem a quantidade de pontos numa etiqueta, a frequência que roda (grande, com os dois
    últimos dígitos pequenos) e AM ou FM;
  - a linha 2 tem o passo numa etiqueta, a frequência do pico da passada em verde, com um ▲, e o
    dBm (o número em branco e a unidade em cinza);
  - o gráfico (160 × 64) fica num painel verde-escuro com linhas de 1/4 em 1/4: barras verdes com o
    topo claro, a coluna sendo medida em branco e a linha do limite tracejada em âmbar;
  - embaixo, as duas pontas da janela, a linha de base e as marcas do começo, do meio e do fim.
- **Cada pedaço pela rotina de fábrica que o desenha.** As cinco rotinas de desenho do espectro
  são trocadas inteiras, com um `goto` na entrada: a frequência (`0x01E45D96`), os dígitos
  digitados (`0x01E4AFC2`), o AM/FM (`0x01E4B04C`), o dBm (`0x01E4ADFE`) e as pontas
  (`0x01E4AB2A`). Viram chamadas da tela B a limpeza da entrada (`0x01E4CFE2`), a janela de cada
  uma das 5 fatias do gráfico e a linha do limite (`0x01E4D72E`). Os ganchos que já eram nossos
  (rótulos, pico, barras) passaram a desenhar no tema B.
- **Provado no desmontado** que só o modo 20 chega a esses sítios, que essas cinco rotinas só são
  chamadas deles, que nenhuma constante aponta para elas e que nada desvia para o meio delas.
  Tudo está na [seção 11 da engenharia reversa](engenharia-reversa.md#11-espectro).
- **A barra de cima do tema B também no espectro.** Com a tecla MO no espectro, a rotina de
  fábrica desenharia o medidor por cima do gráfico; na tela B só a barra acende.
- **O `.vozes` caiu de 996 para 674 bytes**, porque o desenho passou para o `tela.o`. Saíram os
  quatro imediatos que a v26 e a v34 mudavam dentro do dBm e do AM/FM, e a `4f` do formato e da
  fonte de 12, que o pico não usa mais.
- **Uma armadilha de pilha.** A primeira montagem passava 4 bytes do máximo de fábrica. O
  `janela_b()` era sempre chamado com a cor de fundo, e o compilador tinha tirado esse argumento.
  Chamado pela fatia com outra cor, o argumento voltou para a pilha e o quadro da `desenha()`
  cresceu 8 bytes. A fatia passou a chamar o `JANELA()` de fábrica direto, e a pilha voltou a ser
  a de fábrica nas três raízes.
- **A prévia chama os próprios ganchos**, na ordem em que o firmware os chama (cenário 32), e a
  imagem tem de sair igual à da tela composta (30).

## v40 — 26/10 — os ajustes do rádio conferidos e a troca de modulação mais rápida ✔️ testada

Testada no rádio: "funcionou tudo".

- **O travamento da troca para USB/LSB dependia do que estava salvo no rádio, não do código.** A
  v36, a v38 e a v39 travavam, e a v25 e a v34 não. Cada diagnóstico funcionou: montagens com só
  uma parte dos ganchos, escolhidas pela variável `BJ8300_DIAG_GANCHOS` do `montar.py`. Depois a
  própria v39, gravada de novo, também funcionou. O uso em SSB na v25 tinha regravado os ajustes, e
  o valor que travava não ficou provado.
- **Os ajustes do rádio passam por uma conferência** (`confere_radio`, no `telas.c`) na entrada no
  rádio e em cada troca de modulação, antes de o chip receber os valores. Volta ao padrão o que
  estiver fora dos limites que os próprios menus aceitam (`0x01E44C68`):
  - banda, faixa do AM e o modo memória;
  - o índice da memória do FM, do AM e do SSB;
  - os passos do AM e do SSB e o LNA dos dois;
  - o BFO do VFO e, em cada uma das 15 memórias do SSB, a largura e o BFO;
  - as cópias que o chip recebe.

  Valor válido não muda. A `4p` do `conferir.py` refaz os endereços novos dos bytes de fábrica. O
  `deslocamento()` dela lia o registrador de destino junto com o deslocamento, e foi corrigido.
- **A troca de modulação ficou ~2,5 s mais rápida.** A troca de banda do rádio (`0x01E3DDBA`)
  desliga o chip, espera 0,6 s e espera mais duas vezes 1,5 s antes de ligá-lo. Cada uma das duas
  passou para 0,25 s (`ESPERA_2MS(750)` virou 125, nos dois caminhos), conferida pela `4q`. No SSB,
  o resto da espera é o envio do patch do chip por I2C.
- **O S da fonte média.** Letra sem haste vertical (S, Z, 2) saía fina e mais estreita que as
  outras. Agora as duas colunas das pontas dela têm 2 px. A largura de cada coluna vem pronta do
  `fonte.py` (`fonte_b_media`), e a primeira versão, que contava os pontos a cada letra, passava
  4 bytes da pilha.
- **O item MEMCH do menu do rádio** (FM, AM e SSB) grava: o OK guarda a frequência atual no número
  escolhido (no SSB, também a largura e o BFO), e "CH-xx" marca um número já usado. Ele não muda a
  sintonia. As memórias se chamam pelo modo memória (a tecla de código 31, `0x01E4C8CA`), e o nome
  das estações só vem pelo PC.

## v39 — 26/10 — o editor de nome na tela B, âmbar e TX vermelho ✔️ testada

O que o teste da v38 mostrou, corrigido. O menu `VERSION` mostra `Firmware: V2.0T`, a pedido do
dono, desde a v38.

- **O editor de nome (BANK NAME e CH-NAME) na tela B.** O título fica em cima, e à direita vai um
  chip âmbar com o modo (`abc`, `ABC` ou `123`, trocado pelo `#`). O texto aparece num cartão de
  borda verde, em 2x se couber, com o cursor âmbar no fim e quantos caracteres de quantos. O motor
  de menus chama o indicador do modo (`0x01E48720`) em toda passada do editor, e é ele que desenha o
  editor inteiro: ao abrir, a limpeza de fábrica (`0x01E49B1C`) acabou de apagar tudo. A moldura
  que o firmware desenha direto no LCD (`0x01E4865C`) e a linha do texto (`0x01E48FE0`) não vão
  mais à tela. Detalhes na [seção 13 do mapa da tela](mapa-da-tela.md#13-as-outras-telas-v37-a-v39).
- **Banco ligado e canal no banco em âmbar**, no lugar do verde, que mal se via. O " IN" do
  BANK CH saiu, como o " ON" do BANK já tinha saído.
- **TX:** o chip da barra de cima ganhou a bolinha, como o RX, em vermelho, e a barra de sinal
  inteira fica nesse vermelho durante a transmissão (era âmbar).
- **`conferir.py`:** a `4g` só lê instrução dentro das funções (o tamanho de cada uma vem da
  tabela de símbolos do ELF) e dos desvios. A `.rodata` do `tela.o` fica intercalada com o código,
  e a cadeia dos ícones `abc` aparecia como um `goto` para fora.

No primeiro teste o rádio travou ao trocar a modulação de AM ou FM para USB ou LSB. Gravada de
novo, depois dos diagnósticos, ela funcionou: o travamento dependia do que estava salvo no rádio
(ver a v40).

## v38 — 26/10 — o que a v37 mostrou ✔️ testada, com defeitos

Testada no rádio. As correções funcionaram. Ficaram de fora o editor de nome, ainda o de fábrica,
o verde do banco ligado, pouco visível, e o travamento na troca para USB/LSB.

- **Seis endereços de RAM do rádio estavam errados na v37.** O desmontado escreve `h[base+i<<1]`
  para base + i × 2, e eles foram lidos como base + i: a frequência é `h[0x14CA6]` (não
  `0x14993`), a banda `b[0x12A1B]`, o modo memória `b[0x129F9]`, os passos `b[0x12A6F]` e
  `b[0x12A6E]` e o BFO `h[0x14CBC]`. A prévia não pegava, porque imita a RAM com os mesmos
  endereços. O `conferir.py` ganhou a `4p`, que refaz cada endereço dos bytes de fábrica da rotina
  que o lê.
- **Menu:** a piscada de fábrica (o número do item em (133, 57) e, em edição, a linha do cursor na
  fonte antiga) sai toda por um `envia()` do laço do sinal (`0x01E476E4`), que a tela B cala; o
  valor em edição fica parado, com a borda verde e as setas. O marcador da linha
  (`02 10`, dois espaços ou nada) é lido do conteúdo: o nome (tipo 7) e o "NONE" não têm, e o
  BANK NAME perdia as duas primeiras letras ("MOTOROLA" virava "TOROLA").
- **A fonte média**, de 9 px, no menu inteiro: a mesma letra, com cada coluna de haste ou vazia em
  2 px e as outras em 1, e a 2ª e a 6ª linha em 2 px. "NOISE REDUCTION" cabe inteiro.
- **Rádio:** a digitação da frequência (`0x01E49078`), o "SEEK..." (`0x01E4D3F8`) e a espera da
  troca de modulação (o set do item Modulation, `0x01E3DE52`) na tela B. Os dois `envia()` dos
  ajustes de fábrica ficam calados no modo rádio, e a fileira de fábrica parou de piscar.
- **DTMF** (tecla `#`, modo de tela 7): uma caixa com contorno e texto âmbar no lugar do cartão
  do lado em uso.
- **Tensão na partida:** um cartão "VOLTAGE" no lugar de "Volta 8.1V". O firmware escrevia a tensão
  sobre a 8ª posição de "   Voltage" e comia o "ge"; o texto de fábrica que sobra em outro lugar
  virou " VOLTAGE".
- **Os dois lados no banco:** desligar ou ligar um banco põe os dois lados num canal que vale, como
  o DELCH de fábrica faz (`0x01E3C5D6..`: o canal de cada lado que saiu do mapa pula para o próximo,
  e `0x01E3A812` recarrega os dois). Antes só o lado em evidência mudava.
- **A pilha das chamadas por ponteiro.** Os get/set/exibe dos menus são chamados por ponteiro, e a
  análise de pilha não os seguia. Medidos agora no ponto da chamada, ficam em até 652 B, abaixo dos
  740 B de fábrica. Salvar um BANK NAME passava em 40 B desde a v35, porque a saída do editor
  redesenha a tela inteira com a estrutura do banco ainda na pilha; a gravação foi separada.

**Travamento na troca para USB/LSB** (já na v37): o patch do SSB que o rádio manda ao chip
receptor (`0x01E64D40..0x01E68B18`, 15.832 B, enviado em `0x01E3DC42`) está igual ao de fábrica.
O laço do sinal roda dentro da interface, sem concorrência com a carga. A causa não está provada,
e o próximo passo é testar a v36, que não mexia no rádio.

## v37 — 25/10 — as outras telas no tema B, sem chinês ✔️ testada, com defeitos

Testada no rádio. O scan QT ficou bom; o menu, o rádio e a espera do rádio mostraram os defeitos
corrigidos na v38.

- **Menu, rádio, scan QT e espera do rádio no tema B**, desenhados pelo `firmware/src/telas.c` a
  partir das rotinas de fábrica de cada um, sem gancho no `envia()` geral: assim avisos como
  "Sure to Reset?" passam como os de fábrica.
- **Sem chinês:** o item LANGUAGE saiu do menu, o gancho da partida põe o idioma em inglês, e as
  opções em chinês dos itens apontam as tabelas em inglês. O driver do CHIRP esconde o ajuste de
  idioma.
- **A faixa livre cresceu:** o decodificador de MP3 sai junto com o G.729
  (`0x01E257E0..0x01E2EFC4`, 38.884 B; [seção 16](engenharia-reversa.md#16-o-g729-e-o-mp3-fora-v36-e-v37)).
- **O medidor apagado mais claro**, para não se confundir com o preto, também no medidor S do
  rádio.

## v36 — 25/10 — a tela B no lugar do G.729 ✅

**Testada no rádio: gravou inteira e "funcionou tudo, perfeito".** O roteiro de teste foi o da
v35, abaixo.

A v35 com a tela B em outro lugar, porque o rádio recusou gravar a v35. O código da tela é o
mesmo, ligado em outro endereço.

- **A tela B mora no lugar do decodificador G.729**, o das vozes `.wtg`, que não tocam desde a
  v30: `0x01E257E0..0x01E278C8`, 8.424 B dentro do `app.bin`, dos quais a tela usa 5.636. A prova
  de que ele nunca roda está na [seção 16](engenharia-reversa.md#16-o-g729-e-o-mp3-fora-v36-e-v37).
- **O G.729 sai do registro dos decodificadores:** o tipo dele vai de `0x40` para 0, que nenhuma
  busca casa. Assim ele não é escolhido nem por um caminho que tenha escapado do mapa.
- **Nas vozes fica só o espectro**, no C0 e no C1, byte a byte igual ao da v34. Em relação ao rádio
  na v34, muda só o `app.bin`.
- **O `conferir.py`** ganhou a `2h` (o registro, o lugar e nenhuma porta nova para a faixa, no de
  fábrica e no novo) e, na `2g`, a trava de que só o C0 e o C1 mudam.

## v35 — 25/10 — a tela principal B ❌ **o rádio não gravou**

**O carregador recusou a imagem inteira.** No gravador, ela parava nos 5% e pulava para 100%, e o
rádio reiniciava na versão anterior. Nas versões boas ele vai até ~78% antes de pular. Ela não
fica em `firmware/releases/`, porque o rádio não a grava.

Cinco tentativas, todas iguais:

- a v35 por cima da v34 e por cima da V0.12;
- duas imagens de teste: a v34 com só os arquivos de voz C2..C11 trocados, uma com o código da v35
  ali e outra com zeros.

A v34 por cima da V0.12 gravou inteira. Lido o rádio pelo cabo depois, o diretório `tone` estava
byte a byte como o da v34. O carregador decide antes de escrever, e o que ele recusa é a imagem que
muda os arquivos de voz C0..C11. O conteúdo não importa, nem o que já estava no rádio. Mudar só o
C0 e o C1 (v33 e v34) passa, e mudar o `app.bin` sempre passou.

O carregador é o `uart_update.bin` padrão da JieLi, que vem dentro do `ota.bin`, compilado em
19/05/2023. Ele é cifrado, então a regra exata não se lê. Ver a
[seção 14](engenharia-reversa.md#14-flash-interna-partições-e-quem-grava).

O que a v35 trazia, e que a v36 leva:

- **lado em evidência:** cartão com borda verde, nome, canal, frequência grande e medidor de 20 segmentos;
- **etiquetas:** potência (H vermelho, M âmbar, L verde), FM/AM, N, +/−, CT/DCS, R/T, SC, SCR e zona;
- **o outro lado:** compacto;
- **barra de cima:** só o que está ligado (VOX, DW, MO, NR, DTMF ST, Bluetooth, trava), mais RX/TX, tensão e pilha;
- **fonte:** própria, 5×7, ampliada por 2 na frequência.

O firmware continua decidindo quando redesenhar. A tela B entra pela entrada de treze rotinas de
desenho, por um desvio que devolve r0..r3 intactos; fora da tela de dois VFOs, ele refaz as
instruções de fábrica e segue a rotina. A pilha nunca passa do que o firmware de fábrica já usa no
mesmo lugar. Detalhes nas seções 11 e 12 do [mapa da tela](mapa-da-tela.md#12-a-tela-b-v35).

**Para testar:** a tela principal em uso normal: trocar de canal, A/B, receber (o medidor e o RX),
transmitir (TX e a frequência de TX), digitar frequência e canal, mudar potência e modulação, os
ícones de cima (DW, trava, NR…), a bateria; e entrar e sair do menu e do espectro. Com o idioma
chinês a tela fica a de fábrica.

## v34 — 25/10 — pico em verde e o dBm inteiro na troca de AM/FM ✅

A v33 passou no rádio: o espectro funcionou normal rodando do espaço que era das vozes, e o
processador executa dali.

- **dBm inteiro na troca de AM/FM.** A rotina do AM/FM (`0x01E4B04C`) abria a janela
  `127..159 × 17..49` e limpava de preto o dBm, que desde a v26 fica em `112..160 × 36..48`: sobrava
  o "-1" até o fim da passada. O texto dela é de 12 px em y 20, então a janela passa a terminar em
  y 33, como a do "x160" (`0x01E4B066`, `r3 = 49 → 33`). Ela só é chamada pela tecla 23 e pela
  entrada do espectro.
- **Pico em verde.** A frequência do pico sai no verde do tema novo (`#34D399`, `0x3693` em
  RGB565), pela primeira rotina de texto em cor: o caminho ASCII de 12 px do `escreve()` de
  fábrica, com a cor da letra que se pede no lugar do branco fixo. A `4f` confere que a fonte, o
  passo de 12 bytes e a rotina do glifo são os do firmware.

**Para testar:** no espectro, a linha do pico em verde, e trocar AM/FM durante a varredura com o
dBm inteiro.

## v33 — 25/10 — o espectro no espaço das vozes ✅

O código e as tabelas do espectro (946 B) saem da área do fim da imagem para o espaço que era das
vozes. Lá ficam 2268 dos 3260 B, e sobram 992 B para a tela nova. É a prova de que o processador
**executa** dali, e não só lê.

Foi o espectro porque nada dele roda sem o modo de tela 20. No desmontado de fábrica, todo caminho
até os ganchos dele passa pelo caso 20 do tratador de modos, pela entrada do espectro (a única
escrita de 20 no modo) ou pelo "se o modo não for 20, pula" do laço principal. Se a execução dali
falhar, só o espectro cai: a partida, o cabo e o PTT2 + atalho não passam por ele.

A canária saiu, porque o próprio código prova que o dado chega. O `conferir.py` passa a exigir que
só as rotinas do espectro morem no `.vozes`, que só os sítios do espectro desviem para lá e que
nada do `.text` chame o `.vozes` (`2b`, `2d`, `4g`), e as três pegaram o erro numa montagem errada
de propósito. Detalhes na [seção 15 da engenharia reversa](engenharia-reversa.md#15-vozes-fora).

**Para testar:** entrar no espectro e usar como sempre (varredura, OK, #, setas parado num sinal,
dígitos da frequência) e sair. O resto do rádio não muda.

## Driver do CHIRP — 25/10 — COIN e sem Voice Prompts

- **Roger com cinco opções**, como o rádio: OFF, BEEP 1, BEEP 2, **COIN** e TONE1200. O COIN grava o
  valor 4, que não cabe nos 2 bits do roger da classe base: o módulo lê e grava 3 bits, sendo o
  terceiro o bit baixo do byte de cima (`unused_9017`, sem uso no rádio).
- **Sem "Voice Prompts"** no Settings: desde a v30 o firmware não toca voz nenhuma, e o item VOICE
  saiu do menu do rádio.

O teste ganhou os casos do valor 4 (abre como COIN, grava o byte certo) e da página sem o ajuste
de voz. O módulo do release v1.0T não mudou: este acompanha a v32.

## v32 — 25/10 — o espaço das vozes, provado ✅

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
   `0x01E3BAD8`, que vai para o desenhador de glifo `0x01E388EC` e para o vão `0x01E38978`). Trocar
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
