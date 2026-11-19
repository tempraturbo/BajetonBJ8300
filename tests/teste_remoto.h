/* O ambiente do firmware/src/remoto.c no PC (tests/teste_remoto.py): a RAM do radio num vetor, o chip de dados noutro,
 * as rotinas de fabrica e as do bancos.c que ele chama imitadas, e o relogio, que anda a cada leitura. */
#include <string.h>

static unsigned char memoria[0x20000], chip[0x200000];
#define END(endereco)             (memoria + (endereco))
#define FIRMWARE(nome, endereco)  typedef int firmware_##nome
#define MENU_RADIO                0x01E2AA74u      /* onde o menu_tabela estaria */

/* O relogio: cada leitura anda passo_ms. */
static unsigned relogio_ms, passo_ms = 1;
static unsigned relogio(void)
{
    relogio_ms += passo_ms;
    return relogio_ms;
}
#define TIQUE_MS  relogio()

/* As rotinas de fabrica: o que cada uma devolve e quantas vezes foi chamada. O RESPONDE junta tudo o que sai num
 * fluxo, e guarda o maior pedaco e quantos foram. */
static unsigned magic_devolve, magic_vezes, tecla_fisica = 255;
static unsigned medidor = 255, vista, c_lugar, v_terceira, v_freq, v_tudo, v_outro, v_nr, v_porta, v_canal, v_lista;
static unsigned freq_pedida, flash_fora, ble_desligou;
static unsigned char fluxo[8192], resposta[64];
static unsigned fluxo_n, resposta_n, respostas, maior;
static unsigned porta_falsa[2] = {0, 0x1000};      /* o PA12 solto */

unsigned MAGIC(void) { magic_vezes++; return magic_devolve; }
unsigned TECLAS(void) { return tecla_fisica; }
volatile unsigned *PORTA(unsigned n) { return n == 12 ? porta_falsa : 0; }
void RESPONDE(const void *b, unsigned n)
{
    memcpy(resposta, b, n < sizeof resposta ? n : sizeof resposta);
    resposta_n = n;
    respostas++;
    if (fluxo_n + n <= sizeof fluxo)
        memcpy(fluxo + fluxo_n, b, n);
    fluxo_n += n;
    maior = n > maior ? n : maior;
}
void LE_FLASH(unsigned a, void *p, unsigned n)
{
    if (a + n > sizeof chip) {
        flash_fora++;
        return;
    }
    memcpy(p, chip + a, n);
}
void BLE_DESLIGA(void) { ble_desligou++; }
unsigned medidor_s(void) { return medidor; }
unsigned watch_tela(void) { return vista; }
unsigned c_no_lugar(void) { return c_lugar; }
unsigned terceira(void) { return v_terceira; }
unsigned freq_terceira(unsigned t) { freq_pedida = t; return v_freq; }
unsigned watch_le_tudo(void) { return v_tudo; }
unsigned outro_recebendo(void) { return v_outro; }
unsigned short nr_le(void) { return (unsigned short)v_nr; }
unsigned short porta_le(void) { return (unsigned short)v_porta; }
unsigned em_canal(void) { return v_canal; }
unsigned lista_item(void) { return v_lista; }
