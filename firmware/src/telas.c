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
    letra(11, y + 6, 'A' + LETRA(e, i), 1, COR_FUNDO);   /* a C no lado B e a C (v50) */
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
    informacoes(l, y + 20, 1);           /* com o NO TX, como no DUAL (v82) */
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

/* Os grupos do menu (v53). O menu principal tem dois niveis: a tabela (menu_tabela, que o montar.py
 * monta) traz primeiro os grupos, um item cada, e depois os itens de cada grupo, na ordem dos grupos,
 * tudo em ordem alfabetica. O firmware so conhece a tabela corrente, no estado do menu em 0x14BF4: o
 * ponteiro dela [+0], quantos itens [+4], editando [+5], o item [+6] e [+8], o numero digitado [+10] e
 * o tipo do item [+12] - o que a entrada do menu (0x01E4CBC4..0x01E4CBEC), os menus rapidos
 * (0x01E4B3DC..0x01E4B402) e as setas (0x01E49FA6..0x01E49FCC) escrevem. Entrar num grupo e por a
 * tabela dele ali, e voltar e por a dos grupos (os ganchos estao no tela.c). O nivel sai do ponteiro.
 * Cada grupo e um item de lista sem get nem set, com a quantidade de itens no maximo e os nomes deles
 * nas opcoes: o motor de menus monta as linhas com eles, que a tela B nao mostra nesse nivel.
 *
 * O menu do radio (v56) e o mesmo motor, no modo de menu, com uma de duas tabelas de fabrica (a
 * entrada, 0x01E49000: FM e AM numa, SSB e CW na outra, 532 B adiante) e quantos itens a banda tem
 * (0x01E3DE36: FM 2, AM 5, SSB e CW 6). Ele fica numa pasta so, sem grupos. */
#define MENU_TABELA       RAM(u32, 0x00014BF4)
#define MENU_CONTA        RAM(u8, 0x00014BF8)
#define MENU_ITEM         RAM(u16, 0x00014BFC)   /* o item de novo, fora da edicao (as setas, 0x01E49F78) */
#define MENU_DIGITADO     RAM(u16, 0x00014BFE)   /* o numero digitado (0x01E4A2CE) */
#define MENU_TEMPO_DIGITO RAM(u8, 0x00014C0C)    /* 15 a cada digito, e o tique o desce (0x01E4A326) */
#define TAM_ITEM          28
#define RADIO             0xFE                   /* menu_nivel(): o menu do radio (v56) */
#define FORA              0xFF                   /* menu_nivel(): uma tabela que nao e a nossa (os rapidos) */
#define MARCA_SEM_VALOR   1                      /* as marcas de cada item (v55, montar.py MENU_MARCAS) */
#define MARCA_VARIOS      2
#define RADIO_FM_AM       0x01E61168u            /* as tabelas do menu do radio (0x01E49026) */
#define RADIO_SSB         (RADIO_FM_AM + 532)

#ifdef PREVIA
unsigned previa_menu_grupos(void);
unsigned previa_menu_itens(unsigned g);
unsigned previa_menu_marcas(unsigned p);
unsigned previa_item_flags(u32 t, unsigned i);
const char *previa_item_nome(u32 t, unsigned i);
#define MENU_BASE         0x01E30000u
#define MENU_GRUPOS       previa_menu_grupos()
#define MENU_ITENS(g)     previa_menu_itens(g)
#define MENU_MARCAS(p)    previa_menu_marcas(p)
#define ITEM_FLAGS(t, i)  previa_item_flags(t, i)
#else
/* Os dois vetores que o montar.py preenche, no fim do tela.c. Lidos byte a byte: o item tem 28 B
 * sem alinhamento, e o compilador nao pode usar o conteudo que ele ve no fonte (0xFF e zeros). */
extern const unsigned char menu_tabela[], menu_textos[];
#define MENU_BASE         ((u32)menu_tabela)
#define MENU_GRUPOS       (((const volatile u8 *)menu_textos)[0])
#define MENU_MARCAS(p)    (((const volatile u8 *)menu_textos)[4 + (p)])
#define MENU_ITENS(g)     (RAM(u8, MENU_BASE + (g) * TAM_ITEM + 1) | RAM(u8, MENU_BASE + (g) * TAM_ITEM + 2) << 8)
#define ITEM_FLAGS(t, i)  RAM(u8, (t) + (i) * TAM_ITEM)
#endif

/* O nome do item i da tabela t: o ponteiro em +11 ou, no item com titulo por funcao (flags 0x20: so o
 * de memoria do radio, 0x01E3D20E), o que ela monta - o prefixo da banda e MEMCH -, em s. */
static const char *item_nome(u32 t, unsigned i, char *s)
{
#ifdef PREVIA
    (void)s;
    return previa_item_nome(t, i);
#else
    u32 a = t + i * TAM_ITEM + 11;

    if (ITEM_FLAGS(t, i) & 0x20) {
        junta(junta(s, RADIO_BANDA == 0 ? "FM" : RADIO_BANDA == 1 ? "AM" : "SSB"), "MEMCH");
        return s;
    }
    return (const char *)(RAM(u8, a) | RAM(u8, a + 1) << 8 | RAM(u8, a + 2) << 16 | (u32)RAM(u8, a + 3) << 24);
#endif
}

/* Onde comecam os itens do grupo g na tabela (em itens): depois dos grupos e dos itens dos anteriores. */
static unsigned menu_inicio(unsigned g)
{
    unsigned p = MENU_GRUPOS, i;

    for (i = 0; i < g; i++)
        p += MENU_ITENS(i);
    return p;
}

/* 0 nos grupos, g + 1 dentro do grupo g, RADIO no menu do radio, FORA em outra tabela (os menus
 * rapidos, que ficam como os de fabrica). */
static unsigned menu_nivel(void)
{
    u32 t = MENU_TABELA;
    unsigned g, n = MENU_GRUPOS, p = n;

    if (t == MENU_BASE)
        return 0;
    if (t == RADIO_FM_AM || t == RADIO_SSB)
        return RADIO;
    for (g = 0; g < n; g++) {
        if (t == MENU_BASE + p * TAM_ITEM)
            return g + 1;
        p += MENU_ITENS(g);
    }
    return FORA;
}

/* A janela de pasta (v55): escolhendo o grupo, e escolhendo o item dentro dele ou no menu do radio -
 * tudo menos o item aberto (editando) e as tabelas que nao sao as nossas. */
static unsigned menu_pasta(void)
{
    unsigned n = menu_nivel();

    return n == 0 || (n != FORA && MENU_EDITANDO != 1);
}

struct menu_b {
    u8 numero, cursor, editando, editor, nivel, pasta;
};

#define TOPO_MENU   56
#define TOPO_EDITOR (TOPO_MENU + 21)
#define LIGADO      0x01        /* no fim da opcao: ela esta ligada (BANK e CHANNEL dos BANKS, bancos.c) - sai em ambar */

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
    unsigned tipo = MENU_TIPO;

    m->numero = MENU_NUMERO;
    m->cursor = tipo == 7 ? 0 : tipo == 8 ? 3 : MENU_CURSOR;    /* o VERSION aberto e so leitura (v55) */
    m->editando = MENU_EDITANDO == 1 && tipo != 8;
    m->editor = menu_editor();
    m->nivel = menu_nivel();
    m->pasta = menu_pasta();
}

/* A letra do menu (v56), um pouco maior que a media para ler melhor: as colunas do medio, a linha do
 * meio tambem com 2 px (linha_janela, tela.c: 10 de altura) e um px a mais entre as letras. O que nao
 * couber em largura_max sai no medio - os nomes mais compridos, como o NOISE REDUCTION. */
#define ALTURA_MENU 10

static void texto_menu(unsigned x, unsigned y, const char *s, unsigned largura_max, unsigned cor)
{
    const char *t;
    unsigned l = 0;

    for (t = s; *t; t++)
        l += avanco(fonte_b_letra[indice((u8)*t)], MEDIA) + 1;
    if (l > largura_max + 2) {
        texto_medio(x, y + 1, s, largura_max, cor);
        return;
    }
    while (*s)
        x += letra_media(x, y, (u8)*s++, cor, linha_janela) + 1;
}

/* Quantas letras do comeco de s cabem nos 140 px do cartao, no tamanho e. */
static unsigned cabem(const char *s, unsigned e)
{
    unsigned k = 0, l = 0;

    while (s[k] && (l += avanco(fonte_b_letra[indice((u8)s[k])], e)) <= 141)
        k++;
    return k;
}

/* O texto do editor num cartao de borda verde, em 2x se couber (no medio, senao), com o cursor ambar
 * no fim e quantos caracteres de quantos. As mensagens do Morse e do APRS (v65) vao a 37: o que nao
 * cabe numa linha do medio sai em duas, no medio ou, se nem assim couber, no pequeno. */
SEPARADA static void editor_cena(void)
{
    unsigned n = DIGITADOS, i, e, w, k, y = TOPO_EDITOR;
    char s[38];

    for (i = 0; i < n && i < sizeof s - 1; i++)
        s[i] = RAM(u8, DIGITOS + i);
    s[i] = 0;
    pinta(3, y, 157, y + 34, COR_VERDE);
    pinta(4, y + 1, 156, y + 33, COR_CARTAO);
    e = largura(s, 2) + 14 <= 144 ? 2 : MEDIA;
    k = cabem(s, MEDIA);
    if (!s[k]) {
        w = largura(s, e) + (s[0] ? e ? 2 : 1 : 0);
        texto(10, e ? y + 10 : y + 12, s, e, COR_BRANCO);
        pinta(10 + w, e ? y + 25 : y + 23, 10 + w + (e ? 10 : 6), e ? y + 27 : y + 25, COR_TEXTO_AMBAR);
    } else {
        e = s[k + cabem(s + k, MEDIA)] ? 1 : MEDIA;
        k = cabem(s, e);
        texto(10, y + 18, s + k, e, COR_BRANCO);
        w = largura(s + k, e) + (e ? 2 : 1);
        s[k] = 0;
        texto(10, y + 4, s, e, COR_BRANCO);
        pinta(10 + w, y + 28, 10 + w + (e ? 5 : 6), y + 30, COR_TEXTO_AMBAR);
    }
    numero(junta(numero(s, n, 1), "/"), EDITOR_MAX, 1);
    texto(156 - largura(s, 1), y + 38, s, 1, COR_CINZA);
}

/* O item aberto: o titulo e as tres linhas do menu, na letra do menu, ou o editor de nome (a barra e o
 * resumo sao desenhados pela cena_telas, ao lado). */
SEPARADA static void menu_cena(const struct menu_b *m)
{
    static const char *const modos[2] = {"ABC", "123"};
    unsigned r, x = 4, x1 = 154;
    char s[28];

    /* cabecalho: o numero numa etiqueta verde e o titulo; no editor, o modo numa etiqueta ambar.
     * Dentro de um grupo, a pasta dele em ambar e "grupo-item", contando de 1 (v53), e no menu do
     * radio a pasta e o item (v56); nos menus rapidos, o numero de fabrica, de 0 - e o que o digito
     * deles alcanca. */
    if (NA_JANELA(TOPO_MENU, TOPO_MENU + 18)) {
        if (m->editor && EDITOR_MODO - 1u < 2)
            x1 = etiqueta(156, TOPO_MENU + 3, modos[EDITOR_MODO - 1], COR_CHIP_AMBAR, COR_TEXTO_AMBAR, 1) - 3;
        if (m->nivel == FORA)
            numero(s, m->numero, 2);
        else {
            x = texto(4, TOPO_MENU + 5, ICONE_PASTA, 1, COR_TEXTO_AMBAR) + 2;
            numero(m->nivel == RADIO ? s : junta(numero(s, m->nivel, 1), "-"), m->numero + 1, 1);
        }
        x = etiqueta(x, TOPO_MENU + 3, s, COR_CHIP_RX, COR_TEXTO_RX, 0);
        copia_ram(s, MENU_TITULO, 26);
        texto_menu(x + 2, TOPO_MENU + 4, s, x1 - x - 2, COR_BRANCO);
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
        texto_menu(20, y + 4, s + p, 126, cor);
        if (r == m->cursor && m->editando)
            texto(148, y + 5, ICONE_SETAS, 1, COR_VERDE);
    }
}

/* O valor do item do cursor, das linhas que o motor acabou de montar (0x01E487BC; o titulo, cujo envio
 * desenha a janela, sai depois delas, 0x01E48E82): a marcada como gravada (02 10) ou, sem marca (o
 * nome, o OFFSET, o VERSION e o BFO), a primeira, sem os dois espacos. Sem o LIGADO do fim (o BANK
 * ligado), e ate VALOR_MAX letras, mais do que cabe na linha do "L" (o maior, HARDWARE:V01T32, tem 15).
 * Devolve o tamanho. */
#define VALOR_MAX 19

static unsigned valor_do_cursor(char *s)
{
    unsigned r, a = MENU_LINHAS, n;

    for (r = 0; r < 3; r++)
        if (RAM(u8, MENU_LINHAS + r * 27) == 2 && RAM(u8, MENU_LINHAS + r * 27 + 1) == 0x10) {
            a = MENU_LINHAS + r * 27 + 2;
            break;
        }
    if (a == MENU_LINHAS && RAM(u8, a) == ' ' && RAM(u8, a + 1) == ' ')
        a += 2;
    n = copia_ram(s, a, VALOR_MAX);
    if (n && s[n - 1] == LIGADO)
        s[--n] = 0;
    return n;
}

/* Triangulo de 5 px de largura e 3 de altura, com a ponta para cima ou para baixo, a partir de (x, y). */
static void triangulo(unsigned x, unsigned y, unsigned para_baixo, unsigned cor)
{
    unsigned i;

    for (i = 0; i < 3; i++)
        pinta(x + 2 - i, y + (para_baixo ? 2 - i : i), x + 3 + i, y + (para_baixo ? 3 - i : i + 1), cor);
}

/* A janela de pasta, de baixo da barra ao fim, no lugar do resumo e das linhas: os grupos (v53), os
 * itens de um grupo (v55) e o menu do radio (v56). Na barra de titulo a pasta, o numero e o nome do
 * grupo (no radio, "RADIO" e a banda); no corpo, os itens numerados como ficam abertos; embaixo, nos
 * grupos, um ponto por grupo com o deste aceso, e as setas. Escolhendo o grupo, a barra de titulo fica
 * acesa, com o "OK" que o abre, e o corpo mostra os itens dele. Escolhendo o item, a barra apaga e o
 * item do cursor fica num cartao, com a seta do OK e o valor dele embaixo, num "L" verde que o cursor
 * nao seleciona (os itens de acao nao tem). Se nao couber, a lista rola para o cursor e o valor ficarem
 * a vista, com as setas do que ficou escondido e, entre elas, o risco com a barra da posicao, mais
 * grossa que ele. Ambar e a cor de grupo; verde, a de item. */
/* As bandas do radio (0x01E603FC) e a de agora: 0x01E44FB0 trata banda > 4 como FM. Para o titulo
 * do menu do radio (v56) e para o radio_texto, mais abaixo. */
static const char *const bandas_radio[5] = {"FM", "AM", "LSB", "USB", "CW"};

static unsigned radio_banda(void)
{
    unsigned b = RADIO_BANDA;

    return b > 4 ? 0 : b;
}

#define JANELA_Y      (ALTURA_ESTADO + 2)
#define JANELA_CORPO  (JANELA_Y + 19)
#define JANELA_FIM    126
#define JANELA_PASSO  12                   /* a letra do menu (10 de altura) e 2 px entre as linhas */
#define JANELA_LINHAS 6
#define ROLA_X        151                  /* a barra de rolagem: [151, 154) */

SEPARADA static void pasta_cena(const struct menu_b *m)
{
    unsigned radio = m->nivel == RADIO, escolhendo = m->nivel != 0, cursor = escolhendo ? m->numero : FORA;
    unsigned g = radio ? 0 : escolhendo ? m->nivel - 1u : m->numero, total = radio ? 0 : MENU_GRUPOS;
    u32 t = escolhendo ? MENU_TABELA : MENU_BASE + menu_inicio(g) * TAM_ITEM;
    unsigned n = escolhendo ? MENU_CONTA : MENU_ITENS(g), valor = 0, linhas, inicio = 0, k, i, x, y;
    char s[12], v[VALOR_MAX + 1];           /* s: numeros, "RADIO " e a banda, o nome da memoria do radio */

    if (escolhendo && (radio ? (ITEM_FLAGS(t, cursor) & 0x0F) != 4
                             : !(MENU_MARCAS((t - MENU_BASE) / TAM_ITEM + cursor) & MARCA_SEM_VALOR)))
        valor = valor_do_cursor(v) != 0;   /* sem "L": as acoes (no radio, a memoria) */
    linhas = n + valor;
    if (escolhendo && linhas > JANELA_LINHAS && cursor + valor >= JANELA_LINHAS)
        inicio = cursor + valor - (JANELA_LINHAS - 1);    /* escolhendo o grupo, do comeco (v57) */

    pinta(3, JANELA_Y, 157, JANELA_FIM, COR_TEXTO_AMBAR);
    pinta(4, JANELA_Y + 1, 156, JANELA_CORPO - 1, escolhendo ? COR_ESTADO : COR_CHIP_AMBAR);
    pinta(4, JANELA_CORPO, 156, JANELA_FIM - 1, COR_ESTADO);
    texto(8, JANELA_Y + 3, ICONE_PASTA, 2, COR_TEXTO_AMBAR);
    if (radio) {
        junta(junta(s, "RADIO "), bandas_radio[radio_banda()]);
        texto_menu(27, JANELA_Y + 4, s, 100, COR_CINZA);
    } else {
        numero(s, g + 1, 1);
        x = texto(27, JANELA_Y + 5, s, MEDIA, COR_TEXTO_AMBAR) + 5;
        texto_menu(x, JANELA_Y + 4, item_nome(MENU_BASE, g, s), 130 - x, escolhendo ? COR_CINZA : COR_BRANCO);
    }
    if (!escolhendo) {
        x = texto(136, JANELA_Y + 6, "OK", 1, COR_TEXTO) + 1;
        texto(x, JANELA_Y + 6, ICONE_SETA, 1, COR_TEXTO_AMBAR);
    }

    /* as linhas a vista: os itens e, logo depois do item do cursor, o valor dele */
    for (k = 0, y = JANELA_CORPO + 3; k < JANELA_LINHAS && inicio + k < linhas; k++, y += JANELA_PASSO) {
        unsigned l = inicio + k;

        /* o cartao do cursor cobre tambem a linha do valor: a faixa de 32 linhas que so pega essa parte
         * dele tambem o pinta (v58; ate a v57 o 3o item, y 63..87, perdia o que passa de y 80) */
        if (!NA_JANELA(y - 2, y + JANELA_PASSO * (1 + (valor && l == cursor))))
            continue;
        if (valor && l == cursor + 1) {           /* o "L": o valor, que o cursor nao seleciona */
            pinta(13, y - 2, 14, y + 5, COR_VERDE);
            pinta(13, y + 5, 18, y + 6, COR_VERDE);
            texto_menu(21, y, v, 138 - 21, COR_VERDE);
            continue;
        }
        i = l - (valor && l > cursor);
        if (i == cursor) {
            pinta(5, y - 1, 148, y + ALTURA_MENU + 1 + valor * JANELA_PASSO, COR_CHIP_AMBAR);
            texto(141, y + 2, ICONE_SETA, 1, COR_TEXTO_AMBAR);
        }
        numero(s, i + 1, 1);
        texto(9, y + 2, s, 1, COR_TEXTO_AMBAR);
        texto_menu(20, y, item_nome(t, i, s), 138 - 20, i == cursor || !escolhendo ? COR_BRANCO : COR_TEXTO);
    }

    /* a rolagem: as setas do que ficou escondido e, entre elas, o risco com a barra da posicao */
    if (linhas > JANELA_LINHAS) {
        unsigned topo = JANELA_CORPO + 7, altura = JANELA_LINHAS * JANELA_PASSO - 13;

        if (inicio > 0)
            triangulo(ROLA_X - 1, JANELA_CORPO + 2, 0, COR_TEXTO_AMBAR);
        if (inicio + JANELA_LINHAS < linhas)
            triangulo(ROLA_X - 1, topo + altura + 2, 1, COR_TEXTO_AMBAR);
        pinta(ROLA_X + 1, topo, ROLA_X + 2, topo + altura, COR_SEG_FUNDO);
        k = altura * JANELA_LINHAS / linhas;
        y = topo + (altura - k) * inicio / (linhas - JANELA_LINHAS);
        pinta(ROLA_X, y, ROLA_X + 3, y + k, COR_TEXTO_AMBAR);
    }

    x = 80 - (total * 7 - 4) / 2;
    for (i = 0; i < total; i++, x += 7)
        pinta(x, JANELA_FIM - 7, x + 3, JANELA_FIM - 4, i == g ? COR_TEXTO_AMBAR : COR_SEG_FUNDO);
    texto(140, JANELA_FIM - 9, ICONE_SETAS, 1, COR_CINZA);
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

SEPARADA static void radio_texto(char *s, unsigned qual)
{
    static const char *const faixas[4] = {"LW", "MW", "SW", "USW"};
    static const char *const passos[6] = {"1K", "5K", "10K", "100K", "500K", "1M"};
    static const char *const larguras[6] = {"0.5K", "1.0K", "1.2K", "2.2K", "3.0K", "4.0K"};
    unsigned banda = radio_banda(), f = RADIO_FREQ, v;

    s[0] = 0;
    switch (qual) {
    case R_BANDA:
        junta(s, bandas_radio[banda]);
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

/* ---------- os ajustes do radio, antes de o chip iniciar (v40) ----------
 * O firmware manda ao chip a largura, o BFO e o LNA guardados na memoria do radio sem conferir - so a
 * frequencia do SSB e corrigida (0x01E3DB74) -, e um BW fora de 0..5 o faz ler alem da tabela de
 * filtros (0x01E3D916). Um estado assim travou o radio na entrada em SSB ate a v39 (o valor exato nao
 * ficou provado: o uso em SSB na v25 regravou tudo antes que desse para ler). Aqui cada um volta ao
 * padrao quando sai dos limites que os setters de fabrica usam (0x01E44C68: passo do AM ate 3 e do SSB
 * ate 5, BW ate 5, LNA ate 36, BFO de -32760 a 27240), e cada indice de memoria volta a 0 fora das 15
 * (o maximo do item MEMCH). Valor valido nao muda. */
#define RADIO_MEM_FM     RAM(u8, 0x000129F8)    /* b[0x129D8 + 32], 0x01E4923A: a memoria do FM */
#define RADIO_MEM_AM     RAM(u8, 0x00012A1A)    /* b[0x129D8 + 66], 0x01E491A8 */
#define RADIO_LNA_AM     RAM(u8, 0x00012A1C)    /* b[0x129D8 + 68], 0x01E3DA6A */
#define RADIO_BFO_VFO    RAM(u16, 0x00012A20)   /* h[0x129D8 + 68 + 4], 0x01E3DB42/0x01E3DB4E: o do VFO no SSB */
#define RADIO_SSB_MEMS   0x00012A22u            /* 0x129D8 + 74 (0x01E3DB12): 15 de 5 B, freq (2), BW, BFO (2) */
#define RADIO_MEM_SSB    RAM(u8, 0x00012A6D)    /* b[0x129D8 + 149], 0x01E491C8 */
#define RADIO_LNA_SSB    RAM(u8, 0x00012A70)    /* b[0x129D8 + 152], 0x01E3DCCE */
#define MEMORIAS_RADIO   15

#ifndef PREVIA
/* O BFO que o setter de fabrica aceita: (u16)(v + 0x7FF8) <= 60000 (0x01E44ED6). */
static unsigned bfo_vale(unsigned v)
{
    return (u16)(v + 0x7FF8) <= 60000;
}

static void confere_radio(void)
{
    unsigned i;

    if (RADIO_BANDA > 4)
        RADIO_BANDA = 0;
    if (RADIO_FAIXA_AM > 3)
        RADIO_FAIXA_AM = 0;
    if (RADIO_MEMORIAS > 1)
        RADIO_MEMORIAS = 0;
    if (RADIO_MEM_FM >= MEMORIAS_RADIO)
        RADIO_MEM_FM = 0;
    if (RADIO_MEM_AM >= MEMORIAS_RADIO)
        RADIO_MEM_AM = 0;
    if (RADIO_MEM_SSB >= MEMORIAS_RADIO)
        RADIO_MEM_SSB = 0;
    if (RADIO_PASSO_AM > 3)
        RADIO_PASSO_AM = 0;
    if (RADIO_PASSO_SSB > 5)
        RADIO_PASSO_SSB = 0;
    if (RADIO_LNA_AM > 36)
        RADIO_LNA_AM = 0;
    if (RADIO_LNA_SSB > 36)
        RADIO_LNA_SSB = 0;
    if (!bfo_vale(RADIO_BFO_VFO))
        RADIO_BFO_VFO = 0;
    for (i = 0; i < MEMORIAS_RADIO; i++) {
        u32 m = RADIO_SSB_MEMS + i * 5;

        if (RAM(u8, m + 2) > 5)
            RAM(u8, m + 2) = 4;                 /* a largura do VFO no SSB (0x01E3DB62) */
        if (!bfo_vale(RAM(u8, m + 3) | RAM(u8, m + 4) << 8))
            RAM(u8, m + 3) = RAM(u8, m + 4) = 0;
    }
    /* e as copias que o chip vai receber, que a entrada e a troca de modulacao ja encheram */
    if (RADIO_LARGURA > 5)
        RADIO_LARGURA = 4;
    if (RADIO_LNA > 36)
        RADIO_LNA = 0;
    if (!bfo_vale(RADIO_BFO))
        RADIO_BFO = 0;
}
#endif

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
    unsigned pasta = t->modo == 1 && t->m.pasta;

    if (NA_JANELA(0, ALTURA_ESTADO))
        barra_estado(&t->e);
    if (((t->modo == 1 && !pasta) || t->modo == 17) && NA_JANELA(ALTURA_ESTADO, TOPO_MENU))
        resumo(&t->e, ALTURA_ESTADO + 2);
    if (pasta)
        pasta_cena(&t->m);
    else if (t->modo == 1)
        menu_cena(&t->m);
    else if (t->modo == 2)
        radio_cena(t->item, t->variante);
    else if (t->modo == 17)
        scanqt_cena(t->variante);
    else if (t->modo == TELA_INICIO_RADIO)
        radio_inicio_cena();
    /* no espectro (20) so a barra: cada pedaco vem depois, pela rotina dele */
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

/* ---------- a tensao na partida (v38) e com o 0 segurado (v52) ----------
 * Com a mensagem de partida em tensao (b[0x14B88] & 3, 0x01E55EBC), a partida limpa a tela, le o ADC
 * (0x01E45FB0), faz x 444 / 1000 (decimos de volt) e escreve "   Volta" e a tensao por cima da 8a
 * posicao de "   Voltage" - o "ge" se perdia. Com o 0 segurado (acao 33 das teclas, 0x01E4CE96), a
 * mesma conta numa caixa por cima da tela, 1 s, e depois a tela inteira de novo. A tela B mostra as
 * duas no mesmo cartao. E rasa (sem a estrutura das outras telas) porque roda na partida, que ja e
 * funda. */
#define TENSAO_X0  3                       /* o cartao: [3, 157) x [40, 96) */
#define TENSAO_X1  157
#define TENSAO_Y0  40
#define TENSAO_Y1  96

SEPARADA static void tensao_cena(unsigned decimos)
{
    static const char titulo[] = "VOLTAGE";
    char s[8], *p;

    pinta(TENSAO_X0, TENSAO_Y0, TENSAO_X1, TENSAO_Y1, COR_VERDE);
    pinta(TENSAO_X0 + 1, TENSAO_Y0 + 1, TENSAO_X1 - 1, TENSAO_Y1 - 1, COR_CARTAO);
    texto(80 - largura(titulo, MEDIA) / 2, 50, titulo, MEDIA, COR_CINZA);
    p = numero(s, decimos / 10, 1);
    *p++ = '.';
    p = numero(p, decimos % 10, 1);
    junta(p, "V");
    texto(80 - largura(s, 3) / 2, 66, s, 3, COR_BRANCO);
}

/* O cartao na janela [x0, x1) das linhas [y0, y1), em faixas de ate 32 linhas (o buffer de video): a
 * tela inteira na partida, so a area do cartao por cima da tela no ar com o 0 segurado. */
static void tensao_faixas(unsigned decimos, unsigned x0, unsigned x1, unsigned y0, unsigned y1)
{
    unsigned y;

    for (y = y0; y < y1; y += 32) {
        janela_b(x0, x1, y, y + 32 < y1 ? y + 32 : y1, COR_FUNDO);
        tensao_cena(decimos);
        ENVIA();
    }
}

/* ---------- o espectro (modo 20, v41) ----------
 * O firmware desenha o espectro em pedacos, cada um pela sua rotina e na sua janela (secao 11 de
 * docs/engenharia-reversa.md). A tela B troca cada pedaco pelo seu, na mesma ordem dos de fabrica:
 *   y  16..35   a quantidade de pontos, a frequencia (ou o que se digita) e AM/FM;
 *   y  35..49   o passo, a frequencia do pico da passada e o dBm;
 *   y  49..113  o grafico, 160 x 64, que o laco de fabrica redesenha em 5 fatias de 32 colunas;
 *   y 113..128  as pontas da janela.
 * Cada pedaco so desenha o que e dele, numa janela so dele: o pico e o dBm, por exemplo, mudam em
 * momentos diferentes, e nao ha RAM onde guardar um para redesenhar junto com o outro. */
#define ESP_L1       ALTURA_ESTADO
#define ESP_L2       35
#define ESP_GRAFICO  49
#define ESP_EIXO     113
#define ESP_X_FREQ   34                    /* [0, 34) pontos, [34, 134) frequencia, [134, 160) AM/FM */
#define ESP_X_MODO   134
#define ESP_X_PICO   44                    /* [0, 44) passo, [44, 114) pico, [114, 160) dBm */
#define ESP_X_DBM    114
#define COR_PAINEL   COR_CARTAO

/* A frequencia (em 10 Hz) grande, com os dois ultimos digitos pequenos; digitando, a mascara de
 * fabrica (0x01E4AFC2: "---.-----") com os digitos por cima, na ordem. */
static void esp_freq(u32 f, unsigned digitando)
{
    char s[12], d[8], pequeno[3];
    unsigned i, x;

    if (digitando) {
        for (i = 0; i < DIGITADOS && i < sizeof d - 1; i++)
            d[i] = RAM(u8, DIGITOS + i);
        d[i] = 0;
        digitados(s, "---.-----", d);
    } else {
        frequencia(s, f, 5);
    }
    pequeno[0] = s[7];
    pequeno[1] = s[8];
    pequeno[2] = s[7] = 0;
    x = texto(ESP_X_FREQ + 4, ESP_L1 + 3, s, 2, COR_BRANCO) + 1;
    texto(x, ESP_L1 + 10, pequeno, 1, COR_CINZA);
}

/* n e p chegam ja limitados (espectro_rotulos, bancos.c). */
static void esp_pontos(unsigned n)
{
    static const char rotulos[6][5] = {"X160", "X80", "X40", "X20", "X640", "X320"};

    etiqueta(3, ESP_L1 + 4, rotulos[n], COR_CHIP_CANAL, COR_TEXTO, 0);
}

/* A modulacao do espectro ([0x6ED4]): a de fabrica (0x01E4B04C) escolhe o texto por m & 1, "FM" ou
 * "AM" (a tabela 0x01E5EBB4 tem so os dois). */
static void esp_modo(unsigned m)
{
    etiqueta(157, ESP_L1 + 4, m & 1 ? "AM" : "FM", COR_CHIP_RX, COR_TEXTO_RX, 1);
}

static void esp_passo(unsigned p)
{
    static const char rotulos[6][6] = {"6.25K", "8.33K", "12.5K", "20.0K", "25.0K", "10.0K"};

    etiqueta(3, ESP_L2 + 2, rotulos[p], COR_CHIP_CANAL, COR_TEXTO, 0);
}

static void esp_pico(u32 f)
{
    char s[12];
    unsigned x = texto(ESP_X_PICO + 3, ESP_L2 + 4, ICONE_PICO, 1, COR_VERDE) + 2;

    frequencia(s, f, 5);
    texto(x, ESP_L2 + 4, s, 1, COR_VERDE);
}

/* O dBm do firmware ("%03ddBm"): o numero em branco e a unidade em cinza, encostados a direita. */
static void esp_dbm(int v)
{
    char s[8], *p = s;
    unsigned x = 157 - largura("DBM", 1);

    if (v < 0)
        *p++ = '-';
    numero(p, v < 0 ? -v : v, v < 0 ? 2 : 3);
    texto(x, ESP_L2 + 4, "DBM", 1, COR_CINZA);
    texto(x - 2 - largura(s, 1), ESP_L2 + 4, s, 1, COR_BRANCO);
}

/* As pontas da janela, embaixo do grafico, com a linha de base e as marcas do comeco, do meio e do
 * fim (as tres de fabrica, x 14, 80 e 145, ficavam embaixo das pontas). */
static void esp_eixo(u32 inicio, u32 fim)
{
    char s[12];

    pinta(0, ESP_EIXO, 160, ESP_EIXO + 1, COR_LINHA);
    pinta(0, ESP_EIXO + 1, 1, ESP_EIXO + 4, COR_LINHA);
    pinta(79, ESP_EIXO + 1, 81, ESP_EIXO + 4, COR_LINHA);
    pinta(159, ESP_EIXO + 1, 160, ESP_EIXO + 4, COR_LINHA);
    frequencia(s, inicio, 5);
    texto(2, ESP_EIXO + 6, s, 1, COR_CINZA);
    frequencia(s, fim, 5);
    texto(158 - largura(s, 1), ESP_EIXO + 6, s, 1, COR_CINZA);
}

/* O grafico, em coordenadas da tela, sobre o painel: as linhas de 1/4 em 1/4 de [x0, x1), uma coluna
 * (altura 0..64; a que esta sendo medida em branco, as outras em verde com o topo claro) e a linha
 * do limite em y, tracejada em ambar. */
static void esp_grade(unsigned x0, unsigned x1)
{
    unsigned y;

    for (y = ESP_GRAFICO + 16; y < ESP_GRAFICO + 64; y += 16)
        pinta(x0, y, x1, y + 1, COR_CHIP_CANAL);
}

static void esp_coluna(unsigned x, unsigned altura, unsigned atual)
{
    unsigned topo = ESP_GRAFICO + 64 - altura;

    if (!altura)
        return;
    pinta(x, topo, x + 1, ESP_GRAFICO + 64, atual ? COR_BRANCO : COR_SEG_VERDE);
    if (!atual)
        pinta(x, topo, x + 1, topo + 1, COR_TEXTO_RX);
}

static void esp_limite(unsigned x0, unsigned x1, unsigned y)
{
    for (x0 &= ~3u; x0 < x1; x0 += 4)
        pinta(x0, y, x0 + 2, y + 1, COR_TEXTO_AMBAR);
}

/* ---------- APRS RX (v65; v70 com o historico e os contadores; v71 com o MDF; v74 lido da flash) ----------
 * A tela do laco de recepcao do APRS (mensagem.c, aprs_rx_grava), a tela inteira: no alto onde se ouve
 * (como o MDF do lado pede), o FILTER (cinza se a tecla 0 o desligou, v111) e a posicao no historico; dois
 * registros do historico em duas linhas cada, o de cima o mais novo e num fundo proprio; aberto, so o de
 * cima, inteiro (ate 6 linhas); a mensagem para o meu indicativo em ambar; embaixo, num fundo proprio, os
 * contadores (S: syncs na marca e no espaco; B: FCS errado; F: consertados), a tecla 0 e as outras teclas.
 * num: o FILTER desligado, posicao, quantos, syncs na marca, syncs no espaco, BAD, FIX. Os textos (ate TEXTO_MAX, 120) ficam so na flash, e
 * cada linha e lida dela na hora de desenhar (aprs_linhas): o laco que chama ja tem o quadro e o
 * decodificador na pilha, e esta cadeia tem de caber no resto dos 740 B de fabrica. */

/* O texto do registro de a (0 = nenhum) de y para baixo, em ate n linhas de 24 quebrando num espaco, em
 * ambar se e mensagem para o meu indicativo. Se nao couber, a etiqueta MORE vai embaixo da ultima linha
 * (v111; ate a v110, ao lado, com a linha cortada para ela caber), e o OK abre. */
SEPARADA static void aprs_linhas(u32 a, unsigned y, unsigned n)
{
    char s[26];
    unsigned k, j, cor;

    if (!a)
        return;
    LE_FLASH(a, s, 2);
    if ((u8)s[0] != 0x5A)                  /* sem o "usado": gravado pela metade (v75), fica em branco */
        return;
    cor = s[1] ? COR_TEXTO_AMBAR : COR_TEXTO;
    for (a += 2; n--; y += 12) {
        LE_FLASH(a, s, 25);
        for (k = 0; k < 25 && s[k]; k++)
            ;
        if (!k)
            return;
        if (k == 25) {                     /* passa da linha: quebra no ultimo espaco depois do 12o */
            for (k = 24; k > 12 && s[k] != ' '; k--)
                ;
            if (s[k] != ' ')
                k = 24;
        }
        j = k + (s[k] == ' ');             /* onde a linha seguinte comeca */
        if (!n && s[k])                    /* a ultima, e o texto continua: a etiqueta MORE (v103); o j */
            j = 0;                         /* ja nao serve (o laco acaba), e marca sem variavel a mais */
        s[k] = 0;
        if (NA_JANELA(y, y + 12))
            texto(4, y, s, 1, cor);
        if (!j && NA_JANELA(y + 9, y + 20))   /* embaixo da linha, no vao antes do traco (v111) */
            etiqueta(3, y + 9, "MORE", COR_CHIP_VERMELHO, COR_SEG_VERMELHO, 0);
        a += j;
    }
}

SEPARADA static void aprs_cena(u32 a0, u32 a1, unsigned aberto, const u16 *num)
{
    static const char *const ROTULOS[3] = {"S", "B", "F"};
    static const char *const TECLAS_RX[3] = {"OK", "*", "HOLD EXIT"};
    static const u8 CASA[3] = {0, 52, 78};          /* as tres casas dos contadores no rodape (v103; v111 a
                                                     * tecla 0 depois delas) */
    char s[14];                            /* o nome do canal (12) ou "65535/65535", e o 0 */
    unsigned i = LADO_B != 0, x;

    if (NA_JANELA(0, 18)) {
        pinta(0, 0, 160, 17, COR_ESTADO);
        /* onde se ouve (v71): o lado em evidencia (a entrada do menu desarma a dupla escuta), como o MDF
         * dele pede - no VFO, a frequencia -, no lugar do "APRS RX" */
        if ((MODO_TRABALHO >> (i * 4) & 15) != 1 || EXIBICAO(i) == 1)
            frequencia(s, le32(VFO(i)), 5);
        else if (EXIBICAO(i) == 0)
            nome_ou_sem(s, i);
        else
            texto_canal(s, CANAL(i) + 1);
        i = etiqueta(4, 3, s, COR_CHIP_AMBAR, COR_TEXTO_AMBAR, 0);
        if (num[0])                        /* a tecla 0 desligou o FILTER (v111): o canal e os menus */
            etiqueta(i, 3, "FILTER", COR_ESTADO, COR_CINZA, 0);   /* apagado: sem o fundo do selo */
        else                               /* ligado (v103, filtro_rx) */
            etiqueta(i, 3, "FILTER", COR_CHIP_RX, COR_TEXTO_RX, 0);
        if (num[2]) {
            numero(junta(numero(s, num[1], 1), "/"), num[2], 1);
            texto(156 - largura(s, 1), 5, s, 1, COR_CINZA);
        }
        pinta(0, 17, 160, 18, COR_LINHA);
    }
    if (a0)                                /* o de cima e o da posicao no alto: num fundo verde, que se ve
                                            * bem no LCD (v74); aberto, a area toda */
        pinta(3, 20, 157, aberto ? 102 : 56, COR_CHIP_RX);
    if (aberto) {
        aprs_linhas(a0, 22, 6);            /* 2 px acima: o MORE da 6a linha cabe antes do rodape (v111) */
    } else {
        aprs_linhas(a0, 24, 2);
        pinta(4, 59, 156, 60, COR_LINHA);
        aprs_linhas(a1, 68, 2);
    }
    if (NA_JANELA(102, 128)) {
        /* o rodape num fundo proprio (v75; o da barra de cima, na v74, saiu quase preto), em casas (v103):
         * em cima os contadores - os syncs na marca e no espaco (v71), o BAD em vermelho e o FIX em verde
         * quando passam de 0 -, embaixo as teclas em etiquetas, com o que cada uma faz */
        pinta(0, 102, 160, 103, COR_LINHA);
        pinta(0, 103, 160, 128, COR_SEG_FUNDO);
        pinta(0, 115, 160, 116, COR_LINHA);
        for (i = 0; i < 3; i++) {
            if (i)
                pinta(CASA[i], 103, CASA[i] + 1, 115, COR_LINHA);
            x = texto(CASA[i] + 4, 106, ROTULOS[i], 1, COR_CINZA) + 4;
            if (i)
                numero(s, num[4 + i], 1);
            else
                numero(junta(numero(s, num[3], 1), "/"), num[4], 1);
            texto(x, 106, s, 1, i == 1 && num[5] ? COR_SEG_VERMELHO : i == 2 && num[6] ? COR_VERDE : COR_TEXTO);
        }
        pinta(102, 103, 103, 115, COR_LINHA);
        texto(etiqueta(105, 104, "0", COR_ESTADO, COR_TEXTO, 0), 106, "FILTER", 1, COR_CINZA);
        for (i = 0, x = 3; i < 3; i++) {
            unsigned c = etiqueta(x, 117, TECLAS_RX[i], COR_ESTADO, COR_TEXTO, 0);

            if (i == 2)                    /* o EXIT segurado: o HOLD apagado no mesmo quadrado (a fonte nao tem
                                            * minusculas) */
                texto(x + 3, 119, "HOLD", 1, COR_CINZA);
            x = texto(c, 119, i == 2 ? "CLR" : i ? "OFF" : aberto ? "BACK" : "OPEN", 1, COR_CINZA) + 4;
        }
    }
}

/* As faixas de 32 linhas pedidas (bit 0 a de cima): o laco do RX pede uma por vez entre as leituras do
 * modem (v74). */
void tela_aprs(u32 a0, u32 a1, unsigned aberto, const u16 *num, unsigned faixas)
{
    unsigned y;

    for (y = 0; y < 128; y += 32)
        if (faixas >> (y >> 5) & 1) {
            janela_b(0, 160, y, y + 32, COR_FUNDO);
            aprs_cena(a0, a1, aberto, num);
            ENVIA();
        }
}

#ifdef PREVIA
/* O espectro inteiro, so na previa: no radio cada pedaco vem pela sua rotina. */
struct espectro_previa {
    u32 freq, pico, inicio, fim;
    int dbm;
    u8 pontos, passo, modo, digitando, atual, limite;
    u8 alturas[160];
};

void espectro_b(const struct espectro_previa *p)
{
    struct estado e;
    unsigned y, x;

    le_estado(&e);
    for (y = 0; y < 128; y += 32) {
        janela_b(0, 160, y, y + 32, COR_FUNDO);
        barra_estado(&e);
        esp_pontos(p->pontos);
        esp_freq(p->freq, p->digitando);
        esp_modo(p->modo);
        esp_passo(p->passo);
        esp_pico(p->pico);
        esp_dbm(p->dbm);
        pinta(0, ESP_GRAFICO, 160, ESP_GRAFICO + 64, COR_PAINEL);
        esp_grade(0, 160);
        for (x = 0; x < 160; x++)
            esp_coluna(x, p->alturas[x], x == p->atual);
        esp_limite(0, 160, ESP_GRAFICO + 64 - p->limite);
        esp_eixo(p->inicio, p->fim);
        ENVIA();
    }
}

/* A previa desenha a tela inteira de cada modo, lendo a RAM imitada. O menu sai nas mesmas faixas de
 * 32 linhas do firmware, que comecam onde o gancho comeca (tela.c): a janela de pasta de baixo da barra
 * ao fim (16, 48, 80, 112), e fora dela o resumo, o titulo e cada linha a parte. Com as faixas a partir
 * de 0, a previa cortava o cartao do cursor num lugar em que o radio nao corta e nao cortava onde ele
 * corta (v58). */
void telas_b(unsigned modo, unsigned variante)
{
    struct telas t;
    unsigned y;

    le_telas(&t, modo, variante, modo == 2 ? RADIO_ITEM : 0);
    if (modo != 1) {
        desenha_telas(&t, 0, 128);
        return;
    }
    desenha_telas(&t, 0, ALTURA_ESTADO);
    if (menu_pasta()) {
        desenha_telas(&t, ALTURA_ESTADO, 128);
        return;
    }
    desenha_telas(&t, ALTURA_ESTADO, TOPO_MENU);
    for (y = TOPO_MENU; y < 128; y += 18)
        desenha_telas(&t, y, y + 18 < 128 ? y + 18 : 128);
}

void tensao_b(unsigned decimos)
{
    tensao_faixas(decimos, 0, 160, 0, 128);
}

/* O cartao com o 0 segurado: por cima da tela que a previa desenhou antes. */
void tensao_tecla_b(unsigned decimos)
{
    tensao_faixas(decimos, TENSAO_X0, TENSAO_X1, TENSAO_Y0, TENSAO_Y1);
}
#endif
