/* Tela principal do tema B (Fase 2), desenhada por nos.
 *
 * Compila para o radio (o tela.o inteiro vai para o lugar do decodificador G.729, pelo bancos.ld) e
 * para o PC, onde a previa (firmware/previa/) imita a RAM, a janela() e o envia() do firmware e grava
 * a tela em imagem. Do firmware so se usa a janela() (0x01E3AD34), que abre um pedaco da tela e o
 * enche de uma cor, e o envia() (0x01E3ADBC), que o manda ao LCD. Retangulo e letra sao escritos aqui
 * mesmo no buffer de video da janela aberta (0x10190, RGB565 com o byte alto primeiro;
 * docs/mapa-da-tela.md, secao 1).
 *
 * Tudo o que a tela mostra e lido da RAM, nos enderecos que o desenho de fabrica le (mapa na secao 11
 * da doc da tela). A cena inteira e desenhada em cada janela, e o que cai fora dela e cortado:
 * redesenhar so um pedaco e abrir a janela dele e chamar a mesma cena.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#ifdef PREVIA
extern u8 memoria[];                                  /* a RAM imitada (previa/previa.c) */
#define RAM(tipo, endereco)       (*(volatile tipo *)(memoria + (endereco)))
#define FIRMWARE(nome, endereco)
#else
#define RAM(tipo, endereco)       (*(volatile tipo *)(endereco))
#define FIRMWARE(nome, endereco)  __asm__(".set " #nome ", " #endereco)
#endif

#include "fonte_b.h"

FIRMWARE(JANELA, 0x01E3AD34);  void JANELA(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
FIRMWARE(ENVIA, 0x01E3ADBC);   void ENVIA(void);

/* O que a janela() grava, e o buffer que ela enche (docs/mapa-da-tela.md, secao 1). */
#define VIDEO      0x00010190u
#define TAM_VIDEO  10240u
#define JAN_X0     RAM(u8, 0x00014608)
#define JAN_Y0     RAM(u8, 0x0001460A)
#define JAN_X1     RAM(u8, 0x0001460C)
#define JAN_Y1     RAM(u8, 0x0001460E)
#define JAN_LINHA  RAM(u16, 0x00014610)

/* O estado do radio que a tela mostra (docs/mapa-da-tela.md, secao 11). */
#define VFO(lado)        (0x00014DC4u + (lado) * 31)    /* registro de cada lado */
#define LADO_B           RAM(u8, 0x00015E04)             /* lado em evidencia */
#define MODO_TRABALHO    RAM(u8, 0x00014B86)             /* nibble de cada lado: 1 = canal */
#define EXIBICAO(lado)   RAM(u8, 0x00014B79 + (lado))    /* 0 nome, 1 frequencia, 2 numero */
#define CANAL(lado)      RAM(u16, 0x00015E0C + (lado) * 4) /* base 0 */
#define POTENCIA(lado)   RAM(u8, 0x00015E30 + (lado))    /* 0 H, 1 M, 2 L */
#define RT(lado)         RAM(u8, 0x00015E28 + (lado))    /* 1 R, 2 T */
#define MODO_ZONA        RAM(u8, 0x00014BD4)             /* 0 = navega por zona */
#define ZONA(lado)       RAM(u8, 0x00014BDB + (lado))
#define EM_TX            RAM(u8, 0x00014685)             /* 2 = transmitindo (0x01E40F1C) */
#define DIGITADOS        RAM(u8, 0x00014A35)             /* quantos digitos ja digitados */
#define DIGITOS          0x00014A3Au                     /* os digitos, em ASCII */
#define VOX              RAM(u8, 0x00014B6E)
#define TDR              RAM(u8, 0x00014B70)             /* dupla escuta (DW) */
#define MONITOR          RAM(u8, 0x00015370)             /* MO */
#define DTMF_ST          RAM(u8, 0x00014B75)             /* nota musical */
#define REDUCAO_RUIDO    RAM(u8, 0x00014BDE)             /* ondas: item NOISE REDUCTION */
#define BLUETOOTH        RAM(u8, 0x00014B89)
#define TRAVA            RAM(u8, 0x00014B87)
#define TENSAO_BRUTA     RAM(u8, 0x000149BC)             /* x 444 / 1000 = decimos de volt */
#define NIVEL_BATERIA    RAM(u32, 0x000149C0)

/* As cores do rascunho B (#rrggbb convertido para RGB565). */
#define COR_FUNDO          0x0040u   /* #050806 */
#define COR_ESTADO         0x0881u   /* #0a100d */
#define COR_LINHA          0x1123u   /* #17271f */
#define COR_CHIP_DW        0x19C5u   /* #1a3a2c */
#define COR_CHIP_RX        0x09E5u   /* #0f3d2a */
#define COR_TEXTO_RX       0x6F36u   /* #6ee7b7 */
#define COR_TEXTO          0xEF9Du   /* #e8f2ec */
#define COR_VERDE          0x3693u   /* #34d399 */
#define COR_CARTAO         0x0903u   /* #0a2118 */
#define COR_CINZA          0xA636u   /* #a3c4b3 */
#define COR_CHIP_CANAL     0x11C5u   /* #14382a */
#define COR_CHIP_AMBAR     0x3941u   /* #3b2a0b */
#define COR_TEXTO_AMBAR    0xFDE4u   /* #fbbf24 */
#define COR_CHIP_VERMELHO  0x3861u   /* #3b0f0f */
#define COR_SEG_FUNDO      0x1143u   /* #15291f */
#define COR_SEG_VERDE      0x262Bu   /* #22c55e */
#define COR_SEG_AMARELO    0xED81u   /* #eab308 */
#define COR_SEG_VERMELHO   0xEA28u   /* #ef4444 */
#define COR_SELO_B         0x21C5u   /* #20382c */
#define COR_CHIP_CANAL_B   0x08E3u   /* #0f1f18 */
#define COR_SEG_FUNDO_B    0x10E3u   /* #111f18 */
#define COR_BRANCO         0xFFFFu

/* Geometria: barra de cima, cartao do lado em evidencia (68 linhas) e faixa do outro (38). */
#define ALTURA_ESTADO   16
#define ALTURA_CARTAO   68
#define ALTURA_COMPACTO 38
#define TOPO_CARTAO(lado)   ((lado) ? 58u : 18u)
#define TOPO_COMPACTO(lado) ((lado) ? 89u : 19u)

/* Pilha. A tela B roda no meio das chamadas de desenho do firmware, sem saber o tamanho da pilha de
 * quem chama; por isso o caminho mais fundo dela nao passa do que o proprio firmware de fabrica ja usa
 * no mesmo lugar (medido no desmontado: a rotina da interface 0x01E4B416, o laco do sinal 0x01E4622A
 * e a partida). As partes da cena ficam separadas, e nao coladas no desenha(), para os textos de cada
 * uma nao ocuparem a pilha ao mesmo tempo; e o texto e a etiqueta, pequenos, entram colados em quem
 * os chama, para nao somarem dois quadros no fim do caminho. */
#define SEPARADA  __attribute__((noinline))
#define COLADA    inline __attribute__((always_inline))

struct lado {
    u32 freq;              /* em 10 Hz (441.05000 MHz = 44105000); a de TX enquanto transmite */
    u16 canal;             /* 1..960; 0 = VFO */
    u8 exibicao;           /* 0 nome, 1 frequencia, 2 numero */
    u8 potencia, tom, desloc, estreito, am, sc, scr, rt, zona;
    u8 sinal;              /* segmentos acesos do medidor, 0..20 */
};
struct estado {
    struct lado lado[2];
    u8 evidencia, tx, rx;
    char digitos[8];       /* o que se esta digitando no lado em evidencia, "" se nada */
    u8 vox, dw, mo, dtmf, nr, bt, trava;
    u8 tensao;             /* decimos de volt */
    u8 bateria;            /* 0..3 = 1..4 tracos, 4 = so o contorno vermelho, 255 = sem pilha */
};

static u32 le32(u32 a)
{
    return RAM(u8, a) | RAM(u8, a + 1) << 8 | RAM(u8, a + 2) << 16 | (u32)RAM(u8, a + 3) << 24;
}

/* Um lado, como o desenho de fabrica le: frequencia de RX (de TX transmitindo), canal pelo nibble
 * do modo de trabalho, e a linha de informacoes (0x01E3B2C4). AM: o ajuste do canal, de 18 a
 * 999,99999 MHz, ou sempre de 108 a 135,99999 MHz (0x01E3B438..0x01E3B47E). */
static void le_lado(struct lado *l, unsigned i, unsigned tx)
{
    u32 r = VFO(i), rx = le32(r);
    unsigned tom = RAM(u8, r + 4);

    l->freq = tx ? le32(r + 5) : rx;
    l->canal = (MODO_TRABALHO >> (i * 4) & 15) == 1 ? CANAL(i) + 1 : 0;
    l->exibicao = EXIBICAO(i);
    l->potencia = POTENCIA(i);
    l->tom = tom ? tom == 1 ? 1 : 2 : 0;
    l->desloc = RAM(u8, r + 10);
    l->estreito = RAM(u8, r + 11) != 0;
    l->am = rx - 10800000u <= 2799999u || (rx - 1800000u <= 98199999u && RAM(u8, r + 12) == 1);
    l->sc = RAM(u8, r + 13) != 0;
    l->scr = (RAM(u8, r + 14) & 3) != 0;
    l->rt = RT(i);
    l->zona = MODO_ZONA == 0 ? ZONA(i) + 1 : 0;
    l->sinal = 0;
}

/* O medidor, como o de fabrica: transmitindo, cheio pela potencia (H 10, M 5, L 2 de 10,
 * 0x01E40F1C); fora disso, vazio. O nivel do sinal so chega pelo gancho do medidor (tela_b_medidor),
 * quando o degrau muda - a tela inteira de fabrica tambem desenha as barras vazias. 20 segmentos =
 * 2 por nivel. */
static void le_estado(struct estado *e)
{
    unsigned i, v = NIVEL_BATERIA;

    e->evidencia = LADO_B != 0;
    e->tx = EM_TX == 2;
    e->rx = 0;
    for (i = 0; i < 2; i++)
        le_lado(&e->lado[i], i, e->tx && i == e->evidencia);
    if (e->tx)
        e->lado[e->evidencia].sinal = POTENCIA(e->evidencia) == 0 ? 20 : POTENCIA(e->evidencia) == 1 ? 10 : 4;
    e->digitos[0] = 0;              /* so as rotinas de digitacao os mostram (tela_b_digitando) */
    e->vox = VOX != 0;
    e->dw = TDR != 0;
    e->mo = MONITOR != 0;
    e->dtmf = DTMF_ST != 0;
    e->nr = REDUCAO_RUIDO != 0;
    e->bt = BLUETOOTH == 1;
    e->trava = TRAVA != 0;
    e->tensao = TENSAO_BRUTA * 444u / 1000;
    e->bateria = v > 6 ? 3 : v >= 4 ? 4 : v;      /* a conta de 0x01E3B044 */
}

/* ---------- desenho ---------- */

/* Retangulo cheio [x0, x1) x [y0, y1) em coordenadas da tela, cortado pela janela aberta. A janela
 * nunca passa do buffer (janela_b() garante), entao todo pixel cortado cai dentro dele. Entra direto
 * na letra (cada ponto e um retangulo), que fica no fim do caminho mais fundo; o resto chama pinta(). */
static inline __attribute__((always_inline)) void retangulo(unsigned x0, unsigned y0, unsigned x1,
                                                            unsigned y1, unsigned cor)
{
    unsigned jx = JAN_X0, jy = JAN_Y0, linha = JAN_LINHA, troca = (cor >> 8 | cor << 8) & 0xFFFF;

    if (x0 < jx)
        x0 = jx;
    if (x1 > JAN_X1)
        x1 = JAN_X1;
    if (y0 < jy)
        y0 = jy;
    if (y1 > JAN_Y1)
        y1 = JAN_Y1;
    for (; y0 < y1; y0++) {
        volatile u16 *p = &RAM(u16, VIDEO + (y0 - jy) * linha + (x0 - jx) * 2);
        unsigned x;

        for (x = x0; x < x1; x++)
            *p++ = troca;
    }
}

static void pinta(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned cor)
{
    retangulo(x0, y0, x1, y1, cor);
}

/* Abre a janela [x0, x1) x [y0, y1) cheia de cor, com no maximo o que cabe no buffer: a janela()
 * nao tem trava, e uma maior que 10240 B atropela a RAM (docs/mapa-da-tela.md, secao 1). */
static void janela_b(unsigned x0, unsigned x1, unsigned y0, unsigned y1, unsigned cor)
{
    unsigned linhas = TAM_VIDEO / ((x1 - x0) * 2);

    if (y1 - y0 > linhas)
        y1 = y0 + linhas;
    JANELA(x0, x1, y0, y1, 1, cor);
}

/* Indice da letra na fonte: minuscula vira maiuscula, icone (0x80..) depois das letras, e o que a
 * fonte nao tem vira "?". */
static unsigned indice(unsigned c)
{
    if (c - 'a' < 26)
        c -= 32;
    if (c - 0x80 < FONTE_B_ICONES)
        return FONTE_B_LETRAS + c - 0x80;
    return c - FONTE_B_PRIMEIRA < FONTE_B_LETRAS ? c - FONTE_B_PRIMEIRA : '?' - FONTE_B_PRIMEIRA;
}

/* Uma letra com o canto de cima em (x, y), cada ponto um quadrado de e x e. Devolve quanto andar:
 * a largura dela mais uma coluna de espaco. */
static unsigned letra(unsigned x, unsigned y, unsigned c, unsigned e, unsigned cor)
{
    unsigned l = fonte_b_letra[indice(c)], i, b, r;

    if (x < JAN_X1 && y < JAN_Y1 && y + FONTE_B_ALTURA * e > JAN_Y0)
        for (i = 0; i < (l & 7); i++)
            for (b = fonte_b_colunas[(l >> 3) + i], r = y; b; b >>= 1, r += e)
                if (b & 1)
                    retangulo(x + i * e, r, x + i * e + e, r + e, cor);
    return ((l & 7) + 1) * e;
}

COLADA static unsigned texto(unsigned x, unsigned y, const char *s, unsigned e, unsigned cor)
{
    while (*s)
        x += letra(x, y, (u8)*s++, e, cor);
    return x;
}

/* Largura do texto, sem o espaco depois da ultima letra. */
static unsigned largura(const char *s, unsigned e)
{
    unsigned l = 0;

    while (*s)
        l += ((fonte_b_letra[indice((u8)*s++)] & 7) + 1) * e;
    return l ? l - e : 0;
}

/* Etiqueta: texto de 7 px num retangulo de 11 de altura, com 3 px de folga dos lados. Com direita,
 * x e onde ela termina. Devolve onde a proxima pode comecar (com 2 px de vao), ou onde esta comeca. */
COLADA static unsigned etiqueta(unsigned x, unsigned y, const char *s, unsigned fundo, unsigned cor,
                         unsigned direita)
{
    unsigned l = largura(s, 1) + 6;

    if (direita)
        x -= l;
    pinta(x, y, x + l, y + 11, fundo);
    texto(x + 3, y + 2, s, 1, cor);
    return direita ? x : x + l + 2;
}

/* Numero sem sinal em texto, com pelo menos `digitos` digitos. Devolve o fim. */
static char *numero(char *s, unsigned v, unsigned digitos)
{
    char tmp[10];
    unsigned n = 0;

    do {
        tmp[n++] = '0' + v % 10;
        v /= 10;
    } while (v || n < digitos);
    while (n)
        *s++ = tmp[--n];
    *s = 0;
    return s;
}

static char *junta(char *s, const char *t)
{
    while ((*s = *t++) != 0)
        s++;
    return s;
}

/* "441.05000" (casas = 5) ou "441.050" (casas = 3). */
static void frequencia(char *s, u32 f, unsigned casas)
{
    s = numero(s, f / 100000, 3);
    *s++ = '.';
    numero(s, casas == 5 ? f % 100000 : f / 100 % 1000, casas);
}

/* O que se esta digitando sobre a mascara: cada '-' recebe o proximo digito, na ordem. */
static void digitados(char *s, const char *mascara, const char *d)
{
    for (; *mascara; mascara++)
        *s++ = *mascara == '-' && *d ? *d++ : *mascara;
    *s = 0;
}

static void texto_canal(char *s, unsigned canal)
{
    numero(junta(s, "CH "), canal, 3);
}

/* Medidor de 20 segmentos de 4 px, de 5 em 5: 9 verdes, 5 amarelos e 6 vermelhos acesos ate o
 * sinal; transmitindo, todos ambar. */
static void medidor(unsigned x, unsigned y, unsigned altura, unsigned sinal, unsigned tx, unsigned apagado)
{
    unsigned i;

    for (i = 0; i < 20; i++) {
        unsigned cor = i >= sinal ? apagado : tx ? COR_TEXTO_AMBAR : i < 9 ? COR_SEG_VERDE
                                                             : i < 14 ? COR_SEG_AMARELO : COR_SEG_VERMELHO;

        pinta(x + i * 5, y, x + i * 5 + 4, y + altura, cor);
    }
}

/* Uma etiqueta da linha de informacoes, se couber ate x 152; se nao couber, nenhuma depois dela cabe
 * (devolve 160). Devolve onde a proxima comeca. */
static unsigned info(unsigned x, unsigned y, const char *s)
{
    return x + largura(s, 1) + 6 <= 152 ? etiqueta(x, y, s, COR_CHIP_CANAL, COR_TEXTO, 0) : 160;
}

/* As etiquetas da linha de informacoes, da esquerda para a direita, enquanto couberem. A da potencia
 * vem primeiro, na cor do nivel: H vermelho, M ambar, L verde. */
SEPARADA static void informacoes(const struct lado *l, unsigned y)
{
    static const char *const potencias[3] = {"H", "M", "L"};
    static const u16 fundos[3] = {COR_CHIP_VERMELHO, COR_CHIP_AMBAR, COR_CHIP_RX};
    static const u16 cores[3] = {COR_SEG_VERMELHO, COR_TEXTO_AMBAR, COR_TEXTO_RX};
    char zona[6];
    unsigned x = 8;

    if (l->potencia < 3)
        x = etiqueta(x, y, potencias[l->potencia], fundos[l->potencia], cores[l->potencia], 0);
    x = info(x, y, l->am ? "AM" : "FM");
    if (l->estreito)
        x = info(x, y, "N");
    if (l->desloc == 1 || l->desloc == 2)
        x = info(x, y, l->desloc == 1 ? "+" : "-");
    if (l->tom)
        x = info(x, y, l->tom == 1 ? "CT" : "DCS");
    if (l->rt == 1 || l->rt == 2)
        x = info(x, y, l->rt == 1 ? "R" : "T");
    if (l->sc && !l->am)
        x = info(x, y, "SC");
    if (l->scr)
        x = info(x, y, "SCR");
    if (l->zona) {
        numero(junta(zona, "Z"), l->zona, 2);
        info(x, y, zona);
    }
}

/* O nome gravado do lado, como o desenho de fabrica le: ate 12 caracteres, parando em 0 ou 0xFF
 * (0x01E3BDCC..0x01E3BDE6). Lido na hora de desenhar, para nao ocupar a pilha o tempo todo. */
static char *nome(char *s, unsigned lado)
{
    unsigned n, c;

    for (n = 0; n < 12 && (c = RAM(u8, VFO(lado) + 15 + n)) != 0 && c != 0xFF; n++)
        s[n] = c;
    s[n] = 0;
    return s;
}

/* O nome, ou "UNNAMED CH" sem nome, onde a tela de fabrica mostra o nome sozinho. */
static char *nome_ou_sem(char *s, unsigned lado)
{
    if (!nome(s, lado)[0])
        junta(s, "UNNAMED CH");
    return s;
}

/* O lado em evidencia: cartao com borda verde, selo, linha pequena e canal, linha grande (a
 * frequencia com os dois ultimos digitos pequenos, o nome ou o numero, pela exibicao do lado) e
 * medidor. No VFO a linha grande e sempre a frequencia, e a etiqueta diz VFO. Um buffer so, cada
 * texto desenhado logo que montado: e o que mais pesa na pilha. */
SEPARADA static void cartao(const struct estado *e, unsigned i)
{
    const struct lado *l = &e->lado[i];
    unsigned y = TOPO_CARTAO(i), x, digitando = e->digitos[0] && i == e->evidencia;
    unsigned frequencia_grande = !l->canal || l->exibicao == 1;
    char s[16];

    pinta(3, y, 157, y + ALTURA_CARTAO, COR_VERDE);
    pinta(4, y + 1, 156, y + ALTURA_CARTAO - 1, COR_CARTAO);
    pinta(8, y + 4, 19, y + 15, COR_VERDE);
    letra(11, y + 6, 'A' + i, 1, COR_FUNDO);

    /* linha pequena: o nome em cima da frequencia, ou a frequencia inteira em cima do nome */
    if (l->canal && l->exibicao == 1)
        texto(24, y + 6, nome(s, i), 1, COR_CINZA);
    else if (l->canal && l->exibicao == 0) {
        frequencia(s, l->freq, 5);
        texto(24, y + 6, s, 1, COR_CINZA);
    }
    /* etiqueta: VFO, o que se digita, ou o numero (menos quando ele ja e a linha grande) */
    if (!l->canal)
        etiqueta(152, y + 4, "VFO", COR_CHIP_CANAL, COR_TEXTO, 1);
    else if (digitando || l->exibicao != 2) {
        if (digitando)
            digitados(junta(s, "CH "), "---", e->digitos);
        else
            texto_canal(s, l->canal);
        etiqueta(152, y + 4, s, COR_CHIP_CANAL, COR_TEXTO, 1);
    }
    /* linha grande, e os dois ultimos digitos da frequencia pequenos depois dela */
    if (digitando && !l->canal)
        digitados(s, "---.---", e->digitos);
    else if (frequencia_grande)
        frequencia(s, l->freq, 3);
    else if (l->exibicao == 0)
        nome_ou_sem(s, i);
    else
        texto_canal(s, l->canal);
    x = texto(8, y + 20, s, 2, COR_BRANCO) + 1;
    if (digitando && !l->canal)
        texto(x, y + 27, "--", 1, COR_CINZA);
    else if (frequencia_grande) {
        numero(s, l->freq % 100, 2);
        texto(x, y + 27, s, 1, COR_CINZA);
    }
    medidor(8, y + 54, 9, l->sinal, e->tx && i == e->evidencia, COR_SEG_FUNDO);
}

/* O outro lado, compacto: selo, o texto da exibicao (no VFO, a frequencia inteira), o canal e um
 * medidor fino. */
SEPARADA static void compacto(const struct estado *e, unsigned i)
{
    const struct lado *l = &e->lado[i];
    unsigned y = TOPO_COMPACTO(i);
    char s[16];

    pinta(8, y + 4, 19, y + 15, COR_SELO_B);
    letra(11, y + 6, 'A' + i, 1, COR_CINZA);
    if (!l->canal || l->exibicao == 1)
        frequencia(s, l->freq, 5);
    else if (l->exibicao == 0)
        nome_ou_sem(s, i);
    else
        texto_canal(s, l->canal);
    texto(24, y + 6, s, 1, COR_CINZA);
    if (!l->canal)
        etiqueta(152, y + 4, "VFO", COR_CHIP_CANAL_B, COR_CINZA, 1);
    else if (l->exibicao != 2) {
        texto_canal(s, l->canal);
        etiqueta(152, y + 4, s, COR_CHIP_CANAL_B, COR_CINZA, 1);
    }
    medidor(8, y + 24, 4, l->sinal, 0, COR_SEG_FUNDO_B);
}

/* Barra de cima: o que esta ligado (VOX, DW, MO, NR em texto; DTMF ST, Bluetooth e trava em icone),
 * RX ou TX, e a tensao com a pilha. */
SEPARADA static void barra_estado(const struct estado *e)
{
    char s[8], *p;
    unsigned i, x = 4;

    pinta(0, 0, 160, ALTURA_ESTADO - 1, COR_ESTADO);
    pinta(0, ALTURA_ESTADO - 1, 160, ALTURA_ESTADO, COR_LINHA);
    if (e->tx)
        x = etiqueta(x, 2, "TX", COR_CHIP_VERMELHO, COR_SEG_VERMELHO, 0);
    else if (e->rx) {
        pinta(x, 2, x + 22, 13, COR_CHIP_RX);
        pinta(x + 3, 6, x + 6, 9, COR_TEXTO_RX);
        texto(x + 8, 4, "RX", 1, COR_TEXTO_RX);
        x += 24;
    }
    if (e->vox)
        x = etiqueta(x, 2, "VOX", COR_CHIP_DW, COR_TEXTO, 0);
    if (e->dw)
        x = etiqueta(x, 2, "DW", COR_CHIP_DW, COR_TEXTO, 0);
    if (e->mo)
        x = etiqueta(x, 2, "MO", COR_CHIP_DW, COR_TEXTO, 0);
    if (e->nr)
        x = etiqueta(x, 2, "NR", COR_CHIP_DW, COR_TEXTO, 0);
    if (e->dtmf)
        x = texto(x + 1, 4, ICONE_NOTA, 1, COR_VERDE) + 1;
    if (e->bt)
        x = texto(x + 1, 4, ICONE_BLUETOOTH, 1, COR_VERDE) + 1;
    if (e->trava)
        texto(x + 1, 4, ICONE_CADEADO, 1, COR_VERDE);

    p = numero(s, e->tensao / 10, 1);
    *p++ = '.';
    p = numero(p, e->tensao % 10, 1);
    junta(p, "V");
    texto(136 - largura(s, 1), 4, s, 1, COR_TEXTO);
    if (e->bateria != 255) {
        unsigned cor = e->bateria > 3 ? COR_SEG_VERMELHO : COR_VERDE;

        pinta(140, 4, 154, 12, cor);
        pinta(141, 5, 153, 11, COR_ESTADO);
        pinta(154, 6, 156, 10, cor);
        for (i = 0; e->bateria <= 3 && i <= e->bateria; i++)
            pinta(142 + i * 3, 6, 144 + i * 3, 10, COR_VERDE);
    }
}

static void cena(const struct estado *e)
{
    barra_estado(e);
    cartao(e, e->evidencia);
    informacoes(&e->lado[e->evidencia], TOPO_CARTAO(e->evidencia) + 38);
    compacto(e, !e->evidencia);
}

/* As linhas [y0, y1) da tela, em janelas de ate 32 linhas (32 x 160 x 2 = 10240 B, o buffer todo). */
static void desenha(const struct estado *e, unsigned y0, unsigned y1)
{
    for (; y0 < y1; y0 += 32) {
        janela_b(0, 160, y0, y1 < y0 + 32 ? y1 : y0 + 32, COR_FUNDO);
        cena(e);
        ENVIA();
    }
}

void tela_b(void)
{
    struct estado e;

    le_estado(&e);
    desenha(&e, 0, 128);
}

/* O medidor de um lado com o nivel dado (0..10, o do firmware; estado 1 = TX), e a barra de cima,
 * que mostra RX ou TX. E o que o gancho do medidor desenha (b_medidor), colado nele: o RX o chama do
 * laco 0x01E4622A, e assim o caminho fica dentro do que o firmware de fabrica ja usa de pilha ali. */
static inline __attribute__((always_inline)) void medidor_b(unsigned estado, unsigned nivel, unsigned lado)
{
    struct estado e;
    unsigned y;

    le_estado(&e);
    lado = lado != 0;
    e.lado[lado].sinal = nivel < 10 ? nivel * 2 : 20;
    e.tx = estado == 1;
    e.rx = !e.tx && nivel != 0;
    y = lado == e.evidencia ? TOPO_CARTAO(lado) + 54 : TOPO_COMPACTO(lado) + 24;
    desenha(&e, y, y + (lado == e.evidencia ? 9 : 4));
    desenha(&e, 0, ALTURA_ESTADO);
}

#ifdef PREVIA
void tela_b_medidor(unsigned estado, unsigned nivel, unsigned lado)
{
    medidor_b(estado, nivel, lado);
}
#endif

/* As linhas de um lado: o cartao, se ele esta em evidencia, ou a faixa compacta. */
static void desenha_lado(const struct estado *e, unsigned lado)
{
    unsigned y = lado == e->evidencia ? TOPO_CARTAO(lado) : TOPO_COMPACTO(lado);

    desenha(e, y, y + (lado == e->evidencia ? ALTURA_CARTAO : ALTURA_COMPACTO));
}

/* O lado em evidencia com o que se esta digitando: os digitos em 0x14A3A, quantos em b[0x14A35] - o
 * que as duas rotinas de digitacao do firmware leem (0x01E4B29C e 0x01E4B1DE). Como nelas, so este
 * desenho mostra os digitos; qualquer outro redesenho mostra o lado normal. */
void tela_b_digitando(void)
{
    struct estado e;
    unsigned i;

    le_estado(&e);
    for (i = 0; i < DIGITADOS && i < sizeof e.digitos - 1; i++)
        e.digitos[i] = RAM(u8, DIGITOS + i);
    e.digitos[i] = 0;
    desenha_lado(&e, e.evidencia);
}

#ifndef PREVIA
/* ---------- ligacao com o firmware ----------
 * Treze rotinas que desenham a tela de dois VFOs ganham um desvio na entrada (DESVIO, abaixo). O
 * gancho de cada uma recebe os argumentos dela e devolve 1 quando desenhou a tela B no lugar - a
 * rotina de fabrica nao roda - ou 0, e entao ela roda como sempre. A tela B so desenha na tela de
 * dois VFOs (layout() diferente de 0, a mesma regra das rotinas de fabrica) e fora do idioma
 * chines, que a fonte nao tem. Quem decide QUANDO desenhar continua sendo o firmware. */
FIRMWARE(LAYOUT, 0x01E3B1A2);  unsigned LAYOUT(unsigned);
FIRMWARE(LIMPA, 0x01E3ACBA);   void LIMPA(unsigned);
FIRMWARE(ENCHE, 0x01E3AC68);   void ENCHE(unsigned, unsigned, unsigned);
#define IDIOMA  RAM(u8, 0x00014B74)                      /* 1 = chines */

static unsigned ativa(void)
{
    return LAYOUT(LADO_B) != 0 && IDIOMA != 1;
}

/* 0x01E3C132(0): a tela inteira. Ela zera o estado de TX (0x01E3C1D4); o gancho tambem. Com 1 e a
 * tela de um VFO, que fica a de fabrica. */
u32 b_tela(u32 um_vfo)
{
    if (um_vfo || !ativa())
        return 0;
    EM_TX = 0;
    tela_b();
    return 1;
}

/* 0x01E3B05C, a barra de cima; 0x01E3B03E e 0x01E3AE50, o nivel da bateria e o cadeado, que se
 * redesenham sozinhos - os tres leem o estado da RAM. */
u32 b_barra(void)
{
    struct estado e;

    if (!ativa())
        return 0;
    le_estado(&e);
    desenha(&e, 0, ALTURA_ESTADO);
    return 1;
}

/* 0x01E3AF1E(nivel, janela propria), a pilha: o laco a chama direto, alternando 255 e 4, para piscar
 * a bateria fraca (0x01E460D8..0x01E460F2). O nivel que vem e o que vale. */
u32 b_bateria(u32 nivel)
{
    struct estado e;

    if (!ativa())
        return 0;
    le_estado(&e);
    e.bateria = nivel;
    desenha(&e, 0, ALTURA_ESTADO);
    return 1;
}

/* 0x01E3AEB0(ligado, janela propria), o MO: quem chama passa o estado (o b[0x15370] pode vir depois). */
u32 b_monitor(u32 ligado)
{
    struct estado e;

    if (!ativa())
        return 0;
    le_estado(&e);
    e.mo = ligado != 0;
    desenha(&e, 0, ALTURA_ESTADO);
    return 1;
}

/* 0x01E453E0 (troca de canal ou frequencia): o lado em evidencia. */
u32 b_evidencia(void)
{
    struct estado e;

    if (!ativa())
        return 0;
    le_estado(&e);
    desenha_lado(&e, e.evidencia);
    return 1;
}

/* 0x01E4B29C e 0x01E4B1DE: o numero de canal e a frequencia sendo digitados. */
u32 b_digitando(void)
{
    if (!ativa())
        return 0;
    tela_b_digitando();
    return 1;
}

/* 0x01E413BE(lado, ...) e 0x01E3B2C4(lado, forcar), a linha de informacoes: os dois lados. A tecla
 * A/B (0x01E4DC9E) troca o lado e chama 0x01E41486, que, com b[0x14685] em 0, so redesenha o lado
 * novo por estas duas - e na tela B o cartao muda de lugar, entao o outro lado tambem muda. Sao
 * raras (A/B, potencia, modulacao); a troca de canal vem por b_evidencia. */
u32 b_lado(void)
{
    struct estado e;

    if (!ativa())
        return 0;
    le_estado(&e);
    desenha(&e, ALTURA_ESTADO, 128);
    return 1;
}

/* 0x01E3B592(estado, nivel, lado, janela propria), o medidor; fora da tela inteira e da TX (que sao
 * nossas), so o RX chama (0x01E45328). O nivel que vem e o que vale (o MO passa 10, 0x01E4532C), e a
 * barra de cima acompanha, pelo RX. */
u32 b_medidor(u32 estado, u32 nivel, u32 lado)
{
    if (!ativa())
        return 0;
    medidor_b(estado, nivel, lado);
    return 1;
}

/* 0x01E40F1C(estado), a transmissao, refeita: 1 so grava o estado; 2 grava e desenha o lado em
 * evidencia (frequencia de TX, medidor pela potencia) e a barra de cima; outro valor e o fim da TX,
 * que fora do modo de tela 1 (fora da tela B, portanto) limpa e redesenha tudo, com o estado em 0
 * (0x01E40F1C..0x01E41020). Aqui o estado e gravado ANTES de desenhar, para a tela B ja le-lo. */
u32 b_transmissao(u32 estado)
{
    struct estado e;

    if (!ativa())
        return 0;
    EM_TX = estado == 1 || estado == 2 ? estado : 0;
    if (estado == 1)
        return 1;
    le_estado(&e);
    if (estado != 2)
        desenha(&e, 0, 128);
    else {
        desenha_lado(&e, e.evidencia);
        desenha(&e, 0, ALTURA_ESTADO);
    }
    return 1;
}

/* Desvio na entrada de uma rotina de fabrica: guarda rets e r0..r3 e chama o gancho com os
 * argumentos dela. Se ele desenhou (devolveu 1), volta direto a quem chamou a rotina, com r0..r3
 * como estavam - este compilador sabe quais registradores cada rotina suja, e o chamador pode
 * contar com eles. Se nao, refaz as instrucoes que o desvio cobriu (os bytes de fabrica, iguais) e
 * segue a rotina logo depois delas. So formas de pilha que o proprio firmware usa: [--sp] = rets e
 * {rets} = [sp++] (0x01E3C290), [--sp] = {r3-r0} e {r3-r0} = [sp++] (0x01E51358..0x01E5137E). O
 * montar.py confere os bytes cobertos e o conferir.py, o desvio inteiro. */
#define DESVIO(nome, gancho, volta, bytes)                                                         \
    __asm__(".set " #nome "_volta, " #volta "\n"                                                   \
            ".section .text." #nome ",\"ax\"\n"                                                    \
            ".globl " #nome "\n"                                                                   \
            #nome ":\n\t"                                                                          \
            "[--sp] = rets\n\t"                                                                    \
            "[--sp] = {r3-r0}\n\t"                                                                 \
            "call " #gancho "\n\t"                                                                 \
            "if (r0 == 0) goto " #nome "_fabrica\n\t"                                              \
            "{r3-r0} = [sp++]\n\t"                                                                 \
            "{rets} = [sp++]\n\t"                                                                  \
            "rts\n"                                                                                \
            #nome "_fabrica:\n\t"                                                                  \
            "{r3-r0} = [sp++]\n\t"                                                                 \
            "{rets} = [sp++]\n\t"                                                                  \
            ".byte " bytes "\n\t"                                                                  \
            "goto " #nome "_volta\n\t"                                                             \
            ".previous")

DESVIO(desvio_tela, b_tela, 0x01E3C136, "0x77, 0x04, 0xe2, 0x9e");
DESVIO(desvio_barra, b_barra, 0x01E3B060, "0x76, 0x04, 0xe2, 0x9d");
DESVIO(desvio_nivel_bateria, b_barra, 0x01E3B044, "0xc1, 0xff, 0x80, 0x46, 0x01, 0x00");
DESVIO(desvio_cadeado, b_barra, 0x01E3AE54, "0x75, 0x04, 0xe2, 0x9d");
DESVIO(desvio_bateria, b_bateria, 0x01E3AF22, "0x79, 0x04, 0xe2, 0x9a");
DESVIO(desvio_monitor, b_monitor, 0x01E3AEB4, "0x76, 0x04, 0xe2, 0x9d");
DESVIO(desvio_evidencia, b_evidencia, 0x01E453E6, "0x76, 0x04, 0x40, 0xe0, 0x92, 0x17");
DESVIO(desvio_numero, b_digitando, 0x01E4B2A0, "0x75, 0x04, 0xe2, 0x9b");
DESVIO(desvio_freq_digitada, b_digitando, 0x01E4B1E2, "0x78, 0x04, 0xe2, 0x9a");
DESVIO(desvio_lado, b_lado, 0x01E413C2, "0x76, 0x04, 0xe2, 0x9e");
DESVIO(desvio_informacoes, b_lado, 0x01E3B2C8, "0x7a, 0x04, 0xe2, 0x9c");
DESVIO(desvio_medidor, b_medidor, 0x01E3B596, "0x7c, 0x04, 0xe2, 0x9c");
DESVIO(desvio_transmissao, b_transmissao, 0x01E40F20, "0x76, 0x04, 0xe2, 0x9e");

/* ---------- realce verde, a tela de fabrica quando a B nao desenha ----------
 * Desde a v17 o bloco do VFO em evidencia fica verde (docs/mapa-da-tela.md, secao 7); hoje isso so
 * vale com o idioma chines, em que a tela B nao desenha. O gancho do envio entra nas 16 chamadas de
 * envia() das rotinas da tela de dois VFOs (REALCE_SITIOS_GANCHO, no montar.py) e o da limpeza nas
 * duas que limpam e redesenham a tela inteira (LIMPA_SITIOS).
 *
 * Com a tela B desenhando, nenhuma dessas rotinas deveria chegar ao envio - as treze acima nao rodam.
 * Se alguma chegar, pelo caminho que for, o gancho redesenha da tela B o lado a que a janela pertence
 * (pela separadora da limpeza de fabrica: 88 com o A em evidencia, 55 com o B), no lugar do que a
 * rotina de fabrica desenhou. A limpeza nao roda: a tela inteira vem logo depois (0x01E3C290 e
 * 0x01E40F1C passam por 0x01E3C132) e a desenha toda. */
/* #004900, degrau 18 dos 64 do verde em RGB565. Ja passaram por aqui o 0x00E0 (degrau 7, "nao da
 * nem pra perceber que e verde"), o 0x0280 (20), o 0x0380 (28, claro demais no bloco inteiro) e o
 * 0x02C0 (22, v17 a v19: "ainda bem claro"). */
#define COR_REALCE  0x0240u
#define COR_BUF     ((u16)(COR_REALCE >> 8 | COR_REALCE << 8))
#define AREA        72u                                  /* linhas do lado em evidencia na limpeza */

/* Primeira linha da area do lado em evidencia (16 com o A, 56 com o B), ou 0 fora da tela de dois
 * VFOs: layout() devolve 2 com o A em evidencia, 1 com o B e 0 nas outras telas. */
static unsigned area_realce(void)
{
    unsigned lado = LAYOUT(LADO_B);

    return lado - 1 <= 1 ? 96 - 40 * lado : 0;
}

void gancho_envia(void)
{
    unsigned y0 = JAN_Y0, inicio = area_realce();

    if (inicio && IDIOMA != 1) {
        struct estado e;

        le_estado(&e);
        desenha_lado(&e, y0 >= (e.evidencia ? 56u : 89u));
        return;
    }
    if (inicio && y0 - inicio < AREA) {      /* sem sinal: y0 abaixo do inicio da um numero enorme */
        unsigned n = (JAN_LINHA >> 1) * (JAN_Y1 - y0);
        volatile u16 *p = &RAM(u16, VIDEO);

        if (n > TAM_VIDEO / 2)       /* nunca passa do buffer, mesmo com janela absurda */
            n = TAM_VIDEO / 2;
        while (n--) {
            if (*p == 0)
                *p = COR_BUF;
            p++;
        }
    }
    ENVIA();
}

void gancho_limpa(unsigned modo)
{
    unsigned inicio = area_realce();

    if (inicio && IDIOMA != 1)
        return;
    LIMPA(modo);
    if (inicio)
        ENCHE(inicio, AREA, COR_REALCE);
}
#endif
