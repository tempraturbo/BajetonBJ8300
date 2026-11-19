/* O retrato da tela (v162), gerado pelo firmware/previa/retrato.py: nao edite a mao.

   O que o controle remoto manda ao PC para ele desenhar a tela com o mesmo codigo (src/tela.c e telas.c; o formato
   no alto do remoto.c): os valores que a tela pede ao bancos.c e ao mensagem.c (a previa os imita) e, desde a v163,
   onde o menu_tabela esta, na ordem de RETRATO_VALORES, cada um com o byte de baixo primeiro; o que ela le do chip de dados (RETRATO_FLASH); e os bytes
   das faixas da RAM, na ordem da tabela. As faixas sao as que 111 cenas da previa leram, as que o codigo cita
   com endereco constante, as tabelas lidas com indice, inteiras, e o que o PC usa para escolher o desenho; juntas a
   ate 8 bytes uma da outra. O PC confere a RETRATO_VERSAO, que muda com a tabela e com as do menu. */
#ifndef RETRATO_H
#define RETRATO_H

#define RETRATO_VERSAO  141u
#define RETRATO_BASE    0x7000u
#define RETRATO_FAIXAS  23u
#define RETRATO_RAM     453u
#define RETRATO_VALORES 26u     /* watch_tela (4), terceira (4), freq_terceira (4), watch_le_tudo (4), menu_tabela (4), c_no_lugar (1), outro_recebendo (1), nr_le (1), porta_le (1), em_canal (1), lista_item (1) */
#define RETRATO_FLASH   30u     /* 4 (canal da terceira, +0), 12 (canal da terceira, +20 (o nome)), 14 (nome da memoria do radio) */
#define RETRATO_TAMANHO (26u + 30u + 453u)   /* sem o cabecalho (5 B) e as somas (2) */

/* (endereco - RETRATO_BASE, o byte de baixo e o de cima) e quantos bytes */
static const unsigned char retrato_faixas[23][3] = {
    {0x31, 0x00,   1},   /* 0x07031 */
    {0xD4, 0xB9,   1},   /* 0x129D4 */
    {0xF8, 0xB9,   2},   /* 0x129F8 */
    {0x1A, 0xBA,  87},   /* 0x12A1A */
    {0x85, 0xD6,   2},   /* 0x14685 */
    {0xD0, 0xD8,   1},   /* 0x148D0 */
    {0xBC, 0xD9,   8},   /* 0x149BC */
    {0x35, 0xDA,  42},   /* 0x14A35 */
    {0x6E, 0xDB,  13},   /* 0x14B6E */
    {0x86, 0xDB,   4},   /* 0x14B86 */
    {0xD4, 0xDB,  45},   /* 0x14BD4 */
    {0x0C, 0xDC,   5},   /* 0x14C0C */
    {0xA5, 0xDC,   3},   /* 0x14CA5 */
    {0xB4, 0xDC,  10},   /* 0x14CB4 */
    {0xCE, 0xDC,  10},   /* 0x14CCE */
    {0xC4, 0xDD,  62},   /* 0x14DC4 */
    {0x3D, 0xE3,   1},   /* 0x1533D */
    {0x67, 0xE3,  21},   /* 0x15367 */
    {0x86, 0xE3,   1},   /* 0x15386 */
    {0x94, 0xE4, 108},   /* 0x15494 */
    {0x04, 0xEE,  14},   /* 0x15E04 */
    {0x28, 0xEE,  10},   /* 0x15E28 */
    {0x3C, 0xEE,   2},   /* 0x15E3C */
};

#endif
