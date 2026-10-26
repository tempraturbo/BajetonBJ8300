# Baojie BJ-8300 — firmware v1.0T, bancos de canais e driver do CHIRP

<p align="center">
  <a href="README.en.md"><img src="docs/imagens/botao-english.svg" alt="English version" height="56"></a>
</p>

Firmware modificado para o rádio **Baojie/Bajeton BJ-8300** (JieLi AC695X, CPU pi32v2), feito por
engenharia reversa do firmware de fábrica **V0.12**, sem documentação do fabricante. Traz **16
bancos de canais de verdade**, scramble compatível com o Baofeng UV-17L, espectro mais rápido e um
**driver do CHIRP** próprio. O "T" da versão é de **Tempra**.

> [!TIP]
> ## 🎉 v1.0T — a primeira versão estável
>
> **[⬇️ Baixar a v1.0T na página de Releases](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v1.0T)**
>
> | Arquivo do release | Para que serve |
> |---|---|
> | `BJ8300_BANK_v1.0T.anc` | o firmware, para gravar no rádio |
> | `bj8300_v1.0TEMPRA.py` | o módulo do CHIRP para este firmware |
> | `BinTalk-V1.1.zip` | o gravador (BinTe ANC Bootloader) |
> | `CH340-driver.zip` | o driver do cabo de programação USB |
>
> Testada no rádio. Para gravar: [passo a passo](#-como-gravar-o-firmware-passo-a-passo). Para o
> CHIRP: [passo a passo](#-como-usar-o-chirp-com-este-firmware).

💚 **Gostou do projeto?** [Apoie com uma doação pelo PayPal](#-apoie-o-projeto) — qualquer valor
ajuda a próxima versão a sair.

---

## ✨ O que mudou da V0.12 de fábrica para a v1.0T

### 🆕 Recursos novos

| Recurso | Como fica no rádio |
|---|---|
| 🗂️ **16 bancos de canais** | Cada banco tem **nome de 12 caracteres**, e um canal pode estar em **quantos bancos quiser**. Dá para ligar **vários bancos ao mesmo tempo**: o sobe/desce e a busca passam a ver só os canais dos bancos ligados. Sem nenhum ligado, o rádio funciona como de fábrica. Menus novos: `BANK`, `BANK NAME` e `BANK CH` ([como usar](#usando-os-bancos-no-rádio)) |
| 🔐 **Scramble com 8 frequências** | O `SCRAMBLE` passa de OFF/ON para **OFF e de 2700 a 3400 Hz, de 100 em 100**, em ordem crescente. O **3400 Hz** é o ON de fábrica (fala com outros BJ-8300), e os **modos 1, 2 e 3 do Baofeng UV-17L** caem, cada um, numa dessas frequências. Testado com os três modos |
| 🔔 **Roger BEEP 2** | O `ROGER` ganha uma quarta opção: OFF, BEEP 1 (o de fábrica), **BEEP 2** (quatro tons: 1125, 1857, 1359 e 931 Hz, ~0,4 s no total) e TONE1200 |
| 📊 **Espectro com 320 e 640 pontos** e **passo de 10 kHz** | Além dos 160 pontos de fábrica, para ver uma faixa maior de uma vez |
| 📈 **Pico seguro no espectro** | A barra sobe na hora e, quando o sinal some, desce aos poucos |
| 🔌 **Driver do CHIRP próprio** | Lê e grava os bancos (aba **Banks**), os nomes, o roger e o scramble novos, numa sessão só pelo cabo. Recusa gravar num rádio com firmware de fábrica ([como usar](#-como-usar-o-chirp-com-este-firmware)) |
| 🧰 **Comando estendido no cabo** | Leitura do chip de dados inteiro (endereço de 24 bits) e gravação só na área dos bancos. É o que o driver do CHIRP usa |

### ⚡ Melhorias

| O que | Detalhe |
|---|---|
| 🚀 **Espectro duas vezes mais rápido** | 160 pontos em ≈ 2,5 s. E, parado num sinal achado, a seta continua a varredura sem mexer no limite |
| 🟩 **Realce do VFO selecionado** | O bloco do lado ativo (nome, frequência e barra de sinal) ganha fundo verde; o outro continua preto |
| ↕️ **Setas do menu invertidas** | A seta de cima passa a subir na lista, em vez de avançar o valor |
| 🔤 **Textos em inglês revisados** | 25 cadeias corrigidas e padronizadas (ex.: `VOICEPRI` → `ENCRYPT`). O menu `VERSION` mostra `Firmware: V1.0T` |

### 🐞 Correções

| Problema da V0.12 | Na v1.0T |
|---|---|
| Durante o scan, a barra de sinal ficava congelada no último canal achado, e o número do canal descasava do nome | A tela acompanha cada canal da varredura |

### ➖ Removido

| O que | Por quê |
|---|---|
| Itens de menu `ZONE` e `Zone Or Channel` | Os bancos fazem o papel das zonas, com mais liberdade. A função de zona continua no firmware, e o driver do CHIRP sempre grava **Full Channel**, para o rádio nunca ficar preso em modo zona |

### 🔒 O que não muda

- **Canais, ajustes e calibração** ficam num chip à parte e **não são tocados** pela gravação do
  firmware.
- O **modo de gravação** (`UPDATE`) é lido antes de qualquer parte modificada, e o script de
  montagem recusa escrever nele.
- **Voltar ao de fábrica** é só gravar o [firmware original](firmware/original/), do mesmo jeito.

---

## ⚠️ Antes de gravar

Gravar firmware modificado tem risco. Este projeto tem uma rede de segurança, **testada no
rádio**: ligar o rádio segurando **PTT2 + o botão de atalho** mostra `UPDATE` na tela e deixa ele
esperando gravação. Esse caminho roda na inicialização, **antes** de qualquer coisa que a
modificação toca (o script de montagem recusa escrever nas funções envolvidas; ver `PROTEGIDAS` em
[firmware/scripts/montar.py](firmware/scripts/montar.py)). Durante o desenvolvimento o rádio
entrou em bootloop uma vez ([histórico](docs/historico.md)) e voltou por esse combo, sem abrir o
aparelho.

O gravador também tem o botão *"Click Here If Your Radio Can't Work"*, que deve servir para quando
o firmware nem chega a ligar, mas ele **nunca foi testado**.

**Guarde o firmware de fábrica**: [firmware/original/BJ8300_V0.12.anc](firmware/original/).

---

## 📥 Como gravar o firmware, passo a passo

Precisa de um PC com Windows e do **cabo de programação USB** do rádio (o de chip CH340, o mesmo do
CPS de fábrica).

**1. Baixe os arquivos.** Na [página do release v1.0T](https://github.com/tempraturbo/BajetonBJ8300/releases/tag/v1.0T),
baixe o `BJ8300_BANK_v1.0T.anc`, o `BinTalk-V1.1.zip` e, se o Windows ainda não reconhece o cabo,
o `CH340-driver.zip`. Descompacte os zips. (Os mesmos arquivos estão no repositório, em
`firmware/releases/`, `tools/anc-bootloader/` e `tools/ch340-driver/`.)

**2. Instale o driver do cabo, se precisar.** Ligue o cabo no PC e abra o **Gerenciador de
Dispositivos**. Se em **Portas (COM e LPT)** aparece `USB-SERIAL CH340 (COMx)`, está pronto: anote
o número da COM. Se não aparece, clique com o botão direito no `ch341ser.inf` do `CH340-driver.zip`
→ **Instalar** (ou, num PowerShell **como administrador**, `pnputil /add-driver ch341ser.inf
/install`) e desconecte e conecte o cabo.

**3. Abra o gravador.** Rode o `BinTe ANC Bootloader.exe` (a janela se chama **BinTalk V1.1**).
Feche antes qualquer programa que use a porta COM (CHIRP, CPS).

<p align="center"><img src="docs/imagens/gravador.png" alt="Janela do gravador BinTalk V1.1" width="300"></p>

- No campo de cima, clique em **`...`** e escolha o `BJ8300_BANK_v1.0T.anc`.
- **Port:** a COM do cabo (a do passo 2).
- **Baudrate:** **115200** (já vem assim).

**4. Ponha o rádio em modo de gravação.** Com o rádio **desligado** e o cabo conectado nele, ligue
o rádio **segurando PTT2 + o botão de atalho**, até aparecer `UPDATE` na tela.

**5. Grave.** Clique em **Update** e espere. O log mostra `Update Mode` → `Baudrate OK` →
`Update 3%` → `Jump To Loader` → `Progress 100%` → `Finish`. Não desconecte nada no meio.

**6. Confira.** Desligue e ligue o rádio. No menu `VERSION` deve aparecer `Firmware: V1.0T`, e os
itens `BANK`, `BANK NAME` e `BANK CH` aparecem no menu.

**Se der `Retry1..5 / Failure!`:** desligue o rádio, confira que nenhum outro programa está com a
porta COM, volte ao passo 4 e tente de novo.

---

## 📻 Como usar o CHIRP com este firmware

> [!CAUTION]
> **Com a v1.0T, use só o módulo `bj8300_v1.0TEMPRA.py` deste release.**
>
> - **Não use os modelos de fábrica do CHIRP** que servem para o BJ-8300 de fábrica (**Bajeton
>   BJ7800** e **Radtel RT-920**). Eles não leem nem gravam os bancos, e não conhecem o roger BEEP
>   2 nem as frequências novas do scramble: com o BEEP 2 ligado, a tela de ajustes deles nem abre.
>   O RT-920 ainda oferece o "Zone or Channel", e em modo zona o rádio fica preso, porque o menu
>   para sair dele foi removido.
> - **Não use versões antigas deste módulo**: elas não conhecem as opções novas, e um canal com
>   uma frequência de scramble nova aparece como inválido.
> - Ao contrário também vale: o módulo **recusa** gravar num rádio com o firmware de fábrica.

**1. Instale o CHIRP mais recente.** Baixe o **CHIRP-next** em
[chirpmyradio.com](https://chirpmyradio.com/projects/chirp/wiki/Download). O módulo usa o driver
`radtel_rt900` da versão atual; com um CHIRP antigo, o passo 3 falha com *"Invalid or unsupported
module file"*.

**2. Ligue o modo desenvolvedor (só uma vez).** Menu **Help** → **Developer Mode**. O CHIRP avisa
*"Danger Ahead"*: clique **Yes**. Depois avisa *"Restart Required"*: clique **OK**, **feche e abra o
CHIRP**. O modo fica ligado nas próximas vezes.

**3. Carregue o módulo (toda vez que abrir o CHIRP).** Menu **File** → **Load Module...** (só
aparece com o modo desenvolvedor). O CHIRP avisa que carregar módulo é perigoso: clique **Yes** e
escolha o `bj8300_v1.0TEMPRA.py`. A janela fica **vermelha** e o título mostra
`CHIRP **Module Loaded**`: é o sinal de que o módulo está carregado. Ao fechar o CHIRP, ele é
descarregado, e na próxima vez é preciso carregar de novo.

**4. Leia o rádio.** Com o rádio ligado normalmente e o cabo conectado, menu **Radio** →
**Download from radio...**:

| Campo | Escolha |
|---|---|
| **Port** | a COM do cabo |
| **Vendor** | **Baojie** (não é "Bajeton") |
| **Model** | **BJ8300_v1.0TEMPRA** |

O CHIRP mostra o aviso *"Experimental driver"*, com as instruções do módulo: clique **Yes** e
depois **OK** para ler.

> [!NOTE]
> Se o `BJ8300_v1.0TEMPRA` não aparece em **Baojie**, o módulo não está carregado: volte ao passo 3.

**5. Edite.** No lado direito aparece a lista das partes do rádio:

- **Memories (VHF/UHF):** os 960 canais. As colunas **Scramble** e as outras propriedades do canal
  estão aqui.
- **Banks (VHF/UHF):** uma coluna por banco; marque em quais bancos cada canal está. Para
  renomear um banco, **dê duplo clique no cabeçalho da coluna** dele.
- **Settings:** os ajustes do rádio, com o roger de 4 opções e o scramble dos VFO A e B.
- **Settings → Banks:** um bloco por banco, de **BANK 1** a **BANK 16**:

  | Linha | O que é |
  |---|---|
  | **Name** | o nome do banco (até 12 caracteres), editável aqui também |
  | **Channels** | quantos canais gravados o banco tem (só leitura, contados quando a página abre) |
  | **Enabled** | liga ou desliga o banco |

  Banco sem nenhum canal gravado **não liga**: se você marcar, o CHIRP desmarca na hora. E tirar
  o último canal de um banco o desliga sozinho. No fim da página, **Bank edited by the radio
  menu** escolhe o banco em que os itens `BANK NAME` e `BANK CH` do rádio mexem.
- **Memories (FM / AM / HF):** as outras faixas, como no driver de fábrica.

> [!NOTE]
> Depois de marcar e desmarcar um checkbox, o CHIRP o desenha em cinza e o rótulo em negrito. Isso
> é só o desenho do CHIRP para "mudou" (acontece com qualquer checkbox, até os do driver de
> fábrica); o valor gravado é o desmarcado.

**6. Grave e reinicie o rádio.** Menu **Radio** → **Upload to radio...**. Depois de gravar,
**desligue e ligue o rádio**: o filtro dos bancos é montado na partida.

Guarde uma cópia do primeiro download (**File** → **Save**) antes de mudar qualquer coisa.

---

## Usando os bancos no rádio

No menu aparecem três itens juntos, no lugar onde ficavam os de zona:

| Item | O que faz |
|---|---|
| **BANK** | As setas passam pelos 16 bancos; mostra `n nome` e ` ON` se estiver ligado. **OK liga/desliga** o banco e o escolhe para os outros dois itens |
| **BANK NAME** | Dá nome (12 caracteres) ao banco escolhido, no mesmo editor de texto do `CH-NAME` |
| **BANK CH** | Percorre os 960 canais mostrando `banco-canal`; **OK marca/desmarca** o canal no banco escolhido. ` IN` = está no banco, ` -` = canal vazio na memória |

Detalhes de comportamento:

- Ligar um banco que **não tem nenhum canal gravado** não filtra nada (o rádio ficaria sem canal
  para mostrar) — o firmware desfaz sozinho.
- Marcar ou desmarcar canal **vale na hora**, sem reiniciar.
- Gravar bancos **pelo cabo** (CHIRP) exige desligar e ligar o rádio: o filtro é montado na partida.
- O estado dos bancos fica gravado no chip de dados e **sobrevive a desligar o rádio**.
- Regravar os canais pelo CHIRP ou pelo CPS **não apaga banco nenhum**: os bancos ficam no chip de
  dados (0xC1000 em diante), fora da área de canais.

Na tela inicial a aparência não muda: a numeração continua **absoluta, de 1 a 960** (o canal 500
continua sendo o 500, não "o 3º do banco") e o nome do banco ligado não aparece ali. O que muda é
só o que **o sobe/desce e a busca enxergam** — com banco ligado, eles pulam direto de um canal
marcado para o outro.

---

## Como o driver do CHIRP funciona

O arquivo é [chirp/bj8300_v1.0TEMPRA.py](chirp/bj8300_v1.0TEMPRA.py). O nome é proposital: só
funciona com **este** firmware, e assim ninguém com um BJ-8300 de fábrica o usa por engano. Ele
deriva do `radtel_rt900.py` do CHIRP, que já trata este rádio como família RT-920/BJ7800, e
acrescenta:

- A aba **Banks** e o grupo **Settings → Banks** (acima). Banco ativo sem canal gravado nunca
  sobe para o rádio: o upload o desliga antes de gravar, como o próprio rádio faz.
- Download e upload da área dos bancos pelo comando estendido, junto com os canais e **numa sessão
  só** (o firmware responde ao `E` de saída com um ACK e só fecha a sessão depois); no upload, só
  os setores que mudaram.
- O roger com as quatro opções do rádio e o scramble com as nove, gravando os mesmos valores que o
  menu do rádio grava.
- **Sem zonas:** todo upload grava **Full Channel** (`0x9028` = 1), porque o modo zona continua no
  firmware e, sem o item de menu, o rádio não teria como sair dele — em modo zona o sobe/desce fica
  preso em 64 canais.
- A trava contra firmware de fábrica: o firmware dos bancos responde à leitura estendida com tamanho
  `0x40`, e o de fábrica ecoa o tamanho com o bit 7 ligado e manda mais de 128 bytes. O driver lê
  o cabeçalho, descarta o resto (o cabo não dessincroniza) e para **antes de gravar qualquer
  coisa**.

Três conjuntos de teste, os três sem rádio:

- `python -I tests/teste_bj8300.py` — o miolo dos bancos (formato gravado, soma batendo com a que
  o próprio rádio gravou, pertinência múltipla), com o CHIRP stubado;
- `python -I tests/teste_chirp_real.py` — **contra o CHIRP instalado de verdade**: se a classe
  encaixa na API (modelo de bancos, `apply_to` + callbacks dos ajustes), se imagem salva pelo
  driver RT-920 abre sem estourar, e se roger e scramble terminam nos bytes que o rádio grava;
- `python -I tests/teste_clonagem.py` — a **conversa pelo cabo**: `sync_in` e `sync_out` inteiros
  contra um rádio simulado que responde como o firmware (lido no desmontado), nos dois firmwares.

A elétrica do cabo e o tempo de resposta do rádio só o rádio real testa — isso foi validado à mão
com as ferramentas de `tools/cabo/`.

---

## Versões anteriores

Todas em [firmware/releases/](firmware/releases/). A história completa, com o que quebrou e por
quê, está no [histórico](docs/historico.md).

| Arquivo | Estado |
|---|---|
| `BJ8300_BANK_v36.anc` | **a mais nova** — v34 + a tela principal nova (tema B): cartão do lado em evidência com a frequência grande e o medidor, etiquetas coloridas de potência, modulação e tons, e a barra de cima só com o que está ligado; no idioma chinês a tela fica a de fábrica — testado no rádio |
| `BJ8300_BANK_v34.anc` | v32 + o espectro no espaço das vozes, o pico em verde e o dBm inteiro na troca de AM/FM; a última com a tela principal de fábrica — testado no rádio |
| `BJ8300_BANK_v32.anc` | v27 sem as vozes (e sem o item VOICE), roger COIN (a moeda), leitura de memória pelo cabo; abre ~112 KB de espaço para o tema novo da tela — testado no rádio |
| `BJ8300_BANK_v27.anc` | v1.0T + espectro com a barra atual em branco, a frequência que roda, a linha do pico e o dBm à direita; roger BEEP 3 (a moeda, chamada de COIN desde a v31) — testado no rádio |
| `BJ8300_BANK_v1.0T.anc` | **estável** — a v25 com o nome da versão (mesmo arquivo); use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v24.anc` | v23 + scramble 2900 Hz (o do Baofeng UV-17L no modo 3) — testado no rádio |
| `BJ8300_BANK_v23.anc` | v21 + roger BEEP 2 — testado no rádio |
| `BJ8300_BANK_v21.anc` | v19 + espectro duas vezes mais rápido, passo de 10 kHz, 320 e 640 pontos e pico seguro; verde do realce mais escuro (a v20 sem o bootloop; a v20 foi retirada) — testado no rádio |
| `BJ8300_BANK_v19.anc` | v18 + a seta continua o espectro parado num achado; a tabela de menus no lugar da original — testado no rádio |
| `BJ8300_BANK_v18.anc` | v17 com o fundo verde também na caixinha do RX/TX — não testado no rádio |
| `BJ8300_BANK_v17.anc` | realce do bloco do VFO sem as faixas pretas ([mapa da tela](docs/mapa-da-tela.md)) — testado no rádio |
| `BJ8300_BANK_v16.anc` | primeiro realce funcionando; sobram faixas finas pretas entre as linhas do bloco |
| `BJ8300_BANK_v14.anc` | tentativa anterior do realce: o verde só saía na barra de sinal |
| `BJ8300_BANK_v12.anc` | bancos, setas, textos, correção do scan e o cabo de 24 bits — testado no rádio |
| `firmware/original/BJ8300_V0.12.anc` | firmware de fábrica, para voltar atrás |

---

## Ferramentas pelo cabo

Em [tools/cabo/](tools/cabo/), em PowerShell, sem instalar nada:

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

---

## Compilar

Precisa da toolchain da JieLi (clang para `pi32v2`) e de Python 3:

```sh
sh firmware/build.sh
```

Ele compila `firmware/src/bancos.c` e `firmware/src/tela.c`, monta o `.anc` a partir do original e **confere** o
resultado: CRCs das 12 entradas, código e tabela de menus presentes e iguais nas 6 imagens de
flash, itens de menu, todos os ganchos e remendos, textos, e que nenhum byte foi alterado fora dos
trechos previstos — nem perto das áreas protegidas. A saída termina em `RESULTADO: tudo certo`, e
o build é reprodutível: o mesmo fonte dá o mesmo `.anc`, byte a byte.

O caminho da toolchain está em `firmware/build.sh` (`C:/tools/jl-toolchain-2.5.3/...`).

**Orçamento de espaço:** o código novo mora em três lugares, todos no mesmo endereço nas 6 imagens:

- **a área livre do fim da imagem**, a partir de `0x01E96124`, com 3260 bytes (a v36 usa 2144);
- **o espaço que era das vozes**, com o espectro, mas só dentro dos arquivos C0 e C1 (1066 bytes),
  porque o carregador do rádio recusa a imagem que muda mais arquivos de voz;
- **o lugar do decodificador G.729**, dentro do `app.bin`, com a tela principal B (8424 bytes, dos
  quais ela usa 5636).

A tabela de menus é gravada no lugar da original. [docs/engenharia-reversa.md](docs/engenharia-reversa.md)
explica como medir, onde já foi economizado e por que esses lugares são seguros.

**Prévia da tela B no PC:** `python -I previa/previa.py <pasta>`, de dentro de `firmware/`, grava
cada cenário da tela em PNG. Precisa do zig (`pip install ziglang`), que faz o papel de compilador
de C.

---

## Estrutura do repositório

```
firmware/
  src/          bancos.c (bancos, menus, espectro, cabo), tela.c (a tela principal B) e bancos.ld
  fonte/        a fonte 5x7 da tela B e os ícones, em texto
  previa/       prévia da tela B no PC
  scripts/      montagem (.anc), conferência, desassemblador, análise
  original/     BJ8300_V0.12.anc — firmware de fábrica
  releases/     .anc prontos para gravar
  build.sh      compila, monta e confere
chirp/          driver do CHIRP (bj8300_v1.0TEMPRA.py)
tools/
  anc-bootloader/  gravador BinTe ANC Bootloader (BinTalk V1.1)
  ch340-driver/    driver do cabo USB
  cabo/            ferramentas PowerShell que falam com o rádio
  ghidra-scripts/  scripts usados na análise
logo/           imagem de abertura (BMP) e conversor
tests/          testes do driver do CHIRP
docs/           engenharia reversa, histórico, mapa da tela, imagens e notas de trabalho
```

---

## 💚 Apoie o projeto

A v1.0T nasceu de muitas horas de engenharia reversa: o firmware de fábrica aberto byte a byte, sem
nenhuma documentação do fabricante, cada rotina mapeada no desmontado e 25 versões, quase todas
gravadas e testadas num rádio de verdade, até chegar a uma versão estável. Tudo aqui é e vai
continuar **livre e gratuito**, para usar, estudar e modificar.

Se o seu BJ-8300 ficou melhor — os bancos organizaram seus canais, o scramble passou a falar com o
Baofeng do colega, o espectro ficou mais rápido —, considere pagar um café para o projeto. Qualquer
valor ajuda: é tempo para novas funções, equipamento para testar e a próxima versão saindo.

<p align="center">
  <a href="https://www.paypal.com/donate/?hosted_button_id=2VPWYBMKBSJX6"><img src="https://img.shields.io/badge/Doar%20com-PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white" alt="Doar com PayPal"></a>
  <br><br>
  <a href="https://www.paypal.com/donate/?hosted_button_id=2VPWYBMKBSJX6"><img src="docs/imagens/doacao-paypal.png" alt="QR code para doar pelo PayPal" width="128"></a>
  <br>
  <sub>Aponte a câmera do celular para o QR code, ou clique no botão.</sub>
</p>

**Obrigado!** Cada doação é um sinal de que vale a pena continuar. 📻

---

## Créditos e licença

- O driver do CHIRP **deriva** do `chirp/drivers/radtel_rt900.py` (Copyright 2025, 2026 Fred
  Trimble; 2024 Pavel Moravec, OK2MOP; 2023 Jim Unroe), distribuído sob **GPL-2.0-or-later** — a
  mesma licença se aplica a ele.
- O firmware de fábrica (e a parte dele dentro das releases `.anc`), o gravador BinTe, o driver
  CH340 (WCH) e o logo `bajeton` são dos respectivos fabricantes e estão aqui só para
  reprodutibilidade e recuperação.
- O restante (código dos bancos, scripts, ferramentas e documentação) é de **tempraturbo**, sob a
  licença **[0BSD](LICENSE)**: use, copie, modifique e distribua para o que quiser, sem precisar
  nem dar crédito.

Sem garantia de espécie alguma. Você é responsável pelo que grava no seu rádio.
