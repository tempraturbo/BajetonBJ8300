/* Mensagens que qualquer radio entende (v65): Morse e APRS.
 *
 * O Morse sai pelo gerador de tom do chip de radio (o BK4819), o mesmo do roger: o tom liga e desliga
 * no ritmo do codigo, transmitindo, em FM (o "MCW" que qualquer receptor de FM ouve).
 *
 * O APRS e o padrao de todo radio com APRS: AX.25 a 1200 bps em AFSK Bell 202 (1200 Hz e 2200 Hz),
 * com NRZI, bit stuffing, flags 0x7E e o FCS do X.25. O modem FSK do chip so faz pares fixos de tons
 * (1200/1800 e 1200/2400, o do roger TONE1200), entao o AFSK sai pelo gerador de tom: a cada bit 0 a
 * frequencia do tom troca entre 1200 e 2200 Hz (o NRZI), na hora certa pelo contador do TIMER1.
 *
 * Nada aqui tem .data nem .bss (nao ha RAM nossa): o pacote e montado na pilha e mandado bit a bit.
 * Compila tambem no PC (TESTE_PC), onde tests/teste_mensagem.py troca o chip e o relogio por imitacoes
 * e confere o pacote, o AFSK e o decodificador; e, inteiro, com o laco do APRS RX (TESTE_RX, v75), onde
 * tests/teste_rx.py imita o modem do BK4819, a flash e as teclas.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

#if defined(TESTE_PC)
#include "teste_mensagem.h"           /* REG_W, TOM, agora_us e companhia, imitados */
#elif defined(TESTE_RX)
#include "teste_rx.h"                 /* o laco do APRS RX no PC: o chip, a flash, as teclas e o relogio (v75) */
#else
#define RAM(tipo, endereco)       (*(volatile tipo *)(endereco))
#define FIRMWARE(nome, endereco)  __asm__(".set " #nome ", " #endereco)

FIRMWARE(REG_W, 0x01E39D82);       void REG_W(unsigned, unsigned);   /* registrador do BK4819 */
FIRMWARE(TOM, 0x01E43B4E);         void TOM(unsigned, unsigned);     /* os 2 geradores, Hz x 10.32444 */
FIRMWARE(ESPERA_2MS, 0x01E36286);  void ESPERA_2MS(unsigned);        /* espera ocupada, n x 2 ms */

/* O relogio: b[0x14810] conta milissegundos, de 2 em 2 no tique do TIMER1 (a espera de fabrica,
 * 0x01E36286, espera n x 2 nele), e o contador do TIMER1 (0x1E0504, de 0 ao PRD em 0x1E0508) e a
 * fracao do tique - a conta de 0x01E4F888, que da os microssegundos dentro dele. */
#define TIQUE_MS   RAM(u32, 0x00014810)
#define T1_CNT     RAM(u32, 0x001E0504)
#define T1_PRD     RAM(u32, 0x001E0508)

static u32 agora_us(void)
{
    u32 ms, cnt;

    do {
        ms = TIQUE_MS;
        cnt = T1_CNT;
    } while (ms != TIQUE_MS);              /* o tique veio no meio: le de novo */
    return ms * 1000 + cnt * 2000 / T1_PRD;
}
#endif

#define HZ(f)  ((u16)((f) * 10.32444 + 0.5))     /* unidade do reg 0x71, a conta do roger */
/* A mesma conta para um tom que so se sabe na hora (o do Morse), sem ponto flutuante: 330382 / 32000. */
#define HZ_INT(f)  (((f) * 330382u + 16000) / 32000)

/* ---------- Morse ----------
 * Um byte por caractere, de '!' a 'Z': um 1 de marca e, depois dele, um bit por elemento, do primeiro
 * ao ultimo (1 = traco, 0 = ponto): "A" (.-) = 0b101. 0 = sem codigo (# % * < >). Gerada pelos codigos
 * da ITU e conferida na volta. */
static const u8 MORSE[58] = {
    0x6B, 0x52, 0x00, 0x89, 0x00, 0x28, 0x5E, 0x36, 0x6D, 0x00, 0x2A, 0x73,
    0x61, 0x55, 0x32, 0x3F, 0x2F, 0x27, 0x23, 0x21, 0x20, 0x30, 0x38, 0x3C,
    0x3E, 0x78, 0x6A, 0x00, 0x31, 0x00, 0x4C, 0x5A, 0x05, 0x18, 0x1A, 0x0C,
    0x02, 0x12, 0x0E, 0x10, 0x04, 0x17, 0x0D, 0x14, 0x07, 0x06, 0x0F, 0x16,
    0x1D, 0x0A, 0x08, 0x03, 0x09, 0x11, 0x0B, 0x19, 0x1B, 0x1C,
};

/* O codigo de um caractere (minuscula vira maiuscula), ou 0. */
static unsigned morse_de(unsigned c)
{
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    return c >= '!' && c <= 'Z' ? MORSE[c - '!'] : 0;
}

/* Parar no meio (o PTT ou uma tecla), mais abaixo; no PC, a imitacao. Chamada direta, nao por
 * ponteiro: o conferir.py so aceita destino que ele confere. */
static unsigned para(void);

/* Toca o texto com o gerador ja ligado: a unidade e 1200/wpm ms (a palavra PARIS), ponto 1, traco 3,
 * 1 entre elementos, 3 entre letras e 7 entre palavras. A espera conta de 2 em 2 ms. para() e
 * consultada antes de cada caractere e de cada elemento (o radio limpa o watchdog nela; o mais longe
 * entre duas e 10 unidades, 2,4 s a 5 wpm); devolve 0 se ela pediu para parar no meio. */
unsigned morse_toca(const char *texto, unsigned n, unsigned wpm, unsigned hz)
{
    unsigned u = 600 / wpm, i, c, e;

    for (i = 0; i < n; i++) {
        if (para())
            return 0;
        c = morse_de((u8)texto[i]);
        if (!c) {                          /* espaco (ou caractere sem codigo): fecha a palavra */
            ESPERA_2MS(4 * u);             /* 3 da letra anterior + 4 = 7 */
            continue;
        }
        for (e = 0x80; !(c & e); e >>= 1)  /* acha a marca */
            ;
        for (e >>= 1; e; e >>= 1) {
            if (para())
                return 0;
            TOM(HZ_INT(hz), 0);
            ESPERA_2MS(c & e ? 3 * u : u);
            TOM(0, 0);
            ESPERA_2MS(u);
        }
        ESPERA_2MS(2 * u);                 /* 1 do elemento + 2 = 3 entre letras */
    }
    return 1;
}

/* ---------- AX.25 ----------
 * Endereco: 6 letras deslocadas 1 bit para a esquerda (espaco no que faltar) e o byte do SSID:
 * 0x60 | ssid << 1, com o bit 7 = C no destino (comando, como o APRS manda) e o bit 0 no ultimo
 * endereco. O indicativo vem em texto ("PY2ABC-7"): ate 6 letras e o SSID depois do '-'. */
static u8 *ax_endereco(u8 *p, const char *ind, unsigned n, unsigned extra)
{
    unsigned i = 0, k, ssid = 0;

    for (k = 0; k < 6; k++) {
        unsigned c = i < n && ind[i] != '-' ? (u8)ind[i++] : ' ';
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        *p++ = c << 1;
    }
    while (i < n && ind[i] != '-')         /* indicativo com mais de 6: corta */
        i++;
    if (i < n)
        for (i++; i < n && ind[i] >= '0' && ind[i] <= '9'; i++)
            ssid = ssid * 10 + ind[i] - '0';
    *p++ = 0x60 | (ssid & 15) << 1 | extra;
    return p;
}

/* O FCS do AX.25: CRC-16 do X.25 (polinomio 0x8408 refletido, comeca em 0xFFFF, sai invertido). */
static u16 crc_byte(u16 c, unsigned v)
{
    unsigned k;

    c ^= v;
    for (k = 0; k < 8; k++)
        c = c & 1 ? c >> 1 ^ 0x8408 : c >> 1;
    return c;
}

static u16 fcs(const u8 *p, unsigned n)
{
    u16 c = 0xFFFF;

    while (n--)
        c = crc_byte(c, *p++);
    return ~c;
}

/* Monta o quadro de UI do APRS em q (sem flags, com o FCS no fim, byte baixo primeiro) e devolve o
 * tamanho. caminho: 0 nenhum, 1 WIDE1-1, 2 WIDE1-1,WIDE2-1. info: o campo de informacao, que o
 * chamador monta (mensagem, status). */
#define DESTINO_APRS  "APZBJ8"            /* APZ = software experimental (lista de tocalls do APRS) */
static const char WIDE[2][8] = {"WIDE1-1", "WIDE2-1"};

unsigned ax_quadro(u8 *q, const char *origem, unsigned n_origem, unsigned caminho,
                   const char *info, unsigned n_info)
{
    u8 *p = q;
    unsigned i, f;

    p = ax_endereco(p, DESTINO_APRS, 6, 0x80);
    p = ax_endereco(p, origem, n_origem, caminho ? 0 : 1);
    for (i = 0; i < caminho; i++)
        p = ax_endereco(p, WIDE[i], 7, i + 1 == caminho);
    *p++ = 0x03;                           /* UI */
    *p++ = 0xF0;                           /* sem camada 3 */
    for (i = 0; i < n_info; i++)
        *p++ = info[i];
    f = fcs(q, p - q);
    *p++ = f;
    *p++ = f >> 8;
    return p - q;
}

/* ---------- AFSK 1200 ----------
 * Bell 202: marca (1) = 1200 Hz, espaco (0) = 2200 Hz, 1200 bps. NRZI: o bit 0 troca o tom, o 1 o
 * mantem. Cada bit dura 2500/3 us: o alvo anda 833, 833 e 834 us, 2500 a cada 3 bits, sem acumular
 * erro (e em microssegundos, que dao a volta nos 32 bits junto com o relogio). */
#define MARCA   HZ(1200)
#define ESPACO  HZ(2200)

struct afsk {
    u32 alvo;                              /* fim do bit em curso, em us */
    u8 terco;                              /* qual dos 3 bits do ciclo de 2500 us */
    u8 tom;                                /* 0 marca, 1 espaco */
    u8 uns;                                /* 1s seguidos, para o bit stuffing */
};

static void afsk_bit(struct afsk *a, unsigned b)
{
    if (++a->terco == 3)
        a->terco = 0;
    a->alvo += a->terco ? 833 : 834;
    while ((s32)(agora_us() - a->alvo) < 0)
        ;
    if (!b) {
        a->tom ^= 1;
        REG_W(0x71, a->tom ? ESPACO : MARCA);
    }
}

static void afsk_byte(struct afsk *a, unsigned v, unsigned stuffing)
{
    unsigned k, b;

    for (k = 0; k < 8; k++, v >>= 1) {     /* o bit baixo primeiro */
        b = v & 1;
        afsk_bit(a, b);
        if (!stuffing)
            continue;
        if (!b)
            a->uns = 0;
        else if (++a->uns == 5) {          /* cinco 1s: um 0 de enchimento */
            afsk_bit(a, 0);
            a->uns = 0;
        }
    }
}

/* Manda o quadro com o gerador ja ligado: flags de abertura (o TXDELAY), o quadro com stuffing e
 * flags de fecho. O tom comeca no espaco (v74; ate a v73, na marca): o primeiro bit 0 da flag ja o troca,
 * e as flags ficam paradas na marca, 1200 Hz, a polaridade que o sync de um BK4819 pega (um UV-K5 ou um
 * BJ-8300 de antes da v71 nao sincronizavam no preambulo parado em 2200 Hz). */
void afsk_manda(const u8 *q, unsigned n, unsigned flags)
{
    struct afsk a;
    unsigned i;

    a.uns = a.terco = 0;
    a.tom = 1;
    TOM(ESPACO, 0);
    a.alvo = agora_us();
    for (i = 0; i < flags; i++)
        afsk_byte(&a, 0x7E, 0);
    for (i = 0; i < n; i++)
        afsk_byte(&a, q[i], 1);
    for (i = 0; i < 3; i++)
        afsk_byte(&a, 0x7E, 0);
    afsk_bit(&a, 1);                       /* o ultimo bit inteiro no ar antes de desligar o tom */
}

/* ---------- HDLC na recepcao ----------
 * Recebe os bits de linha (o tom de cada bit: 1 marca, 0 espaco, ou o contrario - o NRZI nao liga
 * para a polaridade), desfaz o NRZI e o stuffing, acha as flags e devolve o tamanho do quadro quando
 * um fecha com o FCS certo (0 nos outros bits). O FCS e calculado byte a byte enquanto o quadro chega
 * (v70): o quadro pode ter qualquer tamanho do AX.25, e so os primeiros `max` bytes ficam em h->q, que
 * e o que a tela usa (a v65 a v69 descartavam todo quadro maior que o buffer, de 128 B: os beacons de
 * digipeater com comentario e os quadros ja repetidos passam disso). Quadro com o FCS errado e guardado
 * inteiro passa pela correcao (conserta) antes de ser recusado. So o quadro APRS e devolvido, e so ele
 * conta como FCS errado (v74, cabecalho_aprs). */
struct hdlc {
    u8 *q;
    u16 max, n;                            /* n conta o quadro inteiro, mesmo o que nao coube */
    u16 crc;                               /* o registro do CRC ate aqui */
    u16 ruins, consertados;                /* quadros com o FCS errado, e os que a correcao salvou */
    u8 linha;                              /* o bit de linha anterior */
    u8 uns;                                /* 1s seguidos */
    u8 nbits, byte;                        /* bits ja juntados no byte em curso */
    u8 dentro;                             /* ja passou por uma flag */
};

#define CRC_CERTO  0xF0B8                  /* o que sobra no registro de um quadro com o FCS certo */

void hdlc_comeca(struct hdlc *h, u8 *q, unsigned max)
{
    h->q = q;
    h->max = max;
    h->n = h->nbits = h->byte = h->uns = h->dentro = h->linha = 0;
    h->crc = 0xFFFF;
}

/* O cabecalho tem forma de APRS? Enderecos de 6 letras ou numeros (com espaco), de 2 a 10, o ultimo com
 * o bit 0, controle UI (0x03) e PID 0xF0. Devolve onde a info comeca, ou 0. O ruido quase nunca passa
 * aqui (v74): e o que separa o quadro APRS que chegou com bits errados (o BAD) do lixo depois do pacote,
 * e o quadro AX.25 que nao e APRS (de outro protocolo) nao vai para a tela. */
static unsigned cabecalho_aprs(const u8 *q, unsigned n)
{
    unsigned i = 0, k, c;

    do {
        if (i + 9 > n || i >= 70)
            return 0;
        for (k = 0; k < 6; k++) {
            c = q[i + k];
            if (c & 1)
                return 0;
            c >>= 1;
            if (c != ' ' && (c < '0' || c > '9') && (c < 'A' || c > 'Z'))
                return 0;
        }
        i += 7;
    } while (!(q[i - 1] & 1));
    return i >= 14 && q[i] == 0x03 && q[i + 1] == 0xF0 ? i + 2 : 0;
}

/* O quadro inteiro tem forma de APRS: o cabecalho e a info so com o que o APRS manda, de 0x1C em diante,
 * CR e LF (como o sanity check do Direwolf; v74). A correcao so aceita um conserto que passe aqui. */
static unsigned parece_aprs(const u8 *q, unsigned n)
{
    unsigned i = cabecalho_aprs(q, n);

    if (!i)
        return 0;
    for (; i < n; i++)
        if (q[i] < 0x1C && q[i] != '\r' && q[i] != '\n')
            return 0;
    return 1;
}

static void troca_par(u8 *q, unsigned p)
{
    q[p >> 3] ^= 1 << (p & 7);
    q[(p + 1) >> 3] ^= 1 << ((p + 1) & 7);
}

/* Dois bits vizinhos errados (o NRZI faz de um tom trocado dois bits errados seguidos), como o FIX_BITS do
 * Direwolf, pela linearidade do CRC: o registro que sobrou difere do certo pelo efeito do erro, e o de um
 * bit na posicao p e o passo do CRC aplicado (total - p) vezes a um 1 - de tras para frente, um passo por
 * posicao, sem refazer o CRC do quadro. Troca o par e devolve 1 + a posicao do primeiro, ou 0. A busca de
 * um bit so (ate a v73) saiu: o CRC-CCITT tem o fator (x+1), entao a sindrome de um erro de peso par (todo
 * tom trocado) tem paridade par e a de um bit so, impar - nunca casam, e ela so aceitava falso positivo. */
static unsigned conserta(u8 *q, unsigned n, u16 crc)
{
    u16 s = crc ^ CRC_CERTO, v = 1, ant = 0;
    unsigned p = n * 8;

    while (p--) {
        v = v & 1 ? v >> 1 ^ 0x8408 : v >> 1;
        if (ant && (v ^ ant) == s) {
            troca_par(q, p);
            return p + 1;
        }
        ant = v;
    }
    return 0;
}

unsigned hdlc_bit(struct hdlc *h, unsigned linha)
{
    unsigned b = linha == h->linha, n, m, crc;

    h->linha = linha;
    if (b) {
        if (++h->uns > 6) {                /* sete 1s: aborta, espera a proxima flag */
            h->dentro = 0;
            h->n = h->nbits = h->byte = 0;
            h->crc = 0xFFFF;
            return 0;
        }
    } else {
        unsigned uns = h->uns, alinhado = h->nbits == 7;
        h->uns = 0;
        if (uns == 6) {                    /* 0111111 0: flag */
            n = h->dentro ? h->n : 0;
            crc = h->crc;
            h->n = h->nbits = h->byte = 0;
            h->crc = 0xFFFF;
            h->dentro = 1;
            if (n < 17)
                return 0;
            m = n - 2 < h->max ? n - 2 : h->max;
            /* a flag deixou os seus 7 primeiros bits no byte em curso: o quadro so fechou inteiro se
             * estava alinhado antes dela */
            if (alinhado && crc == CRC_CERTO)
                return cabecalho_aprs(h->q, m) ? m : 0;
            if (alinhado && n <= h->max && (crc = conserta(h->q, n, crc))) {
                if (parece_aprs(h->q, m)) {
                    h->consertados++;
                    return m;
                }
                troca_par(h->q, crc - 1);  /* o conserto nao era: desfeito */
            }
            /* o BAD (v74): um quadro com cabecalho de APRS que chegou com bits errados, alinhado ou nao
             * (antes so o alinhado contava). O ruido depois do pacote quase nunca tem esse cabecalho. */
            if (cabecalho_aprs(h->q, m))
                h->ruins++;
            return 0;
        }
        if (uns == 5)                      /* 0 de enchimento: some */
            return 0;
    }
    if (!h->dentro)
        return 0;
    h->byte |= b << h->nbits;
    if (++h->nbits == 8) {
        h->crc = crc_byte(h->crc, h->byte);
        if (h->n < h->max)
            h->q[h->n] = h->byte;
        if (++h->n > 400)                  /* o AX.25 nao passa de ~330: e ruido */
            h->dentro = 0;
        h->nbits = h->byte = 0;
    }
    return 0;
}

/* Um numero com pelo menos `largura` algarismos (zeros a esquerda). Sem recursao (v76): ela pesava na
 * pilha do guarda, que monta o texto. */
static char *numero(char *p, unsigned v, unsigned largura)
{
    unsigned m = 1;

    while (--largura)
        m *= 10;
    while (v / m >= 10)
        m *= 10;
    for (; m; m /= 10)
        *p++ = '0' + v / m % 10;
    return p;
}

static char *decimal(char *p, unsigned v)
{
    return numero(p, v, 1);
}

/* Um indicativo do quadro (7 bytes) em texto, "PY2ABC-7"; devolve o fim. */
static char *aprs_indicativo(char *s, const u8 *a)
{
    unsigned k, ssid = a[6] >> 1 & 15;

    for (k = 0; k < 6 && a[k] >> 1 != ' '; k++)
        *s++ = a[k] >> 1;
    if (ssid) {
        *s++ = '-';
        s = decimal(s, ssid);
    }
    return s;
}

/* Os simbolos de APRS mais comuns da tabela primaria, pelo caractere: o que a estacao e. */
static const char SIMBOLOS[] = "#DIGI\0&GATE\0>CAR\0-HOME\0_WX\0kTRUCK\0uTRUCK\0vVAN\0[WALK\0bBIKE\0"
                               "<MOTO\0jJEEP\0RRV\0sBOAT\0OBALLOON\0yYAGI\0rRPTR\0aAMB\0fFIRE\0";

static char *copia(char *s, const char *t)
{
    while (*t)
        *s++ = *t++;
    return s;
}

#define TEXTO_MAX  104                  /* o quadro em texto, na tela e no historico (v74: 120; v75: o
                                         * registro de 128 B guarda tambem o ack e a hora) */
#define TEXTO_BRUTO 0x100               /* no n_meu do aprs_texto: a info como veio, sem decodificar (v76) */

/* O caractere como a tela o mostra: o que nao e ASCII visivel vira ponto (um 0 cortaria o texto, e um
 * byte de 0x80 em diante e icone na fonte do radio). */
static unsigned visivel(unsigned c)
{
    return c >= ' ' && c < 127 ? c : '.';
}

static unsigned alfanumerico(unsigned c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static unsigned algarismo(unsigned c)
{
    return c >= '0' && c <= '9';
}

/* As letras de U+00C0 a U+00FF (os bytes 0xC3 0x80..0xBF do UTF-8) sem o acento: a fonte do radio nao as
 * tem, e cada byte virava um ponto (v74). */
static const char SEM_ACENTO[65] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";

/* ---------- a posicao e o que vem com ela (v76) ----------
 * Tudo em unidades daqui: velocidade em km/h (de nos, ou de milhas por hora no tempo), altitude em metros
 * (de pes), temperatura em graus Celsius, rumo em graus. */

/* "DD MM.mmH" (graus com 2 ou 3 casas), de graus e centesimos de minuto. */
static char *coordenada(char *p, unsigned graus, unsigned cmin, unsigned casas, unsigned letra)
{
    p = numero(p, graus, casas);
    *p++ = ' ';
    p = numero(p, cmin / 100, 2);
    *p++ = '.';
    p = numero(p, cmin % 100, 2);
    *p++ = letra;
    return p;
}

/* "DDMM.mm" + a letra, de uma posicao sem compressao, em "DD MM.mmL"; devolve o fim, ou 0 se nao for
 * uma posicao. graus: 2 na latitude, 3 na longitude. Copiada como veio: os espacos da posicao
 * imprecisa de proposito ficam. */
static char *aprs_coordenada(char *s, const u8 *p, unsigned graus)
{
    unsigned k;

    for (k = 0; k < graus + 5; k++)
        if (k == graus + 2 ? p[k] != '.' : !algarismo(p[k]) && p[k] != ' ')
            return 0;
    k = p[graus + 5];                      /* o hemisferio (v74) */
    if (graus == 2 ? k != 'N' && k != 'S' : k != 'E' && k != 'W')
        return 0;
    for (k = 0; k < graus + 6; k++) {
        if (k == graus)
            *s++ = ' ';
        *s++ = p[k];
    }
    return s;
}

/* " NOME" do simbolo, se e da lista da tabela primaria; senao, o caractere dele. */
static char *simbolo(char *p, unsigned tabela, unsigned codigo)
{
    const char *t = tabela == '/' ? SIMBOLOS : "";

    while (*t && *t != (char)codigo)
        while (*t++)
            ;
    *p++ = ' ';
    if (*t)
        return copia(p, t + 1);
    *p++ = visivel(codigo);
    return p;
}

/* " 45KM/H 120DEG", de nos e graus; nada com os dois em 0 (parado, ou sem o dado). */
static char *movimento(char *p, unsigned rumo, unsigned nos)
{
    if (!rumo && !nos)
        return p;
    *p++ = ' ';
    p = numero(p, (nos * 1852 + 500) / 1000, 1);
    p = copia(p, "KM/H ");
    p = numero(p, rumo, 1);
    return copia(p, "DEG");
}

static char *altitude(char *p, int metros)
{
    p = copia(p, "ALT ");
    if (metros < 0) {
        *p++ = '-';
        metros = -metros;
    }
    p = numero(p, metros, 1);
    *p++ = 'M';
    return p;
}

#define SEM_DADO  (-9999)

/* n algarismos (e espacos ou pontos de "sem dado") em x: o valor, ou SEM_DADO sem dado ou fora do formato.
 * Um '-' na frente vale (a temperatura). */
static int campo(const u8 *x, unsigned n)
{
    int v = 0, sinal = 1;

    if (*x == '-' && n > 1) {
        sinal = -1;
        x++;
        n--;
    }
    while (n--) {
        if (!algarismo(*x))
            return SEM_DADO;
        v = v * 10 + *x++ - '0';
    }
    return v * sinal;
}

/* O tempo (v76): de x ate fim, os campos que valem a tela - o vento (o "ddd/sss" que vem depois do simbolo
 * _ numa posicao, ou o c e o s do relatorio sem posicao), a temperatura (t, em F), a umidade (h, 00 = 100%)
 * e a pressao (b, em decimos de hPa); a chuva, a rajada e o resto passam sem mostrar. O "WX" fica com quem
 * chama (na posicao, o nome do simbolo _ ja o diz). Devolve o fim do texto, e em *px onde os campos acabam. */
static char *tempo(char *p, const u8 **px, const u8 *fim)
{
    static const char LARGURA[] = "c3s3g3t3r3p3P3h2b5L3l3#3";
    const u8 *x = *px;
    const char *l;
    int vento = SEM_DADO, rumo = SEM_DADO, v;

    if (x + 7 <= fim && x[3] == '/') {     /* o vento depois do simbolo: rumo/velocidade em mph */
        rumo = campo(x, 3);
        vento = campo(x + 4, 3);
        x += 7;
    }
    while (x < fim) {
        for (l = LARGURA; *l && *l != *x; l += 2)
            ;
        if (!*l || x + 1 + (l[1] - '0') > fim)
            break;
        v = campo(x + 1, l[1] - '0');
        if (*x == 'c')
            rumo = v;
        else if (*x == 's' && vento == SEM_DADO)
            vento = v;
        else if (*x == 't' && v != SEM_DADO) {   /* F em C, arredondado */
            v = (v - 32) * 50 / 9;
            p = copia(p, " ");
            if (v < 0) {
                *p++ = '-';
                v = -v;
            }
            p = numero(p, (v + 5) / 10, 1);
            *p++ = 'C';
        } else if (*x == 'h' && v >= 0) {
            *p++ = ' ';
            p = numero(p, v ? v : 100, 1);
            *p++ = '%';
        } else if (*x == 'b' && v > 0) {
            *p++ = ' ';
            p = numero(p, (v + 5) / 10, 1);
            p = copia(p, "HPA");
        }
        x += 1 + l[1] - '0';
    }
    if (vento >= 0) {                      /* mph em km/h (SEM_DADO e negativo) */
        *p++ = ' ';
        p = numero(p, (vento * 1609 + 500) / 1000, 1);
        p = copia(p, "KM/H");
        if (rumo >= 0) {
            *p++ = ' ';
            p = numero(p, rumo, 1);
            p = copia(p, "DEG");
        }
    }
    *px = x;
    return p;
}

static unsigned base91(const u8 *x, unsigned n)
{
    unsigned v = 0;

    while (n--)
        v = v * 91 + *x++ - 33;
    return v;
}

/* Graus e centesimos de minuto de num / div (num < 91^4, entao num % div * 6000 cabe em 32 bits). */
static char *fracao(char *p, unsigned num, unsigned div, unsigned casas, unsigned letra)
{
    return coordenada(p, num / div, num % div * 6000 / div, casas, letra);
}

/* Posicao comprimida (v76): "/YYYYXXXX$csT", a tabela, a latitude e a longitude em base 91 (4 cada), o
 * simbolo, o rumo e a velocidade em c e s (o rumo de 4 em 4 graus, a velocidade 1,08^s - 1 nos) e o tipo
 * em T (com os bits 4:3 em 2, c e s sao a altitude, que fica de fora). Devolve 0 se x nao e uma. */
static char *comprimida(char *p, const u8 *x, const u8 *fim)
{
    unsigned k, y, v;

    if (x + 13 > fim || !(x[0] == '/' || x[0] == '\\' || (x[0] >= 'A' && x[0] <= 'Z') ||
                          (x[0] >= 'a' && x[0] <= 'j')))
        return 0;
    for (k = 1; k <= 8; k++)
        if (x[k] < '!' || x[k] > '{')
            return 0;
    y = base91(x + 1, 4);                  /* a latitude e 90 - y / 380926; a longitude, x / 190463 - 180 */
    p = y <= 34283340 ? fracao(p, 34283340 - y, 380926, 2, 'N') : fracao(p, y - 34283340, 380926, 2, 'S');
    *p++ = ' ';
    y = base91(x + 5, 4);
    p = y >= 34283340 ? fracao(p, y - 34283340, 190463, 3, 'E') : fracao(p, 34283340 - y, 190463, 3, 'W');
    p = simbolo(p, x[0], x[9]);
    if (x[10] >= '!' && x[10] <= 'z' && ((x[12] - 33) >> 3 & 3) != 2) {
        for (v = 1000, k = x[11] - 33; k-- && k < 90; )
            v = v * 108 / 100;
        p = movimento(p, (x[10] - 33) * 4, (v - 1000 + 500) / 1000);
    }
    return p;
}

/* As mensagens do Mic-E, pelos tres bits do destino (o A primeiro). */
static const char *const MIC_E[8] = {"EMERGENCY", "PRIORITY", "SPECIAL", "COMMITTED", "RETURNING",
                                     "IN SERVICE", "EN ROUTE", "OFF DUTY"};

/* Mic-E (v76), o formato dos radios Kenwood e Yaesu e de muitos rastreadores: a latitude, a mensagem e os
 * hemisferios vem nos 6 caracteres do destino (d: 0-9, A-J, P-Y valem algarismos, e K, L e Z espaco; nos
 * tres primeiros, A-K e P-Z sao bits 1 da mensagem; no 4o, P-Z e norte; no 5o, P-Z soma 100 a longitude; no
 * 6o, P-Z e oeste); a longitude, a velocidade, o rumo e o simbolo, nos 8 bytes depois do ` ou do ' (cada
 * um + 28; x aponta o primeiro); depois, o tipo do radio (> ] ` ') e a altitude "xxx}" (metros + 10000,
 * base 91), se ha.
 * Devolve 0 se o destino nao e de Mic-E; senao o fim do texto, e em *px onde o comentario comeca. */
static char *mic_e(char *p, const u8 *d, unsigned desloc, const u8 **px, const u8 *fim)
{
    const u8 *x = *px;
    unsigned k, c, lat = 0, bits = 0, proprio = 0, g, m, vel, rumo;

    if (x + 8 > fim)
        return 0;
    for (k = 0; k < 6; k++) {
        c = d[k] >> desloc;
        if (algarismo(c))
            g = c - '0';
        else if (c >= 'A' && c <= 'J')
            g = c - 'A';
        else if (c >= 'P' && c <= 'Y')
            g = c - 'P';
        else if (c == 'K' || c == 'L' || c == 'Z')
            g = 0;
        else
            return 0;
        lat = lat * 10 + g;
        if (k < 3) {
            bits = bits << 1 | (c >= 'A' && c != 'L');
            proprio |= c >= 'A' && c <= 'K';   /* A-K: as mensagens proprias, sem nome */
        }
    }
    for (k = 0; k < 6; k++)
        if (x[k] < 28 || x[k] > 127)
            return 0;
    g = x[0] - 28 + ((d[4] >> desloc) >= 'P' ? 100 : 0);
    g -= g >= 180 && g <= 189 ? 80 : g >= 190 && g <= 199 ? 190 : 0;
    m = x[1] - 28;
    m -= m >= 60 ? 60 : 0;
    if (g > 179 || m > 59 || x[2] - 28 > 99)
        return 0;
    p = coordenada(p, lat / 10000, lat % 10000, 2, (d[3] >> desloc) >= 'P' ? 'N' : 'S');
    *p++ = ' ';
    p = coordenada(p, g, m * 100 + x[2] - 28, 3, (d[5] >> desloc) >= 'P' ? 'W' : 'E');
    p = simbolo(p, x[7], x[6]);
    vel = (x[3] - 28) * 10 + (x[4] - 28) / 10;
    rumo = (x[4] - 28) % 10 * 100 + x[5] - 28;
    p = movimento(p, rumo >= 400 ? rumo - 400 : rumo, vel >= 800 ? vel - 800 : vel);
    x += 8;
    if (x < fim && (*x == '>' || *x == ']' || *x == '`' || *x == '\''))
        x++;
    if (x + 4 <= fim && x[3] == '}' && x[0] >= '!' && x[1] >= '!' && x[2] >= '!') {
        *p++ = ' ';
        p = altitude(p, (int)base91(x, 3) - 10000);
        x += 4;
    }
    if (!proprio) {
        *p++ = ' ';
        p = copia(p, MIC_E[bits]);
    }
    *px = x;
    return p;
}

/* O que pode vir logo depois do simbolo de uma posicao: "ddd/sss" (rumo e velocidade em nos) ou
 * "PHGphgd" (a potencia p^2 W, a altura 10 x 2^h pes e o ganho g dB do digipeater). Devolve o fim do
 * texto, e em *px depois da extensao, se havia uma. */
static char *extensao(char *p, const u8 **px, const u8 *fim)
{
    const u8 *x = *px;
    int r, v;

    if (x + 7 > fim)
        return p;
    if (x[3] == '/' && (r = campo(x, 3)) >= 0 && (v = campo(x + 4, 3)) >= 0) {   /* SEM_DADO e negativo */
        p = movimento(p, r, v);
        *px = x + 7;
    } else if (x[0] == 'P' && x[1] == 'H' && x[2] == 'G' && algarismo(x[3]) && algarismo(x[4]) &&
               algarismo(x[5])) {
        *p++ = ' ';
        p = numero(p, (x[3] - '0') * (x[3] - '0'), 1);
        p = copia(p, "W ");
        p = numero(p, ((10u << (x[4] - '0')) * 3048 + 5000) / 10000, 1);
        p = copia(p, "M ");
        p = numero(p, x[5] - '0', 1);
        p = copia(p, "DBI");
        *px = x + 7;
    }
    return p;
}

/* Uma posicao em x (sem compressao ou comprimida), com o que vem depois dela: o rumo e a velocidade, o
 * PHG, o tempo (o simbolo _). Devolve 0 se x nao e uma posicao; senao o fim do texto, e em *px onde o
 * comentario comeca. */
static char *posicao(char *p, const u8 **px, const u8 *fim)
{
    const u8 *x = *px;
    char *f;

    if (x + 19 <= fim && (f = aprs_coordenada(p, x, 2))) {
        *f++ = ' ';
        if (!(f = aprs_coordenada(f, x + 9, 3)))
            return 0;
        p = simbolo(f, x[8], x[18]);
        x += 19;
        p = x[-11] == '/' && x[-1] == '_' ? tempo(p, &x, fim) : extensao(p, &x, fim);
    } else if ((f = comprimida(p, x, fim))) {
        p = f;
        x += 13;
        if (x[-13] == '/' && x[-4] == '_')
            p = tempo(p, &x, fim);
    } else {
        return 0;
    }
    *px = x;
    return p;
}

/* O quadro em texto legivel, ate TEXTO_MAX caracteres (48 ate a v73: o que cabia em duas linhas; a tela
 * abre o resto no OK), terminado em 0:
 *  - mensagem para o meu indicativo: "ORIGEM> texto" (sem o "{id", que e so da confirmacao), e devolve 1;
 *  - mensagem para outro: "ORIGEM>DESTINO: texto"; a confirmacao: "ORIGEM>DESTINO: ACK id";
 *  - posicao (! = / @, sem compressao ou comprimida, v76): "ORIGEM: 23 18.07S 046 35.59W CAR 45KM/H 120DEG
 *    comentario", com o PHG, a altitude em metros e o tempo decodificados (v76);
 *  - Mic-E (` '), objeto (;) e item ()) (v76): a posicao como acima, o objeto e o item com "OBJ NOME"
 *    ou "ITEM NOME" antes;
 *  - tempo sem posicao (_, v76): "ORIGEM: WX 25C 50% 1013HPA 6KM/H 220DEG";
 *  - o resto: "ORIGEM: info", como veio.
 * Num quadro de terceiros (v71: "}" no comeco da info, o que um igate traz da internet), ORIGEM e a
 * estacao de dentro, e a info e a dela. Na mensagem para mim, id recebe o numero dela (ate 5, sem o "}"
 * da resposta embutida), que o ack devolve (v75); nas outras, fica vazio. Com TEXTO_BRUTO no n_meu (o
 * APRS DECODE em OFF, v76), so a mensagem e decodificada: o resto vai como veio. No comentario, a altitude
 * "/A=pes" vira metros e o "!wXY!" da precisao extra (DAO) sai (v76). */
unsigned aprs_texto(char *s, const u8 *q, unsigned n, const char *meu, unsigned n_meu, char *id)
{
    char *p = s, *f;
    const u8 *x, *fim = q + n, *d = q;     /* d: o destino (do Mic-E), em bytes de endereco ou em texto */
    unsigned i = 7, k, para_mim = 0, bruto = n_meu & TEXTO_BRUTO;

    n_meu &= 0xFF;
    while (i + 7 <= n && !(q[i + 6] & 1))  /* o fim dos enderecos: o bit 0 do ultimo */
        i += 7;
    i += 7 + 2;                            /* controle e PID */
    for (k = i + 1; i < n && q[i] == '}' && k < n && k < i + 11 && q[k] != '>'; k++)
        ;
    if (i < n && q[i] == '}' && k < n && q[k] == '>') {
        /* de terceiros (v71), o que um igate traz da internet, "}ORIGEM>DESTINO,CAMINHO:info": a
         * estacao e a de dentro, e o resto segue pela info dela (o destino de dentro, o do Mic-E, vem em
         * texto) */
        while (++i < k)
            *p++ = visivel(q[i]);
        d = q + k + 1;
        while (k < n && q[k] != ':')
            k++;
        i = k + 1;
    } else {
        p = aprs_indicativo(p, q + 7);
    }
    if (i + 11 <= n && q[i] == ':' && q[i + 10] == ':') {
        for (k = 0; k < 9 && (k < n_meu ? q[i + 1 + k] == (u8)meu[k] : q[i + 1 + k] == ' '); k++)
            ;
        para_mim = k == 9 && n_meu;
        *p++ = '>';
        if (!para_mim) {
            for (k = 1; k < 10 && q[i + k] != ' '; k++)
                *p++ = visivel(q[i + k]);
            *p++ = ':';
        }
        *p++ = ' ';
        i += 11;
        for (k = i + 3; k < n && k < i + 8 && alfanumerico(q[k]); k++)
            ;
        if (k == n && k > i + 3 && q[i] == 'a' && q[i + 1] == 'c' && q[i + 2] == 'k') {
            p = copia(p, "ACK ");          /* "ack" e o id, de 1 a 5, e nada depois (v74: "ackermann" */
            i += 3;                        /* nao e confirmacao) */
        }
    } else {
        *p++ = ':';
        *p++ = ' ';
        x = q + i + 1;
        f = 0;
        if (i < n && !bruto) {
            k = q[i];
            if (k == '`' || k == '\'') {
                f = mic_e(p, d, d == q, &x, fim);
            } else if (k == '_' && x + 8 <= fim) {
                x += 8;                    /* sem posicao: MMDDHHMM e os campos (o vento em c e s) */
                f = tempo(copia(p, "WX"), &x, fim);
            } else if (k == ';' && x + 17 <= fim) {
                f = copia(p, "OBJ ");      /* ;NOME_____*DDHHMMz e a posicao */
                for (k = 0; k < 9; k++)
                    *f++ = visivel(x[k]);
                while (f[-1] == ' ')
                    f--;
                *f++ = ' ';
                x += 17;
                f = posicao(f, &x, fim);
            } else if (k == ')') {         /* )NOME!posicao, o nome de 3 a 9 */
                for (k = 0; k < 9 && x + k < fim && x[k] != '!' && x[k] != '_'; k++)
                    ;
                if (x + k < fim && k >= 3 && (x[k] == '!' || x[k] == '_')) {
                    f = copia(p, "ITEM ");
                    while (*x != '!' && *x != '_')
                        *f++ = visivel(*x++);
                    x++;
                    *f++ = ' ';
                    f = posicao(f, &x, fim);
                }
            } else if (k == '!' || k == '=' || k == '/' || k == '@') {
                if (k == '/' || k == '@')
                    x += 7;                /* com a hora: DDHHMMz */
                f = posicao(p, &x, fim);
            }
        }
        if (f) {
            p = f;
            i = x - q;
            *p++ = ' ';
        }
    }
    for (; i < n && p < s + TEXTO_MAX && !(para_mim && q[i] == '{'); i++) {
        k = q[i];
        if (!bruto && k == '/' && i + 9 <= n && q[i + 1] == 'A' && q[i + 2] == '=' &&
            (int)(k = campo(q + i + 3, 6)) != SEM_DADO) {
            if (p[-1] != ' ')
                *p++ = ' ';
            p = altitude(p, (int)k * 3048 / 10000);   /* "/A=pes" em metros */
            i += 8;
            continue;
        }
        k = q[i];
        if (!bruto && k == '!' && i + 4 < n && q[i + 4] == '!' && q[i + 1] >= 'A' && q[i + 1] <= 'z') {
            i += 4;                        /* o DAO "!wXY!": a precisao extra da posicao, sem texto */
            continue;
        }
        if ((k == 0xC2 || k == 0xC3) && i + 1 < n && q[i + 1] >= 0x80 && q[i + 1] < 0xC0) {
            i++;
            if (k == 0xC3)
                k = SEM_ACENTO[q[i] - 0x80];
            else                           /* U+00B0 e U+00BA (graus, ordinal) e U+00AA (v74) */
                k = q[i] == 0xB0 || q[i] == 0xBA ? 'o' : q[i] == 0xAA ? 'a' : '.';
        }
        if (k >= ' ' || bruto)             /* o CR e o LF do fim saem (v76) */
            *p++ = visivel(k);
    }
    while (p > s && p[-1] == ' ')
        p--;
    *p = 0;
    while (para_mim && i < n && q[i] != '{')   /* o texto pode ter parado no limite antes do numero */
        i++;
    k = 0;
    if (para_mim && i < n)
        while (++i < n && k < 5 && alfanumerico(q[i]))
            id[k++] = q[i];
    id[k] = 0;
    return para_mim;
}

#ifndef TESTE_PC
/* ---------- o menu: as pastas MORSE e APRS ---------- */
FIRMWARE(SPRINTF, 0x01E19DCC);        int SPRINTF(char *, const char *, ...);
FIRMWARE(LIMPA, 0x01E3ACBA);          void LIMPA(unsigned);
FIRMWARE(SAI_DO_EDITOR, 0x01E3C7AA);  void SAI_DO_EDITOR(void);
FIRMWARE(REG_R, 0x01E407C0);          unsigned REG_R(unsigned);
int le_registro(u32, void *, unsigned);          /* bancos.c: marca e soma conferidas, ou zerado */
void grava_registro(u32, void *, unsigned);      /* bancos.c: so em setor apagado ou ja nosso */

#define VALOR          RAM(u16, 0x00014BFC)      /* o valor da linha do item, no menu */
#define TEXTO          ((char *)&RAM(u8, 0x000129B0))   /* 28 B: o texto que o exibe devolve */

/* O editor de texto do firmware, o do CH-NAME: tamanho, maximo, cursor (a posicao de insercao),
 * modo (1 ABC) e a marca 0xAA (a primeira tecla troca o texto todo; o EXIT apaga a ultima letra e o
 * mantem), e o texto em 0x14A3A. O maximo vai a 37: o limite fixo de 15 da insercao (0x01E4A0F0)
 * vira 37 no montar.py, e as tres copias de fabrica com "%15s" (o texto vai a buffers de 27 e 28
 * bytes) passam pelo texto15, logo abaixo. Do 17o byte ao 38o o texto ocupa a copia dos registradores
 * que o espectro guarda ao entrar e devolve ao sair; o editor so existe no menu, e o menu nao abre
 * com o espectro na tela (o despacho das teclas do modo 20 so vai ao tratador dele, 0x01E4BFBE).
 * Do 39o byte em diante mora a estrutura do sistema de arquivos do SDK: 37 e o maximo. */
#define EDITOR_TAM     RAM(u8, 0x00014A35)
#define EDITOR_MAX     RAM(u8, 0x00014A36)
#define EDITOR_CURSOR  RAM(u8, 0x00014A37)
#define EDITOR_MODO    RAM(u8, 0x00014A38)
#define EDITOR_MARCA   RAM(u8, 0x00014A39)
#define EDITOR_TEXTO   (&RAM(char, 0x00014A3A))
#define EDITOR_PINYIN  RAM(u8, 0x00012A74)
#define MODO_ABC       1
#define MAX_MSG        37

/* Os registros na flash, um setor para cada (bancos.c: SETOR_MORSE e SETOR_APRS). Zerado vale o
 * padrao: 15 wpm, 700 Hz, WIDE1-1, sem SSID. */
#define SETOR_MORSE    0x000D4000u
#define SETOR_APRS     0x000D5000u

struct morse {
    u16 marca[2];                          /* "MRS1" */
    u8 tam, wpm, tom, reservado;           /* tom em dezenas de Hz */
    char texto[MAX_MSG + 1];
    u16 soma;
};

struct aprs {
    u16 marca[2];                          /* "APR1" */
    u8 tam_ind, tam_dest, tam_msg, ssid, caminho, id;
    char ind[6], dest[9], msg[MAX_MSG];
    u16 soma;
};

typedef char confere_morse[sizeof(struct morse) == 48 ? 1 : -1];
typedef char confere_aprs[sizeof(struct aprs) == 64 ? 1 : -1];

#define le_morse(m)     le_registro(SETOR_MORSE, (m), sizeof(struct morse))
#define grava_morse(m)  grava_registro(SETOR_MORSE, (m), sizeof(struct morse))
#define le_aprs(a)      le_registro(SETOR_APRS, (a), sizeof(struct aprs))
#define grava_aprs(a)   grava_registro(SETOR_APRS, (a), sizeof(struct aprs))

static void editor_abre(const char *t, unsigned n, unsigned max)
{
    unsigned i;

    if (n > max)
        n = max;
    for (i = 0; i <= MAX_MSG; i++)         /* a insercao nao escreve o 0 do fim: zera o resto */
        EDITOR_TEXTO[i] = i < n ? t[i] : 0;
    EDITOR_TAM = EDITOR_CURSOR = n;
    EDITOR_MAX = max;
    EDITOR_MARCA = 0xAA;
    EDITOR_MODO = MODO_ABC;
    EDITOR_PINYIN = 0;
}

static unsigned editor_texto(char *t, unsigned max)
{
    unsigned i, n = EDITOR_TAM;

    if (n > max)
        n = max;
    for (i = 0; i < n; i++)
        t[i] = EDITOR_TEXTO[i];
    return n;
}

/* A tecla 0 do editor (ABC e abc), no lugar de "()/\*# 0" (o ponteiro dela em 0x01E60B60, montar.py
 * PONTEIROS_LIVRE): o espaco no primeiro toque, e o - (o SSID do APRS), o / = + @ que o Morse e o APRS
 * usam e que nenhuma tecla alcancava (as duas sequencias que os tinham, @$%& e  =+-_, sao das teclas
 * ':' e ';', que o teclado nao gera). */
const char tecla_zero[] = " -/=+@()*#0";

/* No lugar das tres copias de fabrica do texto do editor com "%15s": 0x01E46CCE e 0x01E476BE, para
 * sp+21 no laco do sinal (a piscada; a fabrica poe um '_' no 26o byte), e 0x01E48FBA, para o TEXTO de
 * 28 B. O mesmo resultado ate 15 letras (alinhado a direita em 15), e no maximo 24, o que cabe nos
 * dois - o texto do editor agora vai a 37. O tema B nao mostra nenhuma das tres. */
int texto15(char *d, const char *formato, const char *s)
{
    unsigned n = 0, i = 0;

    (void)formato;
    while (n < 24 && s[n])
        n++;
    for (; i + n < 15; i++)
        d[i] = ' ';
    while (n--)
        d[i++] = *s++;
    d[i] = 0;
    return i;
}

/* Sair do editor como o CH-NAME no fim do set dele (o nome_grava do bancos.c faz o mesmo). */
static void editor_fecha(void)
{
    LIMPA(2);
    SAI_DO_EDITOR();
}

/* O texto no TEXTO para a linha do valor na pasta: ate 24 caracteres, ou 22 e "..". */
static char *resumo(const char *t, unsigned n)
{
    unsigned i, k = n > 24 ? 22 : n;

    for (i = 0; i < k; i++)
        TEXTO[i] = t[i];
    if (k < n) {
        TEXTO[k++] = '.';
        TEXTO[k++] = '.';
    }
    TEXTO[k] = 0;
    return TEXTO;
}

/* ---------- a transmissao ----------
 * O firmware nunca transmite com o menu aberto, e a TX dele e uma maquina de estados no laco do sinal
 * (0x01E4622A): o inicio (0x01E41022) poe b[0x1533C] = 1 e o passo 0 liga o TX; sem o PTT apertado o
 * passo 2 vai ao 3, e o 3 chama o fim (0x01E44604), que toca o roger ainda transmitindo, com o
 * microfone cortado (reg 0x30 = 0xC3FA), e volta a recepcao. O envio usa essa maquina inteira:
 *  - o SEND do menu so arma (ESTADO_MSG);
 *  - na volta seguinte do laco, gancho_ida_ptt (no lugar da chamada da ida do PTT, 0x01E46540)
 *    chama o inicio da TX como o PTT: potencia, desarme da dupla escuta, faixa permitida e canal
 *    ocupado ficam com ele, e o menu fecha (como com o PTT no menu);
 *  - gancho_fim_tx (no lugar da chamada do fim no passo 3, 0x01E46D8C) toca o Morse ou o AFSK no
 *    lugar do roger e chama o fim de fabrica com o roger zerado.
 * O estado mora no byte 0x903A dos ajustes (b[0x14BE6]), livre pela varredura da v59 (so o
 * diagnostico da v63 o usou). Os codigos sao letras que nada mais grava ali (o padrao de fabrica deixa
 * 202), e a partida e os ajustes pelo cabo o zeram (gancho_partida, gancho_ajustes): nada transmite
 * sozinho. O watchdog (~4 s; limpo pela interface a cada 10 ms, que nao roda enquanto o conteudo
 * toca) e limpo por nos (0x01E37E76), como o roger de fabrica faz antes dos tons. */
#define ESTADO_MSG     RAM(u8, 0x00014BE6)
#define ARMADO_MORSE   'M'
#define ARMADO_APRS    'A'
#define ARMADO_ACK     'K'               /* v75: o ack de uma mensagem para mim, armado pelo APRS RX */
#define NO_AR(e)       ((e) | 0x20)      /* 'm', 'a', 'k': a TX comecou, o conteudo vai no fim dela */
#define ARMADO(e)      ((e) == ARMADO_MORSE || (e) == ARMADO_APRS || (e) == ARMADO_ACK)
/* O registro do historico do APRS RX com a origem e o numero do ack (o setor * 32 + o lugar), no 0x903E
 * dos ajustes (b[0x14BEA], livre pela varredura da v59; so vale com o ESTADO_MSG em 'K'). */
#define ACK_REGISTRO   RAM(u8, 0x00014BEA)
#define HIST_SETOR(s)  (0x000D6000u + (s) * 0x1000u)   /* o historico do APRS RX: dois setores */
#define REG_HIST       128u              /* um registro (struct registro_rx, no APRS RX) */
#define ACK_PARA       (2 + TEXTO_MAX + 1)               /* a origem (9) e o numero (6) dentro dele */

FIRMWARE(LE_FLASH, 0x01E390AC);     void LE_FLASH(u32, void *, u32);
FIRMWARE(APAGA_SETOR, 0x01E3A126);  void APAGA_SETOR(u32);
FIRMWARE(GRAVA_FLASH, 0x01E3A25A);  void GRAVA_FLASH(u32, const void *, u32);

FIRMWARE(IDA_PTT, 0x01E43E8A);     void IDA_PTT(void);
FIRMWARE(ACORDA, 0x01E39EE4);      void ACORDA(void);         /* sai da economia de bateria */
FIRMWARE(LUZ_ABR, 0x01E3951C);     void LUZ_ABR(void);        /* acende a luz pelo ABR */
FIRMWARE(INICIO_TX, 0x01E41022);   void INICIO_TX(void);
FIRMWARE(FIM_TX, 0x01E44604);      void FIM_TX(void);
FIRMWARE(GERADOR, 0x01E43AFE);     void GERADOR(unsigned);
FIRMWARE(AUDIO_TX, 0x01E3FF3E);    void AUDIO_TX(unsigned);   /* reg 0x30: 4 = TX sem o microfone */
FIRMWARE(AF, 0x01E39DEA);          void AF(unsigned);
FIRMWARE(PEDE_AMPLI, 0x01E39E2C);  void PEDE_AMPLI(unsigned);
FIRMWARE(WDT, 0x01E37E76);         void WDT(void);
FIRMWARE(TECLAS, 0x01E43D50);      unsigned TECLAS(void);     /* codigo cru, 255 = nenhuma */
FIRMWARE(COLUNAS, 0x01E43CF4);     void COLUNAS(unsigned);    /* o laco repoe as colunas com 60 */
FIRMWARE(DESARMA_DW, 0x01E3FDE4);  void DESARMA_DW(void);     /* a dupla escuta no lado em evidencia */
#define TX_ESTADO      RAM(u8, 0x0001533C)       /* 0 parado, 1 TX, 2 recusada */
#define ALARME         RAM(u8, 0x00014950)
#define BIPE           RAM(u8, 0x00014924)
#define ROGER          RAM(u8, 0x00014B83)
#define PTT_SOLTO()    (RAM(u32, 0x001E5004) & 0x1000)    /* PA12; 0 = apertado (0x01E47986) */

static unsigned tecla(void)
{
    unsigned k = TECLAS();

    COLUNAS(60);
    return k;
}

/* Uma tecla apertada espera soltar, limpando o watchdog: o laco nao rodou enquanto ela descia, e ao
 * voltar ele a veria como tecla nova (o EXIT viraria o A/B na tela principal). */
static void espera_soltar(void)
{
    while (tecla() != 255) {
        WDT();
        ESPERA_2MS(5);
    }
}

#define TECLA_EXIT   5                     /* o codigo cru da varredura (a tabela 0x01E60369) */

/* O EXIT segurado no editor de texto sai sem salvar (v72). No uso normal a fabrica manda o toque curto
 * de uma tecla so quando ela e solta (o laco do sinal, 0x01E46372: apertar nao manda nada; segurar 1 s
 * manda o codigo longo, 35 no EXIT, e marca b[0x146F4] para o solto nao mandar nada), e por isso o
 * gancho da v70, na rotina do EXIT do editor, nunca via o EXIT apertado. Este fica no lugar da leitura
 * das teclas do laco do sinal (0x01E46280, a cada 10 ms), que conta as voltas da mesma tecla em
 * b[0x14680] e so aceita a tecla igual em tres leituras seguidas. Com o EXIT na 60a volta (0,6 s) no
 * editor de texto (MENU_TIPO 7 editando, como a tela B o reconhece), o cursor vai a 0 e a leitura passa a
 * devolver "nenhuma": a fabrica ve o EXIT solto, manda o toque curto, e a rotina do EXIT do editor, com o
 * cursor no comeco, sai sem salvar (0x01E4D486 -> 0x01E4D4F4). Enquanto o EXIT continuar apertado a
 * leitura segue devolvendo "nenhuma" (a marca no 0x903C dos ajustes, b[0x14BE8], livre: nenhum acesso de
 * fabrica pela base 0x14680 nem pelo endereco), para o solto de verdade nao virar outro EXIT, que subiria
 * o menu. Um valor que sobre na marca (os ajustes gravados com ela ligada) se apaga na primeira leitura
 * sem o EXIT. */
#define CONTA_TECLA   RAM(u8, 0x00014680)   /* voltas com a mesma tecla, no laco do sinal */
#define MENU_EDITANDO RAM(u8, 0x00014BF9)
#define MENU_TIPO     RAM(u8, 0x00014C00)
#define EXIT_PRESO    RAM(u8, 0x00014BE8)   /* 0x903C */

unsigned gancho_teclas(void)
{
    unsigned k = TECLAS();

    if (k != TECLA_EXIT)
        EXIT_PRESO = 0;
    else if (EXIT_PRESO)
        k = 255;
    else if (MENU_TIPO == 7 && MENU_EDITANDO == 1 && CONTA_TECLA >= 60) {
        EDITOR_CURSOR = 0;
        EXIT_PRESO = 1;
        k = 255;
    }
    return k;
}

/* Parar no meio: o PTT ou qualquer tecla. */
static unsigned para(void)
{
    unsigned parou;

    WDT();
    parou = tecla() != 255 || !PTT_SOLTO();
    espera_soltar();
    return parou;
}

/* O inicio da TX como o da ida do PTT (0x01E43F3E: acorda, acende a luz e inicia), e a ida de fabrica
 * por ultimo, como salto: com o PTT solto ela nao faz nada depois do nosso inicio. So o rets no quadro,
 * como na ida de fabrica: o caminho mais fundo continua o de fabrica. */
__attribute__((noinline)) static void inicia_tx(void)
{
    ACORDA();
    LUZ_ABR();
    INICIO_TX();
    IDA_PTT();
}

/* O "no ar" e posto antes do inicio, e nada aqui e lido depois dele: as duas saidas sao saltos. Se o
 * inicio recusar a TX (fora da faixa, canal ocupado: b[0x1533C] = 2, que volta a 0 com o PTT solto), a
 * volta seguinte ve a TX parada e desfaz o "no ar". Com um bipe tocando - o do proprio OK que armou o
 * envio (0x01E38BD4(61, 6), antes do set) -, a ida de fabrica nao transmite (0x01E43F42): o envio
 * fica armado e espera a volta seguinte. */
void gancho_ida_ptt(void)
{
    unsigned e = ESTADO_MSG;

    if (ARMADO(e & ~0x20) && (e & 0x20)) {
        if (!PTT_SOLTO() || !TX_ESTADO)    /* o PTT tomou a TX (ela e de voz), ou ela foi recusada */
            ESTADO_MSG = 0;
    } else if (ARMADO(e) && !BIPE) {
        ESTADO_MSG = 0;
        if (!TX_ESTADO && !ALARME && PTT_SOLTO()) {
            ESTADO_MSG = NO_AR(e);
            inicia_tx();
            return;
        }
    }
    IDA_PTT();
}

static void toca_morse(void)
{
    struct morse m;

    le_morse(&m);
    morse_toca(m.texto, m.tam < MAX_MSG ? m.tam : MAX_MSG, m.wpm >= 5 && m.wpm <= 40 ? m.wpm : 15,
               (m.tom >= 40 && m.tom <= 120 ? m.tom : 70) * 10);
}

/* O quadro do APRS: a mensagem ":DESTINO  :texto{id" (destinatario em 9 colunas, como o APRS pede)
 * ou, sem destinatario, o status ">texto"; com ack (v75), ":ORIGEM   :ackNUMERO", da origem e do numero
 * que o RX guardou no registro do historico em ack. O campo de informacao e montado no proprio quadro,
 * no lugar onde o ax_quadro o poria. O id da mensagem anda quando o texto ou o destino mudam
 * (aprs_guarda_texto). */
#define CAB(digis)  (16 + 7 * (digis))

static unsigned quadro_aprs(u8 *q, u32 ack)
{
    struct aprs a;
    char origem[10];
    unsigned i, n, digis;
    u8 *p;

    le_aprs(&a);
    if (!a.tam_ind || (!ack && !a.tam_msg))
        return 0;
    for (i = 0; i < a.tam_ind && i < 6; i++)
        origem[i] = a.ind[i];
    n = i;
    if (a.ssid && a.ssid < 16) {
        origem[n++] = '-';
        n = decimal(origem + n, a.ssid) - origem;
    }
    digis = a.caminho == 1 ? 2 : a.caminho == 2 ? 0 : 1;
    p = q + CAB(digis);
    if (ack) {
        *p++ = ':';
        LE_FLASH(ack, p, 9);
        p += 9;
        p = (u8 *)copia((char *)p, ":ack");
        LE_FLASH(ack + 9, p, 6);
        p[5] = 0;
        while (*p)
            p++;
        return ax_quadro(q, origem, n, digis, (char *)q + CAB(digis), p - q - CAB(digis));
    }
    if (a.tam_dest) {
        *p++ = ':';
        for (i = 0; i < 9; i++)
            *p++ = i < a.tam_dest ? a.dest[i] : ' ';
        *p++ = ':';
    } else {
        *p++ = '>';
    }
    for (i = 0; i < a.tam_msg && i < MAX_MSG; i++)
        *p++ = a.msg[i];
    if (a.tam_dest) {
        *p++ = '{';
        p = (u8 *)decimal((char *)p, a.id);
    }
    return ax_quadro(q, origem, n, digis, (char *)q + CAB(digis), p - q - CAB(digis));
}

/* A mensagem ou o status (ack 0), ou o ack cuja origem esta em ack. */
static void manda_aprs(u32 ack)
{
    u8 q[CAB(2) + 1 + 9 + 1 + MAX_MSG + 3 + 2];
    unsigned n = quadro_aprs(q, ack);

    if (!n)
        return;
    WDT();
    afsk_manda(q, n, 40);                  /* 40 flags: 270 ms para o outro lado abrir o squelch */
    WDT();
}

/* No lugar do roger, no fim da TX que o gancho_ida_ptt ligou: o mesmo preparo e o mesmo desligar do
 * roger BEEP (0x01E44622..0x01E44634 e 0x01E44656..0x01E4466A), o tom 2 desligado (reg 0x70: so o
 * tom 1, com o ganho do roger), e o fim de fabrica com o roger zerado - que nao grava nada. O OK que
 * armou o envio ainda pode estar apertado (a TX comeca umas voltas depois): o conteudo espera as teclas
 * soltarem, senao o Morse o veria como pedido de parar. */
void gancho_fim_tx(void)
{
    unsigned e = ESTADO_MSG, r, s31, s51;

    if (!(e & 0x20) || !ARMADO(e & ~0x20)) {
        FIM_TX();
        return;
    }
    ESTADO_MSG = 0;
    AUDIO_TX(4);                           /* o microfone cortado ja, antes das esperas (v74): a portadora
                                            * muda levava o som do ambiente ao ar */
    espera_soltar();
    if (e != NO_AR(ARMADO_MORSE))          /* v69: 0,5 s de portadora muda antes do pacote, para o */
        ESPERA_2MS(250);                   /* receptor (economia de bateria, squelch lento) abrir */
    GERADOR(1);
    AF(1);
    WDT();
    REG_W(0x70, 0xE000);
    /* sem o scramble e o subtom durante o conteudo (v75): o scramble (bit 1 do 0x31, secao 13) espelha o
     * audio, e o subtom (bit 15 do 0x51, que a fabrica liga com o nivel, 0x01E441BE) soma um tom grave
     * ao AFSK; os dois voltam antes do fim de fabrica */
    s31 = REG_R(0x31);
    s51 = REG_R(0x51);
    REG_W(0x31, s31 & ~2u);
    REG_W(0x51, s51 & ~0x8000u);
    if (e == NO_AR(ARMADO_MORSE))
        toca_morse();
    else                                   /* a mensagem, ou o ack do registro que o RX marcou */
        manda_aprs(e == NO_AR(ARMADO_ACK) ? HIST_SETOR(ACK_REGISTRO >> 5 & 1) + (ACK_REGISTRO & 31) * REG_HIST
                                            + ACK_PARA : 0);
    REG_W(0x51, s51);
    REG_W(0x31, s31);
    PEDE_AMPLI(10);
    TOM(0, 0);
    GERADOR(0);
    AF(0);
    r = ROGER;
    ROGER = 0;
    FIM_TX();
    ROGER = r;
}

/* ---------- MORSE ---------- */
const char titulo_morse_msg[] = "MORSE MESSAGE";
const char titulo_morse_envia[] = "MORSE SEND";
const char titulo_morse_wpm[] = "MORSE SPEED";
const char titulo_morse_tom[] = "MORSE TONE";
const char *const envia_opcoes[1] = {"OK TO SEND"};

u16 morse_msg_le(void)
{
    struct morse m;

    le_morse(&m);
    editor_abre(m.texto, m.tam, MAX_MSG);
    return 0;
}

/* Separada do set, como o grava_nome_do_editor do bancos.c: o registro sai da pilha antes da saida
 * do editor, que redesenha a tela inteira. */
__attribute__((noinline)) static void morse_guarda_texto(void)
{
    struct morse m;

    le_morse(&m);
    m.tam = editor_texto(m.texto, MAX_MSG);
    grava_morse(&m);
}

void morse_msg_grava(void)
{
    morse_guarda_texto();
    editor_fecha();
}

char *morse_msg_exibe(void)
{
    struct morse m;

    le_morse(&m);
    return resumo(m.texto, m.tam);
}

/* Velocidade: 5 a 40 palavras por minuto (a linha 0 e 5); tom: 400 a 1200 Hz de 50 em 50. */
u16 morse_wpm_le(void)
{
    struct morse m;

    le_morse(&m);
    return (m.wpm >= 5 && m.wpm <= 40 ? m.wpm : 15) - 5;
}

void morse_wpm_grava(void)
{
    struct morse m;

    le_morse(&m);
    m.wpm = VALOR <= 35 ? VALOR + 5 : 15;
    grava_morse(&m);
}

char *morse_wpm_exibe(void)
{
    SPRINTF(TEXTO, "%d WPM", VALOR + 5);
    return TEXTO;
}

u16 morse_tom_le(void)
{
    struct morse m;

    le_morse(&m);
    return ((m.tom >= 40 && m.tom <= 120 ? m.tom : 70) - 40) / 5;
}

void morse_tom_grava(void)
{
    struct morse m;

    le_morse(&m);
    m.tom = VALOR <= 16 ? 40 + VALOR * 5 : 70;
    grava_morse(&m);
}

char *morse_tom_exibe(void)
{
    SPRINTF(TEXTO, "%d Hz", 400 + VALOR * 50);
    return TEXTO;
}

u16 acao_le(void)
{
    return 0;
}

void morse_envia_grava(void)
{
    struct morse m;

    le_morse(&m);
    if (m.tam)
        ESTADO_MSG = ARMADO_MORSE;
}

/* ---------- APRS ---------- */
const char titulo_aprs_ind[] = "APRS CALLSIGN";
const char titulo_aprs_ssid[] = "APRS SSID";
const char titulo_aprs_dest[] = "APRS TO";
const char titulo_aprs_msg[] = "APRS MESSAGE";
const char titulo_aprs_envia[] = "APRS SEND";
const char titulo_aprs_caminho[] = "APRS PATH";
const char titulo_aprs_rx[] = "APRS RX";
const char *const aprs_caminho_opcoes[3] = {"WIDE1-1", "WIDE1-1,WIDE2-1", "NONE"};
const char *const aprs_rx_opcoes[1] = {"OK TO LISTEN"};

/* Os tres textos do APRS: o indicativo (6), o destinatario (9, com o SSID depois do '-', que a tecla
 * 0 do editor da) e a mensagem (37). */
static char *aprs_campo(struct aprs *a, unsigned qual, u8 **tam, unsigned *max)
{
    *max = qual == 0 ? 6 : qual == 1 ? 9 : MAX_MSG;
    *tam = qual == 0 ? &a->tam_ind : qual == 1 ? &a->tam_dest : &a->tam_msg;
    return qual == 0 ? a->ind : qual == 1 ? a->dest : a->msg;
}

static void aprs_texto_le(unsigned qual)
{
    struct aprs a;
    unsigned max;
    u8 *tam;
    char *t;

    le_aprs(&a);
    t = aprs_campo(&a, qual, &tam, &max);
    editor_abre(t, *tam, max);
}

/* Uma conta do texto, para saber se ele mudou. */
static unsigned resumo_texto(const char *t, unsigned n)
{
    unsigned h = n;

    while (n--)
        h = h * 31 + (u8)*t++;
    return h;
}

/* O texto do editor no campo. O indicativo e o destinatario vao em maiusculas (o editor tem o modo abc),
 * e so com o que o AX.25 e o APRS aceitam: letras e numeros, e o '-' do SSID no destinatario (v75; um
 * espaco ou uma '/' no endereco faziam decodificadores estritos descartarem o quadro). O numero da
 * mensagem anda quando o destinatario ou o texto mudam: o SEND repete o mesmo. */
__attribute__((noinline)) static void aprs_guarda_texto(unsigned qual)
{
    struct aprs a;
    unsigned max, i, k, n, c, antes;
    u8 *tam;
    char *t;

    le_aprs(&a);
    t = aprs_campo(&a, qual, &tam, &max);
    antes = resumo_texto(t, *tam);
    n = k = editor_texto(t, max);
    if (qual < 2)
        for (i = n = 0; i < k; i++) {
            c = t[i] >= 'a' && t[i] <= 'z' ? t[i] - 32 : t[i];
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '-' && qual == 1))
                t[n++] = c;
        }
    *tam = n;
    if (qual && resumo_texto(t, n) != antes)
        a.id = a.id % 99 + 1;
    grava_aprs(&a);
}

static char *aprs_texto_exibe(unsigned qual)
{
    struct aprs a;
    unsigned max;
    u8 *tam;
    char *t;

    le_aprs(&a);
    t = aprs_campo(&a, qual, &tam, &max);
    return resumo(t, *tam);
}

u16 aprs_ind_le(void)
{
    aprs_texto_le(0);
    return 0;
}

u16 aprs_dest_le(void)
{
    aprs_texto_le(1);
    return 0;
}

u16 aprs_msg_le(void)
{
    aprs_texto_le(2);
    return 0;
}

void aprs_ind_grava(void)
{
    aprs_guarda_texto(0);
    editor_fecha();
}

void aprs_dest_grava(void)
{
    aprs_guarda_texto(1);
    editor_fecha();
}

void aprs_msg_grava(void)
{
    aprs_guarda_texto(2);
    editor_fecha();
}

char *aprs_ind_exibe(void)
{
    return aprs_texto_exibe(0);
}

char *aprs_dest_exibe(void)
{
    return aprs_texto_exibe(1);
}

char *aprs_msg_exibe(void)
{
    return aprs_texto_exibe(2);
}

u16 aprs_ssid_le(void)
{
    struct aprs a;

    le_aprs(&a);
    return a.ssid < 16 ? a.ssid : 0;
}

void aprs_ssid_grava(void)
{
    struct aprs a;

    le_aprs(&a);
    a.ssid = VALOR < 16 ? VALOR : 0;
    grava_aprs(&a);
}

char *aprs_ssid_exibe(void)
{
    SPRINTF(TEXTO, VALOR ? "-%d" : "NONE", VALOR);
    return TEXTO;
}

u16 aprs_caminho_le(void)
{
    struct aprs a;

    le_aprs(&a);
    return a.caminho < 3 ? a.caminho : 0;
}

void aprs_caminho_grava(void)
{
    struct aprs a;

    le_aprs(&a);
    a.caminho = VALOR < 3 ? VALOR : 0;
    grava_aprs(&a);
}

/* ---------- APRS RX ----------
 * O modem FSK do chip em captura crua: FFSK a 1200 bps (reg 0x72 = 0x3065), sync de 2 bytes = duas flags
 * 0x7E em NRZI, sem o CRC do chip, quadro de 2048 bytes. Os bits de linha vao para o hdlc_bit (NRZI,
 * destuffing, FCS e a correcao aqui). A FIFO de RX tem 8 palavras (107 ms), e o laco a le de 4 em 4 a
 * cada aviso de quase cheia, olhando o pedido de interrupcao (reg 0x0C bit 0) a cada 2 ms.
 *
 * A polaridade (v71). Numa flag em NRZI um tom fica 7 bits parado e o outro aparece 1 bit, e qual e qual
 * depende do transmissor. O chip so sincroniza no padrao do 0x5A: com FE FE (parado na marca, 1200 Hz), um
 * WAV com as duas polaridades alternadas so fez sync nas paradas em 1200 Hz, e 95% do trafego real nem
 * mexia no contador (v70). Esperando, o laco troca a cada 50 ms entre FE FE e 01 01 (parado no espaco).
 * Sete bits de linha iguais so existem nas flags (o stuffing os corta dentro do quadro), entao nenhum dos
 * dois padroes aparece no meio de um quadro. Depois do sync o chip passa os bits crus do quadro dele
 * inteiro, de 2048 bytes (13,6 s), e o hdlc_bit acha sozinho as flags dos quadros seguintes, de qualquer
 * polaridade: o laco so rearma no fim desse quadro do chip, ou se a FIFO parar por 300 ms.
 *
 * Nenhum bit fica para tras (v74). O quadro bom vira texto e vai para o historico ali mesmo (guarda),
 * antes de o seguinte ocupar o q, e os bits seguem; a tela vai uma faixa de 32 linhas por volta do laco,
 * entre as leituras do modem (pede). Desde a v75 nada mais para o laco: as teclas sao lidas sem esperar
 * soltar (a que esta apertada so conta de novo depois de solta), o EXIT segurado e medido no proprio laco,
 * e o setor do historico que precisa ser apagado e apagado com a captura parada, fora dos bits.
 *
 * O modem e o 0x3CC3 (FFSK 1200/1800, o do roger TONE1200 de fabrica, o melhor na v71) com o ganho de
 * entrada da CALIBRATION > MODEM GAIN nos bits 9:8 (v73, ganho_modem, abaixo); a tecla # troca o ganho so
 * aqui, para comparar, e a proxima entrada volta ao da CALIBRATION. Na v70 a banda larga so dava sync sem
 * quadro e o ganho 0 nao ouvia nada; na v71 o 1200/2400 (0x33C3) decodificou menos que o 1200/1800, e tirar
 * o de-enfase do RX (bit 8 do 0x2B) nao mudou nada. Na v77, dez valores do 0x58 nunca testados (os bits
 * 7:6 em 0, as bandas 2 e 3, o caminho do NOAA SAME e os modos de RX 1-3 e 5-6, sem documentacao), na
 * tecla 0, nao decodificaram mais que este, e sairam na v78 (secao 24).
 *
 * A saida devolve o modem como a fabrica o deixa. O unico uso de fabrica dele e o roger TONE1200
 * (0x01E44676..0x01E44790), que programa 0x58 (0x3FC3, o ganho 3), 0x59, 0x5A..0x5D, 0x3F, 0x70 e 0x72 a
 * cada vez e no fim deixa o 0x58 em 0, o modem desligado: a saida grava 0 no 0x58 (o ganho de um modo de
 * teste nao fica no radio) e 0 no 0x59, e devolve o 0x3F, o 0x70, o 0x72 e o 0x5E (os limites da FIFO, que
 * a fabrica nunca grava, v73) como estavam na entrada.
 *
 * O laco e do set do item, com o menu aberto: o laco do sinal nao roda, entao o receptor e acordado da
 * economia de bateria e a dupla escuta desarmada antes (fica no lado em evidencia), e o watchdog e
 * limpo aqui. A tela mostra dois quadros por vez, o mais novo em cima, o de cima (o da posicao no alto)
 * num fundo proprio; as setas andam pelo historico (v75: segurada, a seta repete, e o historico da a
 * volta nas duas pontas), o OK abre o de cima inteiro (e fecha), o # troca o ganho, o * apaga a tela
 * (v75; a tecla seguinte so a acende), o EXIT segurado apaga o historico (v71), o EXIT curto fecha o
 * aberto ou sai, e as outras teclas saem. A mensagem para mim com numero sai da tela
 * sozinha, para o ack ir ao ar (v75, guarda). Na barra de baixo, quantos syncs o modem achou em cada
 * polaridade, quantos quadros chegaram com o FCS errado (BAD) e quantos a correcao salvou (FIX), desde a
 * entrada ou a ultima troca de ganho. */
/* A luz da tela (0x01E394D6, acende = 1 ou apaga = 0): com o PWM ligado, o desvio da v60 (b_luz, tela.c) a
 * atende pelo ciclo; sem ele, a de fabrica. */
FIRMWARE(LUZ, 0x01E394D6);          void LUZ(unsigned);

/* A tela (telas.c): os registros do historico na flash, o de cima e o de baixo (0 = vazio); aberto, so o de
 * cima, inteiro; os numeros (o ganho, a posicao no historico, quantos ha, syncs na marca e no espaco, BAD,
 * FIX); e as faixas de 32 linhas a desenhar (bit 0 a de cima). */
void tela_aprs(u32 a0, u32 a1, unsigned aberto, const u16 *num, unsigned faixas);

/* O historico (v75): os setores 0xD6000 e 0xD7000, cada um com um cabecalho ("HST3" e a geracao, um byte
 * que anda a cada setor novo) e 31 registros de 128 B. Um anel: quando o setor que recebe enche, o outro
 * (o dos mais velhos) e apagado e recomeca com a geracao seguinte, e os 31 do cheio passam a ser os
 * velhos. Ficam sempre os ultimos 31 a 62. O estado: hist (quantos ha; -1 se um setor tem outra coisa, e
 * ai nada e gravado) e anel (o setor que recebe no bit 0, se o outro precisa ser apagado no bit 1, e
 * quantos velhos ha nele dos bits 2 em diante). Da v70 a v74 o historico era de um bloco so ("HST1",
 * registros de 64 B; "HST2", de 128 B pelos dois setores seguidos, um cabecalho so): ele e apagado na
 * primeira entrada. */
#define MARCA_HIST   0x33545348u           /* "HST3" */
#define MARCA_V74    0x32545348u           /* "HST2" */
#define MARCA_V70    0x31545348u           /* "HST1" */
#define POR_SETOR    31u
#define USADO        0x5A
#define ANEL_SUJO    2u                    /* o setor que nao recebe tem cabecalho: precisa ser apagado */
#define ANEL_VELHOS(a)  ((a) >> 2)
#define DUPLICADO_MS 30000u                /* a mesma estacao com o mesmo texto: so a primeira entra */
#define TECLA_OK     2                     /* os codigos crus da varredura (a tabela 0x01E60369) */
#define TECLA_CIMA   3
#define TECLA_BAIXO  4
#define TECLA_ESTRELA 9
#define TECLA_MODO   17                    /* # */
#define EXIT_LONGO   100                   /* o EXIT segurado 0,6 s */
#define ABERTO       1u                    /* os bits de modo, no laco */
#define PRESO        2u                    /* uma tecla esta apertada: so conta de novo depois de solta */
#define ESPERA_EXIT  4u                    /* o EXIT apertado: curto ou segurado, so se sabe depois */
#define ESCURO       8u                    /* a tela apagada pelo * */
#define REPETE       16u                   /* uma seta apertada: segurada, ela repete */
#define REPETE_DEPOIS 800u                 /* a seta segurada comeca a repetir em 0,8 s, a cada 0,15 s */
#define REPETE_CADA  150u

/* O registro: o "usado" e gravado por ultimo, e um registro com o ambar ainda apagado e um lugar vazio;
 * uma queda de energia no meio deixa um lugar ocupado e sem o "usado", que a tela pula. Na mensagem para
 * mim com numero, a origem (em 9 colunas, como o ack a quer) e o numero ficam para o ack. */
struct registro_rx {
    u8 usado, ambar;
    char texto[TEXTO_MAX + 1];
    char para[9], id[6];
    u8 sobra[2];
    u32 hora;                              /* TIQUE_MS da chegada, para as copias */
};

typedef char confere_registro[sizeof(struct registro_rx) == REG_HIST ? 1 : -1];
typedef char confere_ack[__builtin_offsetof(struct registro_rx, para) == ACK_PARA ? 1 : -1];

#define MODEM_RX     0x3CC3                /* FFSK 1200/1800, sem o ganho (bits 9:8) */

/* APRS DECODE (v76, a pasta APRS): ON mostra os formatos decodificados (Mic-E, posicao comprimida, tempo,
 * objeto, rumo e velocidade, altitude); OFF, a info como veio, para conferir. No 0x903F dos ajustes
 * (b[0x14BEB], livre pela varredura da v59): OFF so com 0xB1 (o RESET de fabrica deixa 229 nele, que vale
 * ON). Vale para os quadros que chegam depois de mudar: o historico guarda o texto pronto. */
#define DECODE_APRS  RAM(u8, 0x00014BEB)
#define DECODE_OFF   0xB1
const char titulo_aprs_decode[] = "APRS DECODE";
const char *const aprs_decode_opcoes[2] = {"ON", "OFF"};

u16 aprs_decode_le(void)
{
    return DECODE_APRS == DECODE_OFF;
}

void aprs_decode_grava(void)
{
    DECODE_APRS = VALOR ? DECODE_OFF : 0xB0;
}

/* MODEM GAIN (v73, a pasta CALIBRATION): o ganho de entrada do modem FSK do chip, os bits 9:8 do 0x58, que
 * so o APRS RX usa na recepcao (o roger TONE1200 de fabrica so transmite, e a voz nao passa pelo modem).
 * Gravado no 0x903D dos ajustes (b[0x14BE9], livre pela varredura da v59) como 0xA0 + o ganho: qualquer
 * outro valor vale HIGH, o 3 de fabrica - o do radio que nunca mudou o item, e o do RESET de fabrica, que
 * poe no byte o que vier depois da tabela 0x01E603C3 (hoje 198). */
#define GANHO_MODEM  RAM(u8, 0x00014BE9)
#define GANHO_MARCA  0xA0
const char titulo_modem_ganho[] = "MODEM GAIN";
const char *const modem_ganho_opcoes[4] = {"OFF", "LOW", "MEDIUM", "HIGH"};

static unsigned ganho_modem(void)
{
    return (GANHO_MODEM & 0xFC) == GANHO_MARCA ? GANHO_MODEM & 3 : 3;
}

u16 modem_ganho_le(void)
{
    return ganho_modem();
}

void modem_ganho_grava(void)
{
    GANHO_MODEM = GANHO_MARCA | (VALOR < 4 ? VALOR : 3);
}

/* Arma a captura com o sync de uma polaridade: 0 parado na marca (FE FE), 1 no espaco (01 01). */
static void fsk_rx_arma(unsigned pol)
{
    unsigned s = pol ? 0x0101 : 0xFEFE;

    REG_W(0x5A, s);
    REG_W(0x5B, s);
    REG_W(0x59, 0x4000);                   /* limpa a FIFO de RX, sync de 2 bytes */
    REG_W(0x59, 0x1000);                   /* liga a recepcao */
}

/* Um setor do historico comeca: o cabecalho da geracao g (com o setor ja apagado). Colada em quem chama:
 * no caminho da gravacao, um quadro a mais passaria da pilha. */
static inline __attribute__((always_inline)) void setor_novo(unsigned s, unsigned g)
{
    u32 m = MARCA_HIST;
    u8 b = g;

    GRAVA_FLASH(HIST_SETOR(s), &m, 4);
    GRAVA_FLASH(HIST_SETOR(s) + 4, &b, 1);
}

/* Quantos registros o setor tem: os lugares com o ambar gravado, do primeiro em diante. */
static unsigned conta_setor(unsigned s)
{
    unsigned n;
    u8 b;

    for (n = 0; n < POR_SETOR; n++) {
        LE_FLASH(HIST_SETOR(s) + (n + 1) * REG_HIST + 1, &b, 1);
        if (b == 0xFF)
            break;
    }
    return n;
}

/* Apaga o historico (o EXIT segurado na tela, ou o formato velho): os dois setores, e o primeiro recomeca. */
static int hist_limpa(unsigned *anel)
{
    APAGA_SETOR(HIST_SETOR(0));
    APAGA_SETOR(HIST_SETOR(1));
    setor_novo(0, 0);
    *anel = 0;
    return 0;
}

/* Abre o historico: os dois cabecalhos dizem qual setor recebe (a geracao mais nova) e se ha velhos no
 * outro. So usa setor apagado ou ja nosso: com outra coisa num deles, devolve -1 e nada e gravado. */
static int hist_abre(unsigned *anel)
{
    u32 m[2];
    u8 g[2];
    unsigned s, cur, velhos;

    for (s = 0; s < 2; s++) {
        LE_FLASH(HIST_SETOR(s), &m[s], 4);
        LE_FLASH(HIST_SETOR(s) + 4, &g[s], 1);
    }
    if (m[0] == MARCA_V74 || (m[0] == MARCA_V70 && (m[1] == 0xFFFFFFFFu || m[1] == MARCA_HIST)))
        return hist_limpa(anel);           /* o da v70 nao usava o 0xD7000: so com ele livre ou nosso */
    for (s = 0; s < 2; s++)
        if (m[s] != MARCA_HIST && m[s] != 0xFFFFFFFFu)
            return -1;
    if (m[0] != MARCA_HIST && m[1] != MARCA_HIST) {
        setor_novo(0, 0);
        *anel = 0;
        return 0;
    }
    cur = m[0] != MARCA_HIST ? 1 : m[1] != MARCA_HIST ? 0 : (signed char)(g[1] - g[0]) > 0;
    velhos = m[!cur] == MARCA_HIST ? conta_setor(!cur) : 0;
    *anel = cur | (m[!cur] == MARCA_HIST ? ANEL_SUJO : 0) | velhos << 2;
    return velhos + conta_setor(cur);
}

/* O setor que recebe encheu, e o outro ainda tem os velhos: apaga-o ja, com a captura parada (o laco o
 * chama fora dos bits), e os velhos saem. */
static int hist_prepara(int hist, unsigned *anel)
{
    APAGA_SETOR(HIST_SETOR((*anel & 1) ^ 1));
    hist -= ANEL_VELHOS(*anel);
    *anel &= 1;
    return hist;
}

/* Grava o registro no setor que recebe; cheio, o outro recomeca (apagado aqui so se o laco ainda nao o
 * apagou) e os deste viram os velhos. Devolve o numero do registro para o ack (o setor * 32 + o lugar),
 * ou 0 se o historico nao e nosso. */
static unsigned hist_grava(int *hist, unsigned *anel, const struct registro_rx *r)
{
    unsigned a = *anel, cur = a & 1, velhos = ANEL_VELHOS(a), n = *hist - velhos;
    u32 e;
    u8 g;

    if (*hist < 0)
        return 0;
    if (n >= POR_SETOR) {
        LE_FLASH(HIST_SETOR(cur) + 4, &g, 1);
        if (a & ANEL_SUJO)
            APAGA_SETOR(HIST_SETOR(cur ^ 1));
        cur ^= 1;
        setor_novo(cur, g + 1);
        velhos = n;
        n = 0;
        a = cur | ANEL_SUJO | velhos << 2;
    }
    e = HIST_SETOR(cur) + (n + 1) * REG_HIST;
    GRAVA_FLASH(e + 1, (const u8 *)r + 1, REG_HIST - 1);
    GRAVA_FLASH(e, r, 1);                  /* o "usado" por ultimo */
    *anel = a;
    *hist = velhos + n + 1;
    return cur * 32 + n + 1;
}

/* Onde esta o registro i do historico (0 o mais velho), ou 0 se nao ha. */
static u32 hist_endereco(int hist, unsigned anel, int i)
{
    unsigned velhos = ANEL_VELHOS(anel);

    if (i < 0 || i >= hist)
        return 0;
    if ((unsigned)i < velhos)
        return HIST_SETOR((anel & 1) ^ 1) + (i + 1) * REG_HIST;
    return HIST_SETOR(anel & 1) + (i - velhos + 1) * REG_HIST;
}

/* O registro da flash em e e o mesmo r (o texto, a origem e o numero), chegado ha menos de 30 s?
 * Separada: o buffer dela nao soma com o registro do guarda no caminho da gravacao. */
__attribute__((noinline)) static unsigned registro_igual(u32 e, const struct registro_rx *r)
{
    u8 b[12];
    unsigned k, j;

    LE_FLASH(e + REG_HIST - 4, b, 4);
    if (r->hora - (b[0] | b[1] << 8 | b[2] << 16 | (u32)b[3] << 24) > DUPLICADO_MS)
        return 0;
    for (k = 0; k < REG_HIST - 4; k += 12) {
        LE_FLASH(e + k, b, 12);
        for (j = 0; j < 12 && k + j < REG_HIST - 4; j++)   /* sem a hora */
            if (b[j] != ((const u8 *)r)[k + j])
                return 0;
    }
    return 1;
}

/* O registro em montagem, na memoria de video (0x10190, a faixa de 160x32 da tela): o guarda roda entre
 * uma faixa e outra, e o firmware de fabrica ja a empresta como rascunho ao regravar canal. Na pilha, os
 * 128 B dele mais a decodificacao dos formatos (v76) passavam do limite do laco (secao 24). A tela e
 * redesenhada inteira depois de um quadro novo. */
#define REGISTRO_RASCUNHO  ((struct registro_rx *)&RAM(u8, 0x00010190))

/* Um quadro que acabou de chegar, em texto, no historico. A copia do que ja entrou nos ultimos 30 s (a
 * mesma estacao ouvida direto e pelos digipeaters) nao entra de novo. A mensagem para mim com numero arma
 * o ack (v75): o numero do registro em ACK_REGISTRO, e o laco sai da tela para ele ir ao ar - a cada
 * copia, porque o ack anterior pode nao ter chegado. E chamada no meio dos bits, antes de o quadro
 * seguinte ocupar o q. */
__attribute__((noinline)) static void guarda(int *hist, unsigned *anel, const u8 *q, unsigned n,
                                             const char *meu, unsigned n_meu)
{
    struct registro_rx *r = REGISTRO_RASCUNHO;
    unsigned i, k = 0;
    u32 e;

    for (i = 0; i < REG_HIST; i++)
        ((u8 *)r)[i] = 0xFF;
    r->usado = USADO;
    r->ambar = aprs_texto(r->texto, q, n, meu, n_meu | (DECODE_APRS == DECODE_OFF ? TEXTO_BRUTO : 0), r->id);
    for (i = 0; i < 9; i++) {              /* a origem, do comeco do texto ate o "> " */
        if (r->texto[i] == '>')
            k = 1;
        r->para[i] = k || !r->ambar ? ' ' : r->texto[i];
    }
    r->hora = TIQUE_MS;
    for (i = 1, k = 0; i <= 8 && !k && (e = hist_endereco(*hist, *anel, *hist - i)); i++)
        if (registro_igual(e, r))
            k = (e - HIST_SETOR(0)) >> 12 << 5 | (e & 0xFFF) >> 7;
    if (!k)
        k = hist_grava(hist, anel, r);
    if (k && r->ambar && r->id[0]) {
        ACK_REGISTRO = k;
        ESTADO_MSG = ARMADO_ACK;
    }
}

/* Direto na tela, sem rotina no meio: o quadro dela pesaria na pilha do laco. */
#define RX_MOSTRA(faixas) \
    (num[1] = topo + 1, num[2] = hist < 0 ? 0 : hist, num[5] = h.ruins, num[6] = h.consertados, \
     tela_aprs(hist_endereco(hist, anel, hist - 1 - topo), hist_endereco(hist, anel, hist - 2 - topo), \
               modo & ABERTO, num, (faixas)))

/* O meu indicativo em texto ("PY2ABC-7", em maiusculas: o editor tem o modo abc); devolve o tamanho.
 * Separada: o registro sai da pilha antes do laco. */
__attribute__((noinline)) static unsigned meu_indicativo(char *meu)
{
    struct aprs a;
    unsigned n;

    le_aprs(&a);
    for (n = 0; n < a.tam_ind && n < 6; n++)
        meu[n] = a.ind[n] >= 'a' && a.ind[n] <= 'z' ? a.ind[n] - 32 : a.ind[n];
    if (a.ssid && a.ssid < 16) {
        meu[n++] = '-';
        n = decimal(meu + n, a.ssid) - meu;
    }
    return n;
}

void aprs_rx_grava(void)
{
    u8 q[136];                             /* o comeco do quadro: os enderecos e ~100 de info, o que a tela
                                            * mostra (160 na v74; 136 pela pilha do historico, v75) */
    struct hdlc h;
    char meu[10];
    u16 num[7];                            /* ganho, posicao, quantos, syncs na marca e no espaco, BAD, FIX */
    unsigned i, f, n, k, w = 0, n_meu = meu_indicativo(meu), ativo = 0, pol = 0, desde = TIQUE_MS;
    unsigned pede = 15, modo = PRESO, anel = 0, desde_tecla = 0;   /* o OK que abriu ainda esta apertado */
    /* os registradores (de 16 bits) que a saida devolve, dois por variavel: a pilha do laco esta no limite */
    unsigned era1 = REG_R(0x3F) | REG_R(0x70) << 16, era2 = REG_R(0x72) | REG_R(0x5E) << 16;
    int hist = hist_abre(&anel), topo = 0;

    ACORDA();
    DESARMA_DW();
    REG_W(0x70, 0x00E0);                   /* o FSK ligado, com o ganho do roger TONE1200 */
    REG_W(0x72, 0x3065);                   /* 1200 bps */
    num[0] = ganho_modem();
    REG_W(0x58, MODEM_RX | num[0] << 8);
    REG_W(0x5C, 0xAA30);                   /* sem o CRC do chip */
    REG_W(0x5D, 0xFFE0);                   /* 2048 bytes */
    REG_W(0x5E, 0x0204);                   /* quase cheia com 4 palavras */
    REG_W(0x3F, 0x3002);                   /* avisos: sync, quase cheia, fim */
    REG_W(0x02, 0);
    fsk_rx_arma(0);
    hdlc_comeca(&h, q, sizeof q);
    h.ruins = h.consertados = 0;
    num[3] = num[4] = 0;
    for (;;) {
        WDT();
        /* as teclas, sem esperar soltar: a apertada so conta de novo depois de solta; o EXIT, so ao soltar
         * (curto) ou aos 0,6 s (segurado); a seta segurada repete; com a tela apagada, a primeira so a
         * acende */
        k = tecla();
        f = 0;                             /* a acao da tecla */
        if (k == 255) {
            if (modo & ESPERA_EXIT)
                f = TECLA_EXIT;
            modo &= ~(PRESO | ESPERA_EXIT | REPETE);
        } else if (!(modo & PRESO)) {
            modo |= PRESO;
            desde_tecla = TIQUE_MS;
            if (modo & ESCURO) {
                LUZ(1);
                modo &= ~ESCURO;
                pede = 15;
            } else if (k == TECLA_EXIT) {
                modo |= ESPERA_EXIT;
            } else {
                f = k;
                if (k == TECLA_CIMA || k == TECLA_BAIXO)
                    modo |= REPETE;
            }
        } else if ((modo & ESPERA_EXIT) && TIQUE_MS - desde_tecla >= 600) {
            modo &= ~ESPERA_EXIT;
            f = EXIT_LONGO;
        } else if ((modo & REPETE) && TIQUE_MS - desde_tecla >= REPETE_DEPOIS) {
            desde_tecla = TIQUE_MS - REPETE_DEPOIS + REPETE_CADA;
            f = k;
        }
        if (f) {
            if (f == TECLA_MODO) {
                num[0] = (num[0] + 1) & 3;
                REG_W(0x58, MODEM_RX | num[0] << 8);
                ativo = 0;
                fsk_rx_arma(pol);
                desde = TIQUE_MS;
                num[3] = num[4] = h.ruins = h.consertados = 0;
            } else if (f == TECLA_CIMA || f == TECLA_BAIXO) {
                if (hist > 0)              /* em volta: depois do mais velho, o mais novo, e o contrario */
                    topo = (topo + (f == TECLA_BAIXO ? 1 : hist - 1)) % hist;
            } else if (f == TECLA_OK) {
                if ((modo & ABERTO) || hist > 0)
                    modo ^= ABERTO;
            } else if (f == TECLA_ESTRELA) {
                LUZ(0);
                modo |= ESCURO;
            } else if (f == EXIT_LONGO) {  /* segurado: apaga o historico e fica */
                if (hist >= 0)
                    hist = hist_limpa(&anel);
                topo = 0;
                modo &= ~ABERTO;
            } else if (f == TECLA_EXIT && (modo & ABERTO)) {
                modo &= ~ABERTO;
            } else {
                break;                     /* o EXIT curto e as outras teclas saem */
            }
            pede = 15;
            continue;
        }
        ESPERA_2MS(1);
        if (!(REG_R(0x0C) & 1)) {
            if (TIQUE_MS - desde > (ativo ? 300u : 50u)) {
                /* esperando, a outra polaridade a cada 50 ms; recebendo, a FIFO parou: volta a esperar */
                pol ^= !ativo;
                ativo = 0;
                fsk_rx_arma(pol);
                desde = TIQUE_MS;
            } else if (!ativo && (anel & ANEL_SUJO) && hist - (int)ANEL_VELHOS(anel) >= (int)POR_SETOR) {
                hist = hist_prepara(hist, &anel);   /* o setor encheu: o outro se apaga ja, fora dos bits */
                topo = 0;
                modo &= ~ABERTO;
                pede = 15;
            } else if (pede && !(modo & ESCURO)) {
                k = pede & -pede;          /* uma faixa da tela por volta, entre as leituras do modem */
                pede ^= k;
                RX_MOSTRA(k);
            }
            continue;
        }
        REG_W(0x02, 0);
        f = REG_R(0x02);
        if (f & 0x0002) {                  /* sync: as flags ja passaram, o quadro vem */
            hdlc_comeca(&h, q, sizeof q);
            h.dentro = 1;
            h.linha = pol ^ (REG_R(0x0B) >> 7 & 1);     /* a flag acaba no tom do bit sozinho */
            ativo = 1;
            desde = TIQUE_MS;
            num[3 + pol]++;
            pede |= 8;
        }
        i = h.ruins + h.consertados;
        if (f & 0x1000) {
            desde = TIQUE_MS;
            /* as 4 palavras bit a bit: o byte baixo primeiro, o bit alto de cada byte primeiro (a ordem do
             * TX do chip, que o TA1JS usou no UV-K5 com o aprs.fi decodificando, e que o beacon da v65
             * provou) */
            for (k = 0; k < 64; k++) {
                if (!(k & 15))
                    w = REG_R(0x5F);
                if ((n = hdlc_bit(&h, w >> (k & 8 ? 23 - (k & 15) : 7 - (k & 15)) & 1))) {
                    int antes = hist;

                    guarda(&hist, &anel, q, n, meu, n_meu);
                    if (hist == antes)     /* uma copia: nada muda na tela */
                        continue;
                    if ((modo & ABERTO) && hist == antes + 1) {
                        topo++;            /* o aberto continua o mesmo */
                    } else {               /* senao, o novo vai para cima */
                        topo = 0;
                        modo &= ~ABERTO;
                    }
                    pede = 15;
                }
            }
        }
        if (f & 0x2000) {                  /* o quadro do chip acabou: volta a esperar um sync */
            ativo = 0;
            fsk_rx_arma(pol);
            desde = TIQUE_MS;
        }
        if (ESTADO_MSG == ARMADO_ACK)      /* uma mensagem para mim: sai, e o ack vai ao ar (v75) */
            break;
        if (i != h.ruins + h.consertados)
            pede |= 8;
    }
    espera_soltar();
    if (modo & ESCURO)
        LUZ(1);
    REG_W(0x59, 0);
    REG_W(0x58, 0);                        /* desligado, como o roger de fabrica deixa (0x01E44788) */
    REG_W(0x5E, era2 >> 16);
    REG_W(0x3F, era1 & 0xFFFF);
    REG_W(0x70, era1 >> 16);
    REG_W(0x72, era2 & 0xFFFF);
    REG_W(0x02, 0);
}

/* Arma o envio; a mensagem leva um numero (1 a 99) para o outro lado confirmar ("ack"). O numero so anda
 * quando o destinatario ou o texto mudam (v75, aprs_guarda_texto): mandar de novo a mesma mensagem e
 * repeti-la, e o outro lado a confirma sem a mostrar duas vezes. */
void aprs_envia_grava(void)
{
    struct aprs a;

    le_aprs(&a);
    if (!a.tam_ind || !a.tam_msg)
        return;
    if (a.tam_dest && (!a.id || a.id > 99)) {
        a.id = 1;
        grava_aprs(&a);
    }
    ESTADO_MSG = ARMADO_APRS;
}
#endif
