#!/bin/sh
# Compila, liga, monta e confere o firmware do BJ-8300. Sai em firmware/build/BJ8300_BANK.anc.
# Uso: sh firmware/build.sh   (no Git Bash, da raiz do repositorio; ou o compilar.bat, com dois cliques)
#
# Precisa so do Python 3 e do sh do Git para Windows: o compilador da JieLi esta no repositorio, em
# tools/jl-toolchain-2.5.3 (ou na pasta que a variavel JL_TOOLCHAIN apontar).
set -e
cd "$(dirname "$0")"

B="${JL_TOOLCHAIN:-../tools/jl-toolchain-2.5.3}"
ORIGINAL=original/BJ8300_V0.12.anc
SAIDA=build/BJ8300_BANK.anc

# o compilador, com os 5 arquivos conferidos (tools/jl-toolchain-2.5.3/SHA256SUMS), e o Python
[ -f "$B/clang.exe" ] || { echo "nao achei o compilador da JieLi em $B (tools/jl-toolchain-2.5.3)"; exit 1; }
if [ -z "$JL_TOOLCHAIN" ]; then
    (cd "$B" && sha256sum -c --quiet SHA256SUMS) || { echo "o compilador em $B nao confere com o SHA256SUMS"; exit 1; }
fi
python -I -c "import sys; sys.exit(sys.version_info < (3, 8))" 2>/dev/null \
    || { echo "precisa do Python 3.8 ou mais novo, no PATH como 'python'"; exit 1; }

mkdir -p build
[ -f build/app.bin ] || python -I scripts/jlfw.py unpack "$ORIGINAL" build >/dev/null

python -I scripts/fonte.py >/dev/null        # build/fonte_b.h, a fonte da tela B
compila() {   # $1 = arquivo, $2 = o carimbo do fim
    "$B/clang.exe" -target pi32v2 -integrated-as -Oz -fno-builtin -ffreestanding -Ibuild \
        -DCARIMBO_FIM=$2 -c src/$1.c -o build/$1.o
}
liga() {
    "$B/pi32v2-ld.exe" -T src/bancos.ld -o build/bancos.elf build/bancos.o build/tela.o build/mensagem.o
    "$B/llvm-objcopy.exe" -O binary -j .text build/bancos.elf build/bancos.bin
}
# Duas ligacoes (v47): a primeira da o .text (o codigo do fim da imagem), e a segunda grava o CRC16
# dele no carimbo_fim, dentro do app.bin, para o carregador ver toda mudanca no fim (bancos.c).
compila tela 0
compila mensagem 0
compila bancos 0
liga
CARIMBO=$(python -I -c "import sys; sys.path.insert(0, 'scripts'); import jlfw; print(jlfw.crc16(open('build/bancos.bin', 'rb').read()))")
cp build/bancos.bin build/bancos_1.bin
compila bancos $CARIMBO
liga
cmp -s build/bancos.bin build/bancos_1.bin || { echo "o carimbo mudou o .text: recusado"; exit 1; }
rm build/bancos_1.bin
echo "=== carimbo do fim: crc16 do .text = $(printf '%#06x' $CARIMBO)"

"$B/llvm-nm.exe" -n build/bancos.elf > build/simbolos.txt
# o desmontado do codigo novo: o conferir.py decodifica dele todo call/goto (verificacao 4g)
"$B/llvm-objdump.exe" -d -triple=pi32v2 build/bancos.elf > build/bancos.dis
echo "=== simbolos:"
grep ' T ' build/simbolos.txt
echo "=== tamanho: $(stat -c%s build/bancos.bin) bytes"
echo
python -I scripts/montar.py "$ORIGINAL" build/bancos.bin "$SAIDA" build/simbolos.txt
echo
python -I scripts/conferir.py "$SAIDA" build/bancos.bin "$ORIGINAL"

# a pilha (scripts/pilha.py), contra o desmontado do firmware de fabrica, que se gera uma vez
if [ ! -f build/app.dis ] || [ build/app.bin -nt build/app.dis ]; then
    sh scripts/desmonta.sh 0x01e00100 +2 >/dev/null
    "$B/llvm-objdump.exe" -d -triple=pi32v2 build/app.elf > build/app.dis
fi
echo
python -I scripts/pilha.py
echo
echo "=== pronto: firmware/$SAIDA (sha256 $(sha256sum "$SAIDA" | cut -c1-16)...)"
