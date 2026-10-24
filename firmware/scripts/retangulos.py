"""Mapeia as chamadas de janela(x0, x1, y0, y1, limpar, cor) no app.bin do BJ-8300.

Uso: python -I retangulos.py <app.bin> [0xJANELA]

A rotina 0x01E3AD34 define a area de desenho e, com limpar != 0, enche ela com cor (RGB565):
  r0=x0  r1=x1  r2=y0  r3=y1  [sp]=limpar  [sp+4]=cor
Os quatro primeiros vem em registrador, quase sempre postos por "rX = imm8" logo antes, e os dois
da pilha por "[sp]=rX" / "[sp+4]=rX" - entao da para ler o retangulo de cada chamada so olhando
para tras. E assim que se acha, sem Ghidra, qual trecho da tela cada chamada pinta.

Codificacao de "rX = imm8" (deduzida de casos conhecidos e conferida contra o llvm-objdump):
  byte1 & 0xE0 == 0x20 e byte0 & 0xC0 == 0x40
  imm = (byte1 & 0x1F) | ((byte0 >> 3) & 7) << 5      reg = byte0 & 7
"""
import sys

BASE = 0x01E00100
JANELA = 0x01E3AD34
JANELA_ATE = 16            # quantas instrucoes de 2 bytes olhar para tras


def imm8(app, o):
    """(reg, valor) se em o houver um "rX = imm8"; senao None."""
    b0, b1 = app[o], app[o + 1]
    if (b1 & 0xE0) == 0x20 and (b0 & 0xC0) == 0x40:
        return b0 & 7, (b1 & 0x1F) | ((b0 >> 3) & 7) << 5
    return None


def pilha(app, o):
    """([sp+N], reg) se em o houver um "[sp+N] = rX" (2 bytes); senao None."""
    b0, b1 = app[o], app[o + 1]
    if (b1 & 0xF8) == 0x20 and (b0 & 0xF8) == 0x80:      # 8X 2Y -> [sp+Y*4] = rX
        return (b1 & 0x07) * 4, b0 & 0x07
    return None


def chamadas_para(app, alvo):
    """Offsets das chamadas (call jaddr23, 4 bytes) para alvo."""
    achados = []
    for o in range(0, len(app) - 4, 2):
        if app[o + 1] != 0xEA or (app[o] & 0xC0) != 0x80:
            continue
        d = ((app[o] & 0x3F) << 16 | app[o + 3] << 8 | app[o + 2])
        if d & 0x200000:
            d -= 0x400000
        if BASE + o + 4 + d * 2 == alvo:
            achados.append(o)
    return achados


def sitios_de_cor(app, alvo=JANELA, largura_total=True):
    """(endereco do "rX = 0" que alimenta a cor, reg) das chamadas que enchem de preto.

    Serve para o diagnostico: trocando esse unico load de 2 bytes, a faixa daquela chamada passa a
    ser pintada de outra cor - sem crescer o codigo e sem tocar em mais nada.
    """
    achados = []
    for o in chamadas_para(app, alvo):
        regs, pilhas, onde_pilha = {}, {}, {}
        for k in range(1, JANELA_ATE):                   # anda para tras de 2 em 2
            p = o - k * 2
            if p < 0:
                break
            achou = imm8(app, p)
            if achou and achou[0] not in regs:
                regs[achou[0]] = achou[1]
            achou = pilha(app, p)
            if achou and achou[0] not in pilhas:
                pilhas[achou[0]], onde_pilha[achou[0]] = achou[1], p
        reg_cor = pilhas.get(4)
        if reg_cor is None or regs.get(pilhas.get(0)) != 1:
            continue                                     # sem limpar: nao enche nada
        if largura_total and (regs.get(0), regs.get(1)) != (0, 160):
            continue
        # PAGA CARO: o registrador da cor costuma ser reusado depois (p.ex. como x0 = 0), entao o
        # load que interessa e o ULTIMO antes do "[sp+4] = rX" - procurar a partir do call pega o
        # da outra coisa e o remendo cairia no argumento errado.
        onde = valor = None
        for k in range(1, JANELA_ATE):
            p = onde_pilha[4] - k * 2
            if p < 0:
                break
            achou = imm8(app, p)
            if achou and achou[0] == reg_cor:
                onde, valor = p, achou[1]
                break
        if onde is None or valor != 0:
            continue                                     # cor nao e constante preta
        achados.append((BASE + onde, reg_cor, BASE + o))
    return achados


def main():
    app = open(sys.argv[1], 'rb').read()
    if sys.argv[2:3] == ['cores']:
        for endereco, reg, chamada in sitios_de_cor(app):
            print('    (%#010x, %d),   # cor da janela chamada em %#010x' % (endereco, reg, chamada))
        return
    alvo = int(sys.argv[2], 0) if len(sys.argv) > 2 else JANELA
    for o in chamadas_para(app, alvo):
        regs, pilhas = {}, {}
        for k in range(1, JANELA_ATE):                   # anda para tras de 2 em 2
            p = o - k * 2
            if p < 0:
                break
            achou = imm8(app, p)
            if achou and achou[0] not in regs:
                regs[achou[0]] = achou[1]
            achou = pilha(app, p)
            if achou and achou[0] not in pilhas:
                pilhas[achou[0]] = achou[1]
        x0, x1, y0, y1 = (regs.get(i) for i in range(4))
        limpar = regs.get(pilhas.get(0))
        cor = regs.get(pilhas.get(4))
        if None in (x0, x1, y0, y1):
            print('%08X  (argumentos nao sao todos constantes)' % (BASE + o))
            continue
        print('%08X  x %3d..%-3d  y %3d..%-3d  (%dx%d)  limpar=%s cor=%s' % (
            BASE + o, x0, x1, y0, y1, x1 - x0, y1 - y0,
            limpar if limpar is not None else '?',
            ('%#06x' % cor) if cor is not None else '?'))


if __name__ == '__main__':
    main()
