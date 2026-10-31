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
    resposta sai com tam = 0x40 (o gancho do cabo troca o byte); gravacao so em 0xC1000..0xD5FFF
    (ate a v45, 0xD1FFF; da v46 a v48, 0xD2FFF; da v49 a v109, 0xD3FFF: fora dela o bloco e ignorado e
    o ACK sai assim mesmo), apagando o setor quando o bloco o abre;
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
    from chirp import errors, memmap
    from chirp.drivers import baofeng_uv17Pro, radtel_rt900
except ImportError:
    print('chirp nao instalado - nada a testar aqui (o teste_bj8300.py cobre o resto)')
    sys.exit(0)

import teste_chirp_real as real  # noqa: E402  (carrega e registra o driver uma vez so)

bj = real.bj
ACK = b'\x06'


class RadioSimulado(object):
    """O lado do radio no cabo. Guarda o espaco de 64 KB do protocolo e o chip de dados de 1 MB."""

    def __init__(self, bancos, espaco=None, chip=None, fim_gravacao=0x0D6000):
        self.bancos = bancos                       # True = firmware BJ8300_BANK
        self.fim_gravacao = fim_gravacao           # v110: 0xD6000 (o Morse e o APRS); v49-v109 0xD4000;
                                                   # v46-v48 0xD3000; ate a v45, 0xD2000
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
            if 0x0C1000 <= endereco < self.fim_gravacao:
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
    chip[0x0D2000:0x0D2040] = real.registro_watch(2, 1, 4)      # TRIPLE, a C no canal 5, Frequency
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
    confere('1. imagem = fabrica + area dos bancos + WATCH + os 2 do VFO C + MORSE + APRS',
            len(radio._mmap), bj.BLOB_BASE + bj.BLOB_SIZE + 5 * 64)
    confere('1h. WATCH, CH-C e MDF-C lidos pelo cabo', real.watch_de(radio), ('TRIPLE', 5, 'Frequency', True))
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

    # ---------- 6. WATCH (v46): so o bloco que muda, com o que e do radio vindo do radio ----------
    radio3 = baixa(sim)
    # depois do download, a C vai para o lado B no radio (tecla A/B): isso e do radio, nao da imagem
    sim.chip[0x0D2000:0x0D2040] = real.registro_watch(2, 1, 4, 1, 7)
    real.muda_watch(radio3, watch_mode='SINGLE')
    radio3._memobj.settings.tdr = 1
    sim.gravacoes_estendidas = []
    sobe(radio3, sim)
    confere('6. upload com o WATCH mudado grava so o bloco dele', sim.gravacoes_estendidas, [0x0D2000])
    confere('6b. SINGLE da imagem, a C no lado B e o canal do B vindos do radio',
            bytes(sim.chip[0x0D2000:0x0D2040]), real.registro_watch(1, 1, 4, 1, 7))
    confere('6c. SINGLE desliga o TDR no upload (0x9004 bit 0)', sim.espaco[0x9004] & 1, 0)
    sim.gravacoes_estendidas = []
    sobe(radio3, sim)
    confere('6d. de novo, sem mudanca: nada gravado', sim.gravacoes_estendidas, [])

    # firmware ate a v45: a janela do cabo para em 0xD1FFF, o bloco e ignorado com ACK
    velho = RadioSimulado(True, espaco_de_fabrica(0), chip_com_bancos(), fim_gravacao=0x0D2000)
    radio4 = baixa(velho)
    real.muda_watch(radio4, watch_mode='DUAL')
    try:
        sobe(radio4, velho)
        confere('6e. firmware ate a v45: o upload do WATCH avisa', 'aceitou', 'avisou')
    except errors.RadioError as e:
        confere('6e. firmware ate a v45: o upload do WATCH avisa que precisa da v46', 'v46' in str(e), True)
    confere('6f. e os canais e ajustes foram gravados antes', velho.gravacoes_normais > 0, True)
    confere('6g. e o WATCH do radio ficou como estava', bytes(velho.chip[0x0D2000:0x0D2040]),
            real.registro_watch(2, 1, 4))

    # imagem salva antes do WATCH (sem o bloco): o upload nao toca no do radio
    radio5 = bj.BJ8300(memmap.MemoryMapBytes(radio.get_mmap().get_packed()[:bj.WATCH_BASE]))
    radio5.status_fn = lambda s: None
    real.muda_watch(radio5)                                     # a pagina aberta e aplicada sem mudanca
    antes = bytes(sim.chip[0x0D2000:0x0D2040])
    sim.gravacoes_estendidas = []
    sobe(radio5, sim)
    confere('6h. imagem sem o bloco do WATCH: o do radio fica', (0x0D2000 in sim.gravacoes_estendidas,
                                                              bytes(sim.chip[0x0D2000:0x0D2040]) == antes),
            (False, True))

    # ---------- 7. VFO C (v49): os dois blocos em 0xD3000, so quando o VFO da C muda ----------
    sim.chip[0x0D2000:0x0D2040] = real.registro_watch(2, 1, 4)    # a C fora do lado B
    sim.espaco[0x8020:0x8028] = bytes([1, 4, 6, 5, 2, 0, 0, 0])   # o VFO B em 146.52000, como num radio
    radio7 = baixa(sim)
    real.muda_watch(radio7, vfoc_freq='438.500000')
    sim.gravacoes_estendidas = []
    sobe(radio7, sim)
    confere('7. upload com o VFO C mudado grava os 2 blocos dele', sim.gravacoes_estendidas,
            [0x0D3000, 0x0D3040])
    confere('7b. e o radio ficou com o bloco da imagem (marca, a C em 438,5, soma)',
            (bytes(sim.chip[0x0D3000:0x0D3000 + bj.VFC_LEN]) ==
             bytes(radio7._mmap[bj.VFC_BASE:bj.VFC_BASE + bj.VFC_LEN]),
             bytes(sim.chip[0x0D3004:0x0D300C]), real._VfcStore_valido(radio7)),
            (True, bytes([4, 3, 8, 5, 0, 0, 0, 0]), True))
    sim.gravacoes_estendidas = []
    sobe(radio7, sim)
    confere('7c. de novo, sem mudanca: nada gravado', sim.gravacoes_estendidas, [])
    # a C vai para o lado B no radio depois do download: o bloco e do radio (guarda o VFO do B)
    real.muda_watch(radio7, vfoc_freq='436.000000')
    sim.chip[0x0D2000:0x0D2040] = real.registro_watch(2, 1, 4, 1, 7)
    antes = bytes(sim.chip[0x0D3000:0x0D3080])
    sim.gravacoes_estendidas = []
    sobe(radio7, sim)
    confere('7d. com a C no lado B no radio: o bloco do VFO C nao e gravado',
            (0x0D3000 in sim.gravacoes_estendidas, bytes(sim.chip[0x0D3000:0x0D3080]) == antes),
            (False, True))
    # firmware v46-v48: a janela do cabo para em 0xD2FFF, o bloco e ignorado com ACK
    v48 = RadioSimulado(True, espaco_de_fabrica(0), chip_com_bancos(), fim_gravacao=0x0D3000)
    v48.chip[0x0D2000:0x0D2040] = real.registro_watch(2, 1, 4)
    v48.espaco[0x8020:0x8028] = bytes([1, 4, 6, 5, 2, 0, 0, 0])
    radio8 = baixa(v48)
    real.muda_watch(radio8, vfoc_freq='438.500000')
    try:
        sobe(radio8, v48)
        confere('7e. firmware v46-v48: o upload do VFO C avisa', 'aceitou', 'avisou')
    except errors.RadioError as e:
        confere('7e. firmware v46-v48: o upload do VFO C avisa que precisa da v49', 'v49' in str(e), True)
    confere('7f. e o VFO C do radio ficou como estava (apagado)', bytes(v48.chip[0x0D3000:0x0D3004]),
            b'\xff' * 4)

    # ---------- 8. MORSE e APRS (v110): um bloco cada, em 0xD4000 e 0xD5000, so quando mudam ----------
    radio9 = baixa(sim)
    confere('8. sem registro no radio: os blocos vem apagados',
            bytes(radio9._mmap[bj.MORSE_BASE:bj.IMAGE_SIZE]), b'\xff' * 128)
    real.muda_ajustes(radio9, morse_speed='25 WPM', aprs_callsign='N0CALL', aprs_message='TEST')
    sim.gravacoes_estendidas = []
    sobe(radio9, sim)
    confere('8b. upload com o Morse e o APRS mudados grava os 2 blocos', sim.gravacoes_estendidas,
            [0x0D4000, 0x0D5000])
    confere('8c. e o radio ficou com os registros da imagem',
            (bytes(sim.chip[0x0D4000:0x0D4040]) == bytes(radio9._mmap[bj.MORSE_BASE:bj.APRS_BASE]),
             bytes(sim.chip[0x0D5000:0x0D5040]) == bytes(radio9._mmap[bj.APRS_BASE:bj.IMAGE_SIZE]),
             sim.chip[0x0D4005], bytes(sim.chip[0x0D500A:0x0D5010])), (True, True, 25, b'N0CALL'))
    sim.gravacoes_estendidas = []
    sobe(radio9, sim)
    confere('8d. de novo, sem mudanca: nada gravado', sim.gravacoes_estendidas, [])
    radio10 = baixa(sim)
    confere('8e. o download traz os registros de volta',
            (str(real.ajuste_de(radio10, 'morse_speed').value), str(real.ajuste_de(radio10, 'aprs_callsign').value)),
            ('25 WPM', 'N0CALL'))
    # imagem salva antes do Morse e do APRS (sem os blocos): o upload nao toca nos do radio
    radio11 = bj.BJ8300(memmap.MemoryMapBytes(radio9.get_mmap().get_packed()[:bj.MORSE_BASE]))
    radio11.status_fn = lambda s: None
    sim.gravacoes_estendidas = []
    sobe(radio11, sim)
    confere('8f. imagem sem os blocos: os do radio ficam', sim.gravacoes_estendidas, [])
    # firmware v49-v109: a janela do cabo para em 0xD3FFF, o bloco e ignorado com ACK
    v109 = RadioSimulado(True, espaco_de_fabrica(0), chip_com_bancos(), fim_gravacao=0x0D4000)
    radio12 = baixa(v109)
    real.muda_ajustes(radio12, morse_tone='900 Hz')
    try:
        sobe(radio12, v109)
        confere('8g. firmware ate a v109: o upload do Morse avisa', 'aceitou', 'avisou')
    except errors.RadioError as e:
        confere('8g. firmware ate a v109: o upload do Morse avisa que precisa da v110', 'v110' in str(e), True)
    confere('8h. e o registro do radio ficou como estava (apagado)', bytes(v109.chip[0x0D4000:0x0D4004]),
            b'\xff' * 4)

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
