"""Converte a imagem de abertura do BJ-8300 entre BMP e o formato que o radio guarda.

Uso:
  python -I logo_bmp.py para-radio <entrada.bmp> <saida.bin> [previa.png]
  python -I logo_bmp.py para-png   <entrada.bin> <saida.png>

Formato do radio (conferido em 2026-10-24 lendo 0x10000 pelo cabo e renderizando): 160 x 138
pixels em RGB565 **little-endian**, linha por linha de cima para baixo, 44160 bytes, gravados a
partir de 0x10000 no chip de dados (11 setores de 4 KB). O BMP de 24 bits vem de baixo para cima,
com os canais em BGR - por isso a inversao aqui.
"""
import struct
import sys
import zlib

LARGURA, ALTURA = 160, 138
TAM = LARGURA * ALTURA * 2


def le_bmp(caminho):
    """Devolve a imagem como lista de linhas de cima para baixo, cada pixel (r, g, b)."""
    d = open(caminho, 'rb').read()
    if d[:2] != b'BM':
        raise SystemExit('%s nao e um BMP' % caminho)
    inicio = struct.unpack_from('<I', d, 10)[0]
    largura, altura, planos, bits, compressao = struct.unpack_from('<iiHHI', d, 18)
    if (largura, abs(altura), bits, compressao) != (LARGURA, ALTURA, 24, 0):
        raise SystemExit('a imagem tem que ser %dx%d, 24 bits, sem compressao (esta tem %dx%d, '
                         '%d bits, compressao %d)' % (LARGURA, ALTURA, largura, abs(altura),
                                                      bits, compressao))
    passo = (largura * 3 + 3) & ~3
    linhas = []
    for y in range(abs(altura)):
        o = inicio + y * passo
        linhas.append([(d[o + x * 3 + 2], d[o + x * 3 + 1], d[o + x * 3]) for x in range(largura)])
    return linhas if altura < 0 else linhas[::-1]      # altura positiva = de baixo para cima


def para_565(linhas):
    saida = bytearray()
    for linha in linhas:
        for r, g, b in linha:
            saida += struct.pack('<H', (r >> 3) << 11 | (g >> 2) << 5 | b >> 3)
    return bytes(saida)


def de_565(dados):
    linhas = []
    for y in range(ALTURA):
        linha = []
        for x in range(LARGURA):
            v = struct.unpack_from('<H', dados, (y * LARGURA + x) * 2)[0]
            linha.append(((v >> 11 & 31) * 255 // 31, (v >> 5 & 63) * 255 // 63,
                          (v & 31) * 255 // 31))
        linhas.append(linha)
    return linhas


def grava_png(caminho, linhas):
    cru = b''.join(b'\x00' + bytes(c for px in linha for c in px) for linha in linhas)

    def bloco(tipo, dados):
        corpo = tipo + dados
        return struct.pack('>I', len(dados)) + corpo + struct.pack('>I', zlib.crc32(corpo))

    open(caminho, 'wb').write(
        b'\x89PNG\r\n\x1a\n'
        + bloco(b'IHDR', struct.pack('>IIBBBBB', LARGURA, ALTURA, 8, 2, 0, 0, 0))
        + bloco(b'IDAT', zlib.compress(cru, 9))
        + bloco(b'IEND', b''))


def main():
    if len(sys.argv) >= 4 and sys.argv[1] == 'para-radio':
        linhas = le_bmp(sys.argv[2])
        dados = para_565(linhas)
        open(sys.argv[3], 'wb').write(dados)
        print('%s -> %s (%d bytes, %dx%d RGB565)' % (sys.argv[2], sys.argv[3], len(dados),
                                                     LARGURA, ALTURA))
        if len(sys.argv) > 4:
            grava_png(sys.argv[4], de_565(dados))      # previa do que o radio vai mostrar
            print('previa em %s' % sys.argv[4])
    elif len(sys.argv) == 4 and sys.argv[1] == 'para-png':
        dados = open(sys.argv[2], 'rb').read()
        if len(dados) != TAM:
            raise SystemExit('%s tem %d bytes; o logo sao %d' % (sys.argv[2], len(dados), TAM))
        grava_png(sys.argv[3], de_565(dados))
        print('%s -> %s' % (sys.argv[2], sys.argv[3]))
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
