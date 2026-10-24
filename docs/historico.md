# Histórico das versões

> 🇬🇧 English: [changelog.md](changelog.md)

Datas de 2026. Cada versão foi gravada e testada no rádio; o que quebrou está registrado com a
causa, porque é isso que evita repetir.

## v14 — 24/10 — realce refeito a partir do mapa da tela

Feito depois de mapear a tela inteira ([mapa-da-tela.md](mapa-da-tela.md)), que explicou os dois
defeitos da v13 com endereço:

1. **O reset** era estouro do buffer de vídeo. A v13 pintava o bloco inteiro numa chamada só — 71
   linhas em largura cheia = **22.720 bytes** — e o buffer em 0x10190 tem **10.240** (uma banda de
   160×32). `janela()` não tem trava de tamanho (laço em `0x01E3AD8E`), então o excedente caía em
   cima do buffer de texto, da base global e das flags de redesenho.
2. **O realce não aparecer** era variável errada: eu usei `tx_ab` (`0x14B84`, o ajuste gravado na
   memória de dados) em vez de **`b[0x15E04]`**, que é o estado da tela que o próprio firmware
   consulta para decidir a posição de cada linha.

Agora o gancho entra nas **três janelas que formam um bloco** (nome+frequência em `0x01E4B270`,
barra de sinal em `0x01E41468`, número do canal em `0x01E4B314`) e **não mexe na geometria**: só
troca a cor com que o firmware já ia encher aquela janela. Conferido no código de máquina gerado —
r0 a r3 não são tocados —, então o estouro da v13 é impossível por construção.

**Ainda não testado no rádio.** O que está provado é estático: o mapa, a tabela-verdade das sete
combinações de janela e lado, e que a montagem fecha em `RESULTADO: tudo certo`.

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
