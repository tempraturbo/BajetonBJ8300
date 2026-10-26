"""Confere um firmware montado com os bancos, antes de gravar no radio.

Uso: python -I conferir.py <firmware.anc> <bancos.bin> [original.anc]

Verifica, sem precisar do radio:
  - todos os CRCs do arquivo;
  - que o codigo novo e a tabela de menus nova estao nas 6 imagens, no mesmo endereco, e que
    decifrados pelo SFC saem iguais ao que foi montado (gravar em claro ali seria lixo);
  - que o app.bin decifrado e o mesmo nas 6 imagens, de modo que os remendos conferidos na
    primeira valem para todas;
  - que os simbolos que o menu aponta caem dentro do codigo novo;
  - que nas vozes so mudam o C0 e o C1 (o carregador recusa mais que isso) e que a tela B esta no
    lugar do decodificador G.729, fora do registro dos decodificadores e sem porta nova para la;
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
dados_velhos, entradas_velhas, _, app_velho = partes(original)
tabela_esperada = montar.monta_tabela(app_velho)      # a copia certa sai do firmware de fabrica
fonte = ''.join(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', f),
                     encoding='utf-8').read() for f in ('bancos.c', 'tela.c', 'telas.c'))
declaradas = {int(e, 16): n for n, e in re.findall(r'^FIRMWARE\((\w+), (0x[0-9A-Fa-f]+)\)', fonte, re.M)}


def declaradas_por_nome(nome):
    """O endereco de uma rotina declarada em FIRMWARE() no bancos.c ou no tela.c."""
    return next(a for a, n in declaradas.items() if n == nome)


def define(nome):
    """O endereco de um #define de variavel do firmware no bancos.c ou no tela.c: (*(volatile u8 *)0x..),
    ((volatile u8 *)0x..), RAM(u8, 0x..) ou o numero puro."""
    return int(re.search(r'#define\s+%s\s+(?:\(\*?\((?:const )?volatile u\d+ \*\)|RAM\(u\d+, )?(0x[0-9A-Fa-f]+)'
                         % nome, fonte).group(1), 16)


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
elf = os.path.splitext(codigo)[0] + '.elf'
livre_em, tela = montar.codigo_ligado(elf, '.livre')
# v43: a tabela do menu mora no vetor menu_tabela do .livre - o que vai a flash e o .livre com ela
tela_gravada = montar.livre_com_tabela(tela, tabela_esperada) if montar.MENU_NOVO else tela
blocos = [('codigo', montar.ENDERECO_CODIGO, blob), ('tela B', livre_em, tela_gravada)]
if montar.MENU_NOVO:
    blocos.append(('tabela', montar.S[montar.TABELA_SIMBOLO], tabela_esperada))
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

# 2b. os simbolos caem todos dentro do codigo novo: o .text no fim da imagem, o .vozes no espaco que
# era das vozes (v33) ou o .livre no lugar dos decodificadores G.729 e MP3 (v36, v37). Das rotinas que
# o firmware chama, no .vozes so as do espectro (montar.NAS_VOZES), no .livre so as da tela B
# (montar.NA_LIVRE), e
# as outras - a partida, o cabo, os menus - no .text. O .vozes nao passa do fim do C1: o carregador
# recusa a imagem inteira se mais arquivos de voz mudam (v35).
fim_codigo = montar.ENDERECO_CODIGO + len(blob)
vozes_em, vozes = montar.codigo_ligado(elf, '.vozes')
fim_vozes = vozes_em + len(vozes)
fim_livre = livre_em + len(tela)
no_texto = lambda a: montar.ENDERECO_CODIGO <= a < fim_codigo
nas_vozes = lambda a: vozes_em <= a < fim_vozes
na_livre = lambda a: livre_em <= a < fim_livre
fora = {n: a for n, a in montar.S.items() if not no_texto(a) and not nas_vozes(a) and not na_livre(a)}
trocados = sorted(n for n in montar.PRECISA if nas_vozes(montar.S[n]) != (n in montar.NAS_VOZES)
                  or na_livre(montar.S[n]) != (n in montar.NA_LIVRE))
lugares = (vozes_em == montar.VOZES and fim_vozes <= montar.VOZES_ACEITAS_FIM
           and livre_em == montar.LIVRE and fim_livre <= montar.LIVRE_FIM)
print(f'2b. simbolos dentro de {montar.ENDERECO_CODIGO:#x}..{fim_codigo:#x}, {vozes_em:#x}..{fim_vozes:#x} '
      f'(ate o fim do C1, {montar.VOZES_ACEITAS_FIM:#x}) ou {livre_em:#x}..{fim_livre:#x} (a faixa do G.729 '
      f'e do MP3 vai ate {montar.LIVRE_FIM:#x}); nas vozes so os {len(montar.NAS_VOZES)} do espectro, na '
      f'faixa livre so os {len(montar.NA_LIVRE)} da tela B: '
      + ('ok' if not fora and not trocados and lugares else 'FALHOU ' + str({n: hex(a) for n, a in fora.items()})
         + (f' fora do lugar: {trocados}' if trocados else '')))
if fora:
    erros.append('simbolo fora do codigo novo: ' + ', '.join(fora))
if trocados:
    erros.append('rotina no lugar errado (.text x .vozes x .livre): ' + ', '.join(trocados))
if not lugares:
    erros.append('.vozes ou .livre fora do lugar aceito')

# 2c. o codigo foi gravado exatamente onde o linker o ligou. Na v20 o .text saiu ligado para
# 0x01E96124 (o linker alinhou a secao a 4 por causa de uma tabela de ponteiros) e foi gravado em
# 0x01E96122: os conferes acima passavam, porque todos usavam o mesmo endereco fixo dos dois lados,
# e o radio entrou em bootloop. Aqui o endereco e o conteudo vem do proprio ELF.
ligado, conteudo = montar.codigo_ligado(elf)
ok = ligado == montar.ENDERECO_CODIGO and conteudo == blob
print(f'2c. .text ligado para {ligado:#x}, gravado em {montar.ENDERECO_CODIGO:#x}, mesmo conteudo: '
      + ('ok' if ok else 'FALHOU'))
if not ok:
    erros.append(f'codigo ligado para {ligado:#x} e gravado em {montar.ENDERECO_CODIGO:#x}')

# 2d. de ponta a ponta: todo call/goto do app.bin remendado que vai para o codigo novo tem de cair
# no inicio de uma funcao nossa, e os bytes que estao LA na flash, decifrados, nas 6 imagens, tem de
# ser os do ELF. E a conferencia que teria pego a v20 sozinha. Para o .vozes, so dos sitios que o
# montar.py poe la: os do espectro. Para o .livre, os do envio e da limpeza da tela principal e as
# entradas das treze rotinas da tela B.
# Contam os desvios que nos gravamos (bytes diferentes dos de fabrica), fora do proprio .livre - as
# chamadas de dentro do codigo novo sao a 4g. Os 4 bytes lidos em cada
# deslocamento par tambem casam com o fim de uma instrucao e o comeco da seguinte - 0x01E16A12, o
# meio da chamada em 0x01E16A10, "vai" para 0x01E79878 -, e nenhum desvio de fabrica de verdade
# leva ao codigo novo: o fim da imagem era espaco livre, no espaco das vozes so caem os
# desvios do fim do app.bin, que e a copia do que roda na RAM e os dados iniciais (secao 15), e no
# G.729 nada de fora entra (secao 16; a 2h confere de novo).
funcoes = {a: n for n, a in montar.le_simbolos(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'simbolos.txt')).items()}
sitios_vozes = ({montar.ESPECTRO_EM} | {s for s, _, _, _ in montar.ESPECTRO_CHAMADAS}
                | {e for e, _, _, _ in montar.ESPECTRO_FUNCOES})
sitios_livre = ({s for s, _ in montar.REALCE_SITIOS_GANCHO} | {s for s, _ in montar.LIMPA_SITIOS} | {s_ for s_, _, _ in montar.ENVIA_SITIOS}
               | {e for e, _, _, _, _ in montar.TELA_DESVIOS} | {s for s, _, _, _ in montar.ESPECTRO_B_CHAMADAS}
               | {e for e, _, _, _ in montar.ESPECTRO_B_FUNCOES}
               | {s for s, _, _, _ in montar.CHAMADAS_LIVRE + montar.GOTOS_LIVRE + montar.CHAMADAS_NO_LUGAR})
imagens = []
for e in entradas:
    if e['type'] in jlfw.FLASH_TYPES:
        img = dados[e['offset']:e['offset'] + e['size']]
        imagens.append((e['name'], img, jlfw.app_dir(img, chave)[1]))
desvios_novos, ruins = 0, []
for o in range(0, len(app) - 3, 2):
    if (app[o + 1] != 0xEA or not app[o] & 0x80 or app[o:o + 4] == app_velho[o:o + 4]
            or na_livre(montar.BASE_APP + o)):
        continue
    d22 = ((app[o] & 0x3F) << 16) | (app[o + 3] << 8) | app[o + 2]
    destino = montar.BASE_APP + o + 4 + 2 * (d22 - 0x400000 if d22 & 0x200000 else d22)
    if no_texto(destino):
        secao, inicio = conteudo, ligado
    elif nas_vozes(destino) or na_livre(destino):
        secao, inicio, sitios, nome_secao = ((vozes, vozes_em, sitios_vozes, '.vozes') if nas_vozes(destino)
                                             else (tela, livre_em, sitios_livre, '.livre'))
        if montar.BASE_APP + o not in sitios:
            ruins.append(f'{montar.BASE_APP + o:#x} -> {destino:#x} (vai para o {nome_secao} de um sitio que o '
                         'montar nao poe la)')
    else:
        continue
    desvios_novos += 1
    if destino not in funcoes:
        ruins.append(f'{montar.BASE_APP + o:#x} -> {destino:#x} (nao e inicio de funcao)')
        continue
    for nome_img, img, base in imagens:
        pos = destino - montar.BASE_APP + base + 0x100
        if jlfw.sfc(img[pos:pos + 16], chave, pos - base) != secao[destino - inicio:destino - inicio + 16]:
            ruins.append(f'{montar.BASE_APP + o:#x} -> {funcoes[destino]}: bytes diferentes em {nome_img}')
print(f'2d. {desvios_novos} desvios do firmware para o codigo novo, destino conferido na flash das '
      f'{len(imagens)} imagens, os que vao para o .vozes e o .livre so dos sitios previstos: '
      + ('ok' if not ruins else 'FALHOU'))
for r in ruins:
    print('    ' + r)
    erros.append('desvio para o codigo novo errado: ' + r)

# 2e. o app.bin inteiro, decifrado, e o mesmo nas 6 imagens (no de fabrica tambem e): o que se
# confere daqui em diante no app da imagem tipo 0 - remendos do roger e do scramble, textos - vale
# para todas.
diferentes = []
for e in entradas:
    if e['type'] in jlfw.FLASH_TYPES:
        img = dados[e['offset']:e['offset'] + e['size']]
        _, base, sub = jlfw.app_dir(img, chave)
        ent = next(s for s in sub if s['name'] == 'app.bin')
        if jlfw.sfc(img[base + ent['offset']:base + ent['offset'] + ent['size']], chave, ent['offset']) != app:
            diferentes.append(e['name'])
print(f'2e. app.bin decifrado igual nas {len(imagens)} imagens: '
      + ('ok' if not diferentes else 'FALHOU em ' + ', '.join(diferentes)))
if diferentes:
    erros.append('app.bin diferente em ' + ', '.join(diferentes))

# 2f. cada imagem do tamanho de fabrica, com o uboot (tudo antes do diretorio do app) e a tabela de
# particoes iguais aos de fabrica (menos os CRCs do app.bin, que mudam com ele), e os ultimos 32 bytes
# apagados: ali o uboot guarda o marcador BOOT_START_FIRST. A v28 cresceu as imagens e entrou em
# bootloop (ver o bancos.ld): o tamanho e a tabela de particoes ficam como a ferramenta da JieLi fez.
problemas = []
for e in entradas:
    if e['type'] not in jlfw.FLASH_TYPES:
        continue
    v = next(x for x in entradas_velhas if x['name'] == e['name'])
    img = dados[e['offset']:e['offset'] + e['size']]
    velha = dados_velhos[v['offset']:v['offset'] + v['size']]
    if len(img) != len(velha):
        problemas.append(f"{e['name']}: {len(img):#x} B, a de fabrica tem {len(velha):#x}")
        continue
    _, base, sub = jlfw.app_dir(img, chave)
    _, base_velha, sub_velha = jlfw.app_dir(velha, chave)
    if base != base_velha or img[:base] != velha[:base]:
        problemas.append(f"{e['name']}: uboot ou cabecalho diferente")
    if ({s['name']: (s['offset'], s['size'], s['attr']) for s in sub if s['name'] not in ('app.bin', 'app_area_head')}
            != {s['name']: (s['offset'], s['size'], s['attr']) for s in sub_velha
                if s['name'] not in ('app.bin', 'app_area_head')}):
        problemas.append(f"{e['name']}: tabela de particoes diferente")
    if any(b != 0xFF for b in img[-32:]):
        problemas.append(f"{e['name']}: os ultimos 32 bytes nao estao apagados")
print(f'2f. {len(imagens)} imagens do tamanho de fabrica, uboot e particoes intactos, ultimos 32 B '
      'apagados: ' + ('ok' if not problemas else 'FALHOU'))
for p_ in problemas:
    print('    ' + p_)
    erros.append(p_)

# 2g. vozes fora: os tres remendos (de fabrica e novos), a tabela de caminhos lida num lugar so (o
# remendado), e em cada imagem a regiao das vozes dentro dos arquivos C0..E67 do proprio diretorio
# "tone", com o bloco .vozes gravado e decifrado certo, e o resto do diretorio (index.idx, bipes,
# entradas) igual ao de fabrica.
problemas = []
for sitio, antes, depois, oque in montar.VOZES_REMENDOS:
    o = sitio - montar.BASE_APP
    if bytes(app_velho[o:o + len(antes)]) != antes or bytes(app[o:o + len(antes)]) != depois:
        problemas.append(f'remendo das vozes em {sitio:#x} ({oque})')
leituras = [o for o in range(0, len(app_velho) - 5, 2)
            if app_velho[o + 1] == 0xFF and app_velho[o] & 0xF0 == 0xC0
            and struct.unpack_from('<I', app_velho, o + 2)[0] == 0x01E5E8FC]
if [montar.BASE_APP + o for o in leituras] != [0x01E40634]:
    problemas.append('a tabela de caminhos dos sons e lida fora de 0x01E40634: '
                     + ', '.join(hex(montar.BASE_APP + o) for o in leituras))
for e in entradas:
    if e['type'] not in jlfw.FLASH_TYPES:
        continue
    v = next(x for x in entradas_velhas if x['name'] == e['name'])
    img = dados[e['offset']:e['offset'] + e['size']]
    velha = dados_velhos[v['offset']:v['offset'] + v['size']]
    _, base, sub = jlfw.app_dir(img, chave)
    a = next(s for s in sub if s['name'] == 'app.bin')
    tone = base + a['offset'] + a['size']                       # o diretorio vem logo depois do app.bin
    cab = jlfw.jlfs_entry(jlfw.sfc(img[tone:tone + 32], chave, tone - base))
    arquivos = []
    p = tone + 32
    while True:
        x = jlfw.jlfs_entry(jlfw.sfc(img[p:p + 32], chave, p - base))
        if not x['ok']:
            break
        arquivos.append(x)
        p += 32
    vozes_arq = [x for x in arquivos if x['name'][:1] in 'CE' and x['name'][1:2].isdigit()]
    ini = tone + min(x['offset'] for x in vozes_arq)
    fim = tone + max(x['offset'] + x['size'] for x in vozes_arq)
    em_ini = montar.VOZES - montar.BASE_APP + base + 0x100
    em_fim = montar.VOZES_FIM - montar.BASE_APP + base + 0x100
    if cab['name'] != 'tone' or len(vozes_arq) != 118 or not (ini <= em_ini and em_fim <= fim):
        problemas.append(f"{e['name']}: regiao {em_ini:#x}..{em_fim:#x} fora das vozes {ini:#x}..{fim:#x}")
    if jlfw.sfc(img[em_ini:em_ini + len(vozes)], chave, em_ini - base) != vozes:
        problemas.append(f"{e['name']}: bloco .vozes ausente ou diferente")
    # os dados dos arquivos fora do bloco .vozes iguais aos de fabrica; nas entradas, tudo menos os
    # CRCs (offset, tamanho, atributo, nome); e todo CRC do diretorio conferindo com o conteudo, para
    # a atualizacao regravar o que mudou
    _, cab_v, arq_v = jlfw.tone_dir(velha, chave)
    _, cab_n, arq_n = jlfw.tone_dir(img, chave)
    fim_dir = tone + cab['size']
    fim_ent = tone + 32 + 32 * len(arq_n)
    if (img[fim_ent:em_ini] != velha[fim_ent:em_ini]
            or img[em_ini + len(vozes):fim_dir] != velha[em_ini + len(vozes):fim_dir]):
        problemas.append(f"{e['name']}: dados do diretorio tone mudaram fora do bloco .vozes")
    chaves = ('offset', 'size', 'attr', 'name')
    if ([tuple(x[k] for k in chaves) for x in arq_n] != [tuple(x[k] for k in chaves) for x in arq_v]
            or tuple(cab_n[k] for k in chaves) != tuple(cab_v[k] for k in chaves)):
        problemas.append(f"{e['name']}: entradas do diretorio tone mudaram alem dos CRCs")
    ruins = [x['name'] for x in arq_n
             if jlfw.crc16(jlfw.sfc(img[tone + x['offset']:tone + x['offset'] + x['size']], chave,
                                    tone + x['offset'] - base)) != x['crc']]
    if ruins or jlfw.crc16(jlfw.sfc(img[tone + 32:fim_dir], chave, tone + 32 - base)) != cab_n['crc']:
        problemas.append(f"{e['name']}: CRC que nao confere no diretorio tone: {ruins or 'o do diretorio'}")
    mudaram = [x['name'] for x, y in zip(arq_n, arq_v) if x['crc'] != y['crc']]
if not set(mudaram) <= {'C0.wtg', 'C1.wtg'}:
    problemas.append(f'mudam arquivos de voz alem do C0 e do C1 ({", ".join(mudaram)}): o carregador recusa')
print(f'2g. vozes fora: {len(montar.VOZES_REMENDOS)} remendos, tabela lida so no remendado, bloco .vozes '
      f'({len(vozes)} B) dentro das vozes nas {len(imagens)} imagens, CRCs do diretorio tone conferindo, '
      f'mudando so o C0 e o C1 (mudaram: {", ".join(mudaram)} e o do diretorio): '
      + ('ok' if not problemas else 'FALHOU'))
for p_ in problemas:
    print('    ' + p_)
    erros.append(p_)

# 2h. a tela B no lugar dos decodificadores G.729 e MP3 (v36 e v37). O registro dos decodificadores
# (3 de 68 B) muda so no tipo dos dois, que vai a 0; o tela.o esta gravado no comeco da faixa; e a
# faixa e fechada no firmware de fabrica e no novo: nenhum valor de 32 bits (ponteiro ou imediato) de
# fora dela e dos dois registros cai dentro, nem call/goto de 4 bytes - os da flash pelo endereco
# dela, os da copia que roda na RAM pelo endereco de la (0x02000000). A unica excecao e uma instrucao
# cujos bytes, lidos como 32 bits, caem na faixa (montar.CODIFICACOES_FORA), e ela so vale com os bytes
# exatos. A prova inteira, com o desmontado (os desvios curtos e condicionais, as tabelas de casos, o
# alcance), esta em docs/engenharia-reversa.md, secao 16; isto aqui confere que nada mudou nela e que
# o nosso codigo nao abriu porta nova.
problemas = []
for reg, tipo, entradas_dec, oque in montar.DECODIFICADORES:
    o = reg - montar.BASE_APP
    if (struct.unpack_from('<5I', app_velho, o) != (tipo,) + entradas_dec or bytes(app[o:o + 4]) != bytes(4)
            or app[o + 4:o + montar.TAM_REGISTRO] != app_velho[o + 4:o + montar.TAM_REGISTRO]):
        problemas.append(f'registro do {oque} em {reg:#x} diferente do previsto')
o = montar.LIVRE - montar.BASE_APP
if bytes(app[o:o + len(tela_gravada)]) != tela_gravada:
    problemas.append('tela B ausente do app.bin no lugar dos decodificadores')
excecoes = {}
for endereco, codificacao, oque in montar.CODIFICACOES_FORA:
    o = endereco - montar.BASE_APP
    if bytes(app_velho[o:o + 4]) != codificacao or bytes(app[o:o + 4]) != codificacao:
        problemas.append(f'{endereco:#x} nao e mais {oque} ({codificacao.hex()})')
    excecoes[endereco] = codificacao
SEG2_OFF, RAM = 0x6AC0C, 0x02000000      # o que roda na RAM: app.bin + 0x6AC0C em diante, la em 0x02000000
dentro = lambda a: montar.LIVRE <= a < montar.LIVRE_FIM
registro = lambda a: any(r <= a < r + montar.TAM_REGISTRO for r, _, _, _ in montar.DECODIFICADORES)
for nome_app, binario in (('de fabrica', app_velho), ('novo', app)):
    for o in range(0, len(binario) - 3, 2):
        onde = montar.BASE_APP + o
        if dentro(onde) or registro(onde):
            continue
        v = struct.unpack_from('<I', binario, o)[0]
        # v43: o ponteiro da tabela do menu ("r5 = menu_tabela", os 32 bits em PONTEIRO_EM + 2) e a unica
        # porta nova, e de dado: o firmware le a tabela, nao executa
        if (binario is app and montar.MENU_NOVO and onde == montar.PONTEIRO_EM + 2
                and v == montar.S[montar.TABELA_SIMBOLO]):
            continue
        if dentro(v & ~1) and excecoes.get(onde) != bytes(binario[o:o + 4]):
            problemas.append(f'{nome_app}: valor {v:#x} em {onde:#x} aponta para dentro da faixa')
        if binario[o + 1] == 0xEA and binario[o] & 0x80:
            d22 = ((binario[o] & 0x3F) << 16) | (binario[o + 3] << 8) | binario[o + 2]
            pc = (onde if o < SEG2_OFF else RAM + o - SEG2_OFF) + 4
            if dentro(pc + 2 * (d22 - 0x400000 if d22 & 0x200000 else d22)) and onde not in sitios_livre:
                problemas.append(f'{nome_app}: call/goto em {onde:#x} cai dentro da faixa')
print(f'2h. tela B ({len(tela)} B) em {montar.LIVRE:#x}, no lugar do G.729 e do MP3 (ate '
      f'{montar.LIVRE_FIM:#x}); tipo dos dois no registro de fabrica para 0, o resto dos registros intacto; '
      f'nada de fora aponta para a faixa, nem no de fabrica nem no novo (fora os sitios da tela B e '
      f'{len(excecoes)} instrucao conferida byte a byte): ' + ('ok' if not problemas else 'FALHOU'))
for p_ in problemas[:20]:
    print('    ' + p_)
erros += problemas

# 2i. o carimbo do fim (v47): o carimbo_fim tem o CRC16 do .text gravado no fim da imagem e mora dentro
# do app.bin, que o CRC do app_area_head cobre. O carregador de atualizacao so regrava o que tem CRC
# novo, e nenhum CRC de dentro da imagem cobre o fim: sem o carimbo, a versao que so muda o fim e
# recusada (a v46, por cima da v45, parou nos 5% como a v34 gravada por cima dela mesma).
simbolos = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'simbolos.txt')
carimbo = next((int(p[0], 16) for p in map(str.split, open(simbolos)) if p[-1:] == ['carimbo_fim']), None)
o = None if carimbo is None else carimbo - montar.BASE_APP
ok = (o is not None and carimbo % 2 == 0 and montar.LIVRE <= carimbo < montar.LIVRE_FIM
      and 0 <= o and o + 2 <= len(app) and struct.unpack_from('<H', app, o)[0] == jlfw.crc16(blob))
print(f'2i. carimbo do fim em {carimbo or 0:#x}, dentro do app.bin ({len(app)} B): '
      f'crc16 do .text = {jlfw.crc16(blob):#06x}: ' + ('ok' if ok else 'FALHOU'))
if not ok:
    erros.append('carimbo do fim ausente, fora do app.bin ou diferente do CRC16 do .text')

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
    molde = velha[quem['molde'] * T:(quem['molde'] + 1) * T]
    nomes = ('get', 'set', 'nome', 'nome_cn', 'exibe', 'opcoes_cn')
    campos = {c: struct.unpack_from('<I', tabela_esperada, o + montar.CAMPO[c])[0] for c in nomes}
    esperado = {'get': montar.S[quem['get']], 'set': montar.S[quem['set']],
                'nome': montar.S[quem['titulo']], 'nome_cn': montar.S[quem['titulo']]}
    # exibir e opcoes em chines: as nossas, ou as do molde (o MDF-C usa as do MDF-A)
    for c in ('exibe', 'opcoes_cn'):
        esperado[c] = montar.S[quem[c]] if quem.get(c) else struct.unpack_from('<I', molde, montar.CAMPO[c])[0]
    flags_esperadas = molde[0] if quem['flags'] is None else quem['flags']
    bate = flags == flags_esperadas and maximo == quem['maximo'] and campos == esperado
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

# 4r. TRIPLE (v42, v44): a ida e a volta da dupla escuta e a tecla A/B passam pelos ganchos da C; e
# (v49) a chamada que apaga a luz, pelo gancho que a deixa acesa no espectro
previsto_tripla = []
for sitio, era, gancho, oque in montar.CHAMADAS_LIVRE:
    o = sitio - montar.BASE_APP
    ok = (bytes(app_velho[o:o + 4]) == montar.monta_call(sitio, era)
          and bytes(app[o:o + 4]) == montar.monta_call(sitio, montar.S[gancho]))
    print(f'4r. {sitio:#x}: {era:#x} -> {gancho} ({oque}): ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append(f'gancho errado em {sitio:#x} ({oque})')
    previsto_tripla.append((sitio, sitio + 3, oque))
for sitio, era, gancho, oque in montar.GOTOS_LIVRE:
    o = sitio - montar.BASE_APP
    ok = (bytes(app_velho[o:o + 4]) == montar.monta_goto(sitio, era)
          and bytes(app[o:o + 4]) == montar.monta_goto(sitio, montar.S[gancho]))
    print(f'4r. {sitio:#x}: goto {era:#x} -> {gancho} ({oque}): ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append(f'goto errado em {sitio:#x} ({oque})')
    previsto_tripla.append((sitio, sitio + 3, oque))
for sitio, era, gancho, oque in montar.CHAMADAS_NO_LUGAR:   # v50: instrucoes trocadas por chamada + nop
    o = sitio - montar.BASE_APP
    ok = (bytes(app_velho[o:o + len(era)]) == era
          and bytes(app[o:o + len(era)]) == montar.monta_call(sitio, montar.S[gancho]) + montar.NOP[:len(era) - 4])
    print(f'4r. {sitio:#x}: {era.hex()} -> call {gancho} + nop ({oque}): ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append(f'chamada no lugar errada em {sitio:#x} ({oque})')
    previsto_tripla.append((sitio, sitio + len(era) - 1, oque))

if montar.SCAN_REDESENHA:
    o = montar.SCAN_EM - montar.BASE_APP
    ok = bytes(app[o:o + 4]) == montar.monta_call(montar.SCAN_EM, montar.S['gancho_scan'])
    print(f'4a. gancho do scan em {montar.SCAN_EM:#x}: ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append('gancho do scan errado')

o = montar.PONTEIRO_EM - montar.BASE_APP
atual = bytes(app[o:o + len(montar.PONTEIRO_ANTES)])
esperado = montar.monta_ponteiro() if montar.MENU_NOVO else montar.PONTEIRO_ANTES
ok = bytes(app_velho[o:o + len(montar.PONTEIRO_ANTES)]) == montar.PONTEIRO_ANTES and atual == esperado
print(f'4b. ponteiro da tabela em {montar.PONTEIRO_EM:#x}: de fabrica "r0 = 13368; r5 = r6 + r0", agora '
      f'"r5 = {montar.S[montar.TABELA_SIMBOLO]:#x}" (menu_tabela, no .livre): ' + ('ok' if ok else f'FALHOU ({atual.hex()})'))
if not ok:
    erros.append('ponteiro da tabela errado')

# 5. nada escrito fora dos trechos previstos - e nada em area protegida
previsto = ([(montar.PONTEIRO_EM, montar.PONTEIRO_EM + len(montar.PONTEIRO_ANTES) - 1, 'ponteiro da tabela'),
             (montar.CONTAGEM_EM, montar.CONTAGEM_EM + len(montar.CONTAGEM_ANTES) - 1, 'contagem')]
            if montar.MENU_NOVO else [])
previsto += [(e, e + len(v), f'texto {v}') for e, v, _ in (montar.TEXTOS if montar.TEXTOS_REVISADOS else ())]
previsto += [(sitio, sitio + 3, 'setas') for sitio, _ in (montar.SETAS if montar.SETAS_INVERTIDAS else ())]
previsto += [(montar.GANCHO_EM, montar.GANCHO_EM + 3, 'gancho')]
if montar.SCAN_REDESENHA:
    previsto += [(montar.SCAN_EM, montar.SCAN_EM + 3, 'gancho do scan')]
previsto += previsto_tripla
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
    # v41, o espectro na tela B: as chamadas e as rotinas trocadas que vao para o .livre.
    for sitio, era, gancho, oque in montar.ESPECTRO_B_CHAMADAS:
        o = sitio - montar.BASE_APP
        ok = (bytes(app_velho[o:o + 4]) == montar.monta_call(sitio, era)
              and bytes(app[o:o + 4]) == montar.monta_call(sitio, montar.S[gancho]))
        print(f'4h. espectro B {sitio:#x}: {era:#x} -> {gancho} ({oque}): ' + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'chamada do espectro B errada em {sitio:#x}')
    for entrada, antes, nova, oque in montar.ESPECTRO_B_FUNCOES:
        o = entrada - montar.BASE_APP
        ok = (bytes(app_velho[o:o + 4]) == antes
              and bytes(app[o:o + 4]) == montar.monta_goto(entrada, montar.S[nova]))
        print(f'4h. espectro B {entrada:#x} trocada por {nova} ({oque}): ' + ('ok' if ok else 'FALHOU'))
        if not ok:
            erros.append(f'rotina do espectro B mal trocada em {entrada:#x}')
    previsto += [(s, s + 3, 'espectro B') for s, _, _, _ in montar.ESPECTRO_B_CHAMADAS]
    previsto += [(e, e + 3, 'espectro B') for e, _, _, _ in montar.ESPECTRO_B_FUNCOES]
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
                                  ('CFG_FILTRO', 0x01E4A99C, 16), ('ESP_PEDACO', 0x01E4B422, 23)):
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
# porque o destino dela nao se confere. E nada do .text nem do .livre chama o .vozes: o que roda na
# partida, no cabo, nos menus e na tela principal nunca chega ao espaco que era das vozes. Os desvios
# da tela B voltam para dentro da rotina de fabrica, logo depois dos bytes cobertos; esse goto e
# conferido na 4m.
voltas = {e + len(c) for e, c, _, _, _ in montar.TELA_DESVIOS}
usadas, estranhas = {}, []
desmontado = open(os.path.join(os.path.dirname(codigo), 'bancos.dis'), encoding='utf-8',
                  errors='replace').read().splitlines()


def faixas_de_codigo(elf):
    """[inicio, fim) de cada funcao do ELF (simbolo FUNC com tamanho, da .symtab). O desmontado le
    tudo como instrucao, e a .rodata do tela.o fica intercalada com o codigo na faixa .livre (o
    linker poe as secoes na ordem do arquivo): uma cadeia como "\\x85\\x86\\x87" sai como um goto."""
    e = open(elf, 'rb').read()
    shoff = struct.unpack_from('<I', e, 0x20)[0]
    tam_sh, n_sh, _ = struct.unpack_from('<HHH', e, 0x2E)
    faixas = []
    for i in range(n_sh):
        sh = struct.unpack_from('<10I', e, shoff + i * tam_sh)
        if sh[1] != 2:                                  # SHT_SYMTAB
            continue
        for o in range(sh[4], sh[4] + sh[5], sh[9]):
            _, valor, tamanho, info, _, _ = struct.unpack_from('<IIIBBH', e, o)
            if info & 0xF == 2 and tamanho:              # STT_FUNC
                faixas.append((valor, valor + tamanho))
    return faixas


# o codigo: as funcoes em C e os desvios em assembly da tela B (sem tamanho no ELF; a 4m os confere
# byte a byte: a ida, o caminho de fabrica com os bytes cobertos e o goto de volta)
codigo_em = faixas_de_codigo(os.path.splitext(codigo)[0] + '.elf')
codigo_em += [(montar.S[d], montar.S[d + '_fabrica'] + 4 + len(c) + 4) for _, c, d, _, _ in montar.TELA_DESVIOS]
codigo_em.append((montar.S[montar.AJUSTES_FABRICA[0]], montar.S[montar.AJUSTES_FABRICA[0]] + 8))
for linha in desmontado:
    m = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(call|goto)\s+(\S+)', linha)
    if not m:
        continue
    onde, tam, alvo = int(m.group(1), 16), len(m.group(2).split()), m.group(4)
    if not any(a <= onde < b for a, b in codigo_em):
        continue                                        # dado (.rodata) lido como instrucao
    if not re.fullmatch(r'-?\d+', alvo):
        estranhas.append(f'{onde:#x} {m.group(3)} {alvo} (por registrador)')
        continue
    destino = onde + tam + int(alvo)
    if not nas_vozes(onde) and nas_vozes(destino):
        estranhas.append(f'{onde:#x} {m.group(3)} -> {destino:#x} (de fora do .vozes para o .vozes)')
        continue
    if no_texto(destino) or nas_vozes(destino) or na_livre(destino) or (na_livre(onde) and destino in voltas):
        continue
    if destino in declaradas:
        usadas[declaradas[destino]] = usadas.get(declaradas[destino], 0) + 1
    else:
        estranhas.append(f'{onde:#x} {m.group(3)} -> {destino:#x} (fora do codigo e nao declarado)')
print(f'4g. chamadas ao firmware: {sum(usadas.values())}, todas para enderecos declarados '
      f'({len(usadas)} rotinas), nenhuma de fora do .vozes para ele: ' + ('ok' if not estranhas else 'FALHOU'))
for e in estranhas:
    print('    ' + e)
    erros.append('chamada com destino nao conferido: ' + e)

# 4m. a tela B. Cada uma das treze entradas tem de fabrica os bytes que o montar cobre e, remendada,
# um goto para o desvio dela. E o desvio, no .livre, e exatamente: guarda rets e r0..r3 (10 04 60 04,
# as formas de 0x01E3C290 e 0x01E51358), chama o gancho em C, desvia para o caminho de fabrica se ele
# devolver 0 (00 43: "if (r0 == 0) goto" 6 adiante), senao restaura (40 04 88 04) e volta (80 00). O
# caminho de fabrica restaura, refaz os bytes cobertos, iguais aos de fabrica, e desvia de volta para
# logo depois deles na rotina.
problemas = []
for entrada, cobertos, desvio, gancho, oque in montar.TELA_DESVIOS:
    o, d = entrada - montar.BASE_APP, montar.S[desvio]
    v, fab = d - livre_em, montar.S[desvio + '_fabrica'] - livre_em
    volta = montar.S[desvio + '_fabrica'] + 4 + len(cobertos)
    certo = (bytes(app_velho[o:o + len(cobertos)]) == cobertos
             and bytes(app[o:o + 4]) == montar.monta_goto(entrada, d) and na_livre(d)
             and tela[v:v + 4] == bytes.fromhex('10046004')
             and tela[v + 4:v + 8] == montar.monta_call(d + 4, montar.S[gancho])
             and tela[v + 8:v + 16] == bytes.fromhex('0043400488048000') and fab == v + 16
             and tela[fab:fab + 4] == bytes.fromhex('40048804')
             and tela[fab + 4:fab + 4 + len(cobertos)] == cobertos
             and tela[fab + 4 + len(cobertos):fab + 8 + len(cobertos)] == montar.monta_goto(volta, entrada + len(cobertos)))
    if not certo:
        problemas.append(f'{entrada:#x} ({oque}) -> {desvio} -> {gancho}')
print(f'4m. tela B: {len(montar.TELA_DESVIOS)} desvios de entrada, bytes de fabrica, goto, desvio e volta '
      'conferidos byte a byte: ' + ('ok' if not problemas else 'FALHOU'))
for p_ in problemas:
    print('    ' + p_)
    erros.append('desvio da tela B errado: ' + p_)
# Os ajustes do radio de fabrica chamados como rotina (radio_ajustes_fabrica): exatamente os bytes que
# o desvio dela cobre e um goto para logo depois deles. E as chamadas trocadas (ENVIA_SITIOS), todas
# antes chamadas do envia() de fabrica, e as que viram nop (NOP_SITIOS).
nome_fab, entrada_fab = montar.AJUSTES_FABRICA
cobertos_fab = next(c for e, c, _, _, _ in montar.TELA_DESVIOS if e == entrada_fab)
c = montar.S[nome_fab]
v = c - livre_em
certo = (na_livre(c) and tela[v:v + len(cobertos_fab)] == cobertos_fab
         and tela[v + len(cobertos_fab):v + len(cobertos_fab) + 4]
         == montar.monta_goto(c + len(cobertos_fab), entrada_fab + len(cobertos_fab)))
for sitio, gancho, _ in montar.ENVIA_SITIOS:
    o = sitio - montar.BASE_APP
    certo = (certo and bytes(app_velho[o:o + 4]) == montar.monta_call(sitio, montar.ENVIA_ERA)
             and bytes(app[o:o + 4]) == montar.monta_call(sitio, montar.S[gancho]))
for sitio, _ in montar.NOP_SITIOS:
    o = sitio - montar.BASE_APP
    certo = (certo and bytes(app_velho[o:o + 4]) == montar.monta_call(sitio, montar.ENVIA_ERA)
             and bytes(app[o:o + 4]) == montar.NOP)
print(f'4m. {nome_fab}: os {len(cobertos_fab)} bytes de fabrica e o goto para {entrada_fab + len(cobertos_fab):#x}; '
      f'{len(montar.ENVIA_SITIOS)} chamadas do envia() trocadas pelos ganchos e {len(montar.NOP_SITIOS)} por nop: '
      + ('ok' if certo else 'FALHOU'))
if not certo:
    erros.append('radio_ajustes_fabrica ou chamada trocada errados')
previsto_tela = [(e, e + 3, 'tela B') for e, _, _, _, _ in montar.TELA_DESVIOS]
previsto_tela += [(s_, s_ + 3, o_) for s_, _, o_ in montar.ENVIA_SITIOS]
previsto_tela += [(s_, s_ + 3, o_) for s_, o_ in montar.NOP_SITIOS]

# 4q. as esperas da troca de banda do radio (v40): os 4 "r0 = 750" de fabrica viram "r0 = 125".
ok = all(bytes(app_velho[e - montar.BASE_APP:e - montar.BASE_APP + 4]) == montar.ESPERA_ANTES
         and bytes(app[e - montar.BASE_APP:e - montar.BASE_APP + 4]) == montar.ESPERA_NOVA
         for e in montar.ESPERAS_RADIO)
print(f'4q. as {len(montar.ESPERAS_RADIO)} esperas da troca de banda do radio: de 1,5 s para 0,25 s: '
      + ('ok' if ok else 'FALHOU'))
if not ok:
    erros.append('esperas da troca de banda do radio erradas')
previsto_tela += [(e, e + 3, 'espera da troca de banda') for e in montar.ESPERAS_RADIO]

# 4n. o codigo novo nao tem dados em RAM (.data, .bss): nao ha RAM nossa, e o bancos.ld so poe codigo e
# tabelas na flash. Uma variavel global sem const viraria uma dessas secoes e cairia num lugar qualquer.
elf_bytes = open(os.path.splitext(codigo)[0] + '.elf', 'rb').read()
shoff = struct.unpack_from('<I', elf_bytes, 0x20)[0]
tam_sh, n_sh, i_nomes = struct.unpack_from('<HHH', elf_bytes, 0x2E)
cab = [struct.unpack_from('<10I', elf_bytes, shoff + i * tam_sh) for i in range(n_sh)]
nome_de = lambda i: elf_bytes[cab[i_nomes][4] + i:elf_bytes.index(b'\0', cab[i_nomes][4] + i)].decode()
em_ram = [nome_de(c[0]) for c in cab if nome_de(c[0]).startswith(('.data', '.bss', '.sdata', '.sbss')) and c[5]]
print('4n. nenhuma secao de dados em RAM no ELF: ' + ('ok' if not em_ram else 'FALHOU ' + ', '.join(em_ram)))
if em_ram:
    erros.append('secao em RAM no codigo novo: ' + ', '.join(em_ram))

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


def campo_do_item(get, campo):
    """Um campo de 32 bits do item da tabela nova cujo get e a rotina nossa dada."""
    for o in range(0, len(tabela_esperada), montar.TAM_ITEM):
        if struct.unpack_from('<I', tabela_esperada, o + montar.CAMPO['get'])[0] == montar.S[get]:
            return struct.unpack_from('<I', tabela_esperada, o + montar.CAMPO[campo])[0]


# 4o. sem o chines (v37). O IDIOMA que o gancho da partida zera e o byte que o get do item LANGUAGE de
# fabrica le (0x01E3C9A8: "r0 = 1268; r1 = 83584; r0 = b[r1+r0]"), e o item saiu da tabela (ITENS_FORA).
idioma_get = imediato(0x01E3C9AC) + imediato16(0x01E3C9A8)
ok = (define('IDIOMA') == idioma_get and montar.LANGUAGE_ITEM in montar.ITENS_FORA
      and struct.unpack_from('<I', app_velho, montar.TABELA - montar.BASE_APP
                             + montar.LANGUAGE_ITEM * montar.TAM_ITEM + montar.CAMPO['get'])[0] == 0x01E3C9A8)
print(f"4o. IDIOMA = {define('IDIOMA'):#x}, o get do LANGUAGE le {idioma_get:#x}, e o item "
      f'{montar.LANGUAGE_ITEM} (o LANGUAGE) saiu do menu: ' + ('ok' if ok else 'FALHOU'))
if not ok:
    erros.append('IDIOMA nao e o byte do LANGUAGE, ou o item ficou no menu')


def deslocamento(endereco):
    """O deslocamento de um "rX = b[rY+imm]" (50 ee + 2 bytes: rY no nibble alto do 3o byte, rX no do
    4o, e o deslocamento nos dois nibbles baixos, o do 4o em cima) do firmware de fabrica."""
    o = endereco - montar.BASE_APP
    assert app_velho[o:o + 2] == b'\x50\xee', hex(endereco)
    return (app_velho[o + 3] & 0x0F) << 4 | app_velho[o + 2] & 0x0F


# 4p. os enderecos de RAM do radio (e do editor de nome) no telas.c (v38; na v37 seis sairam errados, lidos sem o indice
# x 2 do desmontado). Cada um e refeito dos bytes de fabrica da rotina que o le: a base carregada em
# 32 bits e o indice de 16 bits (deslocado como no acesso, h[] x 2 e [] x 4) ou o deslocamento do
# proprio acesso. O RADIO_NIVEL e o byte antes do RADIO_SINAL (0x01E451AA: r1 += -1).
radio_ram = [
    ('RADIO_BANDA', imediato(0x01E44FC8) + deslocamento(0x01E44FCE)),
    ('RADIO_MEMORIAS', imediato(0x01E449D0) + deslocamento(0x01E44AD2)),
    ('RADIO_PASSO_AM', imediato(0x01E44CBC) + deslocamento(0x01E44D22)),
    ('RADIO_PASSO_SSB', imediato(0x01E44CBC) + deslocamento(0x01E44D30)),
    ('RADIO_FAIXA_AM', imediato(0x01E45010) + imediato16(0x01E4500C)),
    ('RADIO_FREQ', imediato(0x01E449CA) + (imediato16(0x01E449C6) << 1)),
    ('RADIO_MEMORIA', imediato(0x01E449CA) + imediato16(0x01E44ADE)),
    ('RADIO_SINAL', imediato(0x01E451A0) + imediato16(0x01E4519C)),
    ('RADIO_NIVEL', imediato(0x01E451A0) + imediato16(0x01E4519C) - 1),
    ('RADIO_LARGURA', imediato(0x01E44D6E) + imediato16(0x01E44DC2)),
    ('RADIO_LNA', imediato(0x01E44D6E) + imediato16(0x01E44E4A)),
    ('RADIO_BFO', imediato(0x01E44D6E) + (imediato16(0x01E44F12) << 1)),
    ('RADIO_ITEM', imediato(0x01E49344) + (imediato16(0x01E49340) << 2)),
    ('DIGITA_MODO', imediato(0x01E49094) + imediato16(0x01E4909C)),
    ('EDITOR_MODO', imediato(0x01E48728) + imediato16(0x01E48724)),
    # v42: a dupla escuta, que o set do TDR e o WATCH mantem em 0 no SINGLE: o get do item TDR
    ('TDR', imediato(0x01E39570) + imediato16(0x01E3956C)),
    # v40, a conferencia dos ajustes do radio
    ('RADIO_MEM_FM', imediato(0x01E49186) + deslocamento(0x01E4923A)),
    ('RADIO_MEM_AM', imediato(0x01E49186) + deslocamento(0x01E491A8)),
    ('RADIO_MEM_SSB', imediato(0x01E49186) + deslocamento(0x01E491C8)),
    ('RADIO_LNA_AM', imediato(0x01E3DA1E) + deslocamento(0x01E3DA6A)),
    ('RADIO_LNA_SSB', imediato(0x01E3DCC8) + deslocamento(0x01E3DCCE)),
    # "r3 = r1 + 68" (0x01E3DB42) e "r3 = h[r3+4]" (0x01E3DB4E): o BFO do VFO e o LNA do AM + 4
    ('RADIO_BFO_VFO', imediato(0x01E3DA1E) + deslocamento(0x01E3DA6A) + 4),
]
errados = [f'{n} {define(n):#x} (o firmware le {a:#x})' for n, a in radio_ram if define(n) != a]
print(f'4p. {len(radio_ram)} enderecos de RAM do radio refeitos dos bytes de fabrica: '
      + ('ok' if not errados else 'FALHOU ' + ', '.join(errados)))
erros += ['endereco do radio errado: ' + e for e in errados]

# 4j. roger BEEP 2 e BEEP 3. Os enderecos saem das instrucoes do firmware: a variavel e a que o get do item
# (0x01E3C91E) e a propria rotina do roger (0x01E44606) leem; a tabela do BEEP e a que o laco
# de fabrica percorre (r4 de 0x01E44614 + 2051, dois pares de tras para frente); as rotinas de tom e
# de espera sao as que o laco chama. Os dois remendos: os bytes de fabrica e os novos.
roger_get = imediato(0x01E3C922) + imediato16(0x01E3C91E)
roger_rotina = imediato(0x01E4460A) + imediato16(0x01E44606)
beep_tabela = imediato(0x01E44614) + 2051 - 6
o_laco, o_desvio = montar.ROGER_LACO_EM - montar.BASE_APP, montar.ROGER_DESVIO_EM - montar.BASE_APP
verificacoes = [
    ('ROGER e a variavel do item e da rotina', define('ROGER') == roger_get == roger_rotina),
    ('tabela do BEEP de fabrica', define('BEEP_FABRICA') == beep_tabela),
    ('TOM e a rotina que o laco chamava', bytes(app_velho[o_laco + 0xC:o_laco + 0x10])
     == montar.monta_call(montar.ROGER_LACO_EM + 0xC, declaradas_por_nome('TOM'))),
    ('ESPERA_2MS e a rotina que o laco chamava', bytes(app_velho[o_laco + 0x12:o_laco + 0x16])
     == montar.monta_call(montar.ROGER_LACO_EM + 0x12, declaradas_por_nome('ESPERA_2MS'))),
    ('so o 2 desvia antes, para o TONE1200 ("if (r0 == 2) goto 82")',
     bytes(app_velho[o_desvio - 4:o_desvio]) == bytes.fromhex('00f82904')),
    ('desvio dos valores 3 e 4', bytes(app_velho[o_desvio:o_desvio + 4]) == montar.ROGER_DESVIO_ANTES
     and bytes(app[o_desvio:o_desvio + 4]) == montar.ROGER_DESVIO_DEPOIS),
    ('laco dos tons', bytes(app_velho[o_laco:o_laco + 8]) == montar.ROGER_LACO_ANTES
     and bytes(app[o_laco:o_laco + 8]) == montar.monta_call(montar.ROGER_LACO_EM, montar.S['gancho_roger_tons'])
     + montar.monta_goto(montar.ROGER_LACO_EM + 4, montar.ROGER_LACO_FIM)),
    ('opcoes em chines = as em ingles (sem o chines, v37)',
     campo_do_item('roger_le', 'opcoes_cn') == campo_do_item('roger_le', 'exibe') == montar.S['roger_opcoes']),
]
for oque, ok in verificacoes:
    print(f'4j. roger: {oque}: ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append('roger: ' + oque)
previsto += [(montar.ROGER_DESVIO_EM, montar.ROGER_DESVIO_EM + 3, 'roger: desvio'),
             (montar.ROGER_LACO_EM, montar.ROGER_LACO_EM + 7, 'roger: laco')]

# 4k. scramble de 2700 a 3400 Hz. A conta Hz -> registro sai da 1a entrada da tabela de fabrica
# (a que o r1 de 0x01E40936 apontava), 3400 Hz - a frequencia medida na gravacao do dono -, e a
# tabela de fabrica tem de continuar intacta. Do blob saem a tabela nova, as traducoes posicao <->
# valor e os nomes das opcoes: o menu em ordem crescente de 100 em 100, cada posicao gravando um
# valor que inverte na frequencia do nome, e todo valor de 0 a 15 aparecendo no menu com a
# frequencia em que o radio inverte de verdade.
def do_blob(nome, formato, n):
    return struct.unpack_from('<%d%s' % (n, formato), blob, montar.S[nome] - montar.ENDERECO_CODIGO)


def texto_do_blob(endereco):
    o = endereco - montar.ENDERECO_CODIGO
    return blob[o:blob.index(b'\0', o)].decode() if 0 <= o < len(blob) else None


fabrica = imediato(0x01E40936)
o = fabrica - montar.BASE_APP
hz_por_unidade = 3400 / struct.unpack_from('<H', app_velho, o)[0]
freqs = [round(v * hz_por_unidade) for v in do_blob('scramble_freqs', 'H', 16)]
erro_hz = max(abs(v * hz_por_unidade - f) for v, f in zip(do_blob('scramble_freqs', 'H', 16), freqs))
valor, posicao = do_blob('scramble_valor', 'B', 9), do_blob('scramble_posicao', 'B', 16)
nomes = [texto_do_blob(p) for p in do_blob('scramble_opcoes', 'I', 9)]
menu = [None] + [freqs[valor[p]] for p in range(1, 9)]        # frequencia que cada posicao grava
o_rem = montar.SCRAMBLE_EM - montar.BASE_APP
verificacoes = [
    ('tabela de fabrica intacta', app[o:o + 4] == app_velho[o:o + 4]),
    (f'tabela nova: cada entrada a menos de {erro_hz:.2f} Hz da frequencia redonda', erro_hz < 0.5),
    ('valores 1 = 3400 Hz (ON de fabrica) e 2 = 2900 Hz (v24)', freqs[1] == 3400 and freqs[2] == 2900),
    ('menu de 2700 a 3400 Hz, de 100 em 100: ' + ' '.join(map(str, menu[1:])),
     menu[1:] == list(range(2700, 3401, 100)) and valor[0] == 0),
    ('nomes das opcoes', nomes == ['OFF'] + [f'{f} Hz' for f in menu[1:]]),
    ('opcoes em chines = as em ingles (sem o chines, v37)',
     campo_do_item('scramble_le', 'opcoes_cn') == campo_do_item('scramble_le', 'exibe')
     == montar.S['scramble_opcoes']),
    ('posicao <-> valor: ida e volta', all(posicao[valor[p]] == p for p in range(9))),
    ('valores 0 a 15: o menu mostra a frequencia em que o radio inverte',
     posicao[0] == 0 and all(menu[posicao[v]] == freqs[v] for v in range(1, 16))),
    (f'remendo em {montar.SCRAMBLE_EM:#x}: r0 = r4 & 0xF; r1 = scramble_freqs',
     bytes(app_velho[o_rem:o_rem + 14]) == montar.SCRAMBLE_ANTES
     and bytes(app[o_rem:o_rem + 14]) == montar.monta_scramble()
     and struct.unpack_from('<I', app, o_rem + 10)[0] == montar.S['scramble_freqs']),
]
for oque, ok in verificacoes:
    print(f'4k. scramble: {oque}: ' + ('ok' if ok else 'FALHOU'))
    if not ok:
        erros.append('scramble: ' + oque)
previsto.append((montar.SCRAMBLE_EM, montar.SCRAMBLE_EM + 13, 'scramble: frequencia pelo valor'))
previsto += [(s_, s_ + len(a_) - 1, 'vozes: ' + d_) for s_, a_, _, d_ in montar.VOZES_REMENDOS]
previsto += previsto_tela
previsto += [(montar.LIVRE, montar.LIVRE + len(tela) - 1, 'tela B no lugar do G.729 e do MP3')]
previsto += [(r, r + 3, f'tipo do {d} no registro') for r, _, _, d in montar.DECODIFICADORES]

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
