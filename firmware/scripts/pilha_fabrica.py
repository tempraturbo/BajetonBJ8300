"""Maior profundidade de pilha do firmware de fabrica a partir de uma rotina (o tique da interface).

Rotina = do destino de um call ate o proximo destino de call. Quadro = o que o prologo empilha
({rets, rX-r4} e sp += -N nas primeiras instrucoes). A profundidade de uma rotina e o quadro dela mais
a maior entre as rotinas que ela chama (call ou goto para entrada de rotina; goto nao soma quadro do
chamador). Ciclos sao cortados. E uma estimativa por baixo: chamada por registrador nao entra.
Uso: python -I scripts/pilha_fabrica.py build/app.dis 0xRAIZ [...]
O build.sh o usa pelo scripts/pilha.py, que compara com o firmware montado.
"""
import re
import sys

L = re.compile(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t?(.*)$')
ins = []
for l in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = L.match(l)
    if m and 0x01E00100 <= int(m.group(1), 16) < 0x01E6AD0C:
        ins.append((int(m.group(1), 16), len(m.group(2).split()), m.group(3).strip()))
alvos = set()
for a, n, t in ins:
    d = re.search(r'\bcall (-?\d+)$', t)
    if d:
        alvos.add(a + n + int(d.group(1)))
    if re.match(r'\[--sp\] = \{rets', t):
        alvos.add(a)                         # prologo: comeco de rotina, mesmo so alcancada por goto
inicios = sorted(alvos)
import bisect


def rotina_de(a):
    i = bisect.bisect_right(inicios, a) - 1
    return inicios[i] if i >= 0 else None


def empilhados(t):
    """Os bytes de um "[--sp] = {...}": 4 por registrador da lista, com o rets, as faixas "r7-r4" e os soltos
    "r5, r4" (v156: ate ali so "rets, rN" e "rets, rA-rB" contavam, e "{rets, r5, r4}" valia 0)."""
    m = re.match(r'\[--sp\] = \{([^}]*)\}', t)
    if not m:
        return 0
    n = 0
    for p in m.group(1).split(','):
        f = re.fullmatch(r'\s*r(\d+)-r(\d+)\s*', p)
        n += int(f.group(1)) - int(f.group(2)) + 1 if f else 1
    return 4 * n


quadro, chamadas = {}, {}
pos = {a: i for i, (a, _, _) in enumerate(ins)}
for r in inicios:
    if r not in pos:
        continue
    q = 0
    for a, n, t in ins[pos[r]:pos[r] + 4]:
        if re.match(r'\[--sp\] = \{rets', t):
            q += empilhados(t)
        s = re.match(r'sp \+= -(\d+)', t)
        if s:
            q += int(s.group(1))
    quadro[r] = q
for a, n, t in ins:
    d = re.search(r'\b(call|goto) (-?\d+)$', t)
    if not d:
        continue
    alvo = a + n + int(d.group(2))
    if alvo in quadro:
        chamadas.setdefault(rotina_de(a), set()).add((alvo, d.group(1)))

memo = {}
sys.setrecursionlimit(100000)


def fundo(r, pilha=frozenset()):
    if r in memo:
        return memo[r]
    if r in pilha:
        return 0, [r]
    melhor = (0, [])
    for alvo, tipo in chamadas.get(r, ()):
        f, c = fundo(alvo, pilha | {r})
        if tipo == 'goto':
            f -= quadro.get(r, 0)          # o goto sai da rotina: o quadro dela ja foi devolvido
        if f > melhor[0]:
            melhor = (f, c)
    res = (quadro.get(r, 0) + melhor[0], [r] + melhor[1])
    memo[r] = res
    return res


for raiz in sys.argv[2:]:
    t, c = fundo(int(raiz, 16))
    print(f'{raiz}: {t} B de pilha no caminho mais fundo')
    print('   ' + ' > '.join(f'{x:#x}({quadro.get(x, 0)})' for x in c[:25]))
