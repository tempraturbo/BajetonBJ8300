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
 * e confere o pacote, o AFSK e o decodificador.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

#ifdef TESTE_PC
#include "teste_mensagem.h"           /* REG_W, TOM, agora_us e companhia, imitados */
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

static char *decimal(char *p, unsigned v)
{
    if (v >= 10)
        p = decimal(p, v / 10);
    *p++ = '0' + v % 10;
    return p;
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

/* "DDMM.mm" + a letra, de uma posicao sem compressao, em "DD MM.mmL"; devolve o fim, ou 0 se nao for
 * uma posicao. graus: 2 na latitude, 3 na longitude. */
static char *aprs_coordenada(char *s, const u8 *p, unsigned graus)
{
    unsigned k;

    for (k = 0; k < graus + 5; k++)
        if (k == graus + 2 ? p[k] != '.' : (p[k] < '0' || p[k] > '9') && p[k] != ' ')
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

#define TEXTO_MAX  120                  /* o quadro em texto, na tela e no historico (v74) */

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

/* As letras de U+00C0 a U+00FF (os bytes 0xC3 0x80..0xBF do UTF-8) sem o acento: a fonte do radio nao as
 * tem, e cada byte virava um ponto (v74). */
static const char SEM_ACENTO[65] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";

/* O quadro em texto legivel, ate TEXTO_MAX caracteres (48 ate a v73: o que cabia em duas linhas; a tela
 * abre o resto no OK), terminado em 0:
 *  - mensagem para o meu indicativo: "ORIGEM> texto" (sem o "{id", que e so da confirmacao), e devolve 1;
 *  - mensagem para outro: "ORIGEM>DESTINO: texto"; a confirmacao: "ORIGEM>DESTINO: ACK id";
 *  - posicao sem compressao (! = / @): "ORIGEM: 23 18.07S 046 35.59W DIGI comentario";
 *  - o resto: "ORIGEM: info", como veio.
 * Num quadro de terceiros (v71: "}" no comeco da info, o que um igate traz da internet), ORIGEM e a
 * estacao de dentro, e a info e a dela. */
unsigned aprs_texto(char *s, const u8 *q, unsigned n, const char *meu, unsigned n_meu)
{
    char *p = s, *f;
    const u8 *x;
    unsigned i = 7, k, para_mim = 0;

    while (i + 7 <= n && !(q[i + 6] & 1))  /* o fim dos enderecos: o bit 0 do ultimo */
        i += 7;
    i += 7 + 2;                            /* controle e PID */
    for (k = i + 1; i < n && q[i] == '}' && k < n && k < i + 11 && q[k] != '>'; k++)
        ;
    if (i < n && q[i] == '}' && k < n && q[k] == '>') {
        /* de terceiros (v71), o que um igate traz da internet, "}ORIGEM>DESTINO,CAMINHO:info": a
         * estacao e a de dentro, e o resto segue pela info dela */
        while (++i < k)
            *p++ = visivel(q[i]);
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
        if (i < n && (q[i] == '/' || q[i] == '@'))
            x += 7;                        /* com a hora: DDHHMMz */
        if (i < n && (q[i] == '!' || q[i] == '=' || q[i] == '/' || q[i] == '@') && x + 19 <= q + n
            && (f = aprs_coordenada(p, x, 2))) {
            *f++ = ' ';
            if ((f = aprs_coordenada(f, x + 9, 3))) {
                const char *t = x[8] == '/' ? SIMBOLOS : "";   /* a lista e da tabela primaria (v74) */
                p = f;
                while (*t && *t != x[18])  /* o simbolo pelo nome, se e da lista */
                    while (*t++)
                        ;
                *p++ = ' ';
                if (*t)
                    p = copia(p, t + 1);
                else
                    *p++ = visivel(x[18]);
                *p++ = ' ';
                i = x + 19 - q;
            }
        }
    }
    for (; i < n && p < s + TEXTO_MAX && !(para_mim && q[i] == '{'); i++) {
        k = q[i];
        if ((k == 0xC2 || k == 0xC3) && i + 1 < n && q[i + 1] >= 0x80 && q[i + 1] < 0xC0) {
            i++;
            if (k == 0xC3)
                k = SEM_ACENTO[q[i] - 0x80];
            else                           /* U+00B0 e U+00BA (graus, ordinal) e U+00AA (v74) */
                k = q[i] == 0xB0 || q[i] == 0xBA ? 'o' : q[i] == 0xAA ? 'a' : '.';
        }
        *p++ = visivel(k);
    }
    *p = 0;
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
#define TEXTO          ((char *)0x000129B0)       /* 28 B: o texto que o exibe devolve */

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
#define EDITOR_TEXTO   ((volatile char *)0x00014A3A)
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
#define NO_AR(e)       ((e) | 0x20)      /* 'm', 'a': a TX comecou, o conteudo vai no fim dela */

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

/* O EXIT continua apertado por ~0,6 s? (a tela do APRS RX, v71). Separada: dentro do laco do RX, os
 * registros dela pesariam na pilha dele. */
__attribute__((noinline)) static unsigned segurou_exit(void)
{
    unsigned i;

    for (i = 0; i < 300 && tecla() == TECLA_EXIT; i++) {
        WDT();
        ESPERA_2MS(1);
    }
    return i == 300;
}

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

    if (e == NO_AR(ARMADO_MORSE) || e == NO_AR(ARMADO_APRS)) {
        if (!PTT_SOLTO() || !TX_ESTADO)    /* o PTT tomou a TX (ela e de voz), ou ela foi recusada */
            ESTADO_MSG = 0;
    } else if ((e == ARMADO_MORSE || e == ARMADO_APRS) && !BIPE) {
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
 * ou, sem destinatario, o status ">texto". O campo de informacao e montado no proprio quadro, no
 * lugar onde o ax_quadro o poria. O id da mensagem anda no SEND (aprs_envia_grava). */
#define CAB(digis)  (16 + 7 * (digis))

static unsigned quadro_aprs(u8 *q)
{
    struct aprs a;
    char origem[10];
    unsigned i, n, digis;
    u8 *p;

    le_aprs(&a);
    if (!a.tam_ind || !a.tam_msg)
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

static void toca_aprs(void)
{
    u8 q[CAB(2) + 1 + 9 + 1 + MAX_MSG + 3 + 2];
    unsigned n = quadro_aprs(q);

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
    unsigned e = ESTADO_MSG, r;

    if (e != NO_AR(ARMADO_MORSE) && e != NO_AR(ARMADO_APRS)) {
        FIM_TX();
        return;
    }
    ESTADO_MSG = 0;
    AUDIO_TX(4);                           /* o microfone cortado ja, antes das esperas (v74): a portadora
                                            * muda levava o som do ambiente ao ar */
    espera_soltar();
    if (e == NO_AR(ARMADO_APRS))           /* v69: 0,5 s de portadora muda antes do pacote, para o */
        ESPERA_2MS(250);                   /* receptor (economia de bateria, squelch lento) abrir */
    GERADOR(1);
    AF(1);
    WDT();
    REG_W(0x70, 0xE000);
    if (e == NO_AR(ARMADO_MORSE))
        toca_morse();
    else
        toca_aprs();
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

__attribute__((noinline)) static void aprs_guarda_texto(unsigned qual)
{
    struct aprs a;
    unsigned max;
    u8 *tam;
    char *t;

    le_aprs(&a);
    t = aprs_campo(&a, qual, &tam, &max);
    *tam = editor_texto(t, max);
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
 * Nenhum bit fica para tras (v74). Da v71 a v73, o quadro bom parava a leitura no meio do bloco de 4
 * palavras: ate 63 bits depois dele iam fora, e um igate ou um digipeater que manda varios quadros na mesma
 * transmissao, separados por uma a tres flags, perdia o comeco do seguinte. Agora o quadro bom vira texto e
 * vai para o historico ali mesmo (guarda), antes de o seguinte ocupar o q, e os bits seguem. A tela, que
 * levava 4 faixas de uma vez depois de cada quadro, vai uma faixa de 32 linhas por volta do laco, entre as
 * leituras do modem (pede): a FIFO nunca espera mais que uma faixa.
 *
 * O modem e o 0x3CC3 (FFSK 1200/1800, o do roger TONE1200 de fabrica, o melhor na v71) com o ganho de
 * entrada da CALIBRATION > MODEM GAIN nos bits 9:8 (v73, ganho_modem, abaixo); a tecla # troca o ganho so
 * aqui, para comparar, e a proxima entrada volta ao da CALIBRATION. Na v70 a banda larga so dava sync sem
 * quadro e o ganho 0 nao ouvia nada; na v71 o 1200/2400 (0x33C3) decodificou menos que o 1200/1800, e tirar
 * o de-enfase do RX (bit 8 do 0x2B) nao mudou nada.
 *
 * A saida devolve o modem como a fabrica o deixa. O unico uso de fabrica dele e o roger TONE1200
 * (0x01E44676..0x01E44790), que programa 0x58 (0x3FC3, o ganho 3), 0x59, 0x5A..0x5D, 0x3F, 0x70 e 0x72 a
 * cada vez e no fim deixa o 0x58 em 0, o modem desligado: a saida grava 0 no 0x58 (o ganho de um modo de
 * teste nao fica no radio) e 0 no 0x59, e devolve o 0x3F, o 0x70, o 0x72 e o 0x5E (os limites da FIFO, que
 * a fabrica nunca grava, v73) como estavam na entrada.
 *
 * O laco e do set do item, com o menu aberto: o laco do sinal nao roda, entao o receptor e acordado da
 * economia de bateria e a dupla escuta desarmada antes (fica no lado em evidencia), e o watchdog e
 * limpo aqui. Os quadros vao para o historico na flash, e a tela os le de la (v74): os setores 0xD6000 e
 * 0xD7000 ("HST2", ate 63 registros de 128 B, o texto inteiro e a cor), gravados sem apagar ate encher;
 * cheio, os dois sao apagados e recomeca. A tela mostra dois por vez, o mais novo em cima, o de cima
 * (o da posicao no alto) num fundo proprio; as setas andam pelo historico, o OK abre o de cima inteiro (e
 * fecha), o # troca o ganho, o EXIT segurado apaga o historico (v71), o EXIT curto fecha o aberto ou sai, e
 * as outras teclas saem. Na barra de baixo, quantos syncs o modem achou em cada polaridade, quantos quadros
 * chegaram com o FCS errado (BAD) e quantos a correcao salvou (FIX), desde a entrada ou a ultima troca de
 * ganho. */
FIRMWARE(LE_FLASH, 0x01E390AC);     void LE_FLASH(u32, void *, u32);
FIRMWARE(APAGA_SETOR, 0x01E3A126);  void APAGA_SETOR(u32);
FIRMWARE(GRAVA_FLASH, 0x01E3A25A);  void GRAVA_FLASH(u32, const void *, u32);

/* A tela (telas.c): os registros do historico na flash, o de cima e o de baixo (0 = vazio); aberto, so o de
 * cima, inteiro; os numeros (o ganho, a posicao no historico, quantos ha, syncs na marca e no espaco, BAD,
 * FIX); e as faixas de 32 linhas a desenhar (bit 0 a de cima). */
void tela_aprs(u32 a0, u32 a1, unsigned aberto, const u16 *num, unsigned faixas);

#define SETOR_HIST   0x000D6000u           /* e o 0xD7000 (v74): o mapa do chip de dados os da livres */
#define MARCA_HIST   0x32545348u           /* "HST2" (v74) */
#define MARCA_V70    0x31545348u           /* "HST1", o das v70 a v73, com registros de 64 B */
#define REG_HIST     128u
#define HIST_MAX     63
#define USADO        0x5A
#define TECLA_OK     2                     /* os codigos crus da varredura (a tabela 0x01E60369) */
#define TECLA_CIMA   3
#define TECLA_BAIXO  4
#define TECLA_MODO   17                    /* # */

struct registro_rx {
    u8 usado, ambar;
    char texto[REG_HIST - 2];              /* ate TEXTO_MAX e o 0 */
};

#define MODEM_RX     0x3CC3                /* FFSK 1200/1800, sem o ganho (bits 9:8) */

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

/* Apaga o historico (cheio, o EXIT segurado na tela, ou o formato velho): os dois setores e a marca de
 * novo. Devolve quantos ha, 0, ou o -1 de um historico que nao e nosso, sem mexer nele. */
static int hist_limpa(int n)
{
    u32 m = MARCA_HIST;

    if (n < 0)
        return n;
    APAGA_SETOR(SETOR_HIST);
    APAGA_SETOR(SETOR_HIST + 0x1000);
    GRAVA_FLASH(SETOR_HIST, &m, 4);
    return 0;
}

/* Abre o historico: confere a marca e conta os registros. Apagado, ou com o historico das v70 a v73 (os
 * registros de 64 B nao se leem como os de 128), comeca de novo. Devolve quantos ha, ou -1 se o setor tem
 * outra coisa: ai nada e gravado nele. */
static int hist_abre(void)
{
    u32 m;
    u8 b;
    int n;

    LE_FLASH(SETOR_HIST, &m, 4);
    if (m != MARCA_HIST)
        return m == 0xFFFFFFFFu || m == MARCA_V70 ? hist_limpa(0) : -1;
    for (n = 0; n < HIST_MAX; n++) {
        LE_FLASH(SETOR_HIST + (n + 1) * REG_HIST, &b, 1);
        if (b != USADO)
            break;
    }
    return n;
}

static int hist_grava(int n, const struct registro_rx *r)
{
    if (n >= HIST_MAX)                     /* cheio: recomeca */
        n = hist_limpa(n);
    if (n < 0)
        return n;
    GRAVA_FLASH(SETOR_HIST + (n + 1) * REG_HIST, r, sizeof *r);
    return n + 1;
}

/* Onde esta o registro i do historico (0 o mais velho), ou 0 se nao ha. */
static u32 hist_endereco(int n, int i)
{
    return i >= 0 && i < n ? SETOR_HIST + (i + 1) * REG_HIST : 0;
}

/* Um quadro que acabou de chegar, em texto, no historico. Separada: o registro (128 B) so pesa na pilha
 * aqui, fora do caminho da tela; e chamada no meio dos bits, antes de o quadro seguinte ocupar o q. */
__attribute__((noinline)) static int guarda(int hist, const u8 *q, unsigned n, const char *meu, unsigned n_meu)
{
    struct registro_rx r;

    r.usado = USADO;
    r.ambar = aprs_texto(r.texto, q, n, meu, n_meu);
    return hist_grava(hist, &r);
}

/* Direto na tela, sem rotina no meio: o quadro dela pesaria na pilha do laco. */
#define RX_MOSTRA(faixas) \
    (num[1] = topo + 1, num[2] = hist < 0 ? 0 : hist, num[5] = h.ruins, num[6] = h.consertados, \
     tela_aprs(hist_endereco(hist, hist - 1 - topo), hist_endereco(hist, hist - 2 - topo), aberto, num, (faixas)))

/* O meu indicativo em texto ("PY2ABC-7"); devolve o tamanho. Separada: o registro sai da pilha antes
 * do laco. */
__attribute__((noinline)) static unsigned meu_indicativo(char *meu)
{
    struct aprs a;
    unsigned n;

    le_aprs(&a);
    for (n = 0; n < a.tam_ind && n < 6; n++)
        meu[n] = a.ind[n];
    if (a.ssid && a.ssid < 16) {
        meu[n++] = '-';
        n = decimal(meu + n, a.ssid) - meu;
    }
    return n;
}

void aprs_rx_grava(void)
{
    u8 q[160];                             /* o comeco do quadro: os enderecos e ate ~120 de info */
    struct hdlc h;
    char meu[10];
    u16 num[7];                            /* ganho, posicao, quantos, syncs na marca e no espaco, BAD, FIX */
    unsigned i, f, n, k, w = 0, n_meu = meu_indicativo(meu), ativo = 0, pol = 0, desde = TIQUE_MS;
    unsigned pede = 0, aberto = 0;         /* as faixas da tela a redesenhar; o de cima aberto inteiro */
    unsigned era3f = REG_R(0x3F), era70 = REG_R(0x70), era72 = REG_R(0x72), era5e = REG_R(0x5E);
    int hist = hist_abre(), topo = 0;

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
        RX_MOSTRA(15);
        espera_soltar();                   /* a tecla que trouxe aqui (o OK que abriu, a que mudou a tela),
                                            * depois de a tela ja mostrar o efeito dela (v72) */
        for (;;) {
            WDT();
            if ((k = tecla()) != 255)
                break;
            ESPERA_2MS(1);
            if (!(REG_R(0x0C) & 1)) {
                /* esperando, a outra polaridade a cada 50 ms; recebendo, a FIFO parou: volta a esperar */
                if (TIQUE_MS - desde > (ativo ? 300u : 50u)) {
                    pol ^= !ativo;
                    ativo = 0;
                    fsk_rx_arma(pol);
                    desde = TIQUE_MS;
                } else if (pede) {         /* uma faixa da tela por volta, entre as leituras do modem */
                    k = pede & -pede;
                    pede ^= k;
                    RX_MOSTRA(k);
                }
                continue;
            }
            REG_W(0x02, 0);
            f = REG_R(0x02);
            if (f & 0x0002) {              /* sync: as flags ja passaram, o quadro vem */
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
                /* as 4 palavras bit a bit: o byte baixo primeiro, o bit alto de cada byte primeiro (a ordem
                 * do TX do chip, que o TA1JS usou no UV-K5 com o aprs.fi decodificando, e que o beacon da
                 * v65 provou) */
                for (k = 0; k < 64; k++) {
                    if (!(k & 15))
                        w = REG_R(0x5F);
                    if ((n = hdlc_bit(&h, w >> (k & 8 ? 23 - (k & 15) : 7 - (k & 15)) & 1))) {
                        hist = guarda(hist, q, n, meu, n_meu);
                        if (aberto && hist > 1)     /* o aberto continua o mesmo (o historico nao recomecou) */
                            topo++;
                        else                        /* senao, o novo vai para cima */
                            topo = aberto = 0;
                        pede = 15;
                    }
                }
            }
            if (f & 0x2000) {              /* o quadro do chip acabou: volta a esperar um sync */
                ativo = 0;
                fsk_rx_arma(pol);
                desde = TIQUE_MS;
            }
            if (i != h.ruins + h.consertados)
                pede |= 8;
        }
        if (k == TECLA_MODO) {
            num[0] = (num[0] + 1) & 3;
            REG_W(0x58, MODEM_RX | num[0] << 8);
            ativo = 0;
            fsk_rx_arma(pol);
            desde = TIQUE_MS;
            num[3] = num[4] = h.ruins = h.consertados = 0;
        } else if (k == TECLA_CIMA || k == TECLA_BAIXO) {
            if (k == TECLA_CIMA ? topo > 0 : topo + 1 < hist)
                topo += k == TECLA_CIMA ? -1 : 1;
        } else if (k == TECLA_OK) {
            aberto = !aberto && hist > 0;
        } else if (k == TECLA_EXIT && segurou_exit()) {     /* segurado: apaga o historico e fica */
            hist = hist_limpa(hist);
            topo = aberto = 0;
        } else if (k == TECLA_EXIT && aberto) {
            aberto = 0;
        } else {
            break;
        }
    }
    espera_soltar();
    REG_W(0x59, 0);
    REG_W(0x58, 0);                        /* desligado, como o roger de fabrica deixa (0x01E44788) */
    REG_W(0x5E, era5e);
    REG_W(0x3F, era3f);
    REG_W(0x70, era70);
    REG_W(0x72, era72);
    REG_W(0x02, 0);
}

/* Arma o envio; a mensagem leva um numero (1 a 99) para o outro lado confirmar ("ack"). */
void aprs_envia_grava(void)
{
    struct aprs a;

    le_aprs(&a);
    if (!a.tam_ind || !a.tam_msg)
        return;
    if (a.tam_dest) {
        a.id = a.id % 99 + 1;
        grava_aprs(&a);
    }
    ESTADO_MSG = ARMADO_APRS;
}
#endif
