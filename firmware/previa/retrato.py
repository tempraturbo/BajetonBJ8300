"""Gera o firmware/src/retrato.h: as faixas da RAM que o codigo da tela (src/tela.c e telas.c) le, que o controle
remoto manda ao PC (o "retrato" da tela, v162) e com que o PC desenha a tela com o mesmo codigo.

Como: compila a previa (previa.c e maquina.c) numa pasta temporaria com a macro RAM() trocada por uma que anota cada byte lido,
roda todas as cenas e junta (1) o que elas leram, (2) o que o codigo cita com endereco constante e nenhuma cena usou
(RAM(tipo, 0x...), com o tamanho do tipo) e (3) as tabelas indexadas, no tamanho inteiro (TABELAS, abaixo, conferidas
no codigo), com o que o PC usa para escolher o desenho. Fora o buffer de video e os registros da janela, que sao da propria tela. As faixas a ate VAO bytes uma da
outra se juntam (menos faixas na tabela, poucos bytes a mais no retrato).

Precisa do zig (pip install ziglang), como a previa. Uso: python -I firmware/previa/retrato.py
"""
import hashlib
import os
import re
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
FW = os.path.dirname(AQUI)
SRC = os.path.join(FW, 'src')
SAIDA = os.path.join(SRC, 'retrato.h')
VAO = 8
BASE = 0x7000               # os enderecos vao na tabela como 16 bits a partir daqui
TAM = {'u8': 1, 'u16': 2, 'u32': 4, 'char': 1}
VIDEO = (0x10190, 0x10190 + 10240)
JANELA = (0x14608, 0x14612)
# as tabelas lidas com indice, inteiras (endereco, tamanho, o que e)
TABELAS = [
    (0x14A3A, 37, 'o texto em edicao e os digitos digitados (DIGITOS)'),
    (0x14BDB, 2, 'a zona dos dois lados (ZONA)'),
    (0x14DC4, 62, 'os registros da tela dos lados A e B (31 B cada)'),
    (0x15494, 4 * 27, 'o titulo e as 3 linhas do motor de menu (MENU_TITULO, MENU_LINHAS)'),
    (0x12A22, 75, 'as 15 memorias SSB do radio, 5 B cada (RADIO_SSB_MEMS)'),
    # o que a tela nao le e o PC usa para escolher o desenho (o que o firmware passa por parametro)
    (0x148D0, 1, 'o estado do scan QT, [0x14680 + 592]: 2 rodando, 1 parado (0x01E46FE6 e 0x01E474FC)'),
]
# os valores que a previa imita e o firmware calcula, na ordem em que o retrato os manda
VALORES = [('watch_tela', 4), ('terceira', 4), ('freq_terceira', 4), ('watch_le_tudo', 4), ('menu_tabela', 4),
           ('c_no_lugar', 1), ('outro_recebendo', 1), ('nr_le', 1), ('porta_le', 1), ('em_canal', 1),
           ('lista_item', 1)]
# o que a tela le do chip de dados, na ordem do retrato: o comeco e o nome do canal da terceira, e o nome da memoria
# do radio (remoto.c)
FLASH = [('canal da terceira, +0', 4), ('canal da terceira, +20 (o nome)', 12), ('nome da memoria do radio', 14)]


def le(caminho):
    return open(caminho, encoding='utf-8', errors='surrogateescape').read()


def grava(caminho, texto):
    open(caminho, 'w', encoding='utf-8', errors='surrogateescape', newline='\n').write(texto)


def lidos_nas_cenas(pasta):
    tela = le(os.path.join(SRC, 'tela.c'))
    v = '#define RAM(tipo, endereco)       (*(volatile tipo *)(memoria + (endereco)))'
    if tela.count(v) != 1:
        sys.exit('o tela.c mudou a macro RAM() da previa: ajuste o retrato.py')
    grava(os.path.join(pasta, 'tela.c'), tela.replace(v, (
        'u8 *previa_registra(unsigned endereco, unsigned n);\n'
        '#define RAM(tipo, endereco)       (*(volatile tipo *)previa_registra((endereco), sizeof(tipo)))')))
    grava(os.path.join(pasta, 'telas.c'), le(os.path.join(SRC, 'telas.c')))
    maquina = le(os.path.join(AQUI, 'maquina.c'))
    if maquina.count('#include "../src/tela.c"') != 1:
        sys.exit('o maquina.c mudou o include do tela.c: ajuste o retrato.py')
    maquina = maquina.replace('#include "../src/tela.c"', '''static u8 lido[0x20000];
u8 *previa_registra(unsigned e, unsigned n)
{
    unsigned i;

    for (i = 0; i < n && e + i < sizeof lido; i++)
        lido[e + i] = 1;
    return memoria + e;
}

static void despeja(void)
{
    unsigned a;
    FILE *f = fopen("lidos.bin", "wb");

    for (a = 0; a < sizeof lido; a++)
        fputc(lido[a], f);
    fclose(f);
}

#include "tela.c"''')
    grava(os.path.join(pasta, 'maquina.c'), maquina)
    previa = le(os.path.join(AQUI, 'previa.c'))
    m = re.search(r'int main\(int argc, char \*\*argv\)\s*\{', previa)
    previa = previa[:m.end()] + '\n    atexit(despeja);' + previa[m.end():]
    grava(os.path.join(pasta, 'previa.c'), previa)
    exe = os.path.join(pasta, 'previa_retrato.exe')
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O1', '-w', '-DPREVIA', '-I', os.path.join(FW, 'build'),
                    '-I', AQUI, '-o', exe, os.path.join(pasta, 'previa.c')], check=True)
    telas = os.path.join(pasta, 'telas')
    os.makedirs(telas)
    subprocess.run([exe, telas], check=True, cwd=pasta, stdout=subprocess.DEVNULL)
    lido = open(os.path.join(pasta, 'lidos.bin'), 'rb').read()
    return {a for a, x in enumerate(lido) if x}, len(os.listdir(telas))


def main():
    with tempfile.TemporaryDirectory() as pasta:
        cenas, n_cenas = lidos_nas_cenas(pasta)
    fonte = le(os.path.join(SRC, 'tela.c')) + le(os.path.join(SRC, 'telas.c'))
    codigo = set()
    for m in re.finditer(r'RAM\((u8|u16|u32|char)\s*,\s*(0x[0-9A-Fa-f]+)u?\s*\)', fonte):
        a = int(m.group(2), 16)
        codigo.update(range(a, a + TAM[m.group(1)]))
    for a, n, _ in TABELAS:
        codigo.update(range(a, a + n))
    todos = {a for a in cenas | codigo
             if 0x1000 <= a < 0x20000 and not VIDEO[0] <= a < VIDEO[1] and not JANELA[0] <= a < JANELA[1]}
    faixas = []
    for b in sorted(todos):
        if faixas and b - (faixas[-1][0] + faixas[-1][1]) <= VAO:
            faixas[-1][1] = b - faixas[-1][0] + 1
        else:
            faixas.append([b, 1])
    if any(a < BASE or a + n - BASE > 0xFFFF or n > 255 for a, n in faixas):
        sys.exit('uma faixa nao cabe na tabela (16 bits a partir de 0x%X, ate 255 B)' % BASE)
    ram = sum(n for _, n in faixas)
    valores = sum(n for _, n in VALORES)
    flash = sum(n for _, n in FLASH)
    tabela = ''.join('    {0x%02X, 0x%02X, %3d},   /* 0x%05X */\n' % ((a - BASE) & 255, (a - BASE) >> 8, n, a)
                     for a, n in faixas)
    # a versao muda com o formato e com as tabelas do menu, que o PC tem de ter iguais (o menu_grupos.h da previa)
    grupos = os.path.join(FW, 'build', 'menu_grupos.h')
    versao = hashlib.sha256((tabela + repr(VALORES) + repr(FLASH)).encode()
                            + open(grupos, 'rb').read()).digest()[0]
    texto = f'''/* O retrato da tela (v162), gerado pelo firmware/previa/retrato.py: nao edite a mao.

   O que o controle remoto manda ao PC para ele desenhar a tela com o mesmo codigo (src/tela.c e telas.c; o formato
   no alto do remoto.c): os valores que a tela pede ao bancos.c e ao mensagem.c (a previa os imita) e, desde a v163,
   onde o menu_tabela esta, na ordem de RETRATO_VALORES, cada um com o byte de baixo primeiro; o que ela le do chip de dados (RETRATO_FLASH); e os bytes
   das faixas da RAM, na ordem da tabela. As faixas sao as que {n_cenas} cenas da previa leram, as que o codigo cita
   com endereco constante, as tabelas lidas com indice, inteiras, e o que o PC usa para escolher o desenho; juntas a
   ate {VAO} bytes uma da outra. O PC confere a RETRATO_VERSAO, que muda com a tabela e com as do menu. */
#ifndef RETRATO_H
#define RETRATO_H

#define RETRATO_VERSAO  {versao}u
#define RETRATO_BASE    0x{BASE:X}u
#define RETRATO_FAIXAS  {len(faixas)}u
#define RETRATO_RAM     {ram}u
#define RETRATO_VALORES {valores}u     /* {", ".join(f"{n} ({t})" for n, t in VALORES)} */
#define RETRATO_FLASH   {flash}u     /* {", ".join(f"{n} ({t})" for t, n in FLASH)} */
#define RETRATO_TAMANHO ({valores}u + {flash}u + {ram}u)   /* sem o cabecalho (5 B) e as somas (2) */

/* (endereco - RETRATO_BASE, o byte de baixo e o de cima) e quantos bytes */
static const unsigned char retrato_faixas[{len(faixas)}][3] = {{
{tabela}}};

#endif
'''
    grava(SAIDA, texto)
    print(f'{SAIDA}: {len(faixas)} faixas, {ram} B da RAM + {valores} B de valores + {flash} B da flash = '
          f'{ram + valores + flash} B, versao {versao} ({n_cenas} cenas)')


if __name__ == '__main__':
    main()
