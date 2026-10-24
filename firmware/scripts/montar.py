"""Monta o firmware do BJ-8300 com os bancos de canais.

Uso: python -I montar.py <original.anc> <bancos.bin> <saida.anc> [simbolos.txt]

Os enderecos das funcoes novas sao lidos do arquivo de simbolos gerado pelo llvm-nm (por padrao
build/simbolos.txt, que o build.sh escreve). Antes eles ficavam escritos aqui, e bastava recompilar
para o menu passar a apontar para o lugar errado sem ninguem perceber.

O que faz:
  1. grava o codigo novo no espaco livre do fim de cada imagem de flash;
  2. grava ali tambem uma COPIA da tabela do menu principal com dois itens novos no fim, e troca
     o ponteiro da tabela no laco da interface para essa copia - assim **nenhum item de menu e
     sacrificado**: so o ZONE da lugar ao BANK, porque o banco faz o papel dele;
  3. enganxa o filtro dos bancos no fim do carregamento de configuracao;
  4. recalcula todos os CRCs e escreve o .anc.

Dentro do app.bin mudam **10 bytes**: a chamada do gancho (4) e as duas instrucoes que montavam o
endereco da tabela (6). Todo o resto do firmware fica byte a byte igual ao de fabrica.

TRAVA DE SEGURANCA: recusa qualquer escrita que caia nas funcoes que o radio executa antes de
checar o combo PTT2+atalho, ou que fazem o modo UPDATE funcionar. Elas sao o caminho de
recuperacao pelo cabo: se um firmware ruim quebrasse esse caminho, o radio so voltaria abrindo o
aparelho. Os limites foram lidos no Ghidra e estao em PROTEGIDAS.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jlfw

BASE_APP = 0x01E00100

PROTEGIDAS = [
    (0x01E57472, 0x01E57A40, 'main do app: inicializa o hardware e escolhe o modo'),
    (0x01E56BB6, 0x01E56CF7, 'inicializacao do app: le as teclas do combo de recuperacao'),
    (0x01E432FC, 0x01E43355, 'desenha a tela UPDATE'),
    (0x01E4DEA8, 0x01E4DF60, 'tick periodico que decide se a interface roda'),
    (0x01E55AF4, 0x01E55C99, 'parte do modo UPDATE'),
    (0x01E3CFD2, 0x01E3CFD5, 'parte do modo UPDATE'),
]

# Onde o codigo novo roda (igual nas 6 imagens; ver bancos.ld).
ENDERECO_CODIGO = 0x01E96122

PRECISA = ('gancho_cabo', 'gancho_scan', 'gancho_barra', 'bank_exibe', 'bank_le', 'bank_grava', 'nome_exibe', 'nome_le', 'nome_grava',
           'gancho_partida', 'canal_exibe', 'canal_le', 'canal_grava',
           'titulo_bank', 'titulo_nome', 'titulo_canal')


def le_simbolos(caminho):
    """Le a saida do llvm-nm ("endereco T nome") e confere que nao falta nenhum simbolo."""
    tabela = {}
    with open(caminho) as f:
        for linha in f:
            partes = linha.split()
            if len(partes) == 3 and partes[1] in 'Tt':
                tabela[partes[2]] = int(partes[0], 16)
    faltando = [n for n in PRECISA if n not in tabela]
    if faltando:
        raise SystemExit(f'faltam simbolos em {caminho}: {", ".join(faltando)}')
    return tabela


S = {}   # preenchido em main() a partir do arquivo de simbolos

# Campos de um item de menu (28 bytes, compactado, confirmado no motor 0x01E489E0):
#   +0 flags | +1 valor maximo (u16) | +3 get | +7 set | +0xb nome | +0xf nome_cn
#   +0x13 exibe (ou lista de opcoes em ingles) | +0x17 opcoes em chines | +0x1b id do item
# Os dois ultimos campos nao sao tocados: ficam como estavam no item aproveitado. O motor le o
# maximo como u16 (h[0x14C0E]) e aceita 960 - e o que ele mesmo faz com o MEMCH em Full Channel.
CAMPO = {'get': 3, 'set': 7, 'nome': 0xB, 'nome_cn': 0xF, 'exibe': 0x13}
FLAGS_NUMERO = 0x80   # +0x13 e funcao de exibicao, chamada por linha desenhada (como era a zona)
FLAGS_EDITOR = 0x07   # abre o editor de texto (como o CH-NAME)

# A tabela viva do menu principal, com 53 itens de 28 bytes (o item 0 e o SQL). Quem a carrega e o
# laco da interface, em PONTEIRO_EM, com "r0 = 13368; r5 = r6 + r0" (r6 = 0x01E5FB60) - por ser
# soma de registradores, procurar o endereco pronto no desmontado nao acha nada. A quantidade de
# itens o firmware conta sozinho (FUN_01E4A81A): a tabela acaba no primeiro item com os 28 bytes
# zerados, e por isso a copia leva um terminador no fim.
MENU_NOVO = True          # False = nao realoca a tabela (sem os itens BANK): corte de diagnostico
TABELA = 0x01E62F98
ITENS_ORIGINAIS = 53
TAM_ITEM = 0x1C
PONTEIRO_EM = 0x01E4CBB2
PONTEIRO_ANTES = bytes.fromhex('40e03834651c')     # r0 = 13368 ; r5 = r6 + r0

# Como o firmware conta os itens varrendo a tabela, e a copia tem mais itens que a original, a
# contagem e trocada por uma constante na propria chamada que a pedia (4 bytes, no lugar exato) -
# assim a tabela nao precisa de terminador e a varredura nunca sai para a area apagada.
CONTAGEM_EM = 0x01E4CBE8
CONTAGEM_ANTES = bytes.fromhex('bfea17ee')         # call FUN_01E4A81A (conta itens da tabela)

# Itens na COPIA da tabela. Eles entram JUNTOS, logo depois do ZONE - que continua existindo, com
# todos os outros itens de fabrica. Cada um copia o molde de um item do mesmo tipo que funciona (o
# ZONE para numero com exibicao propria, o CH-NAME para o editor de texto), o que traz de graca os
# campos que nao mexemos: opcoes em chines (+0x17) e numero do aviso falado (+0x1B, 0 nos dois
# moldes = nao fala nada).
ZONE, ZONA_OU_CANAL, CH_NAME = 11, 10, 19
# Itens de fabrica que NAO entram na copia, a pedido do usuario: o ZONE e o "Zone Or Channel"
# (escolher entre navegar por zona ou por canal cheio), que sem o ZONE nao tem mais serventia. Os
# 56 bytes de tabela que eles devolvem sao o que faz o realce do VFO caber no espaco livre.
ITENS_FORA = (ZONA_OU_CANAL, ZONE)
DEPOIS_DE = ZONE
ITENS = [
    dict(nome='BANK', molde=ZONE, flags=FLAGS_NUMERO, maximo=16,
         get='bank_le', set='bank_grava', titulo='titulo_bank', exibe='bank_exibe'),
    dict(nome='BANK NAME', molde=CH_NAME, flags=FLAGS_EDITOR, maximo=0,
         get='nome_le', set='nome_grava', titulo='titulo_nome', exibe='nome_exibe'),
    dict(nome='BANK CH', molde=ZONE, flags=FLAGS_NUMERO, maximo=960,
         get='canal_le', set='canal_grava', titulo='titulo_canal', exibe='canal_exibe'),
]

# ---- revisao dos textos em ingles do menu (ver notas/textos-revisao.md) ----
# As cadeias vem coladas na flash, sem folga: trocar no lugar so pode ter tamanho igual ou menor
# (o resto vai a zero, e o texto termina no zero). O padrao e CAIXA ALTA, que e o que 90% dos
# textos ja usa, com a unidade fisica mantendo a grafia certa (Hz). Duas cadeias do STEP sao
# sufixo de outras - "2.5 K" dentro de "12.5 K" e "5.0 K" dentro de "25.0 K" -, entao so a maior
# entra aqui e a menor sai corrigida junto.
TEXTOS_REVISADOS = True
TEXTOS = [
    (0x01E5A560, 'Middle', 'MIDDLE'),
    (0x01E5A4A0, 'Normal Mode', 'NORMAL MODE'),
    (0x01E5A4AC, 'Super  Mode', 'SUPER MODE'),
    (0x01E5A4B8, 'Deep   Mode', 'DEEP MODE'),
    (0x01E5A581, '6.25 K', '6.25K'),
    (0x01E5A588, '8.33 K', '8.33K'),
    (0x01E5A58F, '10.0 K', '10.0K'),
    (0x01E5A596, '12.5 K', '12.5K'),
    (0x01E5A59D, '20.0 K', '20.0K'),
    (0x01E5A5A4, '25.0 K', '25.0K'),
    (0x01E5A5AB, '50.0 K', '50.0K'),
    (0x01E5A7B3, 'Zone Or Channel', 'ZONE OR CHANNEL'),
    (0x01E5A71A, 'Zone Mode', 'ZONE MODE'),
    (0x01E5A724, 'Full Channel', 'FULL CHANNEL'),
    (0x01E5A8F4, 'POWER ON MSG', 'POWER-ON MSG'),
    (0x01E5A6A1, 'Delet', 'DEL'),
    (0x01E5A6A7, 'Add', 'ADD'),
    (0x01E5A613, '1000hz', '1000Hz'),
    (0x01E5A61A, '1450hz', '1450Hz'),
    (0x01E5A621, '1750hz', '1750Hz'),
    (0x01E5A628, '2100hz', '2100Hz'),
    (0x01E5A60F, 'All', 'ALL'),
    (0x01E5AA81, 'RX Modulation', 'RX MODULATION'),
    # versao do firmware, que o item VERSION mostra (o hardware V01T32 fica como esta)
    (0x01E5FD10, 'V0.12', 'V1.0T'),
    # o nome chines do item e 语音加密 (criptografia de voz): VOICEPRI engana
    (0x01E5A82F, 'VOICEPRI', 'ENCRYPT'),
]

# Chamada no fim do carregamento de configuracao, trocada pelo gancho que aplica o filtro.
GANCHO_EM = 0x01E56AE8
GANCHO_ERA = 0x01E3951C

# Gancho do cabo: as DUAS chamadas que o tratador faz para a rotina de dados 0x01E43970 - a do 'R'
# (le, argumento 0) e a do 'W' (grava, argumento 1). O gancho repassa o argumento, entao um unico
# gancho atende os dois. Com o tamanho de sempre (bit 7 em zero) nada muda e o CHIRP continua
# funcionando; com o bit 7 ligado, os outros 7 bits do tamanho sao o terceiro byte do endereco e o
# bloco e de 64 B - a leitura alcanca o chip inteiro e a gravacao so a area dos bancos.
CABO_ESTENDIDO = True
CABO_SITIOS = ((0x01E4BB66, 'R, leitura'), (0x01E4C0FA, 'W, gravacao'))
CABO_ERA = 0x01E43970

# Gancho do scan: a chamada que avanca o canal durante a varredura passa a pedir o redesenho da
# tela, senao a barra de sinal fica no nivel do ultimo canal achado e o numero descasa do nome.
SCAN_REDESENHA = True
SCAN_EM = 0x01E46EB6
SCAN_ERA = 0x01E458E4

# Realce do VFO selecionado na tela principal. A tela pinta cada faixa chamando
# janela(x0, x1, y0, y1, limpar, cor) em 0x01E3AD34 - sempre com cor 0 (preto). A cor chega num
# registrador carregado por uma instrucao de 2 bytes "rX = imm8", entao trocar a cor e um remendo
# de 2 bytes no lugar: **nao cresce o codigo**.
# Limite da codificacao: imm8 vai de 0 a 255, o que em RGB565 cobre exatamente os verdes e azuis
# escuros (o vermelho mora nos bits altos) - que e o tom que queremos. O proprio firmware ja usa
# cores assim (0x00A0, 0x0082, 0x0065).
#
# REALCE_DIAG pinta de verde TODAS as faixas de largura total que o firmware enche de preto. Nao e
# a versao final: serve para a foto do radio dizer qual chamada pinta qual pedaco da tela, que e a
# unica forma honesta de descobrir isso sem simbolos. A lista sai de
#   python -I scripts/retangulos.py saida/app.bin cores
# Realce do VFO selecionado: a faixa da barra de sinal (160x14) e pintada por
# janela(x0,x1,y0,y1,limpar,cor) sempre com cor 0. O gancho repassa os seis argumentos e so troca a
# cor quando a faixa e a do lado selecionado - o y diz qual lado (41 ou 74 no VFO A, 114 no B,
# conferido no radio) e o VFO ativo sai de tx_ab, que o firmware espelha em 0x14B84.
# A cor fica em COR_REALCE, no bancos.c.
REALCE = True
REALCE_EM = 0x01E41468
REALCE_ERA = 0x01E3AD34
# Diagnostico (pinta faixas inteiras trocando so a cor, 2 bytes por sitio) fica desligado: serviu
# para descobrir qual chamada pinta o que. Os sitios conferidos estao no historico do git.
REALCE_DIAG = False
REALCE_SITIOS = ()

# Sentido das setas no menu. De fabrica a seta de cima AVANCA o valor/item - que esta desenhado
# mais abaixo na tela - e a de baixo retrocede: logico pelo numero, estranho pela posicao. As duas
# rotinas abaixo atendem as duas telas (lista de itens quando b[0x14BF9] != 1, lista de valores
# quando == 1) e so mexem na estrutura do menu em 0x14BF4, entao inverter nao afeta o sobe/desce
# de canal da tela inicial nem a busca. Sao quatro sitios de chamada (duas de cada), um par por
# contexto de tecla; trocando os quatro, cada contexto passa a andar para o outro lado. O editor
# de texto tem caminho proprio no tratador e nao passa por aqui.
SETAS_INVERTIDAS = True
AVANCA, RETROCEDE = 0x01E49F60, 0x01E49FF4
SETAS = ((0x01E4D094, AVANCA), (0x01E4D12A, AVANCA),
         (0x01E4D0A4, RETROCEDE), (0x01E4D0AA, RETROCEDE))


def monta_call(origem, destino):
    """Codifica a instrucao de chamada do pi32v2 (4 bytes, deslocamento de 23 bits em palavras)."""
    d = (destino - origem - 4) // 2
    if not -0x200000 <= d < 0x200000:
        raise SystemExit(f'chamada de {origem:#x} para {destino:#x} esta longe demais')
    d &= 0x3FFFFF
    return bytes([0x80 | ((d >> 16) & 0x3F), 0xEA, d & 0xFF, (d >> 8) & 0xFF])


def confere_trava(escritas):
    for endereco, tamanho, descricao in escritas:
        for ini, fim, oque in PROTEGIDAS:
            if endereco <= fim and ini <= endereco + tamanho - 1:
                raise SystemExit(f'TRAVA: "{descricao}" escreveria em {endereco:#x}..'
                                 f'{endereco + tamanho - 1:#x}, dentro de {ini:#x}..{fim:#x} ({oque}). Recusado.')
    print(f'trava: {len(escritas)} escritas conferidas contra {len(PROTEGIDAS)} areas protegidas - ok')


def monta_imm8(reg, valor):
    """Instrucao de 2 bytes "rX = imm8" (codificacao conferida em scripts/retangulos.py)."""
    if not 0 <= reg <= 7 or not 0 <= valor <= 0xFF:
        raise SystemExit(f'r{reg} = {valor} nao cabe na instrucao de 2 bytes')
    return bytes((0x40 | ((valor >> 5) & 7) << 3 | reg, 0x20 | (valor & 0x1F)))


def alinha32(endereco):
    """A tabela entra logo depois do codigo; o par e so para nao deixar endereco impar."""
    return (endereco + 1) & ~1


def ordem_da_copia():
    """Posicoes da copia: os itens de fabrica (menos o ZONE) com os dos bancos no lugar dele.

    Cada elemento e o indice do item original ou o dicionario de um item novo.

    Saem dois itens de fabrica, a pedido do usuario: o **ZONE** (indice 11) e o **Zone Or Channel**
    (indice 10), que escolhe entre navegar por zona ou por canal cheio - sem o ZONE ele nao tem mais
    serventia. O banco faz o papel dos dois, com muito mais liberdade. Sao 56 bytes de tabela
    devolvidos, que e o que faz o realce do VFO caber no espaco livre da imagem. As FUNCOES
    continuam no firmware; o que sai sao os itens de menu delas.
    """
    de_fabrica = [i for i in range(ITENS_ORIGINAIS) if i not in ITENS_FORA]
    return de_fabrica[:ZONA_OU_CANAL] + list(ITENS) + de_fabrica[ZONA_OU_CANAL:]


def monta_tabela(app):
    """Copia da tabela do menu principal, com os itens dos bancos no meio e nada perdido."""
    inicio = TABELA - BASE_APP
    original = app[inicio:inicio + ITENS_ORIGINAIS * TAM_ITEM]
    tabela = bytearray()
    for quem in ordem_da_copia():
        if isinstance(quem, int):
            tabela += original[quem * TAM_ITEM:(quem + 1) * TAM_ITEM]
            continue
        o = len(tabela)
        tabela += original[quem['molde'] * TAM_ITEM:(quem['molde'] + 1) * TAM_ITEM]
        tabela[o] = quem['flags']
        struct.pack_into('<H', tabela, o + 1, quem['maximo'])
        for campo, chave in (('get', quem['get']), ('set', quem['set']), ('nome', quem['titulo']),
                             ('nome_cn', quem['titulo']), ('exibe', quem['exibe'])):
            struct.pack_into('<I', tabela, o + CAMPO[campo], S[chave])
    return bytes(tabela)


def monta_ponteiro(endereco):
    """Instrucao de 6 bytes que poe o endereco da tabela em r5, no lugar das duas originais."""
    return b'\xc5\xff' + struct.pack('<I', endereco)


def monta_contagem(itens):
    """Instrucao de 4 bytes que poe a quantidade de itens em r0, no lugar da chamada que a contava."""
    return b'\x40\xe0' + struct.pack('<H', itens)


def main():
    origem, codigo, destino = sys.argv[1], sys.argv[2], sys.argv[3]
    simbolos = sys.argv[4] if len(sys.argv) > 4 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'simbolos.txt')
    S.update(le_simbolos(simbolos))
    print(f'simbolos lidos de {os.path.normpath(simbolos)}: {len(S)}')

    dados = jlfw.kdh_scramble(open(origem, 'rb').read())
    _, entradas = jlfw.ufw_entries(dados)
    img = next(e for e in entradas if e['type'] == 0)
    flash = dados[img['offset']:img['offset'] + img['size']]
    chave = jlfw.find_chipkey(flash)
    _, base, sub = jlfw.app_dir(flash, chave)
    ent = next(s for s in sub if s['name'] == 'app.bin')
    app = bytearray(jlfw.sfc(flash[base + ent['offset']:base + ent['offset'] + ent['size']],
                             chave, ent['offset']))

    blob = open(codigo, 'rb').read()
    tabela = monta_tabela(app)
    endereco_tabela = alinha32(ENDERECO_CODIGO + len(blob))

    # Monta a lista de escritas no app.bin e passa pela trava antes de tocar em qualquer byte.
    escritas = [(GANCHO_EM, 4, 'gancho do filtro na partida')]
    if SCAN_REDESENHA:
        escritas.append((SCAN_EM, 4, 'gancho do redesenho no scan'))
    if CABO_ESTENDIDO:
        escritas += [(s, 4, f'gancho do cabo ({p})') for s, p in CABO_SITIOS]
    if MENU_NOVO:
        escritas += [(PONTEIRO_EM, len(PONTEIRO_ANTES), 'ponteiro da tabela de menus'),
                     (CONTAGEM_EM, len(CONTAGEM_ANTES), 'quantidade de itens do menu')]
    if SETAS_INVERTIDAS:
        escritas += [(s, 4, 'sentido das setas no menu') for s, _ in SETAS]
    if REALCE_DIAG:
        escritas += [(e, 2, 'cor de faixa (diagnostico)') for e, _ in REALCE_SITIOS]
    if REALCE:
        escritas.append((REALCE_EM, 4, 'gancho do realce do VFO selecionado'))
    if TEXTOS_REVISADOS:
        escritas += [(e, len(v) + 1, f'texto "{v}"') for e, v, _ in TEXTOS]
    confere_trava(escritas)

    # 1. gancho do filtro
    o = GANCHO_EM - BASE_APP
    antes = bytes(app[o:o + 4])
    if antes != monta_call(GANCHO_EM, GANCHO_ERA):
        raise SystemExit(f'{GANCHO_EM:#x} nao e a chamada esperada para {GANCHO_ERA:#x}: {antes.hex()}')
    app[o:o + 4] = monta_call(GANCHO_EM, S['gancho_partida'])
    print(f"  gancho {GANCHO_EM:#x}: chamada para {GANCHO_ERA:#x} -> {S['gancho_partida']:#x}")

    if CABO_ESTENDIDO:
        for sitio, papel in CABO_SITIOS:
            o = sitio - BASE_APP
            antes = bytes(app[o:o + 4])
            if antes != monta_call(sitio, CABO_ERA):
                raise SystemExit(f'{sitio:#x} nao e a chamada esperada para {CABO_ERA:#x}: {antes.hex()}')
            app[o:o + 4] = monta_call(sitio, S['gancho_cabo'])
            print(f"  gancho do cabo {sitio:#x} ({papel}): chamada para {CABO_ERA:#x} -> "
                  f"{S['gancho_cabo']:#x}")

    if SCAN_REDESENHA:
        o = SCAN_EM - BASE_APP
        antes = bytes(app[o:o + 4])
        if antes != monta_call(SCAN_EM, SCAN_ERA):
            raise SystemExit(f'{SCAN_EM:#x} nao e a chamada esperada para {SCAN_ERA:#x}: {antes.hex()}')
        app[o:o + 4] = monta_call(SCAN_EM, S['gancho_scan'])
        print(f"  gancho do scan {SCAN_EM:#x}: chamada para {SCAN_ERA:#x} -> {S['gancho_scan']:#x} "
              '(pede redesenho a cada canal da varredura)')

    # 2 e 3. ponteiro da tabela de menus -> a nossa copia, e a quantidade de itens fixa.
    # Com MENU_NOVO desligado nada disso entra: o menu fica o de fabrica (sem os itens BANK), o
    # que serve de corte de diagnostico - o gancho, o filtro, as setas e os textos continuam.
    itens = len(tabela) // TAM_ITEM
    if MENU_NOVO:
        o = PONTEIRO_EM - BASE_APP
        antes = bytes(app[o:o + len(PONTEIRO_ANTES)])
        if antes != PONTEIRO_ANTES:
            raise SystemExit(f'{PONTEIRO_EM:#x} nao e o calculo esperado do endereco da tabela: {antes.hex()}')
        app[o:o + len(PONTEIRO_ANTES)] = monta_ponteiro(endereco_tabela)
        print(f'  tabela de menus {PONTEIRO_EM:#x}: {TABELA:#x} -> {endereco_tabela:#x} '
              f'({itens} itens: os {ITENS_ORIGINAIS} de fabrica, nenhum perdido, mais os dos bancos)')
        for pos, quem in enumerate(ordem_da_copia()):
            if not isinstance(quem, int):
                print(f"    item {pos:2d}: \"{quem['nome']}\" (max {quem['maximo']}, "
                      f"flags {quem['flags']:#04x}, logo depois do item {DEPOIS_DE})")

        o = CONTAGEM_EM - BASE_APP
        antes = bytes(app[o:o + len(CONTAGEM_ANTES)])
        if antes != CONTAGEM_ANTES:
            raise SystemExit(f'{CONTAGEM_EM:#x} nao e a chamada esperada da contagem: {antes.hex()}')
        app[o:o + len(CONTAGEM_ANTES)] = monta_contagem(itens)
        print(f'  quantidade de itens {CONTAGEM_EM:#x}: contagem na tabela -> {itens} fixo')
    else:
        print('  MENU_NOVO desligado: menu de fabrica, sem os itens BANK (corte de diagnostico)')

    # 4. sentido das setas no menu
    if SETAS_INVERTIDAS:
        for sitio, era in SETAS:
            o = sitio - BASE_APP
            antes = bytes(app[o:o + 4])
            if antes != monta_call(sitio, era):
                raise SystemExit(f'{sitio:#x} nao e a chamada esperada para {era:#x}: {antes.hex()}')
            app[o:o + 4] = monta_call(sitio, RETROCEDE if era == AVANCA else AVANCA)
        print(f'  setas do menu invertidas em {len(SETAS)} sitios: a de cima passa a subir na lista')

    # 4b. realce do VFO selecionado: o gancho entra na chamada que pinta a faixa da barra
    if REALCE:
        o = REALCE_EM - BASE_APP
        antes = bytes(app[o:o + 4])
        if antes != monta_call(REALCE_EM, REALCE_ERA):
            raise SystemExit(f'{REALCE_EM:#x} nao e a chamada esperada para {REALCE_ERA:#x}: '
                             f'{antes.hex()}')
        app[o:o + 4] = monta_call(REALCE_EM, S['gancho_barra'])
        print(f"  gancho do realce {REALCE_EM:#x}: chamada para {REALCE_ERA:#x} -> "
              f"{S['gancho_barra']:#x} (faixa da barra do VFO selecionado)")

    # 4c. diagnostico das faixas: so a cor do preenchimento, 2 bytes por sitio
    if REALCE_DIAG:
        for endereco, reg in REALCE_SITIOS:
            o = endereco - BASE_APP
            if bytes(app[o:o + 2]) != monta_imm8(reg, 0):
                raise SystemExit(f'{endereco:#x} nao e "r{reg} = 0": {bytes(app[o:o + 2]).hex()}')
            app[o:o + 2] = monta_imm8(reg, COR_REALCE)
        print(f'  DIAGNOSTICO: {len(REALCE_SITIOS)} faixas de largura total pintadas de '
              f'{COR_REALCE:#06x} (nao e a versao final)')

    # 5. revisao dos textos em ingles
    if TEXTOS_REVISADOS:
        for endereco, velho, texto in TEXTOS:
            o = endereco - BASE_APP
            if bytes(app[o:o + len(velho) + 1]) != velho.encode() + b'\0':
                raise SystemExit(f'{endereco:#x} nao contem "{velho}"')
            if len(texto) > len(velho):
                raise SystemExit(f'"{texto}" nao cabe no lugar de "{velho}"')
            app[o:o + len(velho) + 1] = texto.encode() + bytes(len(velho) + 1 - len(texto))
        print(f'  textos revisados: {len(TEXTOS)} cadeias, nenhum byte a mais')

    # 6. codigo novo, copia da tabela e CRCs
    print(f'codigo novo: {len(blob)} B em {ENDERECO_CODIGO:#x}; tabela: {len(tabela)} B em '
          f'{endereco_tabela:#x} (ultimo byte {endereco_tabela + len(tabela) - 1:#x})')
    blocos = [(ENDERECO_CODIGO, blob)] + ([(endereco_tabela, tabela)] if MENU_NOVO else [])
    jlfw.repack(origem, bytes(app), destino, blob=blocos)
    print(f'gerado: {destino}')


if __name__ == '__main__':
    main()
