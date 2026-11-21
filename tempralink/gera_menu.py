"""Gera o menu-bj8300.txt, a descrição dos ajustes do BJ-8300 que o Tempra Link embute (Ajustes.pas).

Uso: python -I tempralink/gera_menu.py        (precisa do CHIRP instalado, como o tests/teste_chirp_real.py)

O Tempra Link não interpreta os ajustes: para cada item do menu do rádio, ele sabe os bytes que cada valor põe na
memória. Este gerador tira esses bytes do driver do CHIRP deste repositório (chirp/bj8300_v4.2TEMPRA.py, conferido
contra o rádio): aplica cada opção do ajuste correspondente numa imagem e anota os bits que mudam de uma opção para
outra. Os nomes das pastas e dos itens são os do menu do rádio (firmware/scripts/montar.py, GRUPOS); o texto de cada
valor é o que o rádio mostra quando a lista do rádio diz a mesma coisa que a do CHIRP (TEXTOS_DO_RADIO, tirados das
listas do firmware), e o do CHIRP em maiúsculas quando não.

O arquivo, uma linha por registro, com os campos separados por TAB:
  R  endereço  assinatura  tamanho      um registro com assinatura e soma no fim (bancos.c): o rádio o lê só se as
                                        duas conferem; sem ele, os campos valem 0 (o VFO C, os do VFO B)
  F  lado  endereço                     o VFO A, B ou C (struct vfo): a frequência nos 8 primeiros bytes, um algarismo
                                        por byte, em 10 Hz, e o OFFSET nos bytes 20 a 25, em kHz
  T  nome  registro  tamanho  início  máximo   um texto: o byte do tamanho, onde começa e quantas letras cabem
  P  pasta
  I  item  lado  fora                   '-' vale para o rádio; A, B ou C é de um VFO; fora, a posição do valor que
                                        o rádio mostra quando os bytes não batem com nenhum (vazio: não há)
  V  texto  endereço/máscara=valor,...  o valor: os bits da máscara em cada endereço (em hexadecimal)
"""
import importlib.util
import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAIDA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'menu-bj8300.txt')

try:
    from chirp import memmap, settings
except ImportError:
    sys.exit('precisa do chirp instalado (pip install https://codeload.github.com/kk7ds/chirp/tar.gz/refs/heads/master '
             '--no-deps)')

_caminho = os.path.join(RAIZ, 'chirp', 'bj8300_v4.2TEMPRA.py')
_nome = 'chirp.loaded.%s' % os.path.splitext(os.path.basename(_caminho))[0]
_spec = importlib.util.spec_from_file_location(_nome, _caminho)
bj = importlib.util.module_from_spec(_spec)
sys.modules[_nome] = bj
_spec.loader.exec_module(bj)

# Os itens, pasta a pasta, na ordem do menu do rádio: (item, lado, ajuste do driver, filtro das opções).
# Fora daqui ficam o que é ação ou tela (MESSAGES, CH MEMORY, RESET, VERSION, SCAN ADD), os bancos (página própria),
# o RX GAIN (0x9040, fora do bloco que o cabo lê) e os textos, as frequências e o OFFSET (linhas T e F).
LADOS = (('A', 'vfo_a_%s', 'vfo.a.%s'), ('B', 'vfo_b_%s', 'vfo.b.%s'), ('C', 'vfoc_%s', 'vfoc_%s'))


def por_lado(item, campo, filtro=None, ponto=False):
    return [(item, lado, (pontilhado if ponto else nome) % campo, filtro) for lado, nome, pontilhado in LADOS]


MENU = [
    ('APRS (SMS)', [('DECODE', '-', 'aprs_decode', None), ('PATH', '-', 'aprs_path', None),
                    ('SSID', '-', 'aprs_ssid', None)]),
    ('AUDIO', [('DE-EMPHASIS', '-', 'deemphasis', None), ('EQ BASS', '-', 'eq_rx_bass', None),
               ('EQ TREBLE', '-', 'eq_rx_treble', None), ('EXPANDER', '-', 'expander', None),
               ('NOISE GATE', '-', 'noise_gate', None), ('SQUELCH', '-', 'settings_sql', None),
               ('TAIL CUT', '-', 'settings_ste', None)]),
    ('CALIBRATION', [('CRYSTAL FINE', '-', 'settings_unknown_9037', 'cristal'),
                     ('LIGHT PWM', '-', 'settings_unknown_903b', None), ('MODEM GAIN', '-', 'modem_gain', None)]),
    ('CHANNEL', por_lado('BANDWIDTH', 'widenarr') + [('BUSY LOCK', '-', 'settings_busy_lock', None)] +
     por_lado('ENCRYPT', 'voicepri') + por_lado('RX MODULATION', 'rxmod') + por_lado('SCRAMBLE', 'scramble') +
     por_lado('STEP', 'step') + por_lado('TX POWER', 'txpower')),
    ('DISPLAY', [('BACKLIGHT TIME', '-', 'settings_abr', None), ('BRIGHTNESS', '-', 'settings_unknown_9039', None),
                 ('MENU EXIT TIME', '-', 'settings_menuquit', None), ('POWER-ON MSG', '-', 'settings_ponmsg', None),
                 ('SHOW A', '-', 'settings_mdfa', None), ('SHOW B', '-', 'settings_mdfb', None),
                 ('SHOW C', '-', 'watch_mdf_c', None)]),
    ('DTMF', [('DTMF CODE', '-', 'settings_pttid', None), ('DTMF DELAY', '-', 'settings_pttlt', None)] +
     por_lado('DTMF GROUP', 'scode') + [('DTMF SIDETONE', '-', 'settings_dtmfst', None)]),
    ('KEYS', [('AUTOLOCK', '-', 'settings_autolk', None), ('KEY BEEP', '-', 'settings_beep', None),
              ('PF2', '-', 'settings_skey2_sp1', None), ('PF2 LONG', '-', 'settings_skey2_lp', None),
              ('PF3', '-', 'settings_skey3_sp', None), ('PF3 LONG', '-', 'settings_topkey_sp', None)]),
    ('MIC', [('COMPRESSOR', '-', 'settings_unknown_9035', None), ('EQ BASS', '-', 'eq_tx_bass', None),
             ('EQ TREBLE', '-', 'eq_tx_treble', None), ('MIC AGC', '-', 'settings_unknown_9036', None),
             ('MIC GAIN', '-', 'settings_unknown_9033', None),
             ('NOISE REDUCTION', '-', 'settings_unknown_9032', None)]),
    ('MORSE', [('SPEED', '-', 'morse_speed', None), ('TONE', '-', 'morse_tone', None)]),
    ('REPEATER', [('RPT TAIL CUT', '-', 'settings_rpste', None), ('RPT TAIL DELAY', '-', 'settings_rptrl', None),
                  ('TONE BURST', '-', 'settings_tone', None)] + por_lado('TX SHIFT', 'sftd')),
    ('SCAN', [('SCAN RESUME', '-', 'settings_screv', None),
              ('THRESHOLD', '-', 'settings_unused_9022', None)]),   # v167
    ('SYSTEM', [('ALARM MODE', '-', 'settings_almode', None), ('BATTERY SAVE', '-', 'settings_save', None),
                ('BLUETOOTH', '-', 'settings_bluetooth', None)]),
    ('TONES', por_lado('RX CTCSS', 'rxtone', 'ctcss', True) + por_lado('RX DCS', 'rxtone', 'dcs', True) +
     por_lado('TX CTCSS', 'txtone', 'ctcss', True) + por_lado('TX DCS', 'txtone', 'dcs', True)),
    ('TX', [('ROGER', '-', 'settings_roger', None), ('TX TIMEOUT', '-', 'settings_tot', None),
            ('VOX', '-', 'settings_vox', None), ('VOX DELAY', '-', 'settings_voxd', None)]),
    ('WATCH', [('TRIPLE WATCH', '-', 'settings_tdr', None), ('TX SIDE', '-', 'settings_dualtx', None),
               ('WATCH', '-', 'watch_mode', None)]),
]
# O texto que o rádio mostra (as listas do firmware, lidas do app.bin da V3.7T com o montar.py): entra no lugar do
# texto do CHIRP quando a lista tem o mesmo tamanho e diz a mesma coisa (mesmas letras e algarismos, sem olhar
# maiúsculas, espaços e o "MODE" do BATTERY SAVE).
TEXTOS_DO_RADIO = {
    'STEP': ['2.5 K', '5.0 K', '6.25K', '8.33K', '10.0K', '12.5K', '20.0K', '25.0K', '50.0K'],
    'TX POWER': ['HIGH', 'MIDDLE', 'LOW'],
    'BATTERY SAVE': ['OFF', 'NORMAL MODE', 'SUPER MODE', 'DEEP MODE'],
    'VOX': ['OFF', '1', '2', '3', '4', '5', '6', '7', '8', '9'],
    'BANDWIDTH': ['WIDE', 'NARROW'],
    'ROGER': ['OFF', 'BEEP 1', 'BEEP 2', 'COIN', 'TONE1200'],
    'SCAN RESUME': ['TO', 'CO', 'SE'],
    'PF2': ['RADIO', 'TX POWER', 'SCAN', 'SEARCH', 'NOAA', 'SOS', 'SPECTRUM', 'PTTB'],
    'PF2 LONG': ['RADIO', 'TX POWER', 'SCAN', 'SEARCH', 'NOAA', 'SOS', 'SPECTRUM'],
    'PF3': ['RADIO', 'TX POWER', 'SCAN', 'SEARCH', 'NOAA', 'SOS', 'SPECTRUM'],
    'TX SHIFT': ['OFF', '+', '-'],
    'POWER-ON MSG': ['LOGO', 'VOLTAGE'],
    'DTMF CODE': ['OFF', 'BOT', 'EOT', 'BOTH'],
    'DTMF SIDETONE': ['OFF', 'DT-ST', 'ANI-ST', 'DT+ANI'],
    'TONE BURST': ['1000Hz', '1450Hz', '1750Hz', '2100Hz'],
    'SHOW A': ['NAME', 'FREQ', 'CH'], 'SHOW B': ['NAME', 'FREQ', 'CH'], 'SHOW C': ['NAME', 'FREQ', 'CH'],
    'ALARM MODE': ['ON SITE', 'SEND SOUND', 'SEND CODE'],
    'SCRAMBLE': ['OFF'] + ['%d Hz' % f for f in range(2700, 3401, 100)],
    'RX MODULATION': ['FM', 'AM'],
}
LIGA = ['OFF', 'ON']   # o OFF e ON de fábrica, de todo item de ligar e desligar

# Onde cada bloco da área estendida fica na imagem do CHIRP, e onde fica no rádio.
BLOCOS = ([(bj.BLOB_BASE + i * bj.SECTOR_SIZE, a, bj.SECTOR_SIZE) for i, a in enumerate(bj._sector_addresses())] +
          [(bj.WATCH_BASE, bj.WATCH_SECTOR, bj.EXT_BLOCK),
           (bj.VFC_BASE, bj.VFC_SECTOR, bj.VFC_BLOCKS * bj.EXT_BLOCK),
           (bj.MORSE_BASE, bj.MORSE_SECTOR, bj.EXT_BLOCK), (bj.APRS_BASE, bj.APRS_SECTOR, bj.EXT_BLOCK)])
# os registros com assinatura e soma: a soma muda com tudo, e quem a refaz é o Tempra Link
REGISTROS = [(bj.WATCH_SECTOR, bj.WATCH_MAGIC, bj.WATCH_LEN), (bj.VFC_SECTOR, bj.VFC_MAGIC, bj.VFC_LEN),
             (bj.MORSE_SECTOR, bj.MORSE_MAGIC, bj.MORSE_LEN), (bj.APRS_SECTOR, bj.APRS_MAGIC, bj.APRS_LEN)]
SOMAS = {r + k for r, _, n in REGISTROS for k in (n - 2, n - 1)}
# Os bytes que o rádio só lê com uma marca (o driver: EXPANDER_MARK, EQ_MARK, NOISE_GATE_MARK e o 0xA0 do MODEM GAIN):
# (bits da marca, marca). O valor de um item desses leva a marca junto, e sem ela o rádio usa o de fábrica (a linha I
# diz qual). Na imagem de base eles já estão marcados, para um item não mexer no outro que divide o byte.
MARCAS = {0x9034: (0xF0, 0xC0), 0x903A: (0xC0, 0x80), 0x903C: (0xC0, 0x80), 0x9031: (0xF8, 0xE8),
          0x903D: (0xFC, 0xA0)}


def no_radio(o):
    """O endereço no rádio do byte o da imagem do CHIRP."""
    if o < bj.BLOB_BASE:
        return o
    for base, endereco, tam in BLOCOS:
        if base <= o < base + tam:
            return endereco + o - base
    raise SystemExit('byte %X da imagem fora dos blocos conhecidos' % o)


def acha(arvore, nome):
    for el in arvore:
        if isinstance(el, settings.RadioSetting):
            if el.get_name() == nome:
                return el
        else:
            r = acha(el, nome)
            if r is not None:
                return r
    return None


def opcoes(valor, filtro):
    """(texto do CHIRP, o que se põe no valor) de cada opção."""
    if isinstance(valor, settings.RadioSettingValueBoolean):
        return [(t, b) for t, b in zip(LIGA, (False, True))]
    if isinstance(valor, settings.RadioSettingValueInteger):
        passo = 10 if filtro == 'cristal' else 1   # o CRYSTAL FINE do rádio anda de 10 em 10 Hz (401 valores)
        return [(('%+d HZ' % v if v else '0 HZ') if filtro == 'cristal' else str(v), v)
                for v in range(valor.get_min(), valor.get_max() + 1, passo)]
    lista = valor.get_options()
    if filtro == 'ctcss':
        lista = [o for o in lista if o == 'Off' or o.endswith('Hz')]
    elif filtro == 'dcs':
        lista = [o for o in lista if o == 'Off' or o.startswith('D')]
    return [(o, o) for o in lista]


def mesmo(a, b):
    def chave(s):
        return s.upper().replace(' ', '').replace('MODE', '')
    return len(a) == len(b) and all(chave(x) == chave(y) for x, y in zip(a, b))


def texto_do_chirp(t, filtro):
    t = t.upper()
    if filtro in ('ctcss', 'dcs'):
        t = t.replace('HZ', '')
    return t.replace(' SECONDS', 'S').replace(' SECOND', 'S').replace(' MS', 'MS')


def imagem_base():
    """Uma imagem zerada com os registros já feitos: assim nenhuma opção cria registro, e o que muda é só o campo."""
    r = bj.BJ8300(memmap.MemoryMapBytes(b'\x00' * bj.IMAGE_SIZE))
    vfo = bytes([1, 4, 6, 5, 2, 0, 0, 0]) + bytes(bj.VFO_LEN - 8)   # 146.52 MHz: o driver recusa a frequência 0
    r._mmap[0x8000] = vfo + vfo
    for endereco, (_, marca) in MARCAS.items():
        r._mmap[endereco] = bytes([marca])
    for base, magia, tam in ((bj.WATCH_BASE, bj.WATCH_MAGIC, bj.WATCH_LEN),
                             (bj.MORSE_BASE, bj.MORSE_MAGIC, bj.MORSE_LEN),
                             (bj.APRS_BASE, bj.APRS_MAGIC, bj.APRS_LEN),
                             (bj.VFC_BASE, bj.VFC_MAGIC, bj.VFC_LEN)):
        rec = bytearray(magia + (vfo + vfo if magia == bj.VFC_MAGIC else b'') + bytes(tam - 4))[:tam]
        rec[tam - 2:tam] = bj._checksum(bytes(rec[:tam - 2])).to_bytes(2, 'little')
        # o resto do bloco apagado, como os registros do driver e do rádio ficam
        blocos = bj.VFC_BLOCKS if magia == bj.VFC_MAGIC else 1
        r._mmap[base] = bytes(rec) + b'\xff' * (blocos * bj.EXT_BLOCK - tam)
    return r._mmap.get_packed()


def item(base, nome, filtro, mascara_inteira):
    radio = bj.BJ8300(memmap.MemoryMapBytes(base))
    el = acha(radio.get_settings(), nome)
    if el is None:
        raise SystemExit('o driver não tem o ajuste %s' % nome)
    ops = opcoes(el.value, filtro)
    so_ele = settings.RadioSettings(settings.RadioSettingGroup('so', 'so', el))
    imagens = []
    for _, v in ops:
        # a mesma imagem de volta à base, no lugar (os campos do driver apontam para ela), e só este ajuste aplicado,
        # mesmo o valor que a base já tem (os ajustes do driver só gravam o que mudou)
        radio._mmap[0] = base
        el.value.set_value(v)
        el.value._has_changed = True
        radio.set_settings(so_ele)
        imagens.append(radio._mmap.get_packed())
    mascaras = {}
    for img in imagens[1:]:
        for o, (x, y) in enumerate(zip(imagens[0], img)):
            if x != y:
                mascaras[o] = mascaras.get(o, 0) | (x ^ y)
    for o in list(mascaras):
        if no_radio(o) in SOMAS:
            del mascaras[o]
        elif mascara_inteira or inteiro(radio, el, so_ele, base, o, ops):
            mascaras[o] = 0xFF
        elif no_radio(o) in MARCAS:
            mascaras[o] |= MARCAS[no_radio(o)][0]
    if not mascaras:
        raise SystemExit('as opções de %s não mudam nada na imagem' % nome)
    valores = []
    for (texto, _), img in zip(ops, imagens):
        bytes_ = ','.join('%X/%02X=%02X' % (no_radio(o), m, img[o] & m) for o, m in sorted(mascaras.items()))
        valores.append((texto, bytes_))
    vistos = [b for _, b in valores]
    if len(set(vistos)) != len(vistos):
        raise SystemExit('duas opções de %s põem os mesmos bytes' % nome)
    return valores, padrao(base, nome, ops, mascaras, imagens)


def inteiro(radio, el, so_ele, base, o, ops):
    """Se o ajuste grava o byte o inteiro (um u8 do MEM_FORMAT, ou um byte cru do driver): aí o rádio compara o byte
    todo, e um valor fora da lista é outra coisa (o Fora). Prova: cada opção dá o mesmo byte partindo dele em 00 e em FF.
    Os bytes com marca nunca são inteiros: a marca e o outro ajuste ficam."""
    if no_radio(o) in MARCAS:
        return False
    for _, v in ops:
        bytes_ = []
        for antes in (0x00, 0xFF):
            img = bytearray(base)
            img[o] = antes
            radio._mmap[0] = bytes(img)
            el.value.set_value(v)
            el.value._has_changed = True
            radio.set_settings(so_ele)
            bytes_.append(radio._mmap.get_packed()[o])
        if bytes_[0] != bytes_[1]:
            return False
    return True


def padrao(base, nome, ops, mascaras, imagens):
    """A opção que o rádio mostra quando os bytes não batem com nenhuma (um byte sem a marca, um valor fora da lista,
    a imagem apagada): a que o driver lê num byte assim. None se o driver não aceita um byte assim."""
    usados = {tuple(img[o] & m for o, m in sorted(mascaras.items())) for img in imagens}
    for c in (0x00, 0xFF, 0xD0, 0xCA, 0x55, 0xAA, 0x0F, 0x10):
        if tuple(c & m for _, m in sorted(mascaras.items())) in usados:
            continue
        img = bytearray(base)
        for o in mascaras:
            img[o] = c
        try:
            v = acha(bj.BJ8300(memmap.MemoryMapBytes(bytes(img))).get_settings(), nome).value
            if isinstance(v, settings.RadioSettingValueBoolean):
                atual = bool(v)
            elif isinstance(v, settings.RadioSettingValueInteger):
                atual = int(v)
            else:
                atual = str(v)
            return [x for _, x in ops].index(atual)
        except Exception:
            return None
    return None


def main():
    base = imagem_base()
    linhas = []
    for endereco, magia, tam in REGISTROS:
        linhas.append('R\t%X\t%s\t%d' % (endereco, magia.decode('ascii'), tam))
    for lado, endereco in (('A', 0x8000), ('B', 0x8000 + bj.VFO_LEN), ('C', bj.VFC_SECTOR + 4)):
        linhas.append('F\t%s\t%X' % (lado, endereco))
    for nome, campo in (('CALLSIGN', bj.APRS_CALL), ('TO', bj.APRS_TO), ('APRS MESSAGE', bj.APRS_MESSAGE),
                        ('MORSE MESSAGE', bj.MORSE_TEXT)):
        registro = bj.APRS_SECTOR if campo is not bj.MORSE_TEXT else bj.MORSE_SECTOR
        linhas.append('T\t%s\t%X\t%d\t%d\t%d' % (nome, registro, campo[1], campo[2], campo[3]))
    for pasta, itens in MENU:
        linhas.append('P\t%s' % pasta)
        for nome, lado, ajuste, filtro in itens:
            valores, fora = item(base, ajuste, filtro, ajuste == 'aprs_decode')
            textos_chirp = [texto_do_chirp(t, filtro) for t, _ in valores]
            radio = TEXTOS_DO_RADIO.get(nome)
            textos = radio if radio and mesmo(radio, textos_chirp) else textos_chirp
            linhas.append('I\t%s\t%s\t%s' % (nome, lado, '' if fora is None else fora))
            for texto, (_, b) in zip(textos, valores):
                linhas.append('V\t%s\t%s' % (texto, b))
    with open(SAIDA, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(linhas) + '\n')
    print('%s: %d linhas' % (SAIDA, len(linhas)))


if __name__ == '__main__':
    main()
