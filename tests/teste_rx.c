/* Roda o laco do APRS RX (firmware/src/mensagem.c, aprs_rx_grava) no PC, com o modem, a flash e as teclas
 * do teste_rx.h, para o teste_rx.py (v75). Uso:
 *   teste_rx <ar> <roteiro> <indicativo> <ssid> <faixa_us> [<flash antes> [<flash depois>]]
 * <ar>: os bits de linha ('0'/'1'), 1200 por segundo desde o tempo 0; <roteiro>: as teclas, "de:ate:tecla"
 * em ms separados por virgula ("-" sem nenhuma); <faixa_us>: quanto a tela gasta por faixa de 32 linhas;
 * <flash antes>/<flash depois>: a imagem de 0xD0000 a 0xDFFFF a carregar e a gravar.
 * Imprime, depois que o laco sai:
 *   H <quantos> e um "R <ambar> <texto>" por registro do historico, do mais velho ao mais novo
 *   E <estado do envio> <registro do ack>, e "A <quadro do ack em hexa>" se o ack ficou armado
 *   S <syncs que o modem deu no FE FE> <no 01 01>
 *   M e cada valor gravado no 0x58, em hexa, na ordem (v77)
 *   C <syncs marca> <syncs espaco> <BAD> <FIX> <transbordou> <faixas> <ms> <luz> <posicao na tela> */
#include "../firmware/src/mensagem.c"

static u8 *le_arquivo(const char *nome, unsigned *n)
{
    FILE *f = fopen(nome, "rb");
    u8 *p;
    long t;

    if (!f)
        exit(2);
    fseek(f, 0, SEEK_END);
    t = ftell(f);
    fseek(f, 0, SEEK_SET);
    p = malloc(t + 1);
    *n = (unsigned)fread(p, 1, t, f);
    fclose(f);
    return p;
}

int main(int argc, char **argv)
{
    struct aprs *a = (struct aprs *)registro_aprs;
    unsigned n, i, k, anel;
    u8 *texto, q[CAB(2) + 1 + 9 + 1 + MAX_MSG + 3 + 2];
    char *r, *s;
    int hist;

    if (argc < 6)
        return 2;
    texto = le_arquivo(argv[1], &n);
    ar = texto;
    for (i = k = 0; i < n; i++)
        if (texto[i] == '0' || texto[i] == '1')
            texto[k++] = texto[i] - '0';
    ar_n = k;
    for (s = argv[2]; *s && *s != '-' && n_roteiro < 32; n_roteiro++) {
        roteiro[n_roteiro].de = strtoul(s, &s, 10);
        roteiro[n_roteiro].ate = strtoul(s + 1, &s, 10);
        roteiro[n_roteiro].tecla = strtoul(s + 1, &s, 10);
        if (*s == ',')
            s++;
    }
    a->tam_ind = strlen(argv[3]) < 6 ? strlen(argv[3]) : 6;
    memcpy(a->ind, argv[3], a->tam_ind);
    a->ssid = atoi(argv[4]);
    tempo_faixa_us = atoi(argv[5]);
    memset(flash_d, 0xFF, sizeof flash_d);
    if (argc > 6 && strcmp(argv[6], "-")) {
        FILE *f = fopen(argv[6], "rb");
        if (!f || fread(flash_d, 1, sizeof flash_d, f) != sizeof flash_d)
            return 2;
        fclose(f);
    }
    RAM(u32, 0x001E5004) = 0x1000;         /* o PTT solto */
    /* o canal e os menus na entrada (v102): o filtro largo de fabrica, FM com scramble e o compressor ou
     * expansor ligados (bits 1 e 3 do 0x31), o expansor no 0x28 e a DE-EMPHASIS em OFF */
    regs[0x43] = 0x3028;
    regs[0x31] = 0x000A;
    regs[0x28] = 0x2A40;
    regs[0x2B] = 0x0100;

    aprs_rx_grava();

    hist = hist_abre(&anel);
    printf("H %d\n", hist);
    for (i = 0; (int)i < hist; i++) {
        r = (char *)flash_em(hist_endereco(hist, anel, i), REG_HIST);
        printf("R %u %s\n", r[0] == USADO ? (u8)r[1] : 9u, r[0] == USADO ? r + 2 : "");
    }
    printf("E %u %u\n", ESTADO_MSG, ACK_REGISTRO);
    if (ESTADO_MSG == ARMADO_ACK) {
        n = quadro_aprs(q, HIST_SETOR(ACK_REGISTRO >> 5 & 1) + (ACK_REGISTRO & 31) * REG_HIST + ACK_PARA);
        printf("A ");
        for (i = 0; i < n; i++)
            printf("%02x", q[i]);
        printf("\n");
    }
    printf("S %u %u\n", syncs_modem[0], syncs_modem[1]);
    printf("M");
    for (i = 0; i < n_58; i++)
        printf(" %x", escritas_58[i]);
    printf("\n");
    for (i = 0; i < n_fotos; i++)
        printf("F %x %x %x %x\n", fotos[i][0], fotos[i][1], fotos[i][2], fotos[i][3]);
    printf("G %x %x %x %x %u\n", regs[0x43], regs[0x31], regs[0x28], regs[0x2B], n_audio);
    printf("C %u %u %u %u %u %u %u %u %u\n", ultimo_num[3], ultimo_num[4], ultimo_num[5], ultimo_num[6],
           transbordou, faixas_desenhadas, (unsigned)(relogio_us / 1000), luz, ultima_posicao);
    if (argc > 7) {
        FILE *f = fopen(argv[7], "wb");
        fwrite(flash_d, 1, sizeof flash_d, f);
        fclose(f);
    }
    return 0;
}
