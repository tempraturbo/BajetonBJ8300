/* O desenho da tela do BJ-8300 no app (v162): o retrato que o controle remoto manda (o formato no alto do
 * src/remoto.c, as faixas no src/retrato.h) posto na maquina da previa (maquina.c), e a tela desenhada pelo mesmo
 * codigo do radio (src/tela.c e telas.c), como o firmware a desenharia inteira. O tela_pc.py compila esta biblioteca
 * para o Tempra Link do Windows e para o celular.
 *
 * O que o firmware passa por parametro, e nao esta na RAM, sai daqui: a tela de cada modo (o b_tela, o telas_faixa) e
 * a variante dela (no radio, a digitacao pelos digitos; no scan QT, b[0x148D0] = 2 rodando), e o medidor S, que vem
 * da resposta ('~' 'r', o byte do sinal). As telas que o firmware desenha com dados de fora do retrato (o espectro, que
 * fica sem as colunas, e a tela do APRS e do Morse, que vem do historico na flash) voltam marcadas, e o app as
 * explica. Enquanto o laco do APRS ou do Morse roda, o retrato nem sai (o laco do sinal esta parado; remoto.c): o app
 * ve a resposta sem ele.
 */
/* uma janela fora da tela (o radio nao confere): sem efeito, e a tela marcada (maquina.c) */
static unsigned janela_ruim;
#define JANELA_INVALIDA(x0, x1, y0, y1) (janela_ruim = 1)
#include "maquina.c"
#include "../src/retrato.h"

/* no .obj que o Delphi liga (TELA_OBJ), nada: o -export do dllexport e so para a DLL do teste */
#if defined(TELA_OBJ)
#define EXPORTA
#elif defined(_WIN32)
#define EXPORTA __declspec(dllexport)
#else
#define EXPORTA __attribute__((visibility("default")))
#endif

/* o que a tela pede ao bancos.c e ao mensagem.c: os valores do retrato */
static u32 grandes[4];
static u8 pequenos[6];

unsigned watch_tela(void) { return grandes[0]; }
u32 terceira(void) { return grandes[1]; }
u32 freq_terceira(u32 t) { (void)t; return grandes[2]; }   /* o retrato a manda com o terceira() de agora */
u32 watch_le_tudo(void) { return grandes[3]; }
unsigned c_no_lugar(void) { return pequenos[0]; }
unsigned outro_recebendo(void) { return pequenos[1]; }
u16 nr_le(void) { return pequenos[2]; }
u16 porta_le(void) { return pequenos[3]; }
unsigned em_canal(void) { return pequenos[4]; }
unsigned lista_item(void) { return pequenos[5]; }

#define TELA_BJ_INTEIRA   0       /* desenhada como o radio a mostra */
#define TELA_BJ_PARCIAL   1       /* o espectro: o quadro, sem as colunas; a caixa da MESSAGES, sem a tela de baixo */
#define TELA_BJ_FORA      2       /* o APRS ou o Morse chegando: so no radio */
#define TELA_BJ_TAMANHO  -1       /* o retrato nao tem o tamanho deste formato */
#define TELA_BJ_VERSAO   -2       /* de outra versao (outras faixas, ou outras tabelas do menu) */
#define TELA_BJ_SOMA     -3       /* bytes estragados */
#define TELA_BJ_JANELA    3       /* desenhada, mas o codigo pediu uma janela fora da tela (um estado que a previa
                                   * nao conhece): pode faltar pedaco */

EXPORTA int tela_bj_versao(void)
{
    return RETRATO_VERSAO;
}

/* O retrato inteiro, do '~' as somas. */
EXPORTA int tela_bj_tamanho(void)
{
    return 5 + RETRATO_TAMANHO + 2;
}

static u32 le32_de(const u8 *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

/* O retrato na RAM, no chip e nos valores; devolve 0 ou o erro. */
static int poe_retrato(const u8 *r, int n)
{
    const u8 *v = r + 5, *f = v + RETRATO_VALORES, *m = f + RETRATO_FLASH;
    unsigned s1 = 0, s2 = 0, i, j, a, c;

    if (n != tela_bj_tamanho() || r[0] != '~' || r[1] != 'S')
        return TELA_BJ_TAMANHO;
    if (r[2] != RETRATO_VERSAO || (r[3] | r[4] << 8) != RETRATO_TAMANHO)
        return TELA_BJ_VERSAO;
    for (i = 0; i < (unsigned)n - 2; i++) {
        s1 = (s1 + r[i]) & 255;
        s2 = (s2 + s1) & 255;
    }
    if (s1 != r[n - 2] || s2 != r[n - 1])
        return TELA_BJ_SOMA;
    memset(memoria, 0, sizeof memoria);
    memset(chip, 0xFF, sizeof chip);
    for (i = 0; i < 4; i++)
        grandes[i] = le32_de(v + i * 4);
    memcpy(pequenos, v + 20, sizeof pequenos);
    for (i = 0; i < RETRATO_FAIXAS; i++, m += j) {
        a = RETRATO_BASE + (retrato_faixas[i][0] | retrato_faixas[i][1] << 8);
        j = retrato_faixas[i][2];
        memcpy(memoria + a, m, j);
    }
    /* o ponteiro do menu dentro do menu_tabela do radio (que muda de lugar a cada compilacao; v163), no mesmo lugar da
     * tabela da previa: os nomes, as quantidades e as marcas dela sao as do firmware (o menu_grupos.h, na versao) */
    a = MENU_TABELA - le32_de(v + 16);
    if (a < CONTA_DE(menu_previa_nomes) * TAM_ITEM)
        MENU_TABELA = MENU_BASE + a;
    /* a flash, onde o firmware a leu (remoto.c): o canal da terceira (o 0 com ela em 0xFFFF) e a memoria do radio */
    c = grandes[1] & 0xFFFF;
    c = c == 0xFFFF ? 0 : c * 32;
    if (c + 32 <= sizeof chip) {
        memcpy(chip + c, f, 4);
        memcpy(chip + c + 20, f + 4, 12);
    }
    a = memoria[0x12A1B];
    a = (a == 1 ? 0xC110u : a == 0 || a > 4 ? 0xC010u : 0xC210u) + memoria[0x14CB4] * 16;
    memcpy(chip + a, f + 16, 14);                     /* ate 0xC210 + 255 x 16 + 14, dentro do chip */
    /* a potencia indexa uma tabela sem conferir (tela.c, faixa_potencia): fora de 0 a 2, a do L */
    for (i = 0; i < 2; i++)
        if (memoria[0x15E30 + i] > 2)
            memoria[0x15E30 + i] = 2;
    return 0;
}

/* A tela de cada modo, inteira, como o firmware a desenha (as faixas dele, para o menu cortar onde o radio corta). */
static int desenha_modo(void)
{
    unsigned m = MODO_TELA;

    /* o menu calado (v112): a tela do APRS ou do Morse, que o laco deles desenha do historico, na flash; com a caixa
     * da MESSAGES aberta (o editor), o radio a poe por cima dela, e aqui ela vai sozinha */
    if (m == 1 && calado()) {
        if (!menu_editor())
            return TELA_BJ_FORA;
        caixa_b();
        return TELA_BJ_PARCIAL;
    }
    if (m == 1 || m == 2 || m == 17 || m == 20) {
        telas_b(m, m == 2 ? DIGITADOS != 0 : m == 17 ? memoria[0x148D0] == 2 : 0);
        return m == 20 ? TELA_BJ_PARCIAL : TELA_BJ_INTEIRA;
    }
    tela_b();
    if (DIGITADOS && m != 7)              /* no DTMF (7) os digitos ja estao na tela */
        tela_b_digitando();
    return TELA_BJ_INTEIRA;
}

/* O retrato em r (n bytes) e o sinal da resposta (o medidor_s: os segmentos, o bit 7 o microfone na TX, 255
 * nenhum) desenhados em saida (160 x 128, RGB565, linha a linha). Devolve TELA_BJ_INTEIRA, PARCIAL, FORA ou JANELA,
 * ou o erro (negativo), e entao a saida fica como estava. */
EXPORTA int tela_bj_desenha(const unsigned char *r, int n, int sinal, unsigned short *saida)
{
    int k = poe_retrato(r, n);
    unsigned m, lado, nenhum;

    if (k)
        return k;
    memset(tela, 0, sizeof tela);
    if (RAM(u8, 0x00014BDA) >> 7) {          /* a lanterna: a tela toda acesa, e nada se desenha */
        memset(tela, 0xFF, sizeof tela);
        memcpy(saida, tela, sizeof tela);
        return TELA_BJ_INTEIRA;
    }
    janela_ruim = 0;
    k = desenha_modo();
    if (k != TELA_BJ_FORA) {
        m = MODO_TELA;
        /* o lado do medidor de fabrica (0x01E452D0): o em evidencia, ou o outro com a dupla escuta parada nele; o 255
         * (nenhum sinal) tem o bit 7, que fora dele e o microfone na TX */
        lado = (RAM(u8, 0x00015E04) != 0) ^ (RAM(u8, 0x00015378) == 1);
        nenhum = sinal < 0 || sinal >= 255;
        medidor_b(!nenhum && sinal & 0x80 ? 1 : 0, nenhum ? 0 : sinal & 0x7F, lado, m == 1 || m == 2 || m == 17 || m == 20);
    }
    memcpy(saida, tela, sizeof tela);
    return janela_ruim && k == TELA_BJ_INTEIRA ? TELA_BJ_JANELA : k;
}

/* Para o teste (tests/teste_tela_pc.py): o mesmo, com o medidor dado (estado, segmentos e lado de fabrica). */
EXPORTA int tela_bj_desenha_com(const unsigned char *r, int n, int estado, int segmentos, int lado,
                                unsigned short *saida)
{
    int k = poe_retrato(r, n);

    if (k)
        return k;
    memset(tela, 0, sizeof tela);
    janela_ruim = 0;
    k = desenha_modo();
    if (k != TELA_BJ_FORA && segmentos >= 0)
        medidor_b((unsigned)estado, (unsigned)segmentos, (unsigned)lado,
                  MODO_TELA == 1 || MODO_TELA == 2 || MODO_TELA == 17 || MODO_TELA == 20);
    memcpy(saida, tela, sizeof tela);
    return janela_ruim && k == TELA_BJ_INTEIRA ? TELA_BJ_JANELA : k;
}
