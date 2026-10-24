#!/bin/sh
# Desmonta um pedaco do app.bin do BJ-8300 com o llvm-objdump da JieLi, no endereco real.
#
# Uso: sh scripts/desmonta.sh <endereco_inicial> [endereco_final|+tamanho]
#   sh scripts/desmonta.sh 0x01e3e18a +0x40
#
# O llvm-objdump 4.0.1 da toolchain nao le binario cru (-b binary nao existe), entao o app.bin
# entra num .elf por .incbin, linkado no endereco de execucao 0x01e00100. O .elf e gerado uma vez
# e reaproveitado enquanto o app.bin nao mudar.
set -e
cd "$(dirname "$0")/.."
B="C:/tools/jl-toolchain-2.5.3/C\$/JL/pi32/bin"
APP=build/app.bin
ELF=build/app.elf

[ -f "$APP" ] || python -I scripts/jlfw.py unpack original/BJ8300_V0.12.anc build >/dev/null

if [ ! -f "$ELF" ] || [ "$APP" -nt "$ELF" ]; then
    mkdir -p build
    # o clang e nativo do Windows: precisa do caminho em C:/..., nao do /c/... do shell
    printf '.section .text\n.incbin "%s"\n' "$(cygpath -m "$PWD/$APP")" > build/app_wrap.S
    printf 'MEMORY { app (rx) : ORIGIN = 0x01E00100, LENGTH = %d }\nSECTIONS { .text : { *(.text*) } > app }\n' \
        "$(stat -c%s "$APP")" > build/app_wrap.ld
    "$B/clang.exe" -target pi32v2 -integrated-as -c build/app_wrap.S -o build/app_wrap.o
    "$B/pi32v2-ld.exe" -T build/app_wrap.ld -o "$ELF" build/app_wrap.o
fi

ini=$1
case "$2" in
    '')  fim=$((ini + 0x80)) ;;
    +*)  fim=$((ini + ${2#+})) ;;
    *)   fim=$2 ;;
esac
"$B/llvm-objdump.exe" -d -triple=pi32v2 -start-address="$ini" -stop-address="$fim" "$ELF" \
    | sed -e '1,/^Disassembly/d' -e '/^$/d' -e '/section .text/d'
