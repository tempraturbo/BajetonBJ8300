/* O ambiente do firmware/src/remoto.c no PC (tests/teste_remoto.py): a RAM do radio num vetor, as rotinas de
 * fabrica que ele chama imitadas, e o relogio, que anda a cada leitura (a espera do anel tem de vencer sozinha).
 * Compila em 32 bits: os ponteiros do anel da UART ocupam 4 bytes na RAM, como no radio. */
#include <string.h>

typedef char confere_ponteiro[sizeof(void *) == 4 ? 1 : -1];

static unsigned char memoria[0x20000];
#define END(endereco)             (memoria + (endereco))
#define FIRMWARE(nome, endereco)  typedef int firmware_##nome

/* O relogio: cada leitura anda passo_ms; o consumidor do anel tira dreno bytes a cada leitura (0 = parado). */
static unsigned relogio_ms, passo_ms = 1, dreno;
static unsigned relogio(void)
{
    unsigned short *usados = (unsigned short *)(memoria + 0x151AC + 16);

    relogio_ms += passo_ms;
    *usados = *usados > dreno ? *usados - dreno : 0;
    return relogio_ms;
}
#define TIQUE_MS  relogio()

/* As rotinas de fabrica: o que cada uma devolve e quantas vezes foi chamada. */
static unsigned magic_devolve, magic_vezes, tecla_fisica = 255, tela_toda_vezes, motor_menu_vezes, travas;
static unsigned medidor = 255, vista, c_lugar;
static unsigned char resposta[64];
static unsigned resposta_n, respostas;
static unsigned porta_falsa[2] = {0, 0x1000};      /* o PA12 solto */

unsigned MAGIC(void) { magic_vezes++; return magic_devolve; }
unsigned TECLAS(void) { return tecla_fisica; }
volatile unsigned *PORTA(unsigned n) { return n == 12 ? porta_falsa : 0; }
void RESPONDE(const void *b, unsigned n) { memcpy(resposta, b, n); resposta_n = n; respostas++; }
void ANEL_TRAVA(unsigned a) { if (a == 0x151AC + 20) travas++; }
void ANEL_SOLTA(unsigned a) { if (a == 0x151AC + 20) travas--; }
/* O redesenho da tela principal: conta e, com o redesenha ligado, manda a tela em 4 faixas de 160 x 32 pelo envio,
 * como o firmware faz (tests/teste_remoto.c, as fatias). */
static void (*redesenha)(void);
void TELA_TODA(void) { tela_toda_vezes++; if (redesenha) redesenha(); }
unsigned MOTOR_MENU(void) { motor_menu_vezes++; return 0; }
unsigned medidor_s(void) { return medidor; }
unsigned watch_tela(void) { return vista; }
unsigned c_no_lugar(void) { return c_lugar; }
