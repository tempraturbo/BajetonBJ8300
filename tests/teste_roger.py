"""Confere o ROGER CUSTOM do firmware (v164, o roger_custom do firmware/src/bancos.c) no PC: a rotina sai do proprio
bancos.c, com o marca_ok, e roda sobre um chip de dados imitado, com o TOM e a ESPERA_2MS anotados.

Confere: o registro certo toca as notas na ordem, com o tom na unidade do gerador (Hz x 10.32444) e a espera em 2 ms (a
pausa e o tom 0); e nao toca nada com a marca, a soma, a quantidade (0 e 33) ou o total (mais de 3 s) errados, nem com
o setor apagado. Com --amostra <pasta>, grava la o registro de exemplo (roger-exemplo.bin), que os testes do Tempra Link
(tempralink/testes) conferem contra o que ele monta. Precisa do zig (pip install ziglang).
Uso: python -I tests/teste_roger.py [--amostra <pasta>]
"""
import os
import re
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BANCOS = os.path.join(RAIZ, 'firmware', 'src', 'bancos.c')
SETOR = 0xDA000
falhas = []


def confere(nome, obtido, esperado):
    ok = obtido == esperado
    print(f'{nome}: {"ok" if ok else "FALHOU"}')
    if not ok:
        print(f'    obtido:   {obtido!r}\n    esperado: {esperado!r}')
        falhas.append(nome)


def soma(b):
    """A do roger: 0x1234 mais as palavras de 16 bits (o byte de baixo primeiro), em 16 bits."""
    return (0x1234 + sum(struct.unpack(f'<{len(b) // 2}H', b))) & 0xFFFF


def registro(notas, marca=b'BCO1', n=None, soma_extra=0):
    """O registro como o Tempra Link o grava: a marca, a quantidade, um byte livre (0), as 32 notas (Hz e ms, u16, as
    que sobram em 0) e a soma dos 134 bytes."""
    corpo = marca + bytes([len(notas) if n is None else n, 0])
    corpo += b''.join(struct.pack('<HH', hz, ms) for hz, ms in notas) + bytes(4 * (32 - len(notas)))
    return corpo + struct.pack('<H', (soma(corpo) + soma_extra) & 0xFFFF)


def trecho(fonte, inicio, fim):
    i = fonte.index(inicio)
    return fonte[i:fonte.index(fim, i) + len(fim)]


fonte = open(BANCOS, encoding='utf-8').read()
partes = [
    trecho(fonte, '#define MARCA_BANCO', '\n'),
    trecho(fonte, 'static int marca_ok(', '\n}\n'),
    trecho(fonte, '#define SETOR_ROGER', '\n'),
    trecho(fonte, '#define ROGER_NOTAS', '\n'),
    trecho(fonte, '#define ROGER_NOTA(k)', '\n'),
    trecho(fonte, 'NOS_MORTOS(21) static void roger_custom(void)', '\n}\n'),
]
programa = r'''#include <stdio.h>
#include <string.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define NOS_MORTOS(n)
static u8 chip[0x100000];
static void LE_FLASH(u32 a, void *p, u32 n) { memcpy(p, chip + a, n); }
static void TOM(unsigned f1, unsigned f2) { printf("tom %u %u\n", f1 & 0xFFFF, f2); }
static void ESPERA_2MS(unsigned n) { printf("espera %u\n", n); }
''' + '\n'.join(partes) + r'''
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    memset(chip, 0xFF, sizeof chip);
    fread(chip + SETOR_ROGER, 1, 4096, f);
    fclose(f);
    roger_custom();
    printf("fim\n");
    return 0;
}
'''

with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as pasta:
    c = os.path.join(pasta, 'roger.c')
    exe = os.path.join(pasta, 'roger.exe')
    open(c, 'w', encoding='utf-8').write(programa)
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O1', '-w', '-o', exe, c], check=True)

    def toca(reg):
        arq = os.path.join(pasta, 'reg.bin')
        open(arq, 'wb').write(reg + b'\xff' * (4096 - len(reg)))
        saida = subprocess.run([exe, arq], capture_output=True, text=True, check=True).stdout.split('\n')
        return [tuple(int(x) for x in l.split()[1:]) for l in saida if l.startswith(('tom', 'espera'))]

    exemplo = [(988, 80), (0, 20), (1319, 81), (2637, 150), (300, 10), (3000, 1000)]
    reg = registro(exemplo)
    confere('o registro tem 136 B', len(reg), 136)
    esperado = []
    for hz, ms in exemplo:
        esperado += [(hz * 10572 >> 10, 0), ((ms + 1) >> 1,)]
    confere('o certo toca as notas na ordem: o tom em Hz x 10.32444 (a pausa, 0) e a espera em 2 ms', toca(reg), esperado)
    confere('o tom de 1000 Hz e o do BEEP de fabrica (10324)', 1000 * 10572 >> 10, 10324)
    confere('32 notas de 90 ms (2880 ms) tocam', len(toca(registro([(1000, 90)] * 32))), 64)
    confere('com a marca errada, nada', toca(registro(exemplo, marca=b'BCO2')), [])
    confere('com a soma errada, nada', toca(registro(exemplo, soma_extra=1)), [])
    confere('com um byte de nota estragado no comeco (a soma pega), nada', toca(reg[:10] + bytes([reg[10] ^ 4]) + reg[11:]),
            [])
    confere('com o byte livre estragado, nada', toca(reg[:5] + b'\x01' + reg[6:]), [])
    confere('com uma nota que sobra (depois da quantidade) estragada, nada', toca(reg[:120] + b'\x07' + reg[121:]), [])
    confere('com 0 notas, nada', toca(registro([])), [])
    confere('com 33 na quantidade, nada', toca(registro(exemplo, n=33)), [])
    confere('com mais de 3 s ao todo, nada', toca(registro([(1000, 1000)] * 3 + [(1000, 10)])), [])
    confere('com 3 s cravados, toca', len(toca(registro([(1000, 1000)] * 3))), 6)
    confere('as notas depois da quantidade nao contam no total', len(toca(registro([(1000, 1000)] * 3 + [(1000, 900)], n=3))),
            6)
    confere('o setor apagado (0xFF), nada', toca(b'\xff' * 136), [])
    if '--amostra' in sys.argv:
        destino = sys.argv[sys.argv.index('--amostra') + 1]
        open(os.path.join(destino, 'roger-exemplo.bin'), 'wb').write(reg)
        print(f'amostra em {destino}: roger-exemplo.bin ({exemplo})')

print('\ntodos os testes passaram' if not falhas else f'\n{len(falhas)} FALHARAM: ' + ', '.join(falhas))
sys.exit(1 if falhas else 0)
