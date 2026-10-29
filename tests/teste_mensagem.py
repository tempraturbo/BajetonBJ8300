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
pasta = tempfile.mkdtemp()
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


def demodula(amostras):
    """Correlacao numa janela de um bit em 1200 e 2200 Hz; o relogio sai das transicoes."""
    n = round(TAXA / 1200)
    def energia(i0, f):
        re = sum(amostras[i] * math.cos(2 * math.pi * f * i / TAXA) for i in range(i0, i0 + n))
        im = sum(amostras[i] * math.sin(2 * math.pi * f * i / TAXA) for i in range(i0, i0 + n))
        return re * re + im * im
    passo = 4
    tons = [energia(i, 2200) > energia(i, 1200) for i in range(0, len(amostras) - n, passo)]
    trocas = [i * passo + n / 2 for i in range(1, len(tons)) if tons[i] != tons[i - 1]]
    bit = TAXA / 1200
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
errado[20] ^= 0x10                         # um bit trocado: o FCS nao bate, e a correcao o acha
abortado = bons[1][:12]                    # cortado no meio (sem FCS valido)
lixo = bytes(rnd.getrandbits(8) & 0xFE for _ in range(30))   # 30 bytes que nao sao APRS
for polaridade in (0, 1):
    linha = linha_de([bons[0], bytes(errado), bons[1], abortado, bons[2], lixo, bons[3]], polaridade, 200, rnd)
    achados, (ruins, consertados) = decodifica(linha, 330, f'linha{polaridade}.txt')
    confere(f'3. HDLC polaridade {polaridade}: os bons e o de um bit errado consertado, sem o lixo',
            achados, [bons[i][:-2] for i in (0, 0, 1, 2, 3)])
    confere(f'3. polaridade {polaridade}: um consertado, e o lixo contado como FCS errado',
            (consertados, ruins >= 1), (1, True))
    confere(f'3. a referencia daqui concorda nos bons (polaridade {polaridade})', hdlc_ref(linha),
            [bons[i][:-2] for i in (0, 1, 2, 3)])

# um tom trocado no ar: um bit de linha errado vira dois bits errados seguidos depois do NRZI
linha = linha_de([bons[1]], 0, 0, rnd)
linha[8 * 8 + 20 * 8 + 3] ^= 1             # no meio do quadro, depois das 8 flags
achados, (ruins, consertados) = decodifica(linha, 330, 'tom.txt')
confere('3. um tom trocado (dois bits vizinhos) e consertado', (achados, consertados), ([bons[1][:-2]], 1))

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
def texto(origem, caminho, info, meu):
    linha = roda('texto', quadro_ref(origem, caminho, info)[:-2].hex(), meu).rstrip('\n')
    return int(linha[2]), linha[4:]


for origem, info, meu, esperado in (
        ('PY2ARS-14', '!2318.07S/04635.59W#REDE APRS-SP-C', 'PY2ABC-7',
         (0, 'PY2ARS-14: 23 18.07S 046 35.59W DIGI REDE APRS-S')),
        ('PU2XYZ-9', ':PY2ABC-7 :Ola tudo bem?{12', 'PY2ABC-7', (1, 'PU2XYZ-9> Ola tudo bem?')),
        ('PU2XYZ-9', ':PY5ZZZ   :Bom dia{3', 'PY2ABC-7', (0, 'PU2XYZ-9>PY5ZZZ: Bom dia{3')),
        ('PU2XYZ-9', ':PY2ABC-7 :ack12', 'PY2ABC-7', (1, 'PU2XYZ-9> ACK 12')),
        ('PY2ABC', '>BJ-8300 V2.9T', 'PY2ABC', (0, 'PY2ABC: >BJ-8300 V2.9T')),
        ('PY2MOV-9', '@092345z2318.07S/04635.59W>Movel', '', (0, 'PY2MOV-9: 23 18.07S 046 35.59W CAR Movel')),
        ('PY2ZZZ', '!23xx.07S/04635.59W#nao e posicao', '', (0, 'PY2ZZZ: !23xx.07S/04635.59W#nao e posicao'))):
    confere(f'5. texto de {info[:22]!r}', texto(origem, 1, info, meu), esperado)

print()
print('tudo certo' if not falhas else f'FALHARAM {len(falhas)}: {", ".join(falhas)}')
sys.exit(1 if falhas else 0)
