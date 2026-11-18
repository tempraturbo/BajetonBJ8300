/* A maquina da previa (v162; ate a v161 no alto do previa.c): o src/tela.c compilado com -DPREVIA, com a RAM, a
 * janela() e o envia() do firmware imitados como estao mapeados em docs/mapa-da-tela.md, o chip de dados e as tabelas
 * do menu. A tela de 160 x 128 fica em tela[][], em RGB565. Quem inclui (a previa, com os cenarios, e o tela_pc.c, o
 * desenho do app a partir do retrato) da o que o bancos.c e o mensagem.c dariam: watch_tela, terceira,
 * freq_terceira, watch_le_tudo, c_no_lugar, outro_recebendo, nr_le, porta_le, em_canal e lista_item.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

u8 memoria[0x20000];
static u16 tela[128][160];

#define VIDEO      0x00010190u
#define TAM_VIDEO  10240u

/* janela(x0, x1, y0, y1, limpar, cor) 0x01E3AD34: grava os limites e, com limpar, enche o buffer.
 * A do firmware nao tem trava; aqui, janela maior que o buffer para a previa com erro. O app (tela_pc.c) define o
 * JANELA_INVALIDA antes de incluir: a janela fica sem efeito, e ele marca a tela (v162). */
#ifndef JANELA_INVALIDA
#define JANELA_INVALIDA(x0, x1, y0, y1) \
    (fprintf(stderr, "janela invalida: x %u..%u y %u..%u\n", x0, x1, y0, y1), exit(1))
#endif
void JANELA(unsigned x0, unsigned x1, unsigned y0, unsigned y1, unsigned limpar, unsigned cor)
{
    unsigned n = (x1 - x0) * (y1 - y0), i;

    if (x1 <= x0 || y1 <= y0 || x1 > 160 || y1 > 128 || n * 2 > TAM_VIDEO) {
        JANELA_INVALIDA(x0, x1, y0, y1);
        return;
    }
    memoria[0x14608] = x0;
    memoria[0x1460A] = y0;
    memoria[0x1460C] = x1;
    memoria[0x1460E] = y1;
    memoria[0x14610] = (x1 - x0) * 2 & 0xFF;
    memoria[0x14611] = (x1 - x0) * 2 >> 8;
    for (i = 0; limpar && i < n; i++) {
        memoria[VIDEO + i * 2] = cor >> 8;
        memoria[VIDEO + i * 2 + 1] = cor;
    }
}

/* envia() 0x01E3ADBC: manda o buffer da janela aberta para a tela. */
void tela_aprs(u32 a0, u32 a1, unsigned aberto, const u16 *num, unsigned faixas);   /* telas.c (v74) */
void caixa_b(void);                                                                  /* telas.c (v112) */

void ENVIA(void)
{
    unsigned x0 = memoria[0x14608], y0 = memoria[0x1460A], x1 = memoria[0x1460C], y1 = memoria[0x1460E];
    unsigned x, y, o = 0;

    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++, o += 2)
            tela[y][x] = memoria[VIDEO + o] << 8 | memoria[VIDEO + o + 1];
}

/* le_flash() 0x01E390AC: le do chip de dados (aqui, imitado). Fora dele, 0xFF, como o chip apagado (v162: no app o
 * endereco vem da RAM do radio). */
static u8 chip[0x10000];
void LE_FLASH(u32 a, void *p, unsigned n)
{
    if (a < sizeof chip && n <= sizeof chip - a)
        memcpy(p, chip + a, n);
    else
        memset(p, 0xFF, n);
}

#include "../src/tela.c"

/* Os grupos do menu (v53): a previa nao tem a tabela do firmware, e os nomes e as quantidades vem do
 * montar.py (o previa.py gera o menu_grupos.h). */
#include "menu_grupos.h"

unsigned previa_menu_grupos(void)
{
    return MENU_PREVIA_GRUPOS;
}

/* v162: os indices vem da RAM (no app, a do radio): fora das tabelas, nada. */
#define CONTA_DE(v) (sizeof(v) / sizeof((v)[0]))

unsigned previa_menu_itens(unsigned g)
{
    return g < CONTA_DE(menu_previa_itens) ? menu_previa_itens[g] : 0;
}

/* O menu do radio (v56): as duas tabelas de fabrica, como o motor as mostra (o item de memoria com o
 * titulo que 0x01E3D20E monta; flags so com o tipo, que e o que a tela le delas). */
static const char *const previa_radio_fm_am[5] = {"MODULATION", "AMMEMCH", "WORK BAND", "FREQUENCY STEP", "LNA"};
static const char *const previa_radio_ssb[6] = {"MODULATION", "SSBMEMCH", "FREQUENCY STEP", "BANDWIDTH", "LNA",
                                                "BFO"};
static const unsigned char previa_radio_tipos[6] = {0, 4, 0, 0, 0, 13};

const char *previa_item_nome(u32 t, unsigned i)
{
    u32 k = (t - MENU_BASE) / TAM_ITEM + i;

    if (t == RADIO_FM_AM)
        return i < CONTA_DE(previa_radio_fm_am) ? previa_radio_fm_am[i] : "";
    if (t == RADIO_SSB)
        return i < CONTA_DE(previa_radio_ssb) ? previa_radio_ssb[i] : "";
    return t >= MENU_BASE && k < CONTA_DE(menu_previa_nomes) ? menu_previa_nomes[k] : "";
}

unsigned previa_item_flags(u32 t, unsigned i)
{
    return (t == RADIO_FM_AM || t == RADIO_SSB) && i < CONTA_DE(previa_radio_tipos) ? previa_radio_tipos[i] : 0;
}

unsigned previa_menu_marcas(unsigned p)
{
    return p < CONTA_DE(menu_previa_marcas) ? menu_previa_marcas[p] : 0;
}

