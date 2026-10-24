"""Testa o driver contra o CHIRP DE VERDADE, sem radio e sem interface.

Uso: python -I tests/teste_chirp_real.py
Precisa do pacote chirp instalado (pip install https://codeload.github.com/kk7ds/chirp/tar.gz/refs/heads/master --no-deps).
Se ele nao estiver, o teste sai avisando, sem falhar.

O outro teste (teste_bj8300.py) usa stubs e cobre a aritmetica dos bancos. Este aqui cobre o que
stub nenhum cobre: se o driver **encaixa na API real do CHIRP** - se a classe registra, se o
modelo de bancos responde o que o editor de bancos espera, se os ajustes aplicam pelo caminho que
o CHIRP usa (`apply_to` + `run_apply_callback`) e se tudo isso termina gravado na imagem.

O que NAO da para testar aqui: a conversa com o radio pelo cabo (sync_in/sync_out). Essa parte foi
validada a mao com as ferramentas de tools/cabo contra o radio real.
"""
import importlib.util
import os
import struct
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

try:
    from chirp import chirp_common, memmap, settings
except ImportError:
    print('chirp nao instalado - nada a testar aqui (o teste_bj8300.py cobre o resto)')
    sys.exit(0)

CAMINHO = os.path.join(RAIZ, 'chirp', 'bj8300_v1.0TEMPRA.py')
_nome = 'chirp.loaded.%s' % os.path.splitext(os.path.basename(CAMINHO))[0]
_spec = importlib.util.spec_from_file_location(_nome, CAMINHO)
bj = importlib.util.module_from_spec(_spec)
sys.modules[_nome] = bj
_spec.loader.exec_module(bj)


def imagem(ativos=0x0005, escolhido=2, bancos=((0, 'test', [42]), (2, 'bnk', [41]))):
    """Imagem .img: a parte de fabrica zerada + a area dos bancos como o radio grava.

    A parte de fabrica vai em 0x00, nao em 0xFF: o driver de base decodifica a frequencia dos VFO
    em BCD ao montar os ajustes, e 0xFF nao e BCD valido - com a imagem apagada ele levanta
    InternalError antes de chegar no que interessa aqui. Zero e BCD valido e deixa todo ajuste no
    primeiro valor da lista.
    """
    cab = bytearray(bj.MARCA_CAB + struct.pack('<HBB', ativos, escolhido, 0) + b'\x00\x00')
    cab[8:10] = struct.pack('<H', bj._soma(bytes(cab[:8])))
    setores = [bytes(cab).ljust(bj.TAM_SETOR, b'\xff')]
    conteudo = {n: (nome, canais) for n, nome, canais in bancos}
    for b in range(bj.NUM_BANCOS):
        if b not in conteudo:
            setores.append(b'\xff' * bj.TAM_SETOR)
            continue
        nome, canais = conteudo[b]
        reg = bytearray(bj.MARCA_BANCO + b'\x00' * (bj.TAM_REGISTRO - 4))
        reg[4:4 + len(nome)] = nome.encode()
        for c in canais:
            reg[16 + (c >> 3)] |= 1 << (c & 7)
        reg[136:138] = struct.pack('<H', bj._soma(bytes(reg[:136])))
        setores.append(bytes(reg).ljust(bj.TAM_SETOR, b'\xff'))
    return memmap.MemoryMapBytes(b'\x00' * bj.BLOB_BASE + b''.join(setores))


def main():
    falhas = []

    def confere(nome, obtido, esperado):
        ok = obtido == esperado
        print('%-64s %s' % (nome, 'ok' if ok else 'FALHOU (%r != %r)' % (obtido, esperado)))
        if not ok:
            falhas.append(nome)

    # 1. a classe e um radio do CHIRP, registrado e com o nome combinado
    confere('1. deriva de CloneModeRadio', issubclass(bj.BJ8300, chirp_common.CloneModeRadio), True)
    confere('1b. nome do modelo', (bj.BJ8300.VENDOR, bj.BJ8300.MODEL), ('Baojie', 'BJ8300_v1.0TEMPRA'))
    confere('1c. 960 canais, como o radio', bj.BJ8300._upper, 960)

    # 2. abre uma imagem (e o caminho que o CHIRP usa ao carregar um .img)
    radio = bj.BJ8300(imagem())
    rf = radio.get_features()
    confere('2. tem banco e nome de banco', (rf.has_bank, rf.has_bank_names), (True, True))
    confere('2b. limites de canal', rf.memory_bounds, (1, 960))

    # 3. imagem curta (salva pelo driver RT-920) ganha area de bancos apagada em vez de estourar
    curta = bj.BJ8300(memmap.MemoryMapBytes(b'\xff' * bj.BLOB_BASE))
    confere('3. imagem sem area de bancos abre vazia',
            [b.get_name() for b in curta.get_bank_model().get_mappings()][:2],
            ['BANK 1', 'BANK 2'])

    # 4. o modelo de bancos responde o que o editor de bancos do CHIRP chama
    modelo = radio.get_bank_model()
    confere('4. e um MappingModel do CHIRP', isinstance(modelo, chirp_common.MappingModel), True)
    confere('4b. 16 bancos', modelo.get_num_mappings(), 16)
    nomes = [b.get_name() for b in modelo.get_mappings()]
    confere('4c. nomes lidos da imagem', (nomes[0], nomes[2], nomes[5]), ('test', 'bnk', 'BANK 6'))

    mem43 = radio.get_memory(43)
    confere('4d. canal 43 esta no banco 1',
            [b.get_index() for b in modelo.get_memory_mappings(mem43)], [1])
    modelo.add_memory_to_mapping(mem43, modelo.get_mappings()[2])
    confere('4e. depois de marcar, esta nos bancos 1 e 3',
            [b.get_index() for b in modelo.get_memory_mappings(mem43)], [1, 3])
    confere('4f. o banco 3 manteve o que ja tinha e o nome',
            ([m.number for m in modelo.get_mapping_memories(modelo.get_mappings()[2])],
             modelo.get_mappings()[2].get_name()), ([42, 43], 'bnk'))
    modelo.remove_memory_from_mapping(mem43, modelo.get_mappings()[0])
    confere('4g. depois de desmarcar, so no banco 3',
            [b.get_index() for b in modelo.get_memory_mappings(mem43)], [3])
    modelo.get_mappings()[4].set_name('BANDA 2M')
    confere('4h. renomear grava na imagem', modelo.get_mappings()[4].get_name(), 'BANDA 2M')

    # 5. os ajustes aplicam pelo caminho do CHIRP: apply_to devolve os nossos, com callback
    ajustes = radio.get_settings()
    grupo = [g for g in ajustes if g.get_name() == 'bancos']
    confere('5. grupo de bancos nos ajustes', len(grupo), 1)
    nao_mem = ajustes.apply_to(radio._memobj)
    meus = [s for s in nao_mem if s.get_name().startswith('banco_')]
    confere('5b. os nossos ajustes voltam de apply_to com callback',
            (len(meus), all(s.has_apply_callback() for s in meus)), (17, True))

    ligar = [s for s in meus if s.get_name() == 'banco_on_4'][0]
    ligar.value.set_value(True)
    ligar.run_apply_callback()
    confere('5c. ligar o banco 5 pelo ajuste grava no cabecalho',
            bj._Bancos(radio).ativos() & (1 << 4), 1 << 4)

    escolher = [s for s in meus if s.get_name() == 'banco_escolhido'][0]
    escolher.value.set_value(escolher.value.get_options()[6])
    escolher.run_apply_callback()
    confere('5d. escolher o banco 7 grava no cabecalho', bj._Bancos(radio).escolhido(), 6)

    # 6. o que foi mexido esta mesmo nos bytes da imagem, com marca e soma validas
    crua = radio._mmap.get_packed()
    cab = crua[bj.BLOB_BASE:bj.BLOB_BASE + bj.TAM_CABECA]
    confere('6. cabecalho com marca e soma que fecham',
            (cab[:4], struct.unpack('<H', cab[8:10])[0]), (bj.MARCA_CAB, bj._soma(cab[:8])))
    reg5 = crua[bj.BLOB_BASE + 5 * bj.TAM_SETOR:bj.BLOB_BASE + 5 * bj.TAM_SETOR + bj.TAM_REGISTRO]
    confere('6b. banco 5 com marca e soma que fecham',
            (reg5[:4], struct.unpack('<H', reg5[136:])[0]), (bj.MARCA_BANCO, bj._soma(reg5[:136])))
    confere('6c. tamanho da imagem = fabrica + 17 setores',
            len(crua), bj.BLOB_BASE + (bj.NUM_BANCOS + 1) * bj.TAM_SETOR)

    # 7. os pacotes do cabo (o que o radio vai receber)
    confere('7. comando de leitura de 0xC1000', bj._comando(b'R', 0x0C1000),
            bytes([0x52, 0x10, 0x00, 0x8C]))
    confere('7b. os 17 enderecos dos setores',
            (bj._enderecos()[0], bj._enderecos()[-1], len(bj._enderecos())),
            (0x0C1000, 0x0D1000, 17))

    # 8. roger com o BEEP 2: o valor 3 (que a classe base nao conhece) abre, mostra na ordem do
    # radio e grava o valor certo - e a lista da classe base, que outros radios usam, nao muda
    from chirp.drivers import radtel_rt900
    radio = bj.BJ8300(imagem())
    radio._memobj.settings.roger = 3
    roger = [s for s in radio.get_settings().walk() if s.get_name() == 'settings_roger']
    confere('8. ajuste do roger presente, uma vez so', len(roger), 1)
    roger = roger[0]
    confere('8b. as quatro opcoes na ordem do radio', roger.value.get_options(),
            ['OFF', 'BEEP 1', 'BEEP 2', 'TONE1200'])
    confere('8c. valor 3 aparece como BEEP 2', str(roger.value), 'BEEP 2')
    confere('8d. a imagem continua com o valor 3', int(radio._memobj.settings.roger), 3)
    gravados = []
    for nome in ('OFF', 'BEEP 1', 'BEEP 2', 'TONE1200'):
        ajustes = radio.get_settings()
        r = [s for s in ajustes.walk() if s.get_name() == 'settings_roger'][0]
        r.value.set_value(nome)
        # a parte do set_settings que aplica os MemSetting (o roger e um); o resto do set_settings
        # roda os callbacks da classe base, e o da frequencia do VFO nao funciona com a imagem
        # zerada deste teste
        ajustes.apply_to(radio._memobj)
        gravados.append(int(radio._memobj.settings.roger))
    confere('8e. OFF/BEEP 1/BEEP 2/TONE1200 gravam 0/1/3/2', gravados, [0, 1, 3, 2])
    confere('8f. a lista da classe base nao mudou', radtel_rt900.ROGER_LIST,
            ['OFF', 'BEEP', 'TONE1200'])

    # 9. scramble com 2900 Hz. Os bytes entram e saem crus, como o radio os grava: o set do item
    # (0x01E3D06C) poe a posicao no menu no nibble alto (byte 14 do canal, 16 do VFO) e guarda o
    # nibble baixo - aqui com bits ligados de proposito, para ver que ninguem os apaga sem querer.
    def poe_byte(obj, pos, valor):
        cru = bytearray(obj.get_raw())
        cru[pos] = valor
        obj.set_raw(bytes(cru))

    radio = bj.BJ8300(imagem())
    mem = radio.get_memory(10)                 # o registro zerado da imagem conta como canal vazio
    mem.empty, mem.freq = False, 146520000
    mem.power = radio.get_features().valid_power_levels[1]
    radio.set_memory(mem)
    baixo = radio._memobj.memory[9].get_raw()[14] & 0x0F                   # a potencia, entre outros
    poe_byte(radio._memobj.memory[9], 14, 0x20 | baixo)                    # canal 10 em 2900 Hz
    mem = radio.get_memory(10)
    confere('9. canal: as tres opcoes', mem.extra['scramble'].value.get_options(),
            ['Off', '3400 Hz', '2900 Hz'])
    confere('9b. canal com nibble 2 aparece como 2900 Hz', str(mem.extra['scramble'].value), '2900 Hz')
    gravados = []
    for nome in ('Off', '3400 Hz', '2900 Hz'):
        mem = radio.get_memory(10)
        mem.extra['scramble'].value.set_value(nome)
        radio.set_memory(mem)
        gravados.append(radio._memobj.memory[9].get_raw()[14])
    confere('9c. canal: Off/3400/2900 gravam o nibble 0/1/2, potencia junto',
            (baixo != 0, gravados), (True, [baixo, 0x10 | baixo, 0x20 | baixo]))

    poe_byte(radio._memobj.vfo.a, 16, 0x2E)                   # VFO A: 2900 Hz, nibble baixo 0xE
    poe_byte(radio._memobj.vfo.b, 16, 0x11)                   # VFO B: 3400 Hz, potencia 1
    ajustes = {s.get_name(): s for s in radio.get_settings().walk()}
    confere('9d. VFO A e B lidos do nibble alto',
            (str(ajustes['vfo_a_scramble'].value), str(ajustes['vfo_b_scramble'].value)),
            ('2900 Hz', '3400 Hz'))
    gravados = []
    for nome in ('Off', '3400 Hz', '2900 Hz'):
        ajustes = radio.get_settings()
        [s for s in ajustes.walk() if s.get_name() == 'vfo_a_scramble'][0].value.set_value(nome)
        ajustes.apply_to(radio._memobj)                       # ver a nota do 8e
        gravados.append((radio._memobj.vfo.a.get_raw()[16], radio._memobj.vfo.b.get_raw()[16]))
    confere('9e. VFO A: Off/3400/2900 gravam o nibble 0/1/2; o resto e o B ficam',
            gravados, [(0x0E, 0x11), (0x1E, 0x11), (0x2E, 0x11)])
    confere('9f. o mapa da classe base nao mudou', radtel_rt900.RT900BT._scramble_map,
            [('Off', 0x00), ('On', 0x04)])

    print()
    print('FALHAS: %d' % len(falhas) if falhas else 'tudo certo')
    return 1 if falhas else 0


if __name__ == '__main__':
    sys.exit(main())
