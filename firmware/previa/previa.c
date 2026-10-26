/* Previa da tela B no PC: o src/tela.c compilado com -DPREVIA, com a RAM, a janela() e o envia()
 * do firmware imitados como estao mapeados em docs/mapa-da-tela.md. Cada cenario poe o estado do
 * radio na RAM imitada, nos enderecos de verdade, e a tela o le de la como no radio. A tela de
 * 160 x 128 sai em RGB565 (byte alto primeiro, como o buffer de video) num arquivo .rgb565.
 *
 * Uso: previa <pasta de saida>   (o previa.py compila, roda e converte para PNG)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

u8 memoria[0x20000];
static u16 tela[128][160];

#define VIDEO      0x00010190u
#define TAM_VIDEO  10240u

/* janela(x0, x1, y0, y1, limpar, cor) 0x01E3AD34: grava os limites e, com limpar, enche o buffer.
 * A do firmware nao tem trava; aqui, janela maior que o buffer para a previa com erro. */
void JANELA(unsigned x0, unsigned x1, unsigned y0, unsigned y1, unsigned limpar, unsigned cor)
{
    unsigned n = (x1 - x0) * (y1 - y0), i;

    if (x1 <= x0 || y1 <= y0 || x1 > 160 || y1 > 128 || n * 2 > TAM_VIDEO) {
        fprintf(stderr, "janela invalida: x %u..%u y %u..%u\n", x0, x1, y0, y1);
        exit(1);
    }
    memoria[0x14608] = x0;
    memoria[0x1460A] = y0;
    memoria[0x1460C] = x1;
    memoria[0x1460E] = y1;
    memoria[0x14610] = (x1 - x0) * 2 & 0xFF;
    memoria[0x14611] = (x1 - x0) * 2 >> 8;
    for (i = 0; limpar && i < n; i++) {
        memoria[VIDEO + i * 2] = cor >> 8;
        memoria[VIDEO + i * 2 + 1] = cor;
    }
}

/* envia() 0x01E3ADBC: manda o buffer da janela aberta para a tela. */
void ENVIA(void)
{
    unsigned x0 = memoria[0x14608], y0 = memoria[0x1460A], x1 = memoria[0x1460C], y1 = memoria[0x1460E];
    unsigned x, y, o = 0;

    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++, o += 2)
            tela[y][x] = memoria[VIDEO + o] << 8 | memoria[VIDEO + o + 1];
}

#include "../src/tela.c"

static const char *pasta;

static void grava(const char *nome)
{
    char caminho[512];
    FILE *f;
    unsigned x, y;

    snprintf(caminho, sizeof caminho, "%s/%s.rgb565", pasta, nome);
    if (!(f = fopen(caminho, "wb"))) {
        perror(caminho);
        exit(1);
    }
    for (y = 0; y < 128; y++)
        for (x = 0; x < 160; x++) {
            fputc(tela[y][x] >> 8, f);
            fputc(tela[y][x] & 0xFF, f);
        }
    fclose(f);
}

static void poe32(u32 a, u32 v)
{
    memoria[a] = v;
    memoria[a + 1] = v >> 8;
    memoria[a + 2] = v >> 16;
    memoria[a + 3] = v >> 24;
}

/* O radio em repouso: bateria cheia em 8,1 V, sem recepcao, sem zona, os dois lados em canal. */
static void limpa(void)
{
    memset(memoria, 0, sizeof memoria);
    memoria[0x149BC] = 183;            /* 183 x 444 / 1000 = 81 decimos */
    poe32(0x149C0, 3);
    memoria[0x14BD4] = 1;
    memoria[0x14B86] = 0x11;
}

/* canal 0 = VFO (o nibble do modo de trabalho vira 0); senao, 1..960. */
static void lado(unsigned i, u32 rx, u32 tx, const char *nome, unsigned canal, unsigned exibicao,
                 unsigned potencia)
{
    u32 r = 0x14DC4 + i * 31;

    poe32(r, rx);
    poe32(r + 5, tx);
    memset(&memoria[r + 15], 0xFF, 12);
    memcpy(&memoria[r + 15], nome, strlen(nome));
    if (canal) {
        memoria[0x15E0C + i * 4] = (canal - 1) & 0xFF;
        memoria[0x15E0D + i * 4] = (canal - 1) >> 8;
    } else
        memoria[0x14B86] &= i ? 0x0F : 0xF0;
    memoria[0x14B79 + i] = exibicao;
    memoria[0x15E30 + i] = potencia;
}

static void digitando(const char *d)
{
    memoria[0x14A35] = strlen(d);
    memcpy(&memoria[0x14A3A], d, strlen(d));
}

int main(int argc, char **argv)
{
    pasta = argc > 1 ? argv[1] : ".";

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14DC4 + 4] = 1;          /* CT no A */
    memoria[0x14B70] = 1;              /* DW */
    memoria[0x14BDE] = 1;              /* reducao de ruido */
    memoria[0x14B89] = 1;              /* bluetooth */
    tela_b();
    tela_b_medidor(0, 6, 0);           /* o RX desenha o degrau 2 (nivel 6) */
    grava("1-a-em-evidencia-recebendo");

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 1, 2);
    memoria[0x15E04] = 1;
    memoria[0x14B70] = 1;
    memoria[0x14B87] = 1;              /* trava */
    memoria[0x14B75] = 1;              /* DTMF ST */
    tela_b();
    tela_b_medidor(0, 2, 0);           /* DW parou no A: degrau 0 (nivel 2) no A */
    grava("2-b-em-evidencia-dw-no-a");

    limpa();
    lado(0, 12500000, 12500000, "", 0, 1, 2);
    lado(1, 43900000, 43900000, "", 0, 1, 0);
    memoria[0x14DC4 + 11] = 1;         /* N */
    memoria[0x14DC4 + 10] = 2;         /* - */
    memoria[0x14DC4 + 4] = 2;          /* DCS */
    memoria[0x149BC] = 162;            /* 7,1 V */
    poe32(0x149C0, 5);                 /* bateria fraca: contorno vermelho */
    tela_b();
    tela_b_medidor(0, 10, 0);          /* sinal no maximo */
    grava("3-vfo-aereo-am-bateria-fraca");

    limpa();
    lado(0, 43912500, 43912500, "REPETIDORA S", 960, 0, 1);
    lado(1, 14600000, 14600000, "APRS 2M", 1, 2, 0);
    memoria[0x14DC4 + 14] = 1;         /* SCR */
    memoria[0x15E28] = 1;              /* R */
    memoria[0x15370] = 1;              /* MO */
    memoria[0x14B6E] = 1;              /* VOX */
    tela_b();
    grava("4-nome-grande-e-numero");

    limpa();
    lado(0, 14612500, 14672500, "REPETIDORA", 25, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    memoria[0x14DC4 + 10] = 1;         /* + */
    memoria[0x14685] = 2;              /* transmitindo */
    tela_b();
    grava("5-transmitindo");

    limpa();
    lado(0, 14552500, 14552500, "", 0, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 0, 2);
    digitando("4391");
    tela_b();
    tela_b_digitando();
    grava("6-digitando-frequencia");

    limpa();
    lado(0, 44105000, 44105000, "PIGAO 3", 102, 1, 0);
    lado(1, 46256250, 46256250, "BF888-10", 10, 1, 2);
    memoria[0x15E04] = 1;
    digitando("12");
    tela_b();
    tela_b_digitando();
    grava("7-digitando-canal");

    /* a fonte inteira: as 64 letras e os icones em 1x, e os digitos em 2x */
    for (unsigned y = 0; y < 128; y += 32) {
        janela_b(0, 160, y, y + 32, COR_FUNDO);
        texto(4, 4, " !\"#$%&'()*+,-./", 1, COR_TEXTO);
        texto(4, 16, "0123456789:;<=>?", 1, COR_TEXTO);
        texto(4, 28, "@ABCDEFGHIJKLMNO", 1, COR_TEXTO);
        texto(4, 40, "PQRSTUVWXYZ[\\]^_", 1, COR_TEXTO);
        texto(110, 40, ICONE_NOTA " " ICONE_BLUETOOTH " " ICONE_CADEADO, 1, COR_VERDE);
        texto(4, 58, "01234", 2, COR_BRANCO);
        texto(4, 78, "56789", 2, COR_BRANCO);
        texto(4, 100, "2 5 2.5 25", 2, COR_VERDE);
        ENVIA();
    }
    grava("8-fonte");
    return 0;
}
