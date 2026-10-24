"""Lista a tabela de menus do app.bin do BJ-8300.

Uso: python -I menu_table.py <app.bin> [endereco_de_um_item]
Itens de 28 bytes (struct compactada, sem alinhamento), confirmado pelos handlers decompilados:
  get(), set(), nome_en*, nome_cn* (GBK), opcoes_en*|funcao_de_exibicao, opcoes_cn*, flags
O app.bin roda em 0x01e00100; enderecos < 0xd4a8 estao no segmento copiado para a RAM
(app.bin + 0x6ac0c).
"""
import struct
import sys

BASE = 0x01E00100
SEG2_OFF, SEG2_LEN = 0x6AC0C, 0xD4A8

sys.stdout.reconfigure(encoding='utf-8')
app = open(sys.argv[1], 'rb').read()
probe = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x01E630BB


def offset(addr):
    if BASE <= addr < BASE + len(app):
        return addr - BASE
    if addr < SEG2_LEN:
        return SEG2_OFF + addr
    return None


def cstr(addr):
    o = offset(addr)
    if o is None:
        return None
    raw = app[o:app.index(b'\0', o)]
    if not raw or len(raw) > 40:
        return None
    if all(32 <= b < 127 for b in raw):
        return raw.decode('ascii')
    try:
        return raw.decode('gbk')
    except UnicodeDecodeError:
        return None


def item(addr):
    """addr aponta para o campo nome_en; o item comeca 8 bytes antes (get, set)."""
    get, set_, name, cn, opts, opts_cn, flags = struct.unpack('<7I', app[offset(addr) - 8:offset(addr) + 20])
    return dict(get=get, set=set_, name=name, cn=cn, opts=opts, opts_cn=opts_cn, flags=flags)


def options(ptr, count=16):
    o = offset(ptr)
    if o is None or ptr >= SEG2_LEN:               # so listas de ponteiros no segmento da RAM
        return []
    out = []
    for i in range(count):
        s = cstr(struct.unpack_from('<I', app, o + 4 * i)[0])
        if s is None:
            break
        out.append(s)
    return out


start = probe
while cstr(item(start - 28)['name']):
    start -= 28
addr, n = start, 0
while cstr(item(addr)['name']):
    it = item(addr)
    opts = options(it['opts'])
    kind = f"opcoes={it['opts']:#010x}" if it['opts'] < SEG2_LEN else f"exibe={it['opts']:#x}"
    print(f"{n:2d} {addr - 8:#x} {cstr(it['name']):16s} {str(cstr(it['cn'])):8s} get={it['get']:#x} "
          f"set={it['set']:#x} {kind} flags={it['flags']:#010x}" + (f"  [{' | '.join(opts[:6])}...]" if opts else ''))
    addr += 28
    n += 1
print(f'itens: {n}  tabela {start - 8:#x}..{addr - 8:#x}')
