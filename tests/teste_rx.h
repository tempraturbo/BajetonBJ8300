/* O laco do APRS RX (firmware/src/mensagem.c, aprs_rx_grava) no PC, para o teste_rx.py (v75). Imita o que
 * o laco toca no radio:
 *  - o relogio, em microssegundos (TIQUE_MS anda junto), que cada chamada ao "firmware" adianta um pouco;
 *  - o modem FSK do BK4819 em captura crua, como o mapeamos: armado pelo 0x59, ele so sincroniza quando
 *    os ultimos 16 bits de linha sao o padrao do 0x5A; depois passa 2048 bytes de bits crus pela FIFO de
 *    8 palavras (o primeiro bit no bit 7 da palavra, o nono no 15), avisa de quase cheia com 4 e conta
 *    as palavras que nao couberam (transbordou);
 *  - os bits do ar, 1200 por segundo, de um arquivo; depois dele, ruido;
 *  - a flash do chip de dados (de 0xD0000 a 0xDFFFF), que so grava de 1 para 0 e apaga por setor;
 *  - o teclado, por um roteiro de intervalos; a luz; e a tela, que gasta um tempo por faixa.
 * O resto do firmware que o mensagem.c chama nao faz nada aqui. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u8 ram_pc[0x20000];
#define RAM(tipo, endereco)       (*(volatile tipo *)(ram_pc + ((endereco) & 0x1FFFF)))
#define FIRMWARE(nome, endereco)  typedef int firmware_##nome
#define TIQUE_MS                  RAM(u32, 0x00014810)

/* ---------- o relogio e o modem ---------- */
static unsigned long long relogio_us;
static const u8 *ar;                       /* os bits de linha no ar, desde o tempo 0 */
static unsigned ar_n, bit_ar;
static unsigned regs[128], armado, capturando, deslocado, palavra, nbits, palavras;
static unsigned fifo[8], fifo_ini, fifo_n, avisos, avisos_lidos;
static unsigned syncs_modem[2];             /* os syncs que o modem deu, no FE FE e no 01 01 */
static unsigned transbordou, faixas_desenhadas, tempo_faixa_us = 15000, luz = 1, ultima_posicao, ultimo_num[7];
static unsigned escritas_58[32], n_58;      /* o que o laco gravou no 0x58 (o modo e o ganho, v77) */

static void modem_bit(unsigned b)
{
    deslocado = (deslocado << 1 | b) & 0xFFFF;
    if (!capturando) {
        if (armado && deslocado == (regs[0x5A] & 0xFFFF)) {
            syncs_modem[deslocado == 0x0101]++;
            capturando = 1;
            avisos |= 0x0002;
            nbits = palavra = palavras = 0;
        }
        return;
    }
    palavra |= b << (nbits < 8 ? 7 - nbits : 23 - nbits);
    if (++nbits < 16)
        return;
    if (fifo_n == 8)
        transbordou++;
    else
        fifo[(fifo_ini + fifo_n++) & 7] = palavra;
    if (fifo_n >= 4)
        avisos |= 0x1000;
    nbits = palavra = 0;
    if (++palavras == 1024) {              /* os 2048 bytes do 0x5D */
        capturando = armado = 0;
        avisos |= 0x2000;
    }
}

static void anda(unsigned us)
{
    relogio_us += us;
    if (relogio_us > 300000000ull) {       /* 5 min sem o laco sair: preso */
        printf("X o laco nao saiu\n");
        exit(4);
    }
    TIQUE_MS = (u32)(relogio_us / 1000);
    while ((unsigned long long)bit_ar * 1000000 / 1200 < relogio_us) {
        modem_bit(bit_ar < ar_n ? ar[bit_ar] : (unsigned)(rand() & 1));
        bit_ar++;
    }
}

static u32 agora_us(void)
{
    anda(5);
    return (u32)relogio_us;
}

static void REG_W(unsigned reg, unsigned valor)
{
    anda(20);
    regs[reg & 127] = valor;
    if (reg == 0x58 && n_58 < 32)
        escritas_58[n_58++] = valor;
    if (reg == 0x59) {
        if (valor & 0x4000)
            fifo_n = 0;
        armado = (valor & 0x1000) != 0;
        if (!armado)
            capturando = 0;
    } else if (reg == 0x02) {              /* zera os avisos; o 0x02 lido depois diz quais eram */
        avisos_lidos = avisos;
        avisos = 0;
    }
}

static unsigned REG_R(unsigned reg)
{
    unsigned w;

    anda(20);
    if (reg == 0x0C)
        return avisos != 0;
    if (reg == 0x02)
        return avisos_lidos;
    if (reg == 0x0B)
        return 0;
    if (reg == 0x5F) {
        if (!fifo_n)
            return 0;
        w = fifo[fifo_ini++ & 7];
        fifo_n--;
        return w;
    }
    return regs[reg & 127];
}

static void TOM(unsigned f1, unsigned f2)
{
    REG_W(0x71, f1);
    REG_W(0x72, f2);
}

static void ESPERA_2MS(unsigned n)
{
    anda(n * 2000);
}

/* ---------- a flash do chip de dados ---------- */
static u8 flash_d[0x10000];

static u8 *flash_em(u32 a, unsigned n)
{
    if (a < 0xD0000 || a + n > 0xE0000) {
        printf("X flash fora: %x\n", (unsigned)a);
        exit(3);
    }
    return flash_d + (a - 0xD0000);
}

static void LE_FLASH(u32 a, void *p, u32 n)
{
    anda(10 + n);
    memcpy(p, flash_em(a, n), n);
}

static void GRAVA_FLASH(u32 a, const void *p, u32 n)
{
    u8 *f = flash_em(a, n);
    u32 i;

    anda(1000);
    for (i = 0; i < n; i++)
        f[i] &= ((const u8 *)p)[i];        /* a flash so vai de 1 para 0 */
}

static void APAGA_SETOR(u32 a)
{
    anda(60000);
    memset(flash_em(a & ~0xFFFu, 0x1000), 0xFF, 0x1000);
}

/* ---------- as teclas, a luz e a tela ---------- */
static struct {
    unsigned de, ate, tecla;               /* em ms */
} roteiro[32];
static unsigned n_roteiro;

static unsigned TECLAS(void)
{
    unsigned i;

    anda(50);
    for (i = 0; i < n_roteiro; i++)
        if (TIQUE_MS >= roteiro[i].de && TIQUE_MS < roteiro[i].ate)
            return roteiro[i].tecla;
    return 255;
}

static void LUZ(unsigned acende)
{
    luz = acende;
}

void tela_aprs(u32 a0, u32 a1, unsigned aberto, const u16 *num, unsigned faixas)
{
    unsigned i;

    (void)a0, (void)a1, (void)aberto;
    for (i = 0; i < 7; i++)
        ultimo_num[i] = num[i];
    ultima_posicao = num[1];
    for (; faixas; faixas &= faixas - 1) {
        faixas_desenhadas++;
        anda(tempo_faixa_us);
    }
}

/* o registro do APRS (o indicativo), como o bancos.c o le */
static u8 registro_aprs[64];

int le_registro(u32 setor, void *p, unsigned n)
{
    (void)setor;
    memcpy(p, registro_aprs, n);
    return 1;
}

void grava_registro(u32 setor, void *p, unsigned n)
{
    (void)setor;
    memcpy(registro_aprs, p, n);
}

/* ---------- o resto do firmware: nada ---------- */
static void WDT(void) { anda(5); }
static void COLUNAS(unsigned c) { (void)c; anda(5); }
static void ACORDA(void) {}
static void DESARMA_DW(void) {}
static void IDA_PTT(void) {}
static void LUZ_ABR(void) {}
static void INICIO_TX(void) {}
static void FIM_TX(void) {}
static void GERADOR(unsigned x) { (void)x; }
static void AUDIO_TX(unsigned x) { (void)x; }
static void AF(unsigned x) { (void)x; }
static void PEDE_AMPLI(unsigned x) { (void)x; }
static void LIMPA(unsigned x) { (void)x; }
static void SAI_DO_EDITOR(void) {}

static int SPRINTF(char *d, const char *formato, ...)
{
    va_list v;
    int n;

    va_start(v, formato);
    n = vsprintf(d, formato, v);
    va_end(v);
    return n;
}
