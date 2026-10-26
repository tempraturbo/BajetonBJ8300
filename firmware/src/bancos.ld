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
 * O G.729 (v36): o decodificador das vozes .wtg, 0x01E257E0..0x01E278C8 (8424 B, dentro do
 * app.bin), nunca roda desde que as vozes sairam: ele so e escolhido para um arquivo que nao e .sin,
 * aberto pelo tocador de sons, e nada de fora aponta para a faixa alem do registro do decodificador,
 * cujo tipo o montar.py zera (prova em docs/engenharia-reversa.md, secao 16). O tela.o inteiro (a
 * tela principal B) mora ali. Ele nao pode ter .data nem .bss (nao ha RAM nossa): o conferir.py
 * confere. */
MEMORY
{
    novo (rx) : ORIGIN = 0x01E96124, LENGTH = 3260
    g729 (rx) : ORIGIN = 0x01E257E0, LENGTH = 8424
    vozes (rx) : ORIGIN = 0x01E797A0, LENGTH = 0x1BF50
}

SECTIONS
{
    .text : {
        *(EXCLUDE_FILE(*tela.o) .text*)
        *(EXCLUDE_FILE(*tela.o) .rodata*)
    } > novo

    .g729 : {
        *tela.o(.text* .rodata*)
    } > g729

    .vozes : {
        *(.vozes*)
    } > vozes

    /DISCARD/ : { *(.comment) *(.note*) *(.debug*) }
}
