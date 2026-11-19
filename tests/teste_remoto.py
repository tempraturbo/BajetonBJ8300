"""Confere o controle remoto (firmware/src/remoto.c, v156 a v162) no PC: compila o C em 32 bits com o zig (pip install
ziglang), com a RAM e o chip de dados em vetores e o firmware imitado (tests/teste_remoto.h), roda os cenarios do
tests/teste_remoto.c e confere:
  1. o quadro do PC achado em qualquer posicao do buffer, o mais novo valendo, a soma, a tecla fora da tabela, o
     PROGRAMBT11U seguindo para o conferidor de fabrica, e o CONTA so com o carimbo;
  2. a tecla do remoto so com nenhuma fisica apertada, e a resposta (9 bytes) uma vez por quadro, com a soma;
  3. o vigia: a tecla e o PTT seguros ate 15 carimbos de 64 ms, soltos no 16o, com o retrato pendente ou nao;
  4. o PTT pela porta falsa, com o PA12 em 0; com o remoto pelo cabo, o fio do PTT (o RX da serial) nao vale (v157);
  5. o retrato da tela (v162), decodificado aqui do formato do alto do remoto.c, contra o firmware/src/retrato.h: so
     se pedido, pendente na resposta e mandado pelo remoto_retrato uma vez; o cabecalho, os valores, a flash (o canal
     da terceira, o 0 com ela em 0xFFFF, e a memoria do radio nas bandas), cada faixa da RAM e as somas; os pedacos
     de ate 20 bytes;
  6. os bits de quem recebe (a letra e do app desde a v159);
  7. a lanterna segurando o LCD.
Uso: python -I tests/teste_remoto.py
"""
import os
import re
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


# o retrato.h: as constantes e as faixas
H = open(os.path.join(RAIZ, 'firmware', 'src', 'retrato.h'), encoding='utf-8').read()
K = {m.group(1): int(m.group(2)) for m in re.finditer(r'#define RETRATO_(\w+)\s+(\d+)u', H)}
K['BASE'] = int(re.search(r'#define RETRATO_BASE\s+0x([0-9A-F]+)u', H).group(1), 16)
FAIXAS = [(K['BASE'] + (int(a, 16) | int(b, 16) << 8), int(n))
          for a, b, n in re.findall(r'\{0x([0-9A-F]{2}), 0x([0-9A-F]{2}),\s*(\d+)\}', H)]
TAMANHO = K['VALORES'] + K['FLASH'] + K['RAM']


def ram(a):
    """O padrao do teste_remoto.c na RAM."""
    return (a * 7 + (a >> 8)) & 255


def chip(a):
    return (a * 13 + (a >> 9)) & 255


def nome_radio(banda):
    """O endereco do nome da memoria do radio na flash (o R_NOME do telas.c)."""
    return 0xC110 if banda == 1 else 0xC010 if banda == 0 or banda > 4 else 0xC210


def retrato(fluxo):
    """O retrato depois da resposta: (cabecalho, valores, flash, RAM, somas certas) ou None."""
    if len(fluxo) != 9 + 5 + TAMANHO + 2 or fluxo[9:11] != b'~S':
        return None
    r = fluxo[9:]
    s1 = s2 = 0
    for x in r[:-2]:
        s1 = (s1 + x) & 255
        s2 = (s2 + s1) & 255
    v = r[5:5 + K['VALORES']]
    f = r[5 + K['VALORES']:5 + K['VALORES'] + K['FLASH']]
    m = r[5 + K['VALORES'] + K['FLASH']:-2]
    return (r[2], r[3] | r[4] << 8), v, f, m, (s1, s2) == (r[-2], r[-1])


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

# 1. o quadro (REM: bits 4:0 a tecla + 1, 5 o PTT, 6 o retrato, 7 a resposta pendente)
confere('1. o quadro mais novo, no meio do lixo, sem o conferidor', v('quadro_novo'), [0, 0, 18 | 32 | 128])
confere('1. o PROGRAMBT11U segue para o conferidor (1 abre a sessao)', v('programbt'), [1, 1, 0])
confere('1. a soma errada nao vale', v('soma_errada'), [7, 1, 0])
confere('1. a tecla 18 (fora da tabela) e nenhuma', v('tecla_18'), [128])
confere('1. com 6 bytes nao ha quadro', v('seis_bytes'), [7, 1, 0])
confere('1. o CONTA com o carimbo (o retrato pendente e o ESCURO velho saem; o ACORDADO, v164, fica)', v('carimbo'),
        [777 & 31 | 128])

# 2. a tecla e a resposta
confere('2. a tecla do remoto com nenhuma apertada; uma resposta de 9 bytes', v('tecla_remota'), [9, 1, 9])
resp = v('resposta')
confere('2. a resposta: ~ r, o REM sem o bit 7, tx, rx, sinal, lado, tensao', resp[:8], [126, 114, 10, 0, 4, 13, 0, 167])
confere('2. a soma da resposta', resp[8], sum(resp[:8]) & 255)
confere('2. a tecla fisica vale mais, e a resposta nao se repete', v('tecla_fisica'), [2, 1])

# 3. o vigia
for k in (0, 1):
    com = ' (com o retrato pendente)' if k else ''
    confere(f'3. aos 15 carimbos a tecla e o PTT seguem seguros{com}', v(f'vigia_15_{k}'), [4, 0])
    confere(f'3. aos 16 (1 s) soltam, e o REM zera{com}', v(f'vigia_16_{k}'), [255, 0x1000, 0])

# 4. o PTT
confere('4. o PTT do remoto: porta falsa com o PA12 em 0', v('ptt'), [0, 0])
confere('4. sem ele, a porta de fabrica', v('ptt_solto'), [0x1000, 1])
confere('4. v157: sem remoto, o fio apertado e o PTT', v('fio_sem_remoto'), [0])
confere('4. v157: com o remoto pelo cabo (reservados em 0xFF), o fio apertado nao vale', v('fio_cabo'), [0, 128, 0x1000])
confere('4. v157: pelo BLE, o fio do radio vale', v('fio_ble'), [0])
confere('4. v157: o PTT do quadro aperta pelo cabo', v('fio_cabo_ptt'), [0])
confere('4. v166: o inicio da TX desliga o BLE sem ligacao', v('ble_sem_ligacao'), [1])
confere('4. v166: com a ligacao aberta, nao', v('ble_com_ligacao'), [1])

# 5. o retrato
confere('5. antes da resposta nada sai, e o remoto_retrato devolve a tecla', v('antes_da_resposta'), [77, 0])
confere('5. a resposta sai so (9 B), e o retrato fica pendente; o REM leva o pedido', v('pendente'), [9, 32, 64])
confere('5. o retrato sai: a tecla de volta, a marca limpa, pedacos de ate 20 B, nada fora do chip',
        v('retrato_sai'), [5, 0, 20, 0])
fluxo = bytes.fromhex(linhas['fluxo'][0])
confere('5. a resposta vem antes, inteira', (fluxo[:2], len(fluxo) > 9 and sum(fluxo[:8]) & 255 == fluxo[8]),
        (b'~r', True))
rt = retrato(fluxo)
confere('5. o retrato: ~ S, do tamanho do retrato.h, e as somas de Fletcher conferem', rt is not None and rt[4], True)
if rt:
    confere('5. o cabecalho: a versao e o tamanho do retrato.h', rt[0], (K['VERSAO'], TAMANHO))
    esperado = (0x80000103).to_bytes(4, 'little') + (0x0102002A).to_bytes(4, 'little') + \
        (0x08A1B2C3).to_bytes(4, 'little') + (0x00070203).to_bytes(4, 'little') +         (0x01E2AA74).to_bytes(4, 'little') + bytes([1, 1, 3, 4, 1, 6])
    confere('5. os valores, na ordem do RETRATO_VALORES', rt[1], esperado)
    c = 42 * 32
    confere('5. a flash: o comeco e o nome do canal 42 (o da terceira, mesmo em VFO)', rt[2][:16],
            bytes(chip(c + i) for i in range(4)) + bytes(chip(c + 20 + i) for i in range(12)))
    confere('5. a flash: a memoria do radio (a banda e a memoria do padrao)', rt[2][16:],
            bytes(chip(nome_radio(ram(0x12A1B)) + ram(0x14CB4) * 16 + i) for i in range(14)))
    # o CONTA e o REM vao como estavam no envio (estado do remoto, nao o padrao da RAM)
    vivo = dict(zip((0x14BED, 0x14BEE), v('estado_no_envio')))
    esperado = b''.join(bytes(vivo.get(a + i, ram(a + i)) for i in range(n)) for a, n in FAIXAS)
    confere(f'5. a RAM: as {len(FAIXAS)} faixas do retrato.h, na ordem', rt[3], esperado)
confere('5. so uma vez: nem a pendencia de novo, nem a resposta seguinte sem quadro novo', v('uma_vez'), [1])
confere('5. a frequencia da terceira pedida com o terceira() inteiro', v('freq_pedida'), [1])
for k, banda in enumerate((0, 1, 2, 3, 4, 5, 255)):
    rt = retrato(bytes.fromhex(linhas[f'banda{banda}'][0]))
    c = 0 if k == 0 else 42 * 32
    obtido = rt and (rt[4], rt[2][16:], rt[2][:4])
    confere(f'5. a banda {banda}, a memoria {k * 3 + 1}: o nome em 0x{nome_radio(banda):X} + 16 x memoria'
            + (', e a terceira em 0xFFFF lida do 0' if k == 0 else ''), obtido,
            (True, bytes(chip(nome_radio(banda) + (k * 3 + 1) * 16 + i) for i in range(14)),
             bytes(chip(c + i) for i in range(4))))
confere('5. sem o pedido, so a resposta', v('sem_pedido'), [9])
confere('5. o quadro novo sem pedido, antes da volta ao laco, tira o retrato pendente', v('substituido'), [9, 0])

# 6. quem recebe, cru (v159; a letra e do app, tempralink/Remoto.pas): bits 1:0 a vista, 2 a evidencia no B, 3 a
# dupla escuta parada no outro, 4 o rodizio com a terceira no lugar
esperado = []
for vis, b, dw, cl in ((0, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 1, 1, 0), (2, 0, 1, 0), (2, 0, 1, 1), (3, 1, 0, 0),
                       (3, 0, 1, 0), (3, 0, 1, 1), (1, 0, 0, 0)):
    esperado.append(vis | b << 2 | dw << 3 | cl << 4)
confere('6. os bits de quem recebe: a vista, a evidencia, a dupla escuta no outro e o rodizio', v('quem'), esperado)

# 7. a lanterna
confere('7. a lanterna segura o LCD (1); sem ela, a de fabrica segue (0)', v('lanterna'), [1, 0])

# 8. a tela apagada (v164)
confere('8. o bit 4 do quadro liga o ESCURO (64), com o carimbo', v('escuro'), [64, 2000 & 31])
confere('8. sem tecla do radio, o ESCURO fica e nada acorda', v('escuro_remota'), [255, 64])
confere('8. a tecla do radio vale e acorda a tela (ACORDADO, 128)', v('escuro_fisica'), [6, 192])
confere('8. o quadro seguinte guarda o ACORDADO', v('escuro_guarda'), [192])
confere('8. v165: a resposta diz que a tecla do radio acendeu a tela apagada (o bit 7 do rem)', v('escuro_avisa'), [128])
confere('8. o PC acende: o ESCURO sai, e o ACORDADO fica so neste quadro', v('escuro_desliga'), [128])
confere('8. sem o ESCURO a resposta nao fala do ACORDADO', v('escuro_desliga_avisa'), [0])
confere('8. no quadro seguinte sem o escuro, o ACORDADO sai', v('escuro_segue_aceso'), [0])
confere('8. v165: o pedido novo apaga na hora, sem o ACORDADO de uma tecla de antes', v('escuro_de_novo'), [64])
confere('8. e a resposta nao diz que acordou', v('escuro_de_novo_avisa'), [0])
confere('8. aos 15 carimbos sem quadro o ESCURO segue', v('escuro_15'), [64])
confere('8. aos 16 (1 s) o vigia o apaga com o REM: a luz volta', v('escuro_16'), [0, 0])
confere('8. e aos 33, com o bit 4 do tempo de volta a 0, ele nao volta', v('escuro_33'), [0])

print('\ntodos os testes passaram' if not falhas else f'\n{len(falhas)} FALHARAM: ' + ', '.join(falhas))
sys.exit(1 if falhas else 0)
