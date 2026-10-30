"""Confere o firmware/src/mensagem.c (Morse e APRS) no PC, contra implementacoes independentes daqui.

Compila o C com o zig (pip install ziglang), com o chip de radio e o relogio imitados
(tests/teste_mensagem.h), e confere:
  1. o quadro AX.25 (enderecos, caminho, FCS) contra um montador escrito aqui;
  2. o AFSK: as trocas de tom que o C manda ao chip viram audio (a 48 kHz, com a fase continua, como o
     gerador de tom faz), e um demodulador escrito aqui (correlacao em 1200 e 2200 Hz, relogio tirado
     das transicoes, NRZI, destuffing, flags e FCS) tem de achar o quadro certo. O audio fica em
     firmware/build/aprs.wav, para conferir num decodificador de APRS (o atest do Direwolf, por exemplo);
  3. o decodificador HDLC do C, com bits de linha montados aqui: quadros nas duas polaridades, ruido
     entre eles, um com um bit errado (nao pode passar) e um abortado;
  4. o Morse: os tempos de tom ligado e desligado viram pontos, tracos e espacos, e o texto tem de
     voltar igual.
Uso: python -I tests/teste_mensagem.py
"""
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
import wave

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAIDA = os.path.join(RAIZ, 'firmware', 'build')       # fora do git, como o resto do build
falhas = []


def confere(nome, obtido, esperado):
    ok = obtido == esperado
    print(f'{nome}: {"ok" if ok else "FALHOU"}')
    if not ok:
        print(f'    obtido:   {obtido!r}\n    esperado: {esperado!r}')
        falhas.append(nome)


# ---------- o programa em C ----------
_temporaria = tempfile.TemporaryDirectory(ignore_cleanup_errors=True)   # apagada no fim (v74)
pasta = _temporaria.name
exe = os.path.join(pasta, 'teste_mensagem.exe')
r = subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O1', '-Wall', '-Wextra', '-DTESTE_PC',
                    '-I', os.path.join(RAIZ, 'tests'), '-o', exe,
                    os.path.join(RAIZ, 'tests', 'teste_mensagem.c')], capture_output=True, text=True)
if r.returncode:
    sys.exit('nao compilou:\n' + r.stderr)
if r.stderr.strip():
    print(r.stderr)


def roda(*args):
    """Roda o programa; nas linhas "R", desfaz a volta dos 32 bits do relogio (ele comeca perto dela)."""
    saida, volta, ant = [], 0, None
    for l in subprocess.run([exe, *map(str, args)], capture_output=True, text=True, check=True).stdout.splitlines():
        c = l.split()
        if c and c[0] == 'R':
            t = int(c[1])
            if ant is not None and t < ant:
                volta += 1 << 32
            ant = t
            l = f'R {t + volta} {c[2]} {c[3]}'
        saida.append(l)
    return ''.join(l + '\n' for l in saida)


# ---------- 1. AX.25, montado aqui ----------
def crc_x25(dados):
    c = 0xFFFF
    for b in dados:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0x8408 if c & 1 else c >> 1
    return c ^ 0xFFFF


assert crc_x25(b'123456789') == 0x906E          # o valor de conferencia do CRC-16/X.25


def endereco(ind, comando=False, ultimo=False):
    nome, _, ssid = ind.upper().partition('-')
    return (bytes(ord(c) << 1 for c in nome[:6].ljust(6)) +
            bytes([0x60 | (int(ssid or 0) & 15) << 1 | (0x80 if comando else 0) | (1 if ultimo else 0)]))


def quadro_ref(origem, caminho, info):
    digis = ['WIDE1-1', 'WIDE2-1'][:caminho]
    q = endereco('APZBJ8', comando=True) + endereco(origem, ultimo=not digis)
    for i, d in enumerate(digis):
        q += endereco(d, ultimo=i == len(digis) - 1)
    q += b'\x03\xf0' + info.encode('latin-1')
    c = crc_x25(q)
    return q + bytes([c & 0xFF, c >> 8])


CASOS = [('PY2ABC-7', 2, ':PU2XYZ   :Ola, teste do BJ-8300{1'),
         ('PY2ABC', 0, '>BJ-8300 V2.9T'),
         ('pu2xyz-15', 1, ':BLN1     :boletim'),
         ('PP5ABCDEF-3', 1, '>indicativo longo, cortado em 6')]
for origem, caminho, info in CASOS:
    q = roda('quadro', origem, caminho, info).split()[1]
    confere(f'1. quadro {origem} caminho {caminho}', q, quadro_ref(origem, caminho, info).hex())


# ---------- 2. AFSK: do log do chip ao audio, e de volta ----------
TAXA = 48000


def unidade_hz(v):
    return v / 10.32444


def audio_do_log(log):
    """As escritas no reg 0x71 viram um tom de fase continua (o tom 2 fica em 0, como no codigo)."""
    trocas = [(int(t), int(v)) for c, t, reg, v in (l.split() for l in log.splitlines())
              if c == 'R' and int(reg) == 0x71]
    t0, fim = trocas[0][0], trocas[-1][0] + 2000
    amostras, fase, k = [], 0.0, 0
    f = unidade_hz(trocas[0][1])
    for i in range(int((fim - t0) * TAXA / 1e6)):
        t = t0 + i * 1e6 / TAXA
        while k + 1 < len(trocas) and trocas[k + 1][0] <= t:
            k += 1
            f = unidade_hz(trocas[k][1])
        fase += 2 * math.pi * f / TAXA
        amostras.append(math.sin(fase) if f else 0.0)
    return amostras


def demodula(amostras, taxa=TAXA):
    """Correlacao numa janela de um bit em 1200 e 2200 Hz; o relogio sai das transicoes."""
    n = round(taxa / 1200)
    def energia(i0, f):
        re = sum(amostras[i] * math.cos(2 * math.pi * f * i / taxa) for i in range(i0, i0 + n))
        im = sum(amostras[i] * math.sin(2 * math.pi * f * i / taxa) for i in range(i0, i0 + n))
        return re * re + im * im
    passo = 4 if taxa > 20000 else 1
    tons = [energia(i, 2200) > energia(i, 1200) for i in range(0, len(amostras) - n, passo)]
    trocas = [i * passo + n / 2 for i in range(1, len(tons)) if tons[i] != tons[i - 1]]
    bit = taxa / 1200
    fases = sorted((t % bit) for t in trocas)
    fase = fases[len(fases) // 2]
    linha = []
    t = fase + bit / 2
    while t + n < len(amostras):
        i = int(t - n / 2) // passo
        if 0 <= i < len(tons):
            linha.append(0 if tons[i] else 1)
        t += bit
    return linha


def hdlc_ref(linha):
    """NRZI (sem troca = 1), destuffing e flags; devolve os quadros com FCS certo, sem o FCS."""
    bits, ant = [], linha[0]
    for b in linha[1:]:
        bits.append(1 if b == ant else 0)
        ant = b
    quadros, atual, uns, dentro = [], [], 0, False
    for b in bits:
        if b:
            uns += 1
            atual.append(1)
            if uns > 6:
                dentro, atual = False, []
            continue
        if uns == 6:
            corpo = atual[:-7]
            if dentro and len(corpo) % 8 == 0 and len(corpo) >= 17 * 8:
                q = bytes(sum(corpo[i + k] << k for k in range(8)) for i in range(0, len(corpo), 8))
                if crc_x25(q[:-2]) == q[-2] | q[-1] << 8:
                    quadros.append(q[:-2])
            atual, dentro, uns = [], True, 0
            continue
        if uns == 5:
            uns = 0
            continue
        uns = 0
        atual.append(0)
    return quadros


os.makedirs(SAIDA, exist_ok=True)
for origem, caminho, info in CASOS[:2]:
    log = roda('afsk', origem, caminho, info)
    amostras = audio_do_log(log)
    achados = hdlc_ref(demodula(amostras))
    confere(f'2. AFSK {origem}: o demodulador acha o quadro', achados, [quadro_ref(origem, caminho, info)[:-2]])
    if caminho == 2:
        with wave.open(os.path.join(SAIDA, 'aprs.wav'), 'wb') as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(TAXA)
            silencio = [0.0] * (TAXA // 4)
            w.writeframes(b''.join(struct.pack('<h', int(a * 20000)) for a in silencio + amostras + silencio))
        trocas = [int(l.split()[1]) for l in log.splitlines() if l.split()[2] == str(0x71)]
        tempos = [b - a for a, b in zip(trocas[1:], trocas[2:])]
        bit = 1e6 / 1200
        erro = max(abs(d - round(d / bit) * bit) for d in tempos)
        confere('2. AFSK: cada troca de tom cai a menos de 10 us da grade de 833,3 us', erro < 10, True)

# ---------- 2b. o AFSK pelo DAC (v86): as amostras de 16 kHz no lugar das do microfone ----------
DAC_TAXA = 16000
for origem, caminho, info in CASOS[:2]:
    amostras = [int(v) for l in roda('dac', origem, caminho, info).splitlines() if l.startswith('A ')
                for v in l.split()[1:]]
    achados = hdlc_ref(demodula([a / 8000 for a in amostras], DAC_TAXA))
    confere(f'2b. DAC {origem}: o demodulador acha o quadro', achados, [quadro_ref(origem, caminho, info)[:-2]])
    tom = [i for i, a in enumerate(amostras) if a]
    trecho = amostras[tom[0]:tom[-1] + 1]
    salto = max(abs(b - a) for a, b in zip(trecho, trecho[1:]))
    confere('2b. DAC: a fase continua (nenhum salto maior que o do 2200 Hz)', salto <= 6912, True)
    confere('2b. DAC: 0,1 s mudo antes do primeiro bit', tom[0] in range(1600, 1605), True)
    if caminho == 2:
        with wave.open(os.path.join(SAIDA, 'aprs_dac.wav'), 'wb') as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(DAC_TAXA)
            w.writeframes(b''.join(struct.pack('<h', a) for a in amostras + [0] * (DAC_TAXA // 4)))


# ---------- 3. o decodificador HDLC do C ----------
def linha_de(quadros_com_fcs, polaridade, ruido, rnd):
    bits = []
    def poe(v, stuffing):
        nonlocal uns
        for k in range(8):
            b = v >> k & 1
            bits.append(b)
            if stuffing:
                uns = uns + 1 if b else 0
                if uns == 5:
                    bits.append(0)
                    uns = 0
    linha, nivel = [], polaridade
    for q in quadros_com_fcs:
        bits.extend(rnd.getrandbits(1) for _ in range(ruido))
        uns = 0
        for _ in range(8):
            poe(0x7E, False)
        for v in q:
            poe(v, True)
        poe(0x7E, False)
        poe(0x7E, False)
    for b in bits:
        if not b:
            nivel ^= 1
        linha.append(nivel)
    return linha


def decodifica(linha, maximo, nome):
    """O decodificador do C: os quadros achados e os contadores (FCS errado, consertados)."""
    arq = os.path.join(pasta, nome)
    open(arq, 'w').write(''.join(map(str, linha)))
    saida = roda('hdlc', arq, maximo).splitlines()
    return ([bytes.fromhex(l.split()[1]) for l in saida if l.startswith('Q')],
            tuple(int(x) for x in saida[-1].split()[1:]))


rnd = random.Random(8300)
bons = [quadro_ref(o, c, i) for o, c, i in CASOS]
errado = bytearray(bons[0])
errado[20] ^= 0x18                         # dois bits vizinhos (o que um tom trocado faz): a correcao o acha
abortado = bons[1][:12]                    # cortado no meio (sem FCS valido)
lixo = bytes(rnd.getrandbits(8) & 0xFE for _ in range(30))   # 30 bytes que nao sao APRS
for polaridade in (0, 1):
    linha = linha_de([bons[0], bytes(errado), bons[1], abortado, bons[2], lixo, bons[3]], polaridade, 200, rnd)
    achados, (ruins, consertados) = decodifica(linha, 330, f'linha{polaridade}.txt')
    confere(f'3. HDLC polaridade {polaridade}: os bons e o de dois bits vizinhos consertado, sem o lixo',
            achados, [bons[i][:-2] for i in (0, 0, 1, 2, 3)])
    confere(f'3. polaridade {polaridade}: um consertado, e o lixo (sem cabecalho de APRS) fora do BAD',
            (consertados, ruins), (1, 0))
    confere(f'3. a referencia daqui concorda nos bons (polaridade {polaridade})', hdlc_ref(linha),
            [bons[i][:-2] for i in (0, 1, 2, 3)])

# um tom trocado no ar: um bit de linha errado vira dois bits errados seguidos depois do NRZI
linha = linha_de([bons[1]], 0, 0, rnd)
linha[8 * 8 + 20 * 8 + 3] ^= 1             # no meio do quadro, depois das 8 flags
achados, (ruins, consertados) = decodifica(linha, 330, 'tom.txt')
confere('3. um tom trocado (dois bits vizinhos) e consertado', (achados, consertados), ([bons[1][:-2]], 1))

# dois tons trocados longe um do outro: a correcao nao conserta, e o quadro conta como BAD (v74)
linha = linha_de([bons[1]], 0, 0, rnd)
linha[8 * 8 + 20 * 8 + 3] ^= 1
linha[8 * 8 + (len(bons[1]) - 4) * 8 + 5] ^= 1   # perto do fim, longe do primeiro
achados, (ruins, consertados) = decodifica(linha, 160, 'dois.txt')
confere('3. dois tons trocados: recusado e contado como BAD', (achados, ruins, consertados), ([], 1, 0))

# a correcao nao inventa quadro (v74): 40 quadros com dois tons trocados, nenhum aceito errado
erros = 0
for k in range(40):
    linha = linha_de([bons[k % 4]], 0, 0, rnd)
    for _ in range(2):
        linha[8 * 8 + 18 * 8 + rnd.randrange(len(linha) - 8 * 8 - 18 * 8 - 3 * 8)] ^= 1
    achados, _ = decodifica(linha, 160, 'varios.txt')
    erros += sum(q != bons[k % 4][:-2] for q in achados)
confere('3. dois tons trocados em 40 quadros: nenhum aceito com conteudo errado', erros, 0)

# um quadro AX.25 certo que nao e APRS (controle 0x00): nao vai para a tela (v74)
outro = endereco('APZBJ8', comando=True) + endereco('PU2XYZ-9', ultimo=True) + b'\x00\xf0teste de outro'
outro += bytes([crc_x25(outro) & 0xFF, crc_x25(outro) >> 8])
achados, (ruins, _) = decodifica(linha_de([outro, bons[0]], 0, 50, rnd), 160, 'outro.txt')
confere('3. o quadro AX.25 que nao e APRS fica de fora', (achados, ruins), ([bons[0][:-2]], 0))

# quadro maior que o buffer: o FCS confere byte a byte, e so o comeco fica guardado
longo = quadro_ref('PY2ARS-14', 2, '>' + 'comentario comprido de digipeater ' * 6)
achados, _ = decodifica(linha_de([longo], 1, 50, rnd), 330, 'longo.txt')
confere(f'3. quadro de {len(longo)} B com o buffer de 330: inteiro', achados, [longo[:-2]])
achados, _ = decodifica(linha_de([longo], 1, 50, rnd), 64, 'longo64.txt')
confere(f'3. o mesmo com o buffer de 64: aceito, com os 64 primeiros bytes', achados, [longo[:64]])


# ---------- 4. Morse ----------
ITU = {'A': '.-', 'B': '-...', 'C': '-.-.', 'D': '-..', 'E': '.', 'F': '..-.', 'G': '--.', 'H': '....',
       'I': '..', 'J': '.---', 'K': '-.-', 'L': '.-..', 'M': '--', 'N': '-.', 'O': '---', 'P': '.--.',
       'Q': '--.-', 'R': '.-.', 'S': '...', 'T': '-', 'U': '..-', 'V': '...-', 'W': '.--', 'X': '-..-',
       'Y': '-.--', 'Z': '--..', '0': '-----', '1': '.----', '2': '..---', '3': '...--', '4': '....-',
       '5': '.....', '6': '-....', '7': '--...', '8': '---..', '9': '----.', '.': '.-.-.-', ',': '--..--',
       '?': '..--..', "'": '.----.', '!': '-.-.--', '/': '-..-.', '(': '-.--.', ')': '-.--.-', '&': '.-...',
       ':': '---...', ';': '-.-.-.', '=': '-...-', '+': '.-.-.', '-': '-....-', '"': '.-..-.',
       '$': '...-..-', '@': '.--.-.'}
DE_VOLTA = {v: k for k, v in ITU.items()}
for wpm, texto in ((20, 'CQ CQ de PY2ABC/8300 k'), (12, 'Ola, 73! (teste) 1+1=2 @ "ok" $5')):
    log = roda('morse', wpm, 700, texto)
    eventos = [(int(t), int(v)) for _, t, reg, v in (l.split() for l in log.splitlines()) if reg in ('113', '0')]
    u = 1200 / wpm * 1000
    sinais, letra, saida = [], '', []
    for (t, v), (t2, _) in zip(eventos, eventos[1:]):
        d = (t2 - t) / u
        if v:                                    # tom ligado: ponto (1) ou traco (3)
            letra += '.' if d < 2 else '-'
            sinais.append(round(d))
        elif d > 5:                              # 7: entre palavras
            saida.append(DE_VOLTA.get(letra, '#') + ' ')
            letra = ''
        elif d > 2:                              # 3: entre letras
            saida.append(DE_VOLTA.get(letra, '#'))
            letra = ''
    if letra:
        saida.append(DE_VOLTA.get(letra, '#'))
    confere(f'4. Morse a {wpm} wpm volta igual', ''.join(saida).strip(), texto.upper())
    confere(f'4. Morse a {wpm} wpm: so pontos de 1 e tracos de 3 unidades', sorted(set(sinais)), [1, 3])
    confere(f'4. Morse a {wpm} wpm: tom de 700 Hz', {round(unidade_hz(v)) for t, v in eventos if v},
            {700})

# ---------- 5. o quadro em texto legivel (a tela do APRS RX) ----------
def texto_e_id(origem, caminho, info, meu, destino='APZBJ8', bruto=False):
    q = quadro_ref(origem, caminho, info)[:-2]
    q = endereco(destino, comando=True) + q[7:]           # o destino (o Mic-E leva a latitude nele)
    t, i = roda('texto', q.hex(), meu, *(['bruto'] if bruto else [])).rstrip('\n').split('\n')
    return int(t[2]), t[4:], i[2:]


def texto(origem, caminho, info, meu):
    return texto_e_id(origem, caminho, info, meu)[:2]


for origem, info, meu, esperado in (
        ('PY2ARS-14', '!2318.07S/04635.59W#REDE APRS-SP-C', 'PY2ABC-7',
         (0, 'PY2ARS-14: 23 18.07S 046 35.59W DIGI REDE APRS-SP-C')),
        ('PU2XYZ-9', ':PY2ABC-7 :Ola tudo bem?{12', 'PY2ABC-7', (1, 'PU2XYZ-9> Ola tudo bem?')),
        ('PU2XYZ-9', ':PY5ZZZ   :Bom dia{3', 'PY2ABC-7', (0, 'PU2XYZ-9>PY5ZZZ: Bom dia{3')),
        ('PU2XYZ-9', ':PY2ABC-7 :ack12', 'PY2ABC-7', (1, 'PU2XYZ-9> ACK 12')),
        ('PY2ABC', '>BJ-8300 V2.9T', 'PY2ABC', (0, 'PY2ABC: >BJ-8300 V2.9T')),
        ('PY2MOV-9', '@092345z2318.07S/04635.59W>Movel', '', (0, 'PY2MOV-9: 23 18.07S 046 35.59W CAR Movel')),
        ('PY2ZZZ', '!23xx.07S/04635.59W#nao e posicao', '', (0, 'PY2ZZZ: !23xx.07S/04635.59W#nao e posicao')),
        # de terceiros (v71): o que um igate traz da internet, mostrado pela estacao de dentro
        ('PY2IGT-10', '}PU2XYZ-9>APRS,TCPIP,PY2IGT-10*:!2300.00S/04600.00W>Movel', '',
         (0, 'PU2XYZ-9: 23 00.00S 046 00.00W CAR Movel')),
        ('PY2IGT-10', '}PU2XYZ-9>APRS,TCPIP,PY2IGT-10*::PY2ABC-7 :Oi pela internet{5', 'PY2ABC-7',
         (1, 'PU2XYZ-9> Oi pela internet')),
        ('PY2IGT-10', '}sem seta nenhuma aqui', '', (0, 'PY2IGT-10: }sem seta nenhuma aqui')),
        ('PY2IGT-10', '}PU2XYZ-9>APRS sem dois pontos', '', (0, 'PU2XYZ-9:')),
        # v74: ate 120 caracteres, e as letras acentuadas (UTF-8) sem o acento
        ('PU2XYZ-9', ':PY2ABC-7 :' + 'Mensagem comprida de teste, ' * 4 + '{9', 'PY2ABC-7',
         (1, ('PU2XYZ-9> ' + 'Mensagem comprida de teste, ' * 4)[:104])),
        ('PU2XYZ-9', '>Estação móvel em São José, ÁÉÍÓÚ çãõ'.encode().decode('latin-1'), '',   # em UTF-8
         (0, 'PU2XYZ-9: >Estacao movel em Sao Jose, AEIOU cao')),
        # v74: so "ack" e o id e confirmacao; a tabela alternativa nao usa os nomes; hemisferio conferido;
        # graus e ordinal; byte de controle no destinatario
        ('PU2XYZ-9', ':PY5ZZZ   :ackermann chegou', '', (0, 'PU2XYZ-9>PY5ZZZ: ackermann chegou')),
        ('PU2XYZ-9', ':PY5ZZZ   :ack12345', '', (0, 'PU2XYZ-9>PY5ZZZ: ACK 12345')),
        ('PU2XYZ-9', '!2300.00S\\04600.00Wbpoeira', '', (0, 'PU2XYZ-9: 23 00.00S 046 00.00W b poeira')),
        ('PU2XYZ-9', '!2300.00X/04600.00Q>sem hemisferio', '', (0, 'PU2XYZ-9: !2300.00X/04600.00Q>sem hemisferio')),
        ('PU2XYZ-9', '>25°C, 1º lugar, 2ª'.encode().decode('latin-1'), '', (0, 'PU2XYZ-9: >25oC, 1o lugar, 2a')),
        ('PU2XYZ-9', ':PY5\x01ZZ   :oi', '', (0, 'PU2XYZ-9>PY5.ZZ: oi'))):
    confere(f'5. texto de {info[:22]!r}', texto(origem, 1, info, meu), esperado)

# ---------- os formatos decodificados (v76), contra codificadores daqui, escritos pela especificacao ----------
def minutos(valor):
    """Graus e centesimos de minuto de um valor positivo em graus, truncados como no radio."""
    g = int(valor)
    return g, int(round((valor - g) * 6000, 6))


def coord_txt(lat, lon):
    g, c = minutos(abs(lat))
    h, d = minutos(abs(lon))
    return (f'{g:02d} {c // 100:02d}.{c % 100:02d}{"N" if lat >= 0 else "S"} '
            f'{h:03d} {d // 100:02d}.{d % 100:02d}{"E" if lon >= 0 else "W"}')


def mic_e(lat, lon, nos, rumo, simbolo='>', tabela='/', mensagem=6, alt=None, tipo=''):
    """O destino e a info de um Mic-E (APRS 1.0.1, cap. 10)."""
    g, c = minutos(abs(lat))
    alg = f'{g:02d}{c:04d}'
    dest = ''
    for k, a in enumerate(alg):
        bit = k < 3 and mensagem >> (2 - k) & 1
        alto = bit or (k == 3 and lat >= 0) or (k == 5 and lon < 0)
        dest += chr(ord('P' if alto else '0') + int(a))
    gl, cl = minutos(abs(lon))
    if gl < 10:
        d, ofs = gl + 90, True
    elif gl < 100:
        d, ofs = gl, False
    elif gl < 110:
        d, ofs = gl - 20, True
    else:
        d, ofs = gl - 100, True
    if ofs:
        dest = dest[:4] + chr(ord(dest[4]) + ord('P') - ord('0')) + dest[5]
    m = cl // 100
    info = '`' + chr(d + 28) + chr(m + 28 if m >= 10 else m + 88) + chr(cl % 100 + 28)
    info += chr(nos // 10 + 28) + chr(nos % 10 * 10 + rumo // 100 + 28) + chr(rumo % 100 + 28)
    info += simbolo + tabela + tipo
    if alt is not None:
        v = alt + 10000
        info += chr(v // 8281 + 33) + chr(v // 91 % 91 + 33) + chr(v % 91 + 33) + '}'
    return dest, info


def comprimida(lat, lon, rumo=None, nos=None, tabela='/', simbolo='>'):
    """A posicao comprimida (APRS 1.0.1, cap. 9)."""
    def b91(v):
        return ''.join(chr(v // 91 ** k % 91 + 33) for k in (3, 2, 1, 0))
    y = round(380926 * (90 - lat))
    x = round(190463 * (180 + lon))
    cs = '  '
    if rumo is not None:
        cs = chr(rumo // 4 + 33) + chr(round(math.log(nos + 1) / math.log(1.08)) + 33)
    return tabela + b91(y) + b91(x) + simbolo + cs + '!', y, x


def comprimida_txt(y, x):
    """O que o radio mostra de y e x, com as contas da especificacao."""
    lat = 90 - y / 380926
    lon = x / 190463 - 180
    def f(v, div, casas, pos, neg):
        n = abs(round(v * div))
        g, c = n // div, n % div * 6000 // div
        return f'{g:0{casas}d} {c // 100:02d}.{c % 100:02d}{pos if v >= 0 else neg}'
    return f(lat, 380926, 2, 'N', 'S') + ' ' + f(lon, 190463, 3, 'E', 'W')


for nome, dest, info, esperado in (
        ('Mic-E', *mic_e(-23.3012, -46.5932, 60, 120, alt=760, tipo=']'),
         (0, f'PU2XYZ-9: {coord_txt(-23.3012, -46.5932)} CAR 111KM/H 120DEG ALT 760M EN ROUTE')),
        ('Mic-E com longitude de 1 algarismo e N/E', *mic_e(51.4775, 5.1208, 0, 0, mensagem=7),
         (0, f'PU2XYZ-9: {coord_txt(51.4775, 5.1208)} CAR OFF DUTY')),
        ('Mic-E com longitude acima de 100', *mic_e(-33.8688, 151.2093, 12, 359, simbolo='k', mensagem=0),
         (0, f'PU2XYZ-9: {coord_txt(-33.8688, 151.2093)} TRUCK 22KM/H 359DEG EMERGENCY')),
        ('Mic-E com comentario', mic_e(-22.7612, -47.1166, 5, 90)[0], mic_e(-22.7612, -47.1166, 5, 90)[1] + 'Ola',
         (0, f'PU2XYZ-9: {coord_txt(-22.7612, -47.1166)} CAR 9KM/H 90DEG EN ROUTE Ola'))):
    confere(f'6. {nome}', texto_e_id('PU2XYZ-9', 1, info, '', destino=dest)[:2], esperado)

cp, y, x = comprimida(-23.5, -46.6, rumo=88, nos=36)
v = 1000
for _ in range(round(math.log(37) / math.log(1.08))):
    v = v * 108 // 100
confere('6. posicao comprimida, com rumo e velocidade', texto('PU2XYZ-9', 1, '!' + cp + 'comprimida', ''),
        (0, f'PU2XYZ-9: {comprimida_txt(y, x)} CAR {((v - 1000 + 500) // 1000 * 1852 + 500) // 1000}KM/H 88DEG comprimida'))
cp, y, x = comprimida(48.8566, 2.3522, simbolo='-')
confere('6. posicao comprimida parada, N e E', texto('PU2XYZ-9', 1, '=' + cp, ''),
        (0, f'PU2XYZ-9: {comprimida_txt(y, x)} HOME'))

for nome, info, esperado in (
        ('rumo e velocidade', '!2318.07S/04635.59W>093/025/A=001234 Tracker',
         'PU2XYZ-9: 23 18.07S 046 35.59W CAR 46KM/H 93DEG ALT 376M Tracker'),
        ('PHG do digipeater', '!2318.07S/04635.59W#PHG5360/Digi',
         'PU2XYZ-9: 23 18.07S 046 35.59W DIGI 25W 24M 6DBI /Digi'),     # 5^2 W, 10 x 2^3 pes, 6 dB
        ('DAO e CR no fim', '!2318.07S/04635.59W-casa !wAB! fim\r', 'PU2XYZ-9: 23 18.07S 046 35.59W HOME casa  fim'),
        ('estacao de tempo', '@092345z2318.07S/04635.59W_220/004g005t077r000p000P000h50b10132',
         'PU2XYZ-9: 23 18.07S 046 35.59W WX 25C 50% 1013HPA 6KM/H 220DEG'),
        ('tempo sem posicao', '_10090556c220s004g005t-05r000h00b09850', 'PU2XYZ-9: WX -21C 100% 985HPA 6KM/H 220DEG'),
        ('objeto', ';FEIRA    *092345z2300.00S/04600.00W>sabado', 'PU2XYZ-9: OBJ FEIRA 23 00.00S 046 00.00W CAR sabado'),
        ('item', ')ARCO!2300.00S/04600.00W-portao', 'PU2XYZ-9: ITEM ARCO 23 00.00S 046 00.00W HOME portao'),
        ('altitude negativa', '!2300.00S/04600.00Ws/A=-00033 mar', 'PU2XYZ-9: 23 00.00S 046 00.00W BOAT ALT -10M mar')):
    confere(f'6. {nome}', texto('PU2XYZ-9', 1, info, ''), (0, esperado))

dest, info = mic_e(-23.3012, -46.5932, 60, 120)
confere('6. Mic-E de terceiros (o destino em texto, de um igate)',
        texto('PY2IGT-10', 1, f'}}PU2XYZ-9>{dest},TCPIP,PY2IGT-10*:{info}', ''),
        (0, f'PU2XYZ-9: {coord_txt(-23.3012, -46.5932)} CAR 111KM/H 120DEG EN ROUTE'))
confere('6. com o APRS DECODE em OFF, a info como veio',
        texto_e_id('PU2XYZ-9', 1, '!2318.07S/04635.59W>093/025', '', bruto=True)[:2],
        (0, 'PU2XYZ-9: !2318.07S/04635.59W>093/025'))
confere('6. e a mensagem para mim continua com o numero para o ack',
        texto_e_id('PU2XYZ-9', 1, ':PY2ABC-7 :oi{4', 'PY2ABC-7', bruto=True), (1, 'PU2XYZ-9> oi', '4'))

# o numero da mensagem para o ack (v75): so na mensagem para mim, ate 5, sem o "}" da resposta embutida,
# mesmo quando o texto passa do limite da tela
for info, meu, esperado in ((':PY2ABC-7 :Ola tudo bem?{12', 'PY2ABC-7', '12'),
                            (':PY2ABC-7 :sem numero', 'PY2ABC-7', ''),
                            (':PY5ZZZ   :para outro{7', 'PY2ABC-7', ''),
                            (':PY2ABC-7 :resposta{AB}CD', 'PY2ABC-7', 'AB'),
                            (':PY2ABC-7 :ack12', 'PY2ABC-7', ''),
                            (':PY2ABC-7 :' + 'x' * 66 + '{99999', 'PY2ABC-7', '99999')):
    confere(f'5. numero para o ack de {info[:24]!r}', texto_e_id('PU2XYZ-9', 1, info, meu)[2], esperado)

print()
print('tudo certo' if not falhas else f'FALHARAM {len(falhas)}: {", ".join(falhas)}')
sys.exit(1 if falhas else 0)
