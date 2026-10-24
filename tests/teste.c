/* Teste do compilador pi32v2 da JieLi: um mapa de bits de canais por banco. */
#define NUM_CANAIS 990

typedef struct {
    char nome[12];
    unsigned char bits[(NUM_CANAIS + 7) / 8];
} banco_t;

int canal_no_banco(const banco_t *b, unsigned canal)
{
    if (canal >= NUM_CANAIS)
        return 0;
    return (b->bits[canal >> 3] >> (canal & 7)) & 1;
}

void alterna_canal(banco_t *b, unsigned canal)
{
    if (canal < NUM_CANAIS)
        b->bits[canal >> 3] ^= (unsigned char)(1u << (canal & 7));
}
