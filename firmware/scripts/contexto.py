"""Quem chama (ou desvia para) um endereco, com as instrucoes de antes de cada chamada.

Le o desmontado inteiro (build/app.dis, que o build.sh gera) e resolve o destino de todo call e goto,
inclusive os de 2 bytes e os desvios condicionais ("if (...) goto N"): destino = endereco da
instrucao + tamanho dela + deslocamento. Serve para ver com que argumentos uma rotina e chamada (os
"r0 = ..." antes da chamada) - foi assim que se levantaram as 154 escritas no chip de radio
(0x01E39D82(reg, valor)).

Uso: python -I scripts/contexto.py build/app.dis <linhas de contexto> 0xALVO [0xALVO ...]
"""
import re
import sys

lines = open(sys.argv[1], encoding='utf-8', errors='replace').read().splitlines()
ctx = int(sys.argv[2])
alvos = {int(a, 16) for a in sys.argv[3:]}
pat = re.compile(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$')
ins = []
for l in lines:
    m = pat.match(l)
    if m:
        ins.append((int(m.group(1), 16), len(m.group(2).split()), m.group(3).strip()))
for i, (a, n, t) in enumerate(ins):
    m = re.search(r'(?:call|goto) (-?\d+)$', t)
    if m and a + n + int(m.group(1)) in alvos:
        print(f'----- {a:#x} -> {a + n + int(m.group(1)):#x}')
        for b, _, x in ins[max(0, i - ctx):i + 1]:
            print(f'  {b:x}: {x}')
