# Toolchain da JieLi 2.5.3 (só o que o build usa)

> 🇬🇧 English below.

Estes cinco executáveis são do compilador oficial da **Zhuhai JieLi** para o núcleo `pi32v2` (o
processador AC695X do BJ-8300), versão 2.5.3. Eles estão aqui para que qualquer pessoa compile o
firmware sem procurar e instalar nada além do Python e do Git; o `firmware/build.sh` usa esta pasta.

| Arquivo | Para quê |
|---|---|
| `clang.exe` | compila o C (`clang 4.0.1`, alvo `pi32v2`) |
| `pi32v2-ld.exe` | liga o código nos endereços do `firmware/src/bancos.ld` |
| `llvm-objcopy.exe` | tira o código ligado do ELF |
| `llvm-nm.exe` | lista os símbolos, que a montagem confere |
| `llvm-objdump.exe` | desmonta o código novo e o de fábrica, para as conferências e a medida de pilha |

**De onde vieram:** do instalador oficial da JieLi (Inno Setup, assinado pela Zhuhai JieLi),
extraídos com o `innoextract` 1.9 sem executar o instalador, e sem nenhuma alteração. A instalação
completa tem 155 MB; o build só precisa destes cinco, que dependem apenas de DLLs do Windows.
Compilar com eles dá o mesmo firmware, byte a byte, que compilar com a instalação inteira.

**Conferência:** o `SHA256SUMS` tem o sha256 de cada um, e o `build.sh` o confere antes de compilar.

O compilador é propriedade da Zhuhai JieLi Technology e está aqui só para compilar este firmware,
como o gravador `tools/anc-bootloader` e o driver `tools/ch340-driver`, que são dos respectivos
fabricantes.

---

# JieLi toolchain 2.5.3 (only what the build uses)

These five executables are from **Zhuhai JieLi**'s official compiler for the `pi32v2` core (the
BJ-8300's AC695X processor), version 2.5.3. They are here so anyone can build the firmware without
finding and installing anything but Python and Git; `firmware/build.sh` uses this folder.

`clang.exe` compiles the C (`clang 4.0.1`, target `pi32v2`), `pi32v2-ld.exe` links the code at the
addresses in `firmware/src/bancos.ld`, `llvm-objcopy.exe` takes the linked code out of the ELF,
`llvm-nm.exe` lists the symbols the assembly checks, and `llvm-objdump.exe` disassembles the new and
the stock code for the checks and the stack measurement.

**Where they came from:** JieLi's official installer (Inno Setup, signed by Zhuhai JieLi), extracted
with `innoextract` 1.9 without running the installer, unmodified. The full install is 155 MB; the build
only needs these five, which depend on Windows DLLs alone. Building with them gives the same firmware,
byte for byte, as building with the full install.

**Check:** `SHA256SUMS` has each file's sha256, and `build.sh` checks it before building.

The compiler belongs to Zhuhai JieLi Technology and is here only to build this firmware, like the
`tools/anc-bootloader` flasher and the `tools/ch340-driver` driver, which belong to their makers.
