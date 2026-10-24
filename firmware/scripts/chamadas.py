"""Acha chamadas (call jaddr23) no app.bin do BJ-8300, sem precisar do Ghidra.

Uso:
  python -I chamadas.py <app.bin> para 0xALVO [0xALVO ...]   quem chama esses enderecos
  python -I chamadas.py <app.bin> de 0xINI 0xFIM             o que esse trecho chama

Codificacao (confirmada contra o llvm-objdump): 4 bytes, b[1] == 0xEA e b[0] == 0x80|alto6;
deslocamento de 22 bits em palavras, com sinal: destino = endereco + 4 + d*2. A varredura e crua
(todo par de bytes alinhado), entao pode achar coincidencias em dados - conferir com desmonta.sh.
"""
import sys

BASE = 0x01E00100
SEG2_OFF, SEG2_LEN, SEG2_BASE = 0x6AC0C, 0xD4A8, 0x02000000   # trecho copiado para a RAM em 0x0


def endereco(off):
    """Endereco de execucao de um offset do app.bin (o trecho copiado roda em 0x0200xxxx)."""
    if SEG2_OFF <= off < SEG2_OFF + SEG2_LEN:
        return SEG2_BASE + off - SEG2_OFF
    return BASE + off


def chamadas(app):
    """Gera (endereco_da_chamada, destino) para cada call encontrada."""
    for off in range(0, len(app) - 3, 2):
        if app[off + 1] != 0xEA or app[off] & 0xC0 != 0x80:
            continue
        d = ((app[off] & 0x3F) << 16) | (app[off + 3] << 8) | app[off + 2]
        if d & 0x200000:
            d -= 0x400000
        origem = endereco(off)
        yield origem, (origem + 4 + d * 2) & 0xFFFFFFFF


def main():
    app = open(sys.argv[1], 'rb').read()
    modo = sys.argv[2]
    if modo == 'para':
        alvos = {int(t, 0) for t in sys.argv[3:]}
        for origem, destino in chamadas(app):
            if destino in alvos:
                print(f'{origem:#09x} chama {destino:#09x}')
    elif modo == 'de':
        ini, fim = int(sys.argv[3], 0), int(sys.argv[4], 0)
        vistos = {}
        for origem, destino in chamadas(app):
            if ini <= origem <= fim:
                vistos.setdefault(destino, []).append(origem)
        for destino in sorted(vistos):
            locais = ' '.join(f'{o:#x}' for o in vistos[destino])
            print(f'{destino:#09x}  <- {locais}')
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
