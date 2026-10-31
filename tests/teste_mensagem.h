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
