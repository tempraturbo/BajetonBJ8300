"""Confere o controle remoto (firmware/src/remoto.c, v156 a v158) no PC: compila o C em 32 bits com o zig (pip install
ziglang), com a RAM num vetor e o firmware imitado (tests/teste_remoto.h), roda os cenarios do tests/teste_remoto.c e
confere:
  1. o quadro do PC achado em qualquer posicao do buffer, o mais novo valendo, a soma, a tecla fora da tabela, e o
     PROGRAMBT11U seguindo para o conferidor de fabrica;
  2. a tecla do remoto so com nenhuma fisica apertada, e a resposta (9 bytes) uma vez por quadro, com a soma;
  3. o vigia: a tecla e o PTT seguros ate 15 carimbos de 64 ms, soltos no 16o;
  4. o PTT pela porta falsa, com o PA12 em 0; com o remoto pelo cabo, o fio do PTT (o RX da serial) nao vale (v157);
  5. o pedido da tela inteira: so pelo cabo, fora da TX, a principal ou o menu, e a marca de perda limpa; senao a
     fatia fica perdida (v158);
  6. os bits de quem recebe (a letra e do app desde a v159);
  7. o espelho, decodificado aqui por um leitor escrito do formato (o mesmo do UV-17L: 01 'T' x0 x1 y0 y1, corridas
     de quantos e a cor com o byte alto primeiro, e o CRC16-XMODEM do 'T' ao fim), com o anel dando a volta: a imagem
     tem de voltar igual;
  8. o anel cheio: a linha se perde na hora, sem esperar (v158); a fatia por no (as linhas do no, o vigia parado no
     redesenho, a fatia que nao coube perdida); a perda de fora das fatias guardada ate a resposta (v159);
  9. nada vai com a lanterna, na sessao, nem com o espelho desligado (pelo BLE, so se pedido).
Uso: python -I tests/teste_remoto.py
"""
import os
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


def crc16(b):
    c = 0
    for x in b:
        c ^= x << 8
        for _ in range(8):
            c = (c << 1 ^ 0x1021 if c & 0x8000 else c << 1) & 0xFFFF
    return c


def quadros(dados):
    """Os quadros da tela num fluxo de bytes: (x0, x1, y0, y1, pixels) dos que conferem, e quantos nao conferiram."""
    saida, ruins, i = [], 0, 0
    while i < len(dados):
        if dados[i] != 1 or i + 6 > len(dados) or dados[i + 1] != ord('T'):
            i += 1
            continue
        x0, x1, y0, y1 = dados[i + 2:i + 6]
        n, p, px = (x1 - x0) * (y1 - y0), i + 6, []
        while n > 0 and p + 3 <= len(dados):
            c = dados[p]
            if c == 0 or c > n:
                break
            px += [dados[p + 1] << 8 | dados[p + 2]] * c
            n -= c
            p += 3
        if n or p + 2 > len(dados) or crc16(dados[i + 1:p]) != dados[p] << 8 | dados[p + 1]:
            ruins += 1
            i += 1
            continue
        saida.append((x0, x1, y0, y1, px))
        i = p + 2
    return saida, ruins


with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as pasta:
    exe = os.path.join(pasta, 'teste_remoto.exe')
    r = subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-target', 'x86-windows-gnu', '-O1', '-Wall',
                        '-Wextra', '-Wno-unused-function', '-I', os.path.join(RAIZ, 'tests'), '-o', exe,
                        os.path.join(RAIZ, 'tests', 'teste_remoto.c')], capture_output=True, text=True)
    if r.returncode:
        sys.exit('nao compilou:\n' + r.stderr)
    if r.stderr.strip():
        print(r.stderr)
    saida = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
linhas = {l.split()[0]: l.split()[1:] for l in saida.splitlines() if l.strip()}
v = lambda nome: [int(x) for x in linhas[nome]]

# 1. o quadro (REM: bits 4:0 a tecla + 1, 5 o PTT, 6 o espelho, 7 a resposta pendente)
confere('1. o quadro mais novo, no meio do lixo, sem o conferidor', v('quadro_novo'), [0, 0, 18 | 32 | 128])
confere('1. o PROGRAMBT11U segue para o conferidor (1 abre a sessao)', v('programbt'), [1, 1, 0])
confere('1. a soma errada nao vale', v('soma_errada'), [7, 1, 0])
confere('1. a tecla 18 (fora da tabela) e nenhuma', v('tecla_18'), [128])
confere('1. com 6 bytes nao ha quadro', v('seis_bytes'), [7, 1, 0])

# 2. a tecla e a resposta
confere('2. a tecla do remoto com nenhuma apertada; uma resposta de 9 bytes', v('tecla_remota'), [9, 1, 9])
resp = v('resposta')
confere('2. a resposta: ~ r, o REM sem o bit 7, tx, rx, sinal, lado, tensao', resp[:8], [126, 114, 10, 0, 4, 13, 0, 167])
confere('2. a soma da resposta', resp[8], sum(resp[:8]) & 255)
confere('2. a tecla fisica vale mais, e a resposta nao se repete', v('tecla_fisica'), [2, 1])

# 3. o vigia
confere('3. aos 15 carimbos a tecla e o PTT seguem seguros', v('vigia_15'), [4, 0])
confere('3. aos 16 (1 s) soltam, e o REM zera', v('vigia_16'), [255, 0x1000, 0])

# 4. o PTT
confere('4. o PTT do remoto: porta falsa com o PA12 em 0', v('ptt'), [0, 0])
confere('4. sem ele, a porta de fabrica', v('ptt_solto'), [0x1000, 1])
confere('4. v157: sem remoto, o fio apertado e o PTT', v('fio_sem_remoto'), [0])
confere('4. v157: com o remoto pelo cabo (reservados em 0xFF), o fio apertado nao vale', v('fio_cabo'), [0, 128, 0x1000])
confere('4. v157: pelo BLE, o fio do radio vale', v('fio_ble'), [0])
confere('4. v157: o PTT do quadro aperta pelo cabo', v('fio_cabo_ptt'), [0])

# 5. a tela inteira
confere('5. a principal (MODO_TELA 0), e a perda limpa', v('tela_principal'), [1, 0, 0])
confere('5. o menu (MODO_TELA 1), e a perda limpa', v('tela_menu'), [0, 1, 0])
confere('5. pelo BLE, nada, e a fatia perdida', v('tela_ble'), [0, 0, 64])
confere('5. na TX, nada, e a fatia perdida', v('tela_tx'), [0, 64])
confere('5. noutra tela (MODO_TELA 2), nada, e a fatia perdida', v('tela_outra'), [0, 0, 64])
confere('5. no meio de uma digitacao (v161), nada, e a fatia perdida', v('tela_digitando'), [0, 64])
confere('5. sem o pedido, a perda fica', v('perda_fica'), [64])

# 6. quem recebe, cru (v159; a letra e do app, tempralink/Remoto.pas): bits 1:0 a vista, 2 a evidencia no B, 3 a
# dupla escuta parada no outro, 4 o rodizio com a terceira no lugar
esperado = []
for vis, b, dw, cl in ((0, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 1, 1, 0), (2, 0, 1, 0), (2, 0, 1, 1), (3, 1, 0, 0),
                       (3, 0, 1, 0), (3, 0, 1, 1), (1, 0, 0, 0)):
    esperado.append(vis | b << 2 | dw << 3 | cl << 4)
confere('6. os bits de quem recebe: a vista, a evidencia, a dupla escuta no outro e o rodizio', v('quem'), esperado)

# 7. o espelho
confere('7. o espelho volta 0 (a de fabrica segue), solta a trava, sem perda', v('espelho'), [0, 0, 0])
bruto, escrita = bytes.fromhex(linhas['anel'][0]), int(linhas['anel'][1])
video = v('video')
qs, ruins = quadros(bruto)
confere('7. 8 quadros, um por linha, todos conferidos', (len(qs), ruins), (8, 0))
imagem = [p for q in sorted(qs, key=lambda q: q[2]) for p in q[4]]
confere('7. a janela (10..74) x (40..48)', sorted({q[:4] for q in qs}),
        [(10, 74, y, y + 1) for y in range(40, 48)])
confere('7. os pixels voltam iguais', imagem, video)
confere('7. o anel deu a volta e a escrita esta no fim do quadro', escrita, (2300 + len(bruto)) % 2400)

# 8. o anel cheio (v158: sem esperar)
confere('8. cheio: a linha se perde na hora, com as duas marcas (v159), e nada entra', v('anel_cheio'),
        [96, 1, 2390])
confere('8. com lugar para uma linha: ela vai, e a seguinte se perde', v('anel_uma'), [64, 1])

# 8b. a fatia por no
for no, linhas_no in ((11, range(48, 64)), (45, range(52, 56)), (1, range(128))):
    confere(f'8b. o no {no}: um redesenho, o REM como o quadro (tecla 4 + 1, espelho, resposta), o CONTA sem marca e '
            'com o carimbo, e no meio o REM vivo e o PTT do quadro', v(f'fatia{no}'), [1, 5 | 64 | 128, 0, 1, 3])
    qs, ruins = quadros(bytes.fromhex(linhas[f'anel_fatia{no}'][0]))
    confere(f'8b. o no {no}: so as linhas dele vao, inteiras, cada uma com a sua cor',
            (ruins, [(q[2], q[4][0]) for q in qs]), (0, [(y, y << 8 | 0x55) for y in linhas_no]))
confere('8b. a fatia que nao coube: perdida, sem a marca da fatia nem a de fora, e nada no anel', v('fatia_cheia'),
        [64, 1])
confere('8c. a perda de fora: marcada, passa pela fatia que coube, vai na resposta (sem a da fatia) e se limpa nela',
        v('sujo'), [32, 32, 0, 0, 0])

# 9. sem espelho
confere('9. a lanterna segura o LCD (1) e nada vai', v('lanterna'), [1, 0])
confere('9. pelo BLE o espelho so vai se pedido, e so a UART (o app so o pede pelo cabo)', v('ble')[0] == 0 and
        v('ble')[1] > 0, True)
confere('9. na sessao nada vai', v('sessao'), [0, 0])
confere('9. com o espelho desligado nada vai', v('desligado'), [0, 0])

print('\ntodos os testes passaram' if not falhas else f'\n{len(falhas)} FALHARAM: ' + ', '.join(falhas))
sys.exit(1 if falhas else 0)
