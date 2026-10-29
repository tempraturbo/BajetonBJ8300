"""Fluxo de uma rotina: o caminho mais curto da entrada ate um ponto, e as saidas que o evitam.

Monta o grafo das instrucoes entre <inicio> e <fim> e responde duas perguntas: por que desvios se
chega ao <alvo> (o caminho mais curto, com os desvios condicionais e as tabelas no caminho), e se ha
retorno ou goto para fora da rotina alcancavel SEM passar pelo primeiro alvo. Foi assim que se provou
que toda volta do laco do sinal (0x01E4622A) passa pela secao de tarefas (0x01E469E0), onde a leitura
do nivel do sinal e chamada (v62).

Uso: python -I scripts/fluxo.py build/app.dis build/app.bin <inicio> <fim> <alvo> [<alvo> ...]
     (enderecos em hexa, sem 0x: python -I scripts/fluxo.py build/app.dis build/app.bin 1e4622a 1e48400 1e469e0)
Arestas: a seguinte (menos depois de goto incondicional, rts e pop de pc), o destino de goto/if goto e
as tabelas tbh/tbb (o tamanho sai do "if (rX > N)" que vem antes). Chamada por registrador nao entra.
"""
import re
import struct
import sys
from collections import deque

dis, binario = sys.argv[1], sys.argv[2]
ini, fim = int(sys.argv[3], 16), int(sys.argv[4], 16)
alvos = [int(a, 16) for a in sys.argv[5:]]
app = open(binario, 'rb').read()
BASE = 0x1E00100
pat = re.compile(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$')
ins = []
for l in open(dis, encoding='utf-8', errors='replace'):
    m = pat.match(l)
    if m:
        a = int(m.group(1), 16)
        if ini <= a < fim:
            ins.append((a, len(m.group(2).split()), m.group(3).strip()))
pos = {a: i for i, (a, _, _) in enumerate(ins)}
suc = {}
limite = None
for i, (a, n, t) in enumerate(ins):
    s = []
    prox = a + n
    m = re.search(r'goto (-?\d+)$', t)
    if re.match(r'if \(r\d+ > (\d+)\)', t):
        limite = int(re.match(r'if \(r\d+ > (\d+)\)', t).group(1))
    if t.startswith('tbh') or t.startswith('tbb'):
        k = (limite or 15) + 1
        base = prox
        for j in range(k):
            v = struct.unpack_from('<h', app, base - BASE + 2 * j)[0] if t.startswith('tbh') else app[base - BASE + j]
            s.append(base + 2 * v)
    elif m:
        s.append(prox + int(m.group(1)))
        if t.startswith('if'):
            s.append(prox)
    elif t.startswith('pc = [sp++]') or t.startswith('{pc') or t == 'rts':
        pass
    else:
        s.append(prox)
    suc[a] = [x for x in s if x in pos]
pred = {}
for a, ss in suc.items():
    for x in ss:
        pred.setdefault(x, []).append(a)


def caminho(de, ate):
    vistos, fila = {de: None}, deque([de])
    while fila:
        a = fila.popleft()
        if a == ate:
            break
        for x in suc.get(a, ()):
            if x not in vistos:
                vistos[x] = a
                fila.append(x)
    if ate not in vistos:
        return None
    c, a = [], ate
    while a is not None:
        c.append(a)
        a = vistos[a]
    return c[::-1]


for alvo in alvos:
    c = caminho(ini, alvo)
    print(f'== {alvo:#x}: ' + ('inalcancavel' if c is None else f'{len(c)} instrucoes'))
    if c:
        for a in c:
            t = ins[pos[a]][2]
            if t.startswith('if') or 'goto' in t or t.startswith('tb') or 'call' in t:
                print(f'   {a:x}: {t}')


if True:
    evita = set(alvos[:1])
    rets = [a for a, n, t in ins if (t.startswith('{pc') or t == 'rts' or t.startswith('pc = [sp++]')) and a >= ini]
    vistos, fila = {ini: None}, deque([ini])
    while fila:
        a = fila.popleft()
        for x in suc.get(a, ()):
            if x not in vistos and x not in evita:
                vistos[x] = a
                fila.append(x)
    saidas = [r for r in rets if r in vistos]
    print(f'== retornos alcancaveis sem passar por {alvos[0]:#x}: {[hex(r) for r in saidas]}')
    for r in saidas[:6]:
        c, a = [], r
        while a is not None:
            c.append(a)
            a = vistos[a]
        print(f'  caminho ate {r:#x}:')
        for a in c[::-1]:
            t = ins[pos[a]][2]
            if t.startswith('if') or t.startswith('tb'):
                print(f'     {a:x}: {t}')
    # saidas por goto para fora da faixa (cauda)
    fora = []
    for a, n, t in ins:
        m = re.search(r'goto (-?\d+)$', t)
        if m and a in vistos:
            d = a + n + int(m.group(1))
            if not (ini <= d < fim):
                fora.append((a, d, t))
    print('== gotos para fora alcancaveis sem passar pelo alvo:', [(hex(a), hex(d)) for a, d, _ in fora])
    for a0, d, _ in fora[:8]:
        c, a = [], a0
        while a is not None:
            c.append(a)
            a = vistos[a]
        print(f'  caminho ate {a0:#x} (vai a {d:#x}):')
        for a in c[::-1]:
            t = ins[pos[a]][2]
            if t.startswith('if') or t.startswith('tb'):
                print(f'     {a:x}: {t}')
