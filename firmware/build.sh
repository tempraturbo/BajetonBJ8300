#!/bin/sh
# Compila, linka, monta e confere o firmware dos bancos do BJ-8300.
# Uso: sh build.sh   (de dentro de C:\temp\bj8300-firmware)
#
# Se os enderecos dos simbolos mudarem (o llvm-nm mostra ao fim), atualizar o dicionario S em
# scripts/montar.py antes de montar, senao o menu aponta para o lugar errado.
set -e
cd "$(dirname "$0")"

B="C:/tools/jl-toolchain-2.5.3/C\$/JL/pi32/bin"
ORIGINAL=original/BJ8300_V0.12.anc
SAIDA=build/BJ8300_BANK.anc

mkdir -p build
[ -f build/app.bin ] || python -I scripts/jlfw.py unpack "$ORIGINAL" build >/dev/null

python -I scripts/fonte.py >/dev/null        # build/fonte_b.h, a fonte da tela B
compila() {   # $1 = arquivo, $2 = o carimbo do fim
    "$B/clang.exe" -target pi32v2 -integrated-as -Oz -fno-builtin -ffreestanding -Ibuild \
        -DCARIMBO_FIM=$2 -c src/$1.c -o build/$1.o
}
liga() {
    "$B/pi32v2-ld.exe" -T src/bancos.ld -o build/bancos.elf build/bancos.o build/tela.o
    "$B/llvm-objcopy.exe" -O binary -j .text build/bancos.elf build/bancos.bin
}
# Duas ligacoes (v47): a primeira da o .text (o codigo do fim da imagem), e a segunda grava o CRC16
# dele no carimbo_fim, dentro do app.bin, para o carregador ver toda mudanca no fim (bancos.c).
compila tela 0
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
python -I scripts/conferir.py "$SAIDA" build/bancos.bin
