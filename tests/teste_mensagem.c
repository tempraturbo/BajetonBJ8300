/* Roda o firmware/src/mensagem.c no PC para o teste_mensagem.py. Comandos na linha de comando:
 *   quadro <origem> <caminho> <info>    imprime o quadro AX.25 em hexa ("Q ...")
 *   afsk <origem> <caminho> <info>      manda o quadro em AFSK: as escritas no chip ("R ...")
 *   morse <wpm> <hz> <texto>            toca o texto em Morse: as escritas no chip
 *   hdlc <arquivo>                      passa os bits de linha do arquivo ('0'/'1') pelo decodificador
 *                                       e imprime cada quadro achado ("Q ...") */
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/mensagem.c"

static void hexa(const u8 *q, unsigned n)
{
    unsigned i;

    printf("Q ");
    for (i = 0; i < n; i++)
        printf("%02x", q[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    u8 q[400];
    unsigned n;

    if (argc == 5 && !strcmp(argv[1], "quadro")) {
        n = ax_quadro(q, argv[2], strlen(argv[2]), atoi(argv[3]), argv[4], strlen(argv[4]));
        hexa(q, n);
    } else if (argc == 5 && !strcmp(argv[1], "afsk")) {
        n = ax_quadro(q, argv[2], strlen(argv[2]), atoi(argv[3]), argv[4], strlen(argv[4]));
        afsk_manda(q, n, 30);
    } else if (argc == 5 && !strcmp(argv[1], "morse")) {
        morse_toca(argv[4], strlen(argv[4]), atoi(argv[2]), atoi(argv[3]));
        printf("R %u 0 0\n", relogio_us);
    } else if (argc == 3 && !strcmp(argv[1], "hdlc")) {
        struct hdlc h;
        FILE *f = fopen(argv[2], "rb");
        int c;

        hdlc_comeca(&h, q, 330);
        while ((c = fgetc(f)) != EOF)
            if (c == '0' || c == '1')
                if ((n = hdlc_bit(&h, c == '1')))
                    hexa(q, n);
        fclose(f);
    } else {
        return 2;
    }
    return 0;
}
