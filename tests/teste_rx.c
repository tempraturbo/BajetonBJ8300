/* Roda o laco do APRS RX (firmware/src/mensagem.c, aprs_rx_grava) no PC, com o modem, a flash e as teclas
 * do teste_rx.h, para o teste_rx.py (v75). Uso:
 *   teste_rx <ar> <roteiro> <indicativo> <ssid> <faixa_us> [<flash antes> [<flash depois> [<tela> <injeta>]]]
 *   teste_rx volta    (v112: a volta da tela MESSAGES, gancho_teclas)
 *   teste_rx caixa    (v112: a caixa, aprs_caixa_abre e aprs_dest_grava)
 *   teste_rx canais   (v140: as listas de canais, CH NAME, DELETE e SAVE)
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
 *   C <syncs marca> <syncs espaco> <BAD> <FIX> <transbordou> <faixas> <ms> <luz> <posicao na tela> <FIFO lida vazia>
 * Com RX_BORDA no ambiente, o aviso de quase cheia so nasce na palavra que leva a FIFO ao limite (teste_rx.h).
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
    TELA_APRS = CAIXA_MSG | FILTRO_OFF;    /* o #: com a ultima mensagem, ate 37 */
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
    /* sem indicativo (v138): o # pede o CALLSIGN (o item 0); vazio pede de novo, gravado abre a da mensagem; e a
     * mensagem sem indicativo nao arma o envio */
    a->tam_ind = 0;
    TELA_APRS = CAIXA_MSG;
    caixa_mostra(aprs_caixa_abre());
    digita("");
    aprs_ind_grava();
    caixa_mostra(aprs_caixa_abre());
    digita("py2abc");
    aprs_ind_grava();
    printf("I %u %.*s\n", TELA_APRS, a->tam_ind, a->ind);
    caixa_mostra(aprs_caixa_abre());
    a->tam_ind = 0;
    digita("Ola");
    grava_mostra();
    return 0;
}

/* As listas de canais (v140): cada caso imprime uma linha, e a tela e o que o motor pede ao lista_exibe em volta do
 * cursor. Canais de teste, contando de 0: 1, 5, 102 (com nome) e 103 (sem nome, mostra a frequencia). */
static void poe_canal(unsigned n, const char *nome)
{
    static const u8 f[4] = {0x00, 0x25, 0x46, 0x14};

    memcpy(flash_canais + (n << 5), f, 4);
    memset(flash_canais + (n << 5) + 4, 0, 16);
    memset(flash_canais + (n << 5) + 20, 0xFF, 12);
    if (nome)
        memcpy(flash_canais + (n << 5) + 20, nome, strlen(nome));
}

static void tela_lista(void)
{
    unsigned c = VALOR;
    int d;

    LINHA_BASE = (u16)c;
    printf("L");
    for (d = -1; d <= 1; d++) {
        VALOR = (u16)(c + d);
        printf(" [%s]", lista_exibe());
    }
    VALOR = (u16)c;
    printf(" %u %u\n", c, MENU_TIPO);
}

static void abre_lista(unsigned item)
{
    MENU_NUMERO = (u8)item;
    MENU_EDITANDO = 1;
    MENU_TIPO = TIPO_LISTA;
    VALOR = lista_le();
    tela_lista();
}

static void digita_nome(const char *t)
{
    EDITOR_TAM = (u8)strlen(t);
    memcpy((char *)EDITOR_TEXTO, t, strlen(t));
}

static void nome_gravado(unsigned n)
{
    printf("N %.12s\n", (char *)flash_canais + (n << 5) + 20);
}

static int canais_teste(void)
{
    memset(flash_canais, 0xFF, sizeof flash_canais);
    poe_canal(1, "BF888-02");
    poe_canal(5, "BF888-06");
    poe_canal(102, "PIGAO 3");
    poe_canal(103, 0);
    MODO_LADOS = 1;
    RAM(u16, 0x00015E0C) = 100;
    MENU_TABELA = 0;

    abre_lista(ITEM_DELETE);                /* o 100 nao existe: abre no mais perto, o 102 */
    lista_seta(1);
    tela_lista();
    lista_seta(1);                          /* do 103 da a volta ao 1 */
    tela_lista();
    lista_seta(-1);
    tela_lista();
    VALOR = 50;                             /* o numero digitado vale pelo mais perto */
    tela_lista();

    abre_lista(ITEM_SAVE);                  /* o 100 esta livre; so numeros livres */
    VALOR = 101;
    lista_seta(1);                          /* pula o 102 e o 103 */
    tela_lista();
    RAM(u8, 0x000153A1) = 165;              /* no modo GMRS, os 30 primeiros ficam fora */
    VALOR = 0;
    tela_lista();
    RAM(u8, 0x000153A1) = 0;

    /* o SAVE: da lista ao editor; cancelar volta a lista sem gravar */
    abre_lista(ITEM_SAVE);
    save_grava();
    printf("A %u %u %s\n", lista_editor_abre(), MENU_TIPO, TITULO_MENU);
    printf("C %u %u\n", lista_cancela(), MENU_TIPO);
    printf("S %s %d\n", TITULO_MENU, existe_no_registro(flash_canais + (100 << 5)));
    /* o OK em branco grava "CH 101", e o editor fecha */
    save_grava();
    lista_editor_abre();
    digita_nome("");
    save_grava();
    nome_gravado(100);
    printf("F %u %u\n", lista_editor_abre(), MENU_TIPO);
    /* com nome, e so espacos valem como em branco */
    VALOR = 104;
    MENU_TIPO = TIPO_LISTA;
    lista_editor_abre();
    digita_nome("TESTE");
    save_grava();
    nome_gravado(104);
    lista_editor_abre();
    VALOR = 105;
    MENU_TIPO = TIPO_LISTA;
    lista_editor_abre();
    digita_nome("   ");
    save_grava();
    nome_gravado(105);
    lista_editor_abre();

    /* o CH NAME: em VFO, o aviso e o "---"; em canal, abre com o nome, e o cru do canal fica */
    MODO_LADOS = 0;
    MENU_NUMERO = ITEM_CH_NAME;
    printf("V %u %s\n", lista_aviso(), lista_exibe());
    MODO_LADOS = 1;
    RAM(u16, 0x00015E0C) = 102;
    abre_lista(ITEM_CH_NAME);
    chname_grava();
    lista_editor_abre();
    printf("E %.*s %s\n", EDITOR_TAM, EDITOR_TEXTO, TITULO_MENU);
    digita_nome("NOVO");
    chname_grava();
    nome_gravado(102);
    printf("R %02x\n", flash_canais[(102 << 5) + 1]);
    lista_editor_abre();
    /* em branco, "CH 103"; o EXIT nao mexe no nome */
    MENU_TIPO = TIPO_LISTA;
    lista_editor_abre();
    printf("X %u\n", lista_cancela());
    nome_gravado(102);
    MENU_TIPO = TIPO_LISTA;
    lista_editor_abre();
    digita_nome("");
    chname_grava();
    nome_gravado(102);
    lista_editor_abre();

    /* o DELETE pergunta; o EXIT da pergunta nao apaga; o OK apaga e segue no mais perto; sem nenhum, o aviso */
    RAM(u16, 0x00015E0C) = 102;
    abre_lista(ITEM_DELETE);
    delete_grava();
    printf("P %u %d\n", MENU_TIPO, existe_no_registro(flash_canais + (102 << 5)));
    printf("Q %u %u\n", lista_cancela(), MENU_TIPO);
    delete_grava();
    delete_grava();
    printf("D %u %u %d\n", VALOR, MENU_TIPO, existe_no_registro(flash_canais + (102 << 5)));
    {
        unsigned k;

        memset(flash_canais, 0xFF, sizeof flash_canais);
        poe_canal(7, "UM");
        abre_lista(ITEM_DELETE);            /* com um so, so ele na tela */
        delete_grava();
        delete_grava();
        printf("Z %u %u\n", MENU_TIPO, lista_aviso());
        /* a memoria cheia */
        for (k = 0; k < CANAIS; k++)
            poe_canal(k, "X");
        MENU_NUMERO = ITEM_SAVE;
        printf("M %u\n", lista_aviso());
    }

    /* os BANKS (v141). O ACTIVE: na pasta, quantos ligados; a lista abre no primeiro, o OK liga na hora e o ligado
     * vem com o LIGADO; banco sem canal nao liga, e o aviso volta a ele */
    memset(flash_canais, 0xFF, sizeof flash_canais);
    poe_canal(1, "BF888-02");
    poe_canal(102, "PIGAO 3");
    banco_canal(0, 102);
    banco_canal(1, 500);                    /* o 501 nao existe: nao conta */
    marcas_teste = MARCA_LISTA | MARCA_BANKS;
    RAM(u16, 0x00015E0C) = 100;
    MENU_NUMERO = 0;
    MENU_EDITANDO = 0;
    VALOR = 0;
    GRAVADO = 0;
    printf("W %u [%s]\n", lista_aviso(), lista_exibe());
    abre_lista(0);
    ativo_grava();
    tela_lista();
    MENU_EDITANDO = 0;
    VALOR = 0;
    printf("W [%s]\n", lista_exibe());
    abre_lista(0);
    lista_seta(1);
    ativo_grava();
    printf("B %u %u %u\n", MENU_TIPO, VALOR, ligados_teste);
    printf("C %u %u %u\n", lista_cancela(), MENU_TIPO, VALOR);
    VALOR = 50;                             /* o numero digitado vale pelo mais perto: o 16 */
    tela_lista();

    /* o CHANNELS: o banco, depois os canais salvos a partir do que esta em uso; o OK poe e tira, e o EXIT volta */
    abre_lista(1);
    canais_grava();
    printf("T %s %u %u\n", TITULO_MENU, VALOR, BANCO_ABERTO);
    tela_lista();
    canais_grava();
    tela_lista();
    lista_seta(1);
    canais_grava();
    tela_lista();
    printf("C %u %u %u %s\n", lista_cancela(), MENU_TIPO, VALOR, TITULO_MENU);
    printf("Y %u %u\n", lista_cancela(), BANCO_ABERTO);

    /* o NAME: sem nome, o editor abre vazio; o titulo diz qual; cancelar nao grava; em branco volta ao de fabrica */
    abre_lista(2);
    nome_grava();
    printf("A %u %u %s\n", lista_editor_abre(), EDITOR_TAM, TITULO_MENU);
    printf("C %u %u %s\n", lista_cancela(), MENU_TIPO, TITULO_MENU);
    nome_grava();
    lista_editor_abre();
    digita_nome("REPETIDORAS");
    nome_grava();
    printf("F %u %u\n", lista_editor_abre(), MENU_TIPO);
    tela_lista();
    nome_grava();
    lista_editor_abre();
    printf("E %.*s %s\n", EDITOR_TAM, EDITOR_TEXTO, TITULO_MENU);
    digita_nome("   ");
    nome_grava();
    lista_editor_abre();
    tela_lista();

    /* sem canal salvo, o CHANNELS avisa; o que so vale num modo, no outro abre com o aviso */
    memset(flash_canais, 0xFF, sizeof flash_canais);
    MENU_NUMERO = 1;
    printf("H %u\n", lista_aviso());
    MENU_NUMERO = 0;
    printf("H %u\n", lista_aviso());
    marcas_teste = MARCA_VFO;
    MODO_LADOS = 1;
    printf("H %u\n", lista_aviso());
    MODO_LADOS = 0;
    printf("H %u\n", lista_aviso());
    marcas_teste = MARCA_CANAL;
    printf("H %u\n", lista_aviso());
    MODO_LADOS = 1;
    printf("H %u\n", lista_aviso());
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
    if (argc == 2 && !strcmp(argv[1], "canais"))
        return canais_teste();
    if (argc < 6)
        return 2;
    borda = getenv("RX_BORDA") != 0;
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
    printf("C %u %u %u %u %u %u %u %u %u %u\n", ultimo_num[3], ultimo_num[4], ultimo_num[5], ultimo_num[6],
           transbordou, faixas_desenhadas, (unsigned)(relogio_us / 1000), luz, ultima_posicao, vazias);
    if (argc > 7) {
        FILE *f = fopen(argv[7], "wb");
        fwrite(flash_d, 1, sizeof flash_d, f);
        fclose(f);
    }
    return 0;
}
