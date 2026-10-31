/* Roda o laco do APRS RX (firmware/src/mensagem.c, aprs_rx_grava) no PC, com o modem, a flash e as teclas
 * do teste_rx.h, para o teste_rx.py (v75). Uso:
 *   teste_rx <ar> <roteiro> <indicativo> <ssid> <faixa_us> [<flash antes> [<flash depois> [<tela> <injeta>]]]
 *   teste_rx volta    (v112: a volta da tela MESSAGES, gancho_teclas)
 *   teste_rx caixa    (v112: a caixa, aprs_caixa_abre e aprs_dest_grava)
 * <tela>/<injeta>: o b[0x14BF2] e o b[0x14BEF] na entrada (a caixa e a volta, v112), com o BEEP PROMPT em 0.
 * <ar>: os bits de linha ('0'/'1'), 1200 por segundo desde o tempo 0; <roteiro>: as teclas, "de:ate:tecla"
 * em ms separados por virgula ("-" sem nenhuma); <faixa_us>: quanto a tela gasta por faixa de 32 linhas;
 * <flash antes>/<flash depois>: a imagem de 0xD0000 a 0xDFFFF a carregar e a gravar.
 * Imprime, depois que o laco sai:
 *   H <quantos> e um "R <ambar> <texto>" por registro do historico, do mais velho ao mais novo
 *   E <estado do envio> <registro do ack>, e "A <quadro do ack em hexa>" se o ack ficou armado
 *   S <syncs que o modem deu no FE FE> <no 01 01>
 *   M e cada valor gravado no 0x58, em hexa, na ordem
 *   F <0x43> <0x31> <0x28> <0x2B> <0x54> <0x75> no comeco de cada tecla do roteiro
 *   G os mesmos registradores no fim, e quantas vezes o aplica_audio rodou
 *   V <o SAVE na primeira tecla> <o SAVE no fim> (o do menu e 3)
 *   C <syncs marca> <syncs espaco> <BAD> <FIX> <transbordou> <faixas> <ms> <luz> <posicao na tela>
 *   T <b[0x14BF2], a caixa e a volta> <b[0x14BEF]> <o BEEP PROMPT> <a tecla presa, + 1> (v112; v114) */
#include "../firmware/src/mensagem.c"

/* A volta (v112): o gancho_teclas numa sequencia de estados, cada chamada uma leitura de 10 ms do laco do
 * sinal. Imprime "K <o que devolveu, em sequencia>" e "R <TELA_APRS> <INJETA> <BEEP> <item> <valor> <tipo>
 * <estado da recepcao> <tecla presa>" depois de cada passo. */
static void leituras(unsigned n)
{
    printf("K");
    while (n--)
        printf(" %u", gancho_teclas());
    printf("\nR %u %u %u %u %u %u %u %u\n", TELA_APRS, INJETA, BEEP_PROMPT, MENU_NUMERO, VALOR, MENU_TIPO,
           ESTADO_RX, TECLA_PRESA);
}

static int volta(void)
{
    RAM(u32, 0x001E5004) = 0x1000;         /* o PTT solto */
    TELA_APRS = VOLTA;                     /* 1. o envio no ar: espera, sem contar */
    BEEP_PROMPT = 1;
    ESTADO_RX = 4;
    ESTADO_MSG = 'a';
    TX_ESTADO = 1;
    leituras(3);
    ESTADO_MSG = TX_ESTADO = 0;            /* 2. na tela principal: um ciclo parado (v114) */
    leituras(16);
    MODO_TELA = 1;                         /* 3. nos grupos: o OK que entra na pasta */
    MENU_TABELA = MENU_BASE;
    leituras(16);
    MENU_TABELA = MENU_BASE + 17 * 28;     /* 4. na pasta, no TO: o item vai ao MESSAGES, e o OK */
    MENU_NUMERO = ITEM_TO;
    MENU_TIPO = 7;
    leituras(1);
    tecla_forcada = TECLA_BAIXO;           /* 5. uma tecla de verdade: a volta desiste e o bipe volta */
    leituras(1);
    tecla_forcada = 255;
    TELA_APRS = CAIXA_MSG | FILTRO_OFF;    /* 6. a caixa fechou pelo EXIT (o editor nao esta aberto): */
    leituras(1);                           /* sintoniza de novo, e espera a sintonia */
    ESTADO_RX = 2;                         /* 7. presa nos grupos: desiste depois de 7 ciclos */
    MENU_TABELA = MENU_BASE;
    leituras(7 * 16 + 1);
    TECLA_PRESA = 5;                       /* 8. a tecla que agiu segurada (v115): nada passa ate soltar de */
    tecla_forcada = TECLA_CERQUILHA;       /* vez - nem a leitura falsa 0 da fabrica no meio */
    leituras(3);
    tecla_forcada = 0;
    leituras(1);
    tecla_forcada = 255;
    leituras(4);
    tecla_forcada = TECLA_CERQUILHA;
    leituras(1);
    tecla_forcada = 255;
    leituras(5);
    tecla_forcada = TECLA_CERQUILHA;
    leituras(1);
    tecla_forcada = 255;
    return 0;
}

/* A caixa (v112): abrir as duas, e o OK de cada uma pelo set do TO. Imprime "B <aberta> <item> <tipo>
 * <tamanho> <maximo> <texto>" e "D <TELA_APRS> <envio> <tam_dest> <tam_msg> <id> <dest>|<msg>". */
static void caixa_mostra(unsigned aberta)
{
    printf("B %u %u %u %u %u %.*s\n", aberta, MENU_NUMERO, MENU_TIPO, EDITOR_TAM, EDITOR_MAX, EDITOR_TAM,
           EDITOR_TEXTO);
}

static void grava_mostra(void)
{
    struct aprs *a = (struct aprs *)registro_aprs;

    aprs_dest_grava();
    printf("D %u %u %u %u %u %.*s|%.*s\n", TELA_APRS, ESTADO_MSG, a->tam_dest, a->tam_msg, a->id, a->tam_dest,
           a->dest, a->tam_msg, a->msg);
    ESTADO_MSG = 0;
}

static void digita(const char *t)
{
    EDITOR_TAM = (u8)strlen(t);
    memcpy((char *)EDITOR_TEXTO, t, strlen(t));
}

static int caixa(void)
{
    struct aprs *a = (struct aprs *)registro_aprs;

    a->tam_ind = 6;
    memcpy(a->ind, "PY2ABC", 6);
    a->tam_dest = 8;
    memcpy(a->dest, "PU2XYZ-9", 8);
    a->id = 7;
    caixa_mostra(aprs_caixa_abre());       /* nada pedido: nao abre */
    TELA_APRS = CAIXA_MSG | FILTRO_OFF;    /* o #: vazia, ate 37 */
    caixa_mostra(aprs_caixa_abre());
    digita("Ola 73");
    grava_mostra();                        /* o OK: a mensagem, o numero novo, o envio armado e a volta */
    TELA_APRS = CAIXA_MSG;
    caixa_mostra(aprs_caixa_abre());
    digita("");
    grava_mostra();                        /* vazia: nada vai ao ar, e a tela volta */
    TELA_APRS = CAIXA_TO;                  /* o # segurado: o TO gravado, ate 9 */
    caixa_mostra(aprs_caixa_abre());
    digita("");
    grava_mostra();                        /* em branco tambem se grava */
    TELA_APRS = 0;                         /* o TO da pasta, fora da caixa */
    digita("py5zz");
    grava_mostra();
    return 0;
}

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

    if (argc == 2 && !strcmp(argv[1], "volta"))
        return volta();
    if (argc == 2 && !strcmp(argv[1], "caixa"))
        return caixa();
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
    RAM(u8, 0x00014B6D) = 3;               /* o SAVE do menu ligado (v111: a tela o desliga e devolve) */
    if (argc > 9) {
        TELA_APRS = atoi(argv[8]);
        INJETA = atoi(argv[9]);
    }

    aprs_rx_grava();
    printf("V %u %u\n", save_na_tela, RAM(u8, 0x00014B6D));
    printf("T %u %u %u %u\n", TELA_APRS, INJETA, BEEP_PROMPT, TECLA_PRESA);

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
        printf("F %x %x %x %x %x %x\n", fotos[i][0], fotos[i][1], fotos[i][2], fotos[i][3], fotos[i][4],
               fotos[i][5]);
    printf("G %x %x %x %x %x %x %u\n", regs[0x43], regs[0x31], regs[0x28], regs[0x2B], regs[0x54], regs[0x75],
           n_audio);
    printf("C %u %u %u %u %u %u %u %u %u\n", ultimo_num[3], ultimo_num[4], ultimo_num[5], ultimo_num[6],
           transbordou, faixas_desenhadas, (unsigned)(relogio_us / 1000), luz, ultima_posicao);
    if (argc > 7) {
        FILE *f = fopen(argv[7], "wb");
        fwrite(flash_d, 1, sizeof flash_d, f);
        fclose(f);
    }
    return 0;
}
