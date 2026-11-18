"""O lado do CHIRP nos testes do Tempra Link do BJ-8300: o driver deste repositório (chirp/bj8300_v3.7TEMPRA.py) é a
referência do que cada byte quer dizer.

  python -I tempralink/testes/fixtures.py gera     monta um rádio pelo driver (canais, bancos, ajustes, textos) e
                                                   escreve a memória dele (radio.bin) e o que o Tempra Link tem que
                                                   ler dela (esperado.txt)
  python -I tempralink/testes/fixtures.py confere  lê a memória que o Tempra Link gravou no rádio simulado
                                                   (gravado.bin) pelo driver e confere as mudanças que o
                                                   TesteTempraLink fez (as mesmas de MUDANCAS, lá e aqui)

A memória vai como no backup: uma área atrás da outra, com o endereço e o tamanho (4 bytes cada, o baixo primeiro).
"""
import os
import struct
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(AQUI))
import gera_menu as gm  # noqa: E402  (carrega o driver)
from chirp import chirp_common, memmap, settings  # noqa: E402

bj = gm.bj
AREAS_FABRICA = [(0x0000, 0x7800), (0x8000, 0x40), (0x9000, 0x40), (0xA000, 0x140), (0xB000, 0x400),
                 (0xC000, 0x400), (0xD000, 0x40), (0xF000, 0x280)]


def areas_da_imagem(img):
    """As áreas do rádio (as do BJ8300.pas) a partir da imagem do CHIRP."""
    saida = []
    for endereco, tam in AREAS_FABRICA:
        dados = bytes(img[endereco:min(endereco + tam, bj.BLOB_BASE)])
        saida.append((endereco, dados + b'\xff' * (tam - len(dados))))
    for base, endereco, tam in gm.BLOCOS:
        saida.append((endereco, bytes(img[base:base + tam])))
    return saida


def imagem_das_areas(areas):
    img = bytearray(b'\xff' * bj.IMAGE_SIZE)
    for endereco, dados in areas:
        if endereco < 0x10000:
            n = min(len(dados), bj.BLOB_BASE - endereco)
            img[endereco:endereco + n] = dados[:n]
        else:
            for base, e, tam in gm.BLOCOS:
                if e == endereco:
                    img[base:base + tam] = dados[:tam]
    return bytes(img)


def escreve_areas(caminho, areas):
    with open(caminho, 'wb') as f:
        for endereco, dados in areas:
            f.write(struct.pack('<II', endereco, len(dados)) + dados)


def le_areas(caminho):
    d = open(caminho, 'rb').read()
    areas, p = [], 0
    while p < len(d):
        endereco, tam = struct.unpack_from('<II', d, p)
        areas.append((endereco, d[p + 8:p + 8 + tam]))
        p += 8 + tam
    return areas


def ajuste(radio, nome, valor):
    arvore = radio.get_settings()
    el = gm.acha(arvore, nome)
    el.value.set_value(valor)
    el.value._has_changed = True
    radio.set_settings(settings.RadioSettings(settings.RadioSettingGroup('so', 'so', el)))


def memoria(numero, freq, nome, **campos):
    m = chirp_common.Memory()
    m.number, m.freq, m.name = numero, freq, nome
    for k, v in campos.items():
        setattr(m, k, v)
    return m


def extras(radio, m, **valores):
    """Os campos de m.extra (encrypt, scramble, bcl, pttid, scode) pelo nome, com o valor como o driver mostra."""
    base = radio.get_memory(m.number) if not m.extra else m
    m.extra = base.extra
    for el in m.extra:
        if el.get_name() in valores:
            el.value.set_value(valores[el.get_name()])


def monta():
    """Um BJ-8300 com um pouco de tudo, feito pelo driver."""
    img = bytearray(b'\xff' * bj.IMAGE_SIZE)
    img[0x8000:0x9000] = bytes(0x1000)
    img[0x9000:0x9040] = bytes(0x40)
    img[0xA000:0xA140] = b'\xff' * 0x140
    img[0xD000] = 0x00
    vfo = bytes([1, 4, 6, 5, 2, 0, 0, 0]) + bytes(24)
    img[0x8000:0x8040] = vfo + vfo
    radio = bj.BJ8300(memmap.MemoryMapBytes(bytes(img)))
    potencias = radio.POWER_LEVELS
    canais = [
        memoria(1, 146520000, 'SIMPLEX'),
        memoria(2, 145290000, 'RPT 88', duplex='-', offset=600000, tmode='Tone', rtone=88.5, mode='NFM',
                power=potencias[1], skip='S'),
        memoria(3, 462562500, 'gmrs 3', duplex='split', offset=467562500, tmode='DTCS', dtcs=23, dtcs_polarity='NR',
                power=potencias[2]),
        memoria(4, 127000000, 'AIR', mode='AM'),
        memoria(5, 433500000, 'SEM TX', duplex='off', tmode='TSQL', ctone=123.0),
        memoria(6, 439000000, 'CROSS', duplex='-', offset=5000000, tmode='Cross', cross_mode='DTCS->Tone',
                dtcs=754, rx_dtcs=754, ctone=67.0, dtcs_polarity='RN'),
    ]
    for m in canais:
        radio.set_memory(m)
    m = radio.get_memory(1)
    extras(radio, m, encrypt='ENCRY2', scramble='3000 Hz', bcl=True, pttid='BOT', scode='5')
    radio.set_memory(m)
    m = radio.get_memory(6)
    extras(radio, m, scramble='3400 Hz', pttid='Both', scode='15')
    radio.set_memory(m)
    bancos = radio.get_bank_model()
    mapas = bancos.get_mappings()
    for n in (2, 3):
        bancos.add_memory_to_mapping(radio.get_memory(n), mapas[0])
    bancos.add_memory_to_mapping(radio.get_memory(4), mapas[2])
    bancos.add_memory_to_mapping(radio.get_memory(3), mapas[15])
    mapas[0].set_name('REPEATERS')
    mapas[2].set_name('AIR')
    loja = bj._BankStore(radio)
    loja.set_enabled(0, True)
    loja.set_enabled(15, True)
    for nome, valor in (('settings_sql', 5), ('settings_tot', '60S'), ('expander', 'Mid'), ('deemphasis', 'Off'),
                        ('eq_rx_bass', '+2 dB'), ('eq_tx_treble', '0 dB'), ('noise_gate', 'High'),
                        ('modem_gain', 'Low'), ('settings_unknown_9037', -120), ('watch_mode', 'TRIPLE'),
                        ('watch_mdf_c', 'Channel'), ('vfo_a_txpower', 'Middle'), ('vfoc_step', '12.5 K'),
                        ('vfoc_scramble', '2900 Hz'), ('vfo.b.rxtone', 'D023I'), ('vfo_b_sftd', '+'),
                        ('settings_roger', 'COIN'), ('settings_topkey_sp', 'FLASHLIGHT'),
                        ('morse_speed', '20 WPM'), ('morse_tone', '800 Hz'), ('aprs_ssid', '-9'),
                        ('aprs_path', 'WIDE1-1,WIDE2-1'), ('aprs_decode', 'Off'), ('aprs_callsign', 'N0CALL'),
                        ('aprs_to', 'APRS'), ('aprs_message', 'Ola, mundo!'), ('morse_message', 'CQ CQ DE N0CALL'),
                        ('pttid/0.code', '123A'), ('pttid/14.code', '*#0'), ('vfo.a.freq', '145.500000'),
                        ('vfoc_freq', '438.500000'), ('vfoc_offset', '5.000000'), ('vfo.b.offset', '0.600000')):
        ajuste(radio, nome, valor)
    return radio


def tom(modo, valor, pol):
    if not modo:
        return 'OFF'
    if modo == 'Tone':
        return '%.1f' % valor
    return 'D%03d%s' % (valor, 'N' if pol == 'N' else 'I')


def esperado(radio):
    linhas = []
    potencias = [str(p) for p in radio.POWER_LEVELS]
    bancos = radio.get_bank_model()
    for n in range(1, 13):
        m = radio.get_memory(n)
        if m.empty:
            linhas.append('C\t%d\t-' % n)
            continue
        rx = m.freq // 10
        if m.duplex == 'off':
            tx = 0
        elif m.duplex == 'split':
            tx = m.offset // 10
        else:
            tx = (m.freq + {'': 0, '+': m.offset, '-': -m.offset}[m.duplex]) // 10
        (txm, txv, txp), (rxm, rxv, rxp) = chirp_common.split_tone_encode(m)
        extra = {el.get_name(): el.value for el in m.extra}
        lista = {b.get_index() for b in bancos.get_memory_mappings(m)}
        linhas.append('\t'.join(str(x) for x in (
            'C', n, m.name, rx, tx, tom(rxm, rxv, rxp), tom(txm, txv, txp), potencias.index(str(m.power)),
            int(m.mode in ('NFM', 'NAM')), int(m.skip != 'S'), int(bool(extra['bcl'])), int('AM' in m.mode),
            radio._voicepri_list.index(str(extra['encrypt'])), extra['scramble'].get_mem_val() >> 2,
            int(str(extra['scode'])) - 1, bj.radtel_rt900.PTTID_LIST.index(str(extra['pttid'])),
            sum(1 << (i - 1) for i in lista))))
    arvore = radio.get_settings()
    for pasta, itens in gm.MENU:
        for nome, lado, aj, filtro in itens:
            el = gm.acha(arvore, aj)
            v = el.value
            ops = [x for _, x in gm.opcoes(v, filtro)]
            if isinstance(v, settings.RadioSettingValueBoolean):
                atual = bool(v)
            elif isinstance(v, settings.RadioSettingValueInteger):
                atual = int(v)
            else:
                atual = str(v)
            if atual in ops:   # um tom DCS não aparece no item do CTCSS, nem o contrário
                linhas.append('A\t%s\t%s\t%s\t%d' % (pasta, nome, lado, ops.index(atual)))
    loja = bj._BankStore(radio)
    for k in range(bj.NUM_BANKS):
        linhas.append('B\t%d\t%s\t%d\t%d' % (k, loja.name(k), loja.enabled_mask() >> k & 1, loja.programmed_count(k)))
    for nome, campo in (('CALLSIGN', bj.APRS_CALL), ('TO', bj.APRS_TO), ('APRS MESSAGE', bj.APRS_MESSAGE),
                        ('MORSE MESSAGE', bj.MORSE_TEXT)):
        linhas.append('T\t%s\t%s' % (nome, bj._record_text(campo[0](radio).record(), campo)))
    for k in range(15):
        codigo = ''.join(bj.radtel_rt900.DTMF_CHARS[x] for x in radio._memobj.pttid[k].code if int(x) < 0x1F)
        linhas.append('D\t%d\t%s' % (k, codigo))
    for k, vfo in enumerate((radio._memobj.vfo.a, radio._memobj.vfo.b, radio._vfc.c)):
        linhas.append('F\t%d\t%d\t%d' % (k, bj._digits(vfo.freq), bj._digits(vfo.offset)))
    return linhas


def gera():
    radio = monta()
    img = radio._mmap.get_packed()
    escreve_areas(os.path.join(AQUI, 'radio.bin'), areas_da_imagem(img))
    with open(os.path.join(AQUI, 'esperado.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(esperado(radio)) + '\n')
    print('radio.bin e esperado.txt')


# As mudanças que o TesteTempraLink faz antes de gravar (TesteGravacao), e o que o driver tem que ler depois.
def confere():
    radio = bj.BJ8300(memmap.MemoryMapBytes(imagem_das_areas(le_areas(os.path.join(AQUI, 'gravado.bin')))))
    erros = []

    def igual(oque, visto, quer):
        if visto != quer:
            erros.append('%s: %r, e devia ser %r' % (oque, visto, quer))

    m = radio.get_memory(10)
    extra = {el.get_name(): str(el.value) for el in m.extra}
    igual('canal 10 nome', m.name, 'TESTE 10')
    igual('canal 10 RX', m.freq, 439500000)
    igual('canal 10 duplex', (m.duplex, m.offset), ('-', 5000000))
    igual('canal 10 tons', (m.tmode, m.cross_mode, m.dtcs, m.dtcs_polarity, m.ctone),
          ('Cross', 'Tone->DTCS', 23, 'NN', 88.5))   # TX 88.5, RX D023N
    igual('canal 10 potência', str(m.power), str(radio.POWER_LEVELS[1]))
    igual('canal 10 modo', m.mode, 'NFM')
    igual('canal 10 extras', (extra['bcl'], extra['encrypt'], extra['scramble'], extra['scode'], extra['pttid']),
          ('True', 'ENCRY2', '3000 Hz', '4', 'BOT'))
    bancos = radio.get_bank_model()
    igual('canal 10 bancos', sorted(b.get_index() for b in bancos.get_memory_mappings(m)), [1, 5, 16])
    igual('canal 2 apagado', radio.get_memory(2).empty, True)
    igual('canal 1 intacto', (radio.get_memory(1).name, radio.get_memory(1).freq), ('SIMPLEX', 146520000))
    loja = bj._BankStore(radio)
    igual('banco 5', (loja.name(4), loja.enabled_mask() >> 4 & 1), ('NOVO', 1))
    igual('banco 1 ainda com o 3', loja.has(0, 2), True)
    arvore = radio.get_settings()
    for nome, quer in (('settings_sql', 7), ('settings_tot', '30S'), ('expander', 'High'), ('deemphasis', 'Off'),
                       ('eq_rx_bass', '+3 dB'), ('watch_mode', 'SINGLE'), ('settings_tdr', False),
                       ('vfoc_step', '12.5 K'), ('vfo_b_txpower', 'Low'), ('morse_speed', '25 WPM'),
                       ('aprs_path', 'None'), ('aprs_callsign', 'NOCALL'), ('morse_message', 'TESTE 73'),
                       ('vfo.a.freq', '145.550000'), ('vfoc_freq', '433.000000'), ('vfoc_offset', '0.600000'),
                       ('pttid/1.code', '9876'), ('noise_gate', 'High'), ('modem_gain', 'Low')):
        v = gm.acha(arvore, nome).value
        visto = bool(v) if isinstance(v, settings.RadioSettingValueBoolean) else (
            int(v) if isinstance(v, settings.RadioSettingValueInteger) else str(v).strip())
        igual(nome, visto, quer)
    for e in erros:
        print('FALHOU', e)
    print('conferido pelo driver: %s' % ('tudo certo' if not erros else '%d erros' % len(erros)))
    return not erros


if __name__ == '__main__':
    if sys.argv[1:] == ['gera']:
        gera()
    elif sys.argv[1:] == ['confere']:
        sys.exit(0 if confere() else 1)
    else:
        sys.exit(__doc__)
