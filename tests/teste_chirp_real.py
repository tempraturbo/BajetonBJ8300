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
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

try:
    from chirp import chirp_common, memmap, settings
except ImportError:
    print('chirp nao instalado - nada a testar aqui (o teste_bj8300.py cobre o resto)')
    sys.exit(0)

CAMINHO = os.path.join(RAIZ, 'chirp', 'bj8300_v2.9TEMPRA.py')
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
    cab = bytearray(bj.HEADER_MAGIC + struct.pack('<HBB', ativos, escolhido, 0) + b'\x00\x00')
    cab[8:10] = struct.pack('<H', bj._checksum(bytes(cab[:8])))
    setores = [bytes(cab).ljust(bj.SECTOR_SIZE, b'\xff')]
    conteudo = {n: (nome, canais) for n, nome, canais in bancos}
    for b in range(bj.NUM_BANKS):
        if b not in conteudo:
            setores.append(b'\xff' * bj.SECTOR_SIZE)
            continue
        nome, canais = conteudo[b]
        reg = bytearray(bj.BANK_MAGIC + b'\x00' * (bj.RECORD_LEN - 4))
        reg[4:4 + len(nome)] = nome.encode()
        for c in canais:
            reg[16 + (c >> 3)] |= 1 << (c & 7)
        reg[136:138] = struct.pack('<H', bj._checksum(bytes(reg[:136])))
        setores.append(bytes(reg).ljust(bj.SECTOR_SIZE, b'\xff'))
    return memmap.MemoryMapBytes(b'\x00' * bj.BLOB_BASE + b''.join(setores))


def registro_watch(modo, mdf, canal, c_no_b=0, canal_b=0, vfo=0):
    """O bloco do WATCH como o firmware grava (struct vigia, grava_registro): 14 bytes e o resto apagado."""
    reg = bytearray(bj.WATCH_MAGIC + struct.pack('<BBHBBH', modo, mdf, canal, c_no_b, vfo, canal_b) + b'\x00\x00')
    reg[12:14] = struct.pack('<H', bj._checksum(bytes(reg[:12])))
    return bytes(reg) + b'\xff' * (bj.EXT_BLOCK - bj.WATCH_LEN)


def watch_de(radio):
    """WATCH, CH-C e MDF-C como o Settings > VFO A/B/C Channel mostra, e se o CH-C aceita mudanca."""
    w = {s.get_name(): s for s in radio.get_settings().walk()}
    return (str(w['watch_mode'].value), int(w['watch_channel_c'].value), str(w['watch_mdf_c'].value),
            w['watch_channel_c'].value.get_mutable())


def ajuste_de(radio, nome):
    return {s.get_name(): s for s in radio.get_settings().walk()}[nome]


def _VfcStore_valido(radio):
    return bj._VfcStore.valid(bj._VfcStore(radio).block())


def muda_watch(radio, **valores):
    """Muda ajustes da pagina Watch e aplica como o set_settings da base: os MemSetting na memoria e,
    dos outros, os callbacks - so os da pagina Watch, porque os das frequencias dos VFO nao leem a
    imagem zerada dos testes (como o aplica() dos bancos faz com os bank_)."""
    ajustes = radio.get_settings()
    w = {s.get_name(): s for s in ajustes.walk()}
    for nome, valor in valores.items():
        w[nome].value.set_value(valor)
    for s in ajustes.apply_to(radio._memobj):
        if s.has_apply_callback() and s.get_name().startswith(('watch_', 'vfoc_')):
            s.run_apply_callback()


def main():
    falhas = []

    def confere(nome, obtido, esperado):
        ok = obtido == esperado
        print('%-64s %s' % (nome, 'ok' if ok else 'FALHOU (%r != %r)' % (obtido, esperado)))
        if not ok:
            falhas.append(nome)

    # 1. a classe e um radio do CHIRP, registrado e com o nome combinado
    confere('1. deriva de CloneModeRadio', issubclass(bj.BJ8300, chirp_common.CloneModeRadio), True)
    confere('1b. nome do modelo', (bj.BJ8300.VENDOR, bj.BJ8300.MODEL), ('Bajeton', 'BJ8300_v2.9TEMPRA'))
    confere('1c. 960 canais, como o radio', bj.BJ8300._upper, 960)

    # 2. abre uma imagem (e o caminho que o CHIRP usa ao carregar um .img)
    radio = bj.BJ8300(imagem())
    rf = radio.get_features()
    confere('2. tem banco e nome de banco', (rf.has_bank, rf.has_bank_names), (True, True))
    confere('2b. limites de canal', rf.memory_bounds, (1, 960))

    # 2c. o .img guarda fabricante e modelo, e o CHIRP so o abre com um driver do mesmo modelo
    # (directory.get_radio_by_image): a imagem salva com os modulos antigos (BJ8300_v1.0TEMPRA,
    # BJ8300_v2.0TEMPRA e BJ8300_v2.5TEMPRA) tem de abrir neste, pelo MODEL_COMPAT que ele registra - e um modelo que
    # ninguem conhece, nao
    from chirp import directory, errors

    class Antigo(bj.BJ8300):                 # so para gravar o .img com o nome antigo; nao registra
        pass

    with tempfile.TemporaryDirectory() as pasta:
        arq = os.path.join(pasta, 'antigo.img')
        # ate o modulo da V2.8T o fabricante era Baojie; desde entao e Bajeton (v62), e as imagens
        # salvas com Baojie, inclusive as do BJ8300_v2.8TEMPRA, abrem neste (o da V2.9T)
        Antigo.VENDOR = 'Baojie'
        for letra, Antigo.MODEL in (('', 'BJ8300_v1.0TEMPRA'), ('2', 'BJ8300_v2.0TEMPRA'),
                                    ('3', 'BJ8300_v2.5TEMPRA'), ('4', 'BJ8300_v2.6TEMPRA'),
                                    ('5', 'BJ8300_v2.7TEMPRA'), ('6', 'BJ8300_v2.8TEMPRA')):
            Antigo(imagem()).save_mmap(arq)
            aberto = directory.get_radio_by_image(arq)
            confere(f'2c{letra}. .img Baojie do {Antigo.MODEL} abre neste, como Bajeton',
                    (isinstance(aberto, bj.BJ8300), aberto.VENDOR, aberto.MODEL),
                    (True, 'Bajeton', 'BJ8300_v2.9TEMPRA'))
        Antigo.VENDOR = 'Bajeton'
        for letra, Antigo.MODEL in (('7', 'BJ8300_v2.8TEMPRA'), ('8', 'BJ8300_v2.9TEMPRA')):
            Antigo(imagem()).save_mmap(arq)
            aberto = directory.get_radio_by_image(arq)
            confere(f'2c{letra}. .img Bajeton do {Antigo.MODEL} abre neste',
                    (isinstance(aberto, bj.BJ8300), aberto.VENDOR, aberto.MODEL),
                    (True, 'Bajeton', 'BJ8300_v2.9TEMPRA'))
        Antigo.MODEL = 'BJ8300_v9.9XYZ'
        Antigo(imagem()).save_mmap(arq)
        try:
            directory.get_radio_by_image(arq)
            recusou = False
        except errors.ImageMetadataInvalidModel:
            recusou = True
        confere('2d. e um modelo desconhecido continua recusado', recusou, True)

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

    # 5. Settings > Banks: um bloco por banco (Name, Channels, Enabled) e o banco do menu do radio.
    #    Aplica pelo caminho do CHIRP: apply_to devolve os nossos e os callbacks rodam como no
    #    set_settings da classe base (so os nossos: o da frequencia do VFO nao funciona com a
    #    imagem zerada deste teste).
    radio = bj.BJ8300(imagem())
    for n in (42, 43):                        # grava os canais que os bancos 1 (43) e 3 (42) tem
        mem = radio.get_memory(n)
        mem.empty, mem.freq = False, 146520000 + n * 25000
        radio.set_memory(mem)
    modelo = radio.get_bank_model()
    banco = bj._BankStore(radio)

    def aplica(ajustes):
        for s in ajustes.apply_to(radio._memobj):
            if s.has_apply_callback() and s.get_name().startswith('bank_'):
                s.run_apply_callback()

    ajustes = radio.get_settings()
    grupos = [g for g in ajustes if g.get_name() == 'banks']
    confere('5. grupo Banks nos ajustes', [g.get_shortname() for g in grupos], ['Banks'])
    ajuste = {s.get_name(): s for s in grupos[0].walk()}
    blocos = [e for e in grupos[0].values() if isinstance(e, settings.RadioSettingSubGroup)]
    confere('5b. um bloco por banco, de BANK 1 a BANK 16', [b.get_shortname() for b in blocos],
            ['BANK %i' % i for i in range(1, 17)])
    confere('5c. cada bloco com Name, Channels e Enabled',
            {tuple(s.get_shortname() for s in b.values()) for b in blocos},
            {('Name', 'Channels', 'Enabled')})
    confere('5d. bancos 1 a 3: nome, canais gravados e ligado',
            [(str(ajuste['bank_%i_name' % i].value), str(ajuste['bank_%i_channels' % i].value),
              bool(ajuste['bank_%i_enabled' % i].value)) for i in (1, 2, 3)],
            [('test', '1', True), ('', '0', False), ('bnk', '1', True)])
    confere('5e. Channels e so leitura', ajuste['bank_1_channels'].value.get_mutable(), False)
    confere('5f. banco do menu com os nomes, e o escolhido da imagem',
            (ajuste['bank_selected'].value.get_options()[:3], str(ajuste['bank_selected'].value)),
            (['BANK 1 (test)', 'BANK 2', 'BANK 3 (bnk)'], 'BANK 3 (bnk)'))

    ajuste['bank_2_name'].value.set_value('REPEATERS')
    aplica(ajustes)
    confere('5g. renomear pelo Settings grava na imagem', banco.name(1), 'REPEATERS')
    modelo.get_mappings()[0].set_name('NOVO')         # renomeado na aba Banks, com a pagina aberta
    aplica(ajustes)
    confere('5h. valor antigo da pagina nao desfaz o nome dado na aba Banks', banco.name(0), 'NOVO')

    e2 = ajuste['bank_2_enabled']
    e2.value.set_value(True)
    aplica(ajustes)
    confere('5i. banco sem canal gravado nao liga (o CHIRP desmarca)',
            (bool(e2.value), banco.enabled_mask() >> 1 & 1), (False, 0))
    modelo.add_memory_to_mapping(radio.get_memory(42), modelo.get_mappings()[3])
    e4 = ajuste['bank_4_enabled']                     # a pagina ainda mostra 0 canais nele
    e4.value.set_value(True)
    aplica(ajustes)
    confere('5j. a regra confere na hora: canal posto na aba Banks deixa ligar',
            (bool(e4.value), banco.enabled_mask() >> 3 & 1), (True, 1))
    modelo.remove_memory_from_mapping(radio.get_memory(43), modelo.get_mappings()[0])
    confere('5k. tirar o ultimo canal do banco o desliga sozinho', banco.enabled_mask() & 1, 0)
    aplica(ajustes)                                   # a pagina ainda tem o banco 1 marcado
    confere('5l. e o valor antigo da pagina nao o religa', banco.enabled_mask() & 1, 0)

    ajuste['bank_selected'].value.set_value(ajuste['bank_selected'].value.get_options()[6])
    aplica(ajustes)
    confere('5m. escolher o banco 7 grava no cabecalho', banco.selected(), 6)

    # 6. o que foi mexido esta mesmo nos bytes da imagem, com marca e soma validas
    crua = radio._mmap.get_packed()
    cab = crua[bj.BLOB_BASE:bj.BLOB_BASE + bj.HEADER_LEN]
    confere('6. cabecalho com marca e soma que fecham',
            (cab[:4], struct.unpack('<H', cab[8:10])[0]), (bj.HEADER_MAGIC, bj._checksum(cab[:8])))
    reg4 = crua[bj.BLOB_BASE + 4 * bj.SECTOR_SIZE:bj.BLOB_BASE + 4 * bj.SECTOR_SIZE + bj.RECORD_LEN]
    confere('6b. banco 4 (que ganhou canal no 5j) com marca e soma que fecham',
            (reg4[:4], struct.unpack('<H', reg4[136:])[0]), (bj.BANK_MAGIC, bj._checksum(reg4[:136])))
    confere('6c. tamanho da imagem = fabrica + 17 setores + o bloco do WATCH + os 2 do VFO C',
            len(crua), bj.BLOB_BASE + (bj.NUM_BANKS + 1) * bj.SECTOR_SIZE + 3 * bj.EXT_BLOCK)

    # 7. os pacotes do cabo (o que o radio vai receber)
    confere('7. comando de leitura de 0xC1000', bj._command(b'R', 0x0C1000),
            bytes([0x52, 0x10, 0x00, 0x8C]))
    confere('7b. os 17 enderecos dos setores',
            (bj._sector_addresses()[0], bj._sector_addresses()[-1], len(bj._sector_addresses())),
            (0x0C1000, 0x0D1000, 17))

    # 8. roger com o BEEP 2 e o COIN: os valores 3 e 4 (que a classe base nao conhece; o 4 passa
    # dos 2 bits dela) abrem, mostram na ordem do radio e gravam o byte certo - e a lista da classe
    # base, que outros radios usam, nao muda
    from chirp.drivers import radtel_rt900
    radio = bj.BJ8300(imagem())
    ajuste = radio._memobj.settings

    def byte_9017():
        return int(ajuste.unused_9017) << 2 | int(ajuste.roger)

    def roger_de(r):
        return [x for x in r.get_settings().walk() if x.get_name() == 'settings_roger']

    ajuste.roger = 3
    roger = roger_de(radio)
    confere('8. ajuste do roger presente, uma vez so', len(roger), 1)
    roger = roger[0]
    confere('8b. as cinco opcoes na ordem do radio', roger.value.get_options(),
            ['OFF', 'BEEP 1', 'BEEP 2', 'COIN', 'TONE1200'])
    confere('8c. valor 3 aparece como BEEP 2', str(roger.value), 'BEEP 2')
    confere('8d. a imagem continua com o valor 3', byte_9017(), 3)
    ajuste.roger, ajuste.unused_9017 = 0, 1
    confere('8g. valor 4 aparece como COIN', str(roger_de(radio)[0].value), 'COIN')
    confere('8h. a imagem continua com o valor 4', byte_9017(), 4)
    gravados = []
    for nome in ('OFF', 'BEEP 1', 'BEEP 2', 'COIN', 'TONE1200'):
        ajustes = radio.get_settings()
        r = [x for x in ajustes.walk() if x.get_name() == 'settings_roger'][0]
        r.value.set_value(nome)
        # a parte do set_settings que aplica os MemSetting (o roger e um); o resto do set_settings
        # roda os callbacks da classe base, e o da frequencia do VFO nao funciona com a imagem
        # zerada deste teste
        ajustes.apply_to(radio._memobj)
        gravados.append(byte_9017())
    confere('8e. OFF/BEEP 1/BEEP 2/COIN/TONE1200 gravam 0/1/3/4/2', gravados, [0, 1, 3, 4, 2])
    nomes = [x.get_name() for x in radio.get_settings().walk()]
    confere('8g2. o Voice Prompts saiu (o firmware nao toca mais voz)', 'settings_voice' in nomes, False)
    confere('8g3. o Language saiu (o firmware fica sempre em ingles, v37)', 'settings_language' in nomes, False)
    confere('8g4. o resto da pagina continua (Beep, DTMF ST)', ['settings_beep' in nomes, 'settings_dtmfst' in nomes],
            [True, True])
    confere('8f. a lista da classe base nao mudou', radtel_rt900.ROGER_LIST,
            ['OFF', 'BEEP', 'TONE1200'])

    # 9. scramble de 2700 a 3400 Hz. Os bytes entram e saem crus, como o radio os grava: o set do
    # item (0x01E3D06C) poe o valor no nibble alto (byte 14 do canal, 16 do VFO) e guarda o nibble
    # baixo - aqui com bits ligados de proposito, para ver que ninguem os apaga sem querer. Qual
    # valor cada opcao grava sai do firmware (scramble_valor no bancos.c), nao do driver.
    import re
    with open(os.path.join(RAIZ, 'firmware', 'src', 'bancos.c'), encoding='utf-8') as f:
        valor = [int(v) for v in re.search(r'scramble_valor\[9\] = \{([^}]*)\}', f.read()).group(1).split(',')]
    opcoes = ['Off'] + ['%d Hz' % hz for hz in range(2700, 3401, 100)]

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
    confere('9. canal: OFF e de 2700 a 3400 Hz, em ordem crescente',
            mem.extra['scramble'].value.get_options(), opcoes)
    confere('9b. canal com o valor 2 aparece como 2900 Hz', str(mem.extra['scramble'].value), '2900 Hz')
    gravados = []
    for nome in opcoes:
        mem = radio.get_memory(10)
        mem.extra['scramble'].value.set_value(nome)
        radio.set_memory(mem)
        gravados.append(radio._memobj.memory[9].get_raw()[14])
    confere('9c. canal: cada opcao grava o valor do firmware, potencia junto',
            (baixo != 0, gravados), (True, [v << 4 | baixo for v in valor]))

    poe_byte(radio._memobj.vfo.a, 16, 0x2E)                   # VFO A: 2900 Hz, nibble baixo 0xE
    poe_byte(radio._memobj.vfo.b, 16, 0x91)                   # VFO B: valor 9 (o radio: 3400 Hz)
    ajustes = {s.get_name(): s for s in radio.get_settings().walk()}
    confere('9d. VFO A e B lidos do nibble alto (o 9 como 3400 Hz)',
            (str(ajustes['vfo_a_scramble'].value), str(ajustes['vfo_b_scramble'].value)),
            ('2900 Hz', '3400 Hz'))
    gravados = []
    for nome in opcoes:
        ajustes = radio.get_settings()
        [s for s in ajustes.walk() if s.get_name() == 'vfo_a_scramble'][0].value.set_value(nome)
        ajustes.apply_to(radio._memobj)                       # ver a nota do 8e
        gravados.append((radio._memobj.vfo.a.get_raw()[16], radio._memobj.vfo.b.get_raw()[16]))
    # o B volta como 1: o apply_to grava todo ajuste, e o 9 e o 1 invertem nos mesmos 3400 Hz
    confere('9e. VFO A: cada opcao grava o valor do firmware; resto do byte fica; B em 3400 Hz',
            gravados, [(v << 4 | 0x0E, 0x11) for v in valor])
    confere('9f. o mapa da classe base nao mudou', radtel_rt900.RT900BT._scramble_map,
            [('Off', 0x00), ('On', 0x04)])

    # 10. WATCH, CH-C e MDF-C (firmware v46): o registro WTC1, no bloco depois dos bancos
    r = bj.BJ8300(imagem())
    confere('10. imagem sem WATCH ganha os blocos apagados (WATCH e VFO C) depois dos bancos',
            (len(r._mmap), bytes(r._mmap[bj.WATCH_BASE:bj.IMAGE_SIZE])),
            (bj.BLOB_BASE + 17 * bj.SECTOR_SIZE + 3 * 64, b'\xff' * 3 * 64))
    confere('10b. sem registro: DUAL, a C no canal 1, MDF-C Name', watch_de(r), ('DUAL', 1, 'Name', True))
    aj = r.get_settings()
    confere('10c. VFO A/B/C Channel e FM/AM/SSB Modulation fora do Specific, no topo',
            [g.get_shortname() for g in aj],
            ['Basic Settings', 'DTMF Settings', 'BJ8300_v2.9TEMPRA Specific', 'VFO A/B/C Channel',
             'FM/AM/SSB Modulation', 'Banks'])
    abc = aj['abblock']
    confere('10c2. o WATCH e o TTR primeiro no VFO A/B/C Channel, e um bloco para cada lado',
            (abc.keys()[:2], abc['settings_tdr'].get_shortname(), 'settings_tdr' in aj['spec'],
             [k for k, e in abc.items() if isinstance(e, settings.RadioSettingSubGroup)]),
            (['watch_mode', 'settings_tdr'], 'TTR (Transmitter Triple Receiver)', False,
             ['achannel', 'bchannel', 'cchannel']))
    # o NOISE REDUCTION: o cabo entrega o 0x9000 da RAM (0x01E43A94), e o item do radio guarda em
    # b[0x14BDE] (get 0x01E3D0E0), que la cai em 0x9032 - e nao em 0x9031, onde a base o le
    r._mmap[0x9031] = 0
    r._mmap[0x9032] = 1
    nr = {s.get_name(): s for s in r.get_settings().walk()}
    confere('10c4. Noise reduction lido de 0x9032 (o byte do radio), so ele',
            (bool(nr['settings_unknown_9032'].value), nr['settings_unknown_9032'].get_shortname(),
             'settings_noise_reduction' in nr),
            (True, 'Noise reduction', False))
    ajustes = r.get_settings()
    {s.get_name(): s for s in ajustes.walk()}['settings_unknown_9032'].value.set_value(False)
    ajustes.apply_to(r._memobj)
    confere('10c5. e gravado em 0x9032, sem mexer em 0x9031', bytes(r._mmap[0x9031:0x9033]), b'\x00\x00')
    r._mmap[0x9031] = 1
    ajustes = r.get_settings()
    {s.get_name(): s for s in ajustes.walk()}['settings_unknown_9032'].value.set_value(True)
    ajustes.apply_to(r._memobj)
    confere('10c6. ligar grava 1 em 0x9032 e deixa 0x9031 como estava', bytes(r._mmap[0x9031:0x9033]), b'\x01\x01')
    confere('10c3. os 3 MDF so no bloco do lado, no mesmo lugar e com o mesmo nome',
            ([abc[b].keys()[1] for b in ('achannel', 'bchannel', 'cchannel')],
             [abc[b].values()[1].get_shortname() for b in ('achannel', 'bchannel', 'cchannel')],
             sorted(s.get_name() for s in aj.walk() if 'mdf' in s.get_name())),
            (['settings_mdfa', 'settings_mdfb', 'watch_mdf_c'],
             ['Memory Display Format %s (MDF-%s)' % (x, x) for x in 'ABC'],
             ['settings_mdfa', 'settings_mdfb', 'watch_mdf_c']))
    muda_watch(r, watch_mode='TRIPLE', watch_channel_c=5, watch_mdf_c='Frequency')
    bloco = bytes(r._mmap[bj.WATCH_BASE:bj.VFC_BASE])
    confere('10d. TRIPLE, canal 5 e Frequency no registro, como o firmware le', bloco,
            registro_watch(2, 1, 4))
    confere('10e. e lidos de volta pela pagina', watch_de(r), ('TRIPLE', 5, 'Frequency', True))
    muda_watch(r, watch_mode='SINGLE')
    confere('10f. mudar so o WATCH deixa o resto do registro', r._mmap[bj.WATCH_BASE:bj.VFC_BASE],
            registro_watch(1, 1, 4))
    r._mmap[bj.WATCH_BASE] = registro_watch(2, 0, 9, 1, 3)    # a C esta no lado B, no radio
    confere('10g. com a C no lado B, o CH-C e so leitura', watch_de(r), ('TRIPLE', 10, 'Name', False))
    muda_watch(r, watch_mode='DUAL', watch_mdf_c='Channel')
    confere('10h. e mudar o WATCH e o MDF-C nao mexe na C no lado B nem no canal do B',
            r._mmap[bj.WATCH_BASE:bj.VFC_BASE], registro_watch(0, 2, 9, 1, 3))
    r._mmap[bj.WATCH_BASE] = registro_watch(7, 5, 2000)
    confere('10i. valores fora da faixa leem o padrao, como o firmware', watch_de(r), ('DUAL', 1, 'Name', True))
    r._mmap[bj.WATCH_BASE] = registro_watch(2, 1, 4)[:12] + b'\x00\x00' + b'\xff' * 50
    confere('10j. soma errada: registro invalido, DUAL', watch_de(r), ('DUAL', 1, 'Name', True))

    # 10k. VFO C (firmware v49): as mesmas opcoes do A e do B, no bloco vfc depois do WATCH
    r = bj.BJ8300(imagem())
    r._mmap[0x8020] = bytes([1, 4, 6, 5, 2, 0, 0, 0])            # o VFO B em 146.52000
    c = ajuste_de(r, 'vfoc_freq')
    confere('10k. sem o bloco do VFO C: ele mostra o do B, e a C em canal',
            (str(c.value), str(ajuste_de(r, 'watch_c_mode').value), _VfcStore_valido(r)),
            ('146.520000', 'Channel Mode', False))
    muda_watch(r, vfoc_freq='145.500000', vfoc_txpower='Low', watch_c_mode='VFO Mode')
    vfo_b = bytes(r._mmap[0x8020:0x8040])
    blk = bytes(r._mmap[bj.VFC_BASE:bj.VFC_BASE + bj.VFC_LEN])
    confere('10l. mudar o VFO C cria o bloco: a C e a copia do B com a mudanca, o B guardado e o B',
            (blk[:4], blk[4:12], blk[36:68] == vfo_b, _VfcStore_valido(r)),
            (bj.VFC_MAGIC, bytes([1, 4, 5, 5, 0, 0, 0, 0]), True, True))
    confere('10m. e lidos de volta pela pagina; o Work Mode da C no bit 0 do byte 9 do WATCH',
            (str(ajuste_de(r, 'vfoc_freq').value), str(ajuste_de(r, 'vfoc_txpower').value),
             str(ajuste_de(r, 'watch_c_mode').value), r._mmap[bj.WATCH_BASE + 9][0] & 1),
            ('145.500000', 'Low', 'VFO Mode', 1))
    # a C no lado B, no radio: o modo e o VFO dela estao nas vagas do B, e o bloco guarda os do B
    r._mmap[bj.WATCH_BASE] = registro_watch(2, 0, 4, 1, 9, bj.WATCH_B_VFO)
    r._mmap[0x8020] = bytes([1, 4, 7, 0, 0, 0, 0, 0])            # a vaga do B tem a C: 147.00000
    aj = {s.get_name(): s for s in r.get_settings().walk()}
    bloco_b = r.get_settings()['abblock']['bchannel']
    confere('10n. com a C no lado B: o VFO C mostra o do lado B, o VFO B o guardado, so leitura',
            (str(aj['vfoc_freq'].value), str(aj['vfob_saved_freq'].value), str(aj['vfob_saved_mode'].value),
             bloco_b.keys()[:3], aj['vfoc_freq'].value.get_mutable(), aj['vfob_saved_freq'].value.get_mutable(),
             aj['watch_c_mode'].value.get_mutable(), 'vfo.b.freq' in aj),
            ('147.000000', '146.520000', 'VFO Mode', ['vfob_saved_mode', 'settings_mdfb', 'vfob_saved_freq'],
             False, False, False, False))
    # 10o. a Voice Privacy do VFO A: a da base nao tinha como ser gravada (sem callback, nao MemSetting)
    r = bj.BJ8300(imagem())
    aj = r.get_settings()
    {s.get_name(): s for s in aj.walk()}['vfo_a_voicepri'].value.set_value('ENCRY2')
    aj.apply_to(r._memobj)
    confere('10o. Voice Privacy do VFO A gravada', int(r._memobj.vfo.a.voicepri), 2)
    # 10p. MIC GAIN (firmware v49): o byte 0x9033 do bloco de ajustes, logo depois do Noise reduction
    r = bj.BJ8300(imagem())
    aj = r.get_settings()
    spec = aj['spec']
    mic = {s.get_name(): s for s in aj.walk()}['settings_unknown_9033']
    confere('10p. Mic Gain no Specific, logo depois do Noise reduction, em HIGH com o byte em 0',
            (spec.keys()[spec.keys().index('settings_unknown_9032') + 1], mic.get_shortname(), str(mic.value)),
            ('settings_unknown_9033', 'Mic Gain', 'HIGH'))
    mic.value.set_value('LOW')
    aj.apply_to(r._memobj)
    confere('10q. LOW grava 2 em 0x9033', r._mmap[0x9033], bytes([2]))
    r._mmap[0x9033] = bytes([7])
    confere('10r. valor fora da faixa le HIGH, como o firmware',
            str(ajuste_de(r, 'settings_unknown_9033').value), 'HIGH')
    # 10s. TX-A/B/C (firmware v50): o TX-A/B de fabrica (0x9013, 2 bits) com o C, logo depois do TTR
    r = bj.BJ8300(imagem())
    aj = r.get_settings()
    abc = aj['abblock']
    tx = {s.get_name(): s for s in aj.walk()}['settings_dualtx']
    confere('10s. TX-A/B/C no VFO A/B/C Channel, logo depois do TTR, com as 4 opcoes',
            (abc.keys()[abc.keys().index('settings_tdr') + 1], tx.get_shortname(),
             list(tx.value.get_options())),
            ('settings_dualtx', 'TX-A/B/C', ['Off', 'A', 'B', 'C']))
    r._mmap[0x9013] = bytes([0xFC])                 # os 6 bits de cima, do driver de base, ficam
    aj = r.get_settings()
    {s.get_name(): s for s in aj.walk()}['settings_dualtx'].value.set_value('C')
    aj.apply_to(r._memobj)
    confere('10t. C grava 3 nos 2 bits de 0x9013, sem mexer nos outros', r._mmap[0x9013], bytes([0xFF]))
    confere('10u. e le de volta C', str(ajuste_de(r, 'settings_dualtx').value), 'C')
    # 10v. a pasta MIC e o CALIBRATION (firmware v59): 0x9035 o compressor, 0x9036 o AGC e 0x9037..0x9038
    # o cristal, em 10 Hz com sinal, logo depois do Mic Gain; o que o reset de fabrica deixa (197, 181 e
    # 0xC4C0) le Off, On e 0, como o firmware
    r = bj.BJ8300(imagem())
    r._mmap[0x9035] = bytes([197, 181, 0xC0, 0xC4])
    aj = r.get_settings()
    spec = aj['spec']
    i = spec.keys().index('settings_unknown_9033')
    confere('10v. Compressor, Mic AGC e Crystal fine depois do Mic Gain; o lixo do reset le Off, On e 0',
            (spec.keys()[i + 1:i + 4], str(ajuste_de(r, 'settings_unknown_9035').value),
             str(ajuste_de(r, 'settings_unknown_9036').value), int(ajuste_de(r, 'settings_unknown_9037').value)),
            (['settings_unknown_9035', 'settings_unknown_9036', 'settings_unknown_9037'], 'Off', 'On', 0))
    aj = r.get_settings()
    s = {x.get_name(): x for x in aj.walk()}
    s['settings_unknown_9035'].value.set_value('High')
    s['settings_unknown_9036'].value.set_value('Off')
    s['settings_unknown_9037'].value.set_value(-300)
    aj.apply_to(r._memobj)
    confere('10w. High grava 3, Off grava 1 e -300 Hz grava -30 (0xFFE2)', r._mmap[0x9035:0x9039],
            bytes([3, 1, 0xE2, 0xFF]))
    confere('10x. e le de volta High, Off e -300', (str(ajuste_de(r, 'settings_unknown_9035').value),
            str(ajuste_de(r, 'settings_unknown_9036').value), int(ajuste_de(r, 'settings_unknown_9037').value)),
            ('High', 'Off', -300))
    # 10y. BRIGHTNESS (firmware v61): 0x9039, depois do Crystal fine; o 163 do reset de fabrica le 100%
    r = bj.BJ8300(imagem())
    r._mmap[0x9039] = bytes([163])
    aj = r.get_settings()
    spec = aj['spec']
    s = {x.get_name(): x for x in aj.walk()}
    confere('10y. Brightness depois do Crystal fine; o 163 do reset le 100%',
            (spec.keys()[spec.keys().index('settings_unknown_9037') + 1], str(s['settings_unknown_9039'].value)),
            ('settings_unknown_9039', '100%'))
    s['settings_unknown_9039'].value.set_value('20%')
    aj.apply_to(r._memobj)
    confere('10z. 20% grava 3 em 0x9039', r._mmap[0x9039], bytes([3]))

    print()
    print('FALHAS: %d' % len(falhas) if falhas else 'tudo certo')
    return 1 if falhas else 0


if __name__ == '__main__':
    sys.exit(main())
