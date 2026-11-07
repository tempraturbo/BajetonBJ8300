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
}

SECTIONS
{
    .text : {
        *(.fim*)
        *(EXCLUDE_FILE(*tela.o *mensagem.o) .text*)
        *(EXCLUDE_FILE(*tela.o *mensagem.o) .rodata*)
    } > novo

    .livre : {
        *tela.o(.text* .rodata*)
        *mensagem.o(.text* .rodata*)
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
    .mortos2 : { *(.mortos2*) } > mortos2
    .mortos3 : { *(.mortos3*) } > mortos3
    .mortos4 : { *(.mortos4*) } > mortos4
    .mortos5 : { *(.mortos5*) } > mortos5
    .mortos6 : { *(.mortos6*) } > mortos6
    .mortos7 : { *(.mortos7*) } > mortos7
    .mortos8 : { *(.mortos8*) } > mortos8
    .mortos9 : { *(.mortos9*) } > mortos9

    /DISCARD/ : { *(.comment) *(.note*) *(.debug*) }
}
