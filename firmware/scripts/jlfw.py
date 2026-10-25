"""Firmware .anc do radio Baojie BJ-8300 (KDH / BinTalk): um UFW JieLi AC695X com uma camada KDH.

Uso:
  python -I jlfw.py unpack <arquivo.anc> <pasta_saida>
  python -I jlfw.py repack <original.anc> <novo_app.bin> <saida.anc>

Camadas, de fora para dentro (todas verificadas pelos CRCs gravados no proprio arquivo e todas
involucoes, entao a mesma funcao cifra e decifra):
1. KDH (BootHelper.Decrypt do 'BinTe ANC Bootloader.exe'): blocos de 1 KB; os 2 primeiros e os
   2 ultimos intactos; modo = bloco % 3 -> intacto / XOR 'KDHT' / XOR 'RBGI', exceto bytes 0x00,
   0xFF, o byte da chave e o seu inverso; o indice da chave so anda nos blocos processados.
2. UFW: cabecalho 0x40 + entradas 0x50 em ENC 0xFFFF. CRCs: cabecalho[0] = crc16(cabecalho[2:]),
   cabecalho[2] = crc16(tabela cifrada), entrada[4] = crc16(corpo em claro). Corpos fora das
   imagens de flash vao em SFC com a chave do chip (bloco = chave ^ (offset_no_arquivo >> 2)),
   exceto os que ja batem crus (ota.bin, tail.bin).
3. Cada imagem de flash (6: 3 variantes x com/sem EOFFSET de 4 KB): tabela JLFS em 0x20 (ENC
   0xFFFF); app_dir_head e seus arquivos em SFC com offsets relativos ao diretorio. Entrada JLFS:
   [0] = crc16(entrada[2:32]), [2] = crc dos dados. O de app_area_head cobre a area em claro de
   0x20 ate o seu tamanho (resto do diretorio + app.bin).
A chave do chip e a que faz a 1a entrada do app_dir_head ter CRC valido e chamar 'app_area_head'.
"""
import os
import struct
import sys

FLASH_TYPES = {0x00, 0x20, 0x21, 0x22, 0x23, 0x24}
KDH_KEYS = (None, b'KDHT', b'RBGI')   # THDK tambem existe no .exe, mas nao e usada


def kdh_scramble(data):
    out = bytearray(data)
    nblk = (len(out) + 1023) // 1024
    ki = 0
    for blk in range(2, nblk - 2):
        key = KDH_KEYS[blk % 3]
        if key is None:
            continue
        for i in range(blk * 1024, min(len(out), blk * 1024 + 1024)):
            b, k = out[i], key[ki]
            if b not in (0, 0xFF, k, k ^ 0xFF):
                out[i] = b ^ k
            ki = (ki + 1) & 3
    return bytes(out)


def step(v):
    return ((v << 1) ^ 0x1021 if v & 0x8000 else v << 1) & 0xFFFF


def enc(data, key):
    out = bytearray(data)
    for i in range(len(out)):
        out[i] ^= key & 0xFF
        key = step(key)
    return bytes(out)


def sfc(data, key, base):
    """SFC: blocos de 32 bytes alinhados pelo ENDERECO, nao pelo inicio dos dados.

    Com base fora do bloco, a fase importa: completa-se a frente com bytes mudos ate o inicio do
    bloco, cifra-se, e eles sao descartados. Para base alinhada o resultado e o mesmo de antes.
    """
    inicio = base & ~31
    desloc = base - inicio
    cheio = bytes(desloc) + bytes(data)
    saida = b''.join(enc(cheio[i:i + 32], key ^ (((inicio + i) >> 2) & 0xFFFF))
                     for i in range(0, len(cheio), 32))
    return saida[desloc:]


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = step(crc)
    return crc


def jlfs_entry(e):
    hcrc, dcrc, off, size, attr = struct.unpack_from('<HHIIB', e, 0)
    return dict(ok=crc16(e[2:32]) == hcrc, crc=dcrc, offset=off, size=size, attr=attr,
                name=e[16:32].split(b'\0')[0].decode('latin1'))


def jlfs(image, start, decode):
    """Entradas JLFS validas a partir de start; 'pos' e o offset relativo ao inicio da tabela."""
    out = []
    for pos in range(0, len(image) - start - 31, 32):
        e = jlfs_entry(decode(image[start + pos:start + pos + 32], pos))
        if not e['ok']:
            break
        out.append(dict(e, pos=pos))
    return out


def ufw_entries(data):
    hdr = enc(data[:0x40], 0xFFFF)
    count = struct.unpack_from('<H', hdr, 8)[0]
    entries = []
    for n in range(count):
        e = enc(data[0x40 + n * 0x50:0x40 + (n + 1) * 0x50], 0xFFFF)
        off, size = struct.unpack_from('<II', e, 8)
        entries.append(dict(index=n, type=e[0], crc=struct.unpack_from('<H', e, 4)[0], offset=off, size=size,
                            name=e[0x40:0x50].split(b'\0')[0].decode('ascii', 'replace'), plain=bytearray(e)))
    return hdr, entries


def app_dir(image, chipkey):
    top = jlfs(image, 0x20, lambda b, _: enc(b, 0xFFFF))
    base = next(t['offset'] for t in top if t['name'] == 'app_dir_head')
    return top, base, jlfs(image, base, lambda b, pos: sfc(b, chipkey, pos))


def find_chipkey(image):
    top = jlfs(image, 0x20, lambda b, _: enc(b, 0xFFFF))
    base = next(t['offset'] for t in top if t['name'] == 'app_dir_head')
    first = image[base:base + 32]
    return next(k for k in range(0x10000)
                if (lambda e: e['ok'] and e['name'] == 'app_area_head')(jlfs_entry(enc(first, k))))


def unpack(src, outdir):
    os.makedirs(outdir, exist_ok=True)
    data = kdh_scramble(open(src, 'rb').read())
    open(os.path.join(outdir, '_sem_kdh.ufw'), 'wb').write(data)
    hdr, entries = ufw_entries(data)
    print(f"chip={hdr[0x10:0x20].split(bytes(1))[0].decode()} "
          f"crc_cabecalho={'ok' if crc16(hdr[2:]) == struct.unpack_from('<H', hdr)[0] else 'FALHOU'} "
          f"crc_tabela={'ok' if crc16(data[0x40:0x40 + len(entries) * 0x50]) == struct.unpack_from('<H', hdr, 2)[0] else 'FALHOU'}")
    fe = next(e for e in entries if e['type'] == 0)
    chipkey = find_chipkey(data[fe['offset']:fe['offset'] + fe['size']])
    print(f'chave_do_chip={chipkey:#06x}')
    for e in entries:
        body = data[e['offset']:e['offset'] + e['size']]
        plain = body if e['type'] in FLASH_TYPES or crc16(body) == e['crc'] else sfc(body, chipkey, e['offset'])
        open(os.path.join(outdir, e['name']), 'wb').write(plain)
        status = ('ok' if crc16(plain) == e['crc'] else 'FALHOU') if e['size'] else '-'
        print(f"  {e['name']:16s} tipo={e['type']:#04x} tam={e['size']:>8,} crc={status}")
        if e['type'] in FLASH_TYPES:
            _, base, sub = app_dir(plain, chipkey)
            for s in sub:
                if s['name'] != 'app.bin':
                    continue
                app = sfc(plain[base + s['offset']:base + s['offset'] + s['size']], chipkey, s['offset'])
                ok = crc16(app) == s['crc']
                print(f"      app_dir_head@{base:#x} app.bin {len(app):,} B crc={'ok' if ok else 'FALHOU'}")
                if e['type'] == 0:
                    open(os.path.join(outdir, 'app.bin'), 'wb').write(app)


def tone_dir(img, chipkey):
    """O diretorio "tone", logo depois do app.bin: (posicao do cabecalho, cabecalho, [entradas]).

    Cada entrada e um JLFS de 32 B, cifrado em SFC pela posicao relativa ao app_dir, com 'pos' = a
    posicao dela na imagem; o deslocamento de cada arquivo conta a partir do cabecalho do diretorio.
    """
    _, base, sub = app_dir(img, chipkey)
    app = next(s for s in sub if s['name'] == 'app.bin')
    tone = base + app['offset'] + app['size']
    cab = jlfs_entry(sfc(img[tone:tone + 32], chipkey, tone - base))
    if not cab['ok'] or cab['name'] != 'tone':
        raise SystemExit(f'nao achei o diretorio tone em {tone:#x}')
    arquivos = []
    p = tone + 32
    while True:
        x = jlfs_entry(sfc(img[p:p + 32], chipkey, p - base))
        if not x['ok']:
            break
        arquivos.append(dict(x, pos=p))
        p += 32
    return tone, cab, arquivos


def refaz_crcs_tone(img, chipkey):
    """Os CRCs do diretorio tone conferindo com o conteudo, como a ferramenta da JieLi os grava: o
    de cada arquivo (crc16 dos dados decifrados) e o do diretorio (crc16 de tudo depois do
    cabecalho), cada entrada com o crc16 dela mesma nos 2 primeiros bytes. A atualizacao pula
    arquivo de recurso cujo CRC nao mudou (v30: dado novo num arquivo de voz, CRC velho na entrada -
    o radio ficou com a voz original). Muda so o que nao confere; devolve quantos arquivos mudaram."""
    _, base, _ = app_dir(img, chipkey)
    tone, cab, arquivos = tone_dir(img, chipkey)

    def regrava(pos, dados_crc):
        raw = bytearray(sfc(img[pos:pos + 32], chipkey, pos - base))
        struct.pack_into('<H', raw, 2, dados_crc)
        struct.pack_into('<H', raw, 0, crc16(raw[2:32]))
        img[pos:pos + 32] = sfc(bytes(raw), chipkey, pos - base)

    mudou = 0
    for x in arquivos:
        ini = tone + x['offset']
        crc = crc16(sfc(img[ini:ini + x['size']], chipkey, ini - base))
        if crc != x['crc']:
            regrava(x['pos'], crc)
            mudou += 1
    crc = crc16(sfc(img[tone + 32:tone + cab['size']], chipkey, tone + 32 - base))
    if crc != cab['crc']:
        regrava(tone, crc)
    return mudou


def repack(src, new_app, dst, blob=None, overwrite=()):
    """Troca o app.bin em todas as imagens de flash e recalcula todos os CRCs.

    blob = (endereco, bytes) ou uma lista desses pares: grava tambem blocos no espaco livre do fim
    de cada imagem, cifrados em SFC como o resto da area do app. O offset dentro da imagem muda de
    uma para outra (o app comeca em lugares diferentes), mas o endereco de execucao e o mesmo em
    todas, que e o que importa. Cada bloco tem que cair em multiplo de 32 B (o bloco do SFC) e em
    area apagada - os dois sao conferidos. O CRC de app_area_head cobre so os primeiros 0x781dc
    bytes da area, bem antes do fim da imagem, entao o que gravamos ali fica fora dele.

    overwrite = faixas (inicio, fim), em endereco de execucao, onde um bloco pode cair sobre dado que
    ja existe (as vozes que deixaram de ser tocadas); fora delas, so em area apagada.
    """
    data = bytearray(kdh_scramble(open(src, 'rb').read()))
    hdr, entries = ufw_entries(data)
    fe = next(e for e in entries if e['type'] == 0)
    chipkey = find_chipkey(data[fe['offset']:fe['offset'] + fe['size']])
    reference = None
    for e in entries:
        if e['type'] not in FLASH_TYPES:
            continue
        img = bytearray(data[e['offset']:e['offset'] + e['size']])
        _, base, sub = app_dir(img, chipkey)
        head = next(s for s in sub if s['name'] == 'app_area_head')
        app = next(s for s in sub if s['name'] == 'app.bin')
        cur = sfc(img[base + app['offset']:base + app['offset'] + app['size']], chipkey, app['offset'])
        if reference is None:
            reference = cur
        if cur != reference:
            raise SystemExit(f"{e['name']}: app.bin difere do da imagem principal; troca automatica recusada")
        if len(new_app) != app['size']:
            raise SystemExit(f'novo app.bin tem {len(new_app)} B; o original tem {app["size"]} B '
                             '(mudar o tamanho ainda nao e suportado)')
        img[base + app['offset']:base + app['offset'] + app['size']] = sfc(new_app, chipkey, app['offset'])

        def rewrite(entry, data_crc):
            raw = bytearray(sfc(img[base + entry['pos']:base + entry['pos'] + 32], chipkey, entry['pos']))
            struct.pack_into('<H', raw, 2, data_crc)
            struct.pack_into('<H', raw, 0, crc16(raw[2:32]))
            img[base + entry['pos']:base + entry['pos'] + 32] = sfc(bytes(raw), chipkey, entry['pos'])

        rewrite(app, crc16(new_app))
        area = sfc(img[base:base + head['size']], chipkey, 0)   # app.bin ja atualizado acima
        rewrite(head, crc16(area[0x20:head['size']]))

        for endereco, codigo in ([] if blob is None else [blob] if isinstance(blob, tuple) else blob):
            pos = endereco - 0x01E00100 + base + 0x100   # mesmo endereco de execucao em cada imagem
            if pos < 0 or pos + len(codigo) > len(img):
                raise SystemExit(f"{e['name']}: {len(codigo)} B nao cabem em {pos:#x} "
                                 f"(a imagem acaba em {len(img):#x})")
            autorizado = any(a <= endereco and endereco + len(codigo) <= b for a, b in overwrite)
            if not autorizado and any(b != 0xFF for b in img[pos:pos + len(codigo)]):
                raise SystemExit(f"{e['name']}: {pos:#x} nao esta livre")
            # Tudo o que fica depois do diretorio do app e lido pelo SFC, que decifra por
            # endereco; o que gravamos tem que ir cifrado com a mesma regra (offset relativo ao
            # diretorio), senao o processador le lixo. A checagem de area livre acima e nos
            # bytes crus, que e como a flash apagada se apresenta.
            img[pos:pos + len(codigo)] = sfc(codigo, chipkey, pos - base)
        if overwrite:
            refaz_crcs_tone(img, chipkey)

        data[e['offset']:e['offset'] + e['size']] = img
        struct.pack_into('<H', e['plain'], 4, crc16(img))

    for e in entries:
        data[0x40 + e['index'] * 0x50:0x40 + (e['index'] + 1) * 0x50] = enc(bytes(e['plain']), 0xFFFF)
    hdr = bytearray(hdr)
    struct.pack_into('<H', hdr, 2, crc16(data[0x40:0x40 + len(entries) * 0x50]))
    struct.pack_into('<H', hdr, 0, crc16(hdr[2:0x40]))
    data[:0x40] = enc(bytes(hdr), 0xFFFF)
    open(dst, 'wb').write(kdh_scramble(data))


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'unpack' and len(sys.argv) == 4:
        unpack(sys.argv[2], sys.argv[3])
    elif cmd == 'repack' and len(sys.argv) == 5:
        repack(sys.argv[2], open(sys.argv[3], 'rb').read(), sys.argv[4])
    else:
        sys.exit(__doc__)
