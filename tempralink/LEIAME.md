# TempraLink-BJ8300.exe

O Tempra Link do BJ-8300 para Windows, sem instalar nada: lê, edita, guarda e grava pelo cabo de programação os
canais, os bancos, os ajustes do menu, os textos do APRS e do Morse, o DTMF e o logo de abertura de um BJ-8300 com o
firmware TEMPRA de uma versão que ele conhece (abaixo, A compatibilidade), e tenta o mesmo pelo Bluetooth (experimental). É um `.exe` só, sem DLL, de 32
bits. Como usar: [o manual, com as telas](../docs/wiki/tempra-link.md).

É o irmão do Tempra Link do Baofeng UV-17L (no repositório dele), com a mesma janela, e separado de propósito: um
não fala com o outro rádio, e um não abre o backup do outro. O controle remoto, que o firmware ganhou na V4.1T,
e a gravação do firmware têm páginas próprias (abaixo).

## A memória pelo cabo

A serial abre a 460800, a do firmware v160 em diante, e, sem resposta, desce a 115200 (v157 a v159) e a 57600 (o de
fábrica e o TEMPRA até a v156), como o driver do CHIRP.

O `CaboBJ.pas` fala o protocolo de fábrica que o driver do CHIRP usa (seção 4 da engenharia reversa): o magic
`PROGRAMBT11U`, o `F` (16 bytes de identificação), a cifra, o `R` e o `W` de 64 bytes (cifrados com `CO 7` abaixo de
`0xF000`) e o `E`; e o estendido do TEMPRA, com o bit 7 do tamanho ligando o endereço de 24 bits. A sessão, antes de
tudo, lê o cabeçalho dos bancos pelo estendido: o TEMPRA responde 64 bytes, e o de fábrica, o tamanho que recebeu (aí
o programa lê o resto, para o cabo não sair de sincronia, fecha a sessão com o `E` e recusa o rádio).

O `BJ8300.pas` guarda as áreas que o driver do CHIRP lê (os canais, os VFOs A e B, os ajustes, o DTMF, o FM, o modo
e os setores do TEMPRA: o cabeçalho e os 16 bancos, o WATCH, o VFO C, o Morse e o APRS) e a calibração, só para o
backup.

- **Os registros do TEMPRA** têm assinatura e soma (`le_registro` e `grava_registro`, `firmware/src/bancos.c`). Um que
  não confere o rádio lê como zerado, com a assinatura, e o VFO C como cópia do VFO B; aqui é igual, e mudar um campo
  grava o registro inteiro e refaz a soma.
- **A gravação** manda só os blocos de 64 bytes de fábrica que mudaram e os setores do TEMPRA que mudaram, inteiros (o
  primeiro bloco apaga o setor). Antes, ela lê o que o rádio tem: igual ao novo, pula; igual ao lido, grava; nem um
  nem outro, o rádio mudou desde a leitura, e nada se grava (com um backup aberto, grava assim mesmo). Os setores do
  TEMPRA se leem de volta depois, porque um firmware que não grava a área responde o ACK do mesmo jeito. O VFO C não se
  grava com o C no lado B do rádio (TRIPLE): ali ele é do rádio.
- **As regras do driver** antes de gravar (`sync_out`): o rádio sem zonas (`0x9028` = 1), sem o TRIPLE WATCH quando o
  WATCH é SINGLE, e sem banco ligado que não tenha canal.
- **O backup** (`.tempra`) é o mesmo formato do UV-17L, com o modelo `BJ-8300`: um recusa o do outro.

## Os ajustes

O `menu-bj8300.txt` (embutido no `.exe`) descreve cada item do menu: as pastas e os nomes do rádio
(`firmware/scripts/montar.py`), o texto de cada valor e os bits que cada valor põe na memória. Quem o gera é o
`gera_menu.py`, a partir do driver do CHIRP deste repositório, que foi conferido contra o rádio:

```
python -I tempralink/gera_menu.py      (precisa do CHIRP instalado; leva uns 3 minutos)
```

Ele aplica cada opção do ajuste correspondente do driver numa imagem e anota os bits que mudam de uma opção para outra:

- o **byte inteiro** onde o driver grava o byte inteiro (cada opção dá o mesmo byte partindo dele em 00 e em FF), e aí
  um valor fora da lista é outra coisa;
- a **marca** dos bytes que o rádio só lê com ela (o EXPANDER e o DE-EMPHASIS em `0x9034`, o EQ em `0x903A` e `0x903C`, o
  NOISE GATE em `0x9031` e o MODEM GAIN em `0x903D`), para um item não desfazer o vizinho que divide o byte;
- o **valor de fora**: o que o rádio mostra quando o byte não bate com nenhum (o byte sem a marca, o de fábrica de um
  RESET), lido pelo próprio driver.

O texto de cada valor é o do rádio quando a lista do firmware diz a mesma coisa que a do CHIRP; senão, o do CHIRP em
maiúsculas. O MIC GAIN põe o DEFAULT na ordem da calibração de cada rádio (`0xF210`), como o menu. Ficam fora do
`menu-bj8300.txt` o que é ação ou tela (MESSAGES, CH MEMORY, RESET, VERSION, SCAN ADD), os bancos (página própria) e o
RX GAIN, que fica em `0x9040`, fora do bloco que o cabo lê.

## O controle remoto

A página Controle remoto liga o rádio ao PC com ele funcionando normalmente: o teclado do rádio, em que cada tecla fica
apertada enquanto o botão do mouse está apertado nela (segurar 1 s é o toque longo), e o PTT, que transmite enquanto
estiver apertado. Pelo cabo, a tela do rádio vem junto e muda com ela; pelo Bluetooth, só o estado (quem recebe, o
S-meter e a tensão). Antes do primeiro quadro, o `Remoto.pas` confere o firmware numa sessão curta do cabo. O
protocolo está no alto do
`firmware/src/remoto.c` e o que o firmware mostrou na seção 38 da engenharia reversa; o `Remoto.pas`, sem nada de
janela, serve também ao app de Android.

- **A tela.** O rádio manda cada linha que vai ao LCD, mas um redesenho só leva umas 28 linhas (a saída dele não anda
  enquanto a tela se desenha), e o que não coube vem marcado na resposta seguinte. Aí o programa pede a tela inteira
  por fatias, uma por vez, cada uma esperando a resposta: do tamanho que a imagem que ele já tem diz que cabe, a
  começar pela linha seguinte à última que chegou antes da perda (o rádio desenha de cima para baixo). A fatia que
  não coube se parte ao meio, até 4 linhas; a que volta sem nenhuma linha não foi redesenhada (o rádio na TX, ou no
  meio de uma digitação) e se pede de novo.
- **As teclas.** Um clique fica no rádio pelo menos 150 ms, e com uma tecla ou o PTT apertados, e até 200 ms depois de
  soltar, nenhuma fatia vai: o redesenho deixa o rádio uns 65 ms sem ler o teclado, e a leitura dele quer a mesma tecla
  em 3 voltas de 10 ms. Com o remoto ligado, a luz da tela não apaga, porque com ela apagada a primeira tecla só a
  acende.
- **O PTT.** O PTT do BJ é o fio do RX da serial do cabo: os bytes do PC o apertam. O firmware v157 em diante ignora o
  PTT do fio com o remoto ligado pelo cabo, e o programa manda os bytes de reserva em 0xFF, com menos bits 0.

## O firmware

A página Firmware grava o `.anc` pelo cabo (`GravadorBJ.pas`), pelo protocolo do BinTe ANC Bootloader, lido no CIL
dele ([seção 42](../docs/engenharia-reversa.md#42-a-gravação-do-firmware-pelo-cabo-o-protocolo-do-bintalk)): o rádio no
modo UPDATE (ligado segurando o PTT2 e o atalho), a serial a 9600, depois a 10000 e a 115200, e o rádio pedindo os
pedaços do arquivo. Antes, o arquivo se confere inteiro (o tamanho, a camada KDH, todos os CRCs e a marca do hardware
do BJ-8300); o de fábrica passa igual. Ao abrir, o programa procura no GitHub o release mais novo (`Atualizacao.pas`, a
API dos releases, sem login); **Baixar e gravar** baixa o `.anc` dele, confere pelo tamanho e pelo SHA-256 que o
GitHub dá e grava; **Voltar ao de fábrica** usa o `firmware/original/BJ8300_V0.12.anc` ao lado do programa, ou o baixa
do repositório, conferido pelo SHA-256 fixo. Os downloads ficam em `%LOCALAPPDATA%\TempraLink\firmware`. Em qualquer
conversa pelo cabo, e no **Verificar**, o programa diz o firmware do rádio: o de fábrica, um TEMPRA que não diz a
versão (de antes da v170) ou o TEMPRA e a versão. No rádio, a gravação funcionou (a v169, em 19/11).

## A compatibilidade e a atualização

O programa só trabalha com as versões do firmware que conhece: a `FAIXA` do `CaboBJ.pas` (a mesma no app do
celular), cuja ponta de cima é sempre a do `firmware/src/versao.h` (o teste confere). A sessão lê a versão logo
depois de ver que o firmware é TEMPRA (a leitura de `0x3F0000`,
[seção 43](../docs/engenharia-reversa.md#43-a-versão-pelo-cabo-e-a-compatibilidade-v170)) e, fora da faixa, para
com o `EIncompativel` (`Compatibilidade.pas`, igual no Tempra Link do UV-17L) antes de ler ou gravar qualquer
bloco. A janela então oferece o certo: para o de fábrica, o TEMPRA sem versão e o antigo, baixar do GitHub e gravar
o firmware mais novo da faixa; para o mais novo que o programa, o Tempra Link novo. Mudou o que o firmware guarda
ou responde, sobe a versão dele, e a faixa do programa junto.

Ao abrir, o `Atualizacao.pas` lê a lista de releases do GitHub (sem login; rascunho e pré-release de fora) e acha
o firmware mais novo da faixa, o mais novo de todos e o Tempra Link mais novo, pela versão no nome do arquivo
(`BJ8300_BANK_v4.1T.anc`, `TempraLink-BJ8300_v1.0.exe`). Com um programa mais novo que o `VERSAO_TEMPRA_LINK`,
a janela pergunta se atualiza: o novo se baixa, confere pelo tamanho e pelo SHA-256 do GitHub, entra no lugar do
que está aberto (este vira `.antigo`, que o novo apaga ao abrir) e abre. Para isso, cada release leva o `.exe`
com a versão no nome.

## O Bluetooth (experimental)

O `Bluetooth.pas` procura os aparelhos BLE pelo Bluetooth do Windows, conecta no escolhido, escreve no Registro todos os
serviços e características que ele anuncia e usa a característica `FFE1` como cabo: pela leitura do firmware (seção
37 da engenharia reversa), o que se escreve nela cai no mesmo buffer que o cabo enche, e a resposta volta por
notificação. Sem a `FFE1`, ele tenta o primeiro serviço fora dos padrões com uma característica de escrita e uma de
notificação. No rádio, o controle remoto pelo Bluetooth funcionou com o app do celular; a leitura e a gravação por ele
ainda não foram testadas.

## O app do celular (Android)

A pasta `celular/` é o Tempra Link no Android (a versão 1.0, ainda sem APK publicado: o `versionName` do projeto e o
`VERSAO_APP` do `JanelaCelular.pas`), pelo Bluetooth do rádio, com as unidades do programa do Windows para a
memória, o menu, o controle remoto, o Bluetooth, o ROGER CUSTOM e a tela do rádio (a `tela/libtelabj.so`). A primeira
tela mostra os rádios usados antes (`RadiosLembrados.pas`, cada um com o apelido que se der a ele; um toque
conecta, com uma procura que para assim que o rádio aparece) e procura os outros (ou abre a demonstração, com um
BJ-8300 simulado); depois vêm quatro abas: **Rádio** (o controle
remoto, com a tela do rádio e a chave Tela do rádio), **Canais** (os canais e os bancos), **Ajustes** (as pastas do
menu, os textos, os VFOs, o DTMF e o ROGER CUSTOM) e **Mais** (ler, gravar, desfazer, os backups, o aviso de recepção,
trocar de rádio e o registro). O registro (`registro.txt`, na pasta do app) guarda as quedas do remoto, o PTT e os
erros, para o teste longe do PC. Com um firmware de fora da faixa, o app não funciona: avisa e abre a página de
releases, porque o celular não grava firmware.

O `celular\compilar.ps1` compila sem a IDE (o msbuild do Delphi) o APK de depuração, porque no Android64 o Delphi só
faz APK em Debug, e com `-Instala` o instala pelo adb; com `-Windows`, sai um `.exe` para testar as telas no PC com o
rádio simulado. No rádio, só o controle remoto foi testado.

## Compilar e testar

Dois cliques no `compilar.bat`: ele roda os testes e, se passarem, gera o `TempraLink-BJ8300.exe` na raiz do
repositório. Precisa do Delphi (feito no RAD Studio 12; outra versão: defina `BDS` com a pasta dela). Com o Python e o
CHIRP instalados, os testes conferem também contra o driver do CHIRP:

- o `testes\fixtures.py gera` monta um BJ-8300 pelo próprio driver (canais com tons, deslocamentos, AM e sem TX, bancos,
  ajustes, textos, VFOs e as memórias do rádio de AM, FM e SSB, pelos sub-rádios) e escreve a memória dele (`radio.bin`) e o que se tem de ler dela (`esperado.txt`);
- o `testes\TesteTempraLink.dpr` lê essa memória pelo rádio simulado (`testes\SimuladorBJ.pas`, que segue a seção 4 e grava
  como a flash do rádio: o bloco que começa um setor o apaga, e os outros só zeram bits) e confere linha a linha; grava
  um conjunto de mudanças, confere o rádio inteiro byte a byte e guarda o resultado (`gravado.bin`); e confere as recusas (o
  firmware de fábrica, o rádio mudo, o rádio que mudou, o firmware que confirma e não guarda, o VFO C com o C no lado
  B), os registros apagados, o backup, o CSV e o logo; o controle remoto, com o rádio simulado falando o protocolo
  dele (as teclas, o PTT, a tela por fatias, a perda que chega no meio da tela inteira, a TX, o clique curto e a tecla
  segurada); as três velocidades do cabo e o firmware que o cabo mostra; e a gravação do firmware, contra um rádio
  simulado no modo UPDATE (`testes\SimuladorUpdate.pas`), com os pacotes do próprio BinTalk, a conferência dos dois
  `.anc` (o de fábrica e o da release) e a leitura do release do GitHub (`testes\github-release.json`);
- o `testes\fixtures.py confere` lê o `gravado.bin` pelo driver e confere as mudanças.

Sem o CHIRP, os testes usam o `radio.bin` e o `esperado.txt` que já estão no repositório.

## Arquivos

```
TempraLink.dpr       o programa
Principal.pas/.dfm   a janela: as páginas, a edição na célula e as tarefas na thread
Visual.pas           o tema escuro, os botões e a marca "((T))" (o mesmo do UV-17L)
BJ8300.pas           a memória do rádio, os canais, os bancos, os ajustes, os textos, o DTMF, o logo e o backup
Ajustes.pas          o menu a partir do menu-bj8300.txt
CaboBJ.pas           o protocolo do cabo
GravadorBJ.pas       a gravação do firmware e a conferência do .anc (sem janela)
Atualizacao.pas      os releases do GitHub, o download conferido e a troca do programa (o mesmo do UV-17L)
Compatibilidade.pas  a faixa de versões do firmware e a recusa do que está fora dela (o mesmo do UV-17L)
Remoto.pas           o controle remoto: os quadros, o estado, a tela por fatias (sem janela)
Bluetooth.pas        o Bluetooth LE (experimental)
Transporte.pas       o caminho até o rádio, que a serial, o Bluetooth e o simulado seguem
PortaSerial.pas      a serial pela API do Windows e a lista das portas (o mesmo do UV-17L)
Logo.pas             a imagem no formato do rádio, 160 x 138 em RGB565
Csv.pas              o CSV do CHIRP
Idioma.pas           português num Windows em português, inglês nos outros
Recursos.rc          o manifesto, o ícone, o menu, o estilo e a versão
gera_menu.py         gera o menu-bj8300.txt a partir do driver do CHIRP
menu-bj8300.txt      a descrição do menu
testes/              os testes, o rádio simulado e o lado do CHIRP
celular/             o app do Android (TempraLinkCelular.dpr; compila pelo compilar.ps1)
```

---

**In English, in short:** Tempra Link for the BJ-8300 reads, edits, keeps and writes a TEMPRA-firmware BJ-8300 through
the programming cable (and, experimentally, Bluetooth LE through the `FFE1` characteristic, also from an Android app
in `celular/`): channels, banks, menu
settings, APRS and Morse texts, DTMF and the boot logo (the logo read only); it is also a remote control, with the radio's screen live over the cable. It only works
with the firmware versions it knows (the radio tells its version since v170), offers the right one otherwise,
and updates itself from the GitHub releases. The menu description is generated from
this repository's CHIRP driver by `gera_menu.py`, and the tests check, both ways, that the program and the driver read
the same memory the same way. Build with `compilar.bat` (Delphi, RAD Studio 12).
