"""Testa o miolo do driver do CHIRP (chirp/bj8300.py) sem precisar do CHIRP instalado.

Uso: python -I testes/teste_bj8300.py

O CHIRP entra como stub: so as classes e funcoes que o driver toca. O que importa aqui e a parte
que nao depende do CHIRP e que, se estiver errada, estraga banco no radio:

  1. a soma que o driver calcula bate com a que o RADIO gravou (0x753C no cabecalho lido em
     2026-10-24, com os bancos 1 e 3 ligados e o banco 3 escolhido);
  2. ler nome, bancos ligados e canais de uma imagem montada com os bytes reais;
  3. marcar e desmarcar canal, inclusive o mesmo canal em dois bancos (o ponto da feature);
  4. renomear e ligar/desligar mantendo marca e soma validas;
  5. o pacote do comando estendido sai igual ao que foi validado no radio (52 10 00 8C).
"""
import os
import struct
import sys
import types

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ---------------------------------------------------------------- stub do CHIRP
def _stub_chirp():
    chirp = types.ModuleType('chirp')
    chirp.__path__ = []

    memmap = types.ModuleType('chirp.memmap')

    class MemoryMapBytes(object):
        """Mesma semantica do original: lista de bytes, fatia devolve bytes, atribuir sobrescreve."""
        def __init__(self, data):
            self._data = list(data)

        def get_packed(self):
            return bytes(self._data)

        def __len__(self):
            return len(self._data)

        def __getitem__(self, pos):
            if isinstance(pos, slice):
                return bytes(self._data[pos])
            return bytes([self._data[pos]])

        def __setitem__(self, pos, value):
            self._data[pos:pos + len(value)] = list(value)

    memmap.MemoryMapBytes = MemoryMapBytes

    errors = types.ModuleType('chirp.errors')

    class RadioError(Exception):
        pass

    class InvalidDataError(Exception):
        pass

    errors.RadioError = RadioError
    bitwise = types.ModuleType('chirp.bitwise')     # o VFO C so e lido quando a imagem e montada
    errors.InvalidDataError = InvalidDataError

    chirp_common = types.ModuleType('chirp.chirp_common')

    class MemoryMapping(object):
        def __init__(self, model, index, name):
            self._model, self._index, self._name = model, index, name

        def get_index(self):
            return self._index

        def get_name(self):
            return self._name

        def __eq__(self, other):
            return self.get_index() == other.get_index()

    class MappingModel(object):
        def __init__(self, radio, name):
            self._radio, self._name = radio, name

    class Memory(object):
        def __init__(self, number):
            self.number = number
            self.empty = False

    chirp_common.MemoryMapping = MemoryMapping
    chirp_common.MappingModel = MappingModel
    chirp_common.Bank = type('Bank', (MemoryMapping,), {})
    chirp_common.NamedBank = type('NamedBank', (chirp_common.Bank,), {})
    chirp_common.BankModel = type('BankModel', (MappingModel,), {})
    chirp_common.Status = type('Status', (object,), {})
    chirp_common.Memory = Memory

    directory = types.ModuleType('chirp.directory')
    directory.register = lambda cls: cls

    settings = types.ModuleType('chirp.settings')

    class RadioSettingGroup(list):
        def __init__(self, name, shortname, *elementos):
            super().__init__(elementos)
            self.name, self.shortname = name, shortname

        def append(self, item):
            super().append(item)

    class RadioSetting(RadioSettingGroup):
        def __init__(self, name, shortname, valor):
            super().__init__(name, shortname)
            self.value = valor
            self._cb = None

        def set_doc(self, _):
            pass

        def set_apply_callback(self, cb, *args):
            self._cb = lambda: cb(self, *args)

        def run_apply_callback(self):
            return self._cb()

    class RadioSettingValueBoolean(object):
        def __init__(self, v):
            self.v = bool(v)

        def __bool__(self):
            return self.v

    class RadioSettingValueList(object):
        def __init__(self, options, current=None, current_index=0):
            self.options, self.indice = list(options), current_index

        def __int__(self):
            return self.indice

    class MemSetting(RadioSetting):
        def __init__(self, path, shortname, valor):
            super().__init__(path.replace('.', '_'), shortname, valor)

    class RadioSettingSubGroup(RadioSettingGroup):
        pass

    class RadioSettingValueString(object):
        def __init__(self, minimo, maximo, atual, autopad=True):
            self.v = atual

    class RadioSettingValueMap(object):
        def __init__(self, mapa, atual):
            self.v = atual

    class InvalidValueError(Exception):
        pass

    class RadioSettingValueInteger(object):
        def __init__(self, minimo, maximo, atual):
            self.v = atual

    settings.MemSetting = MemSetting
    settings.RadioSetting = RadioSetting
    settings.RadioSettingGroup = RadioSettingGroup
    settings.RadioSettingSubGroup = RadioSettingSubGroup
    settings.RadioSettingValueBoolean = RadioSettingValueBoolean
    settings.RadioSettingValueList = RadioSettingValueList
    settings.RadioSettingValueString = RadioSettingValueString
    settings.RadioSettingValueInteger = RadioSettingValueInteger
    settings.RadioSettingValueMap = RadioSettingValueMap
    settings.InvalidValueError = InvalidValueError

    drivers = types.ModuleType('chirp.drivers')
    drivers.__path__ = []
    rt900 = types.ModuleType('chirp.drivers.radtel_rt900')

    class BJ7800(object):
        VENDOR, MODEL, VARIANT = 'Bajeton', 'BJ7800', ''
        _upper, _banks = 960, 15

        def __init__(self, mmap=None):
            self._mmap = mmap
            self.pipe = None

        def get_memory(self, numero):
            return Memory(numero)

        def status_fn(self, _):
            pass

    rt900.BJ7800 = BJ7800
    for nome in ('RT920FM', 'RT920AM', 'RT920HF'):
        setattr(rt900, nome, type(nome, (BJ7800,), {}))
    rt900._read_block = lambda radio, addr, size: b'\x00' * size
    rt900._enter_programming_mode = lambda radio: None
    rt900._exit_programming_mode = lambda radio: None

    chirp.bitwise = bitwise
    for nome, mod in (('chirp', chirp), ('chirp.memmap', memmap), ('chirp.errors', errors),
                      ('chirp.bitwise', bitwise),
                      ('chirp.chirp_common', chirp_common), ('chirp.directory', directory),
                      ('chirp.settings', settings), ('chirp.drivers', drivers),
                      ('chirp.drivers.radtel_rt900', rt900)):
        sys.modules[nome] = mod
    return memmap.MemoryMapBytes


MemoryMapBytes = _stub_chirp()

# Carrega o driver do mesmo jeito que o CHIRP carrega (wxui/main.py: load_module), inclusive o
# nome com ponto - e assim que se prova que "bj8300_v2.7TEMPRA.py" nao quebra o import.
import importlib.util                                           # noqa: E402

CAMINHO = os.path.join(RAIZ, 'chirp', 'bj8300_v2.7TEMPRA.py')
_nome = 'chirp.loaded.%s' % os.path.splitext(os.path.basename(CAMINHO))[0]
_spec = importlib.util.spec_from_file_location(_nome, CAMINHO)
bj8300 = importlib.util.module_from_spec(_spec)
sys.modules[_nome] = bj8300
_spec.loader.exec_module(bj8300)
print('modulo carregado como %s' % _nome)


# ---------------------------------------------------------------- imagem de teste
def registro(nome, canais):
    """Monta o registro de um banco como o firmware grava (marca, nome, mapa, soma)."""
    reg = bytearray(bj8300.BANK_MAGIC + b'\x00' * (bj8300.RECORD_LEN - 4))
    reg[4:4 + len(nome)] = nome.encode()
    for c in canais:
        reg[16 + (c >> 3)] |= 1 << (c & 7)
    reg[136:138] = struct.pack('<H', bj8300._checksum(bytes(reg[:136])))
    return bytes(reg)


def imagem_do_radio():
    """A imagem que o radio tinha em 2026-10-24: bancos 1 e 3 ligados, banco 3 escolhido,
    banco 1 = "test" com o canal 43, banco 3 = "bnk" com o canal 42."""
    cab = bytearray(bj8300.HEADER_MAGIC + struct.pack('<HBB', 0x0005, 2, 0) + b'\x00\x00')
    cab[8:10] = struct.pack('<H', bj8300._checksum(bytes(cab[:8])))
    setores = [bytes(cab).ljust(bj8300.SECTOR_SIZE, b'\xff')]
    conteudo = {0: ('test', [42]), 2: ('bnk', [41])}
    for b in range(bj8300.NUM_BANKS):
        if b in conteudo:
            nome, canais = conteudo[b]
            setores.append(registro(nome, canais).ljust(bj8300.SECTOR_SIZE, b'\xff'))
        else:
            setores.append(b'\xff' * bj8300.SECTOR_SIZE)
    # os canais 42 e 43 (indices 41 e 42) gravados, como no radio: e o que conta para "ligado"
    fabrica = bytearray(b'\xff' * bj8300.BLOB_BASE)
    for c in (41, 42):
        fabrica[c * bj8300.CHANNEL_SIZE:c * bj8300.CHANNEL_SIZE + 4] = b'\x00\x20\x65\x14'
    return MemoryMapBytes(bytes(fabrica) + b''.join(setores)), bytes(cab)


class RadioFalso(bj8300.BJ8300):
    def __init__(self, mmap):
        self._mmap = mmap
        self.pipe = None


# ---------------------------------------------------------------- testes
def main():
    falhas = []

    def confere(nome, obtido, esperado):
        ok = obtido == esperado
        print('%-62s %s' % (nome, 'ok' if ok else 'FALHOU (%r != %r)' % (obtido, esperado)))
        if not ok:
            falhas.append(nome)

    mmap, cab = imagem_do_radio()
    radio = RadioFalso(mmap)
    bancos = bj8300._BankStore(radio)

    # 1. a soma do driver bate com a que o RADIO gravou
    confere('1. soma do cabecalho igual a do radio (0x753C)',
            struct.unpack('<H', cab[8:10])[0], 0x753C)
    confere('1b. soma de banco com nome e um canal (o radio gravou 0x0000)',
            struct.unpack('<H', registro('test', [42])[136:138])[0], 0x0000)

    # 2. leitura
    confere('2. bancos ligados', bancos.enabled_mask(), 0x0005)
    confere('2b. banco escolhido', bancos.selected(), 2)
    confere('2c. nome do banco 1', bancos.name(0), 'test')
    confere('2d. nome do banco 3', bancos.name(2), 'bnk')
    confere('2e. banco sem nada abre vazio', bancos.name(5), '')
    confere('2f. canais do banco 1 (canal 43 = indice 42)', bancos.channels(0), [42])
    confere('2g. canais do banco 3', bancos.channels(2), [41])

    # 3. o ponto da feature: o mesmo canal em mais de um banco
    modelo = bj8300.BJ8300BankModel(radio, 'Banks')
    mem43 = sys.modules['chirp.chirp_common'].Memory(43)
    confere('3. canal 43 aparece so no banco 1',
            [b.get_index() for b in modelo.get_memory_mappings(mem43)], [1])
    modelo.add_memory_to_mapping(mem43, modelo.get_mappings()[2])
    confere('3b. depois de marcar, canal 43 esta nos bancos 1 e 3',
            [b.get_index() for b in modelo.get_memory_mappings(mem43)], [1, 3])
    confere('3c. o banco 3 manteve o canal que ja tinha', bancos.channels(2), [41, 42])
    confere('3d. o nome do banco 3 sobreviveu', bancos.name(2), 'bnk')
    modelo.remove_memory_from_mapping(mem43, modelo.get_mappings()[0])
    confere('3e. depois de desmarcar, canal 43 so no banco 3',
            [b.get_index() for b in modelo.get_memory_mappings(mem43)], [3])
    try:
        modelo.remove_memory_from_mapping(mem43, modelo.get_mappings()[0])
        confere('3f. desmarcar o que nao esta marcado da erro', 'nao deu erro', 'erro')
    except Exception as e:
        confere('3f. desmarcar o que nao esta marcado da erro', type(e).__name__,
                'InvalidDataError')
    # o banco 1 perdeu o ultimo canal: se desligou sozinho; o 3 (com 42 e 43 gravados) continua
    confere('3g. banco que fica sem canal se desliga sozinho', bancos.enabled_mask(), 0x0004)
    confere('3h. contagem de canais gravados (banco 1 e banco 3)',
            (bancos.programmed_count(0), bancos.programmed_count(2)), (0, 2))

    # 4. nome e liga/desliga continuam com marca e soma validas
    modelo.get_mappings()[4].set_name('BANDA 2M')
    confere('4. nome gravado volta certo', bancos.name(4), 'BANDA 2M')
    confere('4b. nome longo e cortado em 12', (bancos.set_name(4, 'NOME MUITO COMPRIDO'),
                                               bancos.name(4))[1], 'NOME MUITO C')
    bancos.set_enabled(4, True)
    bancos.set_enabled(0, False)
    confere('4c. ligou o 5 e desligou o 1', bancos.enabled_mask(), 0x0014)
    bancos.select(4)
    confere('4d. banco escolhido gravado', bancos.selected(), 4)
    # a soma tem que fechar do jeito que o firmware confere
    cru = bytes(radio._mmap[bj8300.BLOB_BASE:bj8300.BLOB_BASE + bj8300.HEADER_LEN])
    confere('4e. soma do cabecalho fecha depois das mudancas',
            struct.unpack('<H', cru[8:10])[0], bj8300._checksum(cru[:8]))
    reg5 = bytes(radio._mmap[bj8300.BLOB_BASE + 5 * bj8300.SECTOR_SIZE:
                             bj8300.BLOB_BASE + 5 * bj8300.SECTOR_SIZE + bj8300.RECORD_LEN])
    confere('4f. soma do banco 5 fecha e a marca esta la',
            (reg5[:4], struct.unpack('<H', reg5[136:])[0]),
            (bj8300.BANK_MAGIC, bj8300._checksum(reg5[:136])))
    confere('4g. o resto do setor fica apagado, como o firmware deixa',
            bytes(radio._mmap[bj8300.BLOB_BASE + 5 * bj8300.SECTOR_SIZE + bj8300.RECORD_LEN:
                              bj8300.BLOB_BASE + 6 * bj8300.SECTOR_SIZE]),
            b'\xff' * (bj8300.SECTOR_SIZE - bj8300.RECORD_LEN))

    # 5. o pacote do cabo, igual ao validado no radio
    confere('5. comando de leitura de 0xC1000', bj8300._command(b'R', 0x0C1000),
            bytes([0x52, 0x10, 0x00, 0x8C]))
    confere('5b. comando de gravacao do setor do banco 16',
            bj8300._command(b'W', 0x0D1000), bytes([0x57, 0x10, 0x00, 0x8D]))
    confere('5c. os 17 enderecos', (bj8300._sector_addresses()[0], bj8300._sector_addresses()[1],
                                    bj8300._sector_addresses()[-1], len(bj8300._sector_addresses())),
            (0x0C1000, 0x0C2000, 0x0D1000, 17))

    # 6. registro estragado vira banco vazio, nunca lixo.
    #    Atencao: a soma do firmware (soma_de, que desloca a cada byte) da 0x0000 sempre que os
    #    ultimos 16 bytes do registro sao zero - ou seja, em todo banco sem canal acima do 833.
    #    Nos registros de banco quem protege de verdade e a marca; no cabecalho, de 8 bytes, a
    #    soma vale. Os dois casos entram aqui.
    radio._mmap[bj8300.BLOB_BASE + bj8300._BankStore._offset(2)] = b'XXXX'
    confere('6. marca estragada -> banco vazio', (bancos.name(2), bancos.channels(2)), ('', []))
    radio._mmap[bj8300.BLOB_BASE + 8] = b'\x01\x00'
    confere('6b. soma do cabecalho que nao fecha -> cabecalho zerado',
            (bancos.enabled_mask(), bancos.selected()), (0, 0))
    confere('6c. gravar depois disso conserta a soma',
            (bancos.set_enabled(1, True), bancos.enabled_mask())[1], 0x0002)

    print()
    print('FALHAS: %d' % len(falhas) if falhas else 'tudo certo')
    return 1 if falhas else 0


if __name__ == '__main__':
    sys.exit(main())
