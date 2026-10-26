/* As outras telas no tema B (v37): menu, scan QT e radio; e, desde a v38, a tensao na partida.
 * Incluido no fim do tela.c (mesmo tela.o, na faixa .livre), usa as pecas da tela principal: a barra
 * de cima, as etiquetas, a fonte e o medidor. Tudo o que estas telas mostram e lido da RAM, nos
 * enderecos que o desenho de fabrica le (docs/mapa-da-tela.md, secao 13); cada endereco traz a
 * instrucao de fabrica que o le (o desmontado escreve h[base+i<<1] para base + i x 2):
 *  - menu (modo de tela 1), 0x01E489E0: titulo do item em 0x15494, numero em b[0x14BFA], tres
 *    linhas de opcao em 0x154AF (27 B cada, com o marcador de 0x01E487BC na frente: 02 10 no valor
 *    gravado, dois espacos nas outras opcoes, nada no nome, no "NONE" e no tipo 8), cursor na linha
 *    b[0x14C10], editando em b[0x14BF9], tipo em b[0x14C00] (7 editando = o editor de nome, que fica
 *    o de fabrica);
 *  - scan QT (modo 17), 0x01E45A82(rodando): o titulo e STOP ou RUNNING;
 *  - radio (modo 2): os enderecos dos RADIO_* abaixo.
 */
#define MENU_TITULO      0x00015494u
#define MENU_LINHAS      0x000154AFu
#define MENU_NUMERO      RAM(u8, 0x00014BFA)
#define MENU_CURSOR      RAM(u8, 0x00014C10)
#define MENU_EDITANDO    RAM(u8, 0x00014BF9)
#define MENU_TIPO        RAM(u8, 0x00014C00)

#define RADIO_BANDA      RAM(u8, 0x00012A1B)    /* b[0x129D8 + 67], 0x01E44FCE: 0 FM, 1 AM, 2 LSB, 3 USB, 4 CW */
#define RADIO_FAIXA_AM   RAM(u8, 0x00014CA5)    /* b[0x14680 + 1573], 0x01E45016: LW, MW, SW, USW */
#define RADIO_FREQ       RAM(u16, 0x00014CA6)   /* h[0x14680 + 787 << 1], 0x01E449DA: FM em 10 kHz, o resto em kHz */
#define RADIO_MEMORIAS   RAM(u8, 0x000129F9)    /* b[0x129D8 + 33], 0x01E44AD2: 1 = navegando pelas memorias */
#define RADIO_MEMORIA    RAM(u8, 0x00014CB4)    /* b[0x14680 + 1588], 0x01E44AE2 */
#define RADIO_SINAL      RAM(u8, 0x00014CCF)    /* b[0x14680 + 1615], 0x01E451A6 */
#define RADIO_NIVEL      RAM(u8, 0x00014CCE)    /* b[0x14680 + 1614], 0x01E451BA: o sinal / 6 ja desenhado */
#define RADIO_PASSO_AM   RAM(u8, 0x00012A6F)    /* b[0x129D8 + 151], 0x01E44D22 */
#define RADIO_PASSO_SSB  RAM(u8, 0x00012A6E)    /* b[0x129D8 + 150], 0x01E44D30 */
#define RADIO_LARGURA    RAM(u8, 0x00014CBA)    /* b[0x14680 + 1594], 0x01E44DC6 */
#define RADIO_LNA        RAM(u8, 0x00014CBB)    /* b[0x14680 + 1595], 0x01E44E4E */
#define RADIO_BFO        RAM(u16, 0x00014CBC)   /* h[0x14680 + 798 << 1], 0x01E44F16, com sinal */
#define RADIO_ITEM       RAM(u32, 0x00014CD4)   /* [0x14680 + 405 << 2], 0x01E4934A: 0 = a frequencia */
#define DIGITA_MODO      RAM(u8, 0x00014A36)    /* b[0x14680 + 950], 0x01E490A0: zeros na frente do digitado */
/* O editor de nome (tipo 7 editando): o texto e o tamanho sao os do digitado (DIGITOS, DIGITADOS), o
 * maximo e o mesmo byte do DIGITA_MODO (o get do item o grava, 0x01E3C732 e o nome_le do bancos.c). */
#define EDITOR_MODO      RAM(u8, 0x00014A38)    /* b[0x14680 + 952], 0x01E48724: 0 abc, 1 ABC, 2 123 (3 = pinyin, so em chines) */
#define EDITOR_DESENHADO RAM(u8, 0x00014686)    /* b[0x14680 + 6], 0x01E48732: o modo ja desenhado; 0xFF ao abrir (0x01E3AC52) */
#define EDITOR_MAX       DIGITA_MODO

FIRMWARE(LE_FLASH, 0x01E390AC);  void LE_FLASH(u32, void *, unsigned);
FIRMWARE(LE_TENSAO, 0x01E45FB0); unsigned LE_TENSAO(void);     /* o ADC da bateria (canal 6), 8 bits */

/* A parte [y0, y1) da tela cruza a janela aberta: as partes de fora nem se montam. */
#define NA_JANELA(y0, y1) (JAN_Y0 < (y1) && JAN_Y1 > (y0))

/* O lado em evidencia em resumo, para as telas que ocupam so a metade de baixo (menu e scan QT):
 * cartao baixo com selo, o texto da exibicao, o canal e a linha de informacoes. */
#define ALTURA_RESUMO 36

SEPARADA static void resumo(const struct estado *e, unsigned y)
{
    unsigned i = e->evidencia;
    const struct lado *l = &e->lado[i];
    char s[16];

    pinta(3, y, 157, y + ALTURA_RESUMO, COR_VERDE);
    pinta(4, y + 1, 156, y + ALTURA_RESUMO - 1, COR_CARTAO);
    pinta(8, y + 4, 19, y + 15, COR_VERDE);
    letra(11, y + 6, 'A' + i, 1, COR_FUNDO);
    if (!l->canal || l->exibicao == 1)
        frequencia(s, l->freq, 5);
    else if (l->exibicao == 0)
        nome_ou_sem(s, i);
    else
        texto_canal(s, l->canal);
    texto(24, y + 6, s, 1, COR_BRANCO);
    if (!l->canal)
        etiqueta(152, y + 4, "VFO", COR_CHIP_CANAL, COR_TEXTO, 1);
    else if (l->exibicao != 2) {
        texto_canal(s, l->canal);
        etiqueta(152, y + 4, s, COR_CHIP_CANAL, COR_TEXTO, 1);
    }
    informacoes(l, y + 20);
}

/* Texto no tamanho medio se couber na largura, senao em 1x (centrado na altura do medio). */
static void texto_medio(unsigned x, unsigned y, const char *s, unsigned largura_max, unsigned cor)
{
    if (largura(s, MEDIA) <= largura_max)
        texto(x, y, s, MEDIA, cor);
    else
        texto(x, y + 1, s, 1, cor);
}

/* Copia ate n caracteres de um texto da RAM (para em 0), sem os espacos do fim - alguns textos de
 * fabrica vem de "%-16s". Devolve o comprimento. */
static unsigned copia_ram(char *s, u32 a, unsigned n)
{
    unsigned c, i = 0;

    while (i < n && (c = RAM(u8, a + i)) != 0)
        s[i++] = c;
    while (i && s[i - 1] == ' ')
        i--;
    s[i] = 0;
    return i;
}

/* ---------- menu ---------- */

struct menu_b {
    u8 numero, cursor, editando, editor;
};

#define TOPO_MENU   56
#define TOPO_EDITOR (TOPO_MENU + 21)
#define LIGADO      0x01        /* no fim da opcao: ela esta ligada (BANK e BANK CH, bancos.c) - sai em ambar */

/* O editor de nome: tipo 7 editando (o motor de menus vai para 0x01E48DAC em vez do titulo e das
 * linhas). Desde a v38 tambem na tela B (os ganchos dele estao no tela.c). */
static unsigned menu_editor(void)
{
    return MENU_TIPO == 7 && MENU_EDITANDO == 1;
}

/* O titulo e as linhas ficam na RAM e sao lidos na hora de desenhar, para nao pesarem na pilha. O
 * nome (tipo 7) tem uma linha so e o motor nao mexe no cursor dele: e sempre a linha 0. */
static void le_menu(struct menu_b *m)
{
    m->numero = MENU_NUMERO;
    m->cursor = MENU_TIPO == 7 ? 0 : MENU_CURSOR;
    m->editando = MENU_EDITANDO == 1;
    m->editor = menu_editor();
}

/* O texto do editor de nome num cartao de borda verde, em 2x se couber (no medio, senao), com o
 * cursor ambar no fim - o editor so acrescenta e apaga no fim - e quantos caracteres de quantos. */
SEPARADA static void editor_cena(void)
{
    unsigned n = DIGITADOS, i, e, w, y = TOPO_EDITOR;
    char s[24];

    for (i = 0; i < n && i < sizeof s - 1; i++)
        s[i] = RAM(u8, DIGITOS + i);
    s[i] = 0;
    pinta(3, y, 157, y + 34, COR_VERDE);
    pinta(4, y + 1, 156, y + 33, COR_CARTAO);
    e = largura(s, 2) + 14 <= 144 ? 2 : MEDIA;
    w = largura(s, e) + (s[0] ? e ? 2 : 1 : 0);
    texto(10, e ? y + 10 : y + 12, s, e, COR_BRANCO);
    pinta(10 + w, e ? y + 25 : y + 23, 10 + w + (e ? 10 : 6), e ? y + 27 : y + 25, COR_TEXTO_AMBAR);
    numero(junta(numero(s, n, 1), "/"), EDITOR_MAX, 1);
    texto(156 - largura(s, 1), y + 38, s, 1, COR_CINZA);
}

/* O titulo e as tres linhas do menu, no tamanho medio, ou o editor de nome (a barra e o resumo sao
 * desenhados pela cena_telas, ao lado). */
SEPARADA static void menu_cena(const struct menu_b *m)
{
    static const char *const modos[3] = {ICONE_MIN_A ICONE_MIN_B ICONE_MIN_C, "ABC", "123"};
    unsigned r, x, x1 = 154;
    char s[28];

    /* cabecalho: o numero numa etiqueta verde e o titulo; no editor, o modo numa etiqueta ambar */
    if (NA_JANELA(TOPO_MENU, TOPO_MENU + 18)) {
        if (m->editor && EDITOR_MODO < 3)
            x1 = etiqueta(156, TOPO_MENU + 3, modos[EDITOR_MODO], COR_CHIP_AMBAR, COR_TEXTO_AMBAR, 1) - 3;
        numero(s, m->numero, 2);
        x = etiqueta(4, TOPO_MENU + 3, s, COR_CHIP_RX, COR_TEXTO_RX, 0);
        copia_ram(s, MENU_TITULO, 26);
        texto_medio(x + 2, TOPO_MENU + 4, s, x1 - x, COR_BRANCO);
        pinta(0, TOPO_MENU + 17, 160, TOPO_MENU + 18, COR_LINHA);
    }
    if (m->editor) {
        if (NA_JANELA(TOPO_MENU + 18, 128))
            editor_cena();
        return;
    }
    /* as tres linhas: a do cursor num cartao (borda verde editando), o valor gravado com o ponto, a
     * opcao ligada em verde */
    for (r = 0; r < 3; r++) {
        unsigned y = TOPO_MENU + 19 + r * 18, cor = COR_CINZA, salvo, p, n;

        if (!NA_JANELA(y, y + 17))
            continue;
        n = copia_ram(s, MENU_LINHAS + r * 27, 27);
        salvo = n >= 2 && s[0] == 2 && s[1] == 0x10;
        p = salvo || (n >= 2 && s[0] == ' ' && s[1] == ' ') ? 2 : 0;
        if (n <= p)
            continue;
        if (r == m->cursor) {
            pinta(3, y, 157, y + 17, m->editando ? COR_VERDE : COR_LINHA);
            pinta(4, y + 1, 156, y + 16, COR_CARTAO);
            cor = COR_BRANCO;
        }
        if (s[n - 1] == LIGADO) {
            s[n - 1] = 0;
            cor = COR_TEXTO_AMBAR;
        }
        if (salvo) {
            pinta(9, y + 6, 14, y + 11, COR_VERDE);
            pinta(10, y + 5, 13, y + 12, COR_VERDE);
            pinta(8, y + 7, 15, y + 10, COR_VERDE);
        }
        texto_medio(20, y + 4, s + p, 126, cor);
        if (r == m->cursor && m->editando)
            texto(148, y + 5, ICONE_SETAS, 1, COR_VERDE);
    }
}

/* ---------- scan QT ---------- */

#define TOPO_ESTADO_QT (TOPO_MENU + 40)    /* a faixa do estado: so o gancho da 0x01E45A82 a desenha */

SEPARADA static void scanqt_cena(unsigned rodando)
{
    static const char titulo[] = "SCAN QT", busca[] = "CTCSS / DCS";
    const char *estado = rodando ? "SCANNING" : "STOP";
    unsigned l = largura(estado, 2) + 16;

    texto(80 - largura(titulo, 2) / 2, TOPO_MENU + 12, titulo, 2, COR_VERDE);
    texto(80 - largura(busca, 1) / 2, TOPO_MENU + 30, busca, 1, COR_CINZA);
    pinta(80 - l / 2, TOPO_ESTADO_QT + 4, 80 + l / 2, TOPO_ESTADO_QT + 24, rodando ? COR_CHIP_RX : COR_CHIP_AMBAR);
    texto(80 - l / 2 + 8, TOPO_ESTADO_QT + 7, estado, 2, rodando ? COR_TEXTO_RX : COR_TEXTO_AMBAR);
}

/* ---------- radio (receptor de FM, AM e SSB) ---------- */

/* Cada texto do radio, montado na hora de desenhar, num buffer so (16 B). */
#define R_BANDA    0
#define R_FAIXA    1
#define R_FREQ     2
#define R_UNIDADE  3
#define R_MEMORIA  4
#define R_NOME     5
#define R_SINAL    6
#define R_DIGITADA 7
#define R_AJUSTE   8     /* + 0 STEP, 1 BW, 2 LNA, 3 BFO */

/* O que a faixa da frequencia mostra: a frequencia, o que se digita (0x01E49078) ou a busca. */
#define RADIO_NORMAL    0
#define RADIO_DIGITANDO 1
#define RADIO_BUSCANDO  2

static unsigned radio_banda(void)
{
    unsigned b = RADIO_BANDA;

    return b > 4 ? 0 : b;                    /* 0x01E44FB0 trata banda > 4 como FM */
}

SEPARADA static void radio_texto(char *s, unsigned qual)
{
    static const char *const bandas[5] = {"FM", "AM", "LSB", "USB", "CW"};
    static const char *const faixas[4] = {"LW", "MW", "SW", "USW"};
    static const char *const passos[6] = {"1K", "5K", "10K", "100K", "500K", "1M"};
    static const char *const larguras[6] = {"0.5K", "1.0K", "1.2K", "2.2K", "3.0K", "4.0K"};
    unsigned banda = radio_banda(), f = RADIO_FREQ, v;

    s[0] = 0;
    switch (qual) {
    case R_BANDA:
        junta(s, bandas[banda]);
        break;
    case R_FAIXA:                            /* a faixa do AM; no SSB, SW */
        if (banda == 1)
            junta(s, faixas[RADIO_FAIXA_AM > 3 ? 0 : RADIO_FAIXA_AM]);
        else if (banda >= 2)
            junta(s, "SW");
        break;
    case R_FREQ:                             /* 0x01E449BA: FM "%03d.%02d"; as outras em kHz abaixo de 1000 */
        if (banda == 0) {
            s = numero(s, f / 100, 1);
            *s++ = '.';
            numero(s, f % 100, 2);
        } else if (f < 1000)
            numero(s, f, 1);
        else {
            s = numero(s, f / 1000, 1);
            *s++ = '.';
            numero(s, f % 1000, 3);
        }
        break;
    case R_UNIDADE:
        junta(s, banda != 0 && f < 1000 ? "KHZ" : "MHZ");
        break;
    case R_DIGITADA: {                       /* 0x01E49078: a mascara, com os zeros que ela poe na
                                              * frente pelo b[0x14A36], e os digitos de 0x14A3A */
        char d[8];

        for (v = 0; v < DIGITADOS && v < sizeof d - 1; v++)
            d[v] = RAM(u8, DIGITOS + v);
        d[v] = 0;
        v = DIGITA_MODO;
        digitados(s, banda == 0 ? v == 3 ? "0--.--" : "---.--" : v == 3 ? "00---" : v == 4 ? "0----" : "-----", d);
        break;
    }
    case R_MEMORIA:                          /* 0x01E449BA: "%03d" da memoria + 1, so nas memorias */
        if (RADIO_MEMORIAS == 1)
            numero(junta(s, "M"), RADIO_MEMORIA + 1, 3);
        break;
    case R_NOME:                             /* 0x01E44B28: 14 B no chip de dados, so nas memorias */
        if (RADIO_MEMORIAS == 1) {
            unsigned n;

            LE_FLASH((banda == 1 ? 0xC110u : banda == 0 ? 0xC010u : 0xC210u) + RADIO_MEMORIA * 16, s, 14);
            for (n = 0; n < 13 && s[n] != 0 && (u8)s[n] != 0xFF; n++)
                ;
            s[n] = 0;
        }
        break;
    case R_SINAL:                            /* 0x01E45198: abaixo de 60 "S" e o nivel; acima "+" e o excesso */
        v = RADIO_SINAL / 6;
        if (RADIO_SINAL < 60) {
            s[0] = 'S';
            numero(s + 1, v, 1);
        } else {
            s[0] = '+';
            numero(s + 1, v - 9 > 9 ? 9 : v - 9, 1);
            junta(s + 2, "0");
        }
        break;
    case R_AJUSTE + 0:                       /* 0x01E44C68: o passo */
        junta(s, banda == 0 ? "100K" : banda == 1 ? passos[RADIO_PASSO_AM & 3] : passos[RADIO_PASSO_SSB % 6]);
        break;
    case R_AJUSTE + 1:                       /* a largura */
        junta(s, banda == 0 ? "100K" : banda == 1 ? "10K" : larguras[RADIO_LARGURA % 6]);
        break;
    case R_AJUSTE + 2:                       /* o LNA: 0 AGC, senao o valor - 1, negativo acima de 1 */
        v = RADIO_LNA;
        if (v == 0)
            junta(s, "AGC");
        else {
            if (v > 1)
                *s++ = '-';
            numero(s, v - 1, 1);
        }
        break;
    case R_AJUSTE + 3:                       /* o BFO so no SSB, com sinal: a de fabrica faz "%06d" e
                                              * troca o 1o caractere pelo sinal (0x01E44F12..0x01E44F30);
                                              * aqui sem os zeros na frente, para caber na casa */
        if (banda < 2)
            junta(s, "-");
        else {
            int b = (short)RADIO_BFO;

            *s = b < 0 ? '-' : '+';
            numero(s + 1, b < 0 ? (unsigned)-b : (unsigned)b, 1);
        }
        break;
    }
}

/* O radio, sem a barra (a cena_telas a desenha ao lado), so as partes que a janela cruza - o nome,
 * que vem do chip de dados, so e lido quando a faixa dele e desenhada. item: o ajuste em edicao,
 * 0 = a frequencia; mostra: RADIO_NORMAL, RADIO_DIGITANDO ou RADIO_BUSCANDO. */
SEPARADA static void radio_cena(unsigned item, unsigned mostra)
{
    static const char *const rotulo[4] = {"STEP", "BW", "LNA", "BFO"};
    char s[16];
    unsigned i, l, e3, nivel = RADIO_SINAL / 6;

    /* sinal: 9 degraus e o S */
    if (NA_JANELA(16, 36)) {
        for (i = 0; i < 9; i++)
            pinta(8 + i * 11, 22 + (8 - i), 17 + i * 11, 31,
                  i >= nivel ? COR_SEG_FUNDO : i < 5 ? COR_SEG_VERDE : i < 7 ? COR_SEG_AMARELO : COR_SEG_VERMELHO);
        radio_texto(s, R_SINAL);
        etiqueta(152, 21, s, COR_CHIP_RX, COR_TEXTO_RX, 1);
    }
    /* a banda e a faixa numa coluna a esquerda; a frequencia (ou o que se digita) alinhada a direita,
     * em 3x se couber, a seta da edicao antes dela e a unidade depois; buscando, "SEEK..." em ambar */
    if (NA_JANELA(36, 68)) {
        radio_texto(s, R_BANDA);
        etiqueta(6, 41, s, COR_CHIP_RX, COR_TEXTO_RX, 0);
        radio_texto(s, R_FAIXA);
        if (s[0])
            etiqueta(6, 54, s, COR_CHIP_CANAL, COR_TEXTO, 0);
        if (mostra == RADIO_BUSCANDO)
            texto(138 - largura("SEEK...", 2), 45, "SEEK...", 2, COR_TEXTO_AMBAR);
        else {
            radio_texto(s, mostra == RADIO_DIGITANDO ? R_DIGITADA : R_FREQ);
            e3 = largura(s, 3) <= 96 ? 3 : 2;
            l = largura(s, e3);
            texto(138 - l, e3 == 3 ? 38 : 45, s, e3, COR_BRANCO);
            if (item == 0)
                texto(138 - l - 8, 45, ICONE_SETA, 1, COR_TEXTO_AMBAR);
            if (mostra == RADIO_DIGITANDO)    /* a de fabrica digita sempre em MHz no FM e em kHz no resto */
                junta(s, radio_banda() == 0 ? "MHZ" : "KHZ");
            else
                radio_texto(s, R_UNIDADE);
            texto(140, 52, s, 1, COR_CINZA);
        }
    }
    /* o nome da memoria num cartao, com o numero dela numa etiqueta: em 2x, no medio ou em 1x, o
     * maior que couber */
    if (NA_JANELA(68, 88)) {
        unsigned x0 = 4, c = COR_CINZA, e;

        pinta(3, 68, 157, 86, COR_VERDE);
        pinta(4, 69, 156, 85, COR_CARTAO);
        radio_texto(s, R_MEMORIA);
        if (s[0])
            x0 = etiqueta(8, 72, s, COR_CHIP_RX, COR_TEXTO_RX, 0);
        radio_texto(s, R_NOME);
        if (s[0])
            c = COR_BRANCO;
        else
            junta(s, x0 > 4 ? "NO NAME" : "NO STATION NAME");
        l = 156 - x0 - 4;
        e = largura(s, 2) <= l ? 2 : largura(s, MEDIA) <= l ? MEDIA : 1;
        texto(x0 + (156 - x0 - largura(s, e)) / 2, e == 2 ? 71 : e == MEDIA ? 73 : 74, s, e, c);
    }
    /* a fileira de ajustes: rotulo e valor, o em edicao em ambar */
    if (NA_JANELA(88, 128))
        for (i = 0; i < 4; i++) {
            unsigned x0 = 3 + i * 39, edit = item == i + 1;

            pinta(x0, 90, x0 + 37, 125, edit ? COR_TEXTO_AMBAR : COR_LINHA);
            pinta(x0 + 1, 91, x0 + 36, 124, edit ? COR_CHIP_AMBAR : COR_CARTAO);
            texto(x0 + 19 - largura(rotulo[i], 1) / 2, 95, rotulo[i], 1, edit ? COR_TEXTO_AMBAR : COR_CINZA);
            radio_texto(s, R_AJUSTE + i);
            texto(x0 + 19 - largura(s, 1) / 2, 110, s, 1, edit ? COR_TEXTO_AMBAR : COR_BRANCO);
        }
}

/* A espera da partida do radio (0x01E4A780) e da troca de modulacao (0x01E3DE52): no lugar de
 * "Radio Initialization / Please Wait...". */
SEPARADA static void radio_inicio_cena(void)
{
    static const char titulo[] = "RADIO", espera[] = "STARTING...";

    pinta(3, 44, 157, 92, COR_VERDE);
    pinta(4, 45, 156, 91, COR_CARTAO);
    texto(80 - largura(titulo, 3) / 2, 52, titulo, 3, COR_VERDE);
    texto(80 - largura(espera, 1) / 2, 78, espera, 1, COR_CINZA);
}

/* ---------- a tela inteira ou faixas dela ---------- */

#define TELA_INICIO_RADIO 0x80              /* nao e modo de fabrica: so a espera da partida */

struct telas {
    u8 modo, variante, item;    /* variante: no scan QT, 1 = rodando; no radio, o que a frequencia mostra */
    struct menu_b m;
    struct estado e;
};

static void le_telas(struct telas *t, unsigned modo, unsigned variante, unsigned item)
{
    t->modo = modo;
    t->variante = variante;
    t->item = item;
    le_estado(&t->e);
    if (modo == 1)
        le_menu(&t->m);
}

/* A cena de um modo: a barra, o resumo (menu e scan QT) e a parte propria, cada uma chamada daqui,
 * lado a lado, para os quadros delas nao se somarem na pilha. */
static void cena_telas(const struct telas *t)
{
    if (NA_JANELA(0, ALTURA_ESTADO))
        barra_estado(&t->e);
    if ((t->modo == 1 || t->modo == 17) && NA_JANELA(ALTURA_ESTADO, TOPO_MENU))
        resumo(&t->e, ALTURA_ESTADO + 2);
    if (t->modo == 1)
        menu_cena(&t->m);
    else if (t->modo == 2)
        radio_cena(t->item, t->variante);
    else if (t->modo == 17)
        scanqt_cena(t->variante);
    else
        radio_inicio_cena();
}

/* As linhas [y0, y1) da tela, em janelas de ate 32 linhas. */
static void desenha_telas(const struct telas *t, unsigned y0, unsigned y1)
{
    for (; y0 < y1; y0 += 32) {
        janela_b(0, 160, y0, y1 < y0 + 32 ? y1 : y0 + 32, COR_FUNDO);
        cena_telas(t);
        ENVIA();
    }
}

#ifndef PREVIA
/* So o resumo do lado em evidencia (menu e scan QT), sem a estrutura das telas: e o que o gancho do
 * lado desenha, raso, porque o motor de menus o chama no fim de cada redesenho. */
SEPARADA static void desenha_resumo(void)
{
    struct estado e;
    unsigned y;

    le_estado(&e);
    for (y = ALTURA_ESTADO; y < TOPO_MENU; y += 32) {
        janela_b(0, 160, y, TOPO_MENU < y + 32 ? TOPO_MENU : y + 32, COR_FUNDO);
        resumo(&e, ALTURA_ESTADO + 2);
        ENVIA();
    }
}
#endif

/* ---------- a tensao na partida (v38) ----------
 * Com a mensagem de partida em tensao (b[0x14B88] & 3, 0x01E55EBC), a partida limpa a tela, le o ADC
 * (0x01E45FB0), faz x 444 / 1000 (decimos de volt) e escreve "   Volta" e a tensao por cima da 8a
 * posicao de "   Voltage" - o "ge" se perdia. A tela B a mostra num cartao, com a mesma conta. E rasa
 * (sem a estrutura das outras telas) porque roda na partida, que ja e funda. */
SEPARADA static void tensao_cena(unsigned decimos)
{
    static const char titulo[] = "VOLTAGE";
    char s[8], *p;

    pinta(3, 40, 157, 96, COR_VERDE);
    pinta(4, 41, 156, 95, COR_CARTAO);
    texto(80 - largura(titulo, MEDIA) / 2, 50, titulo, MEDIA, COR_CINZA);
    p = numero(s, decimos / 10, 1);
    *p++ = '.';
    p = numero(p, decimos % 10, 1);
    junta(p, "V");
    texto(80 - largura(s, 3) / 2, 66, s, 3, COR_BRANCO);
}

#ifdef PREVIA
/* A previa desenha a tela inteira de cada modo, lendo a RAM imitada. */
void telas_b(unsigned modo, unsigned variante)
{
    struct telas t;

    le_telas(&t, modo, variante, modo == 2 ? RADIO_ITEM : 0);
    desenha_telas(&t, 0, 128);
}

void tensao_b(unsigned decimos)
{
    unsigned y;

    for (y = 0; y < 128; y += 32) {
        janela_b(0, 160, y, y + 32, COR_FUNDO);
        tensao_cena(decimos);
        ENVIA();
    }
}
#endif
