"""Confere o desenho da tela do app (firmware/previa/tela_pc.c, v162) contra a previa: cada cena da previa vira o
retrato que o firmware mandaria (o formato no alto do firmware/src/remoto.c, as faixas do retrato.h; o remoto.c ja e
conferido contra o mesmo formato no tests/teste_remoto.py), e o desenho do app, a partir so do retrato, tem de dar a
mesma imagem, pixel por pixel. Confere tambem que so as faixas do retrato bastam (a RAM fora delas zerada no app), e
os erros do retrato (tamanho, versao e somas).

As cenas que a previa desenha por um caminho que o retrato nao leva (o que o firmware passa por parametro: a busca do
radio, a partida dele, o espectro com as colunas, o APRS e o Morse chegando da flash, as caixas por cima de outra tela,
a tensao da partida) ficam em FORA, com o porque; uma delas que passe a sair igual tambem e avisada.

Com --amostras <pasta>, grava la o retrato e a tela de AMOSTRAS (os testes do Tempra Link: tempralink/testes).
Precisa do zig (pip install ziglang), como a previa. Uso: python -I tests/teste_tela_pc.py [--amostras <pasta>]
"""
import ctypes
import os
import re
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PREVIA = os.path.join(RAIZ, 'firmware', 'previa')
SRC = os.path.join(RAIZ, 'firmware', 'src')
sys.path.insert(0, PREVIA)
import tela_pc  # noqa: E402

# cena: o porque de o retrato nao a dar (o desenho dela vem de parametro, nao da RAM)
FORA = {
    '23-radio-seek': 'a busca (RADIO_BUSCANDO) vem do gancho do SEEK, sem marca na RAM',
    '17-radio-iniciando': 'a espera da partida do radio (TELA_INICIO_RADIO) vem do gancho da partida',
    '26-tensao-na-partida': 'a tensao da partida vem do gancho da partida',
    '26b-tensao-com-o-0': 'o cartao da tensao com o 0 segurado vem da acao da tecla, por cima da tela',
    '30-espectro': 'as colunas do espectro vem das rotinas dele (o app mostra o quadro sem elas)',
    '31-espectro-digitando-am': 'as colunas do espectro vem das rotinas dele',
    '32-espectro-pelos-ganchos': 'as colunas do espectro vem das rotinas dele',
    '8-fonte': 'a fonte inteira, so da previa (nao e tela do radio)',
}
PREFIXOS_FORA = {
    '62': 'o APRS e o Morse: o laco deles desenha o historico, da flash, e nem deixa o retrato sair (o app avisa)',
}
# a caixa da MESSAGES por cima do historico: o app a desenha so ela (o resto preto), e se confere dela para baixo
CAIXAS = {'62d-aprs-caixa-mensagem', '62e-aprs-caixa-to', '62f-aprs-caixa-callsign', '62i-morse-caixa',
          '61-editor-mensagem-37', '61b-editor-mensagem-20'}
CAIXA_Y0 = 56                     # o TOPO_MENU do telas.c
# as amostras do --amostras (o retrato e a tela, para os testes do Tempra Link), com o byte do sinal da resposta
AMOSTRAS = {'1-a-em-evidencia-recebendo': 12, '9-menu-roger': 255}

falhas = []


def confere(nome, obtido, esperado):
    ok = obtido == esperado
    print(f'{nome}: {"ok" if ok else "FALHOU"}')
    if not ok:
        print(f'    obtido:   {obtido!r}\n    esperado: {esperado!r}')
        falhas.append(nome)


H = open(os.path.join(SRC, 'retrato.h'), encoding='utf-8').read()
K = {m.group(1): int(m.group(2)) for m in re.finditer(r'#define RETRATO_(\w+)\s+(\d+)u', H)}
K['BASE'] = int(re.search(r'#define RETRATO_BASE\s+0x([0-9A-F]+)u', H).group(1), 16)
FAIXAS = [(K['BASE'] + (int(a, 16) | int(b, 16) << 8), int(n))
          for a, b, n in re.findall(r'\{0x([0-9A-F]{2}), 0x([0-9A-F]{2}),\s*(\d+)\}', H)]
TAMANHO = K['VALORES'] + K['FLASH'] + K['RAM']
# o menu: a previa o tem em MENU_BASE (telas.c), e o radio onde o menu_tabela caiu na compilacao (build/simbolos.txt)
MENU_PREVIA = 0x01E30000
MENU_RADIO = next(int(p[0], 16) for p in map(str.split, open(os.path.join(RAIZ, 'firmware', 'build', 'simbolos.txt')))
                  if p[-1:] == ['menu_tabela'])


def retrato(d, ram=None, menu=MENU_RADIO):
    """O retrato da cena d como o remoto_retrato o monta no radio: o ponteiro do menu (b[0x14BF4]) dentro do
    menu_tabela de la, e o endereco dele nos valores (menu)."""
    grandes = struct.unpack_from('<4I', d, 0)
    ram = bytearray(d[34:34 + 0x20000] if ram is None else ram)
    chip = d[34 + 0x20000:34 + 0x30000]
    t = struct.unpack_from('<I', ram, 0x14BF4)[0] - MENU_PREVIA
    if 0 <= t < 96 * 28:                      # o menu_tabela[96 * 28] do tela.c
        struct.pack_into('<I', ram, 0x14BF4, MENU_RADIO + t)
    c = grandes[1] & 0xFFFF
    c = 0 if c == 0xFFFF else c * 32
    b = ram[0x12A1B]
    radio = (0xC110 if b == 1 else 0xC010 if b == 0 or b > 4 else 0xC210) + ram[0x14CB4] * 16
    corpo = (b'~S' + bytes([K['VERSAO'], TAMANHO & 255, TAMANHO >> 8]) + struct.pack('<5I', *grandes, menu)
             + d[16:22] + chip[c:c + 4] + chip[c + 20:c + 32] + chip[radio:radio + 14]
             + b''.join(ram[a:a + n] for a, n in FAIXAS))
    s1 = s2 = 0
    for x in corpo:
        s1 = (s1 + x) & 255
        s2 = (s2 + s1) & 255
    return corpo + bytes([s1, s2])


def le(caminho):
    return open(caminho, encoding='utf-8', errors='surrogateescape').read()


def grava(caminho, texto):
    open(caminho, 'w', encoding='utf-8', errors='surrogateescape', newline='\n').write(texto)


def troca(s, a, b, nome):
    if s.count(a) != 1:
        sys.exit(f'{nome} mudou ({a[:50]!r}): ajuste o teste_tela_pc.py')
    return s.replace(a, b)


with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as pasta:
    dll = tela_pc.compila({'dll64'}, pasta)[0]
    # a previa, com o medidor de cada cena anotado e cada cena despejada (o que o retrato leria e a imagem)
    t = troca(le(os.path.join(SRC, 'tela.c')),
              'void tela_b_medidor(unsigned estado, unsigned nivel, unsigned lado)\n{\n',
              'void previa_medidor_visto(unsigned, unsigned, unsigned);\n'
              'void tela_b_medidor(unsigned estado, unsigned nivel, unsigned lado)\n{\n'
              '    previa_medidor_visto(estado, nivel, lado);\n', 'o tela.c')
    grava(os.path.join(pasta, 'tela.c'), t)
    grava(os.path.join(pasta, 'telas.c'), le(os.path.join(SRC, 'telas.c')))
    grava(os.path.join(pasta, 'maquina.c'), troca(le(os.path.join(PREVIA, 'maquina.c')), '#include "../src/tela.c"',
                                                  '#include "tela.c"', 'o maquina.c'))
    p = troca(le(os.path.join(PREVIA, 'previa.c')), 'static void grava(const char *nome)\n{\n', r'''static int med_estado = -1, med_nivel, med_lado;
void previa_medidor_visto(unsigned e, unsigned n, unsigned l)
{
    med_estado = (int)e;
    med_nivel = n < 10 ? (int)n * 2 : 24;
    med_lado = (int)l;
}

static void despeja(const char *nome)
{
    char c[600];
    FILE *f;
    u32 v[4];
    u8 q[6];
    int m[3];

    snprintf(c, sizeof c, "%s/%s.cena", pasta, nome);
    f = fopen(c, "wb");
    v[0] = watch_tela();
    v[1] = terceira();
    v[2] = freq_terceira(v[1]);
    v[3] = watch_le_tudo();
    q[0] = c_no_lugar();
    q[1] = outro_recebendo();
    q[2] = nr_le();
    q[3] = porta_le();
    q[4] = em_canal();
    q[5] = lista_item();
    m[0] = med_estado;
    m[1] = med_nivel;
    m[2] = med_lado;
    fwrite(v, 4, 4, f);
    fwrite(q, 1, 6, f);
    fwrite(m, 4, 3, f);
    fwrite(memoria, 1, sizeof memoria, f);
    fwrite(chip, 1, sizeof chip, f);
    fwrite(tela, 2, 160 * 128, f);
    fclose(f);
    med_estado = -1;
}

static void grava(const char *nome)
{
    despeja(nome);
''', 'o previa.c')
    grava(os.path.join(pasta, 'previa.c'), p)
    exe = os.path.join(pasta, 'previa_cenas.exe')
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O1', '-w', '-DPREVIA', '-I',
                    os.path.join(RAIZ, 'firmware', 'build'), '-I', PREVIA, '-o', exe,
                    os.path.join(pasta, 'previa.c')], check=True)
    cenas = os.path.join(pasta, 'cenas')
    os.makedirs(cenas)
    subprocess.run([exe, cenas], check=True, stdout=subprocess.DEVNULL)

    lib = ctypes.CDLL(dll)
    lib.tela_bj_desenha_com.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                        ctypes.c_void_p]
    lib.tela_bj_desenha.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
    confere('o tamanho e a versao do retrato.h', (lib.tela_bj_tamanho(), lib.tela_bj_versao()),
            (5 + TAMANHO + 2, K['VERSAO']))
    saida = (ctypes.c_uint16 * (160 * 128))()
    iguais, diferentes, fora_iguais, codigos, caixas, caixas_ruins = [], [], [], {}, {}, []
    nomes = sorted(n[:-5] for n in os.listdir(cenas) if n.endswith('.cena'))
    primeiro = None
    for nome in nomes:
        d = open(os.path.join(cenas, nome + '.cena'), 'rb').read()
        med = struct.unpack_from('<3i', d, 22)
        img = d[34 + 0x30000:]
        r = retrato(d)
        primeiro = primeiro or r
        k = lib.tela_bj_desenha_com(r, len(r), med[0], med[1] if med[0] >= 0 else -1, med[2], saida)
        codigos[nome] = k
        igual = bytes(saida) == img
        if nome in CAIXAS:
            igual = bytes(saida)[CAIXA_Y0 * 320:] == img[CAIXA_Y0 * 320:]
            caixas[nome] = k
            if not igual:
                caixas_ruins.append(nome)
            continue
        fora = FORA.get(nome) or PREFIXOS_FORA.get(nome.split('-')[0][:2])
        if igual:
            (fora_iguais if fora else iguais).append(nome)
        elif not fora:
            dif = sum(1 for i in range(0, len(img), 2) if img[i:i + 2] != bytes(saida)[i:i + 2])
            diferentes.append(f'{nome} ({dif} pixels, codigo {k})')
    # sem o endereco do menu_tabela do radio (como ate a v162), o menu sai pelo desenho de fabrica, sem as pastas
    no_menu = [n for n in iguais if 0 <= struct.unpack_from('<I', open(os.path.join(cenas, n + '.cena'), 'rb').read(),
                                                             34 + 0x14BF4)[0] - MENU_PREVIA < 96 * 28]
    sem_endereco = []
    for nome in no_menu:
        d = open(os.path.join(cenas, nome + '.cena'), 'rb').read()
        r = retrato(d, menu=MENU_PREVIA)
        lib.tela_bj_desenha_com(r, len(r), -1, -1, 0, saida)
        if bytes(saida) != d[34 + 0x30000:]:
            sem_endereco.append(nome)
    confere(f'v163: as {len(no_menu)} cenas no menu principal dependem do endereco do menu_tabela do radio '
            f'(o radio em 0x{MENU_RADIO:08X})', (len(no_menu) > 10, sem_endereco == no_menu), (True, True))
    print(f'{len(nomes)} cenas: {len(iguais)} iguais, {len(caixas)} caixas da MESSAGES, '
          f'{len(nomes) - len(iguais) - len(caixas) - len(diferentes) - len(fora_iguais)} fora do retrato')
    confere('as cenas que o retrato leva saem iguais a previa, pixel por pixel', diferentes, [])
    confere('nenhuma cena de FORA sai igual (se sair, ela entra)', fora_iguais, [])
    confere('as iguais voltam como inteiras (0)', sorted({codigos[n] for n in iguais}), [0])
    confere(f'as {len(CAIXAS)} caixas da MESSAGES: iguais da caixa para baixo, e parciais (1)',
            (caixas_ruins, sorted(set(caixas.values())), len(caixas)), ([], [1], len(CAIXAS)))
    confere('o espectro volta como parcial (1)', sorted({codigos[n] for n in nomes if n.startswith(('30', '31', '32'))}),
            [1])
    # o desenho como o app o chama (o tela_bj_desenha): o medidor pelo byte do sinal da resposta, e o lado dele pela
    # RAM do retrato (o em evidencia, ou o outro com a dupla escuta parada nele), nas cenas iguais com o medidor
    pelo_sinal, lado_outro, sem_sinal = [], [], []
    for nome in iguais:
        d = open(os.path.join(cenas, nome + '.cena'), 'rb').read()
        med = struct.unpack_from('<3i', d, 22)
        ram = d[34:34 + 0x20000]
        if med[0] < 0:
            # sem medidor: o 255 (nenhum sinal), que o radio so manda fora da TX e sem receber (o medidor_s do bancos.c)
            if ram[0x14685] == 2 or ram[0x15367] in (4, 5):
                continue
            r = retrato(d)
            lib.tela_bj_desenha(r, len(r), 255, saida)
            if bytes(saida) != d[34 + 0x30000:]:
                sem_sinal.append(nome)
            continue
        if ((ram[0x15E04] != 0) ^ (ram[0x15378] == 1)) != med[2]:
            lado_outro.append(nome)      # a previa pediu o medidor de um lado que a RAM dela nao diz
            continue
        r = retrato(d)
        lib.tela_bj_desenha(r, len(r), med[1] | (0x80 if med[0] == 1 else 0), saida)
        if bytes(saida) != d[34 + 0x30000:]:
            pelo_sinal.append(nome)
    print(f'  {len(lado_outro)} cenas com o medidor num lado que a RAM delas nao diz: {", ".join(lado_outro)}')
    confere('pelo byte do sinal, as cenas com o medidor saem iguais', pelo_sinal, [])
    confere('e as sem medidor, com o 255 (nenhum sinal), tambem', sem_sinal, [])
    # as amostras para os testes do Tempra Link (tempralink/testes): o retrato e a tela da previa, com o sinal
    if '--amostras' in sys.argv:
        destino = sys.argv[sys.argv.index('--amostras') + 1]
        for nome, sinal in AMOSTRAS.items():
            d = open(os.path.join(cenas, nome + '.cena'), 'rb').read()
            r = retrato(d)
            k = lib.tela_bj_desenha(r, len(r), sinal, saida)
            confere(f'a amostra {nome} (sinal {sinal}) sai igual a previa', (k, bytes(saida) == d[34 + 0x30000:]),
                    (0, True))
            open(os.path.join(destino, f'retrato-{nome}.bin'), 'wb').write(r)
            open(os.path.join(destino, f'tela-{nome}.bin'), 'wb').write(d[34 + 0x30000:])
    # o menu calado sem a caixa (a tela do APRS ou do Morse ficou, e a volta a pede de novo): fora do retrato (2)
    d = open(os.path.join(cenas, '9-menu-roger.cena'), 'rb').read()
    ram = bytearray(d[34:34 + 0x20000])
    ram[0x14BF2] = 8                  # a VOLTA (mensagem.c)
    r = retrato(d, ram)
    confere('o menu calado sem a caixa: fora do retrato (2)', lib.tela_bj_desenha(r, len(r), 255, saida), 2)
    # os erros: a saida fica como estava
    lib.tela_bj_desenha(primeiro, len(primeiro), 255, saida)
    antes = bytes(saida)
    curto = lib.tela_bj_desenha(primeiro[:-1], len(primeiro) - 1, 255, saida)
    outra = bytearray(primeiro)
    outra[2] ^= 1
    versao = lib.tela_bj_desenha(bytes(outra), len(outra), 255, saida)
    estragado = bytearray(primeiro)
    estragado[100] ^= 0x10
    soma = lib.tela_bj_desenha(bytes(estragado), len(estragado), 255, saida)
    confere('os erros: tamanho -1, versao -2, somas -3, e a imagem fica', (curto, versao, soma, bytes(saida) == antes),
            (-1, -2, -3, True))

print('\ntodos os testes passaram' if not falhas else f'\n{len(falhas)} FALHARAM: ' + ', '.join(falhas))
sys.exit(1 if falhas else 0)
