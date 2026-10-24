"""Confere um firmware montado com os bancos, antes de gravar no radio.

Uso: python -I conferir.py <firmware.anc> <bancos.bin> [original.anc]

Verifica, sem precisar do radio:
  - todos os CRCs do arquivo;
  - que o codigo novo e a copia da tabela de menus estao nas 6 imagens, no mesmo endereco, e que
    decifrados pelo SFC saem iguais ao que foi compilado (gravar em claro ali seria lixo);
  - que os simbolos que o menu aponta caem dentro do codigo novo;
  - que a copia da tabela tem os 53 itens de fabrica intactos (menos o ZONE, que virou BANK), os
    tres itens dos bancos certos, o terminador no fim, e que o firmware vai contar 55 itens;
  - que a chamada do gancho e o ponteiro da tabela foram trocados pelos valores certos;
  - que no app.bin mudaram **so** esses 10 bytes, nenhum em area protegida.
"""
import os
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
ENDERECO_TABELA = montar.alinha32(montar.ENDERECO_CODIGO + len(blob))
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
    blocos.append(('tabela', ENDERECO_TABELA, tabela_esperada))
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

# 3. a copia da tabela: nenhum item de fabrica perdido e os dos bancos no lugar certo
T = montar.TAM_ITEM
velha = app_velho[montar.TABELA - montar.BASE_APP:]
# Os itens em ITENS_FORA saem de proposito; qualquer outro que suma e erro.
esperados = [i for i in range(montar.ITENS_ORIGINAIS) if i not in montar.ITENS_FORA]
faltando = [] if not montar.MENU_NOVO else [i for i in esperados
            if velha[i * T:(i + 1) * T] not in
            [tabela_esperada[j * T:(j + 1) * T] for j in range(len(tabela_esperada) // T)]]
sobrando = [i for i in montar.ITENS_FORA if montar.MENU_NOVO and
            velha[i * T:(i + 1) * T] in
            [tabela_esperada[j * T:(j + 1) * T] for j in range(len(tabela_esperada) // T)]]
print(f'3. os {len(esperados)} itens de fabrica que ficam estao na copia: '
      + ('ok' if not faltando else f'FALHOU, faltam {faltando}')
      + f'; fora de proposito: {list(montar.ITENS_FORA)}'
      + (f' (mas {sobrando} continuam la!)' if sobrando else ''))
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
    campos = {c: struct.unpack_from('<I', tabela_esperada, o + d)[0] for c, d in montar.CAMPO.items()}
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
ok = atual == montar.monta_ponteiro(ENDERECO_TABELA) or not montar.MENU_NOVO
if montar.MENU_NOVO:
    print(f'4b. ponteiro da tabela em {montar.PONTEIRO_EM:#x} -> {ENDERECO_TABELA:#x}: '
          + ('ok' if ok else f'FALHOU ({atual.hex()})'))
if not ok:
    erros.append('ponteiro da tabela errado')

# 5. nada escrito fora do gancho e do ponteiro - e nada em area protegida
previsto = ([(montar.PONTEIRO_EM, montar.PONTEIRO_EM + len(montar.PONTEIRO_ANTES) - 1, 'ponteiro'),
             (montar.CONTAGEM_EM, montar.CONTAGEM_EM + len(montar.CONTAGEM_ANTES) - 1, 'contagem')]
            if montar.MENU_NOVO else [])
previsto += [(e, e + len(v), f'texto {v}') for e, v, _ in (montar.TEXTOS if montar.TEXTOS_REVISADOS else ())]
previsto += [(sitio, sitio + 3, 'setas') for sitio, _ in (montar.SETAS if montar.SETAS_INVERTIDAS else ())]
previsto += [(montar.GANCHO_EM, montar.GANCHO_EM + 3, 'gancho')]
if montar.SCAN_REDESENHA:
    previsto += [(montar.SCAN_EM, montar.SCAN_EM + 3, 'gancho do scan')]
if montar.REALCE:
    o = montar.REALCE_EM - montar.BASE_APP
    ok = bytes(app[o:o + 4]) == montar.monta_call(montar.REALCE_EM, montar.S['gancho_barra'])
    print(f'4e. gancho do realce em {montar.REALCE_EM:#x}: ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append('gancho do realce errado')
    previsto += [(montar.REALCE_EM, montar.REALCE_EM + 3, 'gancho do realce')]

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
      f'(esperado: ate {14 + 16 * bool(montar.SETAS_INVERTIDAS) + (sum(len(v) + 1 for _, v, _ in montar.TEXTOS) if montar.TEXTOS_REVISADOS else 0)})')
for i in mudados:
    endereco = montar.BASE_APP + i
    for ini, fim, oque in montar.PROTEGIDAS:
        if ini <= endereco <= fim:
            erros.append(f'{endereco:#x} esta em area protegida ({oque})')
    if not any(ini <= endereco <= fim for ini, fim, _ in previsto):
        erros.append(f'{endereco:#x} mudou fora dos tres remendos previstos')
print('   so no gancho, no ponteiro e na contagem, nada em area protegida: '
      + ('ok' if not any(('protegida' in x or 'mudou fora' in x) for x in erros) else 'FALHOU'))

print()
print('RESULTADO: ' + ('tudo certo' if not erros else 'PROBLEMAS: ' + '; '.join(erros)))
sys.exit(1 if erros else 0)
