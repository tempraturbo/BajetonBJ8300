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
#define MODO_TELA        RAM(u8, 0x0001533D)             /* 0/3/5 dois VFOs, 1 menu, 2 radio, 7 DTMF,
                                                          * 17 scan QT, 20 espectro */
#define ESTADO_RX        RAM(u8, 0x00015367)             /* 0 e 1 sintoniza, 2 espera, 3 vigia,
                                                          * 4 recebendo, 5 MO (0x01E467A6) */
#define AMPLI_RX         RAM(u8, 0x000129D4)             /* o pedido do amplificador da recepcao: */
                                                         /* PEDE_AMPLI(5) liga, (6) solta (0x01E39E2C) */
/* No menu, recebendo (v63): o estado em 4 e o audio da recepcao aberto. A entrada da recepcao pede o
 * amplificador (0x01E47E1A) antes de por o estado em 4, e o sinal que cai o solta (0x01E47A9E): na v62,
 * so com o estado, o indicador ficava na barra depois do fim da recepcao. */
#define RECEBENDO_MENU   (ESTADO_RX == 4 && AMPLI_RX != 0)

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
#define COR_SEG_FUNDO      0x2A68u   /* #2c4c3f: apagado, claro o bastante para nao sumir no preto */
#define COR_SEG_VERDE      0x262Bu   /* #22c55e */
#define COR_SEG_AMARELO    0xED81u   /* #eab308 */
#define COR_SEG_VERMELHO   0xEA28u   /* #ef4444 */
#define COR_SELO_B         0x21C5u   /* #20382c */
#define COR_CHIP_CANAL_B   0x08E3u   /* #0f1f18 */
#define COR_SEG_FUNDO_B    0x21E6u   /* #223f32 */
#define COR_BRANCO         0xFFFFu

/* Geometria: barra de cima, cartao do lado em evidencia (68 linhas) e faixa do outro (38). */
#define ALTURA_ESTADO   16
#define ALTURA_CARTAO   68
#define ALTURA_COMPACTO 38
#define TOPO_CARTAO(lado)   ((lado) ? 58u : 18u)
#define TOPO_COMPACTO(lado) ((lado) ? 89u : 19u)
/* WATCH em SINGLE (v42): so o lado em evidencia, num cartao da barra ao pe da tela. */
#define TOPO_UNICO      18u
#define ALTURA_UNICO    108u
#define MEDIDOR_UNICO   (TOPO_UNICO + 70)
#define ALTURA_MED_UNICO 12u
/* WATCH em TRIPLE (v42): o cartao de sempre (o do B desce para 39) e duas faixas finas, a do outro
 * lado e a da C, sempre na ordem A, B, C (ver faixa, mais abaixo). */
#define ALTURA_FAIXA     19u
/* A letra de cada lado do firmware - o lado B mostra a C quando ela esta nele (VISTA_TRIPLE_C) - e,
 * pela letra (A 0, B 1, C 2), onde fica: o cartao da que esta em evidencia em 18, 39 ou 59, e as
 * faixas acima dele de 20 em 20 a partir de 18, e abaixo, de 20 em 20 a partir de 88. */
#define LETRA(e, lado)       ((lado) == 0 ? 0u : (e)->vista == VISTA_TRIPLE_C ? 2u : 1u)
#define Y_FAIXA(e, j)        ((j) < LETRA(e, (e)->evidencia) ? 18u + 20u * (j) : 68u + 20u * (j))
#define TOPO_CARTAO_V(e, i)  ((e)->vista >= VISTA_TRIPLE && (i) ? ((e)->vista == VISTA_TRIPLE ? 39u : 59u) \
                                                                : TOPO_CARTAO(i))

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
    char digitos[7];       /* o que se esta digitando no lado em evidencia, "" se nada (ate 6) */
    u8 vista;              /* WATCH (v42): 0 DUAL, 1 SINGLE - no byte que sobrou dos digitos, para a
                            * estrutura nao crescer (ela esta no caminho mais fundo da pilha) */
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
unsigned watch_vista(void);         /* bancos.c: o WATCH, do registro proprio dele */
u32 watch_le_tudo(void);            /* modo | mdf_c << 8 | canal_c << 16 */
#define VISTA_SINGLE 1
#define VISTA_TRIPLE 2
#define VISTA_TRIPLE_C 3                /* TRIPLE com a C no lado B, em evidencia */
unsigned watch_tela(void);          /* bancos.c: o modo (3 = TRIPLE com a C no lado B) | MDF-C << 8 */
u32 terceira(void);                 /* bancos.c: canal | exibicao << 16 da que esta fora dos dois lados */
u32 freq_terceira(u32 t);           /* bancos.c: a frequencia do VFO dela, em dezenas de Hz (v49) */
#define TERCEIRA_VFO   (1u << 24)   /* em terceira(): ela esta em VFO (v49) */

static void le_estado(struct estado *e)
{
    unsigned i, v = NIVEL_BATERIA;

    e->evidencia = LADO_B != 0;
    e->tx = EM_TX == 2;
    e->rx = MODO_TELA == 1 && RECEBENDO_MENU;  /* no menu, quem recebe na barra (v62) */
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
    v = watch_tela();
    e->vista = v & 0xFF;
    if (e->vista == VISTA_TRIPLE_C)     /* a C no lado B aparece como o MDF-C pede, nao o MDF-B */
        e->lado[1].exibicao = v >> 8;
    if (v >> 31)                        /* TX-A/B/C (v51): a terceira no lado B, com o canal dela */
        e->lado[1].canal = v >> 16 & 0x7FFF;
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

/* O tamanho medio (v38), entre o 1x (7 de altura) e o 2x (14): e = MEDIA. A mesma letra, com cada
 * coluna de haste (4 pontos acesos ou mais) e cada coluna vazia com 2 px e as outras com 1, e a 2a e a
 * 6a linha com 2 px - 9 de altura, o traco vertical do 2x e o horizontal do 1x. Letra sem haste (S, Z,
 * 2) ganha os 2 px nas duas colunas das pontas, senao sai fina e mais estreita que as outras (v40).
 * A largura de cada coluna vem pronta do fonte.py (fonte_b_media). 1 px entre letras. */
#define MEDIA         0
#define ALTURA_MEDIA  9
#define COLUNA_MEDIA(k) (1 + (fonte_b_media[(k) >> 3] >> ((k) & 7) & 1))
static const u8 linha_media[8] = {0, 1, 3, 4, 5, 6, 8, 9};
/* O do menu (v56: a janela de pasta e o item aberto), um pouco maior para ler melhor: as colunas do
 * medio, a linha do meio tambem com 2 px (10 de altura: as barras do A, E, F, H, P, R saem como as de
 * cima e de baixo) e um px a mais entre as letras. O que nao cabe sai no medio (texto_menu, telas.c). */
static const u8 linha_janela[8] = {0, 1, 3, 4, 6, 7, 9, 10};

/* Quanto uma letra anda: a largura dela mais o espaco depois. */
static unsigned avanco(unsigned l, unsigned e)
{
    unsigned k = l >> 3, fim = k + (l & 7), a = 1;

    if (e)
        return ((l & 7) + 1) * e;
    for (; k < fim; k++)
        a += COLUNA_MEDIA(k);
    return a;
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

/* A mesma letra no tamanho medio. Separada da letra(), que fica no fim do caminho mais fundo da tela
 * principal: junta, as duas contas guardavam mais registradores e o quadro dela crescia. */
SEPARADA static unsigned letra_media(unsigned x, unsigned y, unsigned c, unsigned cor, const u8 *linhas)
{
    unsigned l = fonte_b_letra[indice(c)], k = l >> 3, fim = k + (l & 7), x0 = x, w, b, r;
    unsigned ve = y < JAN_Y1 && y + linhas[7] > JAN_Y0;

    for (; k < fim; k++, x += w)
        for (w = COLUNA_MEDIA(k), b = fonte_b_colunas[k], r = 0; ve && x < JAN_X1 && b; b >>= 1, r++)
            if (b & 1)
                retangulo(x, y + linhas[r], x + w, y + linhas[r + 1], cor);
    return x - x0 + 1;
}

COLADA static unsigned texto(unsigned x, unsigned y, const char *s, unsigned e, unsigned cor)
{
    while (*s)
        x += e ? letra(x, y, (u8)*s++, e, cor) : letra_media(x, y, (u8)*s++, cor, linha_media);
    return x;
}

/* Largura do texto, sem o espaco depois da ultima letra. */
static unsigned largura(const char *s, unsigned e)
{
    unsigned l = 0;

    while (*s)
        l += avanco(fonte_b_letra[indice((u8)*s++)], e);
    return l ? l - (e ? e : 1) : 0;
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
 * sinal; transmitindo, todos no vermelho do TX (v38; ate a v37, ambar). */
static void medidor(unsigned x, unsigned y, unsigned altura, unsigned sinal, unsigned tx, unsigned apagado)
{
    unsigned i, passo = altura > 9 ? 7 : 5;     /* o do SINGLE (12 de altura) e mais largo, 140 px */

    for (i = 0; i < 20; i++) {
        unsigned cor = i >= sinal ? apagado : tx ? COR_SEG_VERMELHO : i < 9 ? COR_SEG_VERDE
                                                             : i < 14 ? COR_SEG_AMARELO : COR_SEG_VERMELHO;

        pinta(x + i * passo, y, x + i * passo + passo - 1, y + altura, cor);
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
    unsigned y = TOPO_CARTAO_V(e, i), x, digitando = e->digitos[0] && i == e->evidencia;
    unsigned frequencia_grande = !l->canal || l->exibicao == 1;
    char s[16];

    pinta(3, y, 157, y + ALTURA_CARTAO, COR_VERDE);
    pinta(4, y + 1, 156, y + ALTURA_CARTAO - 1, COR_CARTAO);
    pinta(8, y + 4, 19, y + 15, COR_VERDE);
    letra(11, y + 6, 'A' + LETRA(e, i), 1, COR_FUNDO);

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

/* WATCH em SINGLE (v42): o lado em evidencia sozinho, da barra ao pe da tela. O mesmo cartao, maior:
 * a linha grande em 3x (em 2x se nao couber), com os dois ultimos digitos em 2x; o medidor mais alto;
 * e, com deslocamento, a frequencia de transmissao embaixo. As etiquetas vem da cena, ao lado, como
 * no DUAL: chamadas daqui, os quadros se somavam e a pilha passava 28 B da de fabrica. */
SEPARADA static void cartao_unico(const struct estado *e, unsigned i)
{
    const struct lado *l = &e->lado[i];
    unsigned y = TOPO_UNICO, x, g, digitando = e->digitos[0] != 0;
    unsigned frequencia_grande = !l->canal || l->exibicao == 1;
    char s[16];

    pinta(3, y, 157, y + ALTURA_UNICO, COR_VERDE);
    pinta(4, y + 1, 156, y + ALTURA_UNICO - 1, COR_CARTAO);
    pinta(8, y + 4, 19, y + 15, COR_VERDE);
    letra(11, y + 6, 'A' + i, 1, COR_FUNDO);
    /* linha pequena e etiqueta, como no cartao */
    if (l->canal && l->exibicao == 1)
        texto(24, y + 6, nome(s, i), 1, COR_CINZA);
    else if (l->canal && l->exibicao == 0) {
        frequencia(s, l->freq, 5);
        texto(24, y + 6, s, 1, COR_CINZA);
    }
    if (!l->canal)
        etiqueta(152, y + 4, "VFO", COR_CHIP_CANAL, COR_TEXTO, 1);
    else if (digitando || l->exibicao != 2) {
        if (digitando)
            digitados(junta(s, "CH "), "---", e->digitos);
        else
            texto_canal(s, l->canal);
        etiqueta(152, y + 4, s, COR_CHIP_CANAL, COR_TEXTO, 1);
    }
    /* linha grande: 3x se couber, e os dois ultimos digitos um tamanho abaixo, pelo pe */
    if (digitando && !l->canal)
        digitados(s, "---.---", e->digitos);
    else if (frequencia_grande)
        frequencia(s, l->freq, 3);
    else if (l->exibicao == 0)
        nome_ou_sem(s, i);
    else
        texto_canal(s, l->canal);
    g = largura(s, 3) <= 112 ? 3 : 2;
    x = texto(8, y + 22, s, g, COR_BRANCO) + 2;
    if (digitando && !l->canal)
        junta(s, "--");
    else if (frequencia_grande)
        numero(s, l->freq % 100, 2);
    if ((digitando && !l->canal) || frequencia_grande)
        texto(x, y + 22 + FONTE_B_ALTURA, s, g - 1, COR_CINZA);    /* 7 x g contra 7 x (g - 1): pelo pe */
    medidor(10, MEDIDOR_UNICO, ALTURA_MED_UNICO, l->sinal, e->tx, COR_SEG_FUNDO);
    /* com deslocamento, a frequencia em que ele transmite */
    if (!e->tx && (l->desloc == 1 || l->desloc == 2)) {
        frequencia(junta(s, "TX "), le32(VFO(i) + 5), 5);
        texto(10, y + 92, s, 1, COR_CINZA);
    }
}

/* WATCH em TRIPLE (v42): o lado em evidencia no cartao de sempre, e o outro lado e a C em faixas
 * finas de 19 px, sem medidor, sempre na ordem A, B, C: com o A em evidencia, cartao em 18 e as
 * faixas do B e da C em 88 e 108; com o B, a faixa do A em 18, o cartao em 39 e a C em 108. Faixa
 * que esta recebendo fica ambar escuro com a letra forte, como o chip de TX da barra de cima. */

unsigned c_no_lugar(void);          /* bancos.c: 1 enquanto o rodizio poe a C no lugar do outro lado */
unsigned outro_recebendo(void);     /* bancos.c: o outro lado (o B ou a C) esta recebendo */
void LE_FLASH(u32, void *, unsigned);   /* o endereco vem do FIRMWARE() do telas.c, incluido depois */

/* O canal da C, lido do chip de dados: 32 B por canal, a frequencia de RX em BCD com o byte menos
 * significativo primeiro (+0) e o nome em +20 (o formato do driver do CHIRP; e a mesma leitura de 4
 * bytes da partida, existe_no_registro do bancos.c). Canal vazio (primeiro byte 0xFF) devolve 0.
 * Separada: o buffer de 32 B fica no quadro dela, e nao no da faixa, que desenha. */
SEPARADA static u32 le_canal_c(unsigned canal, char *nome12)
{
    u8 r[32];
    u32 f = 0;
    int k;

    LE_FLASH((u32)canal * 32, r, sizeof r);
    if (r[0] == 0xFF || r[3] == 0)
        return 0;
    for (k = 3; k >= 0; k--)
        f = f * 100 + (r[k] >> 4) * 10 + (r[k] & 15);
    for (k = 0; k < 12 && r[20 + k] != 0 && r[20 + k] != 0xFF; k++)
        nome12[k] = r[20 + k];
    nome12[k] = 0;
    return f;
}

/* Uma faixa: selo, o texto da exibicao e o canal (ou VFO). Embutida nas duas que a chamam, e com o
 * canal escrito no mesmo buffer depois de o texto ja estar desenhado: e o fim do caminho mais fundo
 * da tela no TRIPLE, e um quadro a mais (ou um buffer a mais) passava da pilha de fabrica. */
static inline __attribute__((always_inline)) void faixa(unsigned y, unsigned selo, char *s, unsigned canal,
                                                        unsigned rx)
{
    unsigned cor = rx ? COR_TEXTO_AMBAR : COR_CINZA;

    pinta(3, y, 157, y + ALTURA_FAIXA - 1, rx ? COR_CHIP_AMBAR : COR_FUNDO);
    pinta(8, y + 4, 19, y + 15, rx ? COR_TEXTO_AMBAR : COR_SELO_B);
    letra(11, y + 6, selo, 1, rx ? COR_CHIP_AMBAR : COR_CINZA);
    texto(24, y + 6, s, 1, cor);
    if (canal != 0xFFFF) {
        if (canal)
            texto_canal(s, canal);
        etiqueta(152, y + 4, canal ? s : "VFO", rx ? COR_CHIP_AMBAR : COR_CHIP_CANAL_B, cor, 1);
    }
}

/* A faixa do lado fora de evidencia, com os dados dele (os da RAM, os mesmos do compacto), se ela
 * cruza a janela aberta. Recebendo: o outro lado tem sinal e nao e a terceira que esta no lugar dele. */
SEPARADA static void faixa_lado(const struct estado *e, unsigned i)
{
    const struct lado *l = &e->lado[i];
    unsigned y = Y_FAIXA(e, LETRA(e, i));
    char s[16];

    if (JAN_Y0 >= y + ALTURA_FAIXA || JAN_Y1 <= y)
        return;
    if (!l->canal || l->exibicao == 1)
        frequencia(s, l->freq, 5);
    else if (l->exibicao == 0)
        nome_ou_sem(s, i);
    else
        texto_canal(s, l->canal);
    faixa(y, 'A' + LETRA(e, i), s, l->canal && l->exibicao == 2 ? 0xFFFF : l->canal,
          (l->sinal || outro_recebendo()) && !c_no_lugar());
}

/* A faixa da terceira - a que esta fora dos dois lados: a C, ou o B quando a C esta no lado B -,
 * lida do chip de dados e mostrada como o MDF dela pede (0 nome, 1 frequencia, 2 numero). Recebendo:
 * o outro lado tem sinal com a terceira no lugar dele. */
SEPARADA static void faixa_terceira(const struct estado *e)
{
    u32 t, f = 0;
    unsigned canal, mdf, n, j = 3 - LETRA(e, 1), y = Y_FAIXA(e, j);
    char s[16];

    if (JAN_Y0 >= y + ALTURA_FAIXA || JAN_Y1 <= y)
        return;
    t = terceira();
    canal = t & 0xFFFF;
    mdf = t >> 16 & 0xFF;
    if (t & TERCEIRA_VFO) {                /* v49: em VFO, a frequencia e "VFO", como a faixa de um lado */
        frequencia(s, freq_terceira(t), 5);
        n = 0;
    } else {
        if (canal != 0xFFFF)
            f = le_canal_c(canal, s);      /* o nome, em s; trocado abaixo se o MDF pede outra coisa */
        if (!f)
            junta(s, "EMPTY CH");
        else if (mdf == 1)
            frequencia(s, f, 5);
        else if (mdf == 2)
            texto_canal(s, canal + 1);
        else if (!s[0])
            junta(s, "UNNAMED CH");
        n = !f || mdf == 2 ? 0xFFFF : canal + 1;
    }
    faixa(y, 'A' + j, s, n, (e->lado[!e->evidencia].sinal || outro_recebendo()) && c_no_lugar());
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
/* No menu, recebendo (v59), no lugar dos icones: o selo do lado que recebe e o texto que o MDF dele
 * pede, como no resumo, cortado antes da tensao, numa caixa ambar como a faixa que recebe na tela
 * principal (v63: o fundo ambar escuro, o selo ambar com a letra escura e o texto ambar). Quem recebe e
 * o lado em evidencia: a entrada do menu desarma a dupla escuta (0x01E4CBBC) e o laco do sinal so a
 * arma no modo de tela 0 (0x01E47C10). O lado que chega ao medidor no menu e sempre o A (0x01E452E8),
 * e por isso nao serve aqui. */
#define FIM_RECEBENDO 110u
SEPARADA static void quem_recebe(const struct estado *e, unsigned x)
{
    unsigned i = e->evidencia, n;
    const struct lado *l = &e->lado[i];
    char s[16];

    if (!l->canal || l->exibicao == 1)
        frequencia(s, l->freq, 5);
    else if (l->exibicao == 0)
        nome_ou_sem(s, i);
    else
        texto_canal(s, l->canal);
    for (n = 0; s[n]; n++)
        ;
    while (n && x + 18 + largura(s, 1) + 3 > FIM_RECEBENDO)
        s[--n] = 0;
    pinta(x, 2, x + 18 + largura(s, 1) + 3, 13, COR_CHIP_AMBAR);
    pinta(x + 2, 3, x + 13, 12, COR_TEXTO_AMBAR);
    letra(x + 5, 4, 'A' + LETRA(e, i), 1, COR_CHIP_AMBAR);
    texto(x + 18, 4, s, 1, COR_TEXTO_AMBAR);
}

#ifdef DIAG_RX
/* Diagnostico do fim da recepcao no menu (v63d, so com -DDIAG_RX): no lugar dos icones, no menu, o
 * estado da recepcao (E, b[0x15367]), o pedido do amplificador da recepcao (A, b[0x129D4]), os dois
 * sinalizadores que a leitura do nivel olha (F, b[0x1536F]; G, b[0x15368]) e um contador que anda a
 * cada chamada do b_sinal (no 0x903A dos ajustes, b[0x14BE6], livre). */
#define DIAG_CONTA RAM(u8, 0x00014BE6)
SEPARADA static void diag_rx(unsigned x)
{
    static const u32 enderecos[4] = {0x15367, 0x129D4, 0x1536F, 0x15368};
    static const char letras[4] = {'E', 'A', 'F', 'G'};
    char s[24], *p = s;
    unsigned i;

    for (i = 0; i < 4; i++) {
        *p++ = letras[i];
        p = numero(p, RAM(u8, enderecos[i]), 1);
        *p++ = ' ';
    }
    numero(p, DIAG_CONTA % 100, 2);
    texto(x + 1, 4, s, 1, COR_TEXTO_AMBAR);
}
#endif

SEPARADA static void barra_estado(const struct estado *e)
{
    char s[8], *p;
    unsigned i, x = 4;

    pinta(0, 0, 160, ALTURA_ESTADO - 1, COR_ESTADO);
    pinta(0, ALTURA_ESTADO - 1, 160, ALTURA_ESTADO, COR_LINHA);
    /* RX ou TX: o mesmo chip, com a bolinha, em verde ou em vermelho (v38: o TX tinha so o texto) */
    if (e->tx || e->rx) {
        unsigned cor = e->tx ? COR_SEG_VERMELHO : COR_TEXTO_RX;

        pinta(x, 2, x + 22, 13, e->tx ? COR_CHIP_VERMELHO : COR_CHIP_RX);
        pinta(x + 3, 6, x + 6, 9, cor);
        texto(x + 8, 4, e->tx ? "TX" : "RX", 1, cor);
        x += 24;
    }
#ifdef DIAG_RX
    if (MODO_TELA == 1) {
        diag_rx(x);
    } else
#endif
    if (e->rx && MODO_TELA == 1) {
        quem_recebe(e, x);
    } else {
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
    }

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

/* A caixa do DTMF (modo de tela 7, a tecla #), no lugar da de fabrica (0x01E4DA54 abrindo,
 * 0x01E4A8D4 a cada digito): no lugar do cartao do lado em evidencia, com contorno e texto ambar. Os
 * digitos sao os de 0x14A3A (quantos em b[0x14A35], ate 10), completados com '-', como na de fabrica.
 * Faz parte da cena enquanto o modo for 7, entao qualquer redesenho do cartao a mantem; a saida do
 * modo zera o modo antes do redesenho inteiro (0x01E3C29C, 0x01E40D68). */
#define TOPO_DTMF(e)    ((e)->vista == VISTA_SINGLE ? TOPO_UNICO : TOPO_CARTAO_V(e, (e)->evidencia))
#define ALTURA_DTMF     ALTURA_CARTAO

SEPARADA static void caixa_dtmf(unsigned y)
{
    static const char titulo[] = "DTMF";
    unsigned i;
    char s[11];

    pinta(3, y, 157, y + ALTURA_DTMF, COR_TEXTO_AMBAR);
    pinta(5, y + 2, 155, y + ALTURA_DTMF - 2, COR_FUNDO);
    texto(80 - largura(titulo, MEDIA) / 2, y + 14, titulo, MEDIA, COR_TEXTO_AMBAR);
    for (i = 0; i < 10; i++)
        s[i] = i < DIGITADOS ? RAM(u8, DIGITOS + i) : '-';
    s[10] = 0;
    texto(80 - largura(s, 2) / 2, y + 33, s, 2, COR_TEXTO_AMBAR);
}

/* Cada parte so quando a janela aberta a cruza: o resto cairia todo fora dela. */
static void cena(const struct estado *e)
{
    unsigned i = e->evidencia, y0 = JAN_Y0, y1 = JAN_Y1;

    if (y0 < ALTURA_ESTADO)
        barra_estado(e);
    if (e->vista == VISTA_SINGLE) {
        if (y0 < TOPO_UNICO + ALTURA_UNICO && y1 > TOPO_UNICO) {
            cartao_unico(e, i);
            informacoes(&e->lado[i], TOPO_UNICO + 52);
        }
    } else if (e->vista >= VISTA_TRIPLE) {  /* as faixas conferem a janela sozinhas */
        if (y0 < TOPO_CARTAO_V(e, i) + ALTURA_CARTAO && y1 > TOPO_CARTAO_V(e, i)) {
            cartao(e, i);
            informacoes(&e->lado[i], TOPO_CARTAO_V(e, i) + 38);
        }
        faixa_lado(e, !i);
        faixa_terceira(e);
    } else {
        if (y0 < TOPO_CARTAO(i) + ALTURA_CARTAO && y1 > TOPO_CARTAO(i)) {
            cartao(e, i);
            informacoes(&e->lado[i], TOPO_CARTAO(i) + 38);
        }
        if (y0 < TOPO_COMPACTO(!i) + ALTURA_COMPACTO && y1 > TOPO_COMPACTO(!i))
            compacto(e, !i);
    }
    if (MODO_TELA == 7 && y0 < TOPO_DTMF(e) + ALTURA_DTMF && y1 > TOPO_DTMF(e))
        caixa_dtmf(TOPO_DTMF(e));
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
static inline __attribute__((always_inline)) void medidor_b(unsigned estado, unsigned nivel, unsigned lado,
                                                            unsigned so_barra)
{
    struct estado e;
    unsigned y;

    le_estado(&e);
    lado = lado != 0;
    e.lado[lado].sinal = nivel < 10 ? nivel * 2 : 20;
    e.tx = estado == 1;
    e.rx = !e.tx && nivel != 0;
    if (e.vista == VISTA_SINGLE) {       /* so o lado em evidencia tem medidor (o outro nem recebe) */
        if (!so_barra && lado == e.evidencia)
            desenha(&e, MEDIDOR_UNICO, MEDIDOR_UNICO + ALTURA_MED_UNICO);
    } else if (e.vista >= VISTA_TRIPLE) { /* o medidor do cartao, ou as duas faixas (o outro lado ou a terceira) */
        if (!so_barra && lado == e.evidencia) {
            y = TOPO_CARTAO_V(&e, lado) + 54;
            desenha(&e, y, y + 9);
        } else if (!so_barra) {
            y = Y_FAIXA(&e, LETRA(&e, lado));
            desenha(&e, y, y + ALTURA_FAIXA);
            y = Y_FAIXA(&e, 3 - LETRA(&e, 1));
            desenha(&e, y, y + ALTURA_FAIXA);
        }
    } else if (!so_barra) {         /* nas outras telas (v37) nao ha medidor dos lados: so a barra */
        y = lado == e.evidencia ? TOPO_CARTAO(lado) + 54 : TOPO_COMPACTO(lado) + 24;
        desenha(&e, y, y + (lado == e.evidencia ? 9 : 4));
    }
    desenha(&e, 0, ALTURA_ESTADO);
}

#ifdef PREVIA
/* Como o b_medidor: nas outras telas (o menu, o radio, o scan QT) e no espectro, so a barra. */
void tela_b_medidor(unsigned estado, unsigned nivel, unsigned lado)
{
    unsigned m = MODO_TELA;

    medidor_b(estado, nivel, lado, m == 1 || m == 2 || m == 17 || m == 20);
}
#endif

/* As linhas de um lado: o cartao, se ele esta em evidencia, ou a faixa compacta. */
static void desenha_lado(const struct estado *e, unsigned lado)
{
    unsigned y = lado == e->evidencia ? TOPO_CARTAO(lado) : TOPO_COMPACTO(lado);

    if (e->vista == VISTA_SINGLE) {      /* no SINGLE o outro lado nao aparece */
        if (lado == e->evidencia)
            desenha(e, TOPO_UNICO, TOPO_UNICO + ALTURA_UNICO);
        return;
    }
    if (e->vista >= VISTA_TRIPLE) {      /* o cartao, ou a faixa do outro lado */
        y = lado == e->evidencia ? TOPO_CARTAO_V(e, lado) : Y_FAIXA(e, LETRA(e, lado));
        desenha(e, y, y + (lado == e->evidencia ? ALTURA_CARTAO : ALTURA_FAIXA));
        return;
    }
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

#include "telas.c"

/* ---------- o espectro (modo 20, v41): as pecas estao no telas.c ---------- */
/* Cada pedaco na janela dele, cheia do fundo, e ao LCD. */
#define PEDACO_B(x0, x1, y0, y1, desenho)                         \
    do {                                                            \
        janela_b(x0, x1, y0, y1, COR_FUNDO);                        \
        desenho;                                                    \
        ENVIA();                                                    \
    } while (0)

/* As rotinas de fabrica trocadas inteiras (um goto na entrada, montar.py: ESPECTRO_B_FUNCOES). So o
 * espectro as chama (docs/engenharia-reversa.md, secao 11), e elas so desenhavam. */
void b_esp_freq(u32 f)                      /* 0x01E45D96 (MOSTRA_FREQ) */
{
    PEDACO_B(ESP_X_FREQ, ESP_X_MODO, ESP_L1, ESP_L2, esp_freq(f, 0));
}

void b_esp_digitos(void)                    /* 0x01E4AFC2, teclas 16 e 18 */
{
    PEDACO_B(ESP_X_FREQ, ESP_X_MODO, ESP_L1, ESP_L2, esp_freq(0, 1));
}

void b_esp_modo(u32 m)                      /* 0x01E4B04C, entrada e tecla 23 */
{
    PEDACO_B(ESP_X_MODO, 160, ESP_L1, ESP_L2, esp_modo(m));
}

void b_esp_dbm(int v)                       /* 0x01E4ADFE */
{
    PEDACO_B(ESP_X_DBM, 160, ESP_L2, ESP_GRAFICO, esp_dbm(v));
}

void b_esp_eixo(u32 inicio, u32 fim)        /* 0x01E4AB2A (ROTULOS_FREQ) */
{
    PEDACO_B(0, 160, ESP_EIXO, 128, esp_eixo(inicio, fim));
}

/* Os ganchos do espectro no bancos.c (no .vozes) desenham por aqui: os rotulos (a rotina dele troca
 * 0x01E4ABDC), o pico da passada (f = 0 so o apaga, na janela nova) e cada coluna das barras, com x
 * relativo a fatia aberta e o topo de fabrica (64 menos a altura). */
void b_esp_rotulos(unsigned n, unsigned p)
{
    PEDACO_B(0, ESP_X_FREQ, ESP_L1, ESP_L2, esp_pontos(n));
    PEDACO_B(0, ESP_X_PICO, ESP_L2, ESP_GRAFICO, esp_passo(p));
}

void b_esp_pico(u32 f)
{
    PEDACO_B(ESP_X_PICO, ESP_X_DBM, ESP_L2, ESP_GRAFICO, if (f) esp_pico(f));
}

void b_esp_coluna(unsigned x, unsigned topo, unsigned atual)
{
    esp_coluna(JAN_X0 + x, topo < 64 ? 64 - topo : 0, atual);
}

/* No lugar da janela de cada uma das 5 fatias do grafico (32 x 64, cheia de preto pela cor 0 que o
 * laco poe na pilha): o painel e a grade. A janela e a de fabrica (4 KB, cabe no buffer), entao vai
 * direto ao JANELA(): pelo janela_b() com outra cor, a cor deixava de ser constante nele e voltava a
 * ir pela pilha, e o quadro da desenha() crescia 8 B. */
void gancho_fatia(unsigned x0, unsigned x1, unsigned y0, unsigned y1)
{
    JANELA(x0, x1, y0, y1, 1, COR_PAINEL);
    esp_grade(x0, x1);
}

/* No lugar da linha do limite de cada fatia (0x01E4D72E: retangulo 0..31 x y..y em 0x07E0, relativo
 * a fatia): tracejada em ambar. */
void gancho_limite(unsigned x0, unsigned x1, unsigned y0, unsigned y1)
{
    esp_limite(JAN_X0 + x0, JAN_X0 + x1 + 1, JAN_Y0 + y0);
    (void)y1;
}

#ifndef PREVIA
/* A tabela do menu principal (v43): o montar.py a preenche aqui, na faixa livre, e a entrada do menu
 * (0x01E4CBB2) a poe no firmware. A original tinha 54 vagas, e o WATCH, o CH-C e o MDF-C nao cabiam;
 * desde a v53 ela traz os grupos e depois os itens de cada um (os grupos do menu, no telas.c). E o que
 * os grupos precisam alem dela vai em menu_textos: a quantidade de grupos no byte 0, e as listas com
 * os nomes dos itens de cada um (as opcoes do item do grupo) e os nomes dos grupos - e, desde a v55,
 * a quantidade de posicoes da tabela no byte 1 e uma marca por posicao a partir do byte 4. Os dois saem do
 * compilador com 0xFF no primeiro byte - para ficar no .rodata, nao no .bss - e zero no resto, e o
 * montar so escreve neles se achar exatamente isso. */
const unsigned char menu_tabela[96 * 28] __attribute__((aligned(4))) = {0xFF};
const unsigned char menu_textos[640] __attribute__((aligned(4))) = {0xFF};

/* ---------- os grupos do menu (v53): a entrada, o OK, o EXIT e o numero digitado ----------
 * O mapa do motor de menus esta no telas.c, junto do desenho. O que muda o nivel faz o mesmo que as
 * setas fazem para mudar de item (0x01E49F60): recarrega o tempo de saida do menu, poe o item e o tipo
 * dele no estado, avisa o item novo e chama o motor, que redesenha. */
FIRMWARE(MOTOR_MENU, 0x01E489E0);  u32 MOTOR_MENU(void);
FIRMWARE(TEMPO_MENU, 0x01E484F8);  void TEMPO_MENU(void);         /* o MENU EXIT TIME de novo */
FIRMWARE(ITEM_MUDOU, 0x01E4851E);  void ITEM_MUDOU(void);         /* no tipo 3 (OFFSET), guarda o valor */
FIRMWARE(OK_MENU, 0x01E49996);     u32 OK_MENU(void);
FIRMWARE(DIGITO_MENU, 0x01E4A0FE); u32 DIGITO_MENU(unsigned);
FIRMWARE(BIPE, 0x01E38B4C);        void BIPE(unsigned);

/* Poe no menu o item i da tabela t, de n itens. */
static void menu_vai(u32 t, unsigned n, unsigned i)
{
    MENU_TABELA = t;
    MENU_CONTA = n;
    MENU_NUMERO = i;
    MENU_ITEM = i;
    MENU_TIPO = ITEM_FLAGS(t, i) & 0x0F;
    MENU_DIGITADO = 0;
    ITEM_MUDOU();
}

/* No lugar do "r5 = menu_tabela" da entrada do menu (0x01E4CBB2, seguido de "r5 = r0"): a tabela dos
 * grupos. Se o menu foi fechado de dentro de um grupo (a tabela que ficou e a dele), reabre nele - a
 * entrada le o item de [+6] logo depois (0x01E4CBCA). */
u32 menu_abre(void)
{
    unsigned n = menu_nivel();

    if (n != 0 && n < RADIO)
        MENU_NUMERO = n - 1;
    return MENU_BASE;
}

/* No lugar da chamada do OK do menu (0x01E4C924, para 0x01E49996): nos grupos, abre o grupo no
 * primeiro item, na janela de pasta; no resto, o OK de fabrica, que abre o item ja editando. O VERSION
 * (tipo 8, v55) o de fabrica nao abre (0x01E49A2E so toca a voz, que nao ha mais): aqui ele abre so
 * para leitura - a edicao ligada, que a tela B desenha sem cursor -, aberto o OK nao faz nada, e o
 * EXIT de fabrica o fecha (cancela a edicao, 0x01E4D500). Devolve o que o motor deixa em r0, como o
 * de fabrica (o bipe, 0x01E4D59C), ou 0, o bipe de tecla sem funcao. */
u32 gancho_menu_ok(void)
{
    unsigned g = MENU_NUMERO, n = menu_nivel();

    if (n == 0) {
        TEMPO_MENU();
        menu_vai(MENU_BASE + menu_inicio(g) * TAM_ITEM, MENU_ITENS(g), 0);
        return MOTOR_MENU();
    }
    if (n >= RADIO || MENU_TIPO != 8)     /* o radio e os menus rapidos: o OK de fabrica */
        return OK_MENU();
    if (MENU_EDITANDO == 1)
        return 0;
    TEMPO_MENU();
    MENU_EDITANDO = 1;
    return MOTOR_MENU();
}

/* No caminho do EXIT fora da edicao (0x01E4D54A, que fecharia o menu; desvio_menu_sai): dentro de um
 * grupo, volta aos grupos, nele. Devolve 1 quando voltou - o desvio segue para o bipe do EXIT
 * (0x01E4D59A) - ou 0, e o menu fecha como antes (o do radio volta ao radio, 0x01E4D556). */
u32 b_menu_sai(void)
{
    unsigned n = menu_nivel();

    if (n == 0 || n >= RADIO)
        return 0;
    TEMPO_MENU();
    menu_vai(MENU_BASE, MENU_GRUPOS, n - 1);
    MOTOR_MENU();
    return 1;
}

/* No EXIT editando, depois de cancelar a edicao (0x01E4D53C, desvio_menu_um): a quantidade de itens
 * em 1 fecha o menu inteiro (0x01E4D566), como num menu rapido de um item. Num grupo (o CALIBRATION
 * tem um item so, v59) devolve 1, e o desvio segue ao bipe do EXIT (0x01E4D59A), por onde o de fabrica
 * passa com mais de um item: a edicao fecha e a lista do grupo fica. */
u32 b_menu_um(void)
{
    unsigned n = menu_nivel();

    return n != 0 && n < RADIO;
}

/* Depois do set, no OK editando (0x01E49A86, onde o firmware zera o estado da recepcao, b[0x15367], e
 * fecha a edicao): nos itens de marcar varios (MARCA_VARIOS: o BANK e o CHANNEL dos BANKS, v55) a
 * edicao continua. So o estado da recepcao vai a 0 - sintoniza de novo, como o de fabrica faz depois
 * de mudar o ajuste - e o desvio segue para o redesenho e a volta (0x01E49C7C). O EXIT sai: cada OK
 * ja gravou. */
u32 b_menu_marca(void)
{
    unsigned n = menu_nivel();

    if (n == 0 || n >= RADIO
        || !(MENU_MARCAS((MENU_TABELA - MENU_BASE) / TAM_ITEM + MENU_NUMERO) & MARCA_VARIOS))
        return 0;
    ESTADO_RX = 0;
    return 1;
}

/* No lugar da chamada do tratador de digitos do menu (0x01E4B9FA, para 0x01E4A0FE; a tecla em ASCII):
 * fora da edicao, nas nossas tabelas e no menu do radio (v56), o numero vai ao item de mesmo numero, contando de 1 como a tela
 * mostra - o de fabrica conta de 0 (0x01E4A2DA). Dois digitos seguidos, antes do tempo do digito
 * acabar e sem seta no meio, fazem um numero so, como no de fabrica (0x01E4A274..0x01E4A2CE). */
u16 cristal_le(void);                 /* bancos.c: o get do CRYSTAL FINE (v59) */
u32 gancho_menu_digito(unsigned tecla)
{
    unsigned d = tecla - '0', conta = MENU_CONTA, n;
    u32 get = MENU_TABELA + MENU_NUMERO * TAM_ITEM + 3;

    get = RAM(u8, get) | RAM(u8, get + 1) << 8 | RAM(u8, get + 2) << 16 | (u32)RAM(u8, get + 3) << 24;
    /* o VERSION aberto e so leitura (v55); o CRYSTAL FINE so anda pelas setas (v59): o numero digitado
     * escolheria a linha desse numero, de -2000 Hz em diante */
    if (MENU_EDITANDO == 1 && (MENU_TIPO == 8 || get == (u32)cristal_le))
        return 0;
    if (MENU_EDITANDO == 1 || menu_nivel() == FORA)
        return DIGITO_MENU(tecla);
    TEMPO_MENU();
    n = MENU_TEMPO_DIGITO && MENU_DIGITADO == MENU_NUMERO + 1u ? MENU_DIGITADO * 10 + d : d;
    if (n - 1 >= conta)
        n = d;
    if (n - 1 >= conta)
        return 0;                     /* nenhum item com esse numero: o bipe de tecla sem funcao */
    menu_vai(MENU_TABELA, conta, n - 1);
    MENU_DIGITADO = n;
    BIPE(6);
    MENU_TEMPO_DIGITO = 15;
    return MOTOR_MENU();
}

/* ---------- ligacao com o firmware ----------
 * Treze rotinas que desenham a tela de dois VFOs ganham um desvio na entrada (DESVIO, abaixo). O
 * gancho de cada uma recebe os argumentos dela e devolve 1 quando desenhou a tela B no lugar - a
 * rotina de fabrica nao roda - ou 0, e entao ela roda como sempre. A tela B so desenha na tela de
 * dois VFOs (layout() diferente de 0, a mesma regra das rotinas de fabrica). Desde a v37 o radio fica
 * sempre em ingles (o gancho da partida, no bancos.c), entao nao ha tela chinesa para respeitar.
 * Quem decide QUANDO desenhar continua sendo o firmware. */
FIRMWARE(LAYOUT, 0x01E3B1A2);  unsigned LAYOUT(unsigned);
FIRMWARE(LIMPA, 0x01E3ACBA);   void LIMPA(unsigned);
static unsigned ativa(void)
{
    return LAYOUT(LADO_B) != 0;
}

/* ---------- as outras telas (v37 e v38) ----------
 * Menu (modo 1), radio (2) e scan QT (17), desenhados pelo telas.c, cada parte pela rotina de fabrica
 * que a desenha - nada no envia() em geral, para aviso e confirmacao ("Sure to Reset?") passarem como
 * os de fabrica e o desenho fundo so rodar nesses pontos:
 *  - a barra de cima: os ganchos da barra, da pilha, do MO e do medidor, como na tela principal;
 *  - o resumo do lado em evidencia (menu e scan QT): o gancho do lado (0x01E3B2C4, que o motor de
 *    menus chama no fim de cada redesenho);
 *  - menu: as duas chamadas do envia() do motor de menus (0x01E489E0), a do titulo e a de cada linha,
 *    e a da piscada do laco do sinal (0x01E476E4, v38), que a tela B cala;
 *  - radio: os desvios do redesenho inteiro (0x01E44FB0), da frequencia (0x01E449BA), do nome
 *    (0x01E44B28), do medidor S (0x01E45198), dos ajustes (0x01E44C68) e, na v38, da frequencia
 *    digitada (0x01E49078); as duas chamadas do envia() dos ajustes (0x01E44CA8, 0x01E44FA8), caladas
 *    no modo 2, e a do "SEEK..." da interface (0x01E4D3F8);
 *  - scan QT: o desvio do redesenho dele (0x01E45A82);
 *  - a espera do radio: a chamada do envia() da partida dele (0x01E4A7D4) e as da troca de modulacao
 *    (0x01E3DEBE e 0x01E3DEEA; a terceira, 0x01E3DF10, vira nop no montar.py);
 *  - o editor de nome do menu (tipo 7 editando, v38): os desvios do indicador do modo (0x01E48720),
 *    que desenha o editor inteiro, e da moldura direto no LCD (0x01E4865C), e a chamada do envia() da
 *    linha do texto (0x01E48FE0), calada. */
static unsigned telas_modo(void)
{
    unsigned m = MODO_TELA;

    return m == 1 || m == 2 || m == 17 ? m : 0;
}

/* As linhas [y0, y1) de um modo. Separada, para a estrutura so pesar na pilha de quem desenha. */
SEPARADA static void telas_faixa(unsigned m, unsigned y0, unsigned y1, unsigned variante, unsigned item)
{
    struct telas t;

    le_telas(&t, m, variante, item);
    desenha_telas(&t, y0, y1);
}

/* No lugar do envia() do titulo (0x01E48E82) e do de cada linha (0x01E48ECA) do motor de menus: a
 * faixa do titulo, ou a da linha que o motor acabou de montar (a janela dele: y 75, 92 ou 109). Fora
 * do modo de menu (ou no editor de nome), o envia() de fabrica. Na janela de pasta (os grupos, v53, e
 * a escolha do item dentro deles, v55) o titulo a desenha inteira, de baixo da barra ao fim, e as
 * linhas nao desenham nada: o motor manda o titulo, as tres linhas, a barra e o resumo em toda passada
 * (0x01E48E82..0x01E48EE4), e as linhas ja estao montadas quando o titulo sai (o valor do cursor). */
void gancho_menu_titulo(void)
{
    if (telas_modo() != 1) {
        ENVIA();
        return;
    }
    if (menu_pasta())
        telas_faixa(1, ALTURA_ESTADO, 128, 0, 0);
    else
        telas_faixa(1, TOPO_MENU, TOPO_MENU + 18, 0, 0);
}

void gancho_menu_linha(void)
{
    unsigned r = JAN_Y0 >= 109 ? 2 : JAN_Y0 >= 92 ? 1 : 0;

    if (telas_modo() != 1) {
        ENVIA();
        return;
    }
    if (!menu_pasta())
        telas_faixa(1, TOPO_MENU + 18 + r * 18, TOPO_MENU + 36 + r * 18, 0, 0);
}

/* No lugar do envia() de 0x01E476E4, por onde sai toda a piscada do menu no laco do sinal: o numero
 * do item em (133, 57) e o espaco que o apaga (contador b[0x14C1A] em 4 e em 0, fora da edicao) e,
 * editando, a linha do cursor na fonte de fabrica e a faixa que a apaga (0x01E472EA, 0x01E47332).
 * No menu da tela B nada disso vai ao LCD - o cartao com borda verde e as setas ja mostram a edicao;
 * no editor de nome (o cursor dele pisca por aqui tambem) e fora do menu, o envia() de fabrica. */
void gancho_menu_pisca(void)
{
    if (telas_modo() != 1)
        ENVIA();
}

/* 0x01E48720, o indicador do modo do editor de nome (abc, ABC, 123), que o motor de menus chama em
 * toda passada no editor (0x01E48DAC, depois de cada tecla) e a troca de modo (#, 0x01E4A5B2). A de
 * fabrica so o desenha quando o modo muda (o ja desenhado em b[0x14686]), com parte direto no LCD (as
 * linhas de y 55 e 88 e a moldura). Na tela B ele desenha o editor inteiro abaixo da barra - ao abrir,
 * a limpeza de fabrica (0x01E49B1C) acabou de apagar tudo - e grava o modo desenhado, como ela. */
u32 b_editor_modo(void)
{
    if (telas_modo() != 1 || !menu_editor())
        return 0;
    EDITOR_DESENHADO = EDITOR_MODO;
    telas_faixa(1, ALTURA_ESTADO, 128, 0, 0);
    return 1;
}

/* 0x01E4865C(x0, y0, x1, y1, cor), a moldura direto no LCD: no editor, a do indicador e a da caixa
 * do pinyin (0x01E48DD0, 0x01E48F18 - esta, a apagada, depois de cada troca de modo). Na tela B nao
 * ha moldura de fabrica. */
u32 b_editor_quadro(void)
{
    return telas_modo() == 1 && menu_editor();
}

/* No lugar do envia() da linha do texto do editor (0x01E48FE0, "%15s" em y 110): o b_editor_modo ja
 * desenhou o texto na mesma passada. */
void gancho_editor_linha(void)
{
    if (telas_modo() != 1 || !menu_editor())
        ENVIA();
}

/* 0x01E44FB0, o redesenho do radio: so desenha (faixa da banda, 0x01E449BA, 0x01E44B28 e
 * 0x01E44C68(0, -1), que com o item 0 nao grava nada), depois de por a banda (> 4) e a faixa do AM
 * (> 3) em 0, como ele faz (0x01E44FD2, 0x01E4501A). Como ele, a seta fica na frequencia. */
u32 b_radio(void)
{
    if (MODO_TELA != 2)
        return 0;
    if (RADIO_BANDA >= 5)
        RADIO_BANDA = 0;
    if (RADIO_FAIXA_AM >= 4)
        RADIO_FAIXA_AM = 0;
    telas_faixa(2, 0, 128, RADIO_NORMAL, 0);
    return 1;
}

/* 0x01E449BA, a frequencia e o numero da memoria (so desenha): a faixa da frequencia e a do nome,
 * onde a tela B mostra a memoria. A seta, pelo item em edicao ([0x14CD4]). */
u32 b_radio_freq(void)
{
    if (MODO_TELA != 2)
        return 0;
    telas_faixa(2, 36, 88, RADIO_NORMAL, RADIO_ITEM);
    return 1;
}

/* 0x01E44B28, o nome da memoria (so le o chip de dados e desenha). */
u32 b_radio_nome(void)
{
    if (MODO_TELA != 2)
        return 0;
    telas_faixa(2, 68, 88, RADIO_NORMAL, RADIO_ITEM);
    return 1;
}

/* 0x01E45198, o medidor S: a de fabrica so redesenha quando o nivel (sinal / 6) muda, e grava o nivel
 * em b[0x14CCE]; o gancho faz o mesmo. */
u32 b_radio_sinal(void)
{
    unsigned nivel = RADIO_SINAL / 6;

    if (MODO_TELA != 2)
        return 0;
    if (RADIO_NIVEL != nivel) {
        RADIO_NIVEL = nivel;
        telas_faixa(2, 16, 36, RADIO_NORMAL, RADIO_ITEM);
    }
    return 1;
}

/* 0x01E44C68(item, valor), a seta e os ajustes: grava o valor (passo, BW, LNA, BFO) e desenha. O
 * gancho a roda inteira (radio_ajustes_fabrica, no fim do arquivo) - os dois envia() dela ficam
 * calados no modo 2 (gancho_radio_mudo), para a fileira de fabrica nao piscar na tela - e depois
 * desenha as faixas com o item. */
void radio_ajustes_fabrica(u32 item, u32 valor);

u32 b_radio_ajustes(u32 item, u32 valor)
{
    if (MODO_TELA != 2)
        return 0;
    radio_ajustes_fabrica(item, valor);
    telas_faixa(2, 36, 68, RADIO_NORMAL, item);
    telas_faixa(2, 88, 128, RADIO_NORMAL, item);
    return 1;
}

/* No lugar dos dois envia() de 0x01E44C68 (0x01E44CA8, a seta; 0x01E44FA8, a fileira de ajustes): no
 * radio a tela B desenha depois, entao o desenho de fabrica nao vai ao LCD. */
void gancho_radio_mudo(void)
{
    if (MODO_TELA != 2)
        ENVIA();
}

/* 0x01E49078, a frequencia sendo digitada (so desenha: a mascara e os digitos de 0x14A3A, e a unidade). */
u32 b_radio_digita(void)
{
    if (MODO_TELA != 2)
        return 0;
    telas_faixa(2, 36, 68, RADIO_DIGITANDO, RADIO_ITEM);
    return 1;
}

/* No lugar do envia() do "SEEK..." que a interface escreve no lugar da frequencia (0x01E4D3D4..
 * 0x01E4D3F8) ao comecar a busca. */
void gancho_radio_seek(void)
{
    if (MODO_TELA != 2) {
        ENVIA();
        return;
    }
    telas_faixa(2, 36, 68, RADIO_BUSCANDO, RADIO_ITEM);
}

/* 0x01E45A82(rodando), o scan QT: so desenha o titulo e o estado. */
u32 b_scanqt(u32 rodando)
{
    if (MODO_TELA != 17)
        return 0;
    telas_faixa(17, 0, 128, rodando != 0, 0);
    return 1;
}

/* No lugar do envia() que manda "Radio Initialization / Please Wait..." na partida do radio
 * (0x01E4A7D4; o modo ja e o 2, gravado em 0x01E4A612) e na troca de modulacao (0x01E3DEBE em
 * chines, 0x01E3DEEA em ingles: o set do item Modulation do menu do radio, 0x01E3DE52, que antes
 * redesenha a tela de um VFO de fabrica - a espera cobre a tela inteira). As duas vem logo antes de o
 * chip do radio iniciar: e aqui que os ajustes guardados sao conferidos (confere_radio, telas.c). */
void gancho_radio_inicio(void)
{
    confere_radio();
    telas_faixa(TELA_INICIO_RADIO, 0, 128, 0, 0);
}

/* No lugar do envia() da caixa do DTMF de fabrica: abrindo (0x01E4DA88, os dois lados) e a cada
 * digito (0x01E4A93C). No modo 7, a faixa da caixa na tela B (a cena a desenha no modo 7); fora dele,
 * o envia() de fabrica. A pergunta e o modo, e nao o ativa(): o layout() devolve 0 com o modo acima
 * de 5 (0x01E3B1B4), e a tecla # grava o 7 (0x01E4CD06, a unica escrita do 7) antes de abrir a caixa.
 * Com o ativa(), da v38 a v46 a caixa saiu sempre a de fabrica. Quem chega aos dois sitios (fecho
 * para tras no desmontado de fabrica): o 0x01E4DA88, so o caso 24 do tbh das acoes (0x01E4BE28, a
 * tecla #, nos modos 0 e 8 - o 8 e a sessao do cabo) e a digitacao da frequencia do radio
 * (0x01E4D93C, so pelo caso do modo 2), que segue com o envia() de fabrica como sempre; o 0x01E4A93C,
 * so pelo caso do modo 7 do tbh dos modos (0x01E4BEF8). */
void gancho_dtmf(void)
{
    struct estado e;

    if (MODO_TELA != 7) {
        ENVIA();
        return;
    }
    le_estado(&e);
    desenha(&e, TOPO_DTMF(&e), TOPO_DTMF(&e) + ALTURA_DTMF);
}

/* No lugar do envia() da tensao na partida (0x01E55F40): a tela B inteira, com a tensao lida como a
 * partida a le (0x01E45FB0 x 444 / 1000). */
void gancho_tensao(void)
{
    tensao_faixas(LE_TENSAO() * 444u / 1000, 0, 160, 0, 128);
}

/* No lugar do envia() da caixa da tensao com o 0 segurado (acao 33 das teclas, 0x01E4CF1C; a mesma
 * conta, 0x01E4CEA6..0x01E4CED2): so o cartao, por cima da tela no ar. Depois o firmware espera 1 s e
 * redesenha a tela inteira (0x01E4CF20..0x01E4CF28), como antes. */
void gancho_tensao_tecla(void)
{
    tensao_faixas(LE_TENSAO() * 444u / 1000, TENSAO_X0, TENSAO_X1, TENSAO_Y0, TENSAO_Y1);
}

/* No lugar da limpeza de fabrica na entrada do espectro (0x01E4CFE2, LIMPA(0): a faixa de cima em
 * cinza e o resto em preto): o fundo e a barra de cima da tela B. */
void gancho_espectro_limpa(void)
{
    telas_faixa(20, 0, 128, 0, 0);
}

/* No menu aberto (v59) nada da tela principal se desenha: a de fabrica pintava o lado em evidencia (o
 * selo de fabrica, que so conhece A e B, o nome e o numero) por cima da janela de pasta, no lugar do
 * resumo do menu de fabrica. O laco do sinal a chama recebendo (0x01E3C132 por 0x01E44812), e as tres
 * rotinas que pintam o bloco e o complemento (0x01E3BD04 e 0x01E3C06E) sao esta, a do lado em
 * evidencia (0x01E453E0) e a de um lado (0x01E413BE, no b_lado). */
static unsigned no_menu(void)
{
    return MODO_TELA == 1;
}

/* 0x01E3C132(0): a tela inteira. Ela zera o estado de TX (0x01E3C1D4); o gancho tambem. Com 1 e a
 * tela de um VFO, que fica a de fabrica e nao mexe no estado de TX (0x01E3C152 pula 0x01E3C1D4). */
u32 b_tela(u32 um_vfo)
{
    if (no_menu()) {
        if (!um_vfo)
            EM_TX = 0;
        return 1;
    }
    if (um_vfo || !ativa())
        return 0;
    EM_TX = 0;
    tela_b();
    return 1;
}

/* A barra de cima e a da tela B na tela de dois VFOs, nas outras telas e, desde a v41, no espectro. */
static unsigned barra_b(void)
{
    return ativa() || telas_modo() || MODO_TELA == 20;
}

/* 0x01E3B05C, a barra de cima; 0x01E3B03E e 0x01E3AE50, o nivel da bateria e o cadeado, que se
 * redesenham sozinhos - os tres leem o estado da RAM. */
u32 b_barra(void)
{
    struct estado e;

    if (!barra_b())
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

    if (!barra_b())
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

    if (!barra_b())
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

    if (no_menu())
        return 1;
    if (!ativa())
        return 0;
    le_estado(&e);
    desenha_lado(&e, e.evidencia);
    return 1;
}

/* 0x01E4B29C e 0x01E4B1DE: o numero de canal e a frequencia sendo digitados. */
u32 b_digitando(void)
{
    if (no_menu())
        return 1;
    if (!ativa())
        return 0;
    tela_b_digitando();
    return 1;
}

/* 0x01E413BE(lado, ...) e 0x01E3B2C4(lado, forcar), a linha de informacoes: os dois lados. A tecla
 * A/B (0x01E4DC9E) troca o lado e chama 0x01E41486, que, com b[0x14685] em 0, so redesenha o lado
 * novo por estas duas - e na tela B o cartao muda de lugar, entao o outro lado tambem muda. Sao
 * raras (A/B, potencia, modulacao); a troca de canal vem por b_evidencia. */
SEPARADA static u32 lado_principal(void)
{
    struct estado e;

    if (!ativa())
        return 0;
    le_estado(&e);
    desenha(&e, ALTURA_ESTADO, 128);
    return 1;
}

/* No menu e no scan QT, so o resumo; a estrutura da tela principal fica na lado_principal, para as
 * duas nao se somarem na pilha. */
u32 b_lado(void)
{
    unsigned m = telas_modo();

    if (m == 1 || m == 17) {
        if (m == 17 || !menu_pasta())      /* a janela de pasta do menu ocupa o lugar */
            desenha_resumo();
        return 1;
    }
    return lado_principal();
}

/* 0x01E46166, a leitura do nivel do sinal (v62). O laco do sinal a chama em qualquer modo de tela, perto
 * de uma vez por segundo: na volta em que o contador rapido (b[0x14612], de 0 a 9 a cada volta) da 0, o
 * lento (b[0x14614]) escolhe a tarefa (o tbh de 0x01E46A38), e o 3 e o 8 vem aqui (0x01E46C7A). Ela le o
 * RSSI e so chama o medidor nos modos 0 e 3 (0x01E46216), e o fim da recepcao nao passa pelo medidor:
 * no menu nada redesenhava a barra, e o indicador da v59 nunca aparecia. No menu (modo 1) o desvio faz
 * o trabalho no lugar dela, que ali so leria o RSSI: redesenha a barra, que o le_estado monta com quem
 * recebe (RECEBENDO_MENU). Em toda chamada, sem lembrar o que a barra mostra (v64): a v62 e a v63
 * guardavam isso em b[0x7031], o nivel do medidor ja desenhado, e o firmware o zera por conta propria
 * (b[0x7031] = 255 em 0x01E45A70, que o laco chama em 0x01E46F52); o indicador achava a barra limpa e
 * nao sumia no fim da recepcao. O diagnostico (v63d) provou no radio que redesenhar sempre funciona. */
SEPARADA static void barra_menu(void)
{
    struct estado e;

    le_estado(&e);
    desenha(&e, 0, ALTURA_ESTADO);
}

u32 b_sinal(void)
{
    if (MODO_TELA != 1)
        return 0;
#ifdef DIAG_RX
    DIAG_CONTA++;
#endif
    barra_menu();
    return 1;
}

/* 0x01E3B592(estado, nivel, lado, janela propria), o medidor; fora da tela inteira e da TX (que sao
 * nossas), so o RX chama (0x01E45328). O nivel que vem e o que vale (o MO passa 10, 0x01E4532C), e a
 * barra de cima acompanha, pelo RX. No espectro so a tecla MO chega aqui (o laco do sinal so chama o
 * medidor nos modos 0 e 3, 0x01E46216), e a de fabrica desenharia o medidor por cima do grafico: so
 * a barra. */
u32 b_medidor(u32 estado, u32 nivel, u32 lado)
{
    unsigned outra = telas_modo() != 0 || MODO_TELA == 20;

    if (!ativa() && !outra)
        return 0;
    medidor_b(estado, nivel, lado, outra);
    return 1;
}

/* ---------- a luz da tela por PWM (v60; o BRIGHTNESS, v61) ----------
 * A luz e o PB7 (pino 23): a rotina de fabrica (0x01E394D6) so liga e desliga o bit 7 do OUT da porta
 * B (0x1E5040, da tabela de 0x01E006D0 que 0x01E001BA le) e grava a marca b[0x15386]. O TIMER2
 * (0x1E0600), que o firmware nao usa, gera o PWM, como o timer_pwm_init do SDK da JieLi (o cristal de
 * 24 MHz, /4, contando), e o canal de saida 2 o leva ao pino: a rotina de fabrica que liga um canal
 * (0x01E4370A(pino, fonte), o gpio_output_channle do SDK) poe a fonte nos bits 27:24 do CON3 do IOMAP
 * (0x1E5124) e o pino com PU = PD = 1, OUT = 1 e DIE = 0. Com o pino no canal, o OUT escolhe o canal, e
 * a rotina de fabrica nao pode mais rodar (apagar passaria o pino ao CH0): o desvio na entrada dela
 * acende e apaga pelo ciclo do PWM. Antes do PWM (a partida acende a luz no logo, 0x01E55FB0 e
 * 0x01E5607E) roda a de fabrica. O CH2 so e usado pela UART2: a abertura generica de serial
 * (0x01E6A974, so na troca para o modo de atualizacao, 0x01E4C096, e em 0x01E54A92) pega a primeira
 * livre, a UART0 no CH0, a UART1 no CH1 e a UART2 no CH2; o cabo da partida e a UART1 no CH1
 * (0x01E55AF4, a fonte 17 de 0x01E5FCB6). Com a UART2 ligada, o PWM nao entra e a luz fica a de
 * fabrica; se ela for ligada depois, o pino so repete o TX dela. */
#define T2_CON       RAM(u32, 0x001E0600)
#define T2_CNT       RAM(u32, 0x001E0604)
#define T2_PRD       RAM(u32, 0x001E0608)
#define T2_PWM       RAM(u32, 0x001E060C)
#define UART2_CON0   RAM(u32, 0x001E2200)
#define PB_OUT       RAM(u32, 0x001E5040)   /* a porta B (JL_PORTB): OUT, DIE +0x0C, PD +0x14 */
#define PB_DIE       RAM(u32, 0x001E504C)
#define PB_PD        RAM(u32, 0x001E5054)
#define PINO_LUZ     0x80u                  /* o PB7 */
#define MARCA_LUZ    RAM(u8, 0x00015386)
#define LUZ_PRD      240u                   /* a escala dos ciclos: 24 MHz / 4 / 240 = 25 kHz */
/* A frequencia do PWM (v67, o LIGHT PWM da pasta CALIBRATION): o 0x903B dos ajustes (b[0x14BE7]), livre
 * pela mesma varredura da v59 (de 0x14BDF a 0x14BEB nenhum acesso de fabrica fora as copias do bloco),
 * 0 = 1 kHz, 1 = 5 kHz, 2 = 25 kHz e, desde a v68, 3 = 240 Hz e 4 = 580 Hz (o PRD a 6 MHz: 6000, 1200,
 * 240, 25000 e 10345). Fora da faixa vale 240 Hz, o padrao desde a v69 (no radio do dono, sem cintilar
 * e sem interferencia num radio encostado): e o que o RESET de fabrica deixa, 189 (a tabela 0x01E603C3
 * tem no lugar do 0x903B o byte 189 do texto que vem depois dela). Os valores gravados ficam os da v68.
 * O menu as mostra em ordem de frequencia (luz_pwm_posicao). O ciclo de cada nivel vai na mesma escala
 * (luz_ciclo). */
#define LUZ_FREQ     RAM(u8, 0x00014BE7)
#define LUZ_PADRAO   3                      /* 240 Hz */
static const u16 luz_prd[5] = {6000, 1200, 240, 25000, 10345};
/* O BRIGHTNESS (v61): o nivel no 0x9039 dos ajustes (b[0x14BE5], livre como os da v59), de 0 = 100% a
 * 4 = 10%, espacados para o olho; fora da faixa (o RESET de fabrica deixa 163) vale 100%. */
#define BRILHO       RAM(u8, 0x00014BE5)
static const u8 luz_ciclos[5] = {240, 144, 84, 48, 24};
#define CH2_T2_PWM   0x2E                   /* o CH2_T2_PWM_OUT do gpio.h do SDK */
FIRMWARE(CANAL_SAIDA, 0x01E4370A); void CANAL_SAIDA(unsigned pino, unsigned fonte);

static unsigned luz_prd_de(void)
{
    return luz_prd[LUZ_FREQ < 5 ? LUZ_FREQ : LUZ_PADRAO];
}

/* O ciclo do nivel, na escala do PRD de agora. */
static unsigned luz_ciclo(void)
{
    return luz_ciclos[BRILHO < 5 ? BRILHO : 0] * T2_PRD / LUZ_PRD;
}

/* O pino da luz com o ciclo dado (v62). Cada borda do PWM espalha um pouco de RF, que um receptor
 * encostado ouve (e o proprio radio, as vezes): o PWM so existe com a luz acesa abaixo de 100%. Acesa
 * a 100% ou apagada, o pino e saida comum, como a de fabrica - fora do canal, com o PD em 0 e o DIE em 1,
 * como a iniciacao da placa o deixa (0x01E54844..0x01E54854), e o OUT. Para o canal, a rotina de
 * fabrica, so com a UART2 desligada (o CH2 e dela). */
static void luz_poe(unsigned ciclo)
{
    if (ciclo && ciclo < T2_PRD && !(UART2_CON0 & 1)) {
        T2_PWM = ciclo;
        if (!(PB_PD & PINO_LUZ))
            CANAL_SAIDA(23, CH2_T2_PWM);
        return;
    }
    PB_PD &= ~PINO_LUZ;
    PB_DIE |= PINO_LUZ;
    if (ciclo)
        PB_OUT |= PINO_LUZ;
    else
        PB_OUT &= ~PINO_LUZ;
}

/* Do gancho da partida (bancos.c), com a luz como a partida a deixou: o TIMER2 fica contando, e o pino
 * so vai ao canal quando o nivel pede. */
void luz_inicia(void)
{
    if (UART2_CON0 & 1)
        return;
    T2_CON = 0;
    T2_CON = 2 << 2 | 1 << 4;
    T2_PRD = luz_prd_de();
    T2_PWM = 0;
    T2_CNT = 0;
    T2_CON |= 1;
    luz_poe(MARCA_LUZ ? luz_ciclo() : 0);
}

/* 0x01E394D6(acende): com o PWM ligado, a marca como a de fabrica grava e o ciclo; sem ele, a de
 * fabrica. */
u32 b_luz(u32 acende)
{
    if (!(T2_CON & 1))
        return 0;
    MARCA_LUZ = acende != 0;
    luz_poe(acende ? luz_ciclo() : 0);
    return 1;
}

/* O nivel e a frequencia novos na hora (o set do BRIGHTNESS e do LIGHT PWM, e os ajustes que mudam por
 * fora do menu, bancos.c). */
void luz_aplica(void)
{
    if (!(T2_CON & 1))
        return;
    if (T2_PRD != luz_prd_de()) {
        T2_PRD = luz_prd_de();
        T2_CNT = 0;
    }
    luz_poe(MARCA_LUZ ? luz_ciclo() : 0);
}

/* ---------- LIGHT PWM (v67, a pasta CALIBRATION) ----------
 * A frequencia do PWM da luz (LUZ_FREQ, acima). A energia das bordas que cai num canal de VHF/UHF e
 * proporcional a ela: 25 kHz e 1 kHz ainda deram interferencia num radio encostado, 240 Hz nao (v69,
 * o padrao). O teste da forca do pino (os bits do PB7 em 0x1E5058 e 0x1E505C, o HD0 e o HD do SDK) nao
 * mudou nada na luz, e saiu. */
#define VALOR_LINHA  RAM(u16, 0x00014BFC)   /* o valor da linha do item, no menu (o VALOR do bancos.c) */
const char titulo_luz_pwm[] = "LIGHT PWM";
const char *const luz_pwm_opcoes[5] = {"240 HZ", "580 HZ", "1 KHZ", "5 KHZ", "25 KHZ"};
static const u8 luz_pwm_valor[5] = {3, 4, 0, 1, 2};      /* posicao no menu -> valor gravado */
static const u8 luz_pwm_posicao[5] = {2, 3, 4, 0, 1};    /* valor gravado -> posicao no menu */

u16 luz_pwm_le(void)
{
    return luz_pwm_posicao[LUZ_FREQ < 5 ? LUZ_FREQ : LUZ_PADRAO];
}

void luz_pwm_grava(void)
{
    LUZ_FREQ = luz_pwm_valor[VALOR_LINHA < 5 ? VALOR_LINHA : 0];
    luz_aplica();
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
DESVIO(desvio_luz, b_luz, 0x01E394DE, "0x10, 0x04, 0xc1, 0xff, 0x80, 0x46, 0x01, 0x00");
DESVIO(desvio_sinal, b_sinal, 0x01E4616E, "0x77, 0x04, 0xc5, 0xff, 0x80, 0x46, 0x01, 0x00");
DESVIO(desvio_radio, b_radio, 0x01E44FB4, "0x77, 0x04, 0xe2, 0x9b");
DESVIO(desvio_radio_freq, b_radio_freq, 0x01E449BE, "0x79, 0x04, 0xe2, 0x9a");
DESVIO(desvio_radio_nome, b_radio_nome, 0x01E44B2C, "0x75, 0x04, 0xe2, 0x95");
DESVIO(desvio_radio_sinal, b_radio_sinal, 0x01E4519C, "0x7b, 0x04, 0xe2, 0x9c");
DESVIO(desvio_radio_ajustes, b_radio_ajustes, 0x01E44C6C, "0x7d, 0x04, 0xe2, 0x99");
DESVIO(desvio_scanqt, b_scanqt, 0x01E45A86, "0x76, 0x04, 0xe2, 0x9a");
DESVIO(desvio_radio_digita, b_radio_digita, 0x01E4907C, "0x77, 0x04, 0xe2, 0x9b");
DESVIO(desvio_editor_modo, b_editor_modo, 0x01E48724, "0x76, 0x04, 0xe2, 0x9c");
DESVIO(desvio_editor_quadro, b_editor_quadro, 0x01E48660, "0x7e, 0x04, 0x29, 0x16");

/* Como o DESVIO, mas no meio de uma rotina (v53): quando o gancho resolve, segue para `feito` na
 * rotina, em vez de voltar dela. */
#define DESVIO_MEIO(nome, gancho, volta, feito, bytes)                                             \
    __asm__(".set " #nome "_volta, " #volta "\n"                                                   \
            ".set " #nome "_feito, " #feito "\n"                                                   \
            ".section .text." #nome ",\"ax\"\n"                                                    \
            ".globl " #nome "\n"                                                                   \
            #nome ":\n\t"                                                                          \
            "[--sp] = rets\n\t"                                                                    \
            "[--sp] = {r3-r0}\n\t"                                                                 \
            "call " #gancho "\n\t"                                                                 \
            "if (r0 == 0) goto " #nome "_fabrica\n\t"                                              \
            "{r3-r0} = [sp++]\n\t"                                                                 \
            "{rets} = [sp++]\n\t"                                                                  \
            "goto " #nome "_feito\n"                                                               \
            #nome "_fabrica:\n\t"                                                                  \
            "{r3-r0} = [sp++]\n\t"                                                                 \
            "{rets} = [sp++]\n\t"                                                                  \
            ".byte " bytes "\n\t"                                                                  \
            "goto " #nome "_volta\n\t"                                                             \
            ".previous")

/* O EXIT fora da edicao, no meio da interface: "r0 = 349" (0x01E4D54A) comeca a saida do menu; voltar
 * aos grupos segue para o bipe do EXIT (0x01E4D59A), onde a saida de fabrica tambem termina. */
DESVIO_MEIO(desvio_menu_sai, b_menu_sai, 0x01E4D54E, 0x01E4D59A, "0x40, 0xe0, 0x5d, 0x01");
/* O OK editando, depois do set: "r0 = 3303" (0x01E49A86) comeca o fechamento da edicao; nos itens de
 * marcar varios segue para o redesenho e a volta (0x01E49C7C). */
DESVIO_MEIO(desvio_menu_marca, b_menu_marca, 0x01E49A8A, 0x01E49C7C, "0x40, 0xe0, 0xe7, 0x0c");
/* O EXIT editando, depois de cancelar a edicao: "r0 = 1400" (0x01E4D53C) le a quantidade de itens, que
 * com 1 fecha o menu; num grupo segue ao bipe do EXIT (0x01E4D59A), como com mais itens (v59). */
DESVIO_MEIO(desvio_menu_um, b_menu_um, 0x01E4D540, 0x01E4D59A, "0x40, 0xe0, 0x78, 0x05");

/* 0x01E44C68 de fabrica, chamada como rotina: os bytes que o desvio cobre e a volta. O rets que o
 * primeiro deles empilha e o de quem chamou esta, entao a de fabrica volta para la. */
__asm__(".section .text.radio_ajustes_fabrica,\"ax\"\n"
        ".globl radio_ajustes_fabrica\n"
        "radio_ajustes_fabrica:\n\t"
        ".byte 0x7d, 0x04, 0xe2, 0x99\n\t"
        "goto desvio_radio_ajustes_volta\n"
        ".previous");

/* ---------- envio e limpeza da tela de dois VFOs ----------
 * O gancho do envio entra nas 16 chamadas de envia() das rotinas da tela de dois VFOs
 * (REALCE_SITIOS_GANCHO, no montar.py) e o da limpeza nas duas que limpam e redesenham a tela inteira
 * (LIMPA_SITIOS). Ate a v36 eles tambem pintavam o realce verde da tela de fabrica, que so aparecia
 * no idioma chines; o chines saiu na v37, e o realce com ele.
 *
 * Com a tela B desenhando, nenhuma dessas rotinas deveria chegar ao envio - as treze acima nao rodam.
 * Se alguma chegar, pelo caminho que for, o gancho redesenha da tela B o lado a que a janela pertence
 * (pela separadora da limpeza de fabrica: 88 com o A em evidencia, 55 com o B), no lugar do que a
 * rotina de fabrica desenhou. A limpeza nao roda: a tela inteira vem logo depois (0x01E3C290 e
 * 0x01E40F1C passam por 0x01E3C132) e a desenha toda. Fora da tela de dois VFOs (layout() = 0) os
 * dois fazem o que a chamada de fabrica fazia. */
void gancho_envia(void)
{
    if (ativa()) {
        struct estado e;

        le_estado(&e);
        desenha_lado(&e, JAN_Y0 >= (e.evidencia ? 56u : 89u));
        return;
    }
    ENVIA();
}

void gancho_limpa(unsigned modo)
{
    if (!ativa())
        LIMPA(modo);
}
#endif
