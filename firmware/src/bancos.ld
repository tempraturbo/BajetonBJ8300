/* Coloca o codigo dos bancos no espaco livre do fim da imagem de flash.
 *
 * Esse espaco existe nas 6 variantes de imagem e, em todas, comeca no mesmo endereco de execucao
 * (0x01E96121; o codigo comeca no primeiro byte PAR, 0x01E96122), porque o deslocamento do app compensa a diferenca de tamanho entre elas.
 * A menor das imagens oferece 3295 bytes dai ate o fim. A tabela de menus (54 itens x 28 B = 1512)
 * entra logo depois do codigo, entao o codigo pode ir ate 1782 bytes - e esse o LENGTH aqui.
 * O inicio foi arredondado para 0x01E96140 so para ficar alinhado.
 */
MEMORY
{
    novo (rx) : ORIGIN = 0x01E96122, LENGTH = 1782
}

SECTIONS
{
    .text : {
        *(.text*)
        *(.rodata*)
    } > novo

    /DISCARD/ : { *(.comment) *(.note*) *(.debug*) }
}
