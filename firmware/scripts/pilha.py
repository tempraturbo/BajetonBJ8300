"""Pilha: o firmware montado contra o de fabrica, nas tres raizes e nos itens do menu.

A tela B e os ganchos rodam no meio das chamadas do firmware, na pilha de quem chama; o caminho mais
fundo deles nao pode passar do que o firmware de fabrica ja usa no mesmo contexto. Este script mede:

 - as tres raizes de onde o codigo novo e chamado: a interface (0x01E4B416, 740 B de fabrica), o laco
   do sinal (0x01E4622A, 620 B) e a partida (696 B);
 - os get, set e exibe de todos os itens do menu (a tabela montada e as duas do radio), que o firmware
   chama por ponteiro e a analise do grafo nao segue: o set no OK (0x01E49996) a 144 B, o get e o
   exibe no motor (0x01E489E0) a 296 B, contra os 740 B da interface.

Grafo de fabrica: pilha_fabrica.py (rotinas, quadros, chamadas). Grafo nosso: build/bancos.dis, com o
destino de cada call/goto calculado do deslocamento; chamada nossa para o firmware vale a
profundidade de fabrica daquela rotina. Substituicoes:
 - desvio na entrada (TELA_DESVIOS): a rotina vale max(nosso desvio, a de fabrica), porque o gancho
   pode devolver 0 e a de fabrica rodar depois (o desvio ja desempilhou);
 - rotina trocada inteira (ESPECTRO_FUNCOES): vale a nossa;
 - chamada trocada (os sitios de gancho): a rotina que a contem ganha uma chamada ao gancho.
E uma estimativa por baixo: chamada por registrador nao entra (por isso os menus vao a parte).

Uso: python -I scripts/pilha.py [--detalhe]   (de dentro de firmware/; o build.sh o roda no fim)
Precisa do build/app.dis (o desmontado de fabrica, que o build.sh gera) e do build/bancos.dis.
Sai com 1 se alguma raiz ou algum item passar do de fabrica.
"""
import io
import os
import re
import runpy
import struct
import sys
from contextlib import redirect_stdout

AQUI = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(AQUI, '..', 'build')
sys.path.insert(0, AQUI)
import montar  # noqa: E402

detalhe = '--detalhe' in sys.argv[1:]
app_dis, nosso_dis = os.path.join(BUILD, 'app.dis'), os.path.join(BUILD, 'bancos.dis')
montar.S.update(montar.le_simbolos(os.path.join(BUILD, 'simbolos.txt')))
sys.argv = ['pilha_fabrica.py', app_dis]
with redirect_stdout(io.StringIO()):
    g = runpy.run_path(os.path.join(AQUI, 'pilha_fabrica.py'))
qf, cf, rotina_de = g['quadro'], g['chamadas'], g['rotina_de']
sys.setrecursionlimit(100000)

# o nosso grafo
LIN = re.compile(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t?(.*)$')
qn, cn, ini, atual, conta = {}, {}, {}, None, {}
for l in open(nosso_dis, encoding='utf-8', errors='replace').read().splitlines():
    m = re.match(r'^([\w.]+):$', l.strip())
    if m:
        atual = m.group(1)
        qn.setdefault(atual, 0)
        cn.setdefault(atual, [])
        continue
    m = LIN.match(l)
    if not m or not atual:
        continue
    if atual == 'radio_ajustes_fabrica' and conta.get(atual, 0) >= 2:    # depois dos 2, e o .rodata
        continue
    conta[atual] = conta.get(atual, 0) + 1
    a, n, t = int(m.group(1), 16), len(m.group(2).split()), m.group(3)
    ini.setdefault(atual, a)
    p = re.search(r'\[--sp\] = \{rets(?:, r(\d+)(?:-r(\d+))?)?\}', t)
    if p:
        qn[atual] += 4 * (1 + (0 if not p.group(1) else int(p.group(1)) - int(p.group(2) or p.group(1)) + 1))
    elif re.search(r'\[--sp\] = rets', t):
        qn[atual] += 4
    elif re.search(r'\[--sp\] = \{r3-r0\}', t):
        qn[atual] += 16
    s = re.search(r'\bsp \+= -(\d+)', t)
    if s:
        qn[atual] += int(s.group(1))
    c = re.search(r'\b(call|goto) (-?\d+)\b', t)
    if c:
        cn[atual].append((a + n + int(c.group(2)), c.group(1)))
por_ini = {a: f for f, a in ini.items()}

DESVIOS = {e: d for e, _, d, _, _ in montar.TELA_DESVIOS}
TROCADAS = {e: nova for e, _, nova, _ in getattr(montar, 'ESPECTRO_FUNCOES', ()) + getattr(montar, 'ESPECTRO_B_FUNCOES', ())}
SITIOS = []                              # (sitio, gancho)
SITIOS.append((montar.GANCHO_EM, 'gancho_partida'))
SITIOS += [(s, 'gancho_cabo') for s, _ in montar.CABO_SITIOS]
SITIOS.append((montar.SCAN_EM, 'gancho_scan'))
SITIOS.append((montar.ESPECTRO_EM, 'gancho_tecla_espectro'))
SITIOS += [(s, gn) for s, _, gn, _ in montar.ESPECTRO_CHAMADAS]
SITIOS += [(s, gn) for s, _, gn, _ in getattr(montar, 'ESPECTRO_B_CHAMADAS', ())]
if hasattr(montar, 'DW_TROCA_EM'):
    SITIOS += [(s_, g_) for s_, _, g_, _ in getattr(montar, "CHAMADAS_LIVRE", montar.TRIPLE_CHAMADAS) + getattr(montar, "GOTOS_LIVRE", ()) + getattr(montar, "CHAMADAS_NO_LUGAR", ())]
SITIOS.append((montar.ROGER_LACO_EM, 'gancho_roger_tons'))
SITIOS += [(s, 'gancho_envia') for s, _ in montar.REALCE_SITIOS_GANCHO]
SITIOS += [(s, 'gancho_limpa') for s, _ in montar.LIMPA_SITIOS]
SITIOS += [(s_, g_) for s_, g_, _ in montar.ENVIA_SITIOS]
SITIOS += [(s_, d_) for s_, _, d_, _, _, _ in getattr(montar, 'MENU_DESVIOS', ())]   # v53: o EXIT dos grupos
SITIOS.append((montar.PONTEIRO_EM, 'menu_abre'))                                     # v53: a entrada do menu
extra = {}
for s, gn in SITIOS:
    extra.setdefault(rotina_de(s), set()).add(gn)

memo_f, memo_n = {}, {}


def fab(r, pilha=frozenset(), mexido=True):
    """Profundidade de uma rotina de fabrica; mexido = com as nossas substituicoes."""
    chave = (r, mexido)
    if chave in memo_f:
        return memo_f[chave]
    if r in pilha:
        return 0
    pilha = pilha | {r}
    melhor = 0
    for alvo, tipo in cf.get(r, ()):
        f = fab(alvo, pilha, mexido) - (qf.get(r, 0) if tipo == 'goto' else 0)
        melhor = max(melhor, f)
    if mexido:
        for gn in extra.get(r, ()):
            melhor = max(melhor, nosso(gn))
    v = qf.get(r, 0) + melhor
    if mexido and r in DESVIOS:
        v = max(v, nosso(DESVIOS[r]))
    if mexido and r in TROCADAS:
        v = nosso(TROCADAS[r])
    memo_f[chave] = v
    return v


def nosso(f, pilha=frozenset()):
    if f in memo_n:
        return memo_n[f]
    if f in pilha:
        return 0
    pilha = pilha | {f}
    melhor = 0
    for alvo, tipo in cn.get(f, ()):
        if alvo in por_ini:
            v = nosso(por_ini[alvo], pilha)
        elif (montar.LIVRE <= alvo < montar.LIVRE_FIM or montar.ENDERECO_CODIGO <= alvo < 0x01E96E00
              or montar.VOZES <= alvo < montar.VOZES_FIM):
            continue                       # dentro das nossas faixas: desvio interno, nao rotina de fabrica
        elif alvo == 0x01E44C6C:           # o goto do radio_ajustes_fabrica: o resto da de fabrica
            v = fab(0x01E44C68, mexido=False)
        elif alvo in qf:
            v = fab(alvo)
        else:
            continue                       # desvio dentro da propria rotina
        if tipo == 'goto':                 # salto de cauda para outra rotina: o quadro daqui ja saiu,
            v -= qn.get(f, 0)              # como no grafo de fabrica (v65)
        melhor = max(melhor, v)
    memo_n[f] = qn.get(f, 0) + melhor
    return memo_n[f]



problemas = []
if detalhe:
    print('ponto de entrada                      fabrica  montado  diferenca')
    for e, d in sorted(DESVIOS.items()):
        a, b = fab(e, mexido=False), fab(e)
        print(f'  {d:32s} {a:6d}  {b:6d}  {b - a:+5d}')
    for s, gn in SITIOS:
        print(f'  {gn:20s} em {s:#x}           {nosso(gn):6d}')
    print()
for nome, raiz in (('interface 0x01E4B416', 0x01E4B416), ('laco do sinal 0x01E4622A', 0x01E4622A),
                   ('partida', rotina_de(0x01E56AC0))):
    a, b = fab(raiz, mexido=False), fab(raiz)
    print(f'6. pilha: {nome:26s} de fabrica {a:4d} B, montado {b:4d} B: ' + ('ok' if b <= a else 'PASSA'))
    if b > a:
        problemas.append(nome)

# os get/set/exibe dos menus, chamados por ponteiro, medidos no ponto da chamada
B = 0x01E00100
velho = open(os.path.join(BUILD, 'app.bin'), 'rb').read()
blob = open(os.path.join(BUILD, 'bancos.bin'), 'rb').read()
_, tela = montar.codigo_ligado(os.path.join(BUILD, 'bancos.elf'), '.livre')
tab, _ = montar.monta_menu(velho, blob, tela)
itens = [tab[o:o + 28] for o in range(0, len(tab), 28)]
for T, n in ((0x1e61168, 5), (0x1e6137c, 6)):
    itens += [velho[T - B + i * 28:T - B + i * 28 + 28] for i in range(n)]
pior = []
for it in itens:
    if not any(it):
        continue
    for campo, off, sitio in (('get', 3, 296), ('set', 7, 144), ('exibe', 0x13, 296)):
        a = struct.unpack_from('<I', it, off)[0]
        if campo == 'exibe' and not it[0] & 0x80:
            continue
        if a in por_ini:
            d = nosso(por_ini[a])
            nome = por_ini[a]
        elif a in qf:
            d = fab(a)
            nome = hex(a)
        else:
            continue
        pior.append((sitio + d, campo, nome))
pior = sorted(set(pior), reverse=True)
t, campo, nome = pior[0]
print(f'6. pilha: menus, o pior get/set/exibe ({campo} {nome}): {t} B no ponto da chamada, de 740: '
      + ('ok' if t <= 740 else 'PASSA'))
if detalhe:
    for t_, c_, n_ in pior[:12]:
        print(f'     {c_:5s} {n_:16s} {t_}')
if t > 740:
    problemas.append('menus')
sys.exit(1 if problemas else 0)
