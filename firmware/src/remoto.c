/* O controle remoto (v156 a v166): as teclas, o PTT, um estado curto e o retrato da tela, pelo cabo e pelo Bluetooth.
 *
 * O PC (o Tempra Link) manda, fora da sessao do cabo, um quadro de 7 bytes a cada 250 ms e a cada mudanca, sempre
 * duas vezes seguidas:
 *   '~' 'R' tecla bits x x soma
 *     tecla  o codigo cru da varredura (a tabela 0x01E60357) segurado, ou 255 nenhuma: 0 PF2, 1 PF3, 2 OK, 3 cima,
 *            4 baixo, 5 EXIT, 6 '1', 7 '2', 8 '3', 9 '*', 10 '4', 11 '5', 12 '6', 13 '0', 14 '7', 15 '8', 16 '9', 17 '#'
 *            (o PF2 por exclusao: e o unico fora da matriz)
 *     bits   1 o PTT, 2 o retrato da tela junto da resposta (v162; da v156 a v161 o espelho), 4 a tela do radio
 *            apagada (v164)
 *     x x    nao lidos: o app os manda em 255 (v157: menos bits 0 no fio, que e tambem o do PTT)
 *     soma   a dos 6 bytes de antes
 * Os bytes caem no buffer da sessao (0x16671, a conta em h[0x1666C]), e o firmware, fora da sessao e com 7 ou mais,
 * chama o conferidor do PROGRAMBT11U (0x01E3FD12): o do cabo em 0x01E4B4B4, o do BLE em 0x01E41762. Os dois passam
 * pelo remoto_cabo, que procura o quadro mais novo em qualquer posicao do buffer e devolve 0 (a fabrica zera a conta);
 * sem quadro, segue para o conferidor, e o CHIRP e o PROGRAMBT11U ficam como estavam. Um quadro cortado pelo meio
 * aparece inteiro na segunda copia.
 *
 * O estado mora em dois bytes do espelho dos ajustes, livres pela varredura (0x9041 e 0x9042 no cabo), zerados na
 * partida e nos ajustes que chegam pelo cabo (o zera_envio do bancos.c zera os dois do 0x14BEE):
 *   REM   (b[0x14BEE]) bits 4:0 a tecla + 1 (0 nenhuma), 5 o PTT, 6 o retrato, 7 a resposta pendente
 *   CONTA (b[0x14BED]) bits 4:0 o carimbo do ultimo quadro, em 64 ms; 5 o retrato pendente (v162); 6 o ESCURO, a
 *         tela apagada pelo PC, e 7 o ACORDADO, uma tecla do radio desde o fim do ABR (v164)
 * Sem quadro por 1 s (16 carimbos), quem le o REM o zera, e o ESCURO tambem: o radio solta a tecla e o PTT sozinho, e
 * a luz volta. Com o REM, ou com o ESCURO, a luz da tela nao apaga (o gancho_luz do bancos.c, v161): apagada, a primeira
 * tecla so a acenderia. Com o ESCURO, para o firmware ela segue acesa, mas o PWM a deixa em 0 (o luz_ciclo do tela.c),
 * ate uma tecla do proprio radio, que a acende pelo tempo do ABR (o ACORDADO, que o gancho_luz zera). O laco do sinal
 * reaplica a luz a toda volta (o gancho_teclas do mensagem.c): o PC liga e desliga o ESCURO na hora. O pedido novo (o
 * ESCURO que nao estava) zera o ACORDADO e apaga na hora (v165: ate a v164 a tela acordada so voltava a apagar no fim
 * do ABR, que cada tecla do PC recarrega).
 *
 * A resposta sai na tarefa principal (o remoto_tecla, no laco do sinal), pelo caminho das respostas da sessao
 * (0x01E431E8: a UART, ou o BLE se os bytes vieram dele), em 9 bytes:
 *   '~' 'r' rem tx rx sinal lado tensao soma
 *     rem    o REM sem o bit 7, que leva (v165) o ESCURO com o ACORDADO: uma tecla do radio acendeu a tela que o PC
 *            pediu apagada (o PC a mostra acesa e para de pedir)
 *     tx     b[0x1533C]: 0 parado, 1 transmitindo, 2 recusada
 *     rx     b[0x15367]: 0 e 1 sintoniza, 2 espera, 3 vigia, 4 recebendo, 5 MO
 *     sinal  o medidor_s (bancos.c), lido na hora: os segmentos (0 a 24: 2 por S ate o S9, depois 1 por 10 dB), com o
 *            bit 7 o microfone transmitindo, 255 nenhum (0 no instante em que o TX entra)
 *     lado   de quem o medidor mede, como a tela B mostra (o lado do medidor de fabrica, 0x01E452D0: o em
 *            evidencia, ou o outro com a dupla escuta parada nele; e a terceira quando o rodizio a pos no lugar
 *            dele), os bits crus, e o PC faz a conta (v159): 1:0 a vista do WATCH (0 DUAL, 1 SINGLE, 2 TRIPLE, 3
 *            TRIPLE com a C no lado B), 2 o lado B em evidencia, 3 a dupla escuta parada no outro lado, 4 o rodizio
 *            com a terceira no lugar dele. A letra: com o rodizio, a C (o B na vista 3); senao, o lado em evidencia
 *            (o outro com a dupla escuta parada nele): o A, ou o B (a C na vista 3)
 *     tensao b[0x149BC]: x 444 / 1000 = decimos de volt
 * Com o bit 2 do quadro, depois da resposta vem o retrato da tela (v162), pelo mesmo caminho, em pedacos de ate 20
 * bytes (o que uma notificacao do BLE leva sem MTU negociado):
 *   '~' 'S' versao tamanho(2) valores flash RAM s1 s2
 *     versao, tamanho  os do retrato.h (gerado pelo firmware/previa/retrato.py), o tamanho dos valores, da flash e da
 *                      RAM, o byte de baixo primeiro
 *     valores  o que a tela pede ao bancos.c e ao mensagem.c, na ordem do RETRATO_VALORES, o byte de baixo primeiro; e,
 *              desde a v163, onde o menu_tabela esta, que o PC compara com o ponteiro do menu (b[0x14BF4]): ele muda
 *              de lugar a cada compilacao, e o PC desenha com a tabela da previa
 *     flash    o que a tela le do chip de dados: os 4 B do comeco e os 12 do nome do canal da terceira (o canal de
 *              terceira(), ou o 0 com ele em 0xFFFF; a tela so le o canal fora do VFO e fora do 0xFFFF), e os 14 do
 *              nome da memoria do radio (o endereco do R_NOME do telas.c, so lido nas memorias)
 *     RAM      as faixas do retrato.h, na ordem dele
 *     s1 s2    as somas de Fletcher do '~' ao fim da RAM: s1 a dos bytes e s2 a das s1, a de baixo de cada
 * O PC desenha a tela com o mesmo codigo (src/tela.c e telas.c) em cima do retrato: uns 500 B por tela, contra os
 * 8 KB do espelho, que so levava umas 28 linhas por redesenho (a saida da UART nao anda enquanto a tela se desenha)
 * e saiu na v162. O retrato sai do laco do sinal (o gancho_teclas do mensagem.c chama o remoto_retrato), e nao do
 * remoto_tecla, que tambem roda no fundo do Morse (o tecla do mensagem.c, sem pilha para ele): ali a resposta sai,
 * e o retrato fica pendente ate a volta ao laco.
 * O contexto em que o BLE entrega os bytes (0x01E4E272, por ponteiro) nao foi provado: por isso nada se responde
 * de dentro do remoto_cabo chamado por ele.
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
extern const unsigned char menu_tabela[];           /* tela.c: o menu principal, onde o montar.py o pos */
#define MENU_RADIO                ((u32)menu_tabela)
#endif
#define RAM(tipo, endereco)       (*(volatile tipo *)END(endereco))

#include "retrato.h"

FIRMWARE(MAGIC, 0x01E3FD12);       unsigned MAGIC(void);           /* o PROGRAMBT11U: 1 abre a sessao */
FIRMWARE(TECLAS, 0x01E43D50);      unsigned TECLAS(void);          /* codigo cru, 255 = nenhuma */
FIRMWARE(PORTA, 0x01E001BA);       volatile u32 *PORTA(unsigned);  /* os registradores da porta; so mexe em r0 e r1 */
FIRMWARE(RESPONDE, 0x01E431E8);    void RESPONDE(const void *, unsigned);
FIRMWARE(LE_FLASH, 0x01E390AC);    void LE_FLASH(u32, void *, unsigned);   /* o chip de dados */
FIRMWARE(BLE_DESLIGA, 0x01E3CFD6); void BLE_DESLIGA(void);         /* o ble_module_enable(0): fecha a ligacao e cala */

#define CABO         ((const volatile u8 *)END(0x00016671))
#define CABO_CONTA   RAM(u16, 0x0001666C)
#define PELO_BLE     RAM(u8, 0x000153A0)
#define BLE_LIGACAO  RAM(u16, 0x000146D2)     /* o handle da ligacao BLE, 0 sem ligacao (0x01E3CF68, 0x01E3CE3E) */
#define TX_ESTADO    RAM(u8, 0x0001533C)
#define ESTADO_RX    RAM(u8, 0x00015367)
#define TENSAO       RAM(u8, 0x000149BC)
#define LANTERNA     (RAM(u8, 0x00014BDA) >> 7)
#define LADO_B       RAM(u8, 0x00015E04)      /* o lado em evidencia */
#define DW_OUTRO     RAM(u8, 0x00015378)      /* 1 = a dupla escuta parada fora do lado em evidencia */
unsigned medidor_s(void);                     /* bancos.c: o medidor S da tela B */
/* o que a tela pede ao bancos.c e ao mensagem.c, e o retrato manda calculado (a previa os imita) */
unsigned watch_tela(void);
u32 terceira(void);
u32 freq_terceira(u32 t);
u32 watch_le_tudo(void);
unsigned c_no_lugar(void);
unsigned outro_recebendo(void);
u16 nr_le(void);
u16 porta_le(void);
unsigned em_canal(void);
unsigned lista_item(void);

#define REM          RAM(u8, 0x00014BEE)
#define CONTA        RAM(u8, 0x00014BED)
#define TECLA        31u
#define PTT          32u
#define TELA         64u
#define RESPONDER    128u
#define RETRATO      32u                      /* no CONTA */
#define ESCURO       64u
#define ACORDADO     128u
#define RADIO_BANDA  RAM(u8, 0x00012A1B)      /* os do R_NOME do telas.c */
#define RADIO_MEMORIA RAM(u8, 0x00014CB4)

/* O REM, zerado se o ultimo quadro tem 16 a 31 carimbos de 64 ms (o bit 4 do tempo passado; os bits 5 a 7 do CONTA
 * nao chegam nele), e com ele o ESCURO; zerar o 0 nao muda nada. Quem le o REM a toda volta (o ptt_c) o zera logo que
 * o vigia vence. */
static unsigned remoto_vivo(void)
{
    unsigned r = REM;

    if (((TIQUE_MS >> 6) - CONTA) & 16) {
        REM = r = 0;
        CONTA &= ~ESCURO;
    }
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

/* No lugar do conferidor do PROGRAMBT11U, nos dois sitios (o cabo e o BLE). */
unsigned remoto_cabo(void)
{
    const volatile u8 *p = CABO, *q = 0;
    unsigned n = CABO_CONTA, i;

    for (i = 0; i + 7 <= n; i++)
        if (p[i] == '~' && p[i + 1] == 'R' && soma(p + i, 6) == p[i + 6])
            q = p + i;
    if (!q)
        return MAGIC();
    REM = (q[2] < 18 ? q[2] + 1 : 0) | (q[3] & 3) << 5 | RESPONDER;
    CONTA = (CONTA & CONTA << 1 & ACORDADO) | (TIQUE_MS >> 6 & 31) | (q[3] & 4) << 4;   /* o ACORDADO, so no escuro */
    return 0;
}

/* Os bits de quem o medidor mede (o lado da resposta, no alto do arquivo): a conta da letra e do PC. */
static unsigned quem(void)
{
    return (watch_tela() & 3) | (LADO_B != 0) << 2 | (DW_OUTRO == 1) << 3 | c_no_lugar() << 4;
}

/* O retrato (v162): os bytes um a um num pedaco de 20, que sai cheio pelo caminho das respostas, com as somas. */
struct pedaco {
    u8 b[20];
    unsigned k, s1, s2;
};

__attribute__((noinline)) static void sai(struct pedaco *o, const volatile u8 *p, unsigned n)
{
    for (; n--; p++) {
        o->b[o->k++] = *p;
        o->s1 += *p;
        o->s2 += o->s1;
        if (o->k == sizeof o->b) {
            RESPONDE(o->b, sizeof o->b);
            o->k = 0;
        }
    }
}

/* No laco do sinal (o gancho_teclas do mensagem.c, em toda leitura das teclas): o retrato pendente. Devolve k. */
unsigned remoto_retrato(unsigned k)
{
    struct pedaco o;
    u32 v[5];
    u8 *b = (u8 *)v;
    unsigned i, c, m;

    if (!(CONTA & RETRATO))
        return k;
    CONTA &= ~RETRATO;
    o.k = o.s1 = o.s2 = 0;
    v[0] = '~' | 'S' << 8 | RETRATO_VERSAO << 16 | (RETRATO_TAMANHO & 255) << 24;
    b[4] = RETRATO_TAMANHO >> 8;
    sai(&o, b, 5);
    v[0] = watch_tela();
    v[1] = c = terceira();
    v[2] = freq_terceira(c);
    v[3] = watch_le_tudo();
    v[4] = MENU_RADIO;
    sai(&o, b, 20);
    b[0] = c_no_lugar();
    b[1] = outro_recebendo();
    b[2] = nr_le();
    b[3] = porta_le();
    b[4] = em_canal();
    b[5] = lista_item();
    sai(&o, b, 6);
    c &= 0xFFFF;
    c = c == 0xFFFF ? 0 : c * 32;
    LE_FLASH(c, b, 4);
    LE_FLASH(c + 20, b + 4, 12);
    sai(&o, b, 16);
    m = RADIO_BANDA;
    LE_FLASH((m == 1 ? 0xC110u : m == 0 || m > 4 ? 0xC010u : 0xC210u) + RADIO_MEMORIA * 16, b, 14);
    sai(&o, b, 14);
    for (i = 0; i < RETRATO_FAIXAS; i++)
        sai(&o, (const volatile u8 *)END(RETRATO_BASE + (retrato_faixas[i][0] | retrato_faixas[i][1] << 8)),
            retrato_faixas[i][2]);
    b[0] = (u8)o.s1;
    b[1] = (u8)o.s2;
    sai(&o, b, 2);
    if (o.k)
        RESPONDE(o.b, o.k);
    return k;
}

/* No lugar da varredura das teclas (as duas leituras do mensagem.c): a tecla do remoto quando nenhuma esta
 * apertada, e a resposta pendente; o retrato, se o PC o pediu, fica pendente para o remoto_retrato. */
unsigned remoto_tecla(void)
{
    unsigned k = TECLAS(), r = remoto_vivo();
    union {
        u8 b[12];
        u16 h;
    } u;
    u8 *e = u.b;

    if (r & RESPONDER) {
        REM = r &= ~RESPONDER;
        u.h = '~' | 'r' << 8;
        e[2] = (u8)r | (CONTA & CONTA << 1 & ACORDADO);   /* v165: acordou no escuro */
        e[3] = TX_ESTADO;
        e[4] = ESTADO_RX;
        e[5] = (u8)medidor_s();              /* o 0x100 (o TX entrando, um instante) vai como 0 */
        e[6] = (u8)quem();
        e[7] = TENSAO;
        e[8] = (u8)soma(e, 8);
        RESPONDE(e, 9);
        if (r & TELA)
            CONTA |= RETRATO;
    }
    if (k != 255)
        CONTA |= ACORDADO;                   /* a tecla do radio acende a tela apagada pelo PC (v164) */
    else if (r & TECLA)
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

/* O inicio da TX (0x01E41212, com o BLUETOOTH em ON) desliga o BLE, que fecha a ligacao com o motivo 0x13, e o fim
 * dela (0x01E473D4) o religa: o celular perdia o remoto a cada PTT, do radio ou do app, e o PTT do app durava 1 s, o
 * vigia (medido em 18/11). Com uma ligacao aberta o BLE fica (v166), e o religar do fim nao faz nada com ela aberta
 * (0x01E3CE3E). Sem ligacao, como de fabrica. */
void ble_tx(void)
{
    if (!BLE_LIGACAO)
        BLE_DESLIGA();
}

/* O gancho do envio (desvio_envia, tela.c): com a lanterna acesa nada vai ao LCD (v152), e a de fabrica segue. */
u32 remoto_envia(void)
{
    return LANTERNA;
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
