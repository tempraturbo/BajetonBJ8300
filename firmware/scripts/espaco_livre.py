"""Procura espaco livre no app.bin do BJ-8300 para o codigo novo dos bancos.

Uso: python -I espaco_livre.py <app.bin> [tamanho_minimo]

Procura corridas longas de um mesmo byte (0x00 ou 0xFF), que normalmente sao enchimento entre
secoes e no fim da imagem. O app.bin roda em 0x01e00100; os ultimos 0xd4a8 bytes (a partir de
0x6ac0c) sao copiados para a RAM na partida, entao nao servem como espaco de codigo em flash.
"""
import sys

BASE, SEG2_OFF = 0x01E00100, 0x6AC0C

app = open(sys.argv[1], 'rb').read()
minimo = int(sys.argv[2], 0) if len(sys.argv) > 2 else 64

print(f'app.bin: {len(app):,} bytes; area em flash: 0x0..{SEG2_OFF:#x}; copiado para RAM: {SEG2_OFF:#x}..{len(app):#x}')
i = 0
total = 0
while i < len(app):
    b = app[i]
    j = i
    while j < len(app) and app[j] == b:
        j += 1
    n = j - i
    if n >= minimo and b in (0x00, 0xFF):
        onde = 'flash' if i < SEG2_OFF else 'RAM'
        print(f'  {n:6,} bytes de {b:#04x} em app.bin {i:#08x}..{j:#08x}  (endereco {BASE + i:#010x}, {onde})')
        if onde == 'flash':
            total += n
    i = j
print(f'total de enchimento utilizavel em flash: {total:,} bytes')
