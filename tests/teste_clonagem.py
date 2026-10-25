"""Clona com o CHIRP de verdade contra um radio SIMULADO, nos dois firmwares.

Uso: python -I tests/teste_clonagem.py
Precisa do pacote chirp instalado (ver teste_chirp_real.py); sem ele, sai avisando, sem falhar.

O teste_chirp_real.py confere o driver por dentro (modelo de bancos, ajustes, imagem). Este confere
a conversa pelo cabo: sync_in e sync_out inteiros - os da base RT-920/BJ7800 e os nossos - contra
um radio que responde do jeito que o firmware responde, lido no desmontado:

  - entrada em programacao: magic -> ACK, 'F' -> as duas identificacoes, a troca de cifra -> ACK;
  - 'R' end16 tam: devolve 'R' end16 tam + tam bytes, cifrados abaixo de 0xF000 (o tratador de
    fabrica responde com (tam + 4) & 0xFF bytes: 0x01E4BB6E..0x01E4BB78);
  - 'W' end16 tam + 64 bytes: grava (decifrando abaixo de 0xF000) e responde ACK;
  - FIRMWARE DOS BANCOS: tam com o bit 7 ligado = endereco de 24 bits, bloco de 64 bytes crus, e a
    resposta sai com tam = 0x40 (o gancho do cabo troca o byte); gravacao so em 0xC1000..0xD1FFF,
    apagando o setor quando o bloco o abre;
  - FIRMWARE DE FABRICA: o mesmo comando vira endereco truncado em 16 bits, e a resposta tem
    4 + tam bytes (mais de 128) - 64 de dados e o resto do que estiver no buffer;
  - 'E' (os dois firmwares): responde ACK (0x01E4C0D0 -> 0x01E416C4) e poe a sessao no estado 3,
    que so fecha numa volta do laco sem bytes chegando (0x01E4BA96). O que chega antes disso cai
    no estado 3 e e descartado - foi assim que a segunda sessao do driver antigo falhou no radio
    ("Radio returned unknown identification string"). Aqui o "depois" e o passa_tempo(): o
    CHIRP fecha a porta entre uma operacao e outra, e o radio fecha a sessao.

O que NAO da para testar aqui: a eletrica do cabo e o tempo de resposta do radio. A conversa em si
foi conferida a mao no radio (tools/cabo) nos dois sentidos.
"""
import os
import struct
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, 'tests'))

try:
    from chirp import errors
    from chirp.drivers import baofeng_uv17Pro, radtel_rt900
except ImportError:
    print('chirp nao instalado - nada a testar aqui (o teste_bj8300.py cobre o resto)')
    sys.exit(0)

import teste_chirp_real as real  # noqa: E402  (carrega e registra o driver uma vez so)

bj = real.bj
ACK = b'\x06'


class RadioSimulado(object):
    """O lado do radio no cabo. Guarda o espaco de 64 KB do protocolo e o chip de dados de 1 MB."""

    def __init__(self, bancos, espaco=None, chip=None):
        self.bancos = bancos                       # True = firmware BJ8300_BANK
        self.espaco = bytearray(espaco or b'\x00' * 0x10000)
        self.chip = bytearray(chip or b'\xff' * 0x100000)
        self.saida = bytearray()
        self.entrada = bytearray()
        self.estado = 'magic'
        self.gravacoes_estendidas = []             # enderecos de 24 bits gravados pelo 'W' estendido
        self.comandos_estendidos = 0
        self.gravacoes_normais = 0                 # 'W' de fabrica (canais e ajustes)
        self.sessoes = 0                           # PROGRAM aceitos

    def passa_tempo(self):
        """O CHIRP fechou a porta (a sobra no buffer do PC vai junto) e o radio fechou a sessao."""
        self.saida = bytearray()
        self.entrada = bytearray()
        if self.estado == 'depois_do_E':
            self.estado = 'magic'

    # ---- interface de "serial" que o CHIRP usa ----
    def write(self, dados):
        self.entrada += dados
        self._processa()

    def read(self, n):
        dados, self.saida = bytes(self.saida[:n]), self.saida[n:]
        return dados

    def log(self, *_):
        pass

    # ---- o protocolo ----
    def _processa(self):
        while True:
            e = self.entrada
            if self.estado == 'magic':
                magic = bj.BJ8300._magic
                if magic not in e:
                    return
                del e[:e.index(magic) + len(magic)]
                self.saida += ACK
                self.sessoes += 1
                self.estado = 'F'
            elif self.estado == 'depois_do_E':
                del e[:]                           # estado 3: descartado ate a sessao fechar
                return
            elif self.estado == 'F':
                if not e:
                    return
                if e[:1] == b'F':
                    del e[:1]
                    self.saida += bj.BJ8300._fingerprint[0] + bj.BJ8300._fingerprint2[0]
                    self.estado = 'cifra'
                else:
                    del e[:1]
            elif self.estado == 'cifra':
                cifra = bj.BJ8300._cryptsetup
                if len(e) < len(cifra):
                    return
                del e[:len(cifra)]
                self.saida += ACK
                self.estado = 'comandos'
            else:
                if not e:
                    return
                if e[:1] == b'E':
                    del e[:1]
                    self.saida += ACK
                    self.estado = 'depois_do_E'
                    continue
                if e[:1] == b'R':
                    if len(e) < 4:
                        return
                    self._le(bytes(e[:4]))
                    del e[:4]
                    continue
                if e[:1] == b'W':
                    if len(e) < 4 + 64:
                        return
                    self._grava(bytes(e[:4]), bytes(e[4:68]))
                    del e[:68]
                    continue
                raise AssertionError('comando inesperado no cabo: %r' % bytes(e[:8]))

    def _le(self, pacote):
        end16, tam = struct.unpack('>HB', pacote[1:4])
        if tam & 0x80 and self.bancos:
            self.comandos_estendidos += 1
            endereco = ((tam & 0x7F) << 16) | end16
            self.saida += pacote[:3] + b'\x40' + self.chip[endereco:endereco + 64]
            return
        dados = bytes(self.espaco[end16:end16 + 64])
        if end16 < 0xF000:
            dados = baofeng_uv17Pro._crypt(1, dados)
        resto = b'\x00' * max(0, tam - 64)              # o que mais estiver no buffer
        self.saida += pacote + (dados + resto)[:tam]

    def _grava(self, pacote, dados):
        end16, tam = struct.unpack('>HB', pacote[1:4])
        if tam & 0x80 and self.bancos:
            self.comandos_estendidos += 1
            endereco = ((tam & 0x7F) << 16) | end16
            if 0x0C1000 <= endereco < 0x0D2000:
                if endereco & 0xFFF == 0:
                    self.chip[endereco:endereco + 0x1000] = b'\xff' * 0x1000
                self.chip[endereco:endereco + 64] = dados
                self.gravacoes_estendidas.append(endereco)
            self.saida += ACK
            return
        if end16 < 0xF000:
            dados = baofeng_uv17Pro._crypt(1, dados)
        self.espaco[end16:end16 + 64] = dados          # o de fabrica grava 64, truncado em 16 bits
        if not tam & 0x80:
            self.gravacoes_normais += 1
        self.saida += ACK


def registro_banco(nome, canais):
    reg = bytearray(bj.BANK_MAGIC + b'\x00' * (bj.RECORD_LEN - 4))
    reg[4:4 + len(nome)] = nome.encode()
    for c in canais:
        reg[16 + (c >> 3)] |= 1 << (c & 7)
    reg[136:138] = struct.pack('<H', bj._checksum(bytes(reg[:136])))
    return bytes(reg)


def chip_com_bancos():
    chip = bytearray(b'\xff' * 0x100000)
    cab = bytearray(bj.HEADER_MAGIC + struct.pack('<HBB', 0x0005, 2, 0) + b'\x00\x00')
    cab[8:10] = struct.pack('<H', bj._checksum(bytes(cab[:8])))
    chip[0x0C1000:0x0C1000 + 10] = cab
    for n, nome, canais in ((0, 'test', [42]), (2, 'bnk', [41, 100])):
        chip[0x0C2000 + n * 0x1000:0x0C2000 + n * 0x1000 + bj.RECORD_LEN] = \
            registro_banco(nome, canais)
    return bytes(chip)


def espaco_de_fabrica(zona):
    """Espaco de 64 KB com o modo de zona pedido em 0x9028 e gravados os canais que os bancos usam
    (42, 43, 101 e 500 = indices 41, 42, 100 e 499): banco ligado sem canal gravado o driver
    desliga no upload, e o teste 2 espera um upload que nao muda nada."""
    e = bytearray(b'\x00' * 0x10000)
    for c in (41, 42, 100, 499):
        e[c * 32:c * 32 + 4] = b'\x00\x20\x65\x14'          # 146.52 MHz em lbcd
    e[0x9028] = zona
    return bytes(e)


def baixa(simulado):
    simulado.passa_tempo()
    radio = bj.BJ8300(simulado)
    radio.status_fn = lambda s: None
    radio.sync_in()
    return radio


def sobe(radio, simulado):
    simulado.passa_tempo()
    radio.pipe = simulado
    radio.sync_out()


def main():
    falhas = []

    def confere(nome, obtido, esperado):
        ok = obtido == esperado
        print('%-66s %s' % (nome, 'ok' if ok else 'FALHOU (%r != %r)' % (obtido, esperado)))
        if not ok:
            falhas.append(nome)

    # ---------- 1. download com o firmware dos bancos ----------
    sim = RadioSimulado(True, espaco_de_fabrica(0), chip_com_bancos())
    radio = baixa(sim)
    modelo = radio.get_bank_model()
    confere('1. imagem = parte de fabrica + area dos bancos',
            len(radio._mmap), bj.BLOB_BASE + bj.BLOB_SIZE)
    confere('1b. nomes dos bancos lidos pelo cabo',
            [b.get_name() for b in modelo.get_mappings()][:3], ['test', 'BANK 2', 'bnk'])
    confere('1c. canais do banco 3 (41 e 100 no radio = 42 e 101 no CHIRP)',
            [m.number for m in modelo.get_mapping_memories(modelo.get_mappings()[2])], [42, 101])
    confere('1d. bancos ligados e escolhido', (bj._BankStore(radio).enabled_mask(),
                                              bj._BankStore(radio).selected()), (0x0005, 2))
    confere('1e. download nao grava nada no chip', sim.gravacoes_estendidas, [])
    confere('1f. download inteiro numa sessao so', sim.sessoes, 1)
    # o que derrubou o driver antigo no radio: abrir outra sessao logo depois do 'E'
    try:
        radtel_rt900._enter_programming_mode(radio)
        confere('1g. sessao logo depois do E falha como no radio', 'abriu', 'falhou')
    except errors.RadioError as e:
        confere('1g. sessao logo depois do E falha como no radio',
                'identification' in str(e), True)

    # ---------- 2. upload sem mexer nos bancos: nada a gravar na area dos bancos ----------
    sim.sessoes = 0
    sobe(radio, sim)
    confere('2a. upload inteiro numa sessao so', sim.sessoes, 1)
    confere('2. upload sem mudanca nos bancos nao grava setor nenhum', sim.gravacoes_estendidas, [])
    confere('2b. upload poe o radio em Full Channel (0x9028 = 1)', sim.espaco[0x9028], 1)

    # ---------- 3. upload com mudanca: so os setores mudados, com marca e soma ----------
    mem = radio.get_memory(500)
    modelo.add_memory_to_mapping(mem, modelo.get_mappings()[4])
    modelo.get_mappings()[4].set_name('BANDA 2M')
    bj._BankStore(radio).set_enabled(4, True)
    sim.gravacoes_estendidas = []
    sobe(radio, sim)
    setor4 = 0x0C2000 + 4 * 0x1000
    confere('3. grava so o cabecalho e o banco 5, tres blocos cada',
            sim.gravacoes_estendidas,
            [0x0C1000, 0x0C1040, 0x0C1080, setor4, setor4 + 0x40, setor4 + 0x80])
    reg = bytes(sim.chip[setor4:setor4 + bj.RECORD_LEN])
    confere('3b. banco 5 no chip: marca, nome, canal 499 e soma',
            (reg[:4], reg[4:12], reg[16 + 499 // 8] >> (499 % 8) & 1,
             struct.unpack('<H', reg[136:])[0] == bj._checksum(reg[:136])),
            (bj.BANK_MAGIC, b'BANDA 2M', 1, True))
    cab = bytes(sim.chip[0x0C1000:0x0C1000 + 10])
    confere('3c. cabecalho no chip: banco 5 ligado e soma que fecha',
            (struct.unpack('<H', cab[4:6])[0], struct.unpack('<H', cab[8:10])[0] == bj._checksum(cab[:8])),
            (0x0015, True))
    confere('3d. o resto do setor fica apagado, como o firmware deixa',
            set(sim.chip[setor4 + bj.RECORD_LEN:setor4 + 0x1000]), {0xFF})
    # banco ligado sem nenhum canal gravado nao sobe ligado (o radio tambem recusa)
    bj._BankStore(radio).set_enabled(8, True)
    sobe(radio, sim)
    cab = bytes(sim.chip[0x0C1000:0x0C1000 + 10])
    confere('3e. banco 9 ligado e vazio sobe desligado; os outros ficam',
            struct.unpack('<H', cab[4:6])[0], 0x0015)

    # ---------- 4. ida e volta: baixar de novo da o mesmo ----------
    radio2 = baixa(sim)
    m2 = radio2.get_bank_model()
    confere('4. depois de baixar de novo, o banco 5 continua com nome e canal',
            (m2.get_mappings()[4].get_name(),
             [m.number for m in m2.get_mapping_memories(m2.get_mappings()[4])]),
            ('BANDA 2M', [500]))

    # ---------- 5. firmware de fabrica: recusa com a mensagem certa, sem gravar estendido ----------
    fab = RadioSimulado(False, espaco_de_fabrica(1))
    try:
        baixa(fab)
        confere('5. download com firmware de fabrica recusado', 'aceitou', 'recusou')
    except errors.RadioError as e:
        confere('5. download com firmware de fabrica recusado com a mensagem certa',
                'not running the banks firmware' in str(e), True)
    try:
        sobe(radio, fab)
        confere('5b. upload com firmware de fabrica recusado', 'aceitou', 'recusou')
    except errors.RadioError as e:
        confere('5b. upload com firmware de fabrica recusado com a mensagem certa',
                'not running the banks firmware' in str(e), True)
    confere('5c. o firmware de fabrica nao recebeu nenhum W estendido',
            sum(1 for _ in fab.gravacoes_estendidas), 0)
    confere('5d. e o cabo so ficou com o ACK do E (sem dessincronizar)', bytes(fab.saida), ACK)
    confere('5e. e nada foi gravado nele, nem canal (a conferencia vem antes)',
            fab.gravacoes_normais, 0)

    print()
    print('FALHAS: %d' % len(falhas) if falhas else 'tudo certo')
    return 1 if falhas else 0


if __name__ == '__main__':
    sys.exit(main())
