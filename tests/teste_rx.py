"""O laco do APRS RX (firmware/src/mensagem.c, aprs_rx_grava) inteiro no PC (v75).

Compila o mensagem.c com o tests/teste_rx.h (TESTE_RX: o modem do BK4819, a flash, as teclas e a tela
imitados, como estao mapeados) e roda o laco com transmissoes montadas aqui, conferindo:
  1. dois quadros na mesma transmissao, separados por uma flag so, entram os dois (da v71 a v73 o
     segundo se perdia);
  2. o preambulo parado em 2200 Hz (a outra polaridade) tambem sincroniza;
  3. a copia do mesmo quadro (o digipeater) nao entra de novo;
  4. a mensagem para mim com numero sai do laco com o ack armado, e o ack e ":ORIGEM   :ackNUMERO";
  5. o historico em anel guarda os ultimos 31 a 62, em ordem, e o da v74 ("HST2") e apagado;
  6. a tela, gastando 15 ms por faixa, nao faz a FIFO transbordar numa rajada;
  7. as teclas: o * apaga a luz e a tecla seguinte so a acende; a seta da a volta; segurada, repete;
     o EXIT segurado apaga o historico e o curto sai.
Uso: python -I tests/teste_rx.py   (precisa do zig: pip install ziglang)
"""
import os
import random
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
falhas = []


def confere(nome, obtido, esperado):
    ok = obtido == esperado
    print(f'{nome}: {"ok" if ok else "FALHOU"}')
    if not ok:
        print(f'    obtido:   {obtido!r}\n    esperado: {esperado!r}')
        falhas.append(nome)


_temporaria = tempfile.TemporaryDirectory(ignore_cleanup_errors=True)
pasta = _temporaria.name
exe = os.path.join(pasta, 'teste_rx.exe')
r = subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O1', '-Wall', '-Wextra', '-DTESTE_RX',
                    '-I', os.path.join(RAIZ, 'tests'), '-o', exe, os.path.join(RAIZ, 'tests', 'teste_rx.c')],
                   capture_output=True, text=True)
if r.returncode or 'warning' in r.stderr:
    sys.exit('nao compilou limpo:\n' + r.stderr)


# ---------- os quadros (AX.25 UI, como um TNC os manda) ----------
def crc_x25(dados):
    c = 0xFFFF
    for b in dados:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0x8408 if c & 1 else c >> 1
    return c ^ 0xFFFF


def endereco(ind, comando=False, ultimo=False):
    nome, _, ssid = ind.upper().partition('-')
    return (bytes(ord(c) << 1 for c in nome[:6].ljust(6)) +
            bytes([0x60 | (int(ssid or 0) & 15) << 1 | (0x80 if comando else 0) | (1 if ultimo else 0)]))


def quadro(origem, info, digis=('WIDE1-1',)):
    q = endereco('APRS', comando=True) + endereco(origem, ultimo=not digis)
    for i, d in enumerate(digis):
        q += endereco(d, ultimo=i == len(digis) - 1)
    q += b'\x03\xf0' + info.encode('latin-1')
    c = crc_x25(q)
    return q + bytes([c & 0xFF, c >> 8])


def transmissao(quadros, parado_na_marca=True, preambulo=40, entre=1):
    """Os bits de linha de uma transmissao: o preambulo de flags, os quadros separados por `entre` flags
    e duas de fecho. A linha (1 = marca, 1200 Hz) troca a cada bit 0 (NRZI); parado_na_marca escolhe a
    polaridade das flags (o "tipo B", que o FE FE pega; senao o "tipo A")."""
    bits = []

    def flags(n):
        for _ in range(n):
            bits.extend((0x7E >> k) & 1 for k in range(8))

    flags(preambulo)
    for i, q in enumerate(quadros):
        uns = 0
        for v in q:
            for k in range(8):
                b = (v >> k) & 1
                bits.append(b)
                uns = uns + 1 if b else 0
                if uns == 5:
                    bits.append(0)
                    uns = 0
        flags(entre if i < len(quadros) - 1 else 2)
    linha, nivel = [], 0 if parado_na_marca else 1
    for b in bits:
        if not b:
            nivel ^= 1
        linha.append(nivel)
    return linha


def ar_com(*partes, ruido=True):
    """Monta o ar: (segundo em que comeca, bits de linha) por transmissao, ruido no resto (no ruido o
    modem as vezes sincroniza por acaso e captura 13,6 s, como no radio; sem ruido, a linha parada)."""
    rnd = random.Random(75)
    ar = []
    for comeco, linha in sorted(partes, key=lambda p: p[0]):
        alvo = int(comeco * 1200)
        while len(ar) < alvo:
            ar.append(rnd.getrandbits(1) if ruido else 1)
        ar.extend(linha)
    return ar


TECLA = {'OK': 2, 'CIMA': 3, 'BAIXO': 4, 'EXIT': 5, 'ESTRELA': 9, 'ZERO': 13, 'MODO': 17}


def roda(ar, teclas, meu='PY2ABC', ssid=7, faixa_us=15000, flash_antes=None, nome='ar'):
    arq = os.path.join(pasta, nome + '.txt')
    open(arq, 'w').write(''.join(map(str, ar)))
    rot = ','.join(f'{int(de * 1000)}:{int(ate * 1000)}:{TECLA[t]}' for de, ate, t in teclas) or '-'
    antes = '-'
    if flash_antes is not None:
        antes = os.path.join(pasta, nome + '.antes')
        open(antes, 'wb').write(flash_antes)
    depois = os.path.join(pasta, nome + '.depois')
    s = subprocess.run([exe, arq, rot, meu, str(ssid), str(faixa_us), antes, depois], capture_output=True,
                       text=True, timeout=120)
    if s.returncode:
        sys.exit(f'o laco falhou ({s.returncode}):\n{s.stdout}\n{s.stderr}')
    saida = {'R': [], 'A': None}
    for l in s.stdout.splitlines():
        if l.startswith('R '):
            saida['R'].append(l[2:])
        elif l.startswith('H '):
            saida['H'] = int(l[2:])
        elif l.startswith('E '):
            saida['E'] = tuple(int(x) for x in l[2:].split())
        elif l.startswith('A '):
            saida['A'] = bytes.fromhex(l[2:])
        elif l.startswith('F '):
            saida.setdefault('F', []).append(tuple(int(x, 16) for x in l[2:].split()))
        elif l.startswith('G '):
            g = l[2:].split()
            saida['G'] = (tuple(int(x, 16) for x in g[:4]), int(g[4]))
        elif l.startswith('M'):
            saida['M'] = [int(x, 16) for x in l[1:].split()]
        elif l.startswith('C '):
            c = [int(x) for x in l[2:].split()]
            saida['C'] = dict(zip(('marca', 'espaco', 'bad', 'fix', 'transbordou', 'faixas', 'ms', 'luz', 'pos'), c))
    saida['flash'] = open(depois, 'rb').read()
    return saida


SAI = (0.0, 0.0, 'OK')                         # (o OK que abriu a tela, ja solto)


def sai_em(t):
    return [(t, t + 0.1, 'EXIT')]


# 1 e 6. dois quadros colados (uma flag entre eles), com a tela gastando 15 ms por faixa
a1 = quadro('PU2XYZ-9', '!2300.00S/04600.00W>primeiro')
a2 = quadro('PU2XYZ-9', '>segundo, colado ao primeiro')
s = roda(ar_com((1.0, transmissao([a1, a2], entre=1))), sai_em(4.0), nome='colados')
confere('1. dois quadros com uma flag entre eles: entram os dois',
        s['R'], ['0 PU2XYZ-9: 23 00.00S 046 00.00W CAR primeiro', '0 PU2XYZ-9: >segundo, colado ao primeiro'])
confere('6. a FIFO nao transborda com a tela gastando 15 ms por faixa', s['C']['transbordou'], 0)

# 2. as duas polaridades do preambulo, cada uma sozinha (dentro dos 13,6 s de captura de um sync, o quadro
# seguinte entra sem sync nenhum, de qualquer polaridade)
for parado, nome, chave in ((True, 'marca', 'marca'), (False, 'espaco', 'espaco')):
    s = roda(ar_com((1.0, transmissao([quadro('PU2AAA-1', '>parado na ' + nome)], parado_na_marca=parado)),
                    ruido=False), sai_em(4.0), nome='polaridade_' + nome)
    confere(f'2. o preambulo parado na {nome} sincroniza e entra', (s['R'], s['C'][chave] >= 1),
            (['0 PU2AAA-1: >parado na ' + nome], True))
s = roda(ar_com((1.0, transmissao([quadro('PU2AAA-1', '>um')], parado_na_marca=True)),
                (3.0, transmissao([quadro('PU2BBB-2', '>outro')], parado_na_marca=False))), sai_em(6.0),
         nome='polaridades')
confere('2. dentro da captura, a outra polaridade entra pelos mesmos bits', s['R'],
        ['0 PU2AAA-1: >um', '0 PU2BBB-2: >outro'])

# 3. o mesmo quadro de novo 1 s depois (o digipeater): uma vez so; 40 s depois, entra de novo
c1 = quadro('PU2CCC-3', '>uma vez so')
s = roda(ar_com((1.0, transmissao([c1])), (2.5, transmissao([c1])), (45.0, transmissao([c1]))), sai_em(48.0),
         nome='copias')
confere('3. a copia em 30 s nao entra; depois de 30 s, entra', s['R'], ['0 PU2CCC-3: >uma vez so'] * 2)

# 4. a mensagem para mim com numero: o laco sai com o ack armado
m1 = quadro('PU2XYZ-9', ':PY2ABC-7 :Oi, tudo bem?{12')
s = roda(ar_com((1.0, transmissao([m1]))), sai_em(9.0), nome='ack')
confere('4. a mensagem para mim entra em ambar', s['R'], ['1 PU2XYZ-9> Oi, tudo bem?'])
confere('4. o laco saiu antes do EXIT, com o ack armado', (s['E'][0], s['C']['ms'] < 8000), (ord('K'), True))
info = s['A'][s['A'].index(b'\x03\xf0') + 2:-2] if s['A'] else b''   # sem o FCS
confere('4. o ack: ":PU2XYZ-9 :ack12", do meu indicativo', (info, s['A'][7:14] if s['A'] else b''),
        (b':PU2XYZ-9 :ack12', endereco('PY2ABC-7', ultimo=False)[:6] + bytes([s['A'][13]]) if s['A'] else b''))
m2 = quadro('PU2XYZ-9', ':PY5ZZZ   :para outro{3')
s = roda(ar_com((1.0, transmissao([m2]))), sai_em(4.0), nome='sem_ack')
confere('4. a mensagem para outro nao arma ack', s['E'][0], 0)

# 5. o anel: 70 quadros, em rajadas de 5; ficam os ultimos, em ordem
partes = [(1.0 + 3 * k, transmissao([quadro('PU2D-1', f'>n{5 * k + j:03d}') for j in range(5)])) for k in range(14)]
s = roda(ar_com(*partes), sai_em(1.0 + 3 * 14 + 2), nome='anel')
ultimos = [int(t[-3:]) for t in s['R']]
confere('5. depois de 70, o historico tem de 31 a 62, os ultimos e em ordem',
        (31 <= s['H'] <= 62, ultimos == list(range(70 - s['H'], 70))), (True, True))
confere('5. a rajada que caiu na troca de setor nao transbordou', s['C']['transbordou'], 0)

# 5. o historico da v74 ("HST2") e apagado na entrada
velho = bytearray(b'\xff' * 0x10000)
velho[0x6000:0x6004] = (0x32545348).to_bytes(4, 'little')
velho[0x6080] = 0x5A
s = roda(ar_com((1.0, transmissao([quadro('PU2EEE-5', '>novo')]))), sai_em(3.0), flash_antes=bytes(velho),
         nome='migra')
confere('5. o "HST2" da v74 vira o "HST3", com so o quadro novo', (s['flash'][0x6000:0x6004], s['R']),
        ((0x33545348).to_bytes(4, 'little'), ['0 PU2EEE-5: >novo']))
outro = bytearray(b'\xff' * 0x10000)
outro[0x7000:0x7004] = b'XYZW'                   # o 0xD7000 com outra coisa: nada e gravado
s = roda(ar_com((1.0, transmissao([quadro('PU2EEE-5', '>novo')]))), sai_em(3.0), flash_antes=bytes(outro),
         nome='alheio')
confere('5. com o 0xD7000 ocupado por outra coisa, nada e gravado nele nem no 0xD6000',
        (s['H'], s['flash'][0x7000:0x7004], s['flash'][0x6000:0x6004]), (-1, b'XYZW', b'\xff' * 4))

# 7. as teclas, com 5 quadros no historico
partes = [(1.0, transmissao([quadro('PU2F-1', f'>t{j}') for j in range(5)]))]
s = roda(ar_com(*partes), [(3.0, 3.1, 'ESTRELA'), (4.0, 4.1, 'EXIT')] + sai_em(7.0), nome='estrela')
confere('7. o * apaga a luz; a tecla seguinte (o EXIT) so a acende, e o laco segue ate o EXIT de 7 s',
        (s['C']['luz'], s['C']['ms'] >= 7000), (1, True))
s = roda(ar_com(*partes), [(3.0, 3.1, 'OK'), (4.0, 4.1, 'EXIT')] + sai_em(7.0), nome='aberta')
confere('7. com a mensagem aberta, o EXIT curto a fecha, e o seguinte sai', s['C']['ms'] >= 7000, True)
s = roda(ar_com(*partes), [(3.0, 3.1, 'CIMA')] + sai_em(5.0), nome='volta')
confere('7. a seta para cima no mais novo vai ao mais velho', s['C']['pos'], 5)
s = roda(ar_com(*partes), [(3.0, 5.0, 'BAIXO')] + sai_em(6.0), nome='repete')
confere('7. a seta segurada 2 s repete (mais de uma volta em 5)', s['C']['pos'] != 2, True)
s = roda(ar_com(*partes), [(3.0, 3.8, 'EXIT')] + sai_em(5.0), nome='limpa')
confere('7. o EXIT segurado apaga o historico e o laco segue', (s['H'], s['C']['ms'] >= 5000), (0, True))

# 8. o modem: o 0x58 montado campo a campo, pelos comentarios do driver do BK4819 do UV-K5 (o modo de RX 7
# = FFSK 1200/1800, a banda 1, os bits 7:6 em 3), com o ganho do MODEM GAIN (HIGH de fabrica, a RAM zerada); a
# saida desliga o modem. O # (v103: nao troca mais o ganho) sai da tela, como as outras teclas sem funcao.
def reg58(rx, banda, b76, ganho):
    return 1 << 13 | rx << 10 | ganho << 8 | b76 << 6 | banda << 1 | 1


s = roda(ar_com(), sai_em(2.0), nome='ganho')
confere('8. a entrada poe o ganho do MODEM GAIN, e a saida desliga o modem', s['M'], [reg58(7, 1, 3, 3), 0])
s = roda(ar_com(), [(1.0, 1.1, 'MODO')], nome='cerquilha')
confere('8. o # sai da tela', (s['M'], s['C']['ms'] < 1500), ([reg58(7, 1, 3, 3), 0], True))

# 9. o FILTER (v102; sempre ligado desde a v103): a entrada poe o filtro largo do espectro (0x43 = 0x49A8),
# FM sem scramble nem compressor (0x31), o expansor e a porta desligados (0x28 = 0x0A00) e os filtros de
# audio do RX de fabrica com o cancelamento de corrente continua (0x2B = 0x4000); a saida devolve o canal e
# os menus. Cada foto e o que estava no comeco da tecla.
s = roda(ar_com(), [(1.0, 1.1, 'CIMA')] + sai_em(2.0), nome='filtro')
antes, ligado = (0x3028, 0x000A, 0x2A40, 0x0100), (0x49A8, 0x0000, 0x0A00, 0x4000)
confere('9. a entrada liga o FILTER, e a saida devolve o canal e os menus',
        (s['F'], s['G']), ([ligado, ligado], (antes, 1)))

print()
print('tudo certo' if not falhas else f'{len(falhas)} FALHARAM: ' + ', '.join(falhas))
sys.exit(1 if falhas else 0)
