/* Coloca o codigo dos bancos no espaco livre do fim da imagem de flash.
 *
 * Esse espaco existe nas 6 variantes de imagem e, em todas, comeca no mesmo endereco de execucao
 * (0x01E96121), porque o deslocamento do app compensa a diferenca de tamanho entre elas. A menor
 * das imagens vai ate 0x01E96DFF. Ate a v18 a tabela de menus (1512 B) tambem morava aqui; desde a
 * v19 ela e gravada no lugar da original, dentro do app.bin (ver TABELA no montar.py).
 *
 * O codigo comeca no primeiro endereco MULTIPLO DE 4, 0x01E96124, e vai ate o fim: 3292 bytes.
 * PAGO CARO (v20, bootloop): ate a v19 ele comecava em 0x01E96122, o primeiro par, mas o linker
 * alinha a secao pelo maior alinhamento que encontra dentro dela - e a v20 trouxe tabelas de
 * ponteiros (alinhamento 4), que empurraram o .text para 0x01E96124 enquanto o montador gravava em
 * 0x01E96122. Tudo ficou 2 bytes fora do lugar e o primeiro gancho da partida pulava para o meio
 * de uma instrucao. Agora o montar.py e o conferir.py leem o endereco do .text no ELF e recusam
 * montar se ele nao for o ENDERECO_CODIGO.
 *
 * E para ANTES dos ultimos 32 bytes da menor imagem (0x01E96DE0 = 0x01E96E00 - 32): 3260 bytes. Ali
 * o uboot guarda o marcador BOOT_START_FIRST (32 bytes antes do fim da imagem, documentado para a
 * familia anterior por kagaimiq.github.io/jielie). Nas imagens flash2/4/6 a v26 e a v27 terminavam
 * dentro deles; nada aconteceu no radio do dono, mas o espaco nao e nosso.
 *
 * O tamanho da imagem NAO cresce: a v28 cresceu as 6 imagens 8 KB (sem mexer na particao PRCT, que
 * a ferramenta da JieLi sempre grava com o tamanho da imagem) e o radio entrou em bootloop logo
 * depois do logo, recuperado pelo PTT2 + atalho. Causa nao provada; ver docs/engenharia-reversa.md.
 */
/* As vozes (v30): os dados dos arquivos de voz C0..C49 e E0..E67, no diretorio "tone" logo depois
 * do app.bin, deixaram de ser tocados (tres remendos, ver VOZES_REMENDOS no montar.py) e o espaco
 * e nosso: do C0, alinhado a 4, ate o fim do E67. Mesmo endereco nas 6 imagens; o tamanho delas e
 * a tabela de particoes nao mudam. Desde a v33 o codigo e as tabelas do espectro moram aqui
 * (.vozes.text e .vozes.rodata, no bancos.c).
 *
 * PAGO CARO (v35, recusada pelo carregador): o carregador de atualizacao NAO grava uma imagem que
 * mude os arquivos de voz C0..C11 - desiste antes de escrever qualquer byte, provado lendo o radio
 * pelo cabo. Mudar so o C0 e o C1 (o espectro, v33 e v34) passa, e mudar o app.bin sempre passou.
 * Por isso o .vozes fica so com o espectro, dentro do C0 e do C1.
 *
 * Os decodificadores G.729 e MP3 (v36 e v37): o das vozes .wtg e o de MP3, colados, 0x01E257E0..
 * 0x01E2EFC4 (38884 B, dentro do app.bin). Nunca rodam: o tocador de sons so os escolhe para um
 * arquivo .wtg ou .mp3, as vozes sairam e MP3 nao ha nenhum, e nada de fora aponta para a faixa
 * alem do registro dos decodificadores, cujo tipo o montar.py zera nos dois (prova em
 * docs/engenharia-reversa.md, secao 16). O tela.o inteiro (a tela principal B) mora ali. Ele nao
 * pode ter .data nem .bss (nao ha RAM nossa): o conferir.py confere. Desde a v65 o mensagem.o (o
 * Morse e o APRS) mora ali tambem, com a mesma regra.
 *
 * Os caminhos das vozes (v138): os nomes "mnt/sdfile/res/tone/C0.*" a "E67.*" (0x01E5AB5E..0x01E5B6E0), mortos
 * desde a v30 (a fila de sons sai antes de ler a tabela de caminhos, e o C16 do main nao toca). De 0x01E5AB60
 * (alinhado) ate antes do 0x01E5B6E0 (o caminho do B0, vivo, comeca logo depois) mora o .caminhos: so codigo e
 * dado que o NOSSO codigo usa - nenhuma rotina que o firmware chama -, sem .data nem .bss. O unico valor vivo
 * que apontava para dentro (o "*" do editor no modo 123) passa para o "*" do B0. Prova em
 * docs/engenharia-reversa.md, secao 29; o conferir.py a refaz (2j). */
MEMORY
{
    novo (rx) : ORIGIN = 0x01E96124, LENGTH = 3260
    livre (rx) : ORIGIN = 0x01E257E0, LENGTH = 38884
    vozes (rx) : ORIGIN = 0x01E797A0, LENGTH = 0x1BF50
    caminhos (rx) : ORIGIN = 0x01E5AB60, LENGTH = 2944
    reserva (rx) : ORIGIN = 0x01E62F98, LENGTH = 1512   /* a tabela de menu de fabrica, morta desde a v43 (v140) */
    sons (rx) : ORIGIN = 0x01E5E8FC, LENGTH = 472      /* as entradas 0..117 da tabela de caminhos, mortas (v144) */
    /* v152: nomes de item de fabrica sem nenhum ponteiro: a tabela que os lia morreu na v43 (montar.py MORTOS) */
    mortos1 (rx) : ORIGIN = 0x01E5A768, LENGTH = 104
    mortos2 (rx) : ORIGIN = 0x01E5A7E0, LENGTH = 78
    mortos3 (rx) : ORIGIN = 0x01E5AA00, LENGTH = 48
    mortos4 (rx) : ORIGIN = 0x01E5A872, LENGTH = 46
    mortos5 (rx) : ORIGIN = 0x01E5A9BC, LENGTH = 44
    mortos6 (rx) : ORIGIN = 0x01E5A96E, LENGTH = 30
    mortos7 (rx) : ORIGIN = 0x01E5A8AE, LENGTH = 24
    mortos8 (rx) : ORIGIN = 0x01E5A90E, LENGTH = 18
    mortos9 (rx) : ORIGIN = 0x01E5A748, LENGTH = 16
    mortos10 (rx) : ORIGIN = 0x01E5A934, LENGTH = 14
    /* v156: o codigo de fabrica do espectro que os ganchos dele deixaram sem chamador (montar.py MORTOS): o corpo das
     * rotinas trocadas por um goto na entrada, depois dele, e duas rotinas inteiras cujas chamadas foram trocadas.
     * Recebem rotinas do mensagem.c sem mudanca, e o .livre que elas deixam recebe o remoto.c. */
    mortos11 (rx) : ORIGIN = 0x01E4AB2E, LENGTH = 174
    mortos12 (rx) : ORIGIN = 0x01E4ABE0, LENGTH = 148
    mortos13 (rx) : ORIGIN = 0x01E4AFC6, LENGTH = 134
    mortos14 (rx) : ORIGIN = 0x01E4AD7C, LENGTH = 130
    mortos15 (rx) : ORIGIN = 0x01E4AAA8, LENGTH = 130
    mortos16 (rx) : ORIGIN = 0x01E4B050, LENGTH = 80
    mortos17 (rx) : ORIGIN = 0x01E45D9A, LENGTH = 76
    mortos18 (rx) : ORIGIN = 0x01E4AE02, LENGTH = 66
    mortos19 (rx) : ORIGIN = 0x01E4A9A0, LENGTH = 34
    mortos20 (rx) : ORIGIN = 0x01E4A9C6, LENGTH = 32
    /* v164: as rotinas de fabrica dos itens do menu que sairam (o VOICE, o LANGUAGE, o Zone Or Channel) ou que
     * trocaram de rotina (o CH-NAME, o ROGER, o NOISE REDUCTION e o get do SCRAMBLE): so a tabela de fabrica,
     * morta desde a v43, apontava para elas (montar.py MORTOS). Recebem rotinas que so o nosso codigo chama. */
    mortos21 (rx) : ORIGIN = 0x01E3E25A, LENGTH = 210   /* Zone Or Channel: o set */
    mortos22 (rx) : ORIGIN = 0x01E3C6E8, LENGTH = 194   /* CH-NAME: a exibicao e o get */
    mortos23 (rx) : ORIGIN = 0x01E3C7BE, LENGTH = 112   /* CH-NAME: o set */
    mortos24 (rx) : ORIGIN = 0x01E3CDAA, LENGTH = 56    /* LANGUAGE: a exibicao */
    mortos25 (rx) : ORIGIN = 0x01E39928, LENGTH = 46    /* VOICE: o get e o set */
    mortos26 (rx) : ORIGIN = 0x01E3D0E0, LENGTH = 44    /* NOISE REDUCTION: o get e o set */
    mortos27 (rx) : ORIGIN = 0x01E3C91E, LENGTH = 40    /* ROGER: o get e o set */
    mortos28 (rx) : ORIGIN = 0x01E3C9A8, LENGTH = 40    /* LANGUAGE: o get e o set */
    mortos29 (rx) : ORIGIN = 0x01E3D058, LENGTH = 20    /* SCRAMBLE: o get */
    mortos30 (rx) : ORIGIN = 0x01E3E242, LENGTH = 20    /* Zone Or Channel: o get */
    /* v173: os corpos das rotinas de fabrica que desenham o radio, trocadas inteiras (montar.py RADIO_B_FUNCOES). */
    mortos31 (rx) : ORIGIN = 0x01E449C0, LENGTH = 360   /* a frequencia do radio (alinhada a 4: a tabela do radio) */
    mortos32 (rx) : ORIGIN = 0x01E44B2C, LENGTH = 316   /* o nome da memoria do radio */
    mortos33 (rx) : ORIGIN = 0x01E44FB4, LENGTH = 216   /* o radio inteiro */
    mortos34 (rx) : ORIGIN = 0x01E4519C, LENGTH = 208   /* o medidor S do radio */
    mortos35 (rx) : ORIGIN = 0x01E4907C, LENGTH = 264   /* a frequencia digitada no radio */
    /* v174: o MEMCH de fabrica (a exibicao, o titulo e o get) e as tabelas velhas do menu do radio */
    mortos36 (rx) : ORIGIN = 0x01E3D1DA, LENGTH = 170
    mortos37 (rx) : ORIGIN = 0x01E61168, LENGTH = 140
    mortos38 (rx) : ORIGIN = 0x01E6137C, LENGTH = 168
}

SECTIONS
{
    .text : {
        *(.fim*)
        *(EXCLUDE_FILE(*tela.o *mensagem.o *remoto.o) .text*)
        *(EXCLUDE_FILE(*tela.o *mensagem.o *remoto.o) .rodata*)
    } > novo

    .livre : {
        *tela.o(.text* .rodata*)
        *mensagem.o(.text* .rodata*)
        *remoto.o(.text* .rodata*)      /* v156: o controle remoto */
        *(.livre.*)
        *(.carimbo)     /* v47: o CRC16 do .text, por ultimo, para nada mudar de lugar com ele */
    } > livre

    .vozes : {
        *(.vozes*)
    } > vozes

    .caminhos : {
        *(.caminhos*)
    } > caminhos
    .reserva : {
        *(.reserva*)
    } > reserva
    .sons : {
        *(.sons*)
    } > sons
    .mortos10 : { *(.mortos10.*) } > mortos10
    .mortos1 : { *(.mortos1.*) } > mortos1
    .mortos2 : { *(.mortos2.*) } > mortos2      /* v156: com o ponto, que o .mortos20 nao caia aqui */
    .mortos3 : { *(.mortos3.*) } > mortos3
    .mortos4 : { *(.mortos4.*) } > mortos4
    .mortos5 : { *(.mortos5.*) } > mortos5
    .mortos6 : { *(.mortos6.*) } > mortos6
    .mortos7 : { *(.mortos7.*) } > mortos7
    .mortos8 : { *(.mortos8.*) } > mortos8
    .mortos9 : { *(.mortos9.*) } > mortos9
    .mortos11 : { *(.mortos11.*) } > mortos11
    .mortos12 : { *(.mortos12.*) } > mortos12
    .mortos13 : { *(.mortos13.*) } > mortos13
    .mortos14 : { *(.mortos14.*) } > mortos14
    .mortos15 : { *(.mortos15.*) } > mortos15
    .mortos16 : { *(.mortos16.*) } > mortos16
    .mortos17 : { *(.mortos17.*) } > mortos17
    .mortos18 : { *(.mortos18.*) } > mortos18
    .mortos19 : { *(.mortos19.*) } > mortos19
    .mortos20 : { *(.mortos20.*) } > mortos20
    .mortos21 : { *(.mortos21.*) } > mortos21
    .mortos22 : { *(.mortos22.*) } > mortos22
    .mortos23 : { *(.mortos23.*) } > mortos23
    .mortos24 : { *(.mortos24.*) } > mortos24
    .mortos25 : { *(.mortos25.*) } > mortos25
    .mortos26 : { *(.mortos26.*) } > mortos26
    .mortos27 : { *(.mortos27.*) } > mortos27
    .mortos28 : { *(.mortos28.*) } > mortos28
    .mortos29 : { *(.mortos29.*) } > mortos29
    .mortos30 : { *(.mortos30.*) } > mortos30
    .mortos31 : { *(.mortos31.*) } > mortos31
    .mortos32 : { *(.mortos32.*) } > mortos32
    .mortos33 : { *(.mortos33.*) } > mortos33
    .mortos34 : { *(.mortos34.*) } > mortos34
    .mortos35 : { *(.mortos35.*) } > mortos35
    .mortos36 : { *(.mortos36.*) } > mortos36
    .mortos37 : { *(.mortos37.*) } > mortos37
    .mortos38 : { *(.mortos38.*) } > mortos38

    /DISCARD/ : { *(.comment) *(.note*) *(.debug*) }
}
