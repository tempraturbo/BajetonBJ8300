/* Mensagens que qualquer radio entende (v65): Morse e APRS.
 *
 * O Morse sai pelo gerador de tom do chip de radio (o BK4819), o mesmo do roger: o tom liga e desliga
 * no ritmo do codigo, transmitindo, em FM (o "MCW" que qualquer receptor de FM ouve).
 *
 * O APRS e o padrao de todo radio com APRS: AX.25 a 1200 bps em AFSK Bell 202 (1200 Hz e 2200 Hz),
 * com NRZI, bit stuffing, flags 0x7E e o FCS do X.25. O modem FSK do chip so faz pares fixos de tons
 * (1200/1800 e 1200/2400, o do roger TONE1200), entao o AFSK sai como de um TNC de placa de som: amostras
 * no lugar das do microfone (v88, o AFSK pelo DAC, abaixo). Da v65 a v85 ele saia pelo gerador de tom do
 * chip, que nao chegava aos 2200 Hz; esse envio saiu dos testes do PC na v111.
 *
 * Nada aqui tem .data nem .bss (nao ha RAM nossa): o pacote e montado na pilha e mandado bit a bit.
 * Compila tambem no PC (TESTE_PC), onde tests/teste_mensagem.py troca o chip e o relogio por imitacoes
 * e confere o pacote, o AFSK, o decodificador e o Morse; e, inteiro, com o laco do APRS RX (TESTE_RX, v75), onde
 * tests/teste_rx.py imita o modem do BK4819, a flash e as teclas.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

#if defined(TESTE_PC)
#include "teste_mensagem.h"           /* REG_W, TOM e companhia, imitados */
#elif defined(TESTE_RX)
#include "teste_rx.h"                 /* o laco do APRS RX no PC: o chip, a flash, as teclas e o relogio (v75) */
#else
#define RAM(tipo, endereco)       (*(volatile tipo *)(endereco))
#define FIRMWARE(nome, endereco)  __asm__(".set " #nome ", " #endereco)

FIRMWARE(REG_W, 0x01E39D82);       void REG_W(unsigned, unsigned);   /* registrador do BK4819 */
FIRMWARE(TOM, 0x01E43B4E);         void TOM(unsigned, unsigned);     /* os 2 geradores, Hz x 10.32444 */
FIRMWARE(ESPERA_2MS, 0x01E36286);  void ESPERA_2MS(unsigned);        /* espera ocupada, n x 2 ms */

/* O relogio: b[0x14810] conta milissegundos, de 2 em 2 no tique do TIMER1 (a espera de fabrica,
 * 0x01E36286, espera n x 2 nele). */
#define TIQUE_MS   RAM(u32, 0x00014810)

/* A tabela do menu (tela.c): no comeco, a dos grupos (v112, a volta da tela MESSAGES). */
extern const unsigned char menu_tabela[];
#define MENU_BASE  ((u32)menu_tabela)

/* O numero em texto e a copia de texto, os dois com o 0 do fim: os da tela (tela.c, v113). O 0 cai onde o
 * texto continua (quem monta escreve por cima) ou na folga do fim de cada buffer. */
char *tela_numero(char *s, unsigned v, unsigned digitos);
char *tela_junta(char *s, const char *t);
#define numero  tela_numero
#define junta   tela_junta
#endif

/* Hz na unidade do reg 0x71 (x 10.32444, a conta do roger), para um tom que so se sabe na hora (o do
 * Morse), sem ponto flutuante: 330382 / 32000. */
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

#define MORSE_KA  0x35u               /* -.-.-, o comeco de mensagem: sem caractere na tabela */
#define MORSE_AR  0x2Au               /* .-.-., o fim de mensagem: na tabela e o + */

/* Parar no meio (o PTT ou uma tecla), mais abaixo; no PC, a imitacao. Chamada direta, nao por
 * ponteiro: o conferir.py so aceita destino que ele confere. */
static unsigned para(void);

/* Toca o texto com o gerador ja ligado: a unidade e 1200/wpm ms (a palavra PARIS), ponto 1, traco 3,
 * 1 entre elementos, 3 entre letras e 7 entre palavras. A espera conta de 2 em 2 ms. para() e
 * consultada antes de cada caractere e de cada elemento (o radio limpa o watchdog nela; o mais longe
 * entre duas e 10 unidades, 2,4 s a 5 wpm); ela pedindo, para no meio. v144: antes, portadora muda, para o
 * squelch de quem recebe abrir (o do UV levou ~0,2 s, medido, e a primeira letra se perdia); 0,5 s desde a v149,
 * e o texto vai entre o KA e o AR, que a leitura usa e qualquer operador entende. */
void morse_toca(const char *texto, unsigned n, unsigned wpm, unsigned hz)
{
    unsigned u = 600 / wpm, i, c, e;

    ESPERA_2MS(250);
    for (i = 0; i <= n + 1; i++) {
        if (para())
            return;
        c = !i ? MORSE_KA : i > n ? MORSE_AR : morse_de((u8)texto[i - 1]);
        if (!c) {                          /* espaco (ou caractere sem codigo): fecha a palavra */
            ESPERA_2MS(4 * u);             /* 3 da letra anterior + 4 = 7 */
            continue;
        }
        for (e = 0x80; !(c & e); e >>= 1)  /* acha a marca */
            ;
        for (e >>= 1; e; e >>= 1) {
            if (para())
                return;
            TOM(HZ_INT(hz), 0);
            ESPERA_2MS(c & e ? 3 * u : u);
            TOM(0, 0);
            ESPERA_2MS(u);
        }
        ESPERA_2MS(2 * u);                 /* 1 do elemento + 2 = 3 entre letras */
    }
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

/* ---------- AFSK pelo DAC (v86; o envio desde a v88) ----------
 * Pelo gerador de tom do chip (ate a v85), o APRSdroid nao leu nenhum pacote nosso, com o ganho, os
 * passa-baixas do TX e a pre-enfase variados (v84): o bit sozinho de 2200 Hz nao chegava aos 2200 Hz. Aqui
 * o AFSK sai como de um TNC de placa de som: amostras a 16 kHz no lugar das do microfone, que o JieLi manda
 * ao DAC e dali a entrada de microfone do BK4819. A fase e continua (um acumulador de 32 bits: o passo de
 * 1200 e 2200 Hz a 16 kHz) e cada bit tem 13, 13 e 14 amostras (40 a cada 3 bits, sem acumular erro).
 * dac_prepara poe o quadro em bits de linha (NRZI e bit stuffing: um bit de referencia
 * no espaco, as flags paradas na marca, o ultimo bit inteiro); dac_bloco enche um bloco de amostras. */
#define INC_MARCA    322122547u            /* 2^32 x 1200 / 16000: o passo da fase por amostra */
#define INC_ESPACO   590558003u            /* 2^32 x 2200 / 16000 */
#define DAC_LINHA    160                   /* 1280 bits de linha: 40 flags e o maior quadro com stuffing */

struct afsk_dac {
    u32 fase;
    u16 bit, bits;                         /* o bit de linha em curso e quantos ha */
    u16 silencio;                          /* amostras mudas antes do primeiro bit */
    u8 amostra, terco;                     /* a amostra dentro do bit; qual dos 3 bits (13, 13, 14 amostras) */
    u8 linha[DAC_LINHA];                   /* o tom de cada bit, o bit 0 primeiro: 1 espaco, 0 marca */
};

/* Um seno de 64 pontos, a -12 dB do fundo de escala: so o primeiro quarto, de 0 a 16, porque o segundo o
 * espelha e a segunda metade e a primeira negativa (v123, pelo espaco; os 64 pontos saem iguais). */
#if defined(TESTE_PC) || defined(TESTE_RX)
#define NAS_VOZES
#define NOS_CAMINHOS
#define NA_RESERVA
#define NOS_SONS
#define NO_FIM
#else
#define NAS_VOZES  __attribute__((section(".vozes.rodata.mensagem")))   /* a sobra do C1 (o .livre acabou) */
/* v138: no lugar dos caminhos das vozes (bancos.ld), so o que o nosso codigo chama - nunca o firmware */
#define NOS_CAMINHOS  __attribute__((section(".caminhos.mensagem")))
#define NOS_SONS      __attribute__((section(".sons.mensagem")))      /* v144: as mesmas regras, nas entradas mortas da tabela de caminhos */
#define NO_FIM        __attribute__((section(".fim.mensagem")))       /* v144: na area do fim da imagem, com o bancos.c */
#define NA_RESERVA    __attribute__((section(".reserva.mensagem")))   /* v140: as mesmas regras, na tabela de menu morta */
#endif
NAS_VOZES static const short seno17[17] = {
    0, 784, 1561, 2322, 3061, 3771, 4445, 5075, 5657, 6184, 6652, 7055, 7391, 7656, 7846, 7961, 8000};

static void linha_poe(struct afsk_dac *d, unsigned tom)
{
    unsigned k = d->bits;

    if (k >= DAC_LINHA * 8)
        return;
    if (tom)
        d->linha[k >> 3] |= 1u << (k & 7);
    else
        d->linha[k >> 3] &= ~(1u << (k & 7));
    d->bits = k + 1;
}

void dac_prepara(struct afsk_dac *d, const u8 *q, unsigned n, unsigned flags, unsigned silencio)
{
    unsigned i, k, v, b, tom = 1, uns = 0, dentro;

    d->fase = 0;
    d->bit = d->bits = 0;
    d->amostra = d->terco = 0;
    d->silencio = silencio;
    linha_poe(d, tom);                     /* a referencia, no espaco: o primeiro bit 0 troca para a marca */
    for (i = 0; i < flags + n + 3; i++) {
        dentro = i >= flags && i < flags + n;
        v = dentro ? q[i - flags] : 0x7E;
        for (k = 0; k < 8; k++, v >>= 1) {     /* o bit baixo primeiro */
            b = v & 1;
            tom ^= !b;
            linha_poe(d, tom);
            if (!dentro)
                continue;
            if (!b)
                uns = 0;
            else if (++uns == 5) {         /* cinco 1s: um 0 de enchimento */
                tom ^= 1;
                linha_poe(d, tom);
                uns = 0;
            }
        }
    }
    linha_poe(d, tom);                     /* o ultimo bit inteiro antes de calar */
}

void dac_bloco(struct afsk_dac *d, short *b, unsigned n)
{
    unsigned i;
    int k;

    for (i = 0; i < n; i++) {
        if (d->silencio) {
            d->silencio--;
            b[i] = 0;
            continue;
        }
        if (d->bit >= d->bits) {
            b[i] = 0;
            continue;
        }
        d->fase += d->linha[d->bit >> 3] >> (d->bit & 7) & 1 ? INC_ESPACO : INC_MARCA;
        k = d->fase >> 26 & 31;
        k = seno17[k > 16 ? 32 - k : k];
        b[i] = (short)((s32)d->fase < 0 ? -k : k);
        if (++d->amostra >= 13u + (d->terco == 2)) {
            d->amostra = 0;
            d->terco = d->terco == 2 ? 0 : d->terco + 1;
            d->bit++;
        }
    }
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
#ifdef TESTE_PC
                    h->consertados++;
#endif
                    return m;
                }
                troca_par(h->q, crc - 1);  /* o conserto nao era: desfeito */
            }
            /* o BAD (v74): um quadro com cabecalho de APRS que chegou com bits errados, alinhado ou nao
             * (antes so o alinhado contava). O ruido depois do pacote quase nunca tem esse cabecalho. */
#ifdef TESTE_PC
            if (cabecalho_aprs(h->q, m))
                h->ruins++;
#endif
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
#define decimal(p, v)  numero((p), (v), 1)

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
        return junta(p, t + 1);
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
    p = junta(p, "KM/H ");
    p = numero(p, rumo, 1);
    return junta(p, "DEG");
}

NOS_SONS static char *altitude(char *p, int metros)
{
    p = junta(p, "ALT ");
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
            p = junta(p, " ");
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
            p = junta(p, "HPA");
        }
        x += 1 + l[1] - '0';
    }
    if (vento >= 0) {                      /* mph em km/h (SEM_DADO e negativo) */
        *p++ = ' ';
        p = numero(p, (vento * 1609 + 500) / 1000, 1);
        p = junta(p, "KM/H");
        if (rumo >= 0) {
            *p++ = ' ';
            p = numero(p, rumo, 1);
            p = junta(p, "DEG");
        }
    }
    *px = x;
    return p;
}

NOS_CAMINHOS static unsigned base91(const u8 *x, unsigned n)
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
NOS_CAMINHOS static char *mic_e(char *p, const u8 *d, unsigned desloc, const u8 **px, const u8 *fim)
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
        p = junta(p, MIC_E[bits]);
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
        p = junta(p, "W ");
        p = numero(p, ((10u << (x[4] - '0')) * 3048 + 5000) / 10000, 1);
        p = junta(p, "M ");
        p = numero(p, x[5] - '0', 1);
        p = junta(p, "DBI");
        *px = x + 7;
    }
    return p;
}

/* Uma posicao em x (sem compressao ou comprimida), com o que vem depois dela: o rumo e a velocidade, o
 * PHG, o tempo (o simbolo _). Devolve 0 se x nao e uma posicao; senao o fim do texto, e em *px onde o
 * comentario comeca. */
NA_RESERVA static char *posicao(char *p, const u8 **px, const u8 *fim)
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
 *  - mensagem para outro: "ORIGEM>DESTINO: texto", tambem sem o "{id" (a fonte nao tem o "{", que saia como
 *    "?25"; v138, como no UV-17L); a confirmacao: "ORIGEM>DESTINO: ACK id";
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
NOS_CAMINHOS unsigned aprs_texto(char *s, const u8 *q, unsigned n, const char *meu, unsigned n_meu, char *id)
{
    char *p = s, *f;
    const u8 *x, *fim = q + n, *d = q;     /* d: o destino (do Mic-E), em bytes de endereco ou em texto */
    unsigned i = 7, k, para_mim = 0, msg = 0, bruto = n_meu & TEXTO_BRUTO;

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
        msg = 1;
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
            p = junta(p, "ACK ");          /* "ack" e o id, de 1 a 5, e nada depois (v74: "ackermann" */
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
                f = tempo(junta(p, "WX"), &x, fim);
            } else if (k == ';' && x + 17 <= fim) {
                f = junta(p, "OBJ ");      /* ;NOME_____*DDHHMMz e a posicao */
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
                    f = junta(p, "ITEM ");
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
    for (; i < n && p < s + TEXTO_MAX && !(msg && q[i] == '{'); i++) {   /* o "{id" de toda mensagem sai (v138) */
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

/* ---------- a leitura do Morse (v144, a mesma do UV-17L) ----------
 * O audio recebido nao chega ao processador (v96): o que chega e o 0x6F do BK4819. Aqui ele nao se porta como no UV-17L,
 * em que o tom fica parado em ~87 (o morse.c de la): medido neste radio (v146d, 49 s de 6 em 6 ms, com o UV mandando
 * ABC DEF a 15 WPM), a portadora muda fica entre 12 e 42, e o tom salta para fora dessa faixa, num ciclo de 5 amostras a
 * 700 Hz (18 13 3 20 117), que parece o medidor passando de 127 e dando a volta; o chiado sem portadora vai de 0 a 126.
 * Depois do tom ele desce de ~112 ate ~23 em ~100 ms. A regra: e tom a amostra de ate 11, que a portadora muda nunca da,
 * ou de 100 para cima, e o tom dura 42 ms alem da ultima dessas. As duas capturas, com o AGC solto e travado, leram tudo.
 * Provado so a 15 WPM e com o tom de 700 Hz, que e o padrao dos dois radios.
 *
 * So o tempo importa, nao a altura do tom: tom e pausa viram ponto, traco e fim de letra pela unidade, que comeca na
 * velocidade do SPEED e acompanha quem manda. Fala tambem passa do limite, mas sai em letras soltas, separadas por
 * pausas de palavra: sem o KA, a mensagem so vale com palavras de duas letras ou mais, em media. */
#define MRX_MUDOU  1u
#define MRX_FIM    2u                  /* a mensagem acabou e vale: quem chama a guarda e da mrx_limpa */
#define TOM_BAIXO  11u                 /* a portadora muda nunca fica nisso ou abaixo */
#define TOM_ALTO   100u                /* nem nisso ou acima */
#define SEGURA_MS  42u                 /* o tom dura isso alem da ultima amostra fora da faixa */
#define CAUDA      55u                 /* quanto o tom parece mais longo, e a pausa mais curta, em ms */
#define UN_MIN     48u                 /* 25 WPM */
#define UN_MAX     300u                /* 4 WPM */
#define RUIDO_MS   1200u               /* mais tom que o traco de 5 WPM: e chiado, a portadora acabou */
#define FIM_MS     2500u               /* silencio que fecha a mensagem */

struct morse_rx {
    u32 desde, virou;                  /* a ultima troca entre tom e silencio, e o comeco da que pode vir */
    u32 inicio;                        /* o comeco do elemento */
    u32 ultima;                        /* a ultima amostra fora da faixa da portadora muda */
    u16 un, n;                         /* a unidade, o tempo do ponto, em ms, e o tamanho do texto */
    u8 letras, palavras;               /* o que a mensagem tem ate agora */
    u8 ligado, contra;                 /* no tom; amostras seguidas do outro lado */
    u8 espera;                         /* parado ate o primeiro silencio: o chiado antes do sinal e depois dele */
    u8 codigo;                         /* a letra em montagem: um 1 de marca e um bit por elemento, como a tabela */
    u8 ka, espaco;                     /* chegou o KA, o comeco de mensagem; o texto ja termina em espaco */
    u8 usado, ambar;                   /* como o comeco do registro do historico: a tela le daqui como de la */
    char texto[TEXTO_MAX + 1];
};

NA_RESERVA static void mrx_limpa(struct morse_rx *m)
{
    m->n = m->letras = m->palavras = 0;
    m->ka = m->espaco = 0;
    m->texto[0] = 0;
}

NA_RESERVA static void mrx_comeca(struct morse_rx *m, unsigned wpm)
{
    unsigned i;

    for (i = 0; i < sizeof *m; i++)
        ((u8 *)m)[i] = 0;
    m->un = wpm < 25 ? 1200u / wpm : UN_MIN;
    m->espera = 1;
    m->usado = 0x5A;
}

NA_RESERVA static unsigned mrx_vale(const struct morse_rx *m)
{
    unsigned palavras = m->palavras + !m->espaco;

    return m->ka || (m->letras >= 3 && m->letras >= 2u * palavras);
}

/* Um ponto ou traco a mais na letra. A unidade anda um quarto do que ele pede. */
NA_RESERVA static void mrx_elemento(struct morse_rx *m, unsigned traco, u32 unidade)
{
    unsigned un = (3u * m->un + unidade) / 4u;

    m->un = un < UN_MIN ? UN_MIN : un > UN_MAX ? UN_MAX : un;
    m->codigo = m->codigo ? m->codigo << 1 | traco : 2u | traco;
}

/* A letra acabou: o codigo volta ao caractere pela tabela do envio. Devolve 1 no AR, o fim da mensagem, ate quando o KA
 * se perdeu; so sem nada escrito ele e o + da tabela. */
NOS_CAMINHOS static unsigned mrx_letra(struct morse_rx *m)
{
    unsigned c = m->codigo, k = '!';

    m->codigo = 0;
    if (c == MORSE_KA) {
        mrx_limpa(m);
        m->ka = 1;
        return 0;
    }
    if (c == MORSE_AR && (m->ka || m->n))
        return 1;
    while (k <= 'Z' && morse_de(k) != c)
        k++;
    if (k > 'Z' && !m->n)              /* o KA sem o comeco, que o squelch comeu: nada de ? no comeco (v149) */
        return 0;
    if (m->n < TEXTO_MAX) {
        m->texto[m->n++] = k > 'Z' ? '?' : k;
        m->texto[m->n] = 0;
    }
    m->letras++;
    m->espaco = 0;
    return 0;
}

/* A mensagem acabou: sem o espaco do fim. */
NOS_CAMINHOS static unsigned mrx_fecha(struct morse_rx *m)
{
    unsigned n = m->n;

    while (n && m->texto[n - 1] == ' ')
        n--;
    m->n = n;
    m->texto[n] = 0;
    if (n && mrx_vale(m))
        return MRX_FIM | MRX_MUDOU;
    mrx_limpa(m);
    return MRX_MUDOU;
}

/* Uma amostra do 0x6F, de 5 em 5 ms ou menos, e se ha portadora; devolve MRX_MUDOU e MRX_FIM. */
NOS_SONS static unsigned mrx_amostra(struct morse_rx *m, u32 agora, unsigned nivel, unsigned aberto)
{
    unsigned tom, ev = 0, un = m->un;
    u32 d;

    /* sem portadora, vale como silencio: uma piscada do squelch no meio do tom nao o quebra, e o que chegou fecha */
    if (aberto && (nivel <= TOM_BAIXO || nivel >= TOM_ALTO))
        m->ultima = agora;
    tom = agora - m->ultima < SEGURA_MS;

    if (tom == m->ligado)
        m->contra = 0;
    else {
        if (!m->contra)
            m->virou = agora;
        if (++m->contra >= 2) {        /* uma amostra so do outro lado e tremida */
            d = m->virou - m->inicio;
            m->desde = m->virou;
            m->ligado = tom;
            m->contra = 0;
            if (!m->espera && !tom && d >= CAUDA + un / 3u) {   /* mais curto que um terco do ponto e pico, nao elemento */
                d -= CAUDA;
                if (d >= 2u * un)
                    mrx_elemento(m, 1, d / 3u);
                else
                    mrx_elemento(m, 0, d);
            }
            if (tom)
                m->inicio = m->virou;
        }
    }

    if (aberto && !tom)
        m->espera = 0;                 /* portadora em silencio: dali em diante e sinal */
    d = agora - m->desde;
    if (m->espera)
        return 0;
    if (m->ligado) {
        if (d > RUIDO_MS) {
            m->espera = 1;
            m->codigo = 0;
            return mrx_fecha(m);
        }
        return 0;
    }
    if (m->codigo && d + CAUDA >= 2u * m->un && mrx_letra(m)) {
        m->espera = 1;                 /* a cauda depois do AR nao e letra */
        return mrx_fecha(m);
    }
    if (m->n && !m->espaco && d + CAUDA >= 5u * m->un) {
        m->texto[m->n] = ' ';          /* a tela anda por palavra: da tempo antes do tom seguinte */
        if (m->n < TEXTO_MAX)
            m->texto[++m->n] = 0;
        m->palavras++;
        m->espaco = 1;
        ev = MRX_MUDOU;
    }
    if (m->n && d >= FIM_MS) {
        m->espera = 1;
        ev = mrx_fecha(m);
    }
    return ev;
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

/* Sair do editor como o CH-NAME no fim do set dele (0x01E3C822): sem isto o editor fica ligado, e o cabecalho do menu
 * mostra o "abc" do teclado em todos os itens, ate sair e entrar no menu de novo. */
NA_RESERVA static void editor_fecha(void)
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
 *  - o SEND do menu (o do Morse; o do APRS, desde a v112, o OK da caixa da tela MESSAGES) so arma
 *    (ESTADO_MSG);
 *  - na volta seguinte do laco, gancho_ida_ptt (no lugar da chamada da ida do PTT, 0x01E46540)
 *    chama o inicio da TX como o PTT: potencia, desarme da dupla escuta, faixa permitida e canal
 *    ocupado ficam com ele, e o menu fecha (como com o PTT no menu);
 *  - gancho_fim_tx (no lugar da chamada do fim no passo 3, 0x01E46D8C) toca o Morse ou o AFSK no
 *    lugar do roger e chama o fim de fabrica com o roger zerado.
 * O estado mora no byte 0x9044 dos ajustes (b[0x14BF0]; ate a v110, o 0x903A, que agora e do EQ), que
 * nada de fabrica le nem grava fora das copias do bloco. Os codigos sao letras que nada mais grava ali (o
 * padrao de fabrica deixa 0), e a partida e os ajustes pelo cabo o zeram (gancho_partida, gancho_ajustes):
 * nada transmite sozinho. O watchdog (~4 s; limpo pela interface a cada 10 ms, que nao roda enquanto o conteudo
 * toca) e limpo por nos (0x01E37E76), como o roger de fabrica faz antes dos tons. */
#define ESTADO_MSG     RAM(u8, 0x00014BF0)
#define ARMADO_MORSE   'M'
#define ARMADO_APRS    'A'
#define ARMADO_ACK     'K'               /* v75: o ack de uma mensagem para mim, armado pelo APRS RX */
#define NO_AR(e)       ((e) | 0x20)      /* 'm', 'a', 'k': a TX comecou, o conteudo vai no fim dela */
#define ARMADO(e)      ((e) == ARMADO_MORSE || (e) == ARMADO_APRS || (e) == ARMADO_ACK)
/* O registro do historico do APRS RX com a origem e o numero do ack (o setor * 32 + o lugar), no 0x903E
 * dos ajustes (b[0x14BEA], livre pela varredura da v59; so vale com o ESTADO_MSG em 'K'). */
#define ACK_REGISTRO   RAM(u8, 0x00014BEA)
#define HIST_SETOR(s)  hist_setor(s)    /* o historico: dois setores, 0xD6000 no APRS e 0xD8000 no Morse (v144) */
#define REG_HIST       128u              /* um registro (struct registro_rx, no APRS RX) */
#define ACK_PARA       (2 + TEXTO_MAX + 1)               /* a origem (9) e o numero (6) dentro dele */

FIRMWARE(LE_FLASH, 0x01E390AC);     void LE_FLASH(u32, void *, u32);
FIRMWARE(APAGA_SETOR, 0x01E3A126);  void APAGA_SETOR(u32);
FIRMWARE(GRAVA_FLASH, 0x01E3A25A);  void GRAVA_FLASH(u32, const void *, u32);

FIRMWARE(IDA_PTT, 0x01E43E8A);     void IDA_PTT(void);
FIRMWARE(ACORDA, 0x01E39EE4);      void ACORDA(void);         /* sai da economia de bateria */
/* O SAVE do menu (b[0x14B6D], o 0x9001 dos ajustes; get 0x01E39342, set 0x01E39352): a economia de bateria do
 * laco do sinal (0x01E48060) so poe o receptor para dormir com ele diferente de 0. Fora o get e o set, nada mais
 * o le nem grava. */
#define SAVE           RAM(u8, 0x00014B6D)
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
NA_RESERVA static void espera_soltar(void)
{
    while (tecla() != 255) {
        WDT();
        ESPERA_2MS(5);
    }
}

#define TECLA_OK     2                     /* os codigos crus da varredura (a tabela 0x01E60369) */
#define TECLA_CIMA   3
#define TECLA_BAIXO  4
#define TECLA_EXIT   5
#define TECLA_ESTRELA 9
#define TECLA_ZERO   13
#define TECLA_CERQUILHA 17

/* O EXIT segurado no editor de texto sai sem salvar (v72). No uso normal a fabrica manda o toque curto
 * de uma tecla so quando ela e solta (o laco do sinal, 0x01E46372: apertar nao manda nada; segurar 1 s
 * manda o codigo longo, 35 no EXIT, e marca b[0x146F4] para o solto nao mandar nada), e por isso o
 * gancho da v70, na rotina do EXIT do editor, nunca via o EXIT apertado. Este fica no lugar da leitura
 * das teclas do laco do sinal (0x01E46280, a cada 10 ms), que conta as voltas da mesma tecla em
 * b[0x14680] e so aceita a tecla igual em tres leituras seguidas. Com o EXIT na 60a volta (0,6 s) no
 * editor de texto (MENU_TIPO 7 editando, como a tela B o reconhece), o cursor vai a 0 e a leitura passa a
 * devolver "nenhuma": a fabrica ve o EXIT solto, manda o toque curto, e a rotina do EXIT do editor, com o
 * cursor no comeco, sai sem salvar (0x01E4D486 -> 0x01E4D4F4). Enquanto o EXIT continuar apertado a
 * leitura segue devolvendo "nenhuma" (a marca no 0x9045 dos ajustes, b[0x14BF1], livre: nenhum acesso de
 * fabrica pela base 0x14680 nem pelo endereco; ate a v110, o 0x903C, que agora e do EQ), para o solto de
 * verdade nao virar outro EXIT, que subiria o menu. Um valor que sobre na marca (os ajustes gravados com
 * ela ligada, ou os 0x20 do RESET de fabrica) se apaga na primeira leitura sem o EXIT. */
#define CONTA_TECLA   RAM(u8, 0x00014680)   /* voltas com a mesma tecla, no laco do sinal */
#define MENU_EDITANDO RAM(u8, 0x00014BF9)
#define MENU_TIPO     RAM(u8, 0x00014C00)
/* A tecla que agiu segurada (o EXIT no editor, v72; o # da tela MESSAGES, v114): enquanto a marca esta no
 * 0x9045 (b[0x14BF1]), toda leitura vale "nenhuma", e ela so sai depois de 5 leituras seguidas sem tecla.
 * A leitura de fabrica (0x01E43D50) as vezes devolve 0, e nao 255, com a tecla apertada (0x01E43D5E: o bit 5
 * de [obj+4] desligado) - a fabrica aguenta isso porque so aceita a mesma tecla em tres leituras seguidas.
 * Na v114 uma leitura assim soltava a marca do #, a fabrica via o # segurado e, em 1 s, ligava a trava do
 * teclado (o codigo longo do #, o evento 2: 0x01E49630 alterna b[0x14B87], o 0x901B dos ajustes). */
#define TECLA_PRESA   RAM(u8, 0x00014BF1)   /* 0: nenhuma; 1 a 5: presa, as leituras soltas que ainda faltam */
#define ESTADO_RX     RAM(u8, 0x00015367)   /* 0 e 1 sintoniza, 2 espera, 3 vigia, 4 recebendo, 5 MO */

/* A caixa da tela MESSAGES (v112). O # curto abre por cima da tela a caixa MESSAGE TO SEND, sempre vazia, e o
 * # segurado a caixa TO, com o destinatario gravado: o laco sai (aprs_rx_grava), e o desvio depois do set
 * (b_menu_marca, tela.c) abre o editor de texto de fabrica no item TO da pasta, sem a limpeza da tela que o
 * OK de fabrica faria (aprs_caixa_abre). A tela B desenha so a caixa e cala o resto (telas_faixa, desenha,
 * b_lado, e a limpeza do EXIT que cancela, gancho_limpa_caixa). O OK chama o set do TO (aprs_dest_grava),
 * que grava o destinatario, ou grava a mensagem e arma o envio; o envio fecha o menu, como o PTT.
 *
 * A volta a tela, depois do OK, do EXIT que cancela, do envio e do ack: a tecla OK, injetada aqui na leitura
 * das teclas do laco do sinal, como se o dono a apertasse - na tela principal ela abre o menu (0x01E4CBA2,
 * o evento 17), que reabre na pasta APRS (menu_abre); nos grupos, entra na pasta; na pasta, o item vai ao
 * MESSAGES e o OK o abre; aberto, o OK chama o set, e a tela volta. So com a TX parada, o envio feito, nenhum
 * bipe tocando (b[0x14924]) e a recepcao fora da sintonia (o estado 2 ou mais), e um ciclo parado antes do
 * primeiro OK (v114): o laco da tela para o laco do sinal, e o audio do chip e o amplificador so voltam quando a
 * recepcao entra no estado 4 (0x01E47E0A e 0x01E47E1A) - com a tela aberta antes, ficavam mudos ate a saida
 * (o OK e o EXIT da caixa sintonizam de novo). O bipe das teclas (o BEEP PROMPT, b[0x14B72], que 0x01E38B4C le)
 * fica calado enquanto isso. Uma
 * tecla de verdade ou o PTT desistem da volta, e ela desiste sozinha depois de 7 ciclos sem a tela voltar (o
 * caminho pede 5, com o parado). Cada OK: apertado 5 leituras de 10 ms (a fabrica so aceita a mesma em tres
 * seguidas) e solto 11, para o toque curto sair e a interface o tratar.
 *
 * O estado mora no 0x9046 dos ajustes (b[0x14BF2]) e a conta da volta no 0x9043 (b[0x14BEF]), livres pela
 * varredura da v111, fora da janela do cabo; a partida e os ajustes de fabrica os zeram. */
#define TELA_APRS     RAM(u8, 0x00014BF2)   /* 0x9046 */
#define CAIXA         3u                    /* a caixa aberta: 1 a mensagem, 2 o TO */
#define CAIXA_MSG     1u
#define CAIXA_TO      2u
#define FILTRO_OFF    4u                    /* a tecla 0 tinha desligado o FILTER: a tela volta assim */
#define VOLTA         8u                    /* a tela MESSAGES volta sozinha */
#define TELA_MORSE    16u                   /* v144: a mesma tela, lendo Morse, na pasta MORSE, onde ela e o item 0 */
#define INJETA        RAM(u8, 0x00014BEF)   /* 0x9043: as leituras do OK injetado, de 1 em diante (bits 6:0), e o */
#define BEEP_PROMPT   RAM(u8, 0x00014B72)   /* BEEP PROMPT (0 ou 1) que a volta calou (bit 7) */
#define MENU_TABELA   RAM(u32, 0x00014BF4)
#define MENU_NUMERO   RAM(u8, 0x00014BFA)
#define MODO_TELA     RAM(u8, 0x0001533D)   /* 0 a tela principal, 1 o menu */
#define ITEM_CALLSIGN 0                     /* na pasta APRS (conferir.py, 3b; v138) */
#define SQL_NIVEL     RAM(u8, 0x00014B6C)   /* v144: o SQUELCH do menu */
#define BANDA_LED     RAM(u8, 0x00015E42)   /* v149: o que a fabrica passa ao LED_RX (0x01E47BE8) */
#define ITEM_MESSAGES 2
#define ITEM_TO       5

/* O setor s do historico da tela em uso: 0xD6000 no APRS e 0xD8000 no Morse, livre no mapa da secao 3 (v144). */
NOS_CAMINHOS static u32 hist_setor(unsigned s)
{
    return 0x000D6000u + (s + (TELA_APRS & TELA_MORSE ? 2 : 0)) * 0x1000u;
}

#ifdef TESTE_RX
#define LED_RX(banda, liga)
#define RX_FECHA()  (TELA_APRS &= 127)
#else
FIRMWARE(LED_RX, 0x01E40CD2);         void LED_RX(unsigned, unsigned);   /* o que a fabrica chama ao abrir (1) e fechar (0) */
FIRMWARE(AUDIO_DO_CHIP, 0x01E39E22);  void AUDIO_DO_CHIP(unsigned);
FIRMWARE(PEDE_AMPLI, 0x01E39E2C);     void PEDE_AMPLI(unsigned);
FIRMWARE(AMPLI, 0x01E39CFC);          void AMPLI(unsigned);
/* As telas MESSAGES so se ouvem com o SQL em OFF: com ele ligado, o alto-falante fecha, o que o espectro faz ao seguir
 * (0x01E4AEFC), e o LED apaga, ao entrar e ao sair. */
NOS_SONS static void rx_fecha(void)
{
    TELA_APRS &= 127;                  /* o estado do LED, do rx_portadora */
    if (SQL_NIVEL) {
        AUDIO_DO_CHIP(0);
        PEDE_AMPLI(6);
        AMPLI(0);
        LED_RX(BANDA_LED, 0);
    }
}
#define RX_FECHA()  rx_fecha()
#endif

/* A portadora, e o LED acompanha, como o laco de fabrica faria (v149). O estado fica no TELA_APRS: a pilha do laco esta no
 * limite, e quem sai dele reescreve o byte. */
NOS_SONS static unsigned rx_portadora(void)
{
    unsigned k = !SQL_NIVEL || REG_R(0x0C) >> 1 & 1;

    if (k != TELA_APRS >> 7) {
        TELA_APRS ^= 128;
        LED_RX(BANDA_LED, k);
    }
    return k;
}

/* A volta acabou (a tela voltou, ou ela desistiu): o bipe como estava, e o estado zerado. Devolve o
 * FILTRO_OFF, que a tela usa. */
static unsigned volta_fim(void)
{
    unsigned f = TELA_APRS / FILTRO_OFF & 1;

    if (INJETA)
        BEEP_PROMPT = INJETA >> 7;
    INJETA = 0;
    TELA_APRS = 0;
    return f;
}

unsigned gancho_teclas(void)
{
    unsigned k = TECLAS(), t = TELA_APRS, j = INJETA, editor = MENU_TIPO == 7 && MENU_EDITANDO == 1;

    if (TECLA_PRESA) {                     /* a tecla que ja agiu segurada: nada passa ate soltar de vez */
        TECLA_PRESA = k == 255 ? TECLA_PRESA - 1 : 5;
        k = 255;
    } else if (k == TECLA_EXIT && editor && CONTA_TECLA >= 60) {
        EDITOR_CURSOR = 0;
        TECLA_PRESA = 5;
        k = 255;
    }
    if ((t & CAIXA) && !editor) {          /* a caixa fechou pelo EXIT (o OK ja pos a volta): sintoniza de */
        TELA_APRS = t = (t & (FILTRO_OFF | TELA_MORSE)) | VOLTA;   /* novo, como o OK de fabrica (0x01E49A86) */
        ESTADO_RX = 0;
    }
    if (!(t & VOLTA))
        return k;
    if (k != 255 || !PTT_SOLTO() || (j & 0x7F) >= 7 * 16) {
        volta_fim();
        return k;
    }
    if (ESTADO_MSG || TX_ESTADO || MODO_TELA > 1 || BIPE || ESTADO_RX < 2)
        return k;                          /* o envio, um bipe ou a sintonia em curso; ou outra tela */
    if (!j) {
        j = BEEP_PROMPT << 7;
        BEEP_PROMPT = 0;
    }
    if (!(j & 15) && MODO_TELA && MENU_TABELA != MENU_BASE && MENU_EDITANDO != 1) {
        MENU_NUMERO = VALOR = t & TELA_MORSE ? 0 : ITEM_MESSAGES;   /* na pasta: o MESSAGES, que o OK abre */
        MENU_TIPO = 0;
    }
    INJETA = j + 1;
    return (j & 15) < 5 && (j & 0x70) ? TECLA_OK : 255;   /* o primeiro ciclo, parado */
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
        p = (u8 *)junta((char *)p, ":ack");
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

/* O AFSK pelo DAC (v86; o envio desde a v88). O estado mora na memoria de video (0x10190), como o registro do
 * APRS RX (v76): enquanto o conteudo toca, a interface nao roda e ninguem desenha. O ESTADO_MSG em 'D'
 * liga o gancho do microfone; o laco espera o ultimo bit, com um prazo (se os blocos do microfone nao
 * vierem, a TX nao trava), e mais 120 ms mudos para o DAC esvaziar. */
#define AFSK_DAC     'D'
#define DAC_ESTADO   ((volatile struct afsk_dac *)0x00010190)

FIRMWARE(MIC_BLOCO, 0x01E41C06);   void MIC_BLOCO(unsigned, short *, unsigned);
FIRMWARE(MIC_DAC, 0x01E41BA0);     unsigned MIC_DAC(short *, unsigned);

/* No lugar do 0x01E41C06 na abertura do microfone (o ponteiro que 0x01E41CEC entrega a interrupcao de
 * audio): com o envio em curso, o bloco vira AFSK e vai direto ao DAC (0x01E41BA0, que o duplica em L/R
 * se a saida e estereo), sem passar pela NR; senao, segue como de fabrica. */
void gancho_mic(unsigned r0, short *b, unsigned bytes)
{
    if (ESTADO_MSG != AFSK_DAC) {
        MIC_BLOCO(r0, b, bytes);
        return;
    }
    dac_bloco((struct afsk_dac *)DAC_ESTADO, b, bytes / 2);
    MIC_DAC(b, bytes);
}

static void afsk_dac_manda(const u8 *q, unsigned n)
{
    volatile struct afsk_dac *d = DAC_ESTADO;
    u32 t0 = TIQUE_MS, prazo;

    dac_prepara((struct afsk_dac *)d, q, n, 40, 8000);     /* 0,5 s mudo (v69) e 40 flags (270 ms) */
    prazo = 500 + d->bits * 5u / 6 + 1000;                  /* ms: o mudo, os bits de 833 us e a folga */
    ESTADO_MSG = AFSK_DAC;
    while ((d->silencio || d->bit < d->bits) && TIQUE_MS - t0 < prazo)
        WDT();
    ESPERA_2MS(60);
    ESTADO_MSG = 0;
}

/* A mensagem ou o status (ack 0), ou o ack cuja origem esta em ack. */
static void manda_aprs(u32 ack)
{
    u8 q[CAB(2) + 1 + 9 + 1 + MAX_MSG + 3 + 2];
    unsigned n = quadro_aprs(q, ack);

    if (n)
        afsk_dac_manda(q, n);
}

/* A tela de enviando (v149): so a palavra, na tela MESSAGES. */
#define ENVIANDO     33u
void tela_aprs(u32 a0, u32 a1, unsigned aberto, const u16 *num, unsigned faixas);

/* No lugar do roger, no fim da TX que o gancho_ida_ptt ligou, e o fim de fabrica com o roger zerado - que
 * nao grava nada. O Morse usa o gerador de tom: o mesmo preparo e o mesmo desligar do roger BEEP
 * (0x01E44622..0x01E44634 e 0x01E44656..0x01E4466A), com o tom 2 desligado (reg 0x70: so o tom 1, com o
 * ganho do roger), e espera as teclas soltarem (o OK que armou o envio ainda pode estar apertado, e o Morse
 * o veria como pedido de parar). O APRS (v86) sai pelo microfone, como a voz: o chip com o ADC do microfone
 * religado (v88; o passo 3 o desliga), e o gancho do microfone poe o mudo e o AFSK no lugar das amostras. */
void gancho_fim_tx(void)
{
    unsigned e = ESTADO_MSG, r, s31, s51, morse = e == NO_AR(ARMADO_MORSE);

    if (!(e & 0x20) || !ARMADO(e & ~0x20)) {
        FIM_TX();
        return;
    }
    ESTADO_MSG = 0;
    if (e != NO_AR(ARMADO_ACK))
        tela_aprs(0, 0, ENVIANDO, 0, 15);
    if (morse) {
        AUDIO_TX(4);                       /* o microfone cortado ja, antes das esperas (v74): a portadora
                                            * muda levava o som do ambiente ao ar */
        espera_soltar();
        GERADOR(1);
        AF(1);
        REG_W(0x70, 0xE000);
    }
    WDT();
    /* sem o scramble e o subtom durante o conteudo (v75): o scramble (bit 1 do 0x31, secao 13) espelha o
     * audio, e o subtom (bit 15 do 0x51, que a fabrica liga com o nivel, 0x01E441BE) soma um tom grave
     * ao AFSK; os dois voltam antes do fim de fabrica */
    s31 = REG_R(0x31);
    s51 = REG_R(0x51);
    REG_W(0x31, s31 & ~2u);
    REG_W(0x51, s51 & ~0x8000u);
    if (morse) {
        toca_morse();
    } else {                               /* a mensagem, ou o ack do registro que o RX marcou */
        /* o ADC do microfone do chip religado durante o pacote: a passagem ao passo 3 (o PTT solto,
         * 0x01E479A0..0x01E479AC) desliga o bit 2 do 0x30, e sem ele o DAC chega ao chip e nao vai ao ar
         * (v87: o PTT segurado leva o tom do DAC, o envio nao) */
        unsigned s30 = REG_R(0x30);

        REG_W(0x30, s30 | 4u);
        manda_aprs(e == NO_AR(ARMADO_ACK) ? HIST_SETOR(ACK_REGISTRO >> 5 & 1) + (ACK_REGISTRO & 31) * REG_HIST
                                            + ACK_PARA : 0);
        REG_W(0x30, s30);
    }
    REG_W(0x51, s51);
    REG_W(0x31, s31);
    if (morse) {
        PEDE_AMPLI(10);
        TOM(0, 0);
        GERADOR(0);
        AF(0);
    }
    r = ROGER;
    ROGER = 0;
    FIM_TX();
    ROGER = r;
}

/* ---------- MORSE ---------- */
const char titulo_morse_wpm[] = "SPEED";
const char titulo_morse_tom[] = "TONE";

/* A caixa da tela do Morse (v144): abre com o ultimo texto, e o OK o guarda e arma o envio. Separada do set: o registro
 * sai da pilha antes do laco. */
__attribute__((noinline)) NO_FIM static void morse_caixa_abre(void)
{
    struct morse m;

    le_morse(&m);
    editor_abre(m.texto, m.tam, MAX_MSG);
}

__attribute__((noinline)) NO_FIM static void morse_caixa_grava(void)
{
    struct morse m;

    le_morse(&m);
    m.tam = editor_texto(m.texto, MAX_MSG);
    grava_morse(&m);
    if (m.tam)
        ESTADO_MSG = ARMADO_MORSE;
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

/* ---------- APRS ---------- */
const char titulo_aprs_ind[] = "CALLSIGN";    /* sem o APRS da pasta (v83) */
const char titulo_aprs_ssid[] = "SSID";
const char titulo_aprs_dest[] = "TO";
const char titulo_aprs_caminho[] = "PATH";
const char titulo_aprs_rx[] = "MESSAGES";      /* o RX ate a v111 */
const char *const aprs_caminho_opcoes[3] = {WIDE[0], "WIDE1-1,WIDE2-1", "NONE"};   /* o do quadro (v111) */
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
NOS_CAMINHOS static unsigned resumo_texto(const char *t, unsigned n)
{
    unsigned h = n;

    while (n--)
        h = h * 31 + (u8)*t++;
    return h;
}

/* O texto do editor no campo. O indicativo e o destinatario vao em maiusculas (o editor tem o modo abc),
 * e so com o que o AX.25 e o APRS aceitam: letras e numeros, e o '-' do SSID no destinatario (v75; um
 * espaco ou uma '/' no endereco faziam decodificadores estritos descartarem o quadro). O numero da
 * mensagem anda quando o destinatario muda e a cada mensagem da caixa (v112: cada uma e nova; ate a v111,
 * so com o texto mudado, e o SEND repetia a mesma). Devolve se ha o indicativo e o campo, o que o envio
 * pede. */
__attribute__((noinline)) static unsigned aprs_guarda_texto(unsigned qual)
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
    if (qual == 2 || (qual && resumo_texto(t, n) != antes))
        a.id = a.id % 99 + 1;
    grava_aprs(&a);
    return a.tam_ind && n;
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

/* O indicativo gravado (v138): sem ele o quadro_aprs nao monta pacote, e o envio armado subia so a portadora.
 * Separada: o registro sai da pilha de quem chama. */
__attribute__((noinline)) NA_RESERVA static unsigned tem_indicativo(void)
{
    struct aprs a;

    le_aprs(&a);
    return a.tam_ind;
}

/* O set do CALLSIGN. Pedido pelo # da tela MESSAGES sem indicativo (v138, aprs_caixa_abre), ele grava sem fechar
 * o editor: o desvio do OK chama o aprs_caixa_abre de novo, que agora acha o indicativo e abre a caixa da
 * mensagem. */
void aprs_ind_grava(void)
{
    aprs_guarda_texto(0);
    if (TELA_APRS & CAIXA_MSG)
        return;
    editor_fecha();
}

/* O set do TO, que tambem e o das duas caixas da tela MESSAGES (v112): na caixa da mensagem, ela vai ao
 * registro e o envio fica armado (com o indicativo e a mensagem); na do TO, o destinatario, vazio inclusive.
 * Da caixa, sem a saida do editor de fabrica, que limparia a tela, e a volta fica pedida. */
void aprs_dest_grava(void)
{
    unsigned t = TELA_APRS;

    if (aprs_guarda_texto(t & CAIXA_MSG ? 2 : 1) && (t & CAIXA_MSG) && tem_indicativo())   /* v138: com pacote */
        ESTADO_MSG = ARMADO_APRS;
    if (!(t & CAIXA)) {
        editor_fecha();
        return;
    }
    TELA_APRS = (t & FILTRO_OFF) | VOLTA;
}

/* Depois do set, no desvio do OK (b_menu_marca, tela.c): o laco da tela MESSAGES saiu pelo #, e a caixa abre
 * no item TO da pasta, sem a limpeza da tela que o OK de fabrica faria (0x01E49B1C): o editor com o TO
 * gravado, ou com a ultima mensagem. O resto do estado e o da abertura do MESSAGES (0x01E49B20..0x01E49B32),
 * que o laco nao mexeu: editando, com o valor 0 (acao_le) e os campos do digitado zerados. Devolve 1 se
 * abriu. */
unsigned aprs_caixa_abre(void)
{
    unsigned c = TELA_APRS & CAIXA;

    if (!c)
        return 0;
    if (TELA_APRS & TELA_MORSE) {          /* v144: no proprio MESSAGES da pasta MORSE, o item 0 */
        MENU_TIPO = 7;
        morse_caixa_abre();
        return 1;
    }
    if (c == CAIXA_MSG && !tem_indicativo()) {   /* v138: sem indicativo, o CALLSIGN antes; gravado, a caixa */
        MENU_NUMERO = ITEM_CALLSIGN;
        MENU_TIPO = 7;
        aprs_texto_le(0);
        return 1;
    }
    MENU_NUMERO = ITEM_TO;
    MENU_TIPO = 7;
    aprs_texto_le(c == CAIXA_TO ? 1 : 2);  /* a mensagem vem com a ultima, como a do Morse (v149) */
    return 1;
}

/* ---------- o menu de canais, CH MEMORY (v140, como no UV-17L) ----------
 * CH NAME, DELETE e SAVE sao listas de canais de verdade: so os salvos, ou so os livres no SAVE, com "102 - PIGAO 3"
 * (o numero, no SAVE), abrindo no canal do lado em evidencia ou no mais perto da lista (no empate, o de numero
 * maior). O motor de fabrica continua desenhando: o item e um numero com exibicao propria, e o VALOR e o canal.
 *
 * - O que muda e o passo: as setas passam pelo lista_seta (gancho_seta_mais e menos, tela.c), que pula o que nao esta
 *   na lista, dando a volta. O motor pede as linhas em volta do VALOR, e o lista_exibe traduz a distancia em passos
 *   da lista. O numero digitado (o de fabrica, DIGITO_MENU) vai ao VALOR como veio: lista_perto o acerta no desenho,
 *   nas setas e no set, e o VALOR fica como esta para o digito seguinte fazer a dezena (0x01E4A29A).
 * - A lista le o chip, nao os mapas de canais (0x15D8B): eles sao filtrados pelos bancos ligados, e o SAVE mostraria
 *   como livre um canal ocupado. Sao 4 B por canal, e so ate achar o vizinho.
 * - No modo GMRS (b[0x153A1] = 165; byte 0xD000 do chip) o MEMCH e o DELCH recusam os canais 1 a 30: o SAVE nao os
 *   mostra, e o DELETE segue a recusa de fabrica. O CH NAME vale em qualquer canal, como de fabrica.
 * - O MENU_TIPO diz o passo: 4 a lista, 7 o editor e 8 o aviso ou a pergunta do DELETE (como o VERSION, so leitura: o
 *   tipo 8 e o que le_menu e gancho_menu_digito ja tratam), o aviso com o VALOR em CANAIS e a pergunta com o canal.
 *   O VALOR fica no canal durante o editor: as setas nao o mexem (lista_seta) e o digito vai ao texto. O SAVE e o CH
 *   NAME so gravam no OK do editor, e cancelar volta a lista (lista_cancela), sem nada no chip.
 * - Os BANKS (v141) sao mais quatro listas: as dos 16 bancos (o ACTIVE, a escolha do CHANNELS e o NAME) e a dos canais
 *   salvos do banco aberto. O que e do chip esta no bancos.c. */
#define CANAIS         960u
#define ITEM_CH_NAME   0                    /* na pasta CH MEMORY (conferir.py, 3b) */
#define ITEM_DELETE    1
#define ITEM_SAVE      2
#define ITEM_NO_BANCO  3                    /* os salvos, com os do banco aberto em ambar */
#define ITEM_ACTIVE    4                    /* daqui em diante, pela ordem da pasta BANKS (3b), os 16 bancos */
#define ITEM_ESCOLHE   5
#define ITEM_NOME      6
#define BANCOS         16u
#define AVISO_SALVE    CANAIS               /* o VALOR no aviso (aviso_cena, telas.c) */
#define AVISO_VAZIO    (CANAIS + 1)
#define AVISO_CHEIA    (CANAIS + 2)
#define AVISO_VFO      (CANAIS + 3)
#define AVISO_BANCO    (CANAIS + 16)        /* mais o banco: o aviso dentro da lista, que volta a ele */
#define MARCA_LISTA    8                    /* as marcas do item (telas.c) */
#define MARCA_BANKS    16
#define MARCA_VFO      32
#define MARCA_CANAL    64
#define TIPO_LISTA     4                    /* o do item na tabela */
#define TIPO_EDITOR    7
#define TIPO_AVISO     8
#define TABELA_RAPIDA  0x01E5E5ACu          /* o menu rapido do 3 segurado: o SAVE, sozinho */
#define LINHA_BASE     RAM(u16, 0x00014C18) /* h[716]: editando, o VALOR de que o motor parte */
#define GRAVADO        RAM(u32, 0x00014C14) /* [357]: fora da edicao, o que o get devolveu */
#define LINHA_CURSOR   RAM(u8, 0x00014C10)
#define MODO_LADOS     RAM(u8, 0x00014B86)  /* um nibble por lado, 1 = canal */
#define LADO_MENU      (RAM(u8, 0x00015E04) != 0)
#define TITULO_MENU    ((char *)&RAM(u8, 0x00015494))
#define BANCO_ABERTO   RAM(u8, 0x00014BF3)  /* no CHANNELS, o banco aberto + 1; 0 escolhendo */
FIRMWARE(DELCH_FABRICA, 0x01E3C516);  void DELCH_FABRICA(void);
FIRMWARE(RECARREGA_LADOS, 0x01E3A812); void RECARREGA_LADOS(void);
FIRMWARE(GRAVA_CANAL, 0x01E3A2BA);    void GRAVA_CANAL(unsigned, const void *, const void *);   /* 20 B + 12 B */
FIRMWARE(MOTOR_MENU, 0x01E489E0);     u32 MOTOR_MENU(void);
FIRMWARE(TEMPO_MENU, 0x01E484F8);     void TEMPO_MENU(void);
int existe_no_registro(const u8 *p);
void memch_grava(const char *nome);
unsigned marcas_do_item(void);
void banco_padrao(unsigned n, char *s);
void banco_nome(unsigned n, char *s);
char *banco_texto(unsigned n, unsigned ativo);
char *banco_resumo(void);
unsigned banco_tem(unsigned b, unsigned canal);
unsigned banco_liga(unsigned n);
void banco_refiltra(void);
void banco_canal(unsigned k, unsigned canal);
void banco_nomeia(unsigned n);
unsigned le_nome(unsigned n, char *destino);

NOS_CAMINHOS unsigned lista_item(void)
{
    if (MENU_TABELA == TABELA_RAPIDA)
        return ITEM_SAVE;
    if (!(marcas_do_item() & MARCA_BANKS))
        return MENU_NUMERO;
    return BANCO_ABERTO && MENU_NUMERO == ITEM_ESCOLHE - ITEM_ACTIVE ? ITEM_NO_BANCO : ITEM_ACTIVE + MENU_NUMERO;
}

/* O lado em evidencia esta num canal? O aviso do CH NAME em VFO e o "---" da pasta. */
NOS_CAMINHOS unsigned em_canal(void)
{
    return (MODO_LADOS >> (LADO_MENU * 4) & 15) == 1;
}

NOS_CAMINHOS static unsigned na_lista(unsigned n, unsigned item)
{
    u8 b[4];
    unsigned salvo;

    if (item > ITEM_NO_BANCO)
        return n < BANCOS;
    LE_FLASH(n << 5, b, 4);
    salvo = existe_no_registro(b);
    return item == ITEM_SAVE ? !salvo && !(RAM(u8, 0x000153A1) == 165 && n < 30) : salvo;
}

/* O da lista mais perto de n, ele incluido (fora da faixa, parte do 0); CANAIS se a lista esta vazia. */
NOS_CAMINHOS static unsigned lista_perto(unsigned n, unsigned item)
{
    unsigned d;

    if (n >= CANAIS)
        n = 0;
    for (d = 0; d < CANAIS; d++) {
        if (n + d < CANAIS && na_lista(n + d, item))
            return n + d;
        if (n >= d && na_lista(n - d, item))
            return n - d;
    }
    return CANAIS;
}

/* O vizinho na lista, dando a volta; n, se ele esta sozinho nela. */
NOS_CAMINHOS static unsigned lista_passo(unsigned n, int sentido, unsigned item)
{
    unsigned k, c = n;

    for (k = 0; k < CANAIS; k++) {
        c = sentido > 0 ? (c + 1 == CANAIS ? 0 : c + 1) : (c ? c - 1 : CANAIS - 1);
        if (na_lista(c, item))
            return c;
    }
    return n;
}

NOS_CAMINHOS static char *tres_digitos(char *p, unsigned v)
{
    p[0] = (char)('0' + v / 100);
    p[1] = (char)('0' + v / 10 % 10);
    p[2] = (char)('0' + v % 10);
    return p + 3;
}

/* "102 - PIGAO 3"; sem nome, a frequencia; no SAVE, so o numero. O do banco aberto, com o LIGADO no fim (telas.c). */
NOS_CAMINHOS static char *lista_texto(unsigned n, unsigned item)
{
    char *p;
    u8 b[4];
    int k = 0;

    if (item > ITEM_NO_BANCO)
        return banco_texto(n, item == ITEM_ACTIVE);
    p = tres_digitos(TEXTO, n + 1);
    if (item != ITEM_SAVE) {
        *p++ = ' ';
        *p++ = '-';
        *p++ = ' ';
        LE_FLASH((n << 5) + 20, p, 12);
        while (k < 12 && p[k] && (u8)p[k] != 0xFF)
            k++;
        p += k;
        if (!k) {
            LE_FLASH(n << 5, b, 4);
            for (k = 8; k--;) {
                *p++ = (char)('0' + (b[k >> 1] >> (k & 1 ? 4 : 0) & 15));
                if (k == 5)
                    *p++ = '.';
            }
        }
    }
    if (item == ITEM_NO_BANCO && banco_tem(BANCO_ABERTO - 1u, n))
        *p++ = 1;
    *p = 0;
    return TEXTO;
}

/* O get: o canal do lado em evidencia (em VFO, o ultimo dele) ou o mais perto da lista; os bancos abrem no primeiro. */
NOS_CAMINHOS static unsigned lista_inicio(void)
{
    unsigned item = lista_item();

    return item > ITEM_NO_BANCO ? 0 : lista_perto(RAM(u16, 0x00015E0C + LADO_MENU * 4), item);
}

u16 lista_le(void)
{
    return (u16)lista_inicio();
}

/* O exibe, uma chamada por linha, com a linha no VALOR. Editando, a linha do cursor e a da lista_perto, e as outras a
 * tantos passos dela quanto o motor pede (com dois na lista, o outro so embaixo; com um, so ele). Na janela da
 * pasta so vale a marcada, e em VFO o CH NAME nao mostra canal: o da lista nao seria o sintonizado. O ACTIVE mostra
 * quantos estao ligados. */
NA_RESERVA static char *lista_linha(void)
{
    unsigned item = lista_item(), a, n, k;
    int d = 0;

    if (MENU_EDITANDO != 1 && VALOR != GRAVADO)
        return "";
    if (MENU_EDITANDO != 1 && item > ITEM_NO_BANCO)
        return banco_resumo();
    a = lista_perto(MENU_EDITANDO == 1 ? LINHA_BASE : VALOR, item);
    if (a >= CANAIS || (item == ITEM_CH_NAME && !em_canal()))
        return "---";
    if (MENU_EDITANDO == 1)
        d = (short)(VALOR - LINHA_BASE);
    n = a;
    for (k = d < 0 ? -d : d; k--;) {
        n = lista_passo(n, d, item);
        if (n == a)
            return "";
    }
    if (d == -1 && n == lista_passo(a, 1, item))
        return "";
    return lista_texto(n, item);
}

char *lista_exibe(void)
{
    return lista_linha();
}

/* O item nao pode abrir agora? Devolve o aviso com que ele abre (menu_ok_item, tela.c), ou 0: o que so vale num modo,
 * como no UV-17L, e a lista vazia. Abrir e tambem o que fecha o banco que tenha ficado aberto. */
NA_RESERVA unsigned lista_aviso(void)
{
    unsigned m = marcas_do_item(), item;

    if (m & MARCA_VFO)
        return em_canal() ? AVISO_VFO : 0;
    if (m & MARCA_CANAL)
        return em_canal() ? 0 : AVISO_SALVE;
    BANCO_ABERTO = 0;
    item = lista_item();
    if (item == ITEM_CH_NAME)
        return em_canal() ? 0 : AVISO_SALVE;
    if (lista_perto(0, item == ITEM_ESCOLHE ? ITEM_DELETE : item) < CANAIS)
        return 0;
    return item == ITEM_SAVE ? AVISO_CHEIA : AVISO_VAZIO;
}

/* A seta com a lista aberta: anda so na lista; no editor, no aviso e na pergunta nao faz nada. */
NOS_CAMINHOS u32 lista_seta(int sentido)
{
    unsigned item = lista_item();

    TEMPO_MENU();
    if (MENU_TIPO == TIPO_LISTA) {
        VALOR = (u16)lista_passo(lista_perto(VALOR, item), sentido, item);
        RAM(u16, 0x00014BFE) = 0;           /* o digitado, como as setas de fabrica (0x01E49FE6) */
        RAM(u32, 0x00014C04) = 0;
        LINHA_CURSOR = 1;
    }
    return MOTOR_MENU();
}

NA_RESERVA static void lista_titulo(const char *t)
{
    unsigned k;

    for (k = 0; (TITULO_MENU[k] = t[k]) != 0; k++)
        ;
}

/* O EXIT na pergunta e no aviso de dentro da lista volta a lista, e no editor vazio (ou segurado) tambem, sem gravar
 * nada; nos canais de um banco, volta aos bancos, nele. O aviso de abertura e a lista sem banco aberto ficam para o
 * EXIT de fabrica. Quem chama redesenha: o motor aqui dentro passaria do limite da pilha da interface. */
NA_RESERVA u32 lista_cancela(void)
{
    if (MENU_TIPO == TIPO_EDITOR) {
        unsigned item = lista_item();

        editor_fecha();
        lista_titulo(item == ITEM_SAVE ? "SAVE" : item == ITEM_NOME ? "NAME" : "CH NAME");
    } else if (MENU_TIPO == TIPO_LISTA) {
        if (!BANCO_ABERTO)
            return 0;
        VALOR = BANCO_ABERTO - 1u;
        BANCO_ABERTO = 0;
        lista_titulo("CHANNELS");
    } else if (MENU_TIPO != TIPO_AVISO || (VALOR >= CANAIS && VALOR < AVISO_BANCO))
        return 0;
    else if (VALOR >= AVISO_BANCO)
        VALOR -= AVISO_BANCO;
    LINHA_CURSOR = 1;
    MENU_TIPO = TIPO_LISTA;
    TEMPO_MENU();
    return 1;
}

/* Depois do set, no desvio do OK (b_menu_marca, tela.c, a marca MARCA_NOMEIA): da lista, abre o editor do canal, com
 * o estado do OK de fabrica (0x01E49B20..0x01E49B32) e o canal no titulo; o SAVE vazio, o CH NAME com o nome; o NAME
 * com o nome dado, vazio se o banco nao tem, e o nome de fabrica no titulo. Do OK do editor, que ja gravou, devolve
 * 0, e a edicao fecha. */
NOS_CAMINHOS unsigned lista_editor_abre(void)
{
    unsigned n = VALOR, k = 0, item = lista_item();

    if (MENU_TIPO == TIPO_EDITOR) {
        MENU_TIPO = TIPO_LISTA;
        return 0;
    }
    if (item == ITEM_NOME)
        k = le_nome(n, TEXTO);
    else if (item == ITEM_CH_NAME) {
        LE_FLASH((n << 5) + 20, TEXTO, 12);
        while (k < 12 && TEXTO[k] && (u8)TEXTO[k] != 0xFF)
            k++;
    }
    MENU_TIPO = TIPO_EDITOR;
    RAM(u16, 0x00014BFE) = 0;
    RAM(u32, 0x00014C04) = 0;
    RAM(u8, 0x00014C08) = 0;
    editor_abre(TEXTO, k, 12);
    if (item == ITEM_NOME) {
        banco_padrao(n, TITULO_MENU);
        return 1;
    }
    TITULO_MENU[0] = 'C';
    TITULO_MENU[1] = 'H';
    TITULO_MENU[2] = ' ';
    *tres_digitos(TITULO_MENU + 3, n + 1) = 0;
    return 1;
}

/* O nome do editor, em 12 B, ou "CH 101" com ele em branco (o OK sem nome, so espacos inclusive). */
NOS_CAMINHOS static void nome_do_editor(char *s, unsigned n)
{
    unsigned k, t = editor_texto(s, 12), vazio = 1;

    for (k = 0; k < 12; k++) {
        if (k >= t)
            s[k] = 0;
        else if (s[k] != ' ')
            vazio = 0;
    }
    if (vazio) {
        s[0] = 'C';
        s[1] = 'H';
        s[2] = ' ';
        *tres_digitos(s + 3, n + 1) = 0;
        for (k = 6; k < 12; k++)
            s[k] = 0;
    }
}

/* Os sets. Da lista, o CH NAME e o SAVE so firmam o canal (o editor abre no desvio do OK), e o DELETE pergunta; do
 * editor, gravam; da pergunta, apaga, e a lista segue no mais perto, para apagar varios; sem nenhum, o aviso. */
NOS_CAMINHOS void chname_grava(void)
{
    char nome[12];
    u8 cru[20];
    unsigned n = VALOR;

    if (MENU_TIPO != TIPO_EDITOR) {
        VALOR = (u16)lista_perto(n, ITEM_CH_NAME);
        return;
    }
    nome_do_editor(nome, n);
    LE_FLASH(n << 5, cru, 20);
    GRAVA_CANAL(n, cru, nome);
    RECARREGA_LADOS();
    editor_fecha();
}

void save_grava(void)
{
    char nome[12];

    if (MENU_TIPO != TIPO_EDITOR) {
        VALOR = (u16)lista_perto(VALOR, ITEM_SAVE);
        return;
    }
    nome_do_editor(nome, VALOR);
    memch_grava(nome);
    editor_fecha();
}

NOS_CAMINHOS void delete_grava(void)
{
    unsigned n = lista_perto(VALOR, ITEM_DELETE);

    if (MENU_TIPO != TIPO_AVISO) {
        VALOR = (u16)n;
        MENU_TIPO = TIPO_AVISO;
        return;
    }
    MENU_TIPO = TIPO_LISTA;
    DELCH_FABRICA();
    banco_refiltra();
    VALOR = (u16)lista_perto(VALOR, ITEM_DELETE);
    if (VALOR >= CANAIS) {
        VALOR = AVISO_VAZIO;
        MENU_TIPO = TIPO_AVISO;
    }
}

/* Os sets dos BANKS. Na lista, o numero digitado vale pelo mais perto, como nas de canais. */
NA_RESERVA void ativo_grava(void)
{
    unsigned n = lista_perto(VALOR, ITEM_ACTIVE);

    VALOR = (u16)n;
    if (!banco_liga(n)) {
        MENU_TIPO = TIPO_AVISO;
        LINHA_CURSOR = 0;
        VALOR = (u16)(AVISO_BANCO + n);
    }
}

NA_RESERVA void canais_grava(void)
{
    unsigned n;

    if (BANCO_ABERTO) {
        VALOR = (u16)lista_perto(VALOR, ITEM_NO_BANCO);
        banco_canal(BANCO_ABERTO - 1u, VALOR);
        return;
    }
    n = lista_perto(VALOR, ITEM_ESCOLHE);
    BANCO_ABERTO = (u8)(n + 1);
    banco_nome(n, TITULO_MENU);
    VALOR = (u16)lista_inicio();
}

NA_RESERVA void nome_grava(void)
{
    if (MENU_TIPO != TIPO_EDITOR) {
        VALOR = (u16)lista_perto(VALOR, ITEM_NOME);
        return;
    }
    banco_nomeia(VALOR);
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
 * cada aviso de quase cheia, olhando o pedido de interrupcao (reg 0x0C bit 0) a cada 20 ms.
 *
 * O apito e o estalo (v123). Ate a v122 o laco lia o 0x0C logo depois de cada tique de 2 ms (o ESPERA_2MS), e
 * cada leitura leva 1,36 ms no BJ: o barramento do BK4819 ocupado 2/3 do tempo, num ritmo de 2 ms, vazava para o
 * audio demodulado - um apito de 500 Hz com os harmonicos (1000, 1500, 2000 e 2500 Hz, em cima dos tons do APRS),
 * medido numa gravacao do dono com o RX GAIN em MIN e o FILTER ligado, que a tela principal nao tem. O rearme
 * esperando o sync (4 escritas, 5,4 ms) dava um estalo a cada ~60 ms. Agora o 0x0C e lido a cada 20 ms (o aviso
 * vem com 4 palavras, ~53 ms antes de a FIFO encher) e o rearme esperando sai a cada 100 ms (a cada 40 desde a
 * v128). O estalo de 60 ms era, na verdade, o AGC automatico (v128, no filtro_rx).
 *
 * A polaridade (v71). Numa flag em NRZI um tom fica 7 bits parado e o outro aparece 1 bit, e qual e qual
 * depende do transmissor. O chip so sincroniza no padrao do 0x5A: com FE FE (parado na marca, 1200 Hz), um
 * WAV com as duas polaridades alternadas so fez sync nas paradas em 1200 Hz, e 95% do trafego real nem
 * mexia no contador (v70). Esperando, o laco troca a cada 40 ms entre FE FE e 01 01 (parado no espaco): um
 * preambulo de ~55 ms ja pega as duas. (Ate a v122, 50 ms; da v124 a v127, ~120 ms, porque o rearme so sai numa
 * leitura do 0x0C, a cada 20 ms, e um preambulo mais curto que ~150 ms comecado na polaridade errada se perdia.)
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
 * entrada da CALIBRATION > MODEM GAIN nos bits 9:8 (v73, ganho_modem, abaixo). Na v70 a banda larga so dava sync sem
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
 * volta nas duas pontas), o OK abre o de cima inteiro (e fecha), o * apaga a tela
 * (v75; a tecla seguinte so a acende), o EXIT segurado apaga o historico (v71), o EXIT curto fecha o
 * aberto ou sai, o # sai pedindo a caixa da mensagem, ou a do TO se ficou apertado 0,6 s (v112, a caixa
 * em gancho_teclas), e as outras teclas saem. A mensagem para mim com numero sai da tela
 * sozinha, para o ack ir ao ar (v75, guarda), e a tela volta depois dele (v112). Na barra de baixo, o P:
 * quantos syncs o modem achou nas duas polaridades, desde a entrada (o BAD e o FIX, o FCS errado e o que
 * a correcao salvou, so no num, ate a v111 na tela). O ganho do modem e o do MODEM GAIN (v103: o # que o
 * trocava saiu), e a cadeia de recepcao entra filtrada para o APRS e sai como estava (filtro_rx, v102 e
 * v103); de volta da caixa ou do envio (v112), o FILTER fica como estava. */
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
 * quantos velhos ha nele dos bits 2 em diante). Da v70 a v74, versoes de teste nunca publicadas, o
 * historico era de um bloco so ("HST1" e "HST2"); a v75 o apagava na primeira entrada, e a v111 tirou isso. */
#define MARCA_HIST   0x33545348u           /* "HST3" */
#define POR_SETOR    31u
#define USADO        0x5A
#define ANEL_SUJO    2u                    /* o setor que nao recebe tem cabecalho: precisa ser apagado */
#define ANEL_VELHOS(a)  ((a) >> 2)
#define DUPLICADO_MS 30000u                /* a mesma estacao com o mesmo texto: so a primeira entra */
#define EXIT_LONGO   100                   /* o EXIT segurado 0,6 s */
#define CERQ_LONGO   101                   /* o # segurado 0,6 s (v114) */
#define MORSE        64u                   /* v144: lendo Morse; o mesmo bit no aberto do tela_aprs */
#define ABERTO       1u                    /* os bits de modo, no laco */
#define PRESO        2u                    /* uma tecla esta apertada: so conta de novo depois de solta */
#define ESPERA_EXIT  4u                    /* o EXIT apertado: curto ou segurado, so se sabe depois */
#define ESPERA_CERQ  32u                   /* o # apertado, do mesmo jeito (v114) */
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
const char titulo_aprs_decode[] = "DECODE";
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
const char *const modem_ganho_opcoes[4] = {"OFF", "LOW", "MID", "HIGH"};   /* MID como os outros (v82) */

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

/* FILTER (v102; sempre ligado desde a v103, com a etiqueta no alto da tela do APRS RX): a cadeia de
 * recepcao como o APRS precisa, sem herdar o canal - o laco so programava o modem, e o resto vinha de quem
 * sintonizou. A entrada liga: FM (0x01E440C4, a troca de AM e FM de fabrica, que tambem grava o 0x42, o 0x2A
 * e o 0x2B), sem scramble nem compressor/expansor (bits 1 e 3 do 0x31), o filtro largo do espectro
 * (FILTRO(0), 0x43 = 0x49A8: 3,75 kHz no sinal forte e no fraco, dobrados, o largo do firmware da comunidade
 * do UV-K5), o expansor e a porta de ruido desligados (0x28 = 0x0A00) e, no 0x2B, os filtros de audio do RX
 * de fabrica com o cancelamento de corrente continua depois do demodulador (bit 14, que vem ligado no chip
 * e a fabrica desliga em FM; v103) e, a pedido do dono (v111), o EQ do RX com os graves em -3 dB e os agudos
 * em +4 dB (os passos da ponta da tabela do bancos.c), que no teste do dono decodificou mais. A saida devolve o
 * que estava: o modo, o 0x31 e o 0x43 que a entrada leu (era3), pelo aplica_audio (bancos.c) o 0x28, o bit 3
 * do 0x31 e, em FM, o 0x2B dos itens das pastas AUDIO e MIC (em AM, o 0x2B da troca de fabrica), e pelo
 * aplica_eq o EQ do menu.
 * O 0x2F (v123): ligado, o do UV-17L (F49B, o padrao do chip), que sobe o audio demodulado; a saida devolve o de
 * fabrica (9890, que so a partida grava, em 0x01E56620). Dos registradores que diferem entre os dois na tela
 * MESSAGES (o UV le ~9 de cada 10 pacotes na mesma frequencia, o BJ 1 ou 2), foi o unico que melhorou a leitura,
 * testados um por um nas versoes de diagnostico v116 a v120 (nao lancadas): o 0x1D do UV poe um zumbido, o 0x37
 * cala o receptor (o BJ precisa do regulador do PLL desviado, v122) e os outros nao mudaram nada.
 * O AGC travado (v128): o estalo a cada 60,50 ms (3 x 2^19 ciclos dos 26 MHz do cristal do BK4819, com qualquer
 * radio e em qualquer tela, e que atravessa os pacotes sem perder o compasso) e o AGC automatico trocando de
 * degrau, que a placa do BJ, com o PLL sem o regulador, ouve. Ligado, o AGC fica fixo no degrau de cima (0x7E bit
 * 15, o degrau nos bits 14:12: B03E), o mesmo em que ele fica com o sinal fraco da rede; a saida devolve o 303E, que
 * so a partida grava (o espectro guarda e devolve o 0x7E). Nas v126 e v127 (diagnostico, nao lancadas) o estalo
 * sumiu com o AGC fixo e nao com o AFC desligado; e nem o AGC, nem o AFC, nem os 0x7E, 0x77, 0x1F, 0x3E e 0x1E do UV
 * fizeram o BJ ler a rede, que o UV, ao lado, le quase inteira. */
FIRMWARE(AM_FM, 0x01E440C4);        void AM_FM(unsigned am);      /* 1 AM, o resto FM */
FIRMWARE(FILTRO, 0x01E4A980);       void FILTRO(unsigned);        /* o 0x43 do espectro: 0 o largo */
void aplica_audio(void);
void aplica_eq(void);
__attribute__((noinline)) static void filtro_rx(unsigned liga, unsigned era3)
{
    if (liga) {
        AM_FM(0);
        REG_W(0x2F, 0xF49B);
        REG_W(0x31, REG_R(0x31) & ~0x0Au);
        FILTRO(0);
        REG_W(0x28, 0x0A00);
        REG_W(0x2B, 0x4000);
        REG_W(0x54, 0x935A);               /* os graves do RX em -3 dB */
        REG_W(0x55, 0x2EFF);
        REG_W(0x75, 0xCC35);               /* os agudos do RX em +4 dB */
        /* o AGC travado no degrau de cima (v128) e o filtro de DC do RX desligado (bits 2:0 = 000, v137: com
         * os bits crus lidos pelo cabo, 3 quadros perfeitos e 1 quase em 7 envios do UV, contra ~27% com ele) */
        REG_W(0x7E, 0xB038);
        return;
    }
    AM_FM(era3 >> 16 & 1);
    REG_W(0x7E, 0x303E);
    REG_W(0x2F, 0x9890);
    REG_W(0x31, era3 >> 16);
    REG_W(0x43, era3 & 0xFFFF);
    aplica_audio();
    aplica_eq();
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

/* Apaga o historico (o EXIT segurado na tela): os dois setores, e o primeiro recomeca. */
static int hist_limpa(unsigned *anel)
{
    APAGA_SETOR(HIST_SETOR(0));
    APAGA_SETOR(HIST_SETOR(1));
    setor_novo(0, 0);
    *anel = 0;
    return 0;
}

/* Abre o historico: os dois cabecalhos dizem qual setor recebe (a geracao mais nova) e se ha velhos no
 * outro. So usa setor apagado ou ja nosso: com outra coisa num deles, devolve -1 e nada e gravado. O do Morse
 * (v147) apaga e recomeca: a faixa e livre no mapa da secao 3, e o que sobra nela sao capturas de diagnostico,
 * como as RAW1 da v92, que o faziam jogar fora tudo o que lia. Fora do laco (v111): colada nele, as variaveis
 * dela iam para o quadro do laco, e a pilha passava. */
__attribute__((noinline)) static int hist_abre(unsigned *anel)
{
    u32 m[2];
    u8 g[2];
    unsigned s, cur, velhos;

    for (s = 0; s < 2; s++) {
        LE_FLASH(HIST_SETOR(s), &m[s], 4);
        LE_FLASH(HIST_SETOR(s) + 4, &g[s], 1);
    }
    for (s = 0; s < 2; s++)
        if (m[s] != MARCA_HIST && m[s] != 0xFFFFFFFFu)
            return TELA_APRS & TELA_MORSE ? hist_limpa(anel) : -1;
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
NA_RESERVA static u32 hist_endereco(int hist, unsigned anel, int i)
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
 * seguinte ocupar o q. O n_meu traz a sessao nos bits 23:16 (v138), que vai para o sobra[0]. */
__attribute__((noinline)) static void guarda(int *hist, unsigned *anel, const u8 *q, unsigned n,
                                             const char *meu, unsigned n_meu)
{
    struct registro_rx *r = REGISTRO_RASCUNHO;
    unsigned i, k = 0;
    u32 e;

    for (i = 0; i < REG_HIST; i++)
        ((u8 *)r)[i] = 0xFF;
    r->usado = USADO;
    r->ambar = aprs_texto(r->texto, q, n, meu, (n_meu & 0xFFFFu) | (DECODE_APRS == DECODE_OFF ? TEXTO_BRUTO : 0),
                          r->id);
    r->sobra[0] = n_meu >> 16;             /* a sessao (v138) */
    r->texto[TEXTO_MAX] = 0;               /* um "/A=" no fim passa do limite (e cai no para, refeito abaixo): v128 */
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

#define MORSE_NUMERO  (1200u / MRX->un)
/* Direto na tela, sem rotina no meio: o quadro dela pesaria na pilha do laco. Lendo Morse, o que esta chegando vai
 * em cima dos outros, lido da RAM, e o P da lugar a velocidade. */
#define RX_MOSTRA(faixas) \
    (num[1] = topo + 1, num[2] = hist < 0 ? 0 : hist, \
     n = modo & MORSE ? (num[3] = MORSE_NUMERO, mrx_vale(MRX)) : 0, \
     tela_aprs(n && !(modo & ABERTO) ? (u32)(__SIZE_TYPE__)&MRX->usado : hist_endereco(hist, anel, hist - 1 - topo), \
               hist_endereco(hist, anel, hist - 2 - topo + n), modo & (ABERTO | MORSE), num, (faixas)))

/* Uma amostra da leitura do Morse: a mensagem que acaba entra no historico como a do APRS. O estado da leitura comeca o
 * registro (usado, ambar e texto): o resto dele, que so o APRS le, vai como estiver. */
#define MRX  ((struct morse_rx *)q)
__attribute__((noinline)) NA_RESERVA static unsigned morse_passo(struct morse_rx *m, int *hist, unsigned *anel)
{
    unsigned e = mrx_amostra(m, TIQUE_MS, REG_R(0x6F) & 0x7F, rx_portadora());

    if (e & MRX_FIM) {
        hist_grava(hist, anel, (const struct registro_rx *)&m->usado);
        mrx_limpa(m);
    }
    return e;
}

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
    /* o comeco do quadro: os enderecos e ~100 de info, o que a tela mostra (160 na v74; 136 pela pilha do historico,
     * v75); lendo Morse (v144), o estado da leitura, que cabe nele */
    u8 q[136] __attribute__((aligned(4)));
    struct hdlc h;
    char meu[10];
    u16 num[5];                            /* FILTER desligado, posicao, quantos, syncs na marca e no espaco */
    unsigned f, n, k, w = 0, n_meu = meu_indicativo(meu), ativo = 0, pol = 0, desde = TIQUE_MS, olha = 0;
    unsigned sync_em = 0;                  /* o ultimo sync (v137): a tela espera 2 s depois dele */
    unsigned pede = 15, modo = PRESO | (TELA_APRS & TELA_MORSE ? MORSE : 0), anel = 0, desde_tecla = 0;   /* o OK que
                                            * abriu ainda esta apertado */
    /* os registradores (de 16 bits) que a saida devolve, dois por variavel: a pilha do laco esta no limite */
    unsigned era1 = REG_R(0x3F) | REG_R(0x70) << 16, era2 = REG_R(0x72) | REG_R(0x5E) << 16;
    unsigned era3 = REG_R(0x43) | REG_R(0x31) << 16;   /* o canal, para o FILTER (v102) */
    unsigned era_save = SAVE;              /* a economia de bateria fica desligada na tela (v111) */
    int hist = hist_abre(&anel), topo = 0;

    /* A sessao (v138, achado da auditoria; o UV-17L faz igual): o registro guarda no sobra[0] a sessao em que
     * entrou, e o registro_igual compara o registro inteiro, entao uma copia so e descartada dentro da mesma
     * sessao - a hora e o ms desde a partida, e sem isso um registro de outra partida com a hora parecida
     * bloqueava o mesmo pacote. A sessao e a do ultimo registro, mais 1 numa entrada nova; na volta da caixa ou
     * do ack ela continua, para a copia do digipeater depois do ack seguir descartada. Ela vai nos bits 23:16
     * do modo, e o num[1] serve de buffer (ele so e escrito no desenho): a pilha do laco esta no limite. */
    num[1] = 0;
    if (hist > 0)
        LE_FLASH(hist_endereco(hist, anel, hist - 1) + __builtin_offsetof(struct registro_rx, sobra), &num[1], 1);
    modo |= (u8)(num[1] + !(TELA_APRS & VOLTA)) << 16;
    num[0] = volta_fim();                  /* de volta da caixa ou do envio (v112): o FILTER como estava */
    SAVE = 0;
    ACORDA();
    DESARMA_DW();
    num[3] = num[4] = 0;
    RX_FECHA();                            /* o laco de fabrica, que o abriria, nao roda aqui */
    if (modo & MORSE) {
        TELA_APRS = TELA_MORSE;            /* o historico dele, ate sair */
        mrx_comeca(MRX, morse_wpm_le() + 5);
    } else {
        REG_W(0x70, 0x00E0);               /* o FSK ligado, com o ganho do roger TONE1200 */
        REG_W(0x72, 0x3065);               /* 1200 bps */
        REG_W(0x58, MODEM_RX | ganho_modem() << 8);
        REG_W(0x5C, 0xAA30);               /* sem o CRC do chip */
        REG_W(0x5D, 0xFFE0);               /* 2048 bytes */
        REG_W(0x5E, 0x0204);               /* quase cheia com 4 palavras */
        REG_W(0x3F, 0x3002);               /* avisos: sync, quase cheia, fim */
        REG_W(0x02, 0);
        filtro_rx(!num[0], era3);          /* desligado, a saida dele (o canal e os menus) nao muda nada */
        fsk_rx_arma(0);
        hdlc_comeca(&h, q, sizeof q);
    }
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
            else if (modo & ESPERA_CERQ)
                f = TECLA_CERQUILHA;
            modo &= ~(PRESO | ESPERA_EXIT | ESPERA_CERQ | REPETE);
        } else if (!(modo & PRESO)) {
            modo |= PRESO;
            desde_tecla = TIQUE_MS;
            if (modo & ESCURO) {
                LUZ(1);
                modo &= ~ESCURO;
                pede = 15;
            } else if (k == TECLA_EXIT) {
                modo |= ESPERA_EXIT;
            } else if (k == TECLA_CERQUILHA) {
                modo |= ESPERA_CERQ;
            } else {
                f = k;
                if (k == TECLA_CIMA || k == TECLA_BAIXO)
                    modo |= REPETE;
            }
        } else if ((modo & (ESPERA_EXIT | ESPERA_CERQ)) && TIQUE_MS - desde_tecla >= 600) {
            f = modo & ESPERA_EXIT ? EXIT_LONGO : CERQ_LONGO;
            modo &= ~(ESPERA_EXIT | ESPERA_CERQ);
        } else if ((modo & REPETE) && TIQUE_MS - desde_tecla >= REPETE_DEPOIS) {
            desde_tecla = TIQUE_MS - REPETE_DEPOIS + REPETE_CADA;
            f = k;
        }
        if (f) {
            if (f == TECLA_CIMA || f == TECLA_BAIXO) {
                if (hist > 0)              /* em volta: depois do mais velho, o mais novo, e o contrario */
                    topo = (topo + (f == TECLA_BAIXO ? 1 : hist - 1)) % hist;
            } else if (f == TECLA_OK) {
                if ((modo & ABERTO) || hist > 0)
                    modo ^= ABERTO;
            } else if (f == TECLA_ESTRELA) {
                LUZ(0);
                modo |= ESCURO;
            } else if (f == TECLA_ZERO && !(modo & MORSE)) {
                /* o FILTER (v111): desligado, o canal e os menus, como na saida; */
                filtro_rx(num[0], era3);   /* os contadores recomecam, para comparar os dois */
                num[0] ^= 1;
                num[3] = num[4] = 0;
            } else if (f == EXIT_LONGO) {  /* segurado: apaga o historico e fica */
                if (hist >= 0)
                    hist = hist_limpa(&anel);
                topo = 0;
                modo &= ~ABERTO;
            } else if (f == TECLA_EXIT && (modo & ABERTO)) {
                modo &= ~ABERTO;
            } else {                       /* o EXIT curto e as outras teclas saem; o # (v112), para a caixa: */
                if (modo & MORSE)          /* o # e a caixa dele, curto ou segurado (v144) */
                    TELA_APRS = f == TECLA_CERQUILHA || f == CERQ_LONGO ? CAIXA_MSG | TELA_MORSE : 0;
                else if (f == TECLA_CERQUILHA)   /* a da mensagem, ou a do TO segurado (v114: a 0,6 s, apertado) */
                    TELA_APRS = CAIXA_MSG;
                else if (f == CERQ_LONGO)
                    TELA_APRS = CAIXA_TO;
                break;
            }
            pede = 15;
            continue;
        }
        ESPERA_2MS(1);
        /* O 0x0C a cada 20 ms; entre as leituras, so a tela (uma faixa por volta). O rearme e o setor do
         * historico, so logo depois de uma leitura sem nada pendente (v128): da v124 a v127 o rearme rodava ate
         * 20 ms depois da ultima leitura, e um sync chegado nesse meio era apagado, com o pacote. A tela
         * espera os 2 s depois de um sync (v137, a regra da v129): o laco chegou a ficar 146 ms sem ler a FIFO
         * (8 palavras, ~107 ms), e uma faixa no comeco do quadro o perdia; a v129 leu 40-50% da rede, a v128
         * nada. */
        if (TIQUE_MS - olha < (modo & MORSE ? 5u : 20u)) {
            if (pede && !(modo & ESCURO) && !(ativo && TIQUE_MS - sync_em < 2000u)) {
                k = pede & -pede;
                pede ^= k;
                RX_MOSTRA(k);
            }
            continue;
        }
        olha = TIQUE_MS;
        if (modo & MORSE) {
            /* A tela anda por palavra (mrx_amostra): sobra tempo antes do tom seguinte. */
            f = morse_passo(MRX, &hist, &anel);
            if (f & MRX_FIM) {
                topo = 0;
                modo &= ~ABERTO;
            }
            if (f)
                pede = 15;
            if (MRX->n)
                continue;
            goto setor;
        }
        rx_portadora();
        if (!(REG_R(0x0C) & 1)) {
            if (TIQUE_MS - desde > (ativo ? 300u : 30u)) {
                /* esperando, a outra polaridade a cada 40 ms (a 2a leitura do 0x0C, de 20 em 20; o UV troca a cada
                 * 50); recebendo, a FIFO parou: volta a esperar */
                pol ^= !ativo;
                ativo = 0;
                fsk_rx_arma(pol);
                desde = TIQUE_MS;
                continue;
            }
            if (ativo)                     /* recebendo e sem aviso: o setor fica para o fim do proximo */
                continue;
            goto setor;
        }
        REG_W(0x02, 0);
        f = REG_R(0x02);
        if (f & 0x0002) {                  /* sync: as flags ja passaram, o quadro vem */
            hdlc_comeca(&h, q, sizeof q);
            h.dentro = 1;
            h.linha = pol ^ (REG_R(0x0B) >> 7 & 1);     /* a flag acaba no tom do bit sozinho */
            ativo = 1;
            sync_em = desde = TIQUE_MS;
            num[3 + pol]++;
            pede |= 8;
        }
        if (f & 0x1000) {
            desde = TIQUE_MS;
            /* as 4 palavras bit a bit: o byte baixo primeiro, o bit alto de cada byte primeiro (a ordem do
             * TX do chip, que o TA1JS usou no UV-K5 com o aprs.fi decodificando, e que o beacon da v65
             * provou). Depois da 4a leitura, o aviso que nasceu durante as leituras (a palavra que entrou) e
             * velho: sem limpa-lo, a proxima leitura achava menos de 4 e lia a FIFO vazia, lixo no hdlc_bit
             * (v124 a v127, achado da auditoria; v128); so o fim do quadro do chip fica. */
            for (k = 0; k < 64; k++) {
                if (!(k & 15)) {
                    w = REG_R(0x5F);
                    if (k == 48) {
                        REG_W(0x02, 0);
                        f |= REG_R(0x02) & 0x2000;
                    }
                }
                if ((n = hdlc_bit(&h, w >> (k & 8 ? 23 - (k & 15) : 7 - (k & 15)) & 1))) {
                    int antes = hist;

                    guarda(&hist, &anel, q, n, meu, n_meu | (modo & 0xFF0000u));   /* a sessao nos 23:16 */
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
        if (ESTADO_MSG == ARMADO_ACK) {    /* uma mensagem para mim: sai, o ack vai ao ar (v75) e a tela */
            TELA_APRS = VOLTA;             /* volta (v112) */
            break;
        }
    setor:
        /* O setor que recebe encheu: o outro se apaga ja (dezenas de ms) - parado, ou logo depois de um aviso,
         * com a FIFO recem-lida e a folga inteira (v128: no meio de uma captura o hist_grava o apagava no meio
         * das palavras do aviso, e a FIFO transbordava). */
        if ((anel & ANEL_SUJO) && hist - (int)ANEL_VELHOS(anel) >= (int)POR_SETOR) {
            hist = hist_prepara(hist, &anel);
            topo = 0;
            modo &= ~ABERTO;
            pede = 15;
        }
    }
    RX_FECHA();
    if (TELA_APRS == CAIXA_TO)             /* o # ainda apertado: a caixa abre ja, e a fabrica nao o ve */
        TECLA_PRESA = 5;
    else
        espera_soltar();
    if (TELA_APRS)                         /* a caixa ou a volta: o FILTER como esta */
        TELA_APRS |= num[0] * FILTRO_OFF;
    if (modo & ESCURO)
        LUZ(1);
    REG_W(0x59, 0);
    REG_W(0x58, 0);                        /* desligado, como o roger de fabrica deixa (0x01E44788) */
    REG_W(0x5E, era2 >> 16);
    REG_W(0x3F, era1 & 0xFFFF);
    REG_W(0x70, era1 >> 16);
    REG_W(0x72, era2 & 0xFFFF);
    REG_W(0x02, 0);
    filtro_rx(0, era3);                    /* o canal e os menus de novo (v102) */
    SAVE = era_save;                       /* antes de o OK gravar os ajustes (salva_menu) */
}

/* O set do MESSAGES da pasta MORSE (v144): a tela, a mesma do APRS, ou o OK da caixa dela. */
void morse_rx_grava(void)
{
    if (TELA_APRS & CAIXA) {
        morse_caixa_grava();
        TELA_APRS = TELA_MORSE | VOLTA;
        MENU_TIPO = 0;                     /* o do item: com 7 e fora da edicao, o motor (0x01E48DE6) chamaria o +0x13, */
        return;                            /* que aqui e a lista de opcoes, e nao uma funcao: o radio reiniciava */
    }
    TELA_APRS |= TELA_MORSE;
    aprs_rx_grava();
}
#endif
