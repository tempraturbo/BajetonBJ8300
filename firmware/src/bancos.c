/* Bancos de canais para o Baojie BJ-8300 (JieLi AC695X, pi32v2).
 *
 * 16 bancos com nome de 12 caracteres, cada canal podendo estar em quantos bancos quiser. Um ou
 * mais bancos ficam ligados ao mesmo tempo; o sobe/desce e a busca passam a ver so os canais
 * desses bancos. Sem banco ligado, o radio funciona exatamente como de fabrica.
 *
 * Tres itens do menu, juntos logo depois do ZONE (que continua existindo, como todo o resto):
 *   BANK       escolhe o banco (as setas passam pelos 16) e liga/desliga ele no OK
 *   BANK NAME  da nome ao banco escolhido, no editor de texto do firmware
 *   BANK CH    percorre os 960 canais e marca/desmarca no banco escolhido
 *
 * Como o filtro funciona, sem alterar nenhuma funcao do firmware: o radio decide quais canais
 * existem lendo dois mapas de bits na RAM, montados na partida varrendo a memoria de canais.
 * Reescrevemos esses mapas com a intersecao entre o que existe e os bancos ligados; a navegacao,
 * a busca e a revalidacao do canal atual ja consultam esses mapas (FUN_01E39FCE e o unico leitor
 * deles), entao passam a respeitar os bancos sozinhas.
 *
 * Para desfazer um filtro, os mapas sao remontados do mesmo jeito que a partida faz - varrendo a
 * memoria de canais com o mesmo critério - em vez de guardar uma copia. Assim nao existe estado
 * nosso em RAM (nao achei RAM comprovadamente livre neste firmware) e canal gravado ou apagado
 * pelo MEMCH/DELCH entra na conta na hora, sem copia velha para desencontrar.
 *
 * REGRA DURA, PAGA CARO (2026-10-23): **nada de usar o "buffer de setor" do firmware em 0x10190**.
 * Aquilo e a memoria de video: 0x01E486C8 enche 10240 bytes dali com uma cor (limpa a tela) e
 * 0x01E4AAF0 escreve pixels de 16 bits em 0x10190 + (y*largura + x)*2. O firmware so o reaproveita
 * como buffer ao regravar um setor de canal, aceitando sujar a tela. Guardar a tabela dos bancos
 * ali fazia a interface pintar por cima dela: o checksum passava a falhar, o filtro via "nenhum
 * banco ligado" e ainda aparecia uma faixa na tela. Por isso tudo aqui le em pedacos pequenos,
 * sempre para a pilha, e a memoria dos bancos e **um setor de 4 KB por banco**, para nunca
 * precisar da tabela inteira em RAM.
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

/* ---------- funcoes do firmware, por endereco fixo (conferidas no desmontado) ----------
 * SPRINTF      0x01E19DCC  destino de ZONE.exibe (0x01E3E140); no maximo dois valores, como os
 *                          itens do proprio firmware fazem
 * LE_FLASH     0x01E390AC  comando 0x03 + endereco de 24 bits na memoria de canais
 * APAGA_SETOR  0x01E3A126  comando 0x20: apaga 4 KB; GRAVA_FLASH 0x01E3A25A grava (fatia em
 *                          paginas de 256 B sozinha). As duas falam com o mesmo chip de dados do
 *                          LE_FLASH, nunca com a flash do firmware.
 * VALIDA_FREQ  0x01E38C3A  1 se a frequencia cai numa faixa permitida (o que a partida usa)
 * REVALIDA     0x01E3A700  se o canal atual nao vale mais, pula para o proximo que vale
 *
 * Cada uma vira um SIMBOLO ABSOLUTO para o linker, e nao um ponteiro feito de constante: assim a
 * chamada sai como "call" relativo de 4 bytes (ou "goto", na cauda), em vez de carregar o endereco
 * num registrador (6 bytes) e chamar por ele (2). Mesmo destino, ~4 bytes a menos por chamada - foi
 * o que abriu espaco para o gancho da limpeza. O linker resolve cada uma no endereco exato, e a
 * montagem confere isso no desmontado do codigo gerado (verificacao 4g do conferir.py).
 */
#define FIRMWARE(nome, endereco)  __asm__(".set " #nome ", " #endereco)
FIRMWARE(SPRINTF, 0x01E19DCC);      int SPRINTF(char *, const char *, ...);
FIRMWARE(LE_FLASH, 0x01E390AC);     void LE_FLASH(u32, void *, u32);
FIRMWARE(APAGA_SETOR, 0x01E3A126);  void APAGA_SETOR(u32);
FIRMWARE(GRAVA_FLASH, 0x01E3A25A);  void GRAVA_FLASH(u32, const void *, u32);
FIRMWARE(VALIDA_FREQ, 0x01E38C3A);  int VALIDA_FREQ(u32);
FIRMWARE(REVALIDA, 0x01E3A700);     void REVALIDA(void);
/* As tres que os ganchos substituem e repassam (o que a chamada original fazia). */
FIRMWARE(FIM_DA_CARGA, 0x01E3951C); void FIM_DA_CARGA(void);     /* ultima etapa da partida */
FIRMWARE(AVANCA_SCAN, 0x01E458E4);  void AVANCA_SCAN(void);      /* proximo canal da varredura */
FIRMWARE(DADOS_CABO, 0x01E43970);   void DADOS_CABO(unsigned);   /* 'R' (0) e 'W' (1) do cabo */
/* Tela: envio ao LCD, layout do lado, limpeza e enchimento (ver o gancho do realce) e a saida do
 * editor de texto que o CH-NAME faz. */
FIRMWARE(ENVIA, 0x01E3ADBC);        void ENVIA(void);
FIRMWARE(LAYOUT, 0x01E3B1A2);       unsigned LAYOUT(unsigned);
FIRMWARE(LIMPA, 0x01E3ACBA);        void LIMPA(unsigned);
FIRMWARE(ENCHE, 0x01E3AC68);        void ENCHE(unsigned, unsigned, unsigned);
FIRMWARE(SAI_DO_EDITOR, 0x01E3C7AA); void SAI_DO_EDITOR(void);

/* ---------- memoria do firmware que usamos ----------
 * VALOR (h[0x14BFC]) e o valor do item em edicao: o motor de menus (0x01E489E0) guarda o valor
 * escolhido, troca por linha enquanto desenha - e por isso que exibe() le dele - e devolve o
 * escolhido antes de chamar set().
 */
#define TEXTO          ((char *)0x000129B0)                  /* buffer do texto que vai para a tela */
#define VALOR          (*(volatile u16 *)0x00014BFC)
#define MAPA_EXISTE    ((volatile u8 *)0x00015D8B)           /* 120 B: canal esta gravado */
#define MAPA_BUSCA     ((volatile u8 *)0x00015D12)           /* 120 B: canal entra na busca */
#define ZONAS_EXISTE   (*(volatile u16 *)0x00015D10)         /* mascaras por zona, montadas junto */
#define ZONAS_EXISTE2  (*(volatile u16 *)0x00015E06)
#define ZONAS_BUSCA    (*(volatile u16 *)0x00015E08)
#define CANAL_ATUAL    (*(volatile u16 *)0x00015E3C)
/* Editor de texto do firmware, lido no item CH-NAME (get 0x01E3C732, set 0x01E3C7BE). */
#define EDITOR_TAM     (*(volatile u8 *)0x00014A35)          /* quantos caracteres tem o texto */
#define EDITOR_TAM_MAX (*(volatile u8 *)0x00014A36)
#define EDITOR_MARCA   (*(volatile u8 *)0x00014A39)          /* 0xAA liga o editor */
#define EDITOR_TEXTO   ((volatile char *)0x00014A3A)
#define EDITOR_POS     (*(volatile u8 *)0x00012A74)
/* Flags de "redesenhar" que a rotina 0x01E41022 do firmware liga quando a tela precisa ser
 * repintada. A terceira que ela mexe (b[0x1533C] = 2) ficou de fora de proposito: aquela troca o
 * modo de tela, e aqui so queremos refrescar o que esta no ar. */
#define REDESENHA1     (*(volatile u8 *)0x00015367)
#define REDESENHA2     (*(volatile u8 *)0x0001536F)
/* Pacote do cabo, que o firmware tambem usa para responder: [0]=letra ('R'/'W'), [1]=endereco
 * alto, [2]=endereco baixo, [3]=tamanho, dados a partir de [4]. O endereco sai do desmontado com
 * base **0x14680** (r13 em 0x01E4B422, o mesmo r6 de 0x01E43970, que le o alto em b[r6+8178], o
 * baixo em b[r6+8179] e os dados em r6+8181): o pacote comeca em 0x16671, nao em 0x16691. */
#define CABO           ((volatile u8 *)0x00016671)
/* Lado selecionado na tela principal: 0 = VFO A, diferente de 0 = VFO B. E a variavel que o
 * proprio firmware consulta para montar a tela - em 0x01E41432 ela decide se a faixa da barra do
 * A sai em 74 (expandido, selecionado) ou 41 (compacto), e em 0x01E4B308 se o numero do canal sai
 * em 62 ou 101. Nao confundir com tx_ab (0x14B84, o ajuste 0x9018 gravado na memoria de dados):
 * aquele e o ajuste, este e o estado da tela, e foi usar o errado que fez a v13 nao realcar nada. */
#define LADO_B         (*(volatile u8 *)0x00015E04)

#define NUM_BANCOS   16
#define NUM_CANAIS   960
#define TAM_MAPA     (NUM_CANAIS / 8)      /* 120 */
#define TAM_NOME     12
#define NUM_ZONAS    15                    /* 15 x 64 = 960; as mascaras por zona cobrem isso */
#define TAM_PEDACO   24                    /* le o mapa de um banco em pedacos; 120 = 5 x 24 */
/* Verde do fundo do bloco do VFO em evidencia: #005900, degrau 22 dos 64 do verde em RGB565. Ja
 * passaram por aqui o 0x00E0 (degrau 7, "nao da nem pra perceber que e verde"), o 0x0280 (20,
 * pedido "um pouco mais claro") e o 0x0380 (28, claro demais no bloco inteiro); este fica entre os
 * dois ultimos. Os ganchos passam a cor como valor de 16 bits, entao aqui nao vale o limite de 8
 * bits do remendo direto. Para mudar o tom, mexer so nesta linha. */
#define COR_REALCE   0x02C0u

/* ---------- onde os bancos ficam gravados ----------
 * Setores proprios depois da area que o firmware usa em 0xC0000 (64 registros de 60 B, que ele le
 * na partida e apaga): o cabecalho em 0xC1000 e **um setor de 4 KB por banco** a partir de 0xC2000.
 * Ficar acima de 0xC0000 garante que o chip alcanca o endereco (o proprio firmware escreve la) e
 * sai de longe de 0x10000, onde o CPS grava a imagem de abertura. Gastar
 * 68 KB de um chip que o firmware usa ate 0xC0000 compra uma coisa que vale muito: para alterar
 * um banco basta ter 138 bytes em RAM, entao nada aqui precisa de buffer grande - e foi buffer
 * grande emprestado (a memoria de video) que quebrou a primeira versao.
 * Nenhum setor nosso divide espaco com canal ou com os dados de fabrica de 0xF000, que so o cabo
 * regrava: apagar o que e nosso nunca poe nada deles em risco.
 */
#define SETOR_CAB      0x000C1000u
#define SETOR_BANCO(n) (0x000C2000u + ((u32)(n) << 12))

/* A marca fica em dois u16 em vez de quatro u8: conferir e gravar a marca passa a ser dois acessos
 * de 16 bits (as duas estruturas tem campo u16, entao o alinhamento de 2 e garantido) em vez de um
 * laco byte a byte, e as cadeias "BNK3"/"BCO1" saem da imagem. O formato gravado nao muda. */
#define MARCA_CAB    0x334B4E42u               /* "BNK3" */
#define MARCA_BANCO  0x314F4342u               /* "BCO1" */
/* A marca sai do proprio endereco - o cabecalho tem a sua, os 16 setores de banco tem a outra -
 * em vez de viajar como parametro em cada chamada: um argumento a menos para errar. */
#define MARCA_DE(endereco)  ((endereco) == SETOR_CAB ? MARCA_CAB : MARCA_BANCO)

struct cabeca {
    u16 marca[2];                          /* "BNK3" */
    u16 ativos;                            /* um bit por banco ligado */
    u8 escolhido;                          /* banco em que os outros dois itens mexem */
    u8 reservado;
    u16 soma;
};

struct banco {
    u16 marca[2];                          /* "BCO1" */
    char nome[TAM_NOME];
    u8 canais[TAM_MAPA];
    u16 soma;
};

#define POS_NOME(n)   (SETOR_BANCO(n) + 4)
#define POS_MAPA(n)   (SETOR_BANCO(n) + 4 + TAM_NOME)

/* Travas de montagem: os acessos por campo (que leem so um pedaco da flash) tem que concordar com
 * o tamanho das estruturas. Se o compilador alinhar diferente do que esta escrito aqui, nao
 * compila. */
typedef char confere_cabeca[sizeof(struct cabeca) == 10 ? 1 : -1];
typedef char confere_banco[sizeof(struct banco) == 4 + TAM_NOME + TAM_MAPA + 2 ? 1 : -1];

/* ---------- utilidades (sem libc: o blob nao linka nada) ----------
 * Os ponteiros sao volatile de proposito: impede o clang de trocar o laco por uma chamada a
 * memset/memcpy, que nao existem aqui.
 */
static void zera(void *destino, unsigned n)
{
    volatile u8 *d = (volatile u8 *)destino;

    while (n--)
        *d++ = 0;
}

/* Acrescenta no fim do texto. Serve para nao passar do quarto argumento no sprintf do firmware:
 * os itens do proprio firmware nunca passam de dois valores, e esse e o caminho testado. */
static void acrescenta(char *destino, const char *texto)
{
    volatile char *d = (volatile char *)destino;

    while (*d)
        d++;
    while (*texto)
        *d++ = *texto++;
    *d = 0;
}

static u16 soma_de(const u8 *p, unsigned n)
{
    unsigned soma = 0x1234;

    while (n--)
        soma = (soma << 1) + *p++;
    return (u16)soma;
}

static int marca_ok(const u16 *m, u32 marca)
{
    return m[0] == (u16)marca && m[1] == (u16)(marca >> 16);
}

/* TRAVA, PAGA CARO (2026-10-24): so grava em setor **apagado** (ou que ja seja nosso).
 * A primeira versao punha os bancos em 0x10000 - e e exatamente ali que o CPS do fabricante grava
 * a imagem de abertura. Apagar o setor do cabecalho e o do banco 1 comeu os primeiros 8 KB da
 * imagem (4096 pixels = as ~25 linhas de cima da tela): era a "faixa branca" no logo, porque flash
 * apagada le 0xFF e 0xFFFF em 16 bits e branco. Conferir que o destino esta apagado protege
 * qualquer coisa que a gente nao saiba que mora la - inclusive se o chip for pequeno e o endereco
 * espelhar em cima dos canais.
 */
static int setor_livre(u32 endereco)
{
    u16 p[2];

    LE_FLASH(endereco, p, sizeof(p));
    if (marca_ok(p, MARCA_DE(endereco)))
        return 1;                          /* ja e nosso: pode regravar */
    return p[0] == 0xFFFF && p[1] == 0xFFFF;   /* apagado: pode usar */
}

/* ---------- cabecalho e bancos: cada um no seu setor ----------
 * Um unico par de funcoes serve para os dois: le para a pilha, confere marca e soma, e se nao
 * bater devolve zerado com a marca posta (nunca lixo). Gravar e apagar o setor e escrever so os
 * bytes da estrutura - a rotina do firmware fatia em paginas sozinha.
 */
static int le_registro(u32 endereco, void *buf, unsigned tam)
{
    u8 *p = (u8 *)buf;
    u16 *m = (u16 *)buf;
    u32 marca = MARCA_DE(endereco);

    LE_FLASH(endereco, p, tam);
    if (marca_ok(m, marca) && *(u16 *)(p + tam - 2) == soma_de(p, tam - 2))
        return 1;
    zera(p, tam);
    m[0] = (u16)marca;
    m[1] = (u16)(marca >> 16);
    return 0;
}

static void grava_registro(u32 endereco, void *buf, unsigned tam)
{
    u8 *p = (u8 *)buf;

    if (!setor_livre(endereco))
        return;                            /* tem coisa de outro dono ali: nao apaga */
    *(u16 *)(p + tam - 2) = soma_de(p, tam - 2);
    APAGA_SETOR(endereco);
    GRAVA_FLASH(endereco, p, tam);
}

#define le_cabeca(c)      le_registro(SETOR_CAB, (c), sizeof(struct cabeca))
#define grava_cabeca(c)   grava_registro(SETOR_CAB, (c), sizeof(struct cabeca))
#define le_banco(n, b)    le_registro(SETOR_BANCO(n), (b), sizeof(struct banco))
#define grava_banco(n, b) grava_registro(SETOR_BANCO(n), (b), sizeof(struct banco))

/* ---------- canais: o mesmo critério da partida ----------
 * A partida (dentro de FUN_01E56084) le 4 bytes do inicio do registro de 32 B: descarta se o
 * primeiro e 0xFF ou se o quarto e 0, decodifica os 4 bytes em BCD do mais significativo para o
 * menos e so aceita o canal se VALIDA_FREQ devolver 1. O bit 0x04 do byte 15 e o "entra na busca".
 */
static int existe_no_registro(const u8 *p)
{
    u32 f = 0;
    unsigned i = 4;
    u8 d;

    if (p[0] == 0xFF || p[3] == 0)
        return 0;
    while (i--) {
        d = p[i];
        f = f * 100 + (d >> 4) * 10 + (d & 0x0F);
    }
    return VALIDA_FREQ(f) == 1;
}

static int canal_existe(unsigned canal)
{
    u8 p[4];

    LE_FLASH(canal << 5, p, sizeof(p));
    return existe_no_registro(p);
}

/* Remonta os dois mapas varrendo a memoria de canais, um registro por vez - as mesmas duas
 * leituras por canal que a partida faz. E o que desfaz qualquer filtro: depois disto os mapas
 * estao como ficariam numa partida. */
static void reconstroi_mapas(void)
{
    unsigned canal;
    u8 p[16];

    zera((void *)MAPA_EXISTE, TAM_MAPA);
    zera((void *)MAPA_BUSCA, TAM_MAPA);
    for (canal = 0; canal < NUM_CANAIS; canal++) {
        /* 16 bytes de uma vez: o inicio do registro (frequencia) e o byte 15 (bit da busca) */
        LE_FLASH(canal << 5, p, sizeof(p));
        if (!existe_no_registro(p))
            continue;
        MAPA_EXISTE[canal >> 3] |= (u8)(1u << (canal & 7));
        if (p[15] & 0x04)
            MAPA_BUSCA[canal >> 3] |= (u8)(1u << (canal & 7));
    }
}

/* Deixa nos mapas a intersecao entre o que existe e os bancos ligados. Com reconstruir != 0,
 * remonta os mapas antes (obrigatorio fora da partida, onde eles podem estar filtrados).
 * Devolve 1 se filtrou. Banco ligado que nao tem nenhum canal existente nao filtra nada: o radio
 * fica mostrando tudo, em vez de ficar sem canal nenhum para mostrar.
 * A uniao dos bancos fica na pilha (120 B) e cada mapa entra em pedacos de 24 B - nada de buffer
 * grande emprestado do firmware. */
/* Deixa nos mapas a intersecao entre o que existe e os bancos ligados. Com reconstruir != 0,
 * remonta os mapas antes (obrigatorio fora da partida, onde eles podem estar filtrados).
 * Devolve 1 se filtrou - e o liga/desliga do BANK usa essa resposta para nao deixar ligado um
 * banco que nao filtra nada, caso em que o radio fica mostrando tudo em vez de ficar sem canal.
 * A uniao fica na pilha e cada mapa entra em pedacos: nada de buffer grande emprestado (o do
 * firmware e a memoria de video). */
static int aplica_filtro(int reconstruir)
{
    u8 uniao[TAM_MAPA], pedaco[TAM_PEDACO];
    struct cabeca c;
    unsigned i, b, off, existe = 0, busca = 0, tem = 0;

    if (reconstruir)
        reconstroi_mapas();
    if (le_cabeca(&c) && c.ativos) {
        zera(uniao, sizeof(uniao));
        for (b = 0; b < NUM_BANCOS; b++) {
            if (!(c.ativos & (1u << b)))
                continue;
            for (off = 0; off < TAM_MAPA; off += TAM_PEDACO) {
                LE_FLASH(POS_MAPA(b) + off, pedaco, sizeof(pedaco));
                for (i = 0; i < sizeof(pedaco); i++)
                    uniao[off + i] |= pedaco[i];
            }
        }
        /* uniao vira a intersecao, e a mesma passada ja diz se sobrou algum canal. So depois
         * de saber que sobrou e que os mapas sao alterados - banco ligado sem nenhum canal
         * existente nao pode zerar a navegacao. O mapa da busca e subconjunto do de existencia,
         * entao cruzar com a intersecao da no mesmo que cruzar com a uniao. */
        for (i = 0; i < TAM_MAPA; i++) {
            uniao[i] &= MAPA_EXISTE[i];
            tem |= uniao[i];
        }
        if (tem)
            for (i = 0; i < TAM_MAPA; i++) {
                MAPA_BUSCA[i] &= uniao[i];
                MAPA_EXISTE[i] = uniao[i];
            }
    }
    /* mascaras por zona, do jeito que a partida as monta: oito bytes de mapa por zona. Em Full
     * Channel o firmware nem as consulta, mas no modo por zona elas evitam que uma zona que o
     * filtro esvaziou continue parecendo cheia. */
    for (i = 0; i < TAM_MAPA; i++) {
        if (MAPA_EXISTE[i])
            existe |= 1u << (i >> 3);
        if (MAPA_BUSCA[i])
            busca |= 1u << (i >> 3);
    }
    ZONAS_EXISTE = (u16)existe;
    ZONAS_EXISTE2 = (u16)existe;
    ZONAS_BUSCA = (u16)busca;
    /* Poe o radio num canal que ainda vale: sem isto, trocar para um canal fora do banco deixaria
     * o radio parado nele. (REVALIDA pede redesenho pelas flags de 0x15367 - cheguei a suspeitar
     * que fosse a faixa na tela de abertura, mas a faixa continuou sem ele.) */
    REVALIDA();
    return tem != 0;
}

/* ---------- gancho da partida ----------
 * Entra no lugar da ultima chamada do carregamento de configuracao e canais: faz o que ela fazia
 * e aplica o filtro. Os mapas acabaram de ser montados pela partida, entao nao precisa remontar.
 * Roda depois da leitura do combo PTT2+atalho, entao nao atrapalha a regravacao pelo cabo.
 */
void gancho_partida(void)
{
    FIM_DA_CARGA();
    aplica_filtro(0);
    /* Na partida o logo ja esta na tela. Se o filtro trocou o canal, o REVALIDA pediu redesenho
     * (0x01E39F20 liga b[0x15367]) e a interface repinta uma banda por cima do logo - era essa a
     * faixa branca. O canal ja foi arrumado; aqui so desfazemos o pedido, porque a tela inicial e
     * desenhada inteira quando o logo sai. Fora da partida o pedido e bem-vindo e fica. */
    REDESENHA1 = 0;
}

/* ---------- gancho do scan ----------
 * A chamada em 0x01E46EB6 avanca o canal durante a varredura (logo depois de o firmware ler o modo
 * SC-REV), mas nada marca a tela como suja: a barra de qualidade do sinal fica congelada no nivel
 * e na cor do ultimo canal que o scan achou, e o numero do canal chega a descasar do nome. Aqui
 * fazemos o que a chamada fazia e pedimos o redesenho. FUN_01E458E4 nao recebe argumento e o que
 * devolve e descartado pelo chamador, entao um gancho sem parametros basta.
 */
void gancho_scan(void)
{
    AVANCA_SCAN();
    REDESENHA1 = 1;
    REDESENHA2 = 1;
}

/* ---------- gancho do cabo: endereco de 24 bits, para ler e para gravar ----------
 * Entra no lugar das duas chamadas que o tratador do cabo faz para a rotina de dados 0x01E43970
 * (que le com argumento 0 e grava com 1): o 'R' chama em 0x01E4BB66 e o 'W' em 0x01E4C0FA. Por
 * isso o gancho recebe e repassa o argumento - um gancho so serve aos dois comandos.
 *
 * O COMANDO NOVO CABE EM 4 BYTES, e nao em 5 (paga caro, 2026-10-24): o tratador dispara assim que
 * **quatro** bytes chegaram (0x01E4BB52 desvia se h[0x1666C] < 4) e zera o contador depois de
 * responder, entao um quinto byte chega tarde - e ainda fica pendente, envenenando o pacote
 * seguinte (foi o que travou o radio no primeiro teste). Em vez disso, o **bit 7 do tamanho** liga
 * o modo estendido: os outros 7 bits sao o terceiro byte do endereco e o bloco e sempre de 64 B.
 * Tamanho com o bit 7 em zero = comportamento de fabrica, byte a byte: o CHIRP nao ve diferenca.
 * O 'W' de fabrica tambem e de 64 B (0x01E4C0E8 espera 68 bytes no buffer), entao o pacote
 * estendido de gravacao tem exatamente o tamanho do de fabrica.
 *
 * Gravacao estendida **so na area dos bancos** (0xC1000..0xD1FFF), apagando o setor quando o bloco
 * comeca nele; qualquer outro endereco e ignorado sem escrever nada. Quem grava banco pelo cabo tem
 * que desligar e ligar o radio depois: o filtro e montado na partida.
 */
void gancho_cabo(unsigned grava)
{
    volatile u8 *p = CABO;
    unsigned tam = p[3], setor;
    u32 endereco;

    if (tam & 0x80) {
        endereco = ((tam & 0x7F) << 16) | ((u32)p[1] << 8) | p[2];
        setor = endereco >> 12;            /* contas por setor: constantes pequenas, codigo menor */
        p[3] = 0x40;                       /* a resposta do 'R' sai com 4 + este tamanho */
        if (!grava)
            LE_FLASH(endereco, (void *)(p + 4), 0x40);
        /* A faixa 0xC1000..0xD1FFF e exclusivamente nossa - o mapa do chip lido pelo cabo em
         * 2026-10-24 mostra que nada mais mora ali, e o chip de 1 MB nao espelha nesse endereco.
         * Quem grava por aqui e o PC mandando registro de banco; o caminho do menu continua com a
         * trava do setor_livre. */
        else if (setor - (SETOR_CAB >> 12) < NUM_BANCOS + 1u) {
            if (endereco == (u32)setor << 12)   /* bloco que abre o setor: apaga antes */
                APAGA_SETOR(endereco);
            GRAVA_FLASH(endereco, (void *)(p + 4), 0x40);
        }
        return;
    }
    DADOS_CABO(grava);
}

/* ---------- gancho do realce: fundo do bloco do VFO em evidencia ----------
 * A tela e desenhada uma janela por vez: janela(x0,x1,y0,y1,limpar,cor) em 0x01E3AD34 guarda os
 * limites em b[0x14608..0x1460E] e h[0x14610] (bytes por linha) e, com limpar != 0, enche o buffer
 * de video (0x10190) com a cor; o desenho entra ali; envia() em 0x01E3ADBC le os mesmos globais e
 * manda o pedaco para o LCD.
 *
 * O gancho entra NO ENVIO, nao no preenchimento, e troca por verde todo pixel preto do pedaco.
 * E a unica forma de pintar o bloco inteiro, porque o fundo do texto NAO vem de janela(): as
 * rotinas de texto enchem a celula de cada caractere com uma cor propria, preta fixa (em
 * 0x01E3BACE e o "r8 = 0" de 0x01E3BAD8, que vai como fundo para o desenhador de glifo em
 * 0x01E388F4 e para o enchimento do vao em 0x01E38978). Trocar so a cor da janela, como a v14
 * fazia, deixava o nome e a frequencia em caixas pretas sobre o verde.
 *
 * O buffer guarda cada pixel com o byte ALTO primeiro (0x01E3AD90: b[p] = cor>>8, b[p+1] = cor),
 * e a CPU e little-endian - por isso COR_BUF entra com os bytes trocados. Preto e 0 dos dois
 * lados, entao a comparacao nao depende disso.
 *
 * Qual bloco esta em evidencia sai da PROPRIA rotina do firmware (0x01E3B1A2), a mesma que ele usa
 * para decidir a posicao de cada linha e que o desenhador de bloco (0x01E3BD04) e o complemento
 * (0x01E3C06E) consultam. Chamada com o lado selecionado ela devolve 2 com o VFO A em evidencia,
 * 1 com o B e **0 quando a tela nao e a de dois VFOs** - so vale para b[0x1533D] em {0,3,5}, e a
 * tela de um VFO so poe 4 ali (0x01E4CDC4). E o que mantem o verde fora das outras telas, que
 * passam por estas mesmas funcoes.
 *
 * A AREA do lado em evidencia e a que o proprio firmware limpa ao montar a tela (0x01E3ACBA no
 * modo 2): 72 linhas a partir de y=16 com o A, ou de y=56 com o B, e a linha separadora branca
 * logo depois (88) ou logo antes (55). Todas as janelas do lado em evidencia caem dentro dela e as
 * do outro lado ficam fora - lidas no desmontado (tabelas em 0x01E3BD40/0x01E3BD4A e 0x01E3C0BE):
 *   A em evidencia, area 16..87    A: 18, 30, 56, 58, 62, 74    B: 91, 93, 114
 *   B em evidencia, area 56..127   A: 19, 21, 41                B: 57, 69, 95, 97, 101, 114
 * (18/57 e a linha de informacoes, a do "H", que so existe para o lado em evidencia.)
 *
 * REGRA DURA, PAGA CARO (v13): o buffer tem 10240 bytes (uma banda de 160x32) e janela() nao tem
 * trava de tamanho - foi pedir janela maior que isso que reiniciava o radio. Aqui o gancho nunca
 * desenha nada: so le os limites que o firmware ja gravou e, por garantia, nao passa do buffer.
 */
#define VIDEO      ((volatile u16 *)0x00010190)
#define PIXELS     (10240u / 2)                    /* o buffer inteiro, em pixels */
/* Os limites que janela() grava (0x01E3AD3C..0x01E3AD74: b[83464] x0, b[83466] y0, b[83468] x1,
 * b[83470] y1, h[83472] bytes por linha). O objdump mostra esses enderecos em DECIMAL; a v15 os
 * converteu errado para 0x145CA/0x145CE/0x145D0 (0x40 a menos), leu tres variaveis alheias e
 * nunca pintou nada. Conferir sempre contra o decimal do desmontado. */
#define JAN_Y0     (*(volatile u8 *)0x0001460A)    /* 83466 */
#define JAN_Y1     (*(volatile u8 *)0x0001460E)    /* 83470 */
#define JAN_LINHA  (*(volatile u16 *)0x00014610)   /* 83472, bytes por linha: (x1 - x0) * 2 */
#define COR_BUF    ((u16)(COR_REALCE >> 8 | COR_REALCE << 8))

#define AREA       72u                             /* linhas da area do lado em evidencia */

/* Primeira linha da area do lado em evidencia (16 com o A, 56 com o B), ou 0 fora da tela de dois
 * VFOs. Chamado com o lado selecionado, layout() devolve 2 com o A em evidencia, 1 com o B e 0 nas
 * outras telas - e ele so testa se o argumento e zero (0x01E3B1CC/0x01E3B1D4), entao LADO_B vai
 * direto. 96 - 40 * lado da 16 para o A e 56 para o B. */
static unsigned area_realce(void)
{
    unsigned lado = LAYOUT(LADO_B);

    return lado - 1 <= 1 ? 96 - 40 * lado : 0;
}

void gancho_envia(void)
{
    unsigned y0 = JAN_Y0, inicio = area_realce();

    if (inicio && y0 - inicio < AREA) {      /* sem sinal: y0 abaixo do inicio da um numero enorme */
        unsigned n = (JAN_LINHA >> 1) * (JAN_Y1 - y0);
        volatile u16 *p = VIDEO;

        if (n > PIXELS)             /* nunca passa do buffer, mesmo com janela absurda */
            n = PIXELS;
        while (n--) {
            if (*p == 0)
                *p = COR_BUF;
            p++;
        }
    }
    ENVIA();
}

/* As janelas nao cobrem a area inteira: entre o nome grande e a frequencia, e entre a frequencia e
 * a barra, sobram faixas que ninguem desenha (secao 8 de docs/mapa-da-tela.md) - e que, depois da
 * limpeza, ficavam pretas. Este gancho entra nas duas chamadas que limpam a tela e em seguida a
 * redesenham inteira (0x01E3C294, dentro de 0x01E3C290, por onde passam a partida e a saida do
 * menu; e 0x01E40F66, dentro de 0x01E40F1C, que e a troca de lado): deixa a limpeza de fabrica
 * correr (preto e a separadora) e enche de verde a area do lado em evidencia. As janelas vem por
 * cima, recoloridas pelo gancho do envio, e nas faixas que sobram fica o verde. Nenhum outro
 * caminho pinta aquelas faixas, entao elas continuam verdes ate a proxima limpeza. A entrada do
 * menu limpa por outra chamada e nao passa por aqui. */
void gancho_limpa(unsigned modo)
{
    unsigned inicio;

    LIMPA(modo);
    if ((inicio = area_realce()) != 0)
        ENCHE(inicio, AREA, COR_REALCE);
}

/* ---------- textos ---------- */
const char titulo_bank[] = "BANK";
const char titulo_nome[] = "BANK NAME";
const char titulo_canal[] = "BANK CH";

/* Le o nome do banco para o destino (TAM_NOME+1 bytes), terminado em zero; fica vazio se o banco
 * ainda nao tem nome. Quem precisa de um padrao visivel e so o editor, que poe "BANK n". */
static void le_nome(unsigned n, char *destino)
{
    zera(destino, TAM_NOME + 1);
    LE_FLASH(POS_NOME(n), destino, TAM_NOME);
    if ((u8)destino[0] <= ' ' || (u8)destino[0] == 0xFF)
        *destino = 0;                      /* cadeia vazia: o resto do buffer ja nao e lido */
}

/* ---------- item BANK: escolhe o banco e liga/desliga (flags 0x80) ---------- */

char *bank_exibe(void)
{
    struct cabeca c;
    char nome[TAM_NOME + 1];
    unsigned n = VALOR;

    le_cabeca(&c);                         /* o motor so chama exibe() com valor < maximo */
    le_nome(n, nome);
    SPRINTF(TEXTO, "%d %s", n + 1, nome);  /* banco sem nome mostra so o numero */
    if ((c.ativos >> n) & 1)
        acrescenta(TEXTO, " ON");
    return TEXTO;
}

u16 bank_le(void)
{
    struct cabeca c;

    le_cabeca(&c);
    return c.escolhido;
}

void bank_grava(void)
{
    struct cabeca c;
    unsigned n = VALOR;
    u16 bit;

    if (n >= NUM_BANCOS)
        return;
    le_cabeca(&c);
    bit = (u16)(1u << n);
    c.escolhido = (u8)n;                   /* o OK sempre escolhe o banco para os outros dois itens */
    c.ativos = (u16)(c.ativos ^ bit);      /* liga ou desliga */
    grava_cabeca(&c);
    if (!aplica_filtro(1) && (c.ativos & bit)) {
        c.ativos = (u16)(c.ativos & ~bit); /* ligou um banco que nao filtra nada: desfaz */
        grava_cabeca(&c);
        aplica_filtro(0);
    }
}

/* ---------- item BANK NAME: usa o editor de texto do firmware (flags 0x07) ---------- */

char *nome_exibe(void)
{
    struct cabeca c;

    le_cabeca(&c);
    le_nome(c.escolhido, TEXTO);
    return TEXTO;
}

/* Chamado quando o item e aberto: prepara o editor com o nome de hoje, como o CH-NAME faz. */
u16 nome_le(void)
{
    struct cabeca c;
    unsigned i;

    le_cabeca(&c);
    EDITOR_TAM_MAX = TAM_NOME;
    EDITOR_MARCA = 0xAA;
    EDITOR_POS = 0;
    le_nome(c.escolhido, (char *)EDITOR_TEXTO);   /* banco sem nome abre o editor vazio */
    for (i = 0; i < TAM_NOME && EDITOR_TEXTO[i]; i++)
        ;
    EDITOR_TAM = (u8)i;
    return 0;
}

void nome_grava(void)
{
    struct cabeca c;
    struct banco b;
    unsigned i, tam = EDITOR_TAM;

    if (tam > TAM_NOME)
        tam = TAM_NOME;
    le_cabeca(&c);
    le_banco(c.escolhido, &b);
    zera(b.nome, TAM_NOME);
    for (i = 0; i < tam; i++)
        b.nome[i] = EDITOR_TEXTO[i];
    grava_banco(c.escolhido, &b);
    /* Sair do editor e o que o CH-NAME faz no fim do set dele (0x01E3C822): sem estas duas
     * chamadas o estado do editor fica ligado e o cabecalho do menu continua mostrando o "abc" do
     * teclado em vez do canal, em todos os itens, ate sair e entrar no menu de novo. */
    LIMPA(2);
    SAI_DO_EDITOR();
}

/* ---------- item BANK CH: percorre os canais e marca (flags 0x80, max 960) ---------- */

char *canal_exibe(void)
{
    struct cabeca c;
    unsigned canal = VALOR;
    u8 mapa = 0;

    le_cabeca(&c);                         /* idem: o maximo do item ja limita o canal */
    LE_FLASH(POS_MAPA(c.escolhido) + (canal >> 3), &mapa, 1);
    SPRINTF(TEXTO, "%d-%d", c.escolhido + 1, canal + 1);   /* banco escolhido - canal */
    if (!canal_existe(canal))
        acrescenta(TEXTO, " -");                           /* canal vazio na memoria */
    if ((mapa >> (canal & 7)) & 1)
        acrescenta(TEXTO, " IN");                          /* esta neste banco */
    return TEXTO;
}

u16 canal_le(void)
{
    return CANAL_ATUAL;                    /* o motor zera o que passar do maximo */
}

void canal_grava(void)
{
    struct cabeca c;
    struct banco b;
    unsigned canal = VALOR;

    if (canal >= NUM_CANAIS)
        return;
    le_cabeca(&c);
    le_banco(c.escolhido, &b);
    b.canais[canal >> 3] ^= (u8)(1u << (canal & 7));
    grava_banco(c.escolhido, &b);
    aplica_filtro(1);                      /* entra ou sai da navegacao na hora */
}
