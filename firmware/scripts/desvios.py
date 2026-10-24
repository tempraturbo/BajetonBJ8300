"""Confere se algum desvio do firmware cai DENTRO de um remendo nosso (e nao no comeco dele).

Uso: python -I desvios.py <app.dis>   (saida do llvm-objdump; ver scripts/desmonta.sh)

Trocar duas instrucoes por uma, como o remendo do ponteiro da tabela de menus fazia ate a v18, so e
seguro se ninguem desviar para o meio do trecho trocado: se desviasse, o processador cairia no meio
de uma instrucao e executaria lixo. Desde a v19 a tabela fica no lugar e o ponteiro nao muda, mas a
conferencia continua valendo para os remendos que restam. O llvm-objdump imprime o destino dos desvios como
deslocamento, entao aqui a conta e feita nas duas convencoes possiveis (relativo ao inicio e ao
fim da instrucao) e qualquer acerto dentro de um remendo e reportado.
"""
import re
import sys

sys.path.insert(0, __file__.rsplit('\\', 1)[0] if '\\' in __file__ else '.')
import montar

LINHA = re.compile(r'^\s*([0-9a-f]{6,8}):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$')
DESVIO = re.compile(r'goto (-?\d+)')


def remendos():
    """(inicio, fim_exclusivo, nome) de cada trecho que o montar.py reescreve."""
    fora = [(montar.GANCHO_EM, 4, 'gancho'),
            (montar.CONTAGEM_EM, len(montar.CONTAGEM_ANTES), 'contagem de itens')]
    if montar.SETAS_INVERTIDAS:
        fora += [(s, 4, 'seta') for s, _ in montar.SETAS]
    if montar.ESPECTRO_TECLAS:
        fora.append((montar.ESPECTRO_EM, len(montar.ESPECTRO_ANTES), 'teclas do espectro'))
        fora += [(s, 4, 'espectro: ' + d) for s, _, _, d in montar.ESPECTRO_CHAMADAS]
        # nas rotinas trocadas inteiras, o corpo antigo vira codigo morto: nenhum desvio pode cair
        # nele (so na entrada, que agora e o goto)
        fora += [(e + 2, 2, 'espectro: entrada de ' + d) for e, _, _, d in montar.ESPECTRO_FUNCOES]
    fora += [(montar.ROGER_DESVIO_EM, 4, 'roger: desvio do valor 3'),
             (montar.ROGER_LACO_EM, len(montar.ROGER_LACO_ANTES), 'roger: laco dos tons'),
             (montar.SCRAMBLE_EM, len(montar.SCRAMBLE_ANTES), 'scramble: frequencia pelo valor')]
    return [(a, a + t, n) for a, t, n in fora]


def main():
    trechos = remendos()
    achados = 0
    for linha in open(sys.argv[1], encoding='utf-8', errors='replace'):
        m = LINHA.match(linha)
        if not m:
            continue
        endereco = int(m.group(1), 16)
        tam = len(m.group(2).split())
        d = DESVIO.search(m.group(3))
        if not d:
            continue
        deslocamento = int(d.group(1))
        # convencao conferida em casos conhecidos (p.ex. o laco em 0x1E48C72): fim da instrucao
        for destino in (endereco + tam + deslocamento,):
            for ini, fim, nome in trechos:
                if ini < destino < fim:                 # cai no MEIO do remendo
                    print(f'  ATENCAO: {endereco:#x} desvia para {destino:#x}, dentro de '
                          f'{nome} ({ini:#x}..{fim - 1:#x})')
                    achados += 1
    print(f'desvios para dentro de remendo: {achados}')
    return 1 if achados else 0


if __name__ == '__main__':
    sys.exit(main())
