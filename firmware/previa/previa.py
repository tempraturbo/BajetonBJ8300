"""Compila e roda a previa da tela B e converte cada tela para PNG (1:1 e ampliada 3x).

Uso: python -I previa/previa.py <pasta de saida>   (de dentro de firmware/)

Precisa do zig (pip install ziglang): o "zig cc" e o compilador de C para o PC. A fonte e
regenerada antes (scripts/fonte.py), para a previa nunca mostrar uma tabela velha.
"""
import os
import struct
import subprocess
import sys
import zlib

AQUI = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.join(AQUI, '..')


def png(caminho, pixels, largura, altura):
    """PNG RGB de 8 bits a partir de linhas de tuplas (r, g, b)."""
    bruto = b''.join(b'\0' + bytes(c for p in linha for c in p) for linha in pixels)

    def bloco(tipo, dados):
        return struct.pack('>I', len(dados)) + tipo + dados + struct.pack('>I', zlib.crc32(tipo + dados))

    with open(caminho, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + bloco(b'IHDR', struct.pack('>IIBBBBB', largura, altura, 8, 2, 0, 0, 0))
                + bloco(b'IDAT', zlib.compress(bruto, 9)) + bloco(b'IEND', b''))


def rgb(v):
    """RGB565 para 8 bits por cor, repetindo os bits altos (0x1F vira 0xFF)."""
    r, g, b = v >> 11, v >> 5 & 0x3F, v & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def menu_grupos_h():
    """build/menu_grupos.h: os grupos do menu (v53) como o montar.py os monta - grupos e itens em ordem
    alfabetica -, para a previa desenhar os mesmos nomes sem a tabela do firmware."""
    sys.path.insert(0, os.path.join(FIRMWARE, 'scripts'))
    import montar
    grupos = sorted(montar.GRUPOS)
    itens = [n for g in grupos for n in sorted(montar.GRUPOS[g])]
    with open(os.path.join(FIRMWARE, 'build', 'menu_grupos.h'), 'w', encoding='ascii') as f:
        f.write('/* Gerado pelo previa/previa.py a partir de scripts/montar.py (GRUPOS). */\n')
        f.write('static const char *const menu_previa_nomes[] = {\n')
        f.write(''.join(f'    "{n}",\n' for n in grupos + itens))
        f.write('};\nstatic const unsigned char menu_previa_itens[] = {'
                + ', '.join(str(len(montar.GRUPOS[g])) for g in grupos) + '};\n')
        marcas = [0] * len(grupos) + [montar.MENU_MARCAS.get((g, n), 0) for g in grupos
                                      for n in sorted(montar.GRUPOS[g])]
        f.write('static const unsigned char menu_previa_marcas[] = {' + ', '.join(map(str, marcas)) + '};\n')
        f.write(f'#define MENU_PREVIA_GRUPOS {len(grupos)}u\n')


def main():
    saida = os.path.abspath(sys.argv[1])
    os.makedirs(saida, exist_ok=True)
    subprocess.run([sys.executable, '-I', os.path.join(FIRMWARE, 'scripts', 'fonte.py')], check=True)
    exe = os.path.join(saida, 'previa.exe')
    menu_grupos_h()
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
                    '-DPREVIA', '-I', os.path.join(FIRMWARE, 'build'), '-o', exe,
                    os.path.join(AQUI, 'previa.c')], check=True)
    subprocess.run([exe, saida], check=True)
    for nome in sorted(n for n in os.listdir(saida) if n.endswith('.rgb565')):
        dados = open(os.path.join(saida, nome), 'rb').read()
        linhas = [[rgb(struct.unpack_from('>H', dados, (y * 160 + x) * 2)[0]) for x in range(160)]
                  for y in range(128)]
        base = os.path.join(saida, nome[:-7])
        png(base + '.png', linhas, 160, 128)
        png(base + '-3x.png', [[p for p in linha for _ in range(3)] for linha in linhas for _ in range(3)],
            480, 384)
        os.remove(os.path.join(saida, nome))
        print(base + '.png')
    os.remove(exe)


if __name__ == '__main__':
    main()
