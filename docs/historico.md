# Histórico das versões

> 🇬🇧 English: [changelog.md](changelog.md)

Datas de 2026. Cada versão foi gravada e testada no rádio; o que quebrou está registrado com a
causa, porque é isso que evita repetir.

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
