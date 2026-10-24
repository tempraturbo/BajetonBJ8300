"""Confere um firmware montado com os bancos, antes de gravar no radio.

Uso: python -I conferir.py <firmware.anc> <bancos.bin> [original.anc]

Verifica, sem precisar do radio:
  - todos os CRCs do arquivo;
  - que o codigo novo e a tabela de menus nova estao nas 6 imagens, no mesmo endereco, e que
    decifrados pelo SFC saem iguais ao que foi montado (gravar em claro ali seria lixo);
  - que os simbolos que o menu aponta caem dentro do codigo novo;
  - que a tabela nova, gravada no lugar da original, tem os itens de fabrica que ficam intactos
    (saem o ZONE e o Zone Or Channel), os tres itens dos bancos certos, e que o firmware vai
    contar exatamente os itens dela;
  - que cada chamada trocada aponta o gancho certo e que o ponteiro da tabela ficou como era;
  - que no app.bin mudou **so** o que esta previsto, nada em area protegida.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jlfw
import montar

montar.S.update(montar.le_simbolos(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'simbolos.txt')))

novo, codigo = sys.argv[1], sys.argv[2]
original = sys.argv[3] if len(sys.argv) > 3 else r'C:\temp\BJ8300_V0.12.anc'
blob = open(codigo, 'rb').read()
erros = []


def partes(caminho):
    dados = jlfw.kdh_scramble(open(caminho, 'rb').read())
    _, entradas = jlfw.ufw_entries(dados)
    img = next(e for e in entradas if e['type'] == 0)
    flash = dados[img['offset']:img['offset'] + img['size']]
    chave = jlfw.find_chipkey(flash)
    _, base, sub = jlfw.app_dir(flash, chave)
    ent = next(s for s in sub if s['name'] == 'app.bin')
    app = jlfw.sfc(flash[base + ent['offset']:base + ent['offset'] + ent['size']], chave, ent['offset'])
    return dados, entradas, chave, app


dados, entradas, chave, app = partes(novo)
_, _, _, app_velho = partes(original)
tabela_esperada = montar.monta_tabela(app_velho)      # a copia certa sai do firmware de fabrica
fonte = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'bancos.c'),
             encoding='utf-8').read()
declaradas = {int(e, 16): n for n, e in re.findall(r'^FIRMWARE\((\w+), (0x[0-9A-Fa-f]+)\)', fonte, re.M)}


def declaradas_por_nome(nome):
    """O endereco de uma rotina declarada em FIRMWARE() no bancos.c."""
    return next(a for a, n in declaradas.items() if n == nome)


def define(nome):
    """O endereco de um #define de variavel do firmware no bancos.c."""
    return int(re.search(r'#define\s+%s\s+\(\*?\((?:const )?volatile u\d+ \*\)(0x[0-9A-Fa-f]+)\)' % nome,
                         fonte).group(1), 16)


def imediato(endereco):
    """O valor de 32 bits de um "rX = imm32" (cX ff + 4 bytes) do firmware de fabrica."""
    o = endereco - montar.BASE_APP
    assert app_velho[o + 1] == 0xFF and app_velho[o] & 0xF0 == 0xC0, hex(endereco)
    return struct.unpack('<I', app_velho[o + 2:o + 6])[0]

# 1. CRCs de todas as entradas
for e in entradas:
    corpo = dados[e['offset']:e['offset'] + e['size']]
    if e['type'] in jlfw.FLASH_TYPES:
        ok = jlfw.crc16(corpo) == e['crc']
    elif e['size'] == 0:
        ok = True
    else:
        ok = jlfw.crc16(corpo) == e['crc'] or jlfw.crc16(jlfw.sfc(corpo, chave, e['offset'])) == e['crc']
    if not ok:
        erros.append(f"CRC de {e['name']} nao confere")
print(f'1. CRCs das {len(entradas)} entradas: ' + ('ok' if not erros else 'FALHOU'))

# 2. codigo novo e tabela presentes, decifrados, nas 6 imagens
blocos = [('codigo', montar.ENDERECO_CODIGO, blob)]
if montar.MENU_NOVO:
    blocos.append(('tabela', montar.TABELA, tabela_esperada))
for nome, endereco, esperado in blocos:
    copias = 0
    for e in entradas:
        if e['type'] not in jlfw.FLASH_TYPES:
            continue
        img = dados[e['offset']:e['offset'] + e['size']]
        _, base, _ = jlfw.app_dir(img, chave)
        pos = endereco - montar.BASE_APP + base + 0x100
        lido = jlfw.sfc(img[pos:pos + len(esperado)], chave, pos - base)
        if lido == esperado:
            copias += 1
        elif img[pos:pos + len(esperado)] == esperado:
            erros.append(f"{nome} gravado EM CLARO em {e['name']}: o SFC vai entregar lixo")
        else:
            erros.append(f"{nome} ausente ou diferente em {e['name']}")
    print(f'2. {nome} ({len(esperado)} B) em {endereco:#x}, decifrado: {copias}/6 imagens')

# 2b. os simbolos que o menu aponta caem todos dentro do codigo novo
fim_codigo = montar.ENDERECO_CODIGO + len(blob)
fora = {n: a for n, a in montar.S.items() if not montar.ENDERECO_CODIGO <= a < fim_codigo}
print(f'2b. simbolos dentro de {montar.ENDERECO_CODIGO:#x}..{fim_codigo:#x}: '
      + ('ok' if not fora else 'FALHOU ' + str({n: hex(a) for n, a in fora.items()})))
if fora:
    erros.append('simbolo fora do codigo novo: ' + ', '.join(fora))

# 2c. o codigo foi gravado exatamente onde o linker o ligou. Na v20 o .text saiu ligado para
# 0x01E96124 (o linker alinhou a secao a 4 por causa de uma tabela de ponteiros) e foi gravado em
# 0x01E96122: os conferes acima passavam, porque todos usavam o mesmo endereco fixo dos dois lados,
# e o radio entrou em bootloop. Aqui o endereco e o conteudo vem do proprio ELF.
ligado, conteudo = montar.codigo_ligado(os.path.splitext(codigo)[0] + '.elf')
ok = ligado == montar.ENDERECO_CODIGO and conteudo == blob
print(f'2c. .text ligado para {ligado:#x}, gravado em {montar.ENDERECO_CODIGO:#x}, mesmo conteudo: '
      + ('ok' if ok else 'FALHOU'))
if not ok:
    erros.append(f'codigo ligado para {ligado:#x} e gravado em {montar.ENDERECO_CODIGO:#x}')

# 2d. de ponta a ponta: todo call/goto do app.bin remendado que vai para o codigo novo tem de cair
# no inicio de uma funcao nossa, e os bytes que estao LA na flash, decifrados, nas 6 imagens, tem de
# ser os do ELF. E a conferencia que teria pego a v20 sozinha.
funcoes = {a: n for n, a in montar.le_simbolos(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'simbolos.txt')).items()}
imagens = []
for e in entradas:
    if e['type'] in jlfw.FLASH_TYPES:
        img = dados[e['offset']:e['offset'] + e['size']]
        imagens.append((e['name'], img, jlfw.app_dir(img, chave)[1]))
desvios_novos, ruins = 0, []
for o in range(0, len(app) - 3, 2):
    if app[o + 1] != 0xEA or not app[o] & 0x80:
        continue
    d22 = ((app[o] & 0x3F) << 16) | (app[o + 3] << 8) | app[o + 2]
    destino = montar.BASE_APP + o + 4 + 2 * (d22 - 0x400000 if d22 & 0x200000 else d22)
    if not ligado <= destino < ligado + len(conteudo):
        continue
    desvios_novos += 1
    if destino not in funcoes:
        ruins.append(f'{montar.BASE_APP + o:#x} -> {destino:#x} (nao e inicio de funcao)')
        continue
    for nome_img, img, base in imagens:
        pos = destino - montar.BASE_APP + base + 0x100
        if jlfw.sfc(img[pos:pos + 16], chave, pos - base) != conteudo[destino - ligado:destino - ligado + 16]:
            ruins.append(f'{montar.BASE_APP + o:#x} -> {funcoes[destino]}: bytes diferentes em {nome_img}')
print(f'2d. {desvios_novos} desvios do firmware para o codigo novo, destino conferido na flash das '
      f'{len(imagens)} imagens: ' + ('ok' if not ruins else 'FALHOU'))
for r in ruins:
    print('    ' + r)
    erros.append('desvio para o codigo novo errado: ' + r)

# 3. a copia da tabela: nenhum item de fabrica perdido e os dos bancos no lugar certo
T = montar.TAM_ITEM
velha = app_velho[montar.TABELA - montar.BASE_APP:]
# Os itens em ITENS_FORA saem de proposito; qualquer outro que suma e erro.
esperados = [i for i in range(montar.ITENS_ORIGINAIS) if i not in montar.ITENS_FORA]


def com_mudancas(i):
    """O item de fabrica i com os campos de ITENS_MUDADOS trocados - e so eles."""
    item = bytearray(velha[i * T:(i + 1) * T])
    for campo, valor in montar.ITENS_MUDADOS.get(i, {}).items():
        if campo == 'maximo':
            struct.pack_into('<H', item, 1, valor)
        else:
            struct.pack_into('<I', item, montar.CAMPO[campo], montar.S[valor])
    return bytes(item)


faltando = [] if not montar.MENU_NOVO else [i for i in esperados
            if com_mudancas(i) not in
            [tabela_esperada[j * T:(j + 1) * T] for j in range(len(tabela_esperada) // T)]]
sobrando = [i for i in montar.ITENS_FORA if montar.MENU_NOVO and
            velha[i * T:(i + 1) * T] in
            [tabela_esperada[j * T:(j + 1) * T] for j in range(len(tabela_esperada) // T)]]
print(f'3. os {len(esperados)} itens de fabrica que ficam estao na copia: '
      + ('ok' if not faltando else f'FALHOU, faltam {faltando}')
      + f'; fora de proposito: {list(montar.ITENS_FORA)}'
      + (f' (mas {sobrando} continuam la!)' if sobrando else '')
      + ''.join(f'; item {i} com {", ".join(sorted(m))} trocados' for i, m in montar.ITENS_MUDADOS.items()))
if faltando:
    erros.append(f'itens de fabrica perdidos: {faltando}')
if sobrando:
    erros.append(f'itens que deviam sair continuam na tabela: {sobrando}')

for pos, quem in enumerate(montar.ordem_da_copia() if montar.MENU_NOVO else []):
    if isinstance(quem, int):
        continue
    o = pos * T
    flags = tabela_esperada[o]
    maximo = struct.unpack_from('<H', tabela_esperada, o + 1)[0]
    campos = {c: struct.unpack_from('<I', tabela_esperada, o + montar.CAMPO[c])[0]
              for c in ('get', 'set', 'nome', 'nome_cn', 'exibe')}     # os campos que os itens dos bancos definem
    esperado = {'get': montar.S[quem['get']], 'set': montar.S[quem['set']],
                'nome': montar.S[quem['titulo']], 'nome_cn': montar.S[quem['titulo']],
                'exibe': montar.S[quem['exibe']]}
    bate = flags == quem['flags'] and maximo == quem['maximo'] and campos == esperado
    print(f"   item {pos:2d} {quem['nome']:10s} flags={flags:#04x} max={maximo:4d} -> "
          + ('ok' if bate else 'FALHOU'))
    if not bate:
        erros.append(f"item {quem['nome']} errado")

itens = len(tabela_esperada) // T
if not montar.MENU_NOVO:
    print('   (MENU_NOVO desligado: tabela, contagem e itens nao entram neste firmware)')
o = montar.CONTAGEM_EM - montar.BASE_APP
atual = bytes(app[o:o + len(montar.CONTAGEM_ANTES)])
ok = atual == montar.monta_contagem(itens) or not montar.MENU_NOVO
if montar.MENU_NOVO:
    print(f'   quantidade de itens fixada em {montar.CONTAGEM_EM:#x} = {itens}: '
          + ('ok' if ok else f'FALHOU ({atual.hex()})'))
if not ok:
    erros.append('quantidade de itens errada')

if montar.SETAS_INVERTIDAS:
    trocadas = 0
    for sitio, era in montar.SETAS:
        o = sitio - montar.BASE_APP
        outra = montar.RETROCEDE if era == montar.AVANCA else montar.AVANCA
        if bytes(app[o:o + 4]) == montar.monta_call(sitio, outra):
            trocadas += 1
    print(f'   setas invertidas: {trocadas}/{len(montar.SETAS)} sitios')
    if trocadas != len(montar.SETAS):
        erros.append('sentido das setas nao foi trocado em todos os sitios')

# 4. gancho e ponteiro da tabela
o = montar.GANCHO_EM - montar.BASE_APP
atual = bytes(app[o:o + 4])
ok = atual == montar.monta_call(montar.GANCHO_EM, montar.S['gancho_partida'])
print(f'4. gancho em {montar.GANCHO_EM:#x}: ' + ('ok' if ok else f'FALHOU ({atual.hex()})'))
if not ok:
    erros.append('gancho errado')

if montar.CABO_ESTENDIDO:
    for sitio, papel in montar.CABO_SITIOS:
        o = sitio - montar.BASE_APP
        ok = bytes(app[o:o + 4]) == montar.monta_call(sitio, montar.S['gancho_cabo'])
        print(f'4d. gancho do cabo em {sitio:#x} ({papel}): ' + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'gancho do cabo errado em {sitio:#x}')

if montar.SCAN_REDESENHA:
    o = montar.SCAN_EM - montar.BASE_APP
    ok = bytes(app[o:o + 4]) == montar.monta_call(montar.SCAN_EM, montar.S['gancho_scan'])
    print(f'4a. gancho do scan em {montar.SCAN_EM:#x}: ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append('gancho do scan errado')

o = montar.PONTEIRO_EM - montar.BASE_APP
atual = bytes(app[o:o + len(montar.PONTEIRO_ANTES)])
ok = atual == montar.PONTEIRO_ANTES
print(f'4b. ponteiro da tabela em {montar.PONTEIRO_EM:#x} intacto (continua em {montar.TABELA:#x}): '
      + ('ok' if ok else f'FALHOU ({atual.hex()})'))
if not ok:
    erros.append('ponteiro da tabela mudou')

# 5. nada escrito fora dos trechos previstos - e nada em area protegida
previsto = ([(montar.TABELA, montar.TABELA + len(tabela_esperada) - 1, 'tabela'),
             (montar.CONTAGEM_EM, montar.CONTAGEM_EM + len(montar.CONTAGEM_ANTES) - 1, 'contagem')]
            if montar.MENU_NOVO else [])
previsto += [(e, e + len(v), f'texto {v}') for e, v, _ in (montar.TEXTOS if montar.TEXTOS_REVISADOS else ())]
previsto += [(sitio, sitio + 3, 'setas') for sitio, _ in (montar.SETAS if montar.SETAS_INVERTIDAS else ())]
previsto += [(montar.GANCHO_EM, montar.GANCHO_EM + 3, 'gancho')]
if montar.SCAN_REDESENHA:
    previsto += [(montar.SCAN_EM, montar.SCAN_EM + 3, 'gancho do scan')]
if montar.ESPECTRO_TECLAS:
    o = montar.ESPECTRO_EM - montar.BASE_APP
    ok = (bytes(app_velho[o:o + 4]) == montar.ESPECTRO_ANTES
          and bytes(app[o:o + 4]) == montar.monta_call(montar.ESPECTRO_EM, montar.S['gancho_tecla_espectro']))
    print(f'4h. gancho das teclas do espectro em {montar.ESPECTRO_EM:#x}: ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append('gancho das teclas do espectro errado')
    previsto += [(montar.ESPECTRO_EM, montar.ESPECTRO_EM + 3, 'gancho das teclas do espectro')]
    for sitio, era, gancho, oque in montar.ESPECTRO_CHAMADAS:
        o = sitio - montar.BASE_APP
        ok = (bytes(app_velho[o:o + 4]) == montar.monta_call(sitio, era)
              and bytes(app[o:o + 4]) == montar.monta_call(sitio, montar.S[gancho]))
        print(f'4h. espectro {sitio:#x}: {era:#x} -> {gancho} ({oque}): ' + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'chamada do espectro errada em {sitio:#x}')
    for entrada, antes, nova, oque in montar.ESPECTRO_FUNCOES:
        o = entrada - montar.BASE_APP
        ok = (bytes(app_velho[o:o + 4]) == antes
              and bytes(app[o:o + 4]) == montar.monta_goto(entrada, montar.S[nova]))
        print(f'4h. espectro {entrada:#x} trocada por {nova} ({oque}): ' + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'rotina do espectro mal trocada em {entrada:#x}')
    previsto += [(s, s + 3, 'espectro') for s, _, _, _ in montar.ESPECTRO_CHAMADAS]
    previsto += [(e, e + 3, 'espectro') for e, _, _, _ in montar.ESPECTRO_FUNCOES]
    # Os enderecos que os ganchos do espectro usam, tirados das instrucoes do firmware que os usam
    # (o "rX = imm32" de cada uma, mais o deslocamento com que ela acessa), nunca digitados.
    for nome, instrucao, soma in (('ESPECTRO_ESTADO', 0x01E4AEAE, 0), ('ESP_BRUTO', 0x01E4AECA, 0),
                                  ('ESP_MIN', 0x01E4ADA2, 0), ('ESP_MAX', 0x01E4AE80, 0),
                                  ('ESP_INDICE', 0x01E4AE4A, 0), ('ESP_PASSO', 0x01E4AE66, 0),
                                  ('ESP_FREQ', 0x01E4AA68, 0), ('ESP_PICO_FREQ', 0x01E4AA68, 4),
                                  ('ESP_CENTRO', 0x01E4AA68, 8), ('ESP_FIM', 0x01E4AA68, 12),
                                  ('ESP_INICIO', 0x01E4AA68, 16), ('ALTURAS', 0x01E4AD8E, 3739),
                                  ('CFG_PONTOS', 0x01E4A9C2, 4), ('CFG_PASSO', 0x01E4A99C, 8),
                                  ('CFG_FILTRO', 0x01E4A99C, 16)):
        no_c, no_firmware = define(nome), imediato(instrucao) + soma
        ok = no_c == no_firmware
        print(f'4f. {nome} = {no_c:#x}, o firmware usa {no_firmware:#x} ({instrucao:#x} + {soma}): '
              + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'{nome} nao e o endereco que o firmware usa')
if montar.REALCE:
    for sitio, oque in montar.REALCE_SITIOS_GANCHO:
        o = sitio - montar.BASE_APP
        ok = bytes(app[o:o + 4]) == montar.monta_call(sitio, montar.S['gancho_envia'])
        print(f'4e. gancho do realce em {sitio:#x} ({oque.split(",")[0]}): '
              + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'gancho do realce errado em {sitio:#x}')
    previsto += [(e, e + 3, 'gancho do realce') for e, _ in montar.REALCE_SITIOS_GANCHO]
    for sitio, oque in montar.LIMPA_SITIOS:
        o = sitio - montar.BASE_APP
        ok = bytes(app[o:o + 4]) == montar.monta_call(sitio, montar.S['gancho_limpa'])
        print(f'4e. gancho da limpeza em {sitio:#x} ({oque.split(":")[0]}): '
              + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'gancho da limpeza errado em {sitio:#x}')
    previsto += [(e, e + 3, 'gancho da limpeza') for e, _ in montar.LIMPA_SITIOS]

    # 4f. os enderecos que o gancho le sao os que a janela() grava - tirados das PROPRIAS instrucoes
    # dela no firmware de fabrica, nunca convertidos a mao. Na v15 eles foram convertidos errado do
    # decimal do objdump (0x40 a menos) e o gancho lia tres variaveis alheias.
    for nome, instrucao, papel in (('JAN_Y0', 0x01E3AD3E, 'y0'), ('JAN_Y1', 0x01E3AD5E, 'y1'),
                                   ('JAN_LINHA', 0x01E3AD6E, 'bytes por linha'),
                                   ('VIDEO', 0x01E3AD86, 'buffer de video')):
        no_c, no_firmware = define(nome), imediato(instrucao)
        ok = no_c == no_firmware
        print(f'4f. {nome} = {no_c:#x}, a janela() usa {no_firmware:#x} ({papel}, '
              f'{instrucao:#x}): ' + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'{nome} nao e o endereco que a janela() usa')

# 4g. todo call/goto do codigo novo vai para dentro dele mesmo ou para uma rotina do firmware
# declarada em FIRMWARE() no bancos.c. As rotinas viraram simbolos absolutos e a chamada e
# relativa, resolvida pelo linker: aqui o destino de cada uma e recalculado do proprio deslocamento
# que esta nos bytes (com ou sem rotulo no desmontado), e chamada por registrador e recusada,
# porque o destino dela nao se confere.
fim_novo = montar.ENDERECO_CODIGO + len(blob)
usadas, estranhas = {}, []
desmontado = open(os.path.join(os.path.dirname(codigo), 'bancos.dis'), encoding='utf-8',
                  errors='replace').read().splitlines()
for linha in desmontado:
    m = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(call|goto)\s+(\S+)', linha)
    if not m:
        continue
    onde, tam, alvo = int(m.group(1), 16), len(m.group(2).split()), m.group(4)
    if not re.fullmatch(r'-?\d+', alvo):
        estranhas.append(f'{onde:#x} {m.group(3)} {alvo} (por registrador)')
        continue
    destino = onde + tam + int(alvo)
    if montar.ENDERECO_CODIGO <= destino < fim_novo:
        continue
    if destino in declaradas:
        usadas[declaradas[destino]] = usadas.get(declaradas[destino], 0) + 1
    else:
        estranhas.append(f'{onde:#x} {m.group(3)} -> {destino:#x} (fora do codigo e nao declarado)')
print(f'4g. chamadas ao firmware: {sum(usadas.values())}, todas para enderecos declarados '
      f'({len(usadas)} rotinas): ' + ('ok' if not estranhas else 'FALHOU'))
for e in estranhas:
    print('    ' + e)
    erros.append('chamada com destino nao conferido: ' + e)

# 4i. a seta do espectro faz a MESMA retomada do firmware. Os quatro passos sao tirados dos bytes
# do firmware de fabrica (o bloco 0x01E4AEFC..0x01E4AF10, que roda quando o sinal achado cai, e a
# chamada do proximo ponto em 0x01E4AF86, para onde ele desvia), nunca digitados: cada call tem de
# ir para uma rotina declarada em FIRMWARE() com o r0 da instrucao logo antes. Depois, o gancho tem
# de fazer as mesmas chamadas, com os mesmos r0, na mesma ordem, e a variavel de estado que ele
# zera tem de ser a que o firmware zera (r6 do proprio tratador, 0x01E4AEAE).
if montar.ESPECTRO_TECLAS:
    retomada = []
    for chamada, r0_em in ((0x01E4AF00, 0x01E4AEFC), (0x01E4AF06, 0x01E4AF04),
                           (0x01E4AF0C, 0x01E4AF0A), (0x01E4AF86, None)):
        o = chamada - montar.BASE_APP
        alvo = next((a for a in declaradas if bytes(app_velho[o:o + 4]) == montar.monta_call(chamada, a)),
                    None)
        r0 = next((v for v in range(256) if r0_em and
                   bytes(app_velho[r0_em - montar.BASE_APP:r0_em - montar.BASE_APP + 2])
                   == montar.monta_imm8(0, v)), None)
        retomada.append((r0, alvo))
    no_gancho, r0, dentro = [], None, False
    for linha in desmontado:
        if re.match(r'^[^.\s]\S*:$', linha.strip()):    # rotulo de funcao; ".GJTIS..." e tabela de salto local
            dentro = linha.strip() == 'gancho_tecla_espectro:'
            continue
        m = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$', linha)
        if not dentro or not m:
            continue
        texto = m.group(3).strip()
        v = re.match(r'r0 = (\d+)$', texto)
        if v:
            r0 = int(v.group(1))
        c = re.match(r'call (-?\d+)', texto)
        if c:
            no_gancho.append((r0, int(m.group(1), 16) + len(m.group(2).split()) + int(c.group(1))))
            r0 = None
    # O proximo ponto passa pelo gancho_avanca (sub-pontos), no firmware remendado e no gancho: a
    # retomada esperada e a de fabrica com a quarta chamada no lugar dele, e o sitio 0x01E4AF86 tem
    # de estar trocado por ele (conferido acima, na lista das chamadas do espectro).
    esperada = retomada[:3] + [(None, montar.S['gancho_avanca'])]
    o = 0x01E4AF86 - montar.BASE_APP
    estado = imediato(0x01E4AEAE)
    ok = (None not in [a for _, a in retomada] and declaradas.get(retomada[3][1]) == 'ESPECTRO_AVANCA'
          and bytes(app[o:o + 4]) == montar.monta_call(0x01E4AF86, montar.S['gancho_avanca'])
          and any(no_gancho[i:i + 4] == esperada for i in range(len(no_gancho)))
          and define('ESPECTRO_ESTADO') == estado)
    print('4i. seta do espectro = retomada do firmware: '
          + ', '.join(f"{declaradas.get(a, hex(a or 0))}({'' if v is None else v})" for v, a in retomada)
          + ' (o ultimo pelo gancho_avanca)'
          + f', estado em {estado:#x}: ' + ('ok' if ok else 'FALHOU (gancho: '
                                            + ', '.join(f'{a:#x}({v})' for v, a in no_gancho) + ')'))
    if not ok:
        erros.append('a seta do espectro nao repete a retomada do firmware')


def imediato16(endereco):
    """O valor de 16 bits de um "rX = imm16" (4X e0 + 2 bytes) do firmware de fabrica."""
    o = endereco - montar.BASE_APP
    assert app_velho[o + 1] == 0xE0 and app_velho[o] & 0xF8 == 0x40, hex(endereco)
    return struct.unpack('<H', app_velho[o + 2:o + 4])[0]


# 4j. roger BEEP 2. Os enderecos saem das instrucoes do firmware: a variavel e a que o get do item
# (0x01E3C91E) e a propria rotina do roger (0x01E44606) leem; a tabela do BEEP e a que o laco
# de fabrica percorre (r4 de 0x01E44614 + 2051, dois pares de tras para frente); as rotinas de tom e
# de espera sao as que o laco chama. Os dois remendos: os bytes de fabrica e os novos.
roger_get = imediato(0x01E3C922) + imediato16(0x01E3C91E)
roger_rotina = imediato(0x01E4460A) + imediato16(0x01E44606)
beep_tabela = imediato(0x01E44614) + 2051 - 6
o_laco, o_desvio = montar.ROGER_LACO_EM - montar.BASE_APP, montar.ROGER_DESVIO_EM - montar.BASE_APP
cn = struct.unpack_from('<I', blob, montar.S['roger_opcoes_cn'] - montar.ENDERECO_CODIGO)[0]
cn_fabrica = struct.unpack_from('<I', app_velho, 0x6AC0C + 0x6E30)[0]      # tabela chinesa, RAM 0x6E30
verificacoes = [
    ('ROGER e a variavel do item e da rotina', define('ROGER') == roger_get == roger_rotina),
    ('tabela do BEEP de fabrica', define('BEEP_FABRICA') == beep_tabela),
    ('TOM e a rotina que o laco chamava', bytes(app_velho[o_laco + 0xC:o_laco + 0x10])
     == montar.monta_call(montar.ROGER_LACO_EM + 0xC, declaradas_por_nome('TOM'))),
    ('ESPERA_2MS e a rotina que o laco chamava', bytes(app_velho[o_laco + 0x12:o_laco + 0x16])
     == montar.monta_call(montar.ROGER_LACO_EM + 0x12, declaradas_por_nome('ESPERA_2MS'))),
    ('desvio do valor 3', bytes(app_velho[o_desvio:o_desvio + 4]) == montar.ROGER_DESVIO_ANTES
     and bytes(app[o_desvio:o_desvio + 4]) == montar.ROGER_DESVIO_DEPOIS),
    ('laco dos tons', bytes(app_velho[o_laco:o_laco + 8]) == montar.ROGER_LACO_ANTES
     and bytes(app[o_laco:o_laco + 8]) == montar.monta_call(montar.ROGER_LACO_EM, montar.S['gancho_roger_tons'])
     + montar.monta_goto(montar.ROGER_LACO_EM + 4, montar.ROGER_LACO_FIM)),
    ('OFF em chines e o de fabrica', cn == cn_fabrica),
]
for oque, ok in verificacoes:
    print(f'4j. roger: {oque}: ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append('roger: ' + oque)
previsto += [(montar.ROGER_DESVIO_EM, montar.ROGER_DESVIO_EM + 3, 'roger: desvio'),
             (montar.ROGER_LACO_EM, montar.ROGER_LACO_EM + 7, 'roger: laco')]

if montar.REALCE_DIAG:
    certas = sum(bytes(app[e - montar.BASE_APP:e - montar.BASE_APP + 2])
                 == montar.monta_imm8(r, montar.COR_REALCE) for e, r in montar.REALCE_SITIOS)
    print(f'4e. faixas pintadas: {certas}/{len(montar.REALCE_SITIOS)}')
    if certas != len(montar.REALCE_SITIOS):
        erros.append('cor de faixa errada')
    previsto += [(e, e + 1, 'cor de faixa') for e, _ in montar.REALCE_SITIOS]
if montar.CABO_ESTENDIDO:
    previsto += [(s, s + 3, f'gancho do cabo ({p})') for s, p in montar.CABO_SITIOS]
mudados = [i for i in range(len(app)) if app[i] != app_velho[i]]
if montar.TEXTOS_REVISADOS:
    certos = 0
    for endereco, velho, texto in montar.TEXTOS:
        o = endereco - montar.BASE_APP
        if bytes(app[o:o + len(texto) + 1]) == texto.encode() + b'\0':
            certos += 1
    print(f'4c. textos revisados: {certos}/{len(montar.TEXTOS)} cadeias')
    if certos != len(montar.TEXTOS):
        erros.append('texto revisado faltando')

print(f'5. bytes alterados no app.bin: {len(mudados)} '
      f'(previstos: ate {sum(fim - ini + 1 for ini, fim, _ in previsto)}, em {len(previsto)} trechos)')
for i in mudados:
    endereco = montar.BASE_APP + i
    for ini, fim, oque in montar.PROTEGIDAS:
        if ini <= endereco <= fim:
            erros.append(f'{endereco:#x} esta em area protegida ({oque})')
    if not any(ini <= endereco <= fim for ini, fim, _ in previsto):
        erros.append(f'{endereco:#x} mudou fora dos trechos previstos')
print('   todos dentro dos trechos previstos, nada em area protegida: '
      + ('ok' if not any(('protegida' in x or 'mudou fora' in x) for x in erros) else 'FALHOU'))

print()
print('RESULTADO: ' + ('tudo certo' if not erros else 'PROBLEMAS: ' + '; '.join(erros)))
sys.exit(1 if erros else 0)
