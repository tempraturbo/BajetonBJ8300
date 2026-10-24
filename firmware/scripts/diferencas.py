"""Mostra, byte a byte, o que mudou de um .anc para outro e em que parte do arquivo cai.

Uso: python -I diferencas.py <original.anc> <novo.anc>

Serve para conferir o empacotamento: a resposta tem que ser so o previsto (cabecalho e tabela do
UFW, por causa dos CRCs; as duas entradas JLFS do app; o app.bin; e a area do codigo novo no fim
de cada imagem). Qualquer faixa fora disso e sinal de que o montador escreveu onde nao devia.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jlfw

BASE_APP = 0x01E00100


def faixas(a, b, folga=16):
    """Faixas (ini, fim) em que os dois arquivos diferem, juntando o que esta a menos de 'folga'."""
    out = []
    i = 0
    while i < min(len(a), len(b)):
        if a[i] == b[i]:
            i += 1
            continue
        ini = i
        ultimo = i
        while i < min(len(a), len(b)) and i - ultimo <= folga:
            if a[i] != b[i]:
                ultimo = i
            i += 1
        out.append((ini, ultimo))
    return out


def main():
    velho = jlfw.kdh_scramble(open(sys.argv[1], 'rb').read())
    novo = jlfw.kdh_scramble(open(sys.argv[2], 'rb').read())
    if len(velho) != len(novo):
        print(f'ATENCAO: tamanhos diferentes: {len(velho)} e {len(novo)}')
    _, entradas = jlfw.ufw_entries(velho)
    chave = jlfw.find_chipkey(velho[entradas[0]['offset']:entradas[0]['offset'] + entradas[0]['size']])

    # mapa das regioes do arquivo, para dizer onde cada diferenca caiu
    regioes = [(0, 0x3F, 'cabecalho do UFW (CRCs)'),
               (0x40, 0x40 + len(entradas) * 0x50 - 1, 'tabela de entradas do UFW (CRCs)')]
    for e in entradas:
        ini, fim = e['offset'], e['offset'] + e['size'] - 1
        if e['type'] not in jlfw.FLASH_TYPES:
            regioes.append((ini, fim, e['name']))
            continue
        img = novo[ini:fim + 1]
        _, base, sub = jlfw.app_dir(img, chave)
        app = next(s for s in sub if s['name'] == 'app.bin')
        head = next(s for s in sub if s['name'] == 'app_area_head')
        regioes.append((ini, ini + base - 1, f"{e['name']}: uboot e diretorio de cima (nao deve mudar)"))
        regioes.append((ini + base + head['pos'], ini + base + head['pos'] + 31,
                        f"{e['name']}: entrada JLFS app_area_head"))
        regioes.append((ini + base + app['pos'], ini + base + app['pos'] + 31,
                        f"{e['name']}: entrada JLFS app.bin"))
        regioes.append((ini + base + app['offset'], ini + base + app['offset'] + app['size'] - 1,
                        f"{e['name']}: app.bin"))
        regioes.append((ini + base + app['offset'] + app['size'], fim,
                        f"{e['name']}: depois do app.bin (recursos e espaco livre do fim)"))

    def onde(p):
        for ini, fim, nome in regioes:
            if ini <= p <= fim:
                return nome, ini
        return '???', 0

    total = 0
    for ini, fim in faixas(velho, novo):
        nome, base_regiao = onde(ini)
        n = sum(1 for i in range(ini, fim + 1) if velho[i] != novo[i])
        total += n
        extra = ''
        if nome.endswith('app.bin'):
            extra = f" = {BASE_APP + ini - base_regiao:#x} no app"
        print(f'{ini:#9x}..{fim:#9x}  {n:5d} bytes  {nome}{extra}')
    print(f'\ntotal de bytes diferentes: {total}')


if __name__ == '__main__':
    main()
