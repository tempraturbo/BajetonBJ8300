/* O chip de radio e o relogio imitados, para o firmware/src/mensagem.c rodar no PC (teste_mensagem.py).
 * Cada escrita no chip sai numa linha "R <us> <reg> <valor>", com o relogio, que anda com as escritas e
 * as esperas e comeca perto do fim dos 32 bits, para as contas provarem que atravessam a volta. */
#include <stdio.h>

static u32 relogio_us = 0xFFFC0000u;      /* da a volta nos 32 bits no meio do conteudo */

static void REG_W(unsigned reg, unsigned valor)
{
    relogio_us += 40;                      /* o tempo de mandar 24 bits pelo fio do chip */
    printf("R %u %u %u\n", relogio_us, reg, valor);
}

static void TOM(unsigned f1, unsigned f2)
{
    REG_W(0x71, f1);
    REG_W(0x72, f2);
}

static void ESPERA_2MS(unsigned n)
{
    relogio_us += n * 2000;
}

static unsigned para(void)
{
    return 0;                              /* no PC nada para o Morse */
}

/* O numero em texto e a copia de texto da tela (tela.c), que o firmware usa (v113). */
static char *numero(char *s, unsigned v, unsigned digitos)
{
    unsigned m = 1;

    while (--digitos)
        m *= 10;
    while (v / m >= 10)
        m *= 10;
    for (; m; m /= 10)
        *s++ = '0' + v / m % 10;
    *s = 0;
    return s;
}

static char *junta(char *s, const char *t)
{
    while ((*s = *t++) != 0)
        s++;
    return s;
}
