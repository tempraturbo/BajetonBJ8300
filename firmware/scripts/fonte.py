"""Gera build/fonte_b.h a partir de fonte/fonte_b.txt (a fonte do tema B).

Uso: python -I scripts/fonte.py   (de dentro de firmware/)

Cada letra vira uma coluna por byte, com o bit 0 na linha de cima, e um u16 por letra com o
comeco dela na tabela de colunas (<< 3) e a largura (3 bits). As 64 letras de 32 a 95 vem primeiro;
os icones ("== icone nome") depois, com os codigos 0x80 em diante e um #define ICONE_NOME cada.
E um bit por coluna com a largura dela no tamanho medio (tela.c, MEDIA): 1 = 2 px, para a coluna de
haste (4 pontos acesos ou mais), a vazia e, na letra sem haste (S, Z, 2), as duas das pontas.
Recusa letra faltando, repetida, com linha de largura diferente ou com caractere que nao seja # ou .
"""
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
ORIGEM = os.path.join(AQUI, '..', 'fonte', 'fonte_b.txt')
DESTINO = os.path.join(AQUI, '..', 'build', 'fonte_b.h')
ALTURA, PRIMEIRA, LETRAS, PRIMEIRO_ICONE = 7, 32, 64, 0x80


def le_fonte(caminho):
    """{letra: linhas} e [(nome do icone, linhas)], na ordem do arquivo."""
    letras, icones, atual = {}, [], None
    for n, linha in enumerate(open(caminho, encoding='utf-8'), 1):
        linha = linha.rstrip('\n')
        if linha.startswith('# '):
            continue                                   # comentario: linha de pixels nao tem espaco
        if linha.startswith('== icone '):
            nome = linha[9:]
            if not nome.isidentifier() or nome in dict(icones):
                raise SystemExit(f'{caminho}:{n}: icone invalido ou repetido: {nome!r}')
            atual = []
            icones.append((nome, atual))
        elif linha.startswith('== '):
            nome = linha[3:]
            c = ' ' if nome == 'espaco' else nome
            if len(c) != 1 or c in letras:
                raise SystemExit(f'{caminho}:{n}: letra invalida ou repetida: {nome!r}')
            atual = letras[c] = []
        elif linha:
            if atual is None or set(linha) - set('#.'):
                raise SystemExit(f'{caminho}:{n}: linha de pixels fora de uma letra: {linha!r}')
            atual.append(linha)
    return letras, icones


def main():
    letras, icones = le_fonte(ORIGEM)
    esperadas = [chr(c) for c in range(PRIMEIRA, PRIMEIRA + LETRAS)]
    faltam = [c for c in esperadas if c not in letras]
    if faltam or len(letras) != LETRAS:
        raise SystemExit(f'faltam letras: {faltam}; sobram: {sorted(set(letras) - set(esperadas))}')
    indice, colunas, media = [], [], []
    for nome, linhas in [(c, letras[c]) for c in esperadas] + icones:
        largura = len(linhas[0])
        if len(linhas) != ALTURA or any(len(l) != largura for l in linhas) or not 1 <= largura <= 7:
            raise SystemExit(f'{nome!r}: precisa de {ALTURA} linhas da mesma largura (1 a 7)')
        indice.append(len(colunas) << 3 | largura)
        letra = [sum(1 << y for y in range(ALTURA) if linhas[y][x] == '#') for x in range(largura)]
        pontos = [bin(c).count('1') for c in letra]
        dupla = [p >= 4 or p == 0 for p in pontos]
        if max(pontos) < 4 and largura > 1:
            dupla[0] = dupla[-1] = True
        colunas += letra
        media += dupla
    total = LETRAS + len(icones)
    os.makedirs(os.path.dirname(DESTINO), exist_ok=True)
    with open(DESTINO, 'w', encoding='utf-8', newline='\n') as f:
        f.write('/* Gerado por scripts/fonte.py a partir de fonte/fonte_b.txt: nao editar a mao. */\n')
        f.write(f'#define FONTE_B_ALTURA    {ALTURA}\n#define FONTE_B_PRIMEIRA  {PRIMEIRA}\n'
                f'#define FONTE_B_LETRAS    {LETRAS}\n#define FONTE_B_ICONES    {len(icones)}\n')
        for i, (nome, _) in enumerate(icones):
            f.write(f'#define ICONE_{nome.upper():12s} "\\x{PRIMEIRO_ICONE + i:02X}"\n')
        f.write('\n/* comeco da letra em fonte_b_colunas (<< 3) | largura; as letras e depois os icones */\n')
        f.write(f'static const u16 fonte_b_letra[{total}] = {{\n')
        for i in range(0, total, 8):
            f.write('    ' + ', '.join(f'0x{v:04X}' for v in indice[i:i + 8]) + ',\n')
        f.write('};\n\n/* uma coluna por byte, bit 0 em cima */\n')
        f.write(f'static const u8 fonte_b_colunas[{len(colunas)}] = {{\n')
        for i in range(0, len(colunas), 12):
            f.write('    ' + ', '.join(f'0x{v:02X}' for v in colunas[i:i + 12]) + ',\n')
        bits = [sum(1 << b for b in range(8) if i + b < len(media) and media[i + b])
                for i in range(0, len(media), 8)]
        f.write('};\n\n/* a coluna k de fonte_b_colunas no tamanho medio: bit k & 7 do byte k >> 3, 1 = 2 px */\n')
        f.write(f'static const u8 fonte_b_media[{len(bits)}] = {{\n')
        for i in range(0, len(bits), 12):
            f.write('    ' + ', '.join(f'0x{v:02X}' for v in bits[i:i + 12]) + ',\n')
        f.write('};\n')
    print(f'{os.path.normpath(DESTINO)}: {LETRAS} letras, {len(icones)} icones, {len(colunas)} colunas')


if __name__ == '__main__':
    main()
