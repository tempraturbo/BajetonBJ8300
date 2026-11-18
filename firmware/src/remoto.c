/* O controle remoto (v156 a v159): as teclas, o PTT, um estado curto e o espelho da tela, pelo cabo e pelo Bluetooth.
 *
 * O PC (o Tempra Link) manda, fora da sessao do cabo, um quadro de 7 bytes a cada 250 ms e a cada mudanca, sempre
 * duas vezes seguidas:
 *   '~' 'R' tecla bits x x soma
 *     tecla  o codigo cru da varredura (a tabela 0x01E60357) segurado, ou 255 nenhuma: 0 PF2, 1 PF3, 2 OK, 3 cima,
 *            4 baixo, 5 EXIT, 6 '1', 7 '2', 8 '3', 9 '*', 10 '4', 11 '5', 12 '6', 13 '0', 14 '7', 15 '8', 16 '9', 17 '#'
 *            (o PF2 por exclusao: e o unico fora da matriz)
 *     bits   1 o PTT, 2 o espelho da tela (so pelo cabo), 4 pede a tela inteira, uma fatia por vez (v158)
 *     x x    o primeiro, com o bit 4, a fatia (v158): nos bits 5:0 um no de 1 a 63 de uma arvore das 128 linhas,
 *            como num heap (o 1 e a tela toda, o n tem os filhos 2n e 2n + 1: do 8 ao 15 as faixas de 16 linhas, do 32
 *            ao 63 as de 4); o segundo, e o primeiro sem o bit 4, nao lidos, e o app os manda em 255 (v157: menos
 *            bits 0 no fio, que e tambem o do PTT)
 *     soma   a dos 6 bytes de antes
 * Os bytes caem no buffer da sessao (0x16671, a conta em h[0x1666C]), e o firmware, fora da sessao e com 7 ou mais,
 * chama o conferidor do PROGRAMBT11U (0x01E3FD12): o do cabo em 0x01E4B4B4, o do BLE em 0x01E41762. Os dois passam
 * pelo remoto_cabo, que procura o quadro mais novo em qualquer posicao do buffer e devolve 0 (a fabrica zera a conta);
 * sem quadro, segue para o conferidor, e o CHIRP e o PROGRAMBT11U ficam como estavam. Um quadro cortado pelo meio
 * aparece inteiro na segunda copia.
 *
 * O estado mora em dois bytes do espelho dos ajustes, livres pela varredura (0x9041 e 0x9042 no cabo), zerados na
 * partida e nos ajustes que chegam pelo cabo (o zera_envio do bancos.c zera os dois do 0x14BEE):
 *   REM   (b[0x14BEE]) bits 4:0 a tecla + 1 (0 nenhuma), 5 o PTT, 6 o espelho, 7 a resposta pendente
 *   CONTA (b[0x14BED]) bits 4:0 o carimbo do ultimo quadro, em 64 ms; 5 o espelho perdeu linha desde a ultima
 *         resposta (v159); 6 a ultima fatia nao coube (ou linha perdida depois dela); 7 a fatia (v158): so durante o
 *         redesenho que o PC pediu, com o no nos bits 5:0 (o carimbo e a perda ficam com o remoto_cabo, e o vigia nao
 *         corre)
 * Sem quadro por 1 s (16 carimbos), quem le o REM o zera: o radio solta a tecla e o PTT e para o espelho sozinho.
 *
 * A resposta sai na tarefa principal (o remoto_tecla, no laco do sinal), pelo caminho das respostas da sessao
 * (0x01E431E8: a UART, ou o BLE se os bytes vieram dele), em 9 bytes:
 *   '~' 'r' rem tx rx sinal lado tensao soma
 *     rem    o REM sem o bit 7, e o bit 7 = a ultima fatia nao coube
 *     tx     b[0x1533C]: 0 parado, 1 transmitindo, 2 recusada
 *     rx     b[0x15367]: 0 e 1 sintoniza, 2 espera, 3 vigia, 4 recebendo, 5 MO
 *     sinal  o medidor_s (bancos.c), lido na hora: os segmentos (0 a 24: 2 por S ate o S9, depois 1 por 10 dB), com o
 *            bit 7 o microfone transmitindo, 255 nenhum (0 no instante em que o TX entra)
 *     lado   de quem o medidor mede, como a tela B mostra (o lado do medidor de fabrica, 0x01E452D0: o em
 *            evidencia, ou o outro com a dupla escuta parada nele; e a terceira quando o rodizio a pos no lugar
 *            dele), os bits crus, e o PC faz a conta (v159, pelo espaco; ate a v158 ia a letra): 1:0 a vista do WATCH
 *            (0 DUAL, 1 SINGLE, 2 TRIPLE, 3 TRIPLE com a C no lado B), 2 o lado B em evidencia, 3 a dupla escuta
 *            parada no outro lado, 4 o rodizio com a terceira no lugar dele; e 5 o espelho perdeu linha desde a
 *            resposta anterior (v159; ela a limpa). A letra: com o rodizio, a C (o B na vista 3); senao, o lado em
 *            evidencia (o outro com a dupla escuta parada nele): o A, ou o B (a C na vista 3)
 *     tensao b[0x149BC]: x 444 / 1000 = decimos de volt
 * O contexto em que o BLE entrega os bytes (0x01E4E272, por ponteiro) nao foi provado: por isso nada se responde
 * nem se desenha de dentro do remoto_cabo chamado por ele.
 *
 * O espelho: cada envio ao LCD (0x01E3ADBC, o desvio_envia do tela.c) manda a janela como no UV-17L: 01 'T' x0 x1
 * y0 y1, as corridas (quantos, o byte alto da cor e o baixo) e o CRC16-XMODEM do 'T' ate a ultima corrida, o byte
 * alto primeiro. Vai uma linha por quadro, escrita direto no anel da UART (0x151AC, 2400 B), como o 0x01E41676 faz e
 * com a mesma trava, sem buffer na pilha: o envio e chamado a ate 556 B, e a interface tem de ficar nos 740 B de
 * fabrica (pilha.py; o caminho do envio fica em 124 B, de 184 que sobram). Quem esvazia o anel e uma tarefa que nao
 * roda enquanto a tela se desenha (v158; na v157, que esperava 250 ms por linha, so o alto da tela chegava ao PC):
 * sem lugar para o pior caso da linha, ela se perde na hora e fica a marca. O PC pede entao a tela inteira por
 * fatias, uma por quadro, e parte ao meio a que nao coube, ate 4 linhas (o pior caso, 4 x 488 B, cabe no anel). A
 * perda marca tambem o bit 5, que so a resposta limpa, depois de o mandar: na v158 a fatia seguinte apagava a marca da
 * perda de antes dela, e o PC terminava a passada com um pedaco velho. Nao vai pelo BLE (o app so o pede pelo cabo),
 * nem na sessao.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#ifdef TESTE_REMOTO
#include "teste_remoto.h"                 /* tests/teste_remoto.py: a RAM num vetor, o firmware e o relogio imitados */
#else
#define END(endereco)             (endereco)
#define FIRMWARE(nome, endereco)  __asm__(".set " #nome ", " #endereco)
#define TIQUE_MS                  RAM(u32, 0x00014810)
#endif
#define RAM(tipo, endereco)       (*(volatile tipo *)END(endereco))

FIRMWARE(MAGIC, 0x01E3FD12);       unsigned MAGIC(void);           /* o PROGRAMBT11U: 1 abre a sessao */
FIRMWARE(TECLAS, 0x01E43D50);      unsigned TECLAS(void);          /* codigo cru, 255 = nenhuma */
FIRMWARE(PORTA, 0x01E001BA);       volatile u32 *PORTA(unsigned);  /* os registradores da porta; so mexe em r0 e r1 */
FIRMWARE(RESPONDE, 0x01E431E8);    void RESPONDE(const void *, unsigned);
FIRMWARE(ANEL_TRAVA, 0x01E4166C);  void ANEL_TRAVA(u32);           /* as do 0x01E41676, com o anel + 20 */
FIRMWARE(ANEL_SOLTA, 0x01E41672);  void ANEL_SOLTA(u32);
FIRMWARE(TELA_TODA, 0x01E44812);   void TELA_TODA(void);           /* a tela principal inteira (a lanterna) */
FIRMWARE(MOTOR_MENU, 0x01E489E0);  u32 MOTOR_MENU(void);           /* o menu inteiro (o fim do OK) */

#define CABO         ((const volatile u8 *)END(0x00016671))
#define CABO_CONTA   RAM(u16, 0x0001666C)
#define SESSAO       RAM(u8, 0x0001666A)
#define PELO_BLE     RAM(u8, 0x000153A0)
#define UART_LIGADA  RAM(u8, 0x00014778)      /* o 0x01E41660: sem ele a resposta nao sai */
#define TX_ESTADO    RAM(u8, 0x0001533C)
#define EM_TX        RAM(u8, 0x00014685)
#define ESTADO_RX    RAM(u8, 0x00015367)
#define TENSAO       RAM(u8, 0x000149BC)
#define MODO_TELA    RAM(u8, 0x0001533D)
#define LANTERNA     (RAM(u8, 0x00014BDA) >> 7)
#define LADO_B       RAM(u8, 0x00015E04)      /* o lado em evidencia */
#define DW_OUTRO     RAM(u8, 0x00015378)      /* 1 = a dupla escuta parada fora do lado em evidencia */
unsigned medidor_s(void);                     /* bancos.c: o medidor S da tela B */
unsigned watch_tela(void);                    /* bancos.c: a vista do WATCH nos bits 1:0 */
unsigned c_no_lugar(void);                    /* bancos.c: o rodizio pos a terceira no lugar do outro lado */
#define DIGITADOS    RAM(u8, 0x00014A35)      /* quantos digitos ja digitados (tela.c) */

#define REM          RAM(u8, 0x00014BEE)
#define CONTA        RAM(u8, 0x00014BED)
#define TECLA        31u
#define PTT          32u
#define ESPELHO      64u
#define RESPONDER    128u
#define CARIMBO      31u
#define SUJO         32u
#define PERDEU       64u
#define FATIA        128u

/* A janela do envio e o buffer dela (docs/mapa-da-tela.md, secao 1): o byte alto de cada pixel primeiro. */
#define VIDEO        ((const u16 *)END(0x00010190))   /* na mao do envio: nada o muda enquanto ele roda */
#define JAN_X0       RAM(u8, 0x00014608)
#define JAN_Y0       RAM(u8, 0x0001460A)
#define JAN_X1       RAM(u8, 0x0001460C)
#define JAN_Y1       RAM(u8, 0x0001460E)

/* O anel da UART (o 0x01E41676 e o 0x01E430B8): o inicio e o fim da memoria, a escrita, os ocupados e o tamanho. */
#define ANEL         0x000151ACu
#define ANEL_PONTEIRO(o) (*(u8 *volatile *)END(ANEL + (o)))
#define ANEL_INICIO  ANEL_PONTEIRO(0)
#define ANEL_FIM     ANEL_PONTEIRO(4)
#define ANEL_ESCRITA ANEL_PONTEIRO(12)        /* o 0x01E41676 os le dentro da trava */
#define ANEL_USADOS  RAM(u16, ANEL + 16)
#define ANEL_TAMANHO RAM(u16, ANEL + 18)

/* O REM, zerado se o ultimo quadro tem 1 s ou mais; no redesenho de uma fatia o CONTA leva o no, e nada se zera. */
static unsigned remoto_vivo(void)
{
    unsigned r = REM, c = CONTA;

    /* 16 a 31 carimbos (o bit 4), fora da fatia (o FATIA, bit 7, cai no 4 deslocado 3); zerar o 0 nao muda nada */
    if (((TIQUE_MS >> 6) - c) & ~(c >> 3) & 16)
        REM = r = 0;
    return r;
}

/* A soma de n bytes, a de baixo. */
static unsigned soma(const volatile u8 *p, unsigned n)
{
    unsigned s = 0;

    while (n--)
        s += *p++;
    return (u8)s;
}

/* No lugar do conferidor do PROGRAMBT11U, nos dois sitios (o cabo e o BLE). Pedida a tela inteira, a perda do
 * espelho fica para tras e, so pelo cabo (aqui e a tarefa principal) e fora da TX, a tela se redesenha: a principal
 * pelo redesenho da lanterna, o menu pelo do fim do OK. */
unsigned remoto_cabo(void)
{
    const volatile u8 *p = CABO, *q = 0;
    unsigned n = CABO_CONTA, i, b, c;

    for (i = 0; i + 7 <= n; i++)
        if (p[i] == '~' && p[i + 1] == 'R' && soma(p + i, 6) == p[i + 6])
            q = p + i;
    if (!q)
        return MAGIC();
    b = q[3];
    REM = (q[2] < 18 ? q[2] + 1 : 0) | (b & 3) << 5 | RESPONDER;
    c = (TIQUE_MS >> 6 & CARIMBO) | (CONTA & (SUJO | PERDEU));
    if (b & 4) {
        /* v158: a fatia que nao se redesenha (pelo BLE, na TX, noutra tela) fica perdida, e o PC a pede de novo. A que
         * se redesenha: o no no CONTA, sem a marca, e a marca que o remoto_envia puser e a resposta dela. v161: nem
         * no meio de uma digitacao: a tela inteira mostra o lado normal (so as rotinas de digitacao mostram os
         * digitos, tela.c), e a digitacao sumia do visor, pelo PC ou pelo teclado do radio. */
        c |= PERDEU;
        if (!PELO_BLE && !TX_ESTADO && !EM_TX && !DIGITADOS && (b = MODO_TELA) < 2) {
            CONTA = FATIA | (q[4] & 63);
            if (b)
                MOTOR_MENU();
            else
                TELA_TODA();
            c = (c & ~PERDEU) | (CONTA & PERDEU);
        }
    }
    CONTA = c;
    return 0;
}

/* Os bits de quem o medidor mede (o lado da resposta, no alto do arquivo): a conta da letra e do PC. */
static unsigned quem(void)
{
    return (watch_tela() & 3) | (LADO_B != 0) << 2 | (DW_OUTRO == 1) << 3 | c_no_lugar() << 4;
}

/* No lugar da varredura das teclas (as duas leituras do mensagem.c): a tecla do remoto quando nenhuma esta
 * apertada, e a resposta pendente. */
unsigned remoto_tecla(void)
{
    unsigned k = TECLAS(), r = remoto_vivo(), c;
    union {
        u8 b[12];
        u16 h;
    } u;
    u8 *e = u.b;

    if (r & RESPONDER) {
        REM = r &= ~RESPONDER;
        CONTA = (c = CONTA) & ~SUJO;
        u.h = '~' | 'r' << 8;
        e[2] = (u8)(r | (c & PERDEU) << 1);
        e[3] = TX_ESTADO;
        e[4] = ESTADO_RX;
        e[5] = (u8)medidor_s();              /* o 0x100 (o TX entrando, um instante) vai como 0 */
        e[6] = (u8)(quem() | (c & SUJO));
        e[7] = TENSAO;
        e[8] = (u8)soma(e, 8);
        RESPONDE(e, 9);
    }
    if (k == 255 && (r & TECLA))
        k = (r & TECLA) - 1;
    return k;
}

/* O PTT (os cinco sitios da execucao que leem o PA12 por 0x01E001BA(12)): segurado pelo remoto, uma porta falsa
 * com o +4 em 0, o PTT apertado. O ptt_porta (no fim) guarda o r2 e o r3, que a de fabrica nao toca.
 * v157: o PA12 e tambem o fio do RX da serial do cabo, e os bytes do PC o apertam: na v156 o radio transmitiu do 5o
 * quadro em diante, sem PTT nenhum no quadro (medido em 18/11). Com o remoto ligado pelo cabo, o PTT e so o do quadro:
 * a outra porta falsa, com o +4 em 0x1000, o PTT solto. Pelo BLE o fio nao e usado, e o PTT do radio segue valendo. */
static const u32 portas_falsas[3] = {0, 0, 0x1000};   /* +0: o +4 em 0 (apertado); +1: o +4 em 0x1000 (solto) */

volatile u32 *ptt_c(unsigned n)
{
    unsigned r = remoto_vivo();

    if (r & PTT)
        return (volatile u32 *)portas_falsas;
    return r && !PELO_BLE ? (volatile u32 *)portas_falsas + 1 : PORTA(n);
}

/* O espelho. Os k bytes de v (o de baixo primeiro) no anel, a partir de e; devolve onde parou. A trava do anel
 * esta com quem chama. */
__attribute__((noinline)) static u8 *poe(u8 *e, u32 v, unsigned k)
{
    for (; k--; v >>= 8) {
        *e = (u8)v;
        if (++e == ANEL_FIM)
            e = ANEL_INICIO;
    }
    return e;
}

/* Uma linha da janela num quadro, direto no anel, com a trava; c e o 'T', x0 e x1 do cabecalho. Uma corrida nunca
 * passa da linha (160 pixels). O CRC16-XMODEM vai do 'T' ao fim das corridas, lido de volta do anel. */
__attribute__((noinline)) static void linha(const u16 *p, unsigned w, u32 c, unsigned y)
{
    u8 *e, *f;
    unsigned n, k, corridas = 0, crc = 0;

    ANEL_TRAVA(ANEL + 20);
    f = ANEL_ESCRITA;
    e = poe(f, 1 | c << 8, 4);
    e = poe(e, y | (y + 1) << 8, 2);
    for (n = w; n; n -= k, p += k, corridas++) {
        for (k = 1; k < n && p[k] == p[0]; k++)
            ;
        e = poe(e, k | ((const u8 *)p)[0] << 8 | ((const u8 *)p)[1] << 16, 3);
    }
    while (++f == ANEL_FIM ? (f = ANEL_INICIO) != e : f != e) {
        crc ^= (unsigned)*f << 8;
        for (k = 8; k--;)
            crc = crc & 0x8000 ? crc << 1 ^ 0x1021 : crc << 1;
    }
    ANEL_ESCRITA = poe(e, (crc >> 8 & 255) | (crc & 255) << 8, 2);   /* o crc passa dos 16 bits: so os de baixo */
    ANEL_USADOS += corridas * 3 + 8;
    ANEL_SOLTA(ANEL + 20);
}

/* O gancho do envio (desvio_envia, tela.c): com a lanterna acesa nada vai ao LCD (v152) nem ao PC; senao o espelho
 * da janela, uma linha por quadro, e a de fabrica segue. v158: o anel nao anda enquanto a tela se desenha (na v157,
 * que esperava ate 250 ms por lugar, so o alto da tela chegava ao PC; a trava dele chama o sistema, 0x02002756, e
 * quem o esvazia nao foi mapeado). Sem lugar para o pior caso da linha, ela se perde na hora e fica a marca. Na
 * fatia pedida pelo PC, so as linhas dela. */
u32 remoto_envia(void)
{
    unsigned x0 = JAN_X0, w = JAN_X1 - x0, y = JAN_Y0, y1 = JAN_Y1, c = CONTA, v = c & FATIA ? c & 63 : 0, u;
    const u16 *p = VIDEO;

    if (LANTERNA)
        return 1;
    /* o espelho escreve so no anel da UART, nunca no BLE, e so o liga quem o pede (o app pelo cabo; v158: sem o
     * PELO_BLE aqui, pelo espaco). Na fatia, so as linhas do no v: as que tem o v entre os ancestrais da folha
     * y + 128 (o 0, fora da fatia, e o de todas). */
    if (remoto_vivo() & ESPELHO && !SESSAO && UART_LIGADA && w - 1 < 160)
        for (; y < y1; y++, p += w) {
            for (u = y | 128; u > v; u >>= 1)
                ;
            if (u == v) {
                if ((unsigned)(ANEL_TAMANHO - ANEL_USADOS) < w * 3 + 8) {
                    CONTA |= SUJO | PERDEU;    /* v159: as duas; no meio da fatia o remoto_cabo so le o 6 */
                    break;
                }
                linha(p, w, 'T' | x0 << 8 | (x0 + w) << 16, y);
            }
        }
    return 0;
}

/* O ptt_porta guarda o r2 e o r3, que a rotina de fabrica nao toca e o 0x01E43C1A le depois da chamada. */
#ifndef TESTE_REMOTO
__asm__(".text\n"
        ".globl ptt_porta\n"
        "ptt_porta:\n\t"
        "[--sp] = {rets, r3, r2}\n\t"
        "call ptt_c\n\t"
        "{pc, r3, r2} = [sp++]\n"
        ".previous");
#endif
