/* Previa da tela B no PC: o src/tela.c compilado com -DPREVIA, com a RAM, a janela() e o envia()
 * do firmware imitados como estao mapeados em docs/mapa-da-tela.md. Cada cenario poe o estado do
 * radio na RAM imitada, nos enderecos de verdade, e a tela o le de la como no radio. A tela de
 * 160 x 128 sai em RGB565 (byte alto primeiro, como o buffer de video) num arquivo .rgb565.
 *
 * Uso: previa <pasta de saida>   (o previa.py compila, roda e converte para PNG)
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
 * A do firmware nao tem trava; aqui, janela maior que o buffer para a previa com erro. */
void JANELA(unsigned x0, unsigned x1, unsigned y0, unsigned y1, unsigned limpar, unsigned cor)
{
    unsigned n = (x1 - x0) * (y1 - y0), i;

    if (x1 <= x0 || y1 <= y0 || x1 > 160 || y1 > 128 || n * 2 > TAM_VIDEO) {
        fprintf(stderr, "janela invalida: x %u..%u y %u..%u\n", x0, x1, y0, y1);
        exit(1);
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
void ENVIA(void)
{
    unsigned x0 = memoria[0x14608], y0 = memoria[0x1460A], x1 = memoria[0x1460C], y1 = memoria[0x1460E];
    unsigned x, y, o = 0;

    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++, o += 2)
            tela[y][x] = memoria[VIDEO + o] << 8 | memoria[VIDEO + o + 1];
}

/* le_flash() 0x01E390AC: le do chip de dados (aqui, imitado). */
static u8 chip[0x10000];
void LE_FLASH(u32 a, void *p, unsigned n)
{
    memcpy(p, chip + a, n);
}

/* O WATCH (bancos.c le do cabecalho dos bancos): aqui cada cenario escolhe. */
static unsigned previa_vista, previa_canal_c, previa_mdf_c, previa_c_no_lugar, previa_outro_rx;
unsigned watch_vista(void)
{
    return previa_vista;
}

u32 watch_le_tudo(void)
{
    return previa_vista | previa_mdf_c << 8 | previa_canal_c << 16;
}

/* o modo para a tela (3 = TRIPLE com a C no lado B) | MDF-C << 8, e a terceira: canal | MDF << 16 */
static u32 previa_alvo;                /* v51: bit 31 e o canal da terceira no cartao do lado B (TX-A/B/C) */
unsigned watch_tela(void)
{
    return previa_vista | previa_mdf_c << 8 | previa_alvo;
}

static unsigned previa_terceira, previa_mdf_terceira;
u32 terceira(void)
{
    return previa_terceira | previa_mdf_terceira << 16;
}

/* v49: a C em VFO (bit 24 de terceira()) - a frequencia do VFO dela, em dezenas de Hz */
static u32 previa_freq_vfo;
u32 freq_terceira(u32 t)
{
    (void)t;
    return previa_freq_vfo;
}

/* o rodizio do TRIPLE (bancos.c): a C no lugar do outro lado, e o outro lado recebendo */
unsigned c_no_lugar(void)
{
    return previa_c_no_lugar;
}

unsigned outro_recebendo(void)
{
    return previa_outro_rx;
}

#include "../src/tela.c"

static const char *pasta;

static void grava(const char *nome)
{
    char caminho[512];
    FILE *f;
    unsigned x, y;

    snprintf(caminho, sizeof caminho, "%s/%s.rgb565", pasta, nome);
    if (!(f = fopen(caminho, "wb"))) {
        perror(caminho);
        exit(1);
    }
    for (y = 0; y < 128; y++)
        for (x = 0; x < 160; x++) {
            fputc(tela[y][x] >> 8, f);
            fputc(tela[y][x] & 0xFF, f);
        }
    fclose(f);
}

static void poe32(u32 a, u32 v)
{
    memoria[a] = v;
    memoria[a + 1] = v >> 8;
    memoria[a + 2] = v >> 16;
    memoria[a + 3] = v >> 24;
}

/* Os grupos do menu (v53): a previa nao tem a tabela do firmware, e os nomes e as quantidades vem do
 * montar.py (o previa.py gera o menu_grupos.h). */
#include "menu_grupos.h"

unsigned previa_menu_grupos(void)
{
    return MENU_PREVIA_GRUPOS;
}

unsigned previa_menu_itens(unsigned g)
{
    return menu_previa_itens[g];
}

/* O menu do radio (v56): as duas tabelas de fabrica, como o motor as mostra (o item de memoria com o
 * titulo que 0x01E3D20E monta; flags so com o tipo, que e o que a tela le delas). */
static const char *const previa_radio_fm_am[5] = {"MODULATION", "AMMEMCH", "WORK BAND", "FREQUENCY STEP", "LNA"};
static const char *const previa_radio_ssb[6] = {"MODULATION", "SSBMEMCH", "FREQUENCY STEP", "BANDWIDTH", "LNA",
                                                "BFO"};
static const unsigned char previa_radio_tipos[6] = {0, 4, 0, 0, 0, 13};

const char *previa_item_nome(u32 t, unsigned i)
{
    if (t == RADIO_FM_AM)
        return previa_radio_fm_am[i];
    if (t == RADIO_SSB)
        return previa_radio_ssb[i];
    return menu_previa_nomes[(t - MENU_BASE) / TAM_ITEM + i];
}

unsigned previa_item_flags(u32 t, unsigned i)
{
    return t == RADIO_FM_AM || t == RADIO_SSB ? previa_radio_tipos[i] : 0;
}

unsigned previa_menu_marcas(unsigned p)
{
    return menu_previa_marcas[p];
}

/* O menu no item i do grupo g (nos grupos com NOS_GRUPOS), como os ganchos do tela.c o poem. */
#define NOS_GRUPOS 0xFFu
static void menu_em(unsigned g, unsigned i)
{
    unsigned p = g == NOS_GRUPOS ? 0 : menu_inicio(g);

    poe32(0x14BF4, MENU_BASE + p * TAM_ITEM);
    memoria[0x14BF8] = g == NOS_GRUPOS ? MENU_PREVIA_GRUPOS : menu_previa_itens[g];
    memoria[0x14BFA] = i;
}

/* O grupo pelo nome, e o menu no item de nome i dele (v59: com o MIC e o CALIBRATION, os numeros dos
 * grupos que vem depois mudaram, e o MIC GAIN saiu do TX). */
static unsigned grupo(const char *g)
{
    unsigned n;

    for (n = 0; n < MENU_PREVIA_GRUPOS; n++)
        if (!strcmp(menu_previa_nomes[n], g))
            return n;
    fprintf(stderr, "grupo %s nao existe\n", g);
    exit(1);
}

static void menu_no(const char *g, const char *i)
{
    unsigned n = grupo(g), p = menu_inicio(n), k;

    for (k = 0; k < menu_previa_itens[n]; k++)
        if (!strcmp(menu_previa_nomes[p + k], i)) {
            menu_em(n, k);
            return;
        }
    fprintf(stderr, "item %s nao existe no grupo %s\n", i, g);
    exit(1);
}

/* O radio em repouso: bateria cheia em 8,1 V, sem recepcao, sem zona, os dois lados em canal. */
static void limpa(void)
{
    memset(memoria, 0, sizeof memoria);
    memoria[0x149BC] = 183;            /* 183 x 444 / 1000 = 81 decimos */
    poe32(0x149C0, 3);
    memoria[0x14BD4] = 1;
    memoria[0x14B86] = 0x11;
}

/* canal 0 = VFO (o nibble do modo de trabalho vira 0); senao, 1..960. */
static void lado(unsigned i, u32 rx, u32 tx, const char *nome, unsigned canal, unsigned exibicao,
                 unsigned potencia)
{
    u32 r = 0x14DC4 + i * 31;

    poe32(r, rx);
    poe32(r + 5, tx);
    memset(&memoria[r + 15], 0xFF, 12);
    memcpy(&memoria[r + 15], nome, strlen(nome));
    if (canal) {
        memoria[0x15E0C + i * 4] = (canal - 1) & 0xFF;
        memoria[0x15E0D + i * 4] = (canal - 1) >> 8;
    } else
        memoria[0x14B86] &= i ? 0x0F : 0xF0;
    memoria[0x14B79 + i] = exibicao;
    memoria[0x15E30 + i] = potencia;
}

static void digitando(const char *d)
{
    memoria[0x14A35] = strlen(d);
    memcpy(&memoria[0x14A3A], d, strlen(d));
}

int main(int argc, char **argv)
{
    pasta = argc > 1 ? argv[1] : ".";

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14DC4 + 4] = 1;          /* CT no A */
    memoria[0x14B70] = 1;              /* DW */
    memoria[0x14BDE] = 1;              /* reducao de ruido */
    memoria[0x14B89] = 1;              /* bluetooth */
    tela_b();
    tela_b_medidor(0, 6, 0);           /* o RX desenha o degrau 2 (nivel 6) */
    grava("1-a-em-evidencia-recebendo");

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 1, 2);
    memoria[0x15E04] = 1;
    memoria[0x14B70] = 1;
    memoria[0x14B87] = 1;              /* trava */
    memoria[0x14B75] = 1;              /* DTMF ST */
    tela_b();
    tela_b_medidor(0, 2, 0);           /* DW parou no A: degrau 0 (nivel 2) no A */
    grava("2-b-em-evidencia-dw-no-a");

    limpa();
    lado(0, 12500000, 12500000, "", 0, 1, 2);
    lado(1, 43900000, 43900000, "", 0, 1, 0);
    memoria[0x14DC4 + 11] = 1;         /* N */
    memoria[0x14DC4 + 10] = 2;         /* - */
    memoria[0x14DC4 + 4] = 2;          /* DCS */
    memoria[0x149BC] = 162;            /* 7,1 V */
    poe32(0x149C0, 5);                 /* bateria fraca: contorno vermelho */
    tela_b();
    tela_b_medidor(0, 10, 0);          /* sinal no maximo */
    grava("3-vfo-aereo-am-bateria-fraca");

    limpa();
    lado(0, 43912500, 43912500, "REPETIDORA S", 960, 0, 1);
    lado(1, 14600000, 14600000, "APRS 2M", 1, 2, 0);
    memoria[0x14DC4 + 14] = 1;         /* SCR */
    memoria[0x15E28] = 1;              /* R */
    memoria[0x15370] = 1;              /* MO */
    memoria[0x14B6E] = 1;              /* VOX */
    tela_b();
    grava("4-nome-grande-e-numero");

    limpa();
    lado(0, 14612500, 14672500, "REPETIDORA", 25, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14DC4 + 10] = 1;         /* + */
    memoria[0x14685] = 2;              /* transmitindo */
    tela_b();
    grava("5-transmitindo");

    limpa();
    lado(0, 14552500, 14552500, "", 0, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    digitando("4391");
    tela_b();
    tela_b_digitando();
    grava("6-digitando-frequencia");

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 1, 2);
    memoria[0x15E04] = 1;
    digitando("12");
    tela_b();
    tela_b_digitando();
    grava("7-digitando-canal");

    /* WATCH em SINGLE (v42): so o lado em evidencia, em tela cheia */
    previa_vista = 1;
    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14DC4 + 4] = 1;          /* CT */
    memoria[0x14BDE] = 1;              /* NR */
    tela_b();
    tela_b_medidor(0, 7, 0);
    grava("33-single-canal-recebendo");

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 14652000, 14712000, "", 0, 1, 0);
    memoria[0x14DC4 + 31 + 10] = 1;    /* + */
    memoria[0x15E04] = 1;
    tela_b();
    grava("34-single-vfo-com-deslocamento");

    limpa();
    lado(0, 14612500, 14672500, "REPETIDORA", 25, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14DC4 + 10] = 1;         /* + */
    memoria[0x14685] = 2;              /* transmitindo */
    tela_b();
    grava("35-single-transmitindo");

    /* o DTMF (modo 7) em SINGLE (v47: ate a v46 a caixa saia sempre a de fabrica, gancho_dtmf) */
    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x1533D] = 7;
    digitando("123*5666");
    tela_b();
    grava("35b-dtmf-single");

    /* WATCH em TRIPLE (v42): a C e o canal 5 da memoria, gravado no chip imitado como o radio grava
     * (32 B por canal, RX em BCD com o byte baixo primeiro, nome em +20) */
    previa_vista = 2;
    previa_canal_c = 4;
    previa_mdf_c = 0;
    previa_terceira = 4;               /* a C, fora dos dois lados */
    previa_mdf_terceira = 0;
    {
        static const u8 canal5[32] = {0x00, 0x25, 0x56, 0x14, 0x00, 0x25, 0x56, 0x14};
        memset(&chip[4 * 32], 0xFF, 32);
        memcpy(&chip[4 * 32], canal5, 8);           /* 145.62500 */
        memcpy(&chip[4 * 32 + 20], "APRS 2M", 7);
    }
    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14B70] = 1;              /* TDR: as tres vigiadas */
    tela_b();
    tela_b_medidor(0, 6, 0);
    grava("36-triple-a-recebendo");

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14B70] = 1;
    previa_c_no_lugar = 1;
    previa_outro_rx = 1;               /* a dupla escuta parou na C */
    tela_b();
    grava("37-triple-c-recebendo");
    previa_c_no_lugar = 0;

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14B70] = 1;
    memoria[0x15E04] = 1;              /* B em evidencia: a faixa do A em cima, a C embaixo */
    previa_mdf_terceira = 1;           /* a C pela frequencia */
    tela_b();
    grava("38-triple-b-em-evidencia-a-recebendo");
    previa_outro_rx = 0;

    /* o DTMF no TRIPLE com o B em evidencia */
    memoria[0x1533D] = 7;
    digitando("123*5666");
    tela_b();
    grava("38b-dtmf-triple-b");

    /* v49: a C em VFO, com o A em evidencia - a faixa dela com a frequencia e "VFO" */
    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14B70] = 1;
    previa_terceira = 4 | 1u << 24;
    previa_freq_vfo = 14652000;
    tela_b();
    grava("38c-triple-c-em-vfo");
    previa_terceira = 4;

    /* a C em evidencia (tecla A/B): ela no lado B, e o B (canal 10) fora, como terceira */
    {
        static const u8 canal10[32] = {0x50, 0x62, 0x25, 0x46, 0x50, 0x62, 0x25, 0x46};
        memset(&chip[9 * 32], 0xFF, 32);
        memcpy(&chip[9 * 32], canal10, 8);          /* 462.56250 */
        memcpy(&chip[9 * 32 + 20], "BF888-10", 8);
    }
    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 14562500, 14562500, "APRS 2M", 5, 0, 0);
    memoria[0x14B70] = 1;
    memoria[0x15E04] = 1;
    previa_vista = 3;
    previa_mdf_c = 0;
    previa_terceira = 9;
    previa_mdf_terceira = 0;
    tela_b();
    tela_b_medidor(0, 4, 1);
    grava("39-triple-c-em-evidencia");

    /* o DTMF no TRIPLE com a C em evidencia (a foto do dono, na v45) */
    memoria[0x1533D] = 7;
    digitando("123*5666");
    tela_b();
    grava("39b-dtmf-triple-c");

    /* v51: TX-A/B/C em C, com o A em evidencia e a C fora dos lados. O PTT pos o lado B em evidencia e
     * a C nele so pela RAM: o registro de tela do B com os dados dela, o canal dado por watch_tela (bit
     * 31), e as vagas do lado B ainda as do B (canal 10), que aparece na faixa de baixo */
    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 14562500, 14562500, "APRS 2M", 10, 0, 0);
    memoria[0x14B70] = 1;
    memoria[0x15E04] = 1;
    memoria[0x14685] = 2;              /* transmitindo */
    memoria[0x15384] = 0xE0;           /* a C no lado B, o A estava em evidencia */
    previa_alvo = 1u << 31 | 5u << 16;
    tela_b();
    tela_b_medidor(1, 10, 1);
    grava("39c-triple-tx-na-c");
    previa_alvo = 0;
    previa_vista = 0;

    /* as outras telas (v37 e v38), com o estado das fotos do dono (PIGAO 2 no canal 101) posto na
     * RAM nos enderecos que o firmware usa, e lido pelo mesmo le_telas() do radio */
    {
        static const char *const roger[3] = {"BEEP 2", "COIN", "TONE1200"};
        unsigned i;

#define BASE_TELAS()                                                            \
        limpa();                                                                \
        lado(0, 44062500, 44062500, "PIGAO 2", 101, 0, 0);                       \
        lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);                       \
        memoria[0x14B70] = 1;                                                   \
        memoria[0x14BDE] = 1;                                                   \
        memoria[0x14B75] = 1
/* uma linha de opcao como 0x01E487BC a monta: marca 2 = valor gravado (02 10), 1 = outra opcao
 * (dois espacos), 0 = sem marcador (o nome, tipo 7) */
#define LINHA(r, marca, texto)                                                  \
        do {                                                                    \
            u8 *l_ = &memoria[0x154AF + (r) * 27];                              \
            memset(l_, 0, 27);                                                  \
            if ((marca) == 2) { l_[0] = 2; l_[1] = 0x10; l_ += 2; }             \
            else if ((marca) == 1) { l_[0] = ' '; l_[1] = ' '; l_ += 2; }       \
            strcpy((char *)l_, texto);                                          \
        } while (0)
#define RADIO_EM(f)                                                             \
        do { memoria[0x14CA6] = (f) & 0xFF; memoria[0x14CA7] = (f) >> 8; } while (0)

        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "ROGER");
        menu_no("TX", "ROGER");
        for (i = 0; i < 3; i++)
            LINHA(i, i == 1 ? 2 : 1, roger[i]);
        memoria[0x14C10] = 1;
        memoria[0x14BF9] = 1;
        telas_b(1, 0);
        grava("9-menu-roger");

        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "SQL");
        menu_no("AUDIO", "SQL");
        LINHA(0, 2, "1");
        LINHA(1, 1, "2");
        LINHA(2, 1, "3");
        memoria[0x14C10] = 1;
        memoria[0x14BF9] = 1;
        telas_b(1, 0);
        grava("10-menu-sql-mudando-o-valor");

        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "NOISE REDUCTION");
        menu_no("AUDIO", "NOISE REDUCTION");
        LINHA(0, 1, "OFF");
        LINHA(1, 2, "ON");
        memoria[0x14C10] = 1;
        memoria[0x14BF9] = 1;
        telas_b(1, 0);
        grava("11-menu-titulo-longo");

        /* v50: o TX-A/B/C, com a C em evidencia (TRIPLE, a C no lado B): o resumo de cima com o selo C
         * (ate a v49 saia B) */
        BASE_TELAS();
        previa_vista = 3;
        memoria[0x15E04] = 1;
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "TX-A/B/C");
        menu_no("WATCH", "TX-A/B/C");
        LINHA(0, 1, "B");
        LINHA(1, 2, "C");
        LINHA(2, 1, "OFF");
        memoria[0x14C10] = 1;
        memoria[0x14BF9] = 1;
        telas_b(1, 0);
        grava("11b-menu-txabc-com-a-c");
        previa_vista = 0;

        /* o item de memoria do menu do radio (0x01E3D20E): S, Z e 2 da fonte media sem haste (v40) */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "SSBMEMCH");
        poe32(0x14BF4, RADIO_SSB);
        memoria[0x14BF8] = 6;
        memoria[0x14BFA] = 1;
        memoria[0x14BF9] = 1;
        memoria[0x12A1B] = 3;
        LINHA(0, 1, "CH-01");
        LINHA(1, 2, "02");
        LINHA(2, 1, "03");
        memoria[0x14C10] = 1;
        telas_b(1, 0);
        grava("29-menu-ssbmemch");

        /* o BANK (bancos.c, bank_exibe): o banco ligado vem com o byte 0x01 no fim */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "BANK");
        menu_no("BANKS", "BANK");
        memoria[0x14C00] = 0;
        LINHA(0, 2, "1 BAOFENG\x01");
        LINHA(1, 1, "2 MOTOROLA");
        LINHA(2, 1, "3 ESPECIAL\x01");
        memoria[0x14C10] = 1;
        memoria[0x14BF9] = 1;
        telas_b(1, 0);
        grava("18-menu-bank");

        /* o BANK NAME e o CH-NAME (tipo 7): uma linha so, sem marcador, e o cursor do motor parado em
         * outra linha (o tipo 7 nao mexe nele) */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "NAME");
        menu_no("BANKS", "NAME");
        memoria[0x14C00] = 7;
        LINHA(0, 0, "MOTOROLA");
        memoria[0x14C10] = 2;
        telas_b(1, 0);
        grava("19-menu-bank-name");

        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "CH-NAME");
        menu_no("MEMORY", "CH-NAME");
        memoria[0x14C00] = 7;
        LINHA(0, 0, "BF888-01");
        telas_b(1, 0);
        grava("20-menu-ch-name");

        /* o editor de nome (tipo 7 editando): o texto em 0x14A3A, o tamanho em b[0x14A35], o maximo
         * em b[0x14A36] e o modo em b[0x14A38] (1 = ABC); um nome curto em 2x e um longo no medio */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "NAME");
        menu_no("BANKS", "NAME");
        memoria[0x14C00] = 7;
        memoria[0x14BF9] = 1;
        digitando("MOTOR");
        memoria[0x14A36] = 12;
        memoria[0x14A38] = 1;
        telas_b(1, 0);
        grava("27-editor-de-nome");
        digitando("REPETIDORA 2");
        memoria[0x14A38] = 2;              /* 123: a tecla # so alterna ABC e 123 (v52) */
        telas_b(1, 0);
        grava("28-editor-de-nome-longo");

        /* o OFFSET sendo digitado (tipo 3, 0x01E4881A): a linha 0 com os digitos sobre "----.----" */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "OFFSET");
        menu_no("REPEATER", "OFFSET");
        memoria[0x14C00] = 3;
        LINHA(0, 1, "123.4---");
        memoria[0x14BF9] = 1;
        telas_b(1, 0);
        grava("21-menu-offset-digitando");

        /* v53: os grupos do menu, a janela de pasta - o BANKS (3 itens) e o CHANNEL (7, o maior) */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        menu_em(NOS_GRUPOS, grupo("BANKS"));
        telas_b(1, 0);
        grava("40-menu-grupos-banks");
        menu_em(NOS_GRUPOS, grupo("CHANNEL"));
        telas_b(1, 0);
        grava("41-menu-grupos-channel");
        menu_em(NOS_GRUPOS, grupo("WATCH"));
        telas_b(1, 0);
        grava("42-menu-grupos-watch");

        /* v55: escolhendo o item dentro do grupo, na mesma janela - o cursor num cartao e o valor em
         * "L" embaixo (das linhas que o motor montou para ele); o CHANNEL (7 itens + o valor) rola */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        menu_no("TX", "ROGER");
        LINHA(0, 1, "BEEP 2");
        LINHA(1, 2, "COIN");
        LINHA(2, 1, "TONE1200");
        telas_b(1, 0);
        grava("43-menu-item-roger");
        menu_no("BANKS", "NAME");
        memoria[0x14C00] = 7;
        LINHA(0, 0, "MOTOROLA");
        LINHA(1, 0, "");
        LINHA(2, 0, "");
        telas_b(1, 0);
        grava("44-menu-item-nome");
        memoria[0x14C00] = 0;
        menu_no("CHANNEL", "ENCRYPT");
        LINHA(0, 2, "OFF");
        LINHA(1, 1, "ON");
        LINHA(2, 0, "");
        telas_b(1, 0);
        grava("45-menu-item-rola-para-baixo");
        menu_no("CHANNEL", "W/N");
        LINHA(0, 1, "WIDE");
        LINHA(1, 2, "NARROW");
        telas_b(1, 0);
        grava("46-menu-item-rola-para-cima");
        menu_no("SYSTEM", "RESET");
        LINHA(0, 2, "VFO");
        LINHA(1, 1, "ALL");
        telas_b(1, 0);
        grava("47-menu-item-sem-valor");
        /* v58: o cursor no 3o item, com o valor - o cartao (y 63..87) atravessa a divisa das faixas do
         * firmware em y 80, e ate a v57 a faixa de baixo nao o pintava */
        menu_no("CHANNEL", "RX MODULATION");
        LINHA(0, 2, "FM");
        LINHA(1, 1, "AM");
        LINHA(2, 0, "");
        telas_b(1, 0);
        grava("47b-menu-item-terceiro");

        /* v56: o menu do radio na janela de pasta - uma pasta so, "RADIO" e a banda no titulo, sem os
         * pontos dos grupos; no USB os 6 itens com o valor rolam */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        memoria[0x12A1B] = 3;
        poe32(0x14BF4, RADIO_SSB);
        memoria[0x14BF8] = 6;
        memoria[0x14BFA] = 3;
        LINHA(0, 1, "1.2K");
        LINHA(1, 2, "2.2K");
        LINHA(2, 1, "3.0K");
        telas_b(1, 0);
        grava("49-menu-radio-usb");
        memoria[0x14BFA] = 1;
        LINHA(0, 1, "CH-01");
        LINHA(1, 2, "02");
        LINHA(2, 1, "03");
        telas_b(1, 0);
        grava("50-menu-radio-memoria");
        memoria[0x12A1B] = 1;
        poe32(0x14BF4, RADIO_FM_AM);
        memoria[0x14BF8] = 5;
        memoria[0x14BFA] = 0;
        LINHA(0, 1, "FM");
        LINHA(1, 2, "AM");
        LINHA(2, 1, "LSB");
        telas_b(1, 0);
        grava("51-menu-radio-am");
        memoria[0x14BFA] = 2;                /* v58: o 3o item do menu do radio, o WORK BAND */
        LINHA(0, 1, "LW");
        LINHA(1, 2, "MW");
        LINHA(2, 1, "SW");
        telas_b(1, 0);
        grava("51b-menu-radio-terceiro");
        memoria[0x14BFA] = 0;
        LINHA(0, 1, "FM");
        LINHA(1, 2, "AM");
        LINHA(2, 1, "LSB");
        strcpy((char *)&memoria[0x15494], "Modulation");
        memoria[0x14BF9] = 1;
        memoria[0x14C10] = 1;
        telas_b(1, 0);
        grava("52-menu-radio-aberto");

        /* o VERSION aberto (v55): so leitura, sem cursor - o EXIT o fecha */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        strcpy((char *)&memoria[0x15494], "VERSION");
        menu_no("SYSTEM", "VERSION");
        memoria[0x14C00] = 8;
        memoria[0x14BF9] = 1;
        LINHA(0, 0, "Firmware:V2.7T");
        LINHA(1, 0, "Hardware:V01T32");
        memoria[0x14C10] = 1;
        telas_b(1, 0);
        grava("48-menu-version-aberto");

        /* v59: a pasta CALIBRATION nos grupos, o COMPRESSOR na pasta MIC e o CRYSTAL FINE, na pasta e
         * aberto (as linhas como o cristal_exibe as monta) */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        menu_em(NOS_GRUPOS, grupo("CALIBRATION"));
        telas_b(1, 0);
        grava("53-menu-grupos-calibration");
        menu_no("MIC", "COMPRESSOR");
        LINHA(0, 1, "LOW");
        LINHA(1, 2, "MID");
        LINHA(2, 1, "HIGH");
        telas_b(1, 0);
        grava("54-menu-mic-compressor");
        menu_no("CALIBRATION", "CRYSTAL FINE");
        LINHA(0, 1, "+110 Hz");
        LINHA(1, 2, "+120 Hz");
        LINHA(2, 1, "+130 Hz");
        telas_b(1, 0);
        grava("55-menu-calibration-cristal");
        strcpy((char *)&memoria[0x15494], "CRYSTAL FINE");
        memoria[0x14BF9] = 1;
        memoria[0x14C10] = 1;
        telas_b(1, 0);
        grava("56-menu-cristal-aberto");
        memoria[0x14BF9] = 0;

        /* v61: o BRIGHTNESS na pasta DISPLAY, com o valor em 35% */
        menu_no("DISPLAY", "BRIGHTNESS");
        LINHA(0, 1, "60%");
        LINHA(1, 2, "35%");
        LINHA(2, 1, "20%");
        telas_b(1, 0);
        grava("58-menu-display-brightness");

        /* v59 e v62: o menu aberto recebendo (o estado da recepcao em 4, que o b_sinal acompanha e o
         * le_estado le) - o lado em evidencia na barra de cima, no lugar dos icones: o B pelo nome, a C
         * (TRIPLE, a C no lado B) pela frequencia, um nome longo cortado antes da tensao, e o sinal sumindo */
        BASE_TELAS();
        memoria[0x1533D] = 1;
        memoria[0x15E04] = 1;
        memoria[0x15367] = 4;
        memoria[0x129D4] = 1;              /* v63: o audio da recepcao aberto */
        menu_em(NOS_GRUPOS, grupo("AUDIO"));
        telas_b(1, 0);
        grava("57-menu-recebendo-b");
        previa_vista = 3;
        previa_mdf_c = 1;
        telas_b(1, 0);
        grava("57b-menu-recebendo-c");
        previa_vista = 0;
        previa_mdf_c = 0;
        memoria[0x15E04] = 0;
        lado(0, 44062500, 44062500, "REPETIDORA 2", 101, 0, 0);
        telas_b(1, 0);
        grava("57c-menu-recebendo-nome-longo");
        memoria[0x15367] = 2;
        memoria[0x129D4] = 0;
        telas_b(1, 0);
        grava("57d-menu-sinal-sumiu");

        BASE_TELAS();
        memoria[0x1533D] = 17;
        telas_b(17, 1);
        grava("12-scanqt-rodando");
        telas_b(17, 0);
        grava("13-scanqt-parado");

        BASE_TELAS();
        memoria[0x1533D] = 2;
        RADIO_EM(10210);                          /* 102,10 MHz */
        memoria[0x14CCF] = 30;                    /* nivel 5 */
        telas_b(2, 0);
        grava("14-radio-fm");

        BASE_TELAS();
        memoria[0x1533D] = 2;
        memoria[0x12A1B] = 1;                     /* AM */
        memoria[0x14CA5] = 1;                     /* MW */
        RADIO_EM(540);                            /* abaixo de 1000 sai em kHz */
        memoria[0x129F9] = 1;                     /* memorias */
        memoria[0x14CB4] = 2;                     /* a terceira */
        strcpy((char *)&chip[0xC110 + 2 * 16], "RADIO BAND");
        memoria[0x14CCF] = 45;                    /* nivel 7 */
        memoria[0x12A6F] = 2;                     /* passo 10K */
        memoria[0x14CD4] = 2;                     /* mudando o BW */
        telas_b(2, 0);
        grava("15-radio-am-mudando-bw");

        BASE_TELAS();
        memoria[0x1533D] = 2;
        memoria[0x12A1B] = 2;                     /* LSB */
        RADIO_EM(7074);                           /* 7,074 MHz */
        memoria[0x14CCF] = 75;                    /* S9 + 20 */
        memoria[0x12A6E] = 0;                     /* passo 1K */
        memoria[0x14CBA] = 3;                     /* 2.2K */
        memoria[0x14CBB] = 1;                     /* LNA 0 */
        memoria[0x14CBC] = (u8)-300;              /* BFO -300 */
        memoria[0x14CBD] = (u16)-300 >> 8;
        memoria[0x14CD4] = 4;                     /* mudando o BFO */
        telas_b(2, 0);
        grava("16-radio-lsb-mudando-bfo");

        BASE_TELAS();
        memoria[0x1533D] = 2;
        RADIO_EM(10210);
        memoria[0x14CCF] = 30;
        digitando("98");                          /* 0x01E49078: "98-.--" MHz */
        telas_b(2, RADIO_DIGITANDO);
        grava("22-radio-digitando");

        BASE_TELAS();
        memoria[0x1533D] = 2;
        RADIO_EM(10210);
        memoria[0x14CCF] = 30;
        telas_b(2, RADIO_BUSCANDO);
        grava("23-radio-seek");

        BASE_TELAS();
        memoria[0x1533D] = 2;
        telas_b(TELA_INICIO_RADIO, 0);
        grava("17-radio-iniciando");

        /* o espectro (modo 20): 160 pontos de 6,25 kHz em 145 MHz, um sinal forte que passou do
         * limite e um fraco, o ruido embaixo, a coluna 95 sendo medida */
        {
            static struct espectro_previa p;
            unsigned c;

            p.inicio = 14500000;
            p.fim = 14500000 + 159 * 625;
            p.atual = 95;
            p.freq = p.inicio + p.atual * 625;
            p.pico = p.inicio + 70 * 625;
            p.dbm = -71;
            p.limite = 40;
            for (c = 0; c < 160; c++) {
                int d1 = (int)c - 70, d2 = (int)c - 124, h = 3 + (c * 7 + c / 3) % 5;

                h += d1 * d1 < 36 ? 50 - d1 * d1 : 0;
                h += d2 * d2 < 16 ? 22 - 3 * (d2 < 0 ? -d2 : d2) : 0;
                p.alturas[c] = h > 64 ? 64 : h;
            }
            BASE_TELAS();
            memoria[0x1533D] = 20;
            espectro_b(&p);
            grava("30-espectro");

            /* o mesmo pelos ganchos, na ordem em que o firmware os chama: a entrada (a limpeza e
             * os pedacos de cima e de baixo), as 5 fatias do grafico (janela, cada coluna com x
             * relativo e o topo de fabrica, a linha do limite) e, no fim da passada, o dBm e o pico.
             * Tem de sair igual ao 30. */
            {
                unsigned k, i;

                BASE_TELAS();
                memoria[0x1533D] = 20;
                telas_b(20, 0);
                b_esp_freq(p.freq);
                b_esp_rotulos(p.pontos, p.passo);
                b_esp_modo(p.modo);
                b_esp_eixo(p.inicio, p.fim);
                for (k = 0; k < 5; k++) {
                    gancho_fatia(k * 32, k * 32 + 32, 49, 113);
                    for (i = 0; i < 32; i++)
                        b_esp_coluna(i, 64 - p.alturas[k * 32 + i], k * 32 + i == p.atual);
                    gancho_limite(0, 31, 64 - p.limite, 64 - p.limite);
                    ENVIA();
                }
                b_esp_dbm(p.dbm);
                b_esp_pico(p.pico);
                grava("32-espectro-pelos-ganchos");
            }

            BASE_TELAS();
            memoria[0x1533D] = 20;
            strcpy((char *)&memoria[0x14A3A], "1452");
            memoria[0x14A35] = 4;
            p.digitando = 1;
            p.pontos = 2;
            p.passo = 5;
            p.modo = 1;
            espectro_b(&p);
            grava("31-espectro-digitando-am");
        }

        /* o DTMF (modo 7) sobre o lado em evidencia, A e depois B */
        limpa();
        lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
        lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
        memoria[0x1533D] = 7;
        digitando("123*5666");
        tela_b();
        grava("24-dtmf-no-a");
        memoria[0x15E04] = 1;
        tela_b();
        grava("25-dtmf-no-b");

        tensao_b(81);
        grava("26-tensao-na-partida");

        /* o 0 segurado (acao 33 das teclas, v52): o mesmo cartao, por cima da tela principal */
        limpa();
        lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
        lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
        tela_b();
        tensao_tecla_b(81);              /* a mesma da barra (183 x 444 / 1000) */
        grava("26b-tensao-com-o-0");
    }

    /* a fonte inteira: as 64 letras e os icones em 1x, e os digitos em 2x */
    for (unsigned y = 0; y < 128; y += 32) {
        janela_b(0, 160, y, y + 32, COR_FUNDO);
        texto(4, 4, " !\"#$%&'()*+,-./", 1, COR_TEXTO);
        texto(4, 16, "0123456789:;<=>?", 1, COR_TEXTO);
        texto(4, 28, "@ABCDEFGHIJKLMNO", 1, COR_TEXTO);
        texto(4, 40, "PQRSTUVWXYZ[\\]^_", 1, COR_TEXTO);
        texto(110, 40, ICONE_NOTA " " ICONE_BLUETOOTH " " ICONE_CADEADO, 1, COR_VERDE);
        texto(4, 58, "01234", 2, COR_BRANCO);
        texto(4, 78, "56789", 2, COR_BRANCO);
        texto(4, 100, "2 5 2.5 25", 2, COR_VERDE);
        texto(80, 58, "MEDIA 0123", MEDIA, COR_BRANCO);
        texto(80, 72, "NOISE RED.", MEDIA, COR_TEXTO_AMBAR);
        ENVIA();
    }
    grava("8-fonte");
    return 0;
}
