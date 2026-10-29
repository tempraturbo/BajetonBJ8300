"""Quem le ou grava um endereco de RAM: base carregada como constante ("rX = N") e acesso b/h/[rX+k]
nas 80 instrucoes seguintes (enquanto rX nao for reescrito), mais a constante exata.

ANTES de concluir "ninguem usa", rodar num endereco que o firmware usa de verdade e ver os usos
aparecerem: por exemplo 706D (o nivel do subtom), lido em 0x01E441AC e 0x01E443CE. Uma varredura que
nao acha o vizinho conhecido nao prova nada (v59: o endereco somado de cabeca procurou o lugar errado).
Nao pega acesso indexado por registrador (b[rA+rB]): o deslocamento de 0x14680 em registrador, como o
"r0 = 1374; r0 = b[r6+r0]", se procura pelo valor (1374) no desmontado.

Uso: python -I scripts/escritas.py build/app.dis <endereco em hexa> [so_gravacao]
"""
import re
import sys

linhas = open(sys.argv[1], encoding='utf-8', errors='replace').read().splitlines()
alvo = int(sys.argv[2], 16)
so_grava = len(sys.argv) > 3
ins = []
for l in linhas:
    m = re.match(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*(.*)$', l)
    if m:
        ins.append((int(m.group(1), 16), m.group(2).strip()))
achados = set()
for i, (a, t) in enumerate(ins):
    m = re.match(r'^(r\d+) = (\d+)$', t)
    if m and str(alvo) == m.group(2):
        achados.add((a, t, 'constante'))
    if not m:
        continue
    reg, base = m.group(1), int(m.group(2))
    if not (alvo - 4096 <= base <= alvo):
        continue
    for b, u in ins[i + 1:i + 81]:
        for acc in re.finditer(r'(b|h)?\[%s\+(-?\d+)\]' % reg, u):
            tam = {'b': 1, 'h': 2, None: 4}[acc.group(1)]
            k = base + int(acc.group(2))
            if k <= alvo < k + tam:
                grava = re.match(r'^(b|h)?\[%s\+-?\d+\] [|&]?= ' % reg, u) is not None
                if grava or not so_grava:
                    achados.add((b, u, f'base {reg} = {base:#x} em {a:x}' + (' GRAVA' if grava else '')))
        if re.match(r'^%s = ' % reg, u) or re.match(r'^%s \+= ' % reg, u):
            break
for a, t, o in sorted(achados):
    print(f'{a:x}: {t:40s} {o}')
print(len(achados), 'acessos')
