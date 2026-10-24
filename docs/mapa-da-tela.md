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
desenhador de glifo (`0x01E388F4`, sexto argumento) como para o enchimento do vão entre caracteres
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

A cor fica em `COR_REALCE`, no `firmware/src/bancos.c` — uma linha só. O valor atual é `0x02C0`
(#005900). O buffer de vídeo guarda o byte alto primeiro, então o gancho do envio usa `COR_BUF`, o
mesmo valor com os bytes trocados (`0xC002`); o enchimento recebe a cor normal, porque a própria
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
