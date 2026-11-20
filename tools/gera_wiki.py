"""Gera a aba Wiki do GitHub a partir de docs/wiki, que é a fonte (versionada e conferida com o resto do repositório).

Uso: python -I tools/gera_wiki.py <pasta do clone de https://github.com/tempraturbo/BajetonBJ8300.wiki.git>

Apaga os .md da pasta e escreve uma página por arquivo de docs/wiki, com o _Sidebar.md e o _Footer.md. Nas páginas, o
título (o "# " do alto) sai, porque a wiki já mostra o nome da página; os links para outra página de docs/wiki viram o
nome dela na wiki, as imagens de docs/imagens vão pelo raw.githubusercontent.com e o resto do repositório vai pelo
github.com. Link para arquivo que não existe, ou para um .md de docs/wiki fora da tabela, para o script. Depois:
git add -A, commit e push no clone da wiki."""
import os
import posixpath
import re
import subprocess
import sys
from urllib.parse import quote

REPO = 'https://github.com/tempraturbo/BajetonBJ8300'
RAW = 'https://raw.githubusercontent.com/tempraturbo/BajetonBJ8300/main/'
DOAR = 'https://www.paypal.com/donate/?hosted_button_id=2VPWYBMKBSJX6'
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# arquivo de docs/wiki -> página da wiki, na ordem do menu lateral
PAGINAS = {
    'README.md': ('Home', '📖 Início'),
    'radio.md': ('Funções-do-rádio', '📻 Funções do rádio'),
    'gravar.md': ('Gravar-o-firmware', '📥 Gravar o firmware'),
    'tempra-link.md': ('Manual-do-Tempra-Link', '💻 Manual do Tempra Link'),
    'chirp.md': ('Usando-o-CHIRP', '🔌 Usando o CHIRP'),
    'versoes.md': ('Versões', '🗂️ Versões'),
    'desenvolvimento.md': ('Desenvolvimento', '🛠️ Desenvolvimento'),
    'README.en.md': ('English', '📖 Start'),
    'radio.en.md': ('Radio-features', "📻 The radio's features"),
    'gravar.en.md': ('Flashing-the-firmware', '📥 Flashing the firmware'),
    'tempra-link.en.md': ('Tempra-Link-manual', '💻 Tempra Link manual'),
    'chirp.en.md': ('Using-CHIRP', '🔌 Using CHIRP'),
    'versoes.en.md': ('Versions', '🗂️ Versions'),
    'desenvolvimento.en.md': ('Development', '🛠️ Development'),
}


def converte(alvo):
    if re.match(r'[a-z]+:', alvo) or alvo.startswith('#'):
        return alvo
    caminho, _, ancora = alvo.partition('#')
    ancora = '#' + ancora if ancora else ''
    real = posixpath.normpath(posixpath.join('docs/wiki', caminho))
    if real.startswith('docs/wiki/') and real.endswith('.md'):
        return quote(PAGINAS[real[len('docs/wiki/'):]][0]) + ancora
    disco = os.path.join(RAIZ, *real.split('/'))
    if not os.path.exists(disco):
        sys.exit(f'link para o que não existe: {alvo}')
    if real.startswith('docs/imagens/'):
        return RAW + real
    if real == 'README.md':
        return REPO + ancora
    return f'{REPO}/{"tree" if os.path.isdir(disco) else "blob"}/main/{real}{ancora}'


def pagina(texto):
    texto = re.sub(r'\A# [^\n]*\n\n?', '', texto)
    partes = re.split(r'(^```[^\n]*\n.*?^```[ \t]*$)', texto, flags=re.M | re.S)
    for i in range(0, len(partes), 2):                  # fora dos blocos de código
        partes[i] = re.sub(r'(\]\()([^)\s]+)(\))', lambda m: m[1] + converte(m[2]) + m[3], partes[i])
        partes[i] = re.sub(r'((?:href|src)=")([^"]+)(")', lambda m: m[1] + converte(m[2]) + m[3], partes[i])
    return ''.join(partes)


def menu(idioma):
    return '\n'.join(f'- [{titulo}]({quote(nome)})' for arq, (nome, titulo) in PAGINAS.items()
                     if arq.endswith('.en.md') == (idioma == 'en'))


def main(destino):
    for nome in os.listdir(destino):
        if nome.endswith('.md'):
            os.remove(os.path.join(destino, nome))
    commit = subprocess.run(['git', '-C', RAIZ, 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True,
                            check=True).stdout.strip()
    saida = {}
    for arq, (nome, _) in PAGINAS.items():
        with open(os.path.join(RAIZ, 'docs', 'wiki', arq), encoding='utf-8') as f:
            saida[nome] = pagina(f.read().replace('\r\n', '\n'))
    sobra = set(n for n in os.listdir(os.path.join(RAIZ, 'docs', 'wiki')) if n.endswith('.md')) - set(PAGINAS)
    if sobra:
        sys.exit(f'páginas de docs/wiki fora da tabela: {sorted(sobra)}')
    saida['_Sidebar'] = (f'**[🏠 Página do projeto / Project page]({REPO})**\n\n**Português**\n\n{menu("pt")}\n\n'
                         f'**English**\n\n{menu("en")}\n\n**[💚 Doar / Donate]({DOAR})**\n')
    saida['_Footer'] = (f'Esta wiki é gerada de [`docs/wiki`]({REPO}/tree/main/docs/wiki) (commit `{commit}`): as '
                        f'mudanças entram por lá. · This wiki is generated from `docs/wiki` (commit `{commit}`): changes '
                        f'go in there. · [💚 Doar / Donate]({DOAR})\n')
    for nome, texto in saida.items():
        with open(os.path.join(destino, nome + '.md'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(texto)
    print(f'{len(saida)} arquivos em {destino}, de docs/wiki no commit {commit}')


if __name__ == '__main__':
    main(sys.argv[1])
