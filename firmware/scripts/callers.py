"""Chamadores crus (call jaddr23) de funcoes do app.bin do BJ-8300, com a funcao que contem cada chamada.

Uso: python -I callers.py <app.bin> <funcs.txt> 0xALVO [0xALVO ...]
funcs.txt vem do ExportFuncs.java ("inicio fim nome" por faixa). A varredura e crua (todo par de
bytes alinhado), entao tambem acha chamadas em trechos que o Ghidra nao desmontou.
"""
import bisect
import sys

BASE, SEG2_OFF, SEG2_LEN, SEG2_BASE = 0x01E00100, 0x6AC0C, 0xD4A8, 0x02000000

app = open(sys.argv[1], 'rb').read()
ranges = sorted((int(a, 16), int(b, 16), n) for a, b, n in (l.split() for l in open(sys.argv[2])))
starts = [r[0] for r in ranges]
targets = {int(t, 16) for t in sys.argv[3:]}


def owner(addr):
    i = bisect.bisect_right(starts, addr) - 1
    if i >= 0 and ranges[i][0] <= addr <= ranges[i][1]:
        return ranges[i][2]
    return '(fora de funcao conhecida)'


def scan(buf, base):
    for i in range(0, len(buf) - 3, 2):
        b0, b1 = buf[i], buf[i + 1]
        if b1 != 0xEA or not 0x80 <= b0 <= 0xBF:
            continue
        d = ((b0 & 0x3F) << 16) | (buf[i + 3] << 8) | buf[i + 2]
        if d & 0x200000:
            d -= 0x400000
        t = base + i + 4 + 2 * d
        if t in targets:
            yield base + i, t


hits = list(scan(app[:SEG2_OFF], BASE)) + list(scan(app[SEG2_OFF:SEG2_OFF + SEG2_LEN], SEG2_BASE))
for t in sorted(targets):
    sites = [a for a, tt in hits if tt == t]
    print(f'{t:#x}: {len(sites)} chamadas')
    for a in sites:
        print(f'   {a:#x}  em {owner(a)}')
