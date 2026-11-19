"""Compila o desenho da tela do app (firmware/previa/tela_pc.c, v162): o codigo da tela do radio com a maquina da
previa, para o Tempra Link do Windows e para o celular.

  win32    tempralink/tela/tela_bj_win32.obj, ligado dentro do TempraLink-BJ8300.exe (o Delphi le COFF)
  win64    tempralink/tela/tela_bj_win64.obj, ligado dentro do app do celular compilado para Windows
  android  tempralink/tela/libtelabj.so, arm64, pelo clang do NDK que o Delphi instala
  dll64    tela_bj_win64.dll, so para o tests/teste_tela_pc.py (o Python de 64 bits a carrega)

Precisa do zig (pip install ziglang), como a previa; o android, do NDK (ANDROID_NDK, ou o do catalogo do RAD Studio).
Uso: python -I firmware/previa/tela_pc.py [win32] [win64] [android] [dll64] [--saida <pasta>]
     (sem alvo, os tres primeiros, em tempralink/tela)
"""
import glob
import os
import subprocess
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
FW = os.path.dirname(AQUI)
RAIZ = os.path.dirname(FW)
FONTE = os.path.join(AQUI, 'tela_pc.c')
OPCOES = ['-O2', '-DPREVIA', '-Wall', '-Wno-unused-function', '-Wno-unused-parameter', '-I', os.path.join(FW, 'build'),
          '-I', AQUI]
NDK_CATALOGO = r'C:\Users\Public\Documents\Embarcadero\Studio\23.0\CatalogRepository\AndroidNDK-*\android-ndk-r*'


def prepara():
    """Os .h gerados que a tela usa (a fonte e as tabelas do menu), como a previa os gera."""
    subprocess.run([sys.executable, '-I', os.path.join(FW, 'scripts', 'fonte.py')], check=True,
                   stdout=subprocess.DEVNULL)
    sys.path.insert(0, AQUI)
    import previa
    previa.menu_grupos_h()


def zig(alvo, saida, *extra):
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-target', alvo, *OPCOES, *extra, '-o', saida, FONTE],
                   check=True)


def clang_ndk():
    ndk = os.environ.get('ANDROID_NDK') or next(iter(sorted(glob.glob(NDK_CATALOGO), reverse=True)), None)
    if not ndk:
        sys.exit('nao achei o NDK (ANDROID_NDK, ou o do catalogo do RAD Studio)')
    return os.path.join(ndk, 'toolchains', 'llvm', 'prebuilt', 'windows-x86_64', 'bin', 'clang.exe')


def objeto(alvo, saida):
    """O .obj que o Delphi liga: sem depuracao, sem o .llvm_addrsig e sem o -export (TELA_OBJ). Os nomes globais sem
    ponto: o Delphi os le como identificadores (os locais com ponto, como os static das funcoes, ele liga)."""
    zig(alvo, saida, '-c', '-g0', '-fno-addrsig', '-fno-stack-protector', '-fno-builtin', '-DTELA_OBJ')
    nm = os.path.join(RAIZ, 'tools', 'jl-toolchain-2.5.3', 'llvm-nm.exe')
    nomes = subprocess.run([nm, saida], capture_output=True, text=True, check=True).stdout.split('\n')
    ruins = [n for n in nomes if len(n.split(' ')) == 3 and n.split(' ')[1].isupper() and '.' in n.split(' ')[2]]
    if ruins:
        sys.exit(f'{saida}: nomes globais com ponto, que o Delphi nao le: {ruins}')


def compila(alvos, pasta):
    os.makedirs(pasta, exist_ok=True)
    prepara()
    feitos = []
    if 'win32' in alvos:
        feitos.append(os.path.join(pasta, 'tela_bj_win32.obj'))
        objeto('x86-windows-gnu', feitos[-1])
    if 'win64' in alvos:
        feitos.append(os.path.join(pasta, 'tela_bj_win64.obj'))
        objeto('x86_64-windows-gnu', feitos[-1])
    if 'dll64' in alvos:
        feitos.append(os.path.join(pasta, 'tela_bj_win64.dll'))
        zig('x86_64-windows-gnu', feitos[-1], '-shared')
        for resto in ('.lib', '.pdb'):
            r = feitos[-1][:-4] + resto
            if os.path.exists(r):
                os.remove(r)
    if 'android' in alvos:
        feitos.append(os.path.join(pasta, 'libtelabj.so'))
        # os segmentos alinhados em 16 KB: com os 4 KB de fábrica do NDK r21, o Android 15 em diante avisa que o app
        # não é compatível com as páginas de 16 KB (e num aparelho delas ele não abre)
        subprocess.run([clang_ndk(), '--target=aarch64-linux-android23', '-shared', '-fPIC', '-fvisibility=hidden',
                        '-Wl,-z,max-page-size=16384', *OPCOES, '-o', feitos[-1], FONTE], check=True)
    return feitos


def main():
    args = sys.argv[1:]
    pasta = os.path.join(RAIZ, 'tempralink', 'tela')
    if '--saida' in args:
        k = args.index('--saida')
        pasta = os.path.abspath(args[k + 1])
        del args[k:k + 2]
    for f in compila(set(args) or {'win32', 'win64', 'android'}, pasta):
        print(f, os.path.getsize(f), 'B')


if __name__ == '__main__':
    main()
