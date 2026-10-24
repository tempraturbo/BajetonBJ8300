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
 */
MEMORY
{
    novo (rx) : ORIGIN = 0x01E96124, LENGTH = 3292
}

SECTIONS
{
    .text : {
        *(.text*)
        *(.rodata*)
    } > novo

    /DISCARD/ : { *(.comment) *(.note*) *(.debug*) }
}
