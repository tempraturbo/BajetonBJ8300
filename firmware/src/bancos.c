/* Bancos de canais para o Baojie BJ-8300 (JieLi AC695X, pi32v2).
 *
 * 16 bancos com nome de 12 caracteres, cada canal podendo estar em quantos bancos quiser. Um ou
 * mais bancos ficam ligados ao mesmo tempo; o sobe/desce e a busca passam a ver so os canais
 * desses bancos. Sem banco ligado, o radio funciona exatamente como de fabrica.
 *
 * Tres itens do menu, juntos no lugar do Zone Or Channel e do ZONE (que sairam do menu; todo o
 * resto continua):
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
/* O memcpy do firmware, na ROM do chip (191 chamadores no app: destino em r0, origem em r1, tamanho
 * em r2 - por exemplo 0x01E3B87C, que copia os 48 bytes de um glifo para a pilha). */
FIRMWARE(COPIA_MEM, 0x021127A8);    void COPIA_MEM(void *, const void *, unsigned);
FIRMWARE(APAGA_SETOR, 0x01E3A126);  void APAGA_SETOR(u32);
FIRMWARE(GRAVA_FLASH, 0x01E3A25A);  void GRAVA_FLASH(u32, const void *, u32);
FIRMWARE(VALIDA_FREQ, 0x01E38C3A);  int VALIDA_FREQ(u32);
FIRMWARE(REVALIDA, 0x01E3A700);     void REVALIDA(void);
/* As tres que o DELCH de fabrica (set 0x01E3C516) usa depois de apagar um canal, para os dois lados:
 * o bit do canal no mapa (busca 0 = o de existencia), o proximo canal que vale (0xFFFF se nenhum) e a
 * recarga dos dois lados, que revalida cada um e deixa o lado em evidencia como estava. */
FIRMWARE(CANAL_VALE, 0x01E39FCE);   unsigned CANAL_VALE(unsigned canal, unsigned busca);
FIRMWARE(PROXIMO_CANAL, 0x01E3C4C0); unsigned PROXIMO_CANAL(unsigned canal, unsigned busca);
FIRMWARE(RECARREGA_LADOS, 0x01E3A812); void RECARREGA_LADOS(void);
/* As tres que os ganchos substituem e repassam (o que a chamada original fazia). */
FIRMWARE(FIM_DA_CARGA, 0x01E3951C); void FIM_DA_CARGA(void);     /* ultima etapa da partida */
FIRMWARE(AVANCA_SCAN, 0x01E458E4);  void AVANCA_SCAN(void);      /* proximo canal da varredura */
FIRMWARE(DADOS_CABO, 0x01E43970);   void DADOS_CABO(unsigned);   /* 'R' (0) e 'W' (1) do cabo */
/* Tela: a limpeza e a saida do editor de texto, que o CH-NAME faz no fim do set dele e o BANK NAME
 * repete. A tela principal, as outras e o espectro (v41) moram no tela.c. */
FIRMWARE(LIMPA, 0x01E3ACBA);        void LIMPA(unsigned);
FIRMWARE(SAI_DO_EDITOR, 0x01E3C7AA); void SAI_DO_EDITOR(void);
/* A decodificacao do lado que a dupla escuta vai ouvir (o registro cru da corrente -> o que a
 * sintonia le); nao recebe argumento (TRIPLE, v42). */
FIRMWARE(DECODIFICA, 0x01E3900A);   void DECODIFICA(void);
/* O resto do que o TRIPLE usa (v44, mapa da dupla escuta no bancos.c, secao TRIPLE): */
FIRMWARE(VOLTA_DW, 0x01E3FD9E);     void VOLTA_DW(void);          /* sombra do lado em evidencia -> corrente, e decodifica */
FIRMWARE(PARA_DW, 0x01E3FDE4);      void PARA_DW(void);           /* desarma a dupla escuta, no lado em evidencia */
FIRMWARE(TROCA_AB, 0x01E40E52);     void TROCA_AB(unsigned);      /* troca o lado em evidencia (a tecla A/B passa 1) */
/* v49, o VFO da C: o registro de VFO do lado (0 A, 1 B) recebe os 32 B do VFO corrente (0x15E80) e,
 * se mudou, vai para o log 0x8000 do chip; e os ajustes (0x14B6C.., com o modo de cada lado em
 * 0x14B86) vao para o log 0x9000 - a tecla VFO/MR os grava assim (0x01E4DCB6). */
FIRMWARE(SALVA_VFO, 0x01E3A4FE);    void SALVA_VFO(unsigned lado);
FIRMWARE(SALVA_AJUSTES, 0x01E3AB36); void SALVA_AJUSTES(void);
FIRMWARE(LUZ, 0x01E394D6);          void LUZ(unsigned acende);    /* a luz da tela: 1 acende, 0 apaga */
/* A tela inteira de novo (limpa, a barra e 0x01E3C132(0)): o fim da troca de lados de fabrica
 * (0x01E40EF4) e do retorno a tela principal que tambem poe o canal no lado (0x01E4BDBE, logo depois
 * de 0x01E40B9E(0, 1)). Nao recebe argumento. */
FIRMWARE(TELA_INTEIRA, 0x01E3C290); void TELA_INTEIRA(void);
FIRMWARE(REGISTRO_TELA, 0x01E39128); void REGISTRO_TELA(void);  /* o registro de tela do lado em evidencia */
/* Espectro: o que o firmware faz para voltar a varrer quando o sinal achado cai (0x01E4AEFC) -
 * fecha a saida de audio do chip de radio (regs 0x47/0x48), solta o pedido do espectro ao
 * amplificador (5 pede, 6 solta), desliga o outro pino do audio e passa ao proximo ponto. */
FIRMWARE(AUDIO_DO_CHIP, 0x01E39E22);   void AUDIO_DO_CHIP(unsigned);
FIRMWARE(PEDE_AMPLI, 0x01E39E2C);      void PEDE_AMPLI(unsigned);
FIRMWARE(AMPLI, 0x01E39CFC);           void AMPLI(unsigned);
FIRMWARE(ESPECTRO_AVANCA, 0x01E4AE44); void ESPECTRO_AVANCA(void);
/* Espectro: as outras rotinas que os ganchos usam (secao 11 de docs/engenharia-reversa.md). */
FIRMWARE(ESPECTRO, 0x01E4AEAC);        void ESPECTRO(unsigned);          /* um passo da varredura */
FIRMWARE(ALTURA, 0x01E4ACA0);          unsigned ALTURA(unsigned);        /* leitura crua -> 0..64 */
FIRMWARE(FILTRO, 0x01E4A980);          void FILTRO(unsigned);            /* filtro do chip (reg 0x43) */
FIRMWARE(JANELA_ESPECTRO, 0x01E4AA3C); void JANELA_ESPECTRO(u32);        /* centra: inicio, fim, passo, limite */
FIRMWARE(MOSTRA_FREQ, 0x01E45D96);     void MOSTRA_FREQ(u32);            /* a frequencia grande do topo */
FIRMWARE(ROTULOS_FREQ, 0x01E4AB2A);    void ROTULOS_FREQ(u32, u32);      /* as do comeco e do fim, embaixo */
/* Roger: as duas rotinas que o laco do BEEP de fabrica usa (0x01E44644 e 0x01E4464A). */
FIRMWARE(TOM, 0x01E43B4E);             void TOM(unsigned, unsigned);     /* freq. dos 2 geradores, Hz x 10.32444 */
/* A espera conta em unidades de 2 ms (v22 tocava cada tom do BEEP 2 por 200 ms em vez de 100): o
 * temporizador interrompe a cada 48000 ciclos de 24 MHz = 2 ms (0x01E3E44C) e soma 2 ao contador
 * [0x14810] (0x01E51356), que anda 1 por ms; a espera vai ate o contador andar n x 2. */
FIRMWARE(ESPERA_2MS, 0x01E36286);      void ESPERA_2MS(unsigned);        /* espera ocupada, n x 2 ms */
/* Scramble: o set de fabrica do item, que grava b[0x14BFC] no canal ou no VFO e em b[0x15E2C]. */
FIRMWARE(SCRAMBLE_SET, 0x01E3D06C);    void SCRAMBLE_SET(void);

/* ---------- memoria do firmware que usamos ----------
 * VALOR (h[0x14BFC]) e o valor do item em edicao: o motor de menus (0x01E489E0) guarda o valor
 * escolhido, troca por linha enquanto desenha - e por isso que exibe() le dele - e devolve o
 * escolhido antes de chamar set().
 */
#define TEXTO          ((char *)0x000129B0)                  /* buffer do texto que vai para a tela */
#define VALOR          (*(volatile u16 *)0x00014BFC)
#define IDIOMA         (*(volatile u8 *)0x00014B74)          /* 1 = chines; o 0x9008 do chip de dados */
#define TDR            (*(volatile u8 *)0x00014B70)          /* dupla escuta; o get do item TDR, 0x01E3956C */
/* WATCH (mais abaixo): o modo, no registro proprio dele */
#define WATCH_DUAL     0
#define WATCH_SINGLE   1
#define WATCH_TRIPLE   2
/* O WATCH e o TRIPLE (v42 a v44) moram na faixa livre (bancos.ld, .livre.*): na area do fim da imagem
 * (3260 B) eles nao cabiam. As ajudantes dos bancos que eles usam ficam la; chamar entre as duas e
 * so mais um call. */
#define NA_LIVRE_COD   __attribute__((section(".livre.bancos.text")))
#define DADO_NA_LIVRE  __attribute__((section(".livre.bancos.rodata")))
unsigned watch_vista(void);
void aplica_mic(void);                    /* o ganho do microfone (v49, MIC GAIN) */
#define MAPA_EXISTE    ((volatile u8 *)0x00015D8B)           /* 120 B: canal esta gravado */
#define MAPA_BUSCA     ((volatile u8 *)0x00015D12)           /* 120 B: canal entra na busca */
#define ZONAS_EXISTE   (*(volatile u16 *)0x00015D10)         /* mascaras por zona, montadas junto */
#define ZONAS_EXISTE2  (*(volatile u16 *)0x00015E06)
#define ZONAS_BUSCA    (*(volatile u16 *)0x00015E08)
#define CANAL_ATUAL    (*(volatile u16 *)0x00015E3C)
#define MODO_ZONA      (*(volatile u8 *)0x00014BD4)          /* 0 = navega por zona (o b[0x14BD4] do DELCH) */
#define CANAL_LADO(l)  (*(volatile u16 *)(0x00015E0C + (l) * 4))  /* canal de cada lado, base 0 (h[0x15E0C], h[0x15E10]) */
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
/* Estado da varredura do espectro (83540): 0 sintoniza o ponto, 1 mede, 2 parado num achado. */
#define ESPECTRO_ESTADO (*(volatile u8 *)0x00014654)
/* Resto da varredura, cada um tirado da instrucao do firmware que o usa (conferir.py, 4f). */
#define ESP_BRUTO      (*(volatile u16 *)0x0001463C)         /* 83516: leitura crua do ponto */
#define ESP_MIN        (*(volatile u16 *)0x00014640)         /* 83520 */
#define ESP_MAX        (*(volatile u16 *)0x00014644)         /* 83524: maior leitura da passada */
#define ESP_INDICE     (*(volatile u8 *)0x0001464E)          /* 83534: coluna (ponto) atual */
#define ESP_PASSO      (*(volatile u16 *)0x00014650)         /* 83536: passo entre colunas, em 10 Hz */
#define ESP_FREQ       (*(volatile u32 *)0x000148DC)         /* 84188: frequencia sintonizada */
#define ESP_PICO_FREQ  (*(volatile u32 *)0x000148E0)         /* 84192: onde esta a maior leitura */
#define ESP_CENTRO     (*(volatile u32 *)0x000148E4)         /* 84196 */
#define ESP_FIM        (*(volatile u32 *)0x000148E8)         /* 84200 */
#define ESP_INICIO     (*(volatile u32 *)0x000148EC)         /* 84204 */
#define ALTURAS        ((volatile u8 *)0x0001551B)           /* 87323: 160 colunas do grafico, 0..64 */
#define ESP_PEDACO     (*(volatile u8 *)0x00014697)          /* 83607: qual das 5 faixas de 32 colunas o laco desenha */
/* Configuracao do espectro (0x6EC0, 73 bytes que o firmware grava na saida e le na partida). */
#define CFG_PONTOS     (*(volatile u32 *)0x00006EC4)         /* 28356: 0..3 = 160 >> n; nossos 4 = 640, 5 = 320 */
#define CFG_PASSO      (*(volatile u32 *)0x00006EC8)         /* 28360: 0..4 os de fabrica; nosso 5 = 10 kHz */
#define CFG_FILTRO     (*(volatile u32 *)0x00006ED0)         /* 28368: filtro do passo, para o FILTRO() */
/* Roger (84867, offset 0x17 do bloco de ajustes, 0x9017 no cabo, 2 bits no CHIRP): 0 OFF, 1 BEEP,
 * 2 TONE1200 e, nosso, 3 = BEEP 2. A tabela do BEEP de fabrica: (f1, f2) do segundo tom e (f1, f2)
 * do primeiro, que o laco de 0x01E44638 le de tras para frente. */
#define ROGER          (*(volatile u8 *)0x00014B83)
#define BEEP_FABRICA   ((const volatile u16 *)0x01E60DC8)
#define SCRAMBLE       (*(volatile u8 *)0x00015E2C)          /* scramble do canal ou VFO atual */
/* Teclas do espectro que viram nossas: OK troca o passo (17), # troca a quantidade de pontos (24). */
#define TECLA_OK         17
#define TECLA_CERQUILHA  24
/* Codigos das setas no tratador de teclas (0x01E4B946 le de b[sp+13]): 19 e a de cima - no modo
 * principal ela leva ao AVANCA do menu e no espectro sobe o limite -, 21 a de baixo. */
#define TECLA_CIMA   19
#define TECLA_BAIXO  21

#define NUM_BANCOS   16
#define NUM_CANAIS   960
#define TAM_MAPA     (NUM_CANAIS / 8)      /* 120 */
#define TAM_NOME     12
#define NUM_ZONAS    15                    /* 15 x 64 = 960; as mascaras por zona cobrem isso */
#define TAM_PEDACO   24                    /* le o mapa de um banco em pedacos; 120 = 5 x 24 */
/* Altura minima da barra do ponto que o espectro esta varrendo, para aparecer onde nao ha sinal. */
#define MINIMO_ATUAL     3

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
/* O WATCH (v43) num setor so dele, logo depois dos 16 bancos: na faixa que o levantamento pelo cabo
 * achou livre (docs/engenharia-reversa.md, secao 3). Desde a v46 a janela que o cabo grava o inclui
 * (gancho_cabo), para o CHIRP gravar o WATCH, o CH-C e o MDF-C. */
#define SETOR_WATCH    0x000D2000u
/* Os VFOs da C (v49): o da C e o do B guardado enquanto a C esta no lado B, no setor seguinte, tambem
 * livre no levantamento. Ficam fora do registro do WATCH porque a tela B le aquele a cada desenho, e
 * 64 B a mais na pilha de quem o le passariam da de fabrica; estes so a tecla A/B e o rodizio leem. */
#define SETOR_VFC      0x000D3000u

/* A marca fica em dois u16 em vez de quatro u8: conferir e gravar a marca passa a ser dois acessos
 * de 16 bits (as duas estruturas tem campo u16, entao o alinhamento de 2 e garantido) em vez de um
 * laco byte a byte, e as cadeias "BNK3"/"BCO1" saem da imagem. O formato gravado nao muda. */
#define MARCA_CAB    0x334B4E42u               /* "BNK3" */
#define MARCA_BANCO  0x314F4342u               /* "BCO1" */
#define MARCA_WATCH  0x31435457u               /* "WTC1" */
#define MARCA_VFC    0x31434656u               /* "VFC1" */
/* A marca sai do proprio endereco - o cabecalho tem a sua, os 16 setores de banco tem a outra -
 * em vez de viajar como parametro em cada chamada: um argumento a menos para errar. */
#define MARCA_DE(endereco)  ((endereco) == SETOR_CAB ? MARCA_CAB : \
                             (endereco) == SETOR_WATCH ? MARCA_WATCH : \
                             (endereco) == SETOR_VFC ? MARCA_VFC : MARCA_BANCO)

struct cabeca {
    u16 marca[2];                          /* "BNK3" */
    u16 ativos;                            /* um bit por banco ligado */
    u8 escolhido;                          /* banco em que os outros dois itens mexem */
    u8 reservado;
    u16 soma;
};

/* O WATCH (v42): quantas frequencias a tela principal mostra e, no TRIPLE, qual canal e a C e como
 * ela aparece. Sem registro valido, DUAL - o radio de sempre. */
struct vigia {
    u16 marca[2];                          /* "WTC1" */
    u8 modo;                               /* WATCH_DUAL, WATCH_SINGLE, WATCH_TRIPLE */
    u8 mdf_c;                              /* a exibicao da C, como a do MDF-A: 0 nome, 1 frequencia, 2 numero */
    u16 canal_c;                           /* o canal da C, 0..959 (o indice do CANAL de cada lado) */
    u8 c_no_b;                             /* 1 = a C esta no lado B, em evidencia (a tecla A/B a pos la) */
    u8 vfo;                                /* v49: bit 0 = a C em VFO; bit 1 = o B guardado em VFO (com a C
                                            * no lado B). Era reservado e gravado em 0: canal, como ate a v48 */
    u16 canal_b;                           /* com a C no lado B: o canal do B, que volta quando ela sai */
    u16 soma;
};

/* Os VFOs da C (v49), no formato do registro de VFO de cada lado (0x14E42 o do A, 0x14E62 o do B: o
 * "struct vfo" de 32 B do CHIRP, que o log 0x8000 guarda). */
struct vfc {
    u16 marca[2];                          /* "VFC1" */
    u8 vfo_c[32];                          /* o da C */
    u8 vfo_b[32];                          /* o do B, enquanto a C esta no lado B */
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
typedef char confere_vigia[sizeof(struct vigia) == 14 ? 1 : -1];
typedef char confere_vfc[sizeof(struct vfc) == 70 ? 1 : -1];
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
#define le_vigia(v)       le_registro(SETOR_WATCH, (v), sizeof(struct vigia))
#define grava_vigia(v)    grava_registro(SETOR_WATCH, (v), sizeof(struct vigia))
#define le_vfc(f)         le_registro(SETOR_VFC, (f), sizeof(struct vfc))
#define grava_vfc(f)      grava_registro(SETOR_VFC, (f), sizeof(struct vfc))
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
 * Devolve 1 se filtrou - e o liga/desliga do BANK usa essa resposta para nao deixar ligado um
 * banco que nao filtra nada, caso em que o radio fica mostrando tudo em vez de ficar sem canal.
 * A uniao fica na pilha e cada mapa entra em pedacos: nada de buffer grande emprestado (o do
 * firmware e a memoria de video). Quem chama poe o radio num canal que ainda vale (revalida_lados
 * ou REVALIDA), depois desta voltar: separada, a uniao nao fica na pilha durante a recarga. */
__attribute__((noinline)) static int aplica_filtro(int reconstruir)
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
    return tem != 0;
}

/* Poe os dois lados num canal que ainda vale, depois de o filtro mudar: sem isto, o lado fora de
 * evidencia ficava parado num canal que saiu do banco (ate a v37 so o REVALIDA rodava, e ele so
 * olha o lado em evidencia). E o que o DELCH de fabrica faz depois de apagar um canal, em canal
 * cheio (0x01E3C5D6..0x01E3C6C4): com algum canal existindo, o de cada lado que saiu do mapa pula
 * para o proximo que vale, e a recarga traz os dois lados (pede o redesenho pelo REVALIDA, como
 * antes). No modo por zona fica o REVALIDA do lado em evidencia, como ate a v37. */
static void revalida_lados(void)
{
    unsigned l;

    if (MODO_ZONA == 0) {
        REVALIDA();
        return;
    }
    if (ZONAS_EXISTE2)
        for (l = 0; l < 2; l++)
            if (!CANAL_VALE(CANAL_LADO(l), 0))
                CANAL_LADO(l) = (u16)PROXIMO_CANAL(CANAL_LADO(l), 0);
    RECARREGA_LADOS();
}

/* ---------- gancho da partida ----------
 * Entra no lugar da ultima chamada do carregamento de configuracao e canais: faz o que ela fazia
 * e aplica o filtro. Os mapas acabaram de ser montados pela partida, entao nao precisa remontar.
 * Roda depois da leitura do combo PTT2+atalho, entao nao atrapalha a regravacao pelo cabo.
 *
 * O radio fica sempre em ingles (v37, a pedido do dono: o item LANGUAGE saiu do menu). Os ajustes
 * acabaram de vir do chip de dados em bloco (0x9000 -> b[0x14B6C]), e um idioma chines gravado la -
 * pelo CHIRP de fabrica, por exemplo - vira ingles aqui, antes da tela inicial. O primeiro ajuste
 * salvo (0x01E3AB36) grava o 0 no chip. Fora daqui, so o set do LANGUAGE (0x01E3C9B8) gravava o
 * idioma, e o retorno aos ajustes de fabrica (0x01E4842E) o preserva.
 */
void gancho_partida(void)
{
    FIM_DA_CARGA();
    IDIOMA = 0;
    aplica_mic();                          /* por cima da copia da calibracao (MIC GAIN) */
    if (watch_vista() == WATCH_SINGLE)     /* no SINGLE a dupla escuta fica desligada (WATCH, abaixo) */
        TDR = 0;
    aplica_filtro(0);
    /* So o lado em evidencia: a partida ja recarregou os dois lados (0x01E56A9E), e uma segunda
     * recarga aqui pediria mais redesenho por cima do logo. */
    REVALIDA();
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

/* ---------- espectro: passos, quantidade de pontos, velocidade e pico ----------
 * Mapa completo na secao 11 de docs/engenharia-reversa.md. Os indices de fabrica da configuracao
 * (pontos 0..3, passo 0..4) mantem o significado, porque ela e gravada na saida do espectro e lida
 * na partida: os valores novos entram no fim (pontos 4 = 640 e 5 = 320, passo 5 = 10 kHz) e so a
 * ORDEM em que as teclas passam por eles muda. Valor fora da faixa (gravado por outra versao) vale
 * como o primeiro.
 *
 * 320 e 640 pontos: a maquina do firmware continua varrendo 160 colunas (o indice dela e um byte e
 * o grafico tem 160 colunas), com o passo entre colunas multiplicado por 2 ou 4; cada coluna mede
 * os seus 2 ou 4 sub-pontos no passo de verdade e fica com o maior. Qual sub-ponto esta sintonizado
 * sai da propria frequencia, entao nao precisa de memoria nova.
 *
 * Desde a v33 o codigo e as tabelas do espectro moram no espaco que era das vozes (ver bancos.ld),
 * e a area do fim da imagem fica para o resto. Nada daqui roda sem o modo de tela 20: todo caminho
 * do firmware ate estes ganchos passa pelo caso 20 do tratador de modos, pela entrada do espectro
 * (que grava o 20) ou pelo "se o modo nao for 20, pula" do laco principal, e nada do .text chama
 * para ca (secao 15 de docs/engenharia-reversa.md; o conferir.py confere as chamadas, 2b e 4g). */
#define NAS_VOZES       __attribute__((section(".vozes.text")))
#define DADO_NAS_VOZES  __attribute__((section(".vozes.rodata")))
/* As pecas da tela B do espectro moram no tela.o (telas.c, v41): os ganchos daqui so as chamam. */
void b_esp_rotulos(unsigned n, unsigned p);
void b_esp_pico(u32 f);
void b_esp_coluna(unsigned x, unsigned topo, unsigned atual);

#define PASSOS    6
#define LARGURAS  6
DADO_NAS_VOZES static const u16 passo_de[PASSOS] = {625, 833, 1250, 2000, 2500, 1000};   /* em 10 Hz */
DADO_NAS_VOZES static const u8 filtro_de[PASSOS] = {2, 1, 1, 0, 0, 1};    /* os de fabrica (0x01E60460) + 10k = 8.33/12.5 */
DADO_NAS_VOZES static const u8 passo_seguinte[PASSOS] = {1, 5, 3, 4, 0, 2};   /* 6.25 8.33 10 12.5 20 25 */

NAS_VOZES static unsigned cfg_pontos(void)
{
    unsigned n = CFG_PONTOS;

    return n < LARGURAS ? n : 0;
}

NAS_VOZES static unsigned cfg_passo(void)
{
    unsigned p = CFG_PASSO;

    return p < PASSOS ? p : 0;
}

/* Sub-pontos por coluna: 4 com 640 pontos, 2 com 320, 1 no resto. */
NAS_VOZES static unsigned sub_pontos(void)
{
    unsigned n = cfg_pontos();

    return n == 4 ? 4 : n == 5 ? 2 : 1;
}

/* Qual sub-ponto da coluna esta sintonizado: a frequencia e inicio + coluna * passo das colunas +
 * sub-ponto * passo de verdade. Sempre 0 com 160 pontos ou menos. */
NAS_VOZES static unsigned sub_ponto(void)
{
    unsigned k = sub_pontos();

    return (ESP_FREQ - ESP_INICIO) / (ESP_PASSO / k) % k;
}

/* No lugar de 0x01E4A9C2 (desvio na entrada dela): quantos pontos a maquina varre. */
NAS_VOZES unsigned espectro_pontos(void)
{
    unsigned n = cfg_pontos();

    return n < 4 ? 160 >> n : 160;
}

/* No lugar de 0x01E4A99C: o passo entre colunas, em 10 Hz. */
NAS_VOZES unsigned espectro_passo(void)
{
    return passo_de[cfg_passo()] * sub_pontos();
}

/* No lugar de 0x01E4ABDC: os rotulos da quantidade de pontos e do passo, pelas nossas tabelas (os
 * indices novos vao alem das de fabrica), desenhados pela tela B. */
NAS_VOZES void espectro_rotulos(unsigned n, unsigned p)
{
    b_esp_rotulos(n < LARGURAS ? n : 0, p < PASSOS ? p : 0);
}

/* Nas chamadas do proximo ponto (0x01E4AF86, que serve a varredura e a retomada, e 0x01E4C6B8, da
 * tecla 2): com sub-pontos, anda o passo de verdade dentro da coluna; no ultimo, volta ao comeco
 * dela e deixa o firmware passar para a coluna seguinte (ou recomecar a janela). */
NAS_VOZES void gancho_avanca(void)
{
    unsigned k = sub_pontos(), s = ESP_PASSO / k;

    if (sub_ponto() != k - 1) {
        ESP_FREQ += s;
        return;
    }
    ESP_FREQ -= (k - 1) * s;
    ESPECTRO_AVANCA();
}

/* Nas duas chamadas do registro do ponto (0x01E4AEE0 parado, 0x01E4AF7A varrendo), no lugar de
 * 0x01E4AD7C. Faz o que ele fazia - maior e menor leitura da passada, onde esta a maior, e a altura
 * nas colunas do ponto - com duas diferencas: a barra SOBE na hora e, quando o sinal cai, desce so
 * a METADE do caminho a cada passada (o sinal curto fica marcado e o ruido para de piscar); e, nos
 * sub-pontos depois do primeiro, a coluna fica com o maior. A leitura e a mesma que o firmware
 * usava: a medida guarda o mesmo valor em ESP_BRUTO e na lista de leituras que ele relia. */
NAS_VOZES void gancho_registra(void)
{
    unsigned bruto = ESP_BRUTO, largura = 160 / espectro_pontos(), novo = ALTURA(bruto);
    unsigned sub = sub_ponto();
    volatile u8 *c = ALTURAS + ESP_INDICE * largura;

    if (bruto > ESP_MAX) {
        ESP_MAX = bruto;
        ESP_PICO_FREQ = ESP_FREQ;
    }
    if (bruto < ESP_MIN)
        ESP_MIN = bruto;
    while (largura--) {
        unsigned velho = *c;

        *c++ = novo >= velho ? novo : sub ? velho : (velho + novo) >> 1;
    }
}

/* Quantas colunas do grafico cada ponto ocupa. */
NAS_VOZES static unsigned largura_ponto(void)
{
    return 160 / espectro_pontos();
}

/* No lugar da chamada que mostra o pico na volta ao comeco da janela (0x01E4AEA4, que ia para
 * MOSTRA_FREQ): a frequencia do pico da passada que terminou vai para a segunda linha, em verde, e
 * fica ate a passada seguinte terminar. */
NAS_VOZES void mostra_pico(u32 freq)
{
    b_esp_pico(freq);
}

/* Nas chamadas que centram a janela pela frequencia digitada (0x01E4C054) e na entrada do
 * espectro (0x01E4CFBC): zera o grafico antes, porque com o pico seguro as barras de outra janela
 * ficariam la, descendo devagar, por cima da nova. Pelo mesmo motivo apaga o pico. */
NAS_VOZES void gancho_janela(u32 centro)
{
    zera((void *)ALTURAS, 160);
    b_esp_pico(0);
    JANELA_ESPECTRO(centro);
}

/* Nas cinco chamadas que desenham as barras (0x01E4D600, 0x01E4D644, 0x01E4D688, 0x01E4D6CC,
 * 0x01E4D710), no lugar de 0x01E4AAA8: o laco principal redesenha uma faixa de 32 colunas por volta
 * (ESP_PEDACO, 0 a 4), e cada barra chega com x relativo a faixa (x0 = x1), a base e o topo (64 menos
 * a altura); a cor vai na pilha e nao serve mais. As colunas do ponto que a varredura esta medindo
 * saem em branco, com 3 px no minimo para aparecer onde nao ha sinal. */
NAS_VOZES void gancho_coluna(unsigned x0, unsigned x1, unsigned base, unsigned topo)
{
    unsigned largura = largura_ponto(), atual = ESP_PEDACO * 32 + x0 - ESP_INDICE * largura < largura;

    if (atual && topo > 64 - MINIMO_ATUAL)
        topo = 64 - MINIMO_ATUAL;
    b_esp_coluna(x0, topo, atual);
    (void)x1;
    (void)base;
}

/* No lugar da chamada da varredura no laco principal (0x01E4D5B2). De fabrica cada ponto custa
 * duas voltas do laco (uma sintoniza, a seguinte mede). Aqui, quando a volta mediu e passou ao
 * ponto seguinte, a mesma volta ja sintoniza esse ponto - e ele so e medido na volta seguinte, como
 * no firmware, entao o chip tem o mesmo tempo para estabilizar. Um ponto por volta: o dobro.
 *
 * A frequencia grande de cima acompanha a varredura. A volta que sintonizou um ponto novo mostra a
 * frequencia dele, mas so quando o laco vai redesenhar a faixa do grafico em que o ponto esta:
 * frequencia e barra branca trocam juntas, a cada 5 voltas, e cada troca manda 3 KB ao LCD. Com
 * digitos sendo digitados (b[0x14A35], o que o firmware confere antes de mostrar o pico) ela nao
 * escreve por cima. Parado num achado nada e sintonizado, e fica a frequencia do achado, que o
 * firmware mostra. */
NAS_VOZES void gancho_espectro(unsigned arg)
{
    unsigned antes = ESPECTRO_ESTADO, sintonizou = antes == 0;

    ESPECTRO(arg);
    if (antes == 1 && ESPECTRO_ESTADO == 0) {
        ESPECTRO(arg);
        sintonizou = 1;
    }
    if (sintonizou && EDITOR_TAM == 0 && ESP_PEDACO == ESP_INDICE * largura_ponto() / 32)
        MOSTRA_FREQ(ESP_FREQ);
}

/* O que o firmware faz depois de trocar o passo ou a quantidade de pontos (0x01E4C628..0x01E4C664):
 * filtro do passo, janela nova, frequencia e rotulos, e a varredura recomeca. */
NAS_VOZES static void reconfigura(void)
{
    unsigned filtro = filtro_de[cfg_passo()];

    CFG_FILTRO = filtro;
    FILTRO(filtro);
    gancho_janela(ESP_CENTRO);
    MOSTRA_FREQ(ESP_PICO_FREQ);
    ROTULOS_FREQ(ESP_INICIO, ESP_FIM);
    espectro_rotulos(cfg_pontos(), cfg_passo());
    ESPECTRO(0);
}

/* ---------- gancho das teclas do espectro ----------
 * Entra no lugar do "r1 = r0 - 16" com que o tratador de teclas do espectro (0x01E4BFBE) escolhe o
 * caso do tbh: recebe a tecla em r0 e devolve r0 = a tecla e r1 = o caso, num u64 (o clang poe a
 * metade de baixo em r0 e a de cima em r1). A chamada passa a sujar r2 e r3, que estao mortos ali
 * em todos os caminhos. Tres coisas mudam, o resto e o caso de fabrica:
 *  - parado num achado (estado 2), as setas (que de fabrica so mexem no limite, +-2 em [0x6ED5])
 *    fazem a mesma retomada do firmware (0x01E4AEFC) e viram o caso da tecla 20, que so bipa;
 *  - OK e # trocam passo e pontos pela nossa ordem, com os valores novos, e fazem o que o firmware
 *    faz em seguida; voltam como tecla 255 no caso 20, que pula o bipe - como as duas de fabrica.
 */
NAS_VOZES unsigned long long gancho_tecla_espectro(unsigned tecla)
{
    unsigned caso = tecla - 16;

    if ((tecla == TECLA_CIMA || tecla == TECLA_BAIXO) && ESPECTRO_ESTADO == 2) {
        ESPECTRO_ESTADO = 0;
        AUDIO_DO_CHIP(0);
        PEDE_AMPLI(6);
        AMPLI(0);
        gancho_avanca();
        caso = 20 - 16;
    } else if (tecla == TECLA_OK || tecla == TECLA_CERQUILHA) {
        if (tecla == TECLA_OK)
            CFG_PASSO = passo_seguinte[cfg_passo()];
        else
            CFG_PONTOS = (cfg_pontos() + 1) % LARGURAS;
        reconfigura();
        tecla = 255;
        caso = 20 - 16;
    }
    return (unsigned long long)caso << 32 | tecla;
}

/* ---------- roger: BEEP 2 e COIN ----------
 * O item ROGER do menu passa a ter cinco opcoes, na ordem OFF, BEEP 1, BEEP 2, COIN, TONE1200. Os
 * valores gravados continuam os de fabrica (0 OFF, 1 BEEP, 2 TONE1200); o BEEP 2 fica com o 3 e o
 * COIN com o 4. No radio o ajuste e um byte inteiro, e no CHIRP o 0x9017 tem 6 bits sem uso acima
 * dos 2 do roger. O get e o set do item trocam posicao e valor pelas duas tabelas abaixo.
 *
 * O fim da transmissao (0x01E44604) le o byte inteiro e toca o roger antes de voltar a receber. O 2
 * desvia logo, para o TONE1200 (0x01E4461A, "se for 2"). O desvio seguinte foi trocado (0x01E4461E:
 * "se nao for 1, pula" virou "se for 0, pula"), entao o 3 e o 4 entram pelo mesmo caminho do BEEP -
 * liga o gerador, toca, desliga, mesmo final - e o laco dos tons (0x01E44638) virou uma chamada
 * para ca e um desvio por cima dele. */
const char *const roger_opcoes[5] = {"OFF", "BEEP 1", "BEEP 2", "COIN", "TONE1200"};
static const u8 roger_valor[5] = {0, 1, 3, 4, 2};      /* posicao no menu -> valor gravado */
static const u8 roger_posicao[5] = {0, 1, 4, 2, 3};    /* valor gravado -> posicao no menu */
/* Os tons, em pares (frequencia, espera em unidades de 2 ms), ate o 0. A frequencia vai em unidades
 * do gerador: Hz x 10.32444, a mesma conta que da 10324 para os 1000 Hz do BEEP e 12389 para o
 * TONE1200. Os dois sons vieram em mp3 do dono, medidos:
 *  - BEEP 2: 1125, 1857, 1359 e 931 Hz, 100 ms cada;
 *  - COIN, a moeda do Super Mario World: 2000 Hz por 70 ms e 2672 Hz por 330 ms. O gerador toca
 *    com volume constante, e no original a segunda nota cai 22 dB depois de uns 40 ms. */
static const u16 beep2[] = {11615, 50, 19172, 50, 14031, 50, 9612, 50, 0};
static const u16 beep3[] = {20649, 35, 27587, 165, 0};

u16 roger_le(void)
{
    unsigned v = ROGER;

    return v < 5 ? roger_posicao[v] : 0;
}

void roger_grava(void)
{
    unsigned p = VALOR;

    ROGER = p < 5 ? roger_valor[p] : 0;
}

/* ---------- WATCH: quantas frequencias a tela principal mostra (v42) ----------
 * SINGLE mostra so o lado em evidencia, em tela cheia; DUAL e o A/B de sempre; TRIPLE poe uma
 * terceira, a C - um canal da memoria (item CH-C), mostrado como o MDF-C pede -, numa faixa fina
 * embaixo, e o lado que nao esta em evidencia tambem fica fino. Tudo mora no registro do WATCH
 * (struct vigia, setor 0xD2000), que a tela B le a cada desenho (watch_le_tudo).
 *
 * A dupla escuta (o TDR, b[0x14B70], que o laco do sinal le na hora - o set de fabrica do item,
 * 0x01E3957C, so grava o byte) segue a regra do dono: no SINGLE fica em 0, e so a frequencia
 * mostrada recebe; no DUAL e no TRIPLE, ligada escuta todas, desligada so a que esta em evidencia.
 * O WATCH em SINGLE a desliga, o set do TDR a mantem em 0 enquanto for SINGLE, e a partida a zera
 * de novo, para o caso de um upload do CHIRP a ter religado. */
DADO_NA_LIVRE const char titulo_watch[] = "WATCH";
DADO_NA_LIVRE const char titulo_canal_c[] = "CH-C";
DADO_NA_LIVRE const char titulo_mdf_c[] = "MDF-C";
DADO_NA_LIVRE const char *const watch_opcoes[3] = {"SINGLE", "DUAL", "TRIPLE"};
DADO_NA_LIVRE static const u8 watch_valor[3] = {WATCH_SINGLE, WATCH_DUAL, WATCH_TRIPLE};   /* posicao no menu -> modo */
DADO_NA_LIVRE static const u8 watch_posicao[3] = {1, 0, 2};                                /* modo -> posicao no menu */

/* O registro inteiro numa palavra, para a tela B: modo | mdf_c << 8 | canal_c << 16. Sem registro
 * valido (radio que nunca mexeu no WATCH), tudo zero: DUAL. */
NA_LIVRE_COD u32 watch_le_tudo(void)
{
    struct vigia v;

    le_vigia(&v);
    if (v.modo > WATCH_TRIPLE)
        v.modo = WATCH_DUAL;
    if (v.mdf_c > 2)
        v.mdf_c = 0;
    if (v.canal_c >= NUM_CANAIS)
        v.canal_c = 0;
    return v.modo | (u32)v.mdf_c << 8 | (u32)v.canal_c << 16;
}

NA_LIVRE_COD unsigned watch_vista(void)
{
    return watch_le_tudo() & 0xFF;
}

NA_LIVRE_COD u16 watch_le(void)
{
    return watch_posicao[watch_vista()];
}

NA_LIVRE_COD void watch_grava(void)
{
    struct vigia v;
    unsigned p = VALOR;

    le_vigia(&v);
    v.modo = p < 3 ? watch_valor[p] : WATCH_DUAL;
    grava_vigia(&v);
    if (v.modo == WATCH_SINGLE)
        TDR = 0;
}

/* O set do item TDR no lugar do de fabrica (que so faz TDR = VALOR): no SINGLE, fica em 0. */
NA_LIVRE_COD void tdr_grava(void)
{
    TDR = watch_vista() == WATCH_SINGLE ? 0 : VALOR;
}

/* CH-C: o canal da C, como o BANK CH percorre os canais (numero, max 960); canal vazio sai com " -". */
NA_LIVRE_COD char *canal_c_exibe(void)
{
    unsigned canal = VALOR;

    SPRINTF(TEXTO, "CH-%03d", canal + 1);
    if (!canal_existe(canal))
        acrescenta(TEXTO, " -");
    return TEXTO;
}

/* Com a C no lado B (TRIPLE, tecla A/B), o canal dela e o do lado B - as setas o mudam, como em
 * qualquer lado -, e o CH-C le e grava esse; com ela em evidencia e em canal, o canal novo vai para
 * ela ja (em VFO, fica guardado no lado para a tecla VFO/MR, como o de qualquer lado). */
#define CANAL_DO_B     (*(volatile u16 *)0x00015E10)         /* h[0x15E0C + 4], o canal gravado do lado B */
#define MODO_LADOS     (*(volatile u8 *)0x00014B86)          /* um nibble por lado: 1 = canal, 0 = VFO */
#define B_EM_CANAL     ((MODO_LADOS >> 4) == 1)

NA_LIVRE_COD u16 canal_c_le(void)
{
    struct vigia v;

    le_vigia(&v);
    return v.c_no_b ? CANAL_DO_B : v.canal_c < NUM_CANAIS ? v.canal_c : 0;
}

NA_LIVRE_COD void canal_c_grava(void)
{
    struct vigia v;
    unsigned canal = VALOR;

    if (canal >= NUM_CANAIS)
        return;
    le_vigia(&v);
    v.canal_c = canal;
    grava_vigia(&v);
    if (v.c_no_b && LADO_B == 1) {
        CANAL_DO_B = canal;
        if (B_EM_CANAL) {
            PARA_DW();
            RECARREGA_LADOS();             /* carrega, grava o canal do lado e o log (0x01E3A42A) */
        }
    }
}

/* MDF-C: a exibicao da C. O item usa a tabela de opcoes do MDF-A, e o valor e o mesmo dele. */
NA_LIVRE_COD u16 mdf_c_le(void)
{
    return watch_le_tudo() >> 8 & 0xFF;
}

NA_LIVRE_COD void mdf_c_grava(void)
{
    struct vigia v;
    unsigned m = VALOR;

    le_vigia(&v);
    v.mdf_c = m < 3 ? m : 0;
    grava_vigia(&v);
}

/* ---------- TRIPLE: a C, um terceiro lado (v44) ----------
 * O firmware so tem dois lados (A e B), e a dupla escuta (o laco do sinal, 0x01E4622A) vai e volta
 * entre eles: ouve o lado em evidencia e, a cada 50 tiques sem sinal, vai ao outro (0x01E48220 -
 * copia a sombra do "outro" para a "corrente", poe b[0x15378] = 1 e decodifica, 0x01E4826E) e volta
 * (0x01E481C6 - b[0x15378] = 0 e 0x01E3FD9E, que copia a sombra do lado em evidencia para a
 * corrente e decodifica, chamada em 0x01E481CC). A sintonia le o que foi decodificado. Nada disso
 * grava na flash, nem mexe no registro da tela (0x14DC4).
 *
 * Desde a v49 a C e um lado como o A e o B: modo (VFO ou canal), VFO de 32 B e canal (o CH-C). O
 * modo e o canal ficam no registro do WATCH; o VFO, no setor 0xD3000 (struct vfc).
 *
 * A C entra de dois jeitos, sem tocar no motor:
 *  - Vigiada (TDR ligado). Na volta do outro lado, em vez de voltar ao lado em evidencia, a
 *    dupla escuta vai a terceira - a que nao esta em nenhum dos dois lados: a C, ou o B quando a C
 *    esta no lado B -, com b[0x15378] em 1 e ela na corrente: em canal, o registro cru lido da
 *    memoria, como o carregador de canal de fabrica le (0x01E3A650: 20 B de canal x 32); em VFO, os
 *    32 B do VFO dela no VFO corrente (0x15E80), como o carregador de VFO de fabrica copia
 *    (0x01E3A5E8: so os 32 B do registro do lado). Na volta seguinte, a de fabrica restaura o lado em
 *    evidencia. As tres ficam com os mesmos 50 tiques: A, B, C, A... A decodificacao e feita como na
 *    ida ao outro lado - com o lado em evidencia invertido em volta dela -, e a potencia do outro
 *    lado, que ela grava (0x01E38F34, com b[0x153A1] = 165), e devolvida.
 *  - Em evidencia (tecla A/B). A troca de lados de fabrica (0x01E40E52, chamada pela tecla em
 *    0x01E4D44C) passa por gancho_ab: A -> B como sempre; B -> C guarda o modo, o canal e o VFO do B
 *    e poe os da C nas vagas do lado B - o nibble do B em 0x14B86, o canal do lado em h[0x15E10] e o
 *    registro de VFO do B (0x14E62, pelo VFO corrente e 0x01E3A4FE(1), que grava o log 0x8000) -, e
 *    remonta os dois lados pela rotina da tecla VFO/MR (0x01E3A812: a partida de cada lado pelo
 *    modo e pelo canal guardados, e 0x01E3A700, que carrega e grava o canal do lado e o log). Os
 *    ajustes vao para o chip como a VFO/MR os grava (0x01E3AB36), e a tela inteira e refeita
 *    (0x01E3C290). Enquanto a C esta no lado B, tudo o que mexe no lado B (VFO/MR, setas, digitacao,
 *    STEP, TXP, tons...) e o firmware de fabrica mexendo nela. C -> A recolhe o modo, o canal e o VFO
 *    que estao no lado B para a C, devolve os do B do mesmo jeito e faz a troca de fabrica. Ate a
 *    v48 era o caminho das setas (0x01E40B9E), que so servia com o B e a C em canal.
 *
 * O estado do rodizio fica em b[0x14BD0], o 0x9024 do bloco de ajustes (o "Single Mode" do RT-900
 * BT, que este firmware nao usa): fora as copias do bloco (gravar ajustes, padrao, partida e cabo),
 * nada o le nem o grava - conferido no desmontado inteiro. Gravar ajustes o leva junto para o chip
 * de dados, e um upload do CHIRP o sobrescreve: os dois so mudam quem vem primeiro no rodizio. */
#define RODIZIO        (*(volatile u8 *)0x00014BD0)          /* 2 = a dupla escuta esta na terceira */
#define DW_OUTRO       (*(volatile u8 *)0x00015378)          /* 1 = a dupla escuta esta fora do lado em evidencia */
#define ESTADO_RX      (*(volatile u8 *)0x00015367)          /* 4 = recebendo (o tbh de 0x01E467A6) */
#define CORRENTE_CRU   ((u8 *)0x00015E44)                    /* o canal cru do lado que esta sendo ouvido, 20 B */
#define CORRENTE_MODO  (*(volatile u8 *)0x00015E12)          /* 1 = canal */
#define EVIDENCIA_MODO (*(volatile u8 *)0x00015E13)
#define CORRENTE_CANAL (*(volatile u16 *)0x00015E3C)
#define POTENCIA_LADO  ((volatile u8 *)0x00015E30)
#define EXIBICAO_B     (*(volatile u8 *)0x00014B7A)          /* o MDF-B */
#define CORRENTE_VFO   ((u8 *)0x00015E80)                    /* o VFO do lado que esta sendo ouvido (36 B; o registro e 32) */
#define REGISTRO_VFO_B ((u8 *)0x00014E62)                    /* o registro de VFO do B, o que o log 0x8000 guarda */
#define POS_VFO(do_b)  (SETOR_VFC + 4 + ((do_b) ? 32u : 0u))  /* o vfo_c ou o vfo_b da struct vfc, no chip */
#define EVIDENCIA_VFO  ((const u8 *)0x00015EA4)              /* a sombra do VFO do lado em evidencia */
/* A marca da troca de lado do PTT (b[0x15384]; o firmware poe 1 quando troca para o TX-A/B) com os bits
 * do TX-A/B/C (mais abaixo): a TX vai a terceira, ela ja esta no lado B, o A estava em evidencia; bits
 * 0..3, a potencia do lado B. */
#define TX_TROCA       (*(volatile u8 *)0x00015384)
#define TX_TERCEIRA    0x80u
#define TX_NO_B        0x40u
#define TX_TROCOU      0x20u
#define TX_VIRTUAL     ((TX_TROCA & (TX_TERCEIRA | TX_NO_B)) == (TX_TERCEIRA | TX_NO_B))

/* Os VFOs da C so valem com o setor dela gravado: sem ele, a C fica em canal, como ate a v48. So a
 * marca, para a pilha de quem desenha a tela (a soma, quem grava confere). */
NA_LIVRE_COD static int vfc_existe(void)
{
    u16 m[2];

    LE_FLASH(SETOR_VFC, m, sizeof m);
    return marca_ok(m, MARCA_VFC);
}

/* A terceira: a que nao esta em nenhum dos dois lados - a C, ou o B quando a C esta no lado B. Bits
 * 0..15 o canal, 16..23 a exibicao, 24 em VFO, 25 o VFO e o do B guardado (senao o da C). 0xFFFF:
 * nao ha terceira (fora do TRIPLE, ou em canal e canal vazio). */
#define TERCEIRA_VFO   (1u << 24)
#define TERCEIRA_DO_B  (1u << 25)
#define TERCEIRA_RAM   (1u << 26)    /* o VFO dela e a sombra do lado em evidencia (TX-A/B/C, v51) */
NA_LIVRE_COD static u32 terceira_de(const struct vigia *v)
{
    unsigned do_b = v->c_no_b != 0, canal = do_b ? v->canal_b : v->canal_c;
    u32 em_vfo = (v->vfo >> do_b & 1) && vfc_existe() ? TERCEIRA_VFO : 0;

    if (v->modo != WATCH_TRIPLE || (!em_vfo && !canal_existe(canal)))
        return 0xFFFF;
    return canal | (u32)(do_b ? EXIBICAO_B : v->mdf_c) << 16 | em_vfo | (do_b ? TERCEIRA_DO_B : 0);
}

/* A frequencia do VFO da terceira (bits como os de terceira()), para a faixa dela na tela B: os 8
 * primeiros bytes do registro de VFO sao os digitos, em dezenas de Hz (como o CHIRP os le). */
NA_LIVRE_COD u32 freq_terceira(u32 t)
{
    u8 d[8];
    u32 f = 0;
    unsigned k;

    if (t & TERCEIRA_RAM)
        COPIA_MEM(d, EVIDENCIA_VFO, sizeof d);
    else
        LE_FLASH(POS_VFO(t & TERCEIRA_DO_B), d, sizeof d);
    for (k = 0; k < 8; k++)
        f = f * 10 + (d[k] < 10 ? d[k] : 0);
    return f;
}

/* Na TX-A/B/C com a terceira no lado B (v51), a que saiu de la e a terceira da tela: as vagas do lado
 * B (o modo, o canal e a sombra do VFO, que o PTT nao toca), com o MDF dela. */
NA_LIVRE_COD static u32 deslocada(const struct vigia *v)
{
    unsigned do_b = !v->c_no_b;

    return CANAL_DO_B | (u32)(do_b ? EXIBICAO_B : v->mdf_c) << 16
           | (B_EM_CANAL ? 0 : TERCEIRA_VFO | TERCEIRA_RAM) | (do_b ? TERCEIRA_DO_B : 0);
}

NA_LIVRE_COD u32 terceira(void)
{
    struct vigia v;

    le_vigia(&v);
    return TX_VIRTUAL ? deslocada(&v) : terceira_de(&v);
}

/* Para a tela: o modo - 3 no lugar do TRIPLE quando a C esta no lado B - e o MDF-C << 8. Na TX-A/B/C
 * com a terceira no lado B (v51), o c_no_b ao contrario, o bit 31 e, nos bits 16..30, o canal dela
 * (base 1; 0 = VFO) para o cartao do lado B, cujas vagas continuam as de quem saiu. */
NA_LIVRE_COD unsigned watch_tela(void)
{
    struct vigia v;
    unsigned tela;
    u32 t;

    le_vigia(&v);
    if (v.modo > WATCH_TRIPLE)
        return WATCH_DUAL;
    tela = (v.modo == WATCH_TRIPLE && (v.c_no_b != 0) != TX_VIRTUAL ? 3 : v.modo)
           | (v.mdf_c < 3 ? v.mdf_c : 0) << 8;
    if (!TX_VIRTUAL)
        return tela;
    t = terceira_de(&v);
    return tela | 1u << 31 | (t & TERCEIRA_VFO ? 0 : ((t & 0xFFFF) + 1) << 16);
}

/* Na ida ao outro lado (0x01E4826E): a de fabrica, e o rodizio sabe que nao esta na terceira. */
NA_LIVRE_COD void gancho_troca_dw(void)
{
    RODIZIO = 0;
    DECODIFICA();
}

/* A terceira (t, de terceira_de()) na corrente: e o que o rodizio ouve e, desde a v50, o que o PTT
 * transmite com o TX-A/B/C nela. */
NA_LIVRE_COD static void le_terceira(u32 t)
{
    if (t & TERCEIRA_VFO) {                  /* como o carregador de VFO de fabrica: os 32 B do registro */
        LE_FLASH(POS_VFO(t & TERCEIRA_DO_B), CORRENTE_VFO, 32);
        CORRENTE_MODO = 0;
    } else {
        LE_FLASH((t & 0xFFFF) * 32, CORRENTE_CRU, 20);
        CORRENTE_MODO = 1;
        CORRENTE_CANAL = t & 0xFFFF;
    }
}

/* No rodizio, decodificada como a ida ao outro lado: com o lado em evidencia invertido em volta. */
NA_LIVRE_COD static void poe_terceira(u32 t)
{
    unsigned lado = !LADO_B;
    u8 potencia;

    le_terceira(t);
    LADO_B = lado;                           /* como a ida ao outro lado decodifica */
    potencia = POTENCIA_LADO[lado];
    DECODIFICA();
    POTENCIA_LADO[lado] = potencia;
    LADO_B = !lado;
}

/* Na volta do outro lado (0x01E481CC, logo depois de b[0x15378] = 0): no TRIPLE, se nao vem da
 * terceira, vai a ela; senao, a volta de fabrica ao lado em evidencia. */
NA_LIVRE_COD void gancho_volta_dw(void)
{
    u32 t = terceira();

    if (t == 0xFFFF || RODIZIO == 2) {
        RODIZIO = 0;
        VOLTA_DW();
        return;
    }
    RODIZIO = 2;
    DW_OUTRO = 1;                            /* continua fora do lado em evidencia */
    poe_terceira(t);
}

/* Para a tela B: o lado fora de evidencia (ou a terceira) esta recebendo, e se e a terceira. */
NA_LIVRE_COD unsigned outro_recebendo(void)
{
    return ESTADO_RX == 4 && DW_OUTRO == 1;
}

NA_LIVRE_COD unsigned c_no_lugar(void)
{
    return DW_OUTRO == 1 && RODIZIO == 2;
}

/* As vagas do lado B (em evidencia): o modo no nibble alto de 0x14B86, o canal guardado em h[0x15E10]
 * e o registro de VFO 0x14E62. recolhe_b copia o VFO - antes, o VFO que esta sendo usado vai para o
 * registro, se o lado esta em VFO (o desarme da dupla escuta deixa o lado em evidencia na corrente) - e
 * devolve 1 com o lado em VFO. */
NA_LIVRE_COD static unsigned recolhe_b(u8 *vfo)
{
    PARA_DW();
    if (CORRENTE_MODO == 0)
        SALVA_VFO(1);
    COPIA_MEM(vfo, REGISTRO_VFO_B, 32);
    return !B_EM_CANAL;
}

/* O contrario: o VFO entra pelo VFO corrente e 0x01E3A4FE(1), que o copia para o registro do B e grava
 * o log 0x8000; o canal e o modo, nas vagas; e a remontagem da tecla VFO/MR (0x01E3A812) refaz os dois
 * lados a partir delas. Os ajustes, com o nibble, vao para o chip como a VFO/MR os grava. */
NA_LIVRE_COD static void poe_no_b(unsigned em_vfo, unsigned canal, const u8 *vfo)
{
    COPIA_MEM(CORRENTE_VFO, vfo, 32);
    SALVA_VFO(1);
    CANAL_DO_B = canal;
    MODO_LADOS = (MODO_LADOS & 0x0F) | (em_vfo ? 0 : 0x10);
    RECARREGA_LADOS();
    SALVA_AJUSTES();
}

/* B -> C: guarda o modo, o canal e o VFO do B e poe os da C. Sem o setor dos VFOs (a primeira vez), a C
 * comeca com o VFO do B. */
NA_LIVRE_COD __attribute__((noinline)) static void vai_c(struct vigia *v)
{
    struct vfc f;
    unsigned tinha = le_vfc(&f);

    v->vfo = (v->vfo & ~2) | recolhe_b(f.vfo_b) << 1;
    if (!tinha)
        COPIA_MEM(f.vfo_c, f.vfo_b, 32);
    v->canal_b = CANAL_DO_B;
    v->c_no_b = 1;
    grava_vigia(v);
    grava_vfc(&f);
    poe_no_b(v->vfo & 1, v->canal_c, f.vfo_c);
}

/* Com o lado B em evidencia e a C nele: recolhe o modo, o canal e o VFO em que a C ficou e devolve os
 * do B. Sem o setor dos VFOs (a C foi para o lado B antes da v49, pelo caminho das setas, que nao mexia
 * em VFO), o registro do B ainda e o dele, e o B volta em canal, como ia. */
NA_LIVRE_COD __attribute__((noinline)) static void volta_b(struct vigia *v)
{
    struct vfc f;
    unsigned tinha = le_vfc(&f), c_em_vfo = recolhe_b(f.vfo_c);

    if (!tinha) {
        COPIA_MEM(f.vfo_b, f.vfo_c, 32);
        v->vfo &= ~2;
    }
    if (!c_em_vfo)
        v->canal_c = CANAL_DO_B;
    v->vfo = (v->vfo & ~1) | c_em_vfo;
    v->c_no_b = 0;
    grava_vigia(v);
    grava_vfc(&f);
    poe_no_b(v->vfo >> 1 & 1, v->canal_b, f.vfo_b);
}

/* A tecla A/B (0x01E4D44C, no lugar de 0x01E40E52(1)). Fora do TRIPLE, a troca de sempre - e, se a C
 * ficou no lado B (o WATCH saiu do TRIPLE com ela em evidencia), o B volta. */
NA_LIVRE_COD void gancho_ab(void)
{
    struct vigia v;

    le_vigia(&v);
    if (LADO_B == 0) {                       /* A -> B */
        TROCA_AB(1);
        if (v.c_no_b) {
            volta_b(&v);
            TELA_INTEIRA();
        }
        return;
    }
    if (v.c_no_b) {                          /* C -> A */
        volta_b(&v);
        TROCA_AB(1);
        return;
    }
    if (v.modo == WATCH_TRIPLE && ((v.vfo & 1) || canal_existe(v.canal_c))) {
        vai_c(&v);                           /* B -> C */
        TELA_INTEIRA();                      /* a remontagem nao redesenha; a troca de fabrica, sim (0x01E40EF4) */
        return;
    }
    TROCA_AB(1);                             /* B -> A: fora do TRIPLE, ou a C em canal vazio */
}

/* ---------- MIC GAIN: o ganho do microfone (v49) ----------
 * A cada transmissao (0x01E477D8) o firmware poe nos 6 bits de baixo do registrador 0x7D do chip de
 * radio (a sensibilidade do microfone; no BK4819, bits 4..0, de 0 a 31 em passos de 0,5 dB) o byte
 * b[0x706C], que a partida copia do 0xF210 da calibracao de fabrica (0x01E561BE); nada mais le nem
 * grava esse byte. O MIC GAIN escolhe um de tres niveis, guardado no byte 0x9033 do bloco de ajustes
 * (b[0x14BDF], que o firmware nao usa: fora as copias do bloco - gravar ajustes, padrao, partida e
 * cabo -, nada o le nem o grava, conferido no desmontado inteiro), e o poe em b[0x706C] na partida
 * (todo caminho ate o gancho da partida passa pela copia da calibracao, 0x01E561CC) e na hora em que
 * muda. A saida do menu grava os ajustes no chip (0x01E3C2B0), e o CHIRP o le e grava no bloco
 * 0x9000. A calibracao de fabrica nao muda. 0, o padrao, e HIGH: o topo, perto dos 30 de fabrica. */
#define MIC_NIVEL      (*(volatile u8 *)0x00014BDF)
#define MIC_GANHO      (*(volatile u8 *)0x0000706C)
DADO_NA_LIVRE const char titulo_mic[] = "MIC GAIN";
DADO_NA_LIVRE const char *const mic_opcoes[3] = {"HIGH", "MID", "LOW"};
DADO_NA_LIVRE static const u8 mic_ganho[3] = {31, 25, 19};   /* o topo, -3 dB e -6 dB */

NA_LIVRE_COD u16 mic_le(void)
{
    return MIC_NIVEL < 3 ? MIC_NIVEL : 0;
}

NA_LIVRE_COD void aplica_mic(void)
{
    MIC_GANHO = mic_ganho[mic_le()];
}

NA_LIVRE_COD void mic_grava(void)
{
    unsigned n = VALOR;

    MIC_NIVEL = n < 3 ? n : 0;
    aplica_mic();                          /* vale ja na proxima transmissao */
}

/* Os ajustes que mudam por fora do menu tambem valem na hora: o 'W' do bloco 0x9000 pelo cabo (o
 * upload do CHIRP) termina num goto para a gravacao dos ajustes (0x01E43A50), e o retorno aos ajustes
 * de fabrica os grava com uma chamada (0x01E4845A), sem reiniciar o radio. Os dois passam por aqui: o
 * ganho do nivel novo, e a gravacao de fabrica (0x01E3AB36, que nao le registrador de entrada). */
NA_LIVRE_COD void gancho_ajustes(void)
{
    aplica_mic();
    SALVA_AJUSTES();
}

/* ---------- a luz no espectro (v49) ----------
 * O laco do sinal apaga a luz quando a contagem do ABR (h[0x14F80], que o tique desce nos modos 0, 2,
 * 3, 11 e 20 - 0x01E41876 e 0x01E41A0C) chega a 0: 0x01E394D6(0) em 0x01E46578, o unico lugar que
 * apaga (a marca da luz, b[0x15386], so vai a 0 dentro dela; os outros tres chamadores acendem). No
 * espectro (modo 20, so a entrada por tecla o grava, 0x01E4CF7E) ela fica acesa. Nada e gravado: ao
 * sair do espectro, ou ao religar o radio, o ABR volta a valer. */
#define MODO_TELA      (*(volatile u8 *)0x0001533D)
NA_LIVRE_COD void gancho_luz(unsigned acende)
{
    if (MODO_TELA != 20)
        LUZ(acende);
}

/* ---------- TX-A/B/C: o PTT transmite no A, no B ou na C (v50, v51) ----------
 * O TX-A/B de fabrica (item 44; b[0x14B7F], o 0x9013 dos ajustes: 0 OFF, 1 A, 2 B) so vale com o TDR
 * ligado. Na ida do PTT (0x01E43F86..0x01E43FBC), se o lado escolhido nao esta em evidencia, o firmware
 * troca os lados (0x01E40E52(0), que nao grava nada) e marca b[0x15384] = 1; no fim da TX (0x01E473DC) e
 * na TX recusada (0x01E411A6), com a marca, destroca e a zera. O get e o set do item so copiam o byte, e
 * nada mais o le (conferido no desmontado inteiro). O item ganha o C (3) e vira TX-A/B/C.
 *
 * A letra que nao esta em nenhum dos dois lados - a C, ou o B quando a C esta no lado B - e a terceira.
 * Ela fica em evidencia enquanto o PTT esta apertado, como o A e o B, sem gravar nada:
 *  - na ida do PTT, a leitura do TX-A/B (0x01E43F92) vira gancho_tx_abc, que devolve ao trecho de
 *    fabrica o lado (1 A, 2 B) ou 0. Para a terceira, poe o lado B em evidencia com a troca de fabrica,
 *    se o A estava nela (TX_TROCOU), e marca b[0x15384] com TX_TERCEIRA (se a marca ja estava posta, o
 *    PTT ja trocou de lado e ganha, como no firmware de fabrica);
 *  - o inicio da TX (0x01E41022) desarma a dupla escuta no lado em evidencia (0x01E3FDE4, em
 *    0x01E410EA) e so depois le a frequencia de TX decodificada (b[0x15341], escrita pela decodificacao
 *    0x01E38DE6). Ali gancho_tx_desarma poe a terceira na corrente do lado B e a decodifica, como o
 *    carregador de canal faz com o lado em evidencia (0x01E3A650); poe a potencia dela no lado B - a TX
 *    tira a potencia de b[0x15E30 + lado em evidencia] (0x01E475A2), e a de cada canal ou VFO e o nibble
 *    baixo do byte 14 do canal cru ou do 16 do VFO (0x01E3A6D0, 0x01E3A682) -, guardando a do B na
 *    marca; refaz o registro de tela do B com os dados dela (0x01E39128: frequencias, tons, potencia e o
 *    nome pelo canal corrente) e redesenha. As vagas do lado B (modo, canal, a sombra) ficam como estavam;
 *  - enquanto a marca diz que a terceira esta no lado B (TX_VIRTUAL), a tela mostra o que o PTT pos ali:
 *    watch_tela() inverte o c_no_b e da o canal dela para o cartao, e terceira() passa a ser a que saiu
 *    do lado B, lida das vagas dele;
 *  - as duas destrocas (0x01E411AE e 0x01E473EE) passam por gancho_tx_volta: zera a marca, devolve a
 *    potencia, a sombra do B a corrente (0x01E3FD9E) e o registro de tela do B, e volta ao A com a troca de
 *    fabrica se foi ela que o tirou de evidencia; sem a marca, a destroca de fabrica. O firmware zera a
 *    marca logo depois delas, e a partida tambem (0x01E56B3A). */
#define TX_ABC         (*(volatile u8 *)0x00014B7F)
DADO_NA_LIVRE const char titulo_txabc[] = "TX-A/B/C";
DADO_NA_LIVRE const char *const txabc_opcoes[4] = {"OFF", "A", "B", "C"};

NA_LIVRE_COD unsigned gancho_tx_abc(void)
{
    unsigned t = TX_ABC;                     /* 0 OFF, 1 A, 2 B, 3 C */
    struct vigia v;

    le_vigia(&v);
    if (t > 3 || (t == 3 && v.modo != WATCH_TRIPLE))
        return 0;                            /* fora do TRIPLE nao ha C */
    if (v.modo != WATCH_TRIPLE || t < 2)
        return t;
    if ((t == 3) == (v.c_no_b != 0))         /* a letra esta no lado B */
        return 2;
    if (TX_TROCA != 0 || terceira_de(&v) == 0xFFFF)
        return 0;
    if (LADO_B == 0) {                       /* o lado B em evidencia, pela troca de fabrica */
        TROCA_AB(0);
        TX_TROCA = TX_TERCEIRA | TX_TROCOU;
    } else {
        TX_TROCA = TX_TERCEIRA;
    }
    return 0;
}

NA_LIVRE_COD void gancho_tx_desarma(void)
{
    u32 t;
    unsigned potencia;

    PARA_DW();
    if ((TX_TROCA & (TX_TERCEIRA | TX_NO_B)) != TX_TERCEIRA || (t = terceira()) == 0xFFFF)
        return;
    potencia = POTENCIA_LADO[LADO_B] & 0x0F;
    le_terceira(t);
    DECODIFICA();                            /* no lado em evidencia, o B, como o carregador de canal */
    POTENCIA_LADO[LADO_B] = (CORRENTE_MODO ? CORRENTE_CRU[14] : CORRENTE_VFO[16]) & 0x0F;
    REGISTRO_TELA();
    TX_TROCA |= TX_NO_B | potencia;
    if (MODO_TELA == 0)
        TELA_INTEIRA();
}

NA_LIVRE_COD void gancho_tx_volta(unsigned r0)
{
    unsigned m = TX_TROCA;

    if (!(m & TX_TERCEIRA)) {
        TROCA_AB(r0);
        return;
    }
    TX_TROCA = 0;                            /* a tela volta a mostrar o que esta nos lados */
    if (m & TX_NO_B) {
        POTENCIA_LADO[LADO_B] = m & 0x0F;
        VOLTA_DW();                          /* a sombra do B de volta a corrente, decodificada */
        REGISTRO_TELA();
    }
    if (m & TX_TROCOU)
        TROCA_AB(0);                         /* de volta ao A (a troca redesenha) */
    else if ((m & TX_NO_B) && MODO_TELA == 0)
        TELA_INTEIRA();
}

/* ---------- scramble: 8 frequencias de inversao, de 2700 a 3400 Hz ----------
 * O scramble do chip e inversao de espectro (bit 1 do reg 0x31), com a frequencia de inversao no
 * reg 0x71. De fabrica ela sai de uma tabela de duas entradas (0x01E68B18), escolhida por "valor
 * == 1": o ON (valor 1) inverte em 3400 Hz. Medido pelo dono com um tom de 1 kHz: este radio
 * inverte em 3400 Hz, e o Baofeng UV-17L, no modo 3, em 2900. Desde a v25 a rotina que aplica
 * (0x01E408E2) le a frequencia de scramble_freqs, pelo valor gravado (remendo de 0x01E4092E no
 * montar.py, com o indice mascarado em 4 bits), e o menu vai de 2700 a 3400 Hz, de 100 em 100.
 *
 * Os valores que ja existiam ficam: 1 = 3400 Hz (o ON de fabrica, o que fala com os outros BJ-8300)
 * e 2 = 2900 Hz (v24). Os novos vao de 3 a 8, e o menu mostra em ordem crescente: o get e o set
 * traduzem posicao e valor. A troca de canal le o nibble inteiro (0x01E3900C: b[0x15E2C] = byte 14
 * >> 4), e o set grava pelo de fabrica, que poe o valor no nibble alto do byte 14 do canal (ou 16
 * do VFO) e em b[0x15E2C]. Valor que nenhum menu grava (9 a 15) inverte em 3400 Hz e aparece como
 * 3400 Hz: menu e radio concordam sempre. */
#define HZ(f)  ((u16)((f) * 10.32444 + 0.5))     /* unidade do reg 0x71, a conta do roger */
const u16 scramble_freqs[16] = {
    HZ(3400), HZ(3400), HZ(2900), HZ(2700), HZ(2800), HZ(3000), HZ(3100), HZ(3200),
    HZ(3300), HZ(3400), HZ(3400), HZ(3400), HZ(3400), HZ(3400), HZ(3400), HZ(3400)};
const u8 scramble_valor[9] = {0, 3, 4, 2, 5, 6, 7, 8, 1};                  /* posicao -> valor */
const u8 scramble_posicao[16] = {0, 8, 3, 1, 2, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8};  /* e a volta */
const char *const scramble_opcoes[9] = {"OFF", "2700 Hz", "2800 Hz", "2900 Hz", "3000 Hz",
                                        "3100 Hz", "3200 Hz", "3300 Hz", "3400 Hz"};

u16 scramble_le(void)
{
    return scramble_posicao[SCRAMBLE & 15];
}

void scramble_grava(void)
{
    unsigned posicao = VALOR;

    VALOR = scramble_valor[posicao];
    SCRAMBLE_SET();
    VALOR = posicao;               /* o motor de menus segue com a posicao */
}

/* No lugar do laco dos tons do BEEP. Com o BEEP de fabrica, faz exatamente o que o laco fazia:
 * os dois pares da tabela, de tras para frente, ESPERA_2MS(40) = 80 ms cada. */
void gancho_roger_tons(void)
{
    unsigned v = ROGER;
    const u16 *t;

    if (v == 3 || v == 4) {
        for (t = v == 3 ? beep2 : beep3; *t; t += 2) {
            TOM(t[0], 0);
            ESPERA_2MS(t[1]);
        }
        return;
    }
    TOM(BEEP_FABRICA[2], BEEP_FABRICA[3]);
    ESPERA_2MS(40);
    TOM(BEEP_FABRICA[0], BEEP_FABRICA[1]);
    ESPERA_2MS(40);
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
 * Gravacao estendida **so na area dos bancos, no WATCH e nos VFOs da C** (0xC1000..0xD3FFF; ate a
 * v45, 0xD1FFF; da v46 a v48, 0xD2FFF),
 * apagando o setor quando o bloco comeca nele; qualquer outro endereco e ignorado sem escrever nada.
 * Quem grava banco pelo cabo tem que desligar e ligar o radio depois: o filtro e montado na
 * partida.
 *
 * Leitura com o bit 22 do endereco ligado (0x400000 em diante, que o chip de dados de 1 MB nao tem):
 * le a memoria do processador a partir de 0x01E00000 - o que o firmware enxerga de fato da flash
 * interna (e da RAM em 0x02000000). So leitura; e um diagnostico (v31).
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
        if (!grava) {
            if (endereco & 0x400000)
                COPIA_MEM((void *)(p + 4), (const void *)(endereco + 0x01A00000), 0x40);  /* 0x01E00000 + (endereco - 0x400000) */
            else
                LE_FLASH(endereco, (void *)(p + 4), 0x40);
        }
        /* A faixa 0xC1000..0xD3FFF e exclusivamente nossa - o mapa do chip lido pelo cabo em
         * 2026-10-24 mostra que nada mais mora ali, e o chip de 1 MB nao espelha nesse endereco.
         * Quem grava por aqui e o PC mandando registro de banco; o caminho do menu continua com a
         * trava do setor_livre. */
        else if (setor - (SETOR_CAB >> 12) < NUM_BANCOS + 3u) {   /* o cabecalho, os bancos, o WATCH e os VFOs da C */
            if (endereco == (u32)setor << 12)   /* bloco que abre o setor: apaga antes */
                APAGA_SETOR(endereco);
            GRAVA_FLASH(endereco, (void *)(p + 4), 0x40);
        }
        return;
    }
    DADOS_CABO(grava);
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
    /* Ligado: um byte 0x01 no fim (LIGADO no telas.c), que a tela B nao escreve e usa para pintar a
     * linha de ambar, no lugar do " ON" de ate a v37. Quem mostra a lista e so a tela B: a piscada de
     * fabrica do menu nao chega mais ao LCD (gancho_menu_pisca, v38). */
    if ((c.ativos >> n) & 1)
        acrescenta(TEXTO, "\x01");
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
    revalida_lados();
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

/* O nome do editor no banco escolhido. Separada do nome_grava (v38): a estrutura do banco sai da
 * pilha antes da saida do editor, que redesenha a tela inteira - a tela B desde a v35, mais funda que
 * a de fabrica. Junto, o set passava do que o firmware de fabrica usa de pilha no mesmo lugar. */
__attribute__((noinline)) static void grava_nome_do_editor(void)
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
}

void nome_grava(void)
{
    grava_nome_do_editor();
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
        acrescenta(TEXTO, "\x01");  /* esta neste banco: a tela B pinta de ambar (LIGADO, telas.c) */
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
    revalida_lados();
}

/* ---------- o carimbo do fim da imagem (v47) ----------
 * O CRC16 do .text (o codigo no fim da imagem, 0x01E96124 em diante), que o build.sh calcula numa
 * primeira ligacao e passa aqui na segunda. O carregador de atualizacao so regrava o que tem CRC novo
 * (v30, e a v34 gravada por cima dela mesma parava nos 5%), e nenhum CRC de dentro da imagem cobre o
 * fim: o do app_area_head vai so ate o fim do app.bin. A v46 mudou 1 byte so ali, e o radio com a v45
 * a recusou. O carimbo mora no .livre, dentro do app.bin (o bancos.ld o poe por ultimo, *(.carimbo)):
 * mudar o fim muda o app.bin, e o carregador grava. O conferir.py confere (2i). */
#ifndef CARIMBO_FIM
#define CARIMBO_FIM 0
#endif
__attribute__((section(".carimbo"), used)) const u16 carimbo_fim = CARIMBO_FIM;
