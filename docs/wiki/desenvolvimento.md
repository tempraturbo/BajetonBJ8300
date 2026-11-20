# 🛠️ Desenvolvimento

<p align="center">
  <a href="desenvolvimento.en.md"><img src="../imagens/botao-english.svg" alt="English version" height="56"></a>
</p>

[📖 Wiki](README.md) · [🏠 Página inicial](../../README.md)

## Ferramentas pelo cabo

Em [tools/cabo/](../../tools/cabo/), em PowerShell, sem instalar nada:

| Script | Para que serve |
|---|---|
| `cabo.ps1` | Biblioteca: handshake, `Le-Bloco`, `Grava-Bloco` (as outras usam) |
| `mapa_memoria.ps1` | Prova a leitura de 24 bits, diz se os dados vêm crus, acha o **tamanho real do chip** e lista o que existe acima de 64 KB |
| `ler_memoria.ps1` | Despeja os 64 KB que o protocolo de fábrica alcança e confere canais e calibração (só leitura) |
| `teste_cabo_grava.ps1` | Prova a gravação na área dos bancos e que o firmware **recusa** gravar fora dela; limpa no fim |
| `logo.ps1` | Lê (e, num firmware que libere a faixa, grava) a imagem de abertura |

Conversão da imagem de abertura: `python -I firmware/scripts/logo_bmp.py para-radio entrada.bmp
saida.bin previa.png` (BMP 160×138 de 24 bits → RGB565, o mesmo que o CPS11 produz, conferido byte
a byte).

> **Nunca mande nada antes do magic** (nem `E`, nem zeros): o rádio compara o magic desde o início
> do buffer e qualquer byte à frente faz todas as tentativas seguintes falharem até desligar o
> aparelho. E o magic termina em `U`, que é comando do modo UPDATE — insistir com o rádio fora de
> sincronia faz ele entrar em UPDATE sozinho.

## Compilar

Qualquer pessoa compila o firmware no Windows só com o que está no repositório: o compilador da JieLi
(clang para `pi32v2`) vem junto, em [`tools/jl-toolchain-2.5.3`](../../tools/jl-toolchain-2.5.3/LEIA-ME.md),
e faltam só dois programas gratuitos.

**1. Instale, uma vez só:**

| Programa | Onde | Detalhe |
|---|---|---|
| **Git para Windows** | <https://git-scm.com/download/win> | o instalador com as opções padrão serve; ele traz o `bash` que roda o build |
| **Python 3.8 ou mais novo** | <https://www.python.org/downloads/> | na primeira tela do instalador, marque **"Add python.exe to PATH"** |

**2. Baixe o repositório:** `git clone https://github.com/tempraturbo/BajetonBJ8300.git`, ou, no
GitHub, **Code → Download ZIP** (e descompacte).

**3. Compile:** dois cliques em **`compilar.bat`**, na raiz do repositório. Ou, no Git Bash, dentro da
pasta do repositório:

```sh
sh firmware/build.sh
```

Leva uns dois minutos. No fim aparecem `RESULTADO: tudo certo`, as quatro linhas da pilha com `ok` e o
arquivo pronto, **`firmware/build/BJ8300_BANK.anc`**: é ele que se grava no rádio
([como gravar](../../README.md#-como-gravar-o-firmware-passo-a-passo)). Se faltar o Git ou o Python, o
`compilar.bat` diz qual e onde baixar.

O build compila `firmware/src/bancos.c` e `firmware/src/tela.c`, monta o `.anc` a partir do original e
**confere** o resultado: CRCs das 12 entradas, código e tabela de menus presentes e iguais nas 6
imagens de flash, itens de menu, todos os ganchos e remendos, textos, e que nenhum byte foi alterado
fora dos trechos previstos — nem perto das áreas protegidas. Depois mede a **pilha**
(`firmware/scripts/pilha.py`): o código novo roda na pilha de quem o chama, e o caminho mais fundo
dele não pode passar do que o firmware de fábrica já usa ali. O build é reprodutível: o mesmo fonte
dá o mesmo `.anc`, byte a byte, então dá para comparar o sha256 com o do release. Antes de compilar
ele confere o sha256 dos cinco arquivos do compilador; para usar outra cópia do compilador, aponte a
variável `JL_TOOLCHAIN` para ela.

**Opcionais:**

- **Prévia da tela no PC** (abaixo): `pip install ziglang`.
- **Teste do Morse e do APRS** (o pacote, o AFSK, o decodificador e a leitura do Morse nas capturas do rádio, no PC):
  `python -I tests/teste_mensagem.py`,
  com o mesmo zig; e o laço da recepção do APRS inteiro, com o modem, a flash e as teclas imitados:
  `python -I tests/teste_rx.py`.
- **Testes do driver do CHIRP:** `python -I tests/teste_bj8300.py` não precisa de nada;
  `python -I tests/teste_chirp_real.py` precisa do CHIRP
  (`pip install https://codeload.github.com/kk7ds/chirp/tar.gz/refs/heads/master --no-deps`).
- **Estudar o firmware de fábrica:** o build deixa o desmontado inteiro em `firmware/build/app.dis`, e
  os scripts de `firmware/scripts` trabalham sobre ele: `desmonta.sh` (um trecho), `contexto.py` (quem
  chama um endereço, com os argumentos), `fluxo.py` (os caminhos dentro de uma rotina) e
  `escritas.py` (quem lê ou grava um endereço de RAM).

**Orçamento de espaço:** o código novo mora em nove lugares, todos no mesmo endereço nas 6 imagens:

- **a área livre do fim da imagem**, a partir de `0x01E96124`, com 3260 bytes (a V4.0T usa 3254);
- **o espaço que era das vozes**, com o espectro, mas só dentro dos arquivos C0 e C1 (1066 bytes,
  dos quais a V4.0T usa 1060), porque o carregador do rádio recusa a imagem que muda mais arquivos de
  voz;
- **o lugar dos decodificadores G.729 e MP3**, dentro do `app.bin`, com as telas B, o WATCH, o
  TRIPLE, o menu em pastas, o APRS e a tabela de menus (38884 bytes, dos quais a V4.0T usa 38882);
- **os caminhos das vozes**, no `app.bin`, só com o que o nosso código chama (2944 bytes, dos quais a V4.0T usa 2944);
- **a tabela de menu de fábrica**, morta desde a v43, no `app.bin` (1512 bytes, dos quais a V4.0T usa 1510);
- **a parte morta da tabela de caminhos** (`.sons`), no `app.bin` (472 bytes, dos quais a V4.0T usa 468);
- **os nomes de item de fábrica sem ponteiro** (`.mortos1` a `.mortos10`), no `app.bin`, em 10 pedaços (422 bytes, dos quais a V4.0T usa 396);
- **trechos de fábrica mortos** (`.mortos11` a `.mortos20`), que recebem as rotinas do Morse e do APRS levadas para lá na v156, para o controle remoto caber (1004 bytes, dos quais a V4.0T usa 990);
- **as rotinas de fábrica dos itens que saíram do menu** (`.mortos21` a `.mortos30`), sem chamador desde a v43 (782 bytes, dos quais a V4.0T usa 717).

A tabela de menus fica no lugar do G.729 e do MP3 desde a v43, num vetor de 97 vagas (a V4.0T usa as 97: as 17
pastas e os 80 itens); a original, de 54, virou espaço de código na v140. [docs/engenharia-reversa.md](../engenharia-reversa.md)
explica como medir, onde já foi economizado e por que esses lugares são seguros.

**Prévia da tela B no PC:** `python -I previa/previa.py <pasta>`, de dentro de `firmware/`, grava
cada cenário da tela em PNG. Precisa do zig (`pip install ziglang`), que faz o papel de compilador
de C.

## Estrutura do repositório

```
compilar.bat    compila com dois cliques (Git para Windows e Python 3)
TempraLink-BJ8300.exe  o Tempra Link: programa o rádio pelo cabo (ou, experimental, pelo Bluetooth)
firmware/
  src/          bancos.c (bancos, menus, espectro, cabo), tela.c (a tela principal B) e bancos.ld
  fonte/        a fonte 5x7 da tela B e os ícones, em texto
  previa/       prévia da tela B no PC
  scripts/      montagem (.anc), conferência, pilha, desassemblador, análise
  original/     BJ8300_V0.12.anc — firmware de fábrica
  releases/     .anc prontos para gravar
  build.sh      compila, monta e confere
chirp/          driver do CHIRP (bj8300_v4.0TEMPRA.py)
tools/
  jl-toolchain-2.5.3/  o compilador da JieLi (os 5 executáveis que o build usa)
  anc-bootloader/  gravador BinTe ANC Bootloader (BinTalk V1.1)
  ch340-driver/    driver do cabo USB
  cabo/            ferramentas PowerShell que falam com o rádio
  ghidra-scripts/  scripts usados na análise
logo/           imagem de abertura (BMP) e conversor
tests/          testes do driver do CHIRP
tempralink/     o código do Tempra Link (Delphi), o gerador do menu e os testes
docs/           o manual do Tempra Link, engenharia reversa, histórico, mapa da tela, imagens e notas de trabalho
```
