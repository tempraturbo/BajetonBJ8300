"""Lista os textos em ingles que o menu do BJ-8300 mostra, com endereco, tamanho e folga.

Uso: python -I textos.py <app.bin> [--csv]

Serve para planejar a revisao dos textos: trocar uma cadeia **no lugar** so pode usar ate o
tamanho original (encurtar pode, e so terminar em zero antes; esticar passaria por cima da cadeia
seguinte). A coluna "folga" diz quantos bytes zerados vem depois do terminador - quando ha folga,
da para crescer esse tanto sem mexer em ponteiro nenhum.

Os nomes dos itens ficam em +0xB (ingles) e +0xF (chines, em GBK - nao mexemos). As listas de
opcoes sao vetores de ponteiros no trecho que o app copia para a RAM (app.bin + 0x6AC0C, rodando
em 0x0), e as cadeias em si ficam na flash; a quantidade de opcoes e o campo "max" do item.
"""
import struct
import sys

BASE = 0x01E00100
SEG2_OFF, SEG2_LEN = 0x6AC0C, 0xD4A8
TABELA, ITENS, TAM_ITEM = 0x01E62F98, 53, 0x1C

app = open(sys.argv[1], 'rb').read()
csv = '--csv' in sys.argv


def offset(addr):
    if BASE <= addr < BASE + len(app):
        return addr - BASE
    if addr < SEG2_LEN:
        return SEG2_OFF + addr
    return None


def cadeia(addr):
    """(texto, tamanho, folga) ou None. Folga = zeros depois do terminador."""
    o = offset(addr)
    if o is None:
        return None
    fim = app.find(b'\0', o)
    if fim < 0 or fim - o > 40:
        return None
    bruto = app[o:fim]
    if not all(32 <= b < 127 for b in bruto):
        return None
    folga = 0
    while fim + 1 + folga < len(app) and app[fim + 1 + folga] == 0:
        folga += 1
    return bruto.decode('ascii'), len(bruto), folga


def campo(item, desloc):
    return struct.unpack_from('<I', app, item + desloc)[0]


linhas = []
for i in range(ITENS):
    item = TABELA - BASE + i * TAM_ITEM
    flags = app[item]
    maximo = struct.unpack_from('<H', app, item + 1)[0]
    nome = cadeia(campo(item, 0xB))
    if nome:
        linhas.append((i, 'nome', campo(item, 0xB)) + nome)
    opcoes = campo(item, 0x13)
    if flags & 0x80 or opcoes >= SEG2_LEN:      # funcao de exibicao, nao lista de opcoes
        continue
    o = offset(opcoes)
    if o is None:
        continue
    for k in range(maximo):
        p = struct.unpack_from('<I', app, o + 4 * k)[0]
        c = cadeia(p)
        if c:
            linhas.append((i, f'op{k}', p) + c)

if csv:
    print('item;tipo;endereco;texto;tam;folga')
    for i, tipo, p, txt, tam, folga in linhas:
        print(f'{i};{tipo};{p:#x};{txt};{tam};{folga}')
else:
    nome_do_item = {i: t for i, tipo, p, t, _, _ in linhas if tipo == 'nome'}
    atual = None
    for i, tipo, p, txt, tam, folga in linhas:
        if i != atual:
            atual = i
            print(f"\n{i:2d} {nome_do_item.get(i, '?')}")
        print(f'   {tipo:5s} {p:#9x} tam={tam:2d} folga={folga:2d}  "{txt}"')
    print(f'\ntotal de cadeias: {len(linhas)}')
