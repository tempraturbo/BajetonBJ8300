# Mapa da tela principal

> 🇬🇧 English: [screen-map.md](screen-map.md)

Levantado em 2026-10-24 no desassemblado do firmware V0.12, em duas rodadas: a primeira (v14) ficou
incompleta e deu errado no rádio, a segunda fechou o caminho de desenho inteiro. Tudo aqui tem
endereço; o que não tiver, não está provado e está marcado como tal.

## 1. Como o firmware desenha

Não existe framebuffer da tela inteira. O desenho é feito **uma janela por vez**:

> **Atenção aos endereços:** o `llvm-objdump` mostra os imediatos em **decimal** (`r4 = 83466`).
> Converter na mão já custou uma versão: a v15 leu `0x145CA` no lugar de `0x1460A` (83466) — 0x40
> a menos — e o realce nunca pintou. Os endereços abaixo foram conferidos contra o decimal, e a
> montagem agora tira os da `janela()` das próprias instruções dela (verificação `4f`).

```
janela(x0, x1, y0, y1, limpar, cor)   0x01E3AD34
    b[0x14608] = x0   b[0x1460A] = y0   b[0x1460C] = x1   b[0x1460E] = y1
    h[0x14610] = (x1 - x0) * 2         (bytes por linha)
    com limpar != 0, enche o buffer com cor
<desenha texto, barra, ícones dentro dela>
envia()                               0x01E3ADBC
    lê os mesmos globais e manda o buffer para o LCD; não recebe argumento nenhum
```

O buffer é a memória de vídeo em **0x10190**, com **10240 bytes** — uma banda de **160×32**. Cada
pixel é RGB565 com o **byte alto primeiro** (`0x01E3AD90`: `b[p] = cor >> 8`, `b[p+1] = cor`), ao
contrário da CPU, que é little-endian: quem escrever um `u16` no buffer tem de trocar os bytes.

E a armadilha que derrubou a v13:

> **`janela()` não tem trava de tamanho.** Ela calcula `(x1-x0) * (y1-y0) * 2` e sai escrevendo a
> partir de 0x10190 (laço em `0x01E3AD8E`). Uma janela maior que 10240 bytes atropela o que vem
> depois: o buffer de texto (0x129B0), a base global (0x14680), as flags de redesenho (0x15367) e
> os mapas de canal. O resultado é reset.

Por isso nenhuma chamada de fábrica passa de 32 linhas em largura cheia — a maior é exatamente
160×32, em `0x01E4A78A`.

## 2. O fundo do texto NÃO vem de `janela()`

Cada rotina de texto enche a célula de cada caractere com uma cor de fundo própria, **preta fixa**.
No desenhador de nome (`0x01E3BACE`) é o `r8 = 0` de `0x01E3BAD8`, que vai como fundo tanto para o
desenhador de glifo (`0x01E388EC`, sexto argumento) como para o enchimento do vão entre caracteres
(`0x01E38978`). A rotina da frequência (`0x01E3BB7A`) faz o mesmo com o `r8 = r3` que recebe.

Consequência prática, e foi o segundo defeito da v14: **trocar a cor da janela não pinta o bloco**.
O fundo da janela aparece só na borda; nome e frequência continuam em caixas pretas.

## 3. Quem está em evidência: `0x01E3B1A2`, não uma variável solta

`b[0x15E04]` é o lado selecionado (**0 = VFO A, diferente de 0 = VFO B**). Não confundir com
`tx_ab` (`0x14B84`), que é o ajuste `0x9018` espelhado em RAM — usar o ajuste no lugar do estado da
tela foi um dos dois erros da v13.

Mas quem decide o **layout** é a rotina `0x01E3B1A2(lado)`, que o próprio firmware consulta no
desenhador de bloco e no complemento. Ela:

1. lê o modo de tela em `b[0x1533D]` e devolve **0** se ele não for 0, 3 ou 5;
2. senão devolve, conforme o lado pedido e `b[0x15E04]`:

| | `layout(0)` (lado A) | `layout(1)` (lado B) |
|---|---|---|
| A selecionado | **2** (expandido) | 3 (compacto) |
| B selecionado | 0 (compacto) | **1** (expandido) |

Chamada com o lado selecionado — `layout(b[0x15E04] != 0)` — ela devolve **2 com o A em
evidência, 1 com o B e 0 quando a tela não é a de dois VFOs**. É a única fonte de verdade: a tela
de um VFO só grava **4** em `b[0x1533D]` (em `0x01E4CDC4`) e usa as mesmas funções de desenho.

O desenhador de bloco usa `layout(lado) - 1` como índice, e de `(3 >> índice) & 1` tira se aquele
bloco é o expandido.

## 4. Os caminhos de desenho da tela de dois VFOs

```
0x01E3C290                limpa e redesenha: partida, saída do menu e mais 14 chamadores
  ├─ 0x01E3ACBA(2)        limpeza (ver abaixo)            ← gancho_limpa em 0x01E3C294
  └─ 0x01E3C278           redesenho completo
0x01E4DC9E                tecla A/B: troca o lado (0x01E3A812) e chama 0x01E41486, que vai a
  └─ 0x01E40F1C(0)        ├─ 0x01E3ACBA(2) limpeza      ← gancho_limpa em 0x01E40F66
                          └─ 0x01E3C132(0) a tela

0x01E3C278                redesenho completo, sem limpar (também é chamado direto, na varredura)
  ├─ 0x01E3B05C           barra de estado (y 0..15) — caminho próprio, sem gancho nosso
  └─ 0x01E3C132(0)        a tela
       ├─ janela 0x01E3C176   faixa da barra do A compacto   (0,160,41,55)
       ├─ janela 0x01E3C19E   faixa da barra do A expandido  (0,160,74,88)
       ├─ 0x01E3B2C4(lado, 0) linha de informações (o `H`)   (0,160,y,y+12), y 18 (A) / 57 (B)
       ├─ janela 0x01E3C1D0   faixa da barra do B            (0,160,114,128)
       ├─ 0x01E3BD04(0, lado, modo)   bloco: frequência e linha do nome
       │    └─ janelas 0x01E3BE20 (0,132,y,y+12)  0x01E3BE58 (0,160,y,y+18)
       │              0x01E3BF50 / 0x01E3BFA6 / 0x01E3C02C (0,160,y,y+24)
       └─ 0x01E3C06E(0, lado)         complemento: faixa da direita (132,160,y,y+22)

atualizações parciais (correm ao trocar de canal, e a da barra também ao variar o sinal)
  0x01E4B29C   número do canal (127,159,y,y+9) + barra via 0x01E413BE (0,160,y,y+14)
  0x01E4B1DE   nome e frequência (7,160,y,y+24)
  0x01E453E0   redesenha só o bloco em evidência: linha de informações, bloco e complemento

recepção e transmissão (0x01E40F1C com argumento 1 ou 2)
  0x01E3B592(1, 2, lado, 1)   barra do lado que recebe/transmite em JANELA PRÓPRIA
                              (19,140,y,y+9), segmentos e o texto RX/TX em x=127 → envia 0x01E3B7CA
  janela 0x01E40FD4           só no modo frequência: a linha grande (0,160,y,y+24) → envia 0x01E41008
```

A linha de informações (`0x01E3B2C4`) só é desenhada para o lado **expandido**: com o `layout()`
em 0 ou 3 ela pula janela e envio (`0x01E3B326`/`0x01E3B32A`).

**A limpeza `0x01E3ACBA(2)` desenha a linha separadora.** Ela enche linhas inteiras com
`0x01E3AC68(y, altura, cor)`, que manda direto ao LCD, sem passar por `janela()`/`envia()`:

| | A em evidência | B em evidência |
|---|---|---|
| área do lado em evidência (preto) | y 16, 72 linhas (16..87) | y 56, 72 linhas (56..127) |
| área do outro lado (preto) | y 89, 39 linhas | y 16, 39 linhas |
| **linha separadora** (branco) | y **88** | y **55** |

A mesma limpeza roda ao entrar no menu (`0x01E4CBAE`), por isso o gancho não entra nela, e sim nas
duas chamadas que limpam **e redesenham a tela principal**.

`0x01E3C132` com argumento **1** é a tela de **um** VFO: ela desvia as três faixas de barra e manda
o bloco para y 32/36. Os onze chamadores passam 0 ou 1 — nunca outro valor.

## 5. A geometria, por índice de layout

Os `y` saem de duas tabelas no desenhador de bloco (`0x01E3BD40` = `0x5D1E45` e `0x01E3BD4A` =
`0x5D3A61`, um byte por índice) e de uma terceira no complemento (`0x01E3C0BE` = `0x5B385F`):

| índice | quem é | frequência (24 linhas) | nome (12/18 linhas) | faixa da direita | barra | número |
|---|---|---|---|---|---|---|
| 1 | A expandido | 30 | 58 | 56 | 74 | 62 |
| 0 | B expandido | 69 | 97 | 95 | 114 | 101 |
| 2 | B compacto | 93 | 93 | 91 | 114 | — |
| 255 | A compacto | 21 | 21 | 19 | 41 | — |

Juntando por lado em evidência:

| | `y` do lado em evidência | `y` do outro lado | limite que separa |
|---|---|---|---|
| A em evidência | 18, 30, 56, 58, 62, 74 | 91, 93, 114 | `y < 89` (74 contra 91) |
| B em evidência | 57, 69, 95, 97, 101, 114 | 19, 21, 41 | `y >= 57` (41 contra 57) |

A linha de informações em y 18/57 vem de `0x01E3B32E` (57 com o `layout()` em 1, senão 18).

Nenhuma janela passa do buffer: 160×24×2 = 7680 B, 160×14×2 = 4480 B, 32×9×2 = 576 B.

## 6. Por que a v14 não funcionou

O usuário relatou exatamente dois sintomas, e cada um tem uma causa com endereço:

1. **«o verde só sai no fundo da barra de sinal, e só ao mudar de canal com a seta»** — os três
   ganchos da v14 (`0x01E4B270`, `0x01E41468`, `0x01E4B314`) estão todos em **atualizações
   parciais**. O redesenho completo, que é o que corre ao trocar de A para B, passa por
   `0x01E3C132` e `0x01E3BD04`, que não tinham gancho nenhum. Dos três, o único que pinta área
   visível de fundo é o da barra — daí o verde aparecer só ali e só quando o canal muda.
2. **«o quadrado completo continua preto»** — mesmo com o gancho no lugar certo, trocar a cor da
   janela não bastaria: o fundo do nome e da frequência vem das rotinas de texto (seção 2).

## 7. Como o realce é feito agora

São dois ganchos, que dividem a mesma conta: a área do lado em evidência é a da limpeza, 72
linhas a partir de 16 (A) ou 56 (B).

```c
static unsigned area_realce(void)         /* 16 com o A, 56 com o B, 0 fora da tela */
{
    unsigned lado = LAYOUT(LADO_B);        /* 2 = A em evidência, 1 = B, 0 = outra tela */
    return lado - 1 <= 1 ? 96 - 40 * lado : 0;
}
```

**1. No envio** (`envia()`, `0x01E3ADBC`): troca por verde **todo pixel preto do pedaço** antes
de ele ir para o LCD. Assim o fundo da janela, a célula de cada caractere e o vão
entre caracteres ficam verdes de uma vez, e o que tem cor própria (texto branco, segmentos da
barra, linha separadora) passa intacto.

```c
void gancho_envia(void)
{
    unsigned y0 = JAN_Y0, inicio = area_realce();

    if (inicio && y0 - inicio < AREA) {    /* sem sinal: y0 abaixo do início vira número enorme */
        unsigned n = (JAN_LINHA >> 1) * (JAN_Y1 - y0);
        volatile u16 *p = VIDEO;

        if (n > PIXELS)             /* nunca passa do buffer */
            n = PIXELS;
        while (n--) {
            if (*p == 0)
                *p = COR_BUF;
            p++;
        }
    }
    ENVIA();
}
```

São **16 sítios**, todas as chamadas de `envia()` dentro das funções que desenham a tela de dois
VFOs, inclusive as do caminho de recepção e transmissão (seção 4), trocadas no lugar: `call` de 4 bytes, **não cresce o código**. O único envio
por salto do firmware (`goto` em `0x01E4544E`, no fim de `0x01E453E0`) só reenvia o último pedaço
do complemento, que o gancho em `0x01E3C12A` já processou. A barra de
estado, os menus e os avisos sobrepostos têm caminho próprio e continuam pretos. E o gancho ainda
confere na rotina do firmware (seção 3) se a tela é mesmo a de dois VFOs, de modo que as outras
telas que passam por essas mesmas funções — a de um VFO, em primeiro lugar — nunca são pintadas.

Esse gancho **não desenha nada**: só lê os limites que `janela()` já gravou e escreve dentro do
buffer, com trava no tamanho dele. O estouro da v13 é impossível por construção.

**2. Na limpeza** (`gancho_limpa`, nas chamadas `0x01E3C294` e `0x01E40F66`): deixa a limpeza de
fábrica correr — preto e a separadora — e enche de verde a área do lado em evidência. As janelas
vêm por cima, recoloridas pelo primeiro gancho, e **nos vãos entre elas fica o verde**. Ninguém
mais pinta aqueles vãos, então eles continuam verdes até a próxima limpeza, que é sempre seguida
deste mesmo enchimento. A separadora (88 ou 55) fica fora da área e não é tocada.

```c
void gancho_limpa(unsigned modo)
{
    unsigned inicio;

    LIMPA(modo);
    if ((inicio = area_realce()) != 0)
        ENCHE(inicio, AREA, COR_REALCE);
}
```

A cor fica em `COR_REALCE`, no `firmware/src/bancos.c` — uma linha só. O valor atual é `0x0240`
(#004900, degrau 18 dos 64 do verde, desde a v20; da v17 à v19 era `0x02C0`). O buffer de vídeo guarda o byte alto primeiro, então o gancho do envio usa `COR_BUF`, o
mesmo valor com os bytes trocados (`0x4002`); o enchimento recebe a cor normal, porque a própria
rotina separa os bytes. Os dois conferidos no código de máquina gerado.

## 8. Vãos que nenhuma janela cobre

O gancho do envio só recolore o que é enviado ao LCD, e nestas faixas **ninguém envia nada**. Na
v16 elas ficavam com o que a limpeza deixou, preto; desde a v17 o gancho da limpeza as deixa
verdes. Calculadas da tabela da seção 5 e conferidas nas fotos da v16:

| | A em evidência | B em evidência |
|---|---|---|
| entre o nome grande e a frequência | y 54..56 em toda a largura; 56..58 em x 0..132 | y 93..95 em toda a largura; 95..97 em x 0..132 |
| entre a frequência e a barra | y 70..74 em x 0..132 | y 109..114 em x 0..132 |

À direita de x=132 não há vão porque a faixa da direita (56..78 com o A, 95..117 com o B) cobre a
altura toda. Também ficam na área, e agora verdes: as linhas 16..17 entre a barra de estado e a
linha do `H` (com o A) e a linha 56 entre a separadora e a linha do `H` (com o B).

## 9. O que ainda não está mapeado

- A linha 15, entre a barra de estado (janela 0..15, linhas 0..14) e a área do A (16): ninguém a
  desenha, nem a limpeza. Fica preta, como borda da barra de estado.
- As outras telas (menu, FM, varredura) usam as mesmas rotinas com outras coordenadas. O gancho não
  as pinta, pela trava da seção 3, mas o realce também não foi pensado para elas.
- A janela do nome começa em x=7 na atualização parcial (`0x01E4B270`) e em x=0 no redesenho
  completo (`0x01E3BF50`): sobra uma tira de 7 pixels que só é repintada no redesenho completo.

## 10. Texto: `escreve()`, as fontes e o glifo

Mapeado em 2026-10-25 para o tema novo, que precisa de texto em cor.

```
escreve(y, x, texto, tamanho, fundo)                 0x01E3B834
    tamanho 12, 16 ou 24; x anda a largura de cada letra
    a cor da letra é fixa: r10 = 0xFFFF (0x01E3B842), branca
glifo(y, x, largura, altura, bits, fundo, frente)    0x01E388EC (14 chamadores)
    não desenha com altura 0, largura 0 ou x > 160; corta a largura em 160 - x
    escreve no buffer da janela aberta, pelo cursor b[0x14604] (x) e b[0x14606] (y), que
    0x01E38724 limita a 159 e 127 e converte pela janela (b[0x14608], b[0x1460A], h[0x14610]);
    pixel que cairia fora dos 10240 B do buffer não é escrito
```

**Os bits** vêm em faixas de 8 linhas. Cada byte é uma coluna, e o bit k é a linha faixa × 8 + k,
com o bit 0 em cima. A primeira faixa ocupa os primeiros `largura` bytes e a seguinte os próximos
(laço em `0x01E38944..0x01E38974`). Bit aceso sai na cor de frente, apagado na de fundo.

**Os ASCII** (32 a 126) de cada tamanho:

| Tamanho | Letra | Bytes | De onde |
|---|---|---|---|
| 12 | 6 × 12 | 12 | chip de dados, `0x86BC0 + c × 12` (`0x01E3B9E6`) |
| 16 | 8 × 16 | 16 | chip de dados, `0x5B5C0 + c × 16`, e cada coluna desce uma linha (`0x01E3B8D0..0x01E3B906`) |
| 24 | 12 × 24 | 36, de 48 em 48 | flash interna, `0x01E63B70 + (c − 32) × 48` (`0x01E3B868`) |

Os de 12 e 16 são lidos pelo `LE_FLASH` (`0x01E390AC`), e o de 24 pelo `memcpy` da ROM
(`0x021127A8`). Byte de 161 em diante é chinês (GB2312, dois bytes), por outros caminhos.

Para escrever em cor basta repetir o caminho ASCII com a frente que se quiser: `janela()`,
`glifo()` letra a letra e `envia()`. É o que o `escreve_cor` do `bancos.c` faz, na fonte de 12
(v34, a linha do pico do espectro).

## 11. Os dados da tela de dois VFOs

Levantado em 2026-10-25 no desmontado de fábrica, para a tela B (seção 12). Base global `0x14680`.

**Cada lado** (A = 0, B = 1) tem um registro em `0x14DC4 + lado × 31` (`0x01E3BD88..0x01E3BD8E`):

| Campo | Onde | Observação |
|---|---|---|
| frequência de RX | u32 em +0 | em 10 Hz; `%03d.%05d` com /100000 |
| tipo de tom | `b[+4]` | 0 nenhum, 1 CT, outro DCS |
| frequência de TX | u32 em +5 | a que a linha grande mostra transmitindo (`0x01E40FE6`) |
| deslocamento | `b[+10]` | 1 "+", 2 "−" |
| largura | `b[+11]` | diferente de 0: "N" |
| AM | `b[+12]` | 1 = AM, de 18 a 999,99999 MHz; de 108 a 135,99999 MHz sempre AM (`0x01E3B438..0x01E3B47E`) |
| SC | `b[+13]` | diferente de 0 e fora de AM: "SC" (significado sem prova) |
| SCR | `b[+14] & 3` | diferente de 0: "SCR" (significado sem prova) |
| nome | 12 B em +15 | para em 0 ou 0xFF (`0x01E3BDCC..0x01E3BDE6`); vazio: "Unnamed CH" |

Fora do registro:

| Dado | Onde |
|---|---|
| lado em evidência | `b[0x15E04]` |
| modo de trabalho | nibble de `b[0x14B86]` (A no baixo, B no alto): 1 = canal |
| exibição | `b[0x14B79]` (A), `b[0x14B7A]` (B): 0 nome, 1 frequência, 2 número |
| número do canal | `h[0x15E0C]` (A), `h[0x15E10]` (B), base 0 |
| potência | `b[0x15E30 + lado]`: 0 H, 1 M, 2 L |
| R/T | `b[0x15E28 + lado]`: 1 R, 2 T (significado sem prova) |
| idioma | `b[0x14B74]`: 1 = chinês |
| zona | com `b[0x14BD4]` = 0, navega por zona; a zona é `b[0x14BDB + lado]` |
| estado de TX | `b[0x14685]`: 2 = transmitindo (`0x01E40F1C`) |
| dígitos digitados | quantos em `b[0x14A35]`, em ASCII a partir de `0x14A3A` (`0x01E4B29C`, `0x01E4B1DE`) |

**Barra de cima** (`0x01E3B05C`, fundo `0x2965`). Todo ícone é sempre desenhado, e o desligado sai
com a frente da cor do fundo:

| Ícone | Liga com |
|---|---|
| VOX | `b[0x14B6E]` |
| DW | `b[0x14B70]` (TDR) |
| MO | `b[0x15370]` |
| nota musical | `b[0x14B75]`, item DTMFST |
| ondas | `b[0x14BDE]`, item NOISE REDUCTION (o get/set dele, `0x01E3D0E0`/`0x01E3D0F4`, é o único que grava) |
| Bluetooth | `b[0x14B89]` = 1 |
| cadeado | `b[0x14B87]` |
| tensão | `b[0x149BC] × 444 / 1000`, em décimos de volt (`0x01E3AFBE`) |
| pilha | nível de `[0x149C0]`: acima de 6 vale 3, de 4 a 6 vale 4, senão ele mesmo (`0x01E3B03E`); 0..3 = 1..4 traços, 4 = só o contorno vermelho; com 5 e 6 o laço pisca o contorno (`0x01E460B2..0x01E460F2`) |

**Medidor.** Só é redesenhado quando o degrau do sinal muda (`0x01E46166`: 5 degraus, nível
2 × degrau + 2) ou pelo monitor, com nível 10 (`0x01E4532C`). O redesenho completo deixa as barras
vazias, e o fim da recepção não chama o medidor. Na transmissão ele fica cheio pela potência
(H 10, M 5, L 2).

**Quem redesenha o quê.**

- **A tela inteira:** `0x01E3C132`, pelos 17 chamadores de `0x01E3C290`; na varredura, por `0x01E3C278` a cada canal.
- **O lado em evidência na troca de canal:** `0x01E453E0`.
- **Um lado e a linha de informações:** `0x01E413BE` e `0x01E3B2C4`. A tecla A/B passa por `0x01E41486`.
- **O medidor:** `0x01E3B592`.
- **A transmissão:** `0x01E40F1C`.
- **Os números sendo digitados:** `0x01E4B29C` e `0x01E4B1DE`.
- **A barra de cima, a pilha, o MO e o cadeado:** cada um tem rotina e janela próprias.

## 12. A tela B (v35)

Desenhada por `firmware/src/tela.c`. Desde a v36 o código mora no lugar do decodificador G.729,
dentro do `app.bin` ([engenharia reversa, seção 16](engenharia-reversa.md#16-o-g729-e-o-mp3-fora-v36-e-v37)); na
v35 ficava no espaço das vozes, e o carregador recusou gravar. Do firmware ela usa só a
`janela()` e o `envia()`; retângulo e letra ela escreve no buffer de vídeo da janela aberta. A cena
inteira é desenhada em faixas de até 32 linhas, e o que cai fora da janela é cortado.

**Quem decide quando desenhar continua sendo o firmware.** Treze rotinas da seção 11 ganham, na
entrada, um `goto` para um desvio no `tela.c`. O desvio:

1. guarda `rets` e r0..r3, com as mesmas instruções que o firmware usa (`0x01E3C290` e `0x01E51358`);
2. chama o gancho em C com os argumentos da rotina;
3. se o gancho desenhou a tela B, volta a quem chamou com r0..r3 intactos;
4. se não desenhou, refaz as instruções de fábrica que o `goto` cobriu e segue a rotina.

A tela B só desenha com `layout()` diferente de 0. Até a v36 também ficava fora do idioma chinês,
porque a fonte não tem os ideogramas; desde a v37 o rádio é sempre em inglês. As outras telas estão
na seção 13. Na tela de um VFO e no espectro, as rotinas rodam como sempre.

| Rotina | Gancho | O que a tela B redesenha |
|---|---|---|
| `0x01E3C132(0)`, a tela inteira | `b_tela` | tudo, e zera o estado de TX como ela |
| `0x01E3B05C`, `0x01E3B03E`, `0x01E3AE50` | `b_barra` | a barra de cima |
| `0x01E3AF1E(nível)`, a pilha | `b_bateria` | a barra, com o nível que vem (o pisca da bateria fraca) |
| `0x01E3AEB0(ligado)`, o MO | `b_monitor` | a barra, com o MO que vem |
| `0x01E453E0` | `b_evidencia` | o lado em evidência |
| `0x01E4B29C`, `0x01E4B1DE` | `b_digitando` | o lado em evidência com os dígitos; só elas os mostram, como na de fábrica |
| `0x01E413BE`, `0x01E3B2C4` | `b_lado` | os dois lados, porque na tecla A/B o cartão muda de lugar |
| `0x01E3B592`, o medidor | `b_medidor` | o medidor do lado, com o nível que vem, e a barra (RX) |
| `0x01E40F1C`, a transmissão | `b_transmissao` | refaz a lógica dela: grava o estado antes de desenhar, e desenha o lado e a barra (2) ou tudo (fim) |

**Provado no desmontado antes de gravar:**

- nenhum desvio nem constante cai dentro dos bytes cobertos;
- nenhuma das treze lê argumento da pilha de quem chama;
- elas só mudam estado no que o gancho repete (`b[0x14685]`);
- a pilha: a profundidade máxima com a tela B é a mesma de fábrica nos três contextos de onde ela é
  chamada (a interface `0x01E4B416`, 740 B; o laço do sinal `0x01E4622A`, 620 B; a partida, 696 B).
  O pior caminho da tela B, 376 B a partir do desvio do medidor, cabe abaixo do mais fundo de cada um.

O envio e a limpeza da tela de fábrica (seção 7) também foram para o `tela.c`. Com a tela B ativa,
o envio redesenha da tela B o lado a que a janela pertence, e a limpeza não roda, porque a tela
inteira vem logo depois. Sem a tela B (`layout()` em 0) os dois fazem o que a chamada de fábrica
fazia; o realce verde de antes, que só aparecia no idioma chinês, saiu na v37.

**Prévia no PC.** `firmware/previa/` compila o mesmo `tela.c` com a RAM, a `janela()` e o `envia()`
imitados e grava a tela em PNG: `python -I previa/previa.py <pasta>` (precisa do zig, por
`pip install ziglang`). A fonte (5×7, ampliada por 2 na frequência) mora em
`firmware/fonte/fonte_b.txt`, desenhada em `#` e `.`; o `scripts/fonte.py` gera a tabela.

**Conferido no build** (`conferir.py`): a 2b, a 2d e a 4g como antes, agora com os sítios da tela; a
4m confere os 13 desvios byte a byte (bytes de fábrica, `goto`, desvio e volta); a 4n, que não há
seção em RAM.

## 13. As outras telas (v37 a v39)

Desenhadas por `firmware/src/telas.c`, que o `tela.c` inclui (o mesmo `tela.o`, na faixa livre). O
modo de tela é `b[0x1533D]`: 0, 3 e 5 a tela de dois VFOs, 1 o menu, 2 o rádio, 7 o DTMF, 17 o
scan QT e 20 o espectro.

**Cada parte entra pela rotina de fábrica que a desenha, nunca pelo `envia()` geral.** Avisos e
confirmações ("Sure to Reset?") passam como os de fábrica, e o desenho fundo da tela B só roda
nesses pontos. Onde a rotina de fábrica desenha e também grava estado, o gancho a roda inteira e
cala o `envia()` dela, em vez de repetir a lógica.

| Onde | Como | Gancho | O que a tela B faz |
|---|---|---|---|
| `0x01E48E82`, `0x01E48ECA` (motor de menus `0x01E489E0`) | `envia()` trocado | `gancho_menu_titulo`, `gancho_menu_linha` | a faixa do título ou da linha que o motor montou |
| `0x01E476E4` (laço do sinal) | `envia()` trocado | `gancho_menu_pisca` | nada no menu: é por ele que sai toda a piscada de fábrica |
| `0x01E3B2C4`, a linha de informações | desvio | `b_lado` | no menu e no scan QT, o resumo do lado em evidência |
| `0x01E48720`, o indicador do modo do editor | desvio | `b_editor_modo` | o editor de nome inteiro |
| `0x01E4865C`, moldura direto no LCD | desvio | `b_editor_quadro` | nada no menu (lê a cor da pilha de quem chama: só a rotina de fábrica, com a pilha refeita, a lê) |
| `0x01E48FE0`, a linha do texto do editor | `envia()` trocado | `gancho_editor_linha` | nada: o desvio do indicador já desenhou o texto |
| `0x01E44FB0`, o rádio inteiro | desvio | `b_radio` | a tela do rádio, depois de pôr banda > 4 e faixa do AM > 3 em 0, como ela |
| `0x01E449BA`, `0x01E44B28`, `0x01E45198` | desvio | `b_radio_freq`, `b_radio_nome`, `b_radio_sinal` | a frequência, o nome da memória e o medidor S |
| `0x01E44C68(item, valor)`, os ajustes | desvio | `b_radio_ajustes` | roda a de fábrica (grava o valor) e redesenha; os dois `envia()` dela (`0x01E44CA8`, `0x01E44FA8`) ficam calados no modo 2 |
| `0x01E49078`, a frequência digitada | desvio | `b_radio_digita` | a máscara com os dígitos no lugar da frequência |
| `0x01E4D3F8`, o "SEEK..." | `envia()` trocado | `gancho_radio_seek` | "SEEK..." em âmbar no lugar da frequência |
| `0x01E4A7D4` (entrada no rádio), `0x01E3DEBE` e `0x01E3DEEA` (troca de modulação) | `envia()` trocado | `gancho_radio_inicio` | a espera, na tela inteira; o "Please Wait..." da troca (`0x01E3DF10`) vira nop |
| `0x01E45A82(rodando)`, o scan QT | desvio | `b_scanqt` | o título e o estado |
| `0x01E4DA88`, `0x01E4A93C` (DTMF) | `envia()` trocado | `gancho_dtmf` | a caixa do DTMF, no lugar do cartão do lado em uso |
| `0x01E55F40` (partida) | `envia()` trocado | `gancho_tensao` | o cartão da tensão |
| `0x01E4CFE2`, a limpeza na entrada do espectro (v41) | chamada trocada | `gancho_espectro_limpa` | o fundo e a barra de cima |
| `0x01E45D96`, `0x01E4AFC2`, `0x01E4B04C`, `0x01E4ADFE`, `0x01E4AB2A` (espectro) | rotina trocada inteira | `b_esp_freq`, `b_esp_digitos`, `b_esp_modo`, `b_esp_dbm`, `b_esp_eixo` | a frequência ou os dígitos, AM/FM, o dBm e as pontas |
| as 5 janelas das fatias do gráfico e a linha do limite (`0x01E4D72E`) | chamada trocada | `gancho_fatia`, `gancho_limite` | o painel com a grade e a linha tracejada em âmbar |

**Espectro** (v41). Mapa das rotinas e prova na [seção 11 da engenharia
reversa](engenharia-reversa.md#11-espectro). A barra de cima, a pilha e o MO desenham a barra B
também no modo 20, e o medidor, a que no espectro só a tecla MO chega, acende só a barra (a de
fábrica o desenharia por cima do gráfico).

**Menu.** O motor `0x01E489E0` monta o título em `0x15494` (26 B) e três linhas em `0x154AF`
(27 B cada). Cada linha passa por `0x01E487BC(linha, texto, marcador)`, que põe na frente `02 10`
(o valor gravado, marcador 2), dois espaços (outra opção, 1) ou nada (0: o nome do tipo 7 e o
"NONE"); o tipo 8 nunca tem marcador. A tela B reconhece o marcador pelo conteúdo. Os outros dados:
número do item `b[0x14BFA]`, cursor `b[0x14C10]`, editando `b[0x14BF9]` e tipo `b[0x14C00]`
(0 e 4 a 6 lista, 2 tom com digitação, 3 frequência, 7 nome, 8 sem marcador, 13 número com sinal).
O BANK e o BANK CH marcam a opção ligada com um byte `0x01` no fim, que a tela B pinta de âmbar.

**A piscada.** No menu, o laço do sinal (`0x01E4622A`) conta `b[0x14C1A]` de 0 a 7, uma vez por
volta. Em 4 ele desenha, em 0 apaga. Fora da edição, pisca o número do item em (133, 57)
(`0x01E47062`, `0x01E47290`); editando, a linha do cursor na fonte de fábrica (`0x01E472EA`,
`0x01E47332`); no editor de nome, a linha do texto com um `_` (`0x01E476B2`). Tudo sai pelo mesmo
`envia()` de `0x01E476E4`.

**O editor de nome** (tipo 7 editando). O texto fica em `0x14A3A`, o tamanho em `b[0x14A35]`, o
máximo em `b[0x14A36]` (gravado pelo get do item) e o modo em `b[0x14A38]`: 0 `abc`, 1 `ABC`,
2 `123`. O 3, pinyin, só existe em chinês (`0x01E4A59C`). Ao abrir, o tratador do OK chama a
limpeza de fábrica (`0x01E49B1C`), e depois de cada tecla o motor roda inteiro (`0x01E4A0FE`
termina em `goto 0x01E489E0`). O modo já desenhado (`b[0x14686]`) não volta a `0xFF` nesse caminho,
então não serve para saber se o editor acabou de abrir: o desvio desenha o editor inteiro em toda
passada.

**Rádio.** Os endereços, cada um refeito dos bytes de fábrica pela `4p` do `conferir.py`:

| Dado | Onde | Lido em |
|---|---|---|
| banda: 0 FM, 1 AM, 2 LSB, 3 USB, 4 CW | `b[0x12A1B]` | `0x01E44FCE` |
| faixa do AM: LW, MW, SW, USW | `b[0x14CA5]` | `0x01E45016` |
| frequência: FM em 10 kHz, o resto em kHz | `h[0x14CA6]` | `0x01E449DA` |
| navegando pelas memórias (1) e a memória | `b[0x129F9]`, `b[0x14CB4]` | `0x01E44AD2`, `0x01E44AE2` |
| sinal e o nível já desenhado (sinal / 6) | `b[0x14CCF]`, `b[0x14CCE]` | `0x01E451A6` |
| passo do AM e do SSB | `b[0x12A6F]`, `b[0x12A6E]` | `0x01E44D22`, `0x01E44D30` |
| largura, LNA | `b[0x14CBA]`, `b[0x14CBB]` | `0x01E44DC6`, `0x01E44E4E` |
| BFO, com sinal | `h[0x14CBC]` | `0x01E44F16` |
| ajuste em edição (0 = a frequência) | `[0x14CD4]` | `0x01E4934A` |
| zeros na frente do que se digita | `b[0x14A36]` | `0x01E490A0` |

A troca de modulação passa pelo menu do próprio rádio: as tabelas `0x01E61168` (FM e AM) e
`0x01E6137C` (SSB), e o set do item Modulation é `0x01E3DE52`. Ele redesenha a tela de um VFO de
fábrica e escreve "Radio Initialization / Please Wait..." antes de iniciar o chip
(`0x01E3DDBA`); no SSB, o patch do chip receptor vai de `0x01E64D40` a `0x01E68B18` (`0x01E3DC42`).
A troca de banda desliga o chip, espera 0,6 s (`0x01E3DBCC`) e mais duas vezes 1,5 s antes de
ligá-lo; desde a v40 cada uma dessas duas é de 0,25 s. Na espera da tela B (entrada no rádio e
troca de modulação) os ajustes guardados passam pela `confere_radio`: o que estiver fora dos
limites dos setters de fábrica (`0x01E44C68`) volta ao padrão (v40, [histórico](historico.md)).

**DTMF** (tecla `#`). O modo vira 7 (`0x01E4CD06`), e a caixa de fábrica abre em `0x01E4DA54` (no
lado A em y 28..68, no B em y 83..124); os dígitos são redesenhados em `0x01E4A8D4`, os de
`0x14A3A`, quantos em `b[0x14A35]`, até 10. Na tela B a caixa faz parte da cena enquanto o modo for
7: a saída zera o modo antes do redesenho inteiro (`0x01E3C29C`, `0x01E40D68`). O `gancho_dtmf`
(nos envios `0x01E4DA88` e `0x01E4A93C`) desenha quando o modo é 7; até a v46 ele perguntava se a
tela de dois VFOs estava ativa, e `0x01E3B1A2` responde 0 com o modo acima de 5, de modo que saía
sempre a caixa de fábrica.

**A tensão na partida** (`0x01E55E9C`, com a mensagem de partida em tensão): a partida lê o ADC em
`0x01E45FB0` e faz × 444 / 1000, em décimos de volt. O texto de fábrica é "   Voltage" com a tensão
escrita sobre a 8ª posição, e o "ge" se perdia.

**A fonte média.** A mesma letra 5×7, com cada coluna de haste (4 pontos acesos ou mais) e cada
coluna vazia com 2 px e as outras com 1, e a 2ª e a 6ª linha com 2 px: 9 de altura, o traço
vertical do 2x e o horizontal do 1x, 1 px entre letras. Letra sem haste (S, Z, 2) ganha os 2 px nas
duas colunas das pontas, senão sai fina (v40). A largura de cada coluna vem pronta do `fonte.py`,
um bit por coluna em `fonte_b_media`. Fica numa `letra_media()` separada, porque
junto da `letra()` o quadro dela crescia, e ela está no fim do caminho mais fundo da tela principal.

**Duas armadilhas que custaram versões:**

- no desmontado, `h[r13+r0<<1]` e `[r13+r0<<2]` são base + índice × 2 e × 4. Lidos como base +
  índice, seis endereços do rádio saíram errados na v37, e a prévia não pega, porque imita a RAM
  com os mesmos endereços;
- a análise de pilha não segue `call rN`. Os get/set/exibe dos menus são chamados por ponteiro: o
  set no OK (`0x01E49996`) a 144 B, o get e o exibe no motor a 296 B. Um set que redesenha a tela
  (o do Modulation, o CH-NAME pela saída do editor `0x01E3C7AA`) entra nisso; medidos no ponto da
  chamada, todos ficam em até 652 B, abaixo dos 740 B de fábrica.

## 14. As vistas do WATCH (v42 a v45)

O WATCH escolhe quantas frequências a tela principal mostra. A tela B lê o registro dele
([engenharia reversa, seção 18](engenharia-reversa.md#18-a-dupla-escuta-e-o-triple-v42-a-v45)) a
cada desenho e guarda a vista no byte que sobrava dos dígitos da estrutura do estado: os dígitos
passaram de 8 para 7 bytes (até 6 dígitos e o zero), e a estrutura não cresceu, porque ela está no
caminho mais fundo da pilha.

| Vista | Valor | O que a tela mostra |
|---|---|---|
| DUAL | 0 | a tela de sempre: o lado em evidência no cartão e o outro compacto |
| SINGLE | 1 | só o lado em evidência, num cartão da barra ao pé da tela |
| TRIPLE | 2 | o A ou o B em evidência no cartão, e os outros dois em faixas finas |
| TRIPLE com a C no lado B | 3 | a C em evidência no cartão, e o A e o B em faixas |

**SINGLE.** O cartão vai de y 18 a 126. A linha grande (a frequência, ou o nome no MDF de nome) em
3x, em 2x quando não cabe, com os dois últimos dígitos da frequência em 2x; as etiquetas abaixo,
desenhadas pela cena, como no DUAL (chamadas de dentro do cartão, os quadros se somavam e a pilha
passava 28 B da de fábrica); o medidor em y 88 com 12 px de altura; e, com deslocamento, a
frequência de transmissão embaixo (`TX 147.12000`). O outro lado não aparece, e o medidor dele nem
é desenhado.

**TRIPLE.** Cada lado tem uma letra: A, B e C, com o lado B mostrando a C na vista 3. A posição sai
da letra:

- o cartão da que está em evidência em y 18 (A), 39 (B) ou 59 (C), com os 68 px de sempre;
- as faixas de 19 px de altura, de 20 em 20: as que vêm antes do cartão a partir de 18, e as que
  vêm depois a partir de 88 (letra 1 em 88, letra 2 em 108).

| Em evidência | y 18 | y 38/39 | y 58/59 | y 88 | y 108 |
|---|---|---|---|---|---|
| A | cartão A | | | faixa B | faixa C |
| B | faixa A | cartão B | | | faixa C |
| C | faixa A | faixa B | cartão C | | |

Cada faixa tem o selo da letra, o texto que o MDF dela pede (o nome, a frequência ou o número) e o
canal numa etiqueta (`VFO` no VFO). A terceira, a que não está em nenhum dos dois lados, é lida do
chip de dados: o registro do canal, com o MDF-C (ou o MDF-B, quando ela é o B). Canal vazio
aparece como `EMPTY CH`. Desde a v49, a terceira em VFO mostra a frequência do VFO dela (lida do
setor `0xD3000`) com `VFO` na etiqueta. **Recebendo**, a faixa fica em âmbar escuro (`#3b2a0b`, o fundo da
etiqueta âmbar) com o selo e o texto em âmbar (`#fbbf24`). O lado fora de evidência recebe quando
tem sinal, ou quando a dupla escuta está nele com o estado em 4, e o que recebe é a terceira quando
o rodízio está nela.

Só o cartão tem medidor. O redesenho do medidor (`b_medidor`) desenha o medidor do cartão e, para o
lado fora de evidência, as duas faixas que podem estar recebendo; o redesenho de um lado
(`desenha_lado`) e a caixa do DTMF seguem a mesma geometria. As faixas conferem sozinhas se cruzam
a janela aberta e se estão recebendo, e o desenho da faixa é embutido nas duas que a chamam: esse é
o fim do caminho mais fundo da tela no TRIPLE, e um quadro a mais passava da pilha de fábrica.

**Na prévia**, as cenas 33 a 35 são o SINGLE (canal recebendo, VFO com deslocamento e
transmitindo) e as 36 a 39 o TRIPLE (o A recebendo, a C recebendo, o B em evidência com o A
recebendo e a C em evidência).
