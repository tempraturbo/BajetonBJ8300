# Mapa da tela principal

> 🇬🇧 English: [screen-map.md](screen-map.md)

Levantado em 2026-10-24 no desassemblado do firmware V0.12, depois de duas versões erradas. Tudo
aqui tem endereço: quem não tiver, não está provado e está marcado como tal.

## 1. Como o firmware desenha

Não existe framebuffer da tela inteira. O desenho é feito **uma janela por vez**:

```
janela(x0, x1, y0, y1, limpar, cor)   0x01E3AD34
    grava os limites nos globais (0x145CA, 0x145CE, 0x145D0) e, com limpar != 0,
    enche a janela com cor
<desenha texto, barra, ícones dentro dela>
envia(...)                            0x01E3ADBC
    lê os mesmos globais e manda o buffer para o LCD
```

O buffer é a memória de vídeo em **0x10190**, com **10240 bytes** — uma banda de **160×32** em
RGB565. E aqui está a armadilha que derrubou a v13:

> **`janela()` não tem trava de tamanho.** Ela calcula `(x1-x0) * (y1-y0) * 2` e sai escrevendo a
> partir de 0x10190 (laço em `0x01E3AD8E`). Uma janela maior que 10240 bytes atropela o que vem
> depois: o buffer de texto (0x129B0), a base global (0x14680), as flags de redesenho (0x15367) e
> os mapas de canal. O resultado é reset.

Por isso nenhuma chamada de fábrica passa de 32 linhas em largura cheia — a maior é exatamente
160×32, em `0x01E4A78A`.

## 2. Quem está selecionado: `b[0x15E04]`

**0 = VFO A, diferente de 0 = VFO B.** É a variável que o próprio firmware consulta para montar a
tela:

| Onde | O que decide |
|---|---|
| `0x01E41432` | a faixa da barra do A sai em **74** (expandido) ou **41** (compacto) |
| `0x01E4B308` | o número do canal sai em **62** ou **101** |
| `0x01E4B264` | a janela de nome+frequência sai em **30** ou **69** |

Não confundir com **`tx_ab` (`0x14B84`)**, que é o ajuste `0x9018` da memória de dados espelhado
em RAM. Usar o ajuste no lugar do estado da tela foi um dos dois erros da v13.

## 3. As três janelas de um bloco

| Janela | Sítio da chamada | Geometria | y no VFO A | y no VFO B |
|---|---|---|---|---|
| nome + frequência | `0x01E4B270` | `(7, 160, y, y+24)` | 30 | 69 |
| barra de sinal | `0x01E41468` | `(0, 160, y, y+14)` | 41 (compacto) ou 74 (expandido) | 114 |
| número do canal | `0x01E4B314` | `(127, 159, y, y+9)` | 62 | 101 |

Todas abaixo do limite do buffer: 153×24×2 = 7344 B, 160×14×2 = 4480 B, 32×9×2 = 576 B.

A janela do número do canal **acompanha a seleção** (62 quando A está selecionado, 101 quando B),
ou seja, é sempre a do lado em evidência; o número do outro lado é desenhado por outro caminho.

### Sequência da tela

```
0x01E4CB72 / 0x01E4CC54   laço da interface, quando b[0x15E13] == 1
  └─ 0x01E4B29C           desenha um bloco
       ├─ 0x01E413BE      faixa da barra de sinal  → janela em 0x01E41468
       ├─ janela          número do canal          → 0x01E4B314
       ├─ 0x01E3BC68      conteúdo do número
       └─ 0x01E3ADBC      envia
0x01E4B1DE                nome e frequência        → janela em 0x01E4B270
  ├─ 0x01E3BACE           nome (texto)
  ├─ 0x01E3BB7A           frequência
  └─ 0x01E3ADBC           envia
```

## 4. O que ainda não está mapeado

- Onde é desenhado o **número do canal do lado NÃO selecionado** (aparece nas fotos, mas não sai
  de nenhuma das três janelas acima).
- A linha separadora branca entre os dois blocos e o indicador `H` de potência: não passam por
  `janela()` com os argumentos que o varredor reconhece.
- As outras telas (menu, VFO simples, FM) usam as mesmas rotinas com outras coordenadas — o realce
  não foi testado nelas.

## 5. Como o realce foi feito, e por que é seguro

O gancho entra nas **três chamadas de janela** da tabela acima e **não muda a geometria**: ele só
escolhe a cor com que o firmware já ia encher aquela janela.

```c
void gancho_realce(unsigned x0, unsigned x1, unsigned y0, unsigned y1,
                   unsigned limpar, unsigned cor)
{
    unsigned limite = y1 - y0 > 14 ? 60 : 100;   /* 24 linhas = nome+frequencia */

    if ((y0 >= limite) == (LADO_B != 0))
        cor = COR_REALCE;
    JANELA(x0, x1, y0, y1, limpar, cor);
}
```

A altura diz qual das três janelas é, e dela sai o limite de `y` que separa os lados. Conferido no
código de máquina gerado (`0x01E963FA`): **r0–r3 não são tocados**, então nenhuma chamada pode
pedir área maior que a original — o estouro da v13 é impossível por construção.

Tabela-verdade das combinações reais:

| Janela | altura → limite | y | lado | pinta quando |
|---|---|---|---|---|
| nome+freq A | 24 → 60 | 30 | A | A selecionado |
| nome+freq B | 24 → 60 | 69 | B | B selecionado |
| barra A expandido | 14 → 100 | 74 | A | A selecionado |
| barra A compacto | 14 → 100 | 41 | A | nunca (só existe com B selecionado) |
| barra B | 14 → 100 | 114 | B | B selecionado |
| número do canal | 9 → 100 | 62 ou 101 | segue a seleção | sempre o lado em evidência |

A cor fica em `COR_REALCE`, no `firmware/src/bancos.c` — uma linha só. O valor atual é `0x0380`
(#007100). Como o gancho passa a cor como valor de 16 bits, aqui **não** vale o limite de 8 bits do
remendo direto de cor (que só alcança verdes escuros demais para enxergar).
