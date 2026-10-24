# Driver do CHIRP para o Baojie/Bajeton BJ-8300 com a feature de BANCOS DE CANAIS.
#
# Derivado do chirp/drivers/radtel_rt900.py (Copyright 2025, 2026 Fred Trimble; 2024 Pavel
# Moravec, OK2MOP; 2023 Jim Unroe), que ja trata o BJ-8300 pela familia RT-920/BJ7800: canais,
# ajustes e protocolo do cabo vem todos dele, sem copia.
#
# Este programa e software livre: voce pode redistribui-lo e/ou modifica-lo sob os termos da
# GNU General Public License, versao 2 ou posterior, publicada pela Free Software Foundation.
# Distribuido na esperanca de ser util, mas SEM NENHUMA GARANTIA.
#
# ---------------------------------------------------------------------------------------------
# O QUE ESTE MODULO ACRESCENTA
#
# O firmware modificado do BJ-8300 (BJ8300_BANK_v12.anc em diante) guarda 16 bancos de canais num
# chip de dados, FORA dos 64 KB que o protocolo do cabo alcanca com endereco de 16 bits:
#
#   0xC1000            cabecalho: marca "BNK3", mascara de bancos ligados, banco escolhido, soma
#   0xC2000 + n*0x1000 banco n: marca "BCO1", nome de 12 caracteres, mapa de 120 bytes (960 canais
#                      a um bit cada) e soma - um setor de 4 KB por banco
#
# Para chegar la, o firmware entende um comando novo que cabe nos mesmos 4 bytes do original: o
# **bit 7 do tamanho** ligado quer dizer "endereco de 24 bits", e os outros 7 bits sao o terceiro
# byte do endereco; o bloco e sempre de 64 bytes. Com o bit 7 em zero nada muda - e por isso que o
# driver de fabrica continua funcionando neste radio.
#
# A gravacao estendida so e aceita pelo firmware dentro de 0xC1000..0xD1FFF (cabecalho + os 16
# bancos) e so apaga setor que esteja vazio ou que ja seja dele: nenhum endereco de canal, de
# calibracao ou de imagem de abertura corre risco por este caminho.
#
# Na imagem do CHIRP (.img) a area dos bancos entra logo depois da parte de fabrica, em 0xF250,
# como 17 setores de 192 bytes - os mesmos tres blocos de 64 bytes que vao e voltam pelo cabo.
# Assim os bancos sobrevivem a salvar e reabrir o .img, como qualquer outro dado do radio.
# ---------------------------------------------------------------------------------------------

import logging
import struct

from chirp import chirp_common, directory, errors, memmap
from chirp.drivers import radtel_rt900
from chirp.settings import RadioSetting, RadioSettingGroup, \
    RadioSettingValueBoolean, RadioSettingValueList

LOG = logging.getLogger(__name__)

# --- area dos bancos no chip de dados (os mesmos valores do src/bancos.c do firmware) ---
SETOR_CAB = 0x0C1000
SETOR_BANCO = 0x0C2000
NUM_BANCOS = 16
TAM_BLOCO = 0x40                               # o cabo so move blocos de 64 bytes
BLOCOS_POR_SETOR = 3                           # 192 bytes cobrem o registro de 138
TAM_SETOR = TAM_BLOCO * BLOCOS_POR_SETOR
TAM_BLOB = (NUM_BANCOS + 1) * TAM_SETOR        # cabecalho + 16 bancos = 3264 bytes

TAM_NOME = 12
TAM_MAPA = 120                                 # 960 canais, um bit cada
TAM_CABECA = 10
TAM_REGISTRO = 4 + TAM_NOME + TAM_MAPA + 2     # 138
MARCA_CAB = b'BNK3'
MARCA_BANCO = b'BCO1'

BLOB_BASE = 0xF250                             # onde a area dos bancos entra no .img
NUM_CANAIS = 960
ACK = b'\x06'


def _soma(dados):
    """Checksum do firmware (soma_de em bancos.c): comeca em 0x1234 e desloca a cada byte."""
    soma = 0x1234
    for b in dados:
        soma = ((soma << 1) + b) & 0xFFFF
    return soma


def _comando(letra, endereco):
    """Pacote estendido: 4 bytes, com o bit 7 do tamanho ligando o endereco de 24 bits."""
    return struct.pack('>cHB', letra, endereco & 0xFFFF, 0x80 | (endereco >> 16))


def _enderecos():
    """Endereco no radio de cada um dos 17 setores: o cabecalho e os 16 bancos."""
    return [SETOR_CAB] + [SETOR_BANCO + b * 0x1000 for b in range(NUM_BANCOS)]


class _Bancos(object):
    """Le e escreve a area dos bancos dentro da imagem, mantendo marca e soma como o firmware.

    Registro invalido (marca errada ou soma que nao fecha) e tratado como banco vazio, nunca como
    lixo: e o mesmo criterio do firmware, entao os dois lados enxergam a mesma coisa.
    """

    def __init__(self, radio):
        self._radio = radio

    # ---- acesso cru a imagem ----
    def _le(self, posicao, tamanho):
        return bytes(self._radio._mmap[BLOB_BASE + posicao:
                                       BLOB_BASE + posicao + tamanho])

    def _grava(self, posicao, dados):
        self._radio._mmap[BLOB_BASE + posicao] = bytes(dados)

    @staticmethod
    def _onde(banco):
        """Deslocamento do setor do banco dentro do blob (o cabecalho ocupa o primeiro)."""
        return (banco + 1) * TAM_SETOR

    # ---- cabecalho ----
    def cabeca(self):
        d = self._le(0, TAM_CABECA)
        if d[:4] == MARCA_CAB and _soma(d[:8]) == struct.unpack('<H', d[8:10])[0]:
            return bytearray(d)
        return bytearray(MARCA_CAB + b'\x00' * (TAM_CABECA - 4))

    def grava_cabeca(self, c):
        c[8:10] = struct.pack('<H', _soma(bytes(c[:8])))
        self._grava(0, bytes(c) + b'\xff' * (TAM_SETOR - TAM_CABECA))

    def ativos(self):
        return struct.unpack('<H', bytes(self.cabeca()[4:6]))[0]

    def liga(self, banco, ligado):
        c = self.cabeca()
        ativos = struct.unpack('<H', bytes(c[4:6]))[0]
        bit = 1 << banco
        c[4:6] = struct.pack('<H', ativos | bit if ligado else ativos & ~bit)
        self.grava_cabeca(c)

    def escolhido(self):
        return self.cabeca()[6]

    def escolhe(self, banco):
        c = self.cabeca()
        c[6] = banco
        self.grava_cabeca(c)

    # ---- registros de banco ----
    def registro(self, banco):
        d = self._le(self._onde(banco), TAM_REGISTRO)
        if d[:4] == MARCA_BANCO and _soma(d[:136]) == struct.unpack('<H', d[136:])[0]:
            return bytearray(d)
        return bytearray(MARCA_BANCO + b'\x00' * (TAM_REGISTRO - 4))

    def grava_registro(self, banco, reg):
        reg[136:138] = struct.pack('<H', _soma(bytes(reg[:136])))
        # o resto do setor fica apagado, do jeito que o firmware deixa
        self._grava(self._onde(banco), bytes(reg) + b'\xff' * (TAM_SETOR - TAM_REGISTRO))

    def nome(self, banco):
        nome = bytes(self.registro(banco)[4:4 + TAM_NOME])
        return nome.split(b'\x00')[0].decode('ascii', 'ignore').rstrip()

    def poe_nome(self, banco, nome):
        reg = self.registro(banco)
        reg[4:4 + TAM_NOME] = nome.encode('ascii', 'ignore')[:TAM_NOME].ljust(TAM_NOME, b'\x00')
        self.grava_registro(banco, reg)

    def tem(self, banco, canal):
        return bool(self.registro(banco)[16 + (canal >> 3)] >> (canal & 7) & 1)

    def poe(self, banco, canal, dentro):
        reg = self.registro(banco)
        bit = 1 << (canal & 7)
        if dentro:
            reg[16 + (canal >> 3)] |= bit
        else:
            reg[16 + (canal >> 3)] &= ~bit & 0xFF
        self.grava_registro(banco, reg)

    def canais(self, banco):
        mapa = self.registro(banco)[16:16 + TAM_MAPA]
        return [c for c in range(NUM_CANAIS) if mapa[c >> 3] >> (c & 7) & 1]

    # ---- a imagem inteira, do jeito que vai e volta pelo cabo ----
    def setores(self):
        """Os 17 setores da imagem, na ordem de _enderecos(): cabecalho e os 16 bancos."""
        return [self._le(i * TAM_SETOR, TAM_SETOR) for i in range(NUM_BANCOS + 1)]


class BancoBJ8300(chirp_common.NamedBank):
    """Um dos 16 bancos: o nome mora no proprio setor do banco, no radio."""

    def get_name(self):
        nome = self._model._bancos.nome(self._index - 1)
        return nome or 'BANK %i' % self._index

    def set_name(self, name):
        self._model._bancos.poe_nome(self._index - 1, name)


class ModeloBancosBJ8300(chirp_common.BankModel):
    """16 bancos de verdade: um canal pode estar em quantos bancos quiser ao mesmo tempo.

    (A classe base do CHIRP e pensada para "zero ou um banco por canal", mas o editor de bancos
    monta uma coluna de marcar por banco e aceita a lista inteira que get_memory_mappings devolve,
    entao a pertinencia multipla funciona na interface.)
    """

    def __init__(self, radio, name='Banks'):
        chirp_common.BankModel.__init__(self, radio, name)
        self._bancos = _Bancos(radio)
        self._lista = [BancoBJ8300(self, i + 1, 'BANK %i' % (i + 1))
                       for i in range(NUM_BANCOS)]

    def get_num_mappings(self):
        return NUM_BANCOS

    def get_mappings(self):
        return self._lista

    def add_memory_to_mapping(self, memory, mapping):
        self._bancos.poe(mapping.get_index() - 1, memory.number - 1, True)

    def remove_memory_from_mapping(self, memory, mapping):
        banco = mapping.get_index() - 1
        if not self._bancos.tem(banco, memory.number - 1):
            raise errors.InvalidDataError(
                'O canal %i nao esta no banco %s' % (memory.number, mapping.get_name()))
        self._bancos.poe(banco, memory.number - 1, False)

    def get_mapping_memories(self, mapping):
        return [self._radio.get_memory(c + 1)
                for c in self._bancos.canais(mapping.get_index() - 1)]

    def get_memory_mappings(self, memory):
        return [b for b in self._lista
                if self._bancos.tem(b.get_index() - 1, memory.number - 1)]


class _BancosPeloCabo(object):
    """A parte que fala com o radio: leitura e gravacao de 24 bits, e a conferencia do firmware.

    Mora num mixin porque vale igual para o radio e para a subdivisao VHF/UHF.
    """

    def _le_estendido(self, endereco):
        self.pipe.write(_comando(b'R', endereco))
        resposta = self.pipe.read(4 + TAM_BLOCO)
        if len(resposta) != 4 + TAM_BLOCO or resposta[0:3] != _comando(b'R', endereco)[0:3]:
            raise errors.RadioError('O radio nao respondeu a leitura de %06X' % endereco)
        return resposta[4:]

    def _grava_estendido(self, endereco, dados):
        if len(dados) != TAM_BLOCO:
            raise errors.RadioError('bloco de %i bytes (tem que ser %i)' % (len(dados), TAM_BLOCO))
        self.pipe.write(_comando(b'W', endereco) + bytes(dados))
        if self.pipe.read(1) != ACK:
            raise errors.RadioError('O radio nao confirmou a gravacao em %06X' % endereco)

    def _confere_firmware(self):
        """O firmware dos bancos entende 24 bits; o de fabrica trunca em 16 e devolve 0x0000.

        Esta conferencia e obrigatoria antes de qualquer gravacao: num firmware de fabrica, um
        'W' estendido viraria escrita em cima dos CANAIS (0xC1000 truncado da 0x1000).
        """
        if self._le_estendido(0x0C0000) == radtel_rt900._read_block(self, 0x0000, TAM_BLOCO):
            raise errors.RadioError(
                'Este radio esta com firmware sem a area de bancos (o endereco de 24 bits nao '
                'passa). Grave o firmware BJ8300_BANK (v12 ou mais novo), ou use o driver '
                'Radtel RT-920 para mexer so nos canais.')

    def _baixa_bancos(self):
        """Le os 17 setores (cabecalho + 16 bancos) e devolve o blob que vai para o .img."""
        blob = b''
        status = chirp_common.Status()
        status.msg = 'Lendo os bancos'
        status.max = NUM_BANCOS + 1
        for i, endereco in enumerate(_enderecos()):
            status.cur = i
            self.status_fn(status)
            for bloco in range(BLOCOS_POR_SETOR):
                blob += self._le_estendido(endereco + bloco * TAM_BLOCO)
        return blob

    def _sobe_bancos(self):
        """Grava no radio so os setores que estao diferentes - o primeiro bloco apaga o setor."""
        setores = _Bancos(self).setores()
        status = chirp_common.Status()
        status.msg = 'Gravando os bancos'
        status.max = NUM_BANCOS + 1
        for i, (endereco, dados) in enumerate(zip(_enderecos(), setores)):
            status.cur = i
            self.status_fn(status)
            no_radio = b''.join(self._le_estendido(endereco + b * TAM_BLOCO)
                                for b in range(BLOCOS_POR_SETOR))
            if no_radio == dados:
                continue
            for bloco in range(BLOCOS_POR_SETOR):
                self._grava_estendido(endereco + bloco * TAM_BLOCO,
                                      dados[bloco * TAM_BLOCO:(bloco + 1) * TAM_BLOCO])


@directory.register
class BJ8300(_BancosPeloCabo, radtel_rt900.BJ7800):
    """Baojie BJ-8300 com o firmware dos bancos de canais."""
    VENDOR = 'Baojie'
    MODEL = 'BJ8300_v1.0TEMPRA'        # nome proprio: so serve com o firmware modificado
    VARIANT = ''

    # Canais, ajustes, faixas e protocolo sao os mesmos do BJ7800 (960 canais de 32 bytes, 15
    # zonas de 64). _banks continua sendo a contagem de ZONAS de fabrica, que o formato bitwise
    # usa; os nossos bancos sao outra coisa e nao passam por ali.

    def get_bank_model(self):
        return ModeloBancosBJ8300(self, 'Bancos')

    def get_features(self):
        rf = super().get_features()
        rf.has_bank = True
        rf.has_bank_names = True
        return rf

    def get_sub_devices(self):
        return [BJ8300VhfUhf(self._mmap),
                radtel_rt900.RT920FM(self._mmap),
                radtel_rt900.RT920AM(self._mmap),
                radtel_rt900.RT920HF(self._mmap)]

    @classmethod
    def get_prompts(cls):
        rp = super().get_prompts()
        rp.experimental = (
            'Driver do BJ-8300 com os 16 bancos do firmware modificado (BJ8300_BANK v12+).\n'
            '\n'
            'Os bancos ficam na aba Banks: uma coluna por banco, e o mesmo canal pode estar\n'
            'em quantos bancos quiser. Ligar e desligar banco fica em Settings > Bancos.\n'
            '\n'
            'Guarde uma copia do primeiro download antes de alterar qualquer coisa.')
        rp.pre_upload = (
            'A gravacao refaz os 16 setores de banco do radio a partir desta imagem: banco que\n'
            'estiver vazio aqui fica vazio no radio. Se a imagem veio do driver RT-920 (que nao\n'
            'le bancos), todos os bancos do radio serao apagados.\n'
            '\n'
            'Depois de gravar, DESLIGUE E LIGUE o radio: o filtro dos bancos e montado na\n'
            'partida.')
        return rp

    def process_mmap(self):
        # Imagem que ainda nao tem a area dos bancos (p.ex. salva pelo driver RT-920) ganha o
        # espaco apagado: assim o editor de bancos abre vazio em vez de estourar.
        if self._mmap is not None and len(self._mmap) < BLOB_BASE + TAM_BLOB:
            falta = BLOB_BASE + TAM_BLOB - len(self._mmap)
            self._mmap = memmap.MemoryMapBytes(self._mmap.get_packed() + b'\xff' * falta)
        super().process_mmap()

    def sync_in(self):
        super().sync_in()                      # canais e ajustes, pelo caminho de fabrica
        try:
            radtel_rt900._enter_programming_mode(self)
            self._confere_firmware()
            blob = self._baixa_bancos()
        except errors.RadioError:
            raise
        except Exception:
            LOG.exception('Erro lendo a area dos bancos')
            raise errors.RadioError('Erro lendo a area dos bancos')
        finally:
            radtel_rt900._exit_programming_mode(self)
        self._mmap = memmap.MemoryMapBytes(self._mmap.get_packed()[:BLOB_BASE] + blob)
        self.process_mmap()

    def sync_out(self):
        super().sync_out()                     # canais e ajustes, pelo caminho de fabrica
        try:
            radtel_rt900._enter_programming_mode(self)
            self._confere_firmware()
            self._sobe_bancos()
        except errors.RadioError:
            raise
        except Exception:
            LOG.exception('Erro gravando a area dos bancos')
            raise errors.RadioError('Erro gravando a area dos bancos')
        finally:
            radtel_rt900._exit_programming_mode(self)

    def get_settings(self):
        ajustes = super().get_settings()
        bancos = _Bancos(self)
        ativos = bancos.ativos()
        grupo = RadioSettingGroup('bancos', 'Bancos')

        for i in range(NUM_BANCOS):
            nome = bancos.nome(i)
            rotulo = 'Banco %i%s ligado' % (i + 1, ' (%s)' % nome if nome else '')
            rs = RadioSetting('banco_on_%i' % i, rotulo,
                              RadioSettingValueBoolean(bool(ativos >> i & 1)))
            rs.set_doc('Com um ou mais bancos ligados, o sobe/desce e a busca so veem os canais '
                       'desses bancos. Sem nenhum ligado, o radio funciona como de fabrica.')
            rs.set_apply_callback(self._aplica_ligado, i)
            grupo.append(rs)

        escolhido = bancos.escolhido()
        opcoes = ['%i%s' % (i + 1, ' (%s)' % bancos.nome(i) if bancos.nome(i) else '')
                  for i in range(NUM_BANCOS)]
        rs = RadioSetting('banco_escolhido', 'Banco escolhido no menu do radio',
                          RadioSettingValueList(
                              opcoes,
                              current_index=escolhido if escolhido < NUM_BANCOS else 0))
        rs.set_doc('E o banco em que os itens BANK NAME e BANK CH do radio mexem.')
        rs.set_apply_callback(self._aplica_escolhido)
        grupo.append(rs)

        ajustes.append(grupo)
        return ajustes

    def _aplica_ligado(self, setting, banco):
        _Bancos(self).liga(banco, bool(setting.value))

    def _aplica_escolhido(self, setting):
        _Bancos(self).escolhe(int(setting.value))      # RadioSettingValueList da o indice


class BJ8300VhfUhf(BJ8300):
    """Subdivisao VHF/UHF: e aqui que ficam os 960 canais e, com eles, os bancos."""
    VENDOR = 'Baojie'
    MODEL = 'BJ8300_v1.0TEMPRA'
    VARIANT = 'VHF/UHF'
