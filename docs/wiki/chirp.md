# 🔌 O CHIRP com este firmware

<p align="center">
  <a href="chirp.en.md"><img src="../imagens/botao-english.svg" alt="English version" height="56"></a>
</p>

[📖 Wiki](README.md) · [🏠 Página inicial](../../README.md)

## 📻 Como usar o CHIRP com este firmware

> [!CAUTION]
> **Com a V4.1T, use só o módulo `bj8300_v4.1TEMPRA.py` deste release.** Ele substitui os
> anteriores (de `bj8300_v1.0TEMPRA.py` a `bj8300_v3.7TEMPRA.py`). O modelo no CHIRP passou a ser
> **Bajeton → `BJ8300_v4.1TEMPRA`** (até a V2.8T ele ficava em Baojie), e uma imagem `.img` salva com
> um módulo antigo abre nele normalmente. As versões anteriores têm o módulo delas no release de cada
> uma.
>
> - **Não use os modelos de fábrica do CHIRP** que servem para o BJ-8300 de fábrica (**Bajeton
>   BJ7800** e **Radtel RT-920**). Eles não leem nem gravam os bancos, e não conhecem o roger BEEP
>   2 nem as frequências novas do scramble: com o BEEP 2 ligado, a tela de ajustes deles nem abre.
>   O RT-920 ainda oferece o "Zone or Channel", e em modo zona o rádio fica preso, porque o menu
>   para sair dele foi removido.
> - **Não use versões antigas deste módulo**: elas não conhecem as opções novas, e um canal com
>   uma frequência de scramble nova aparece como inválido.
> - Ao contrário também vale: o módulo **recusa** gravar num rádio com o firmware de fábrica. E, como o Tempra
>   Link, ele lê a versão do firmware e só trabalha com a V4.1T: num rádio com a V3.7T ou uma de antes, use o
>   módulo do release dela, ou grave a V4.1T.

**1. Instale o CHIRP mais recente.** Baixe o **CHIRP-next** em
[chirpmyradio.com](https://chirpmyradio.com/projects/chirp/wiki/Download). O módulo usa o driver
`radtel_rt900` da versão atual; com um CHIRP antigo, o passo 3 falha com *"Invalid or unsupported
module file"*.

**2. Ligue o modo desenvolvedor (só uma vez).** Menu **Help** → **Developer Mode**. O CHIRP avisa
*"Danger Ahead"*: clique **Yes**. Depois avisa *"Restart Required"*: clique **OK**, **feche e abra o
CHIRP**. O modo fica ligado nas próximas vezes.

**3. Carregue o módulo (toda vez que abrir o CHIRP).** Menu **File** → **Load Module...** (só
aparece com o modo desenvolvedor). O CHIRP avisa que carregar módulo é perigoso: clique **Yes** e
escolha o `bj8300_v4.1TEMPRA.py`. A janela fica **vermelha** e o título mostra
`CHIRP **Module Loaded**`: é o sinal de que o módulo está carregado. Ao fechar o CHIRP, ele é
descarregado, e na próxima vez é preciso carregar de novo.

**4. Leia o rádio.** Com o rádio ligado normalmente e o cabo conectado, menu **Radio** →
**Download from radio...**:

| Campo | Escolha |
|---|---|
| **Port** | a COM do cabo |
| **Vendor** | **Bajeton** |
| **Model** | **BJ8300_v4.1TEMPRA** (não o BJ7800, que é o modelo de fábrica) |

O CHIRP mostra o aviso *"Experimental driver"*, com as instruções do módulo: clique **Yes** e
depois **OK** para ler.

> [!NOTE]
> Se o `BJ8300_v4.1TEMPRA` não aparece em **Bajeton**, o módulo não está carregado: volte ao passo 3.

**5. Edite.** No lado direito aparece a lista das partes do rádio:

- **Memories (VHF/UHF):** os 960 canais. As colunas **Scramble** e as outras propriedades do canal
  estão aqui.
- **Banks (VHF/UHF):** uma coluna por banco; marque em quais bancos cada canal está. Para
  renomear um banco, **dê duplo clique no cabeçalho da coluna** dele.
- **Settings:** os ajustes do rádio, com o roger de 6 opções e, no **BJ8300_v4.1TEMPRA Specific**,
  logo depois do Noise reduction, o **Expander**, a **Noise gate**, a **De-emphasis**, o **EQ bass (RX)** e o
  **EQ treble (RX)**, o **Mic Gain** (com o DEFAULT, na ordem do rádio), o **Compressor**, o **Mic AGC**, o **EQ bass (TX)** e o
  **EQ treble (TX)**, o **Crystal fine** (em Hz, em 435,5 MHz), o **Brightness**, o **Light PWM** e o
  **Modem gain**, além do **PF3 Key (Long Press)**, logo depois do PF3, e do **Scan threshold**, logo depois do Scan
  Resume Method.
- **Settings → APRS (SMS) e Settings → Morse:** o indicativo, o SSID, o PATH, o destinatário, a mensagem e o DECODE do APRS, e
  a mensagem, a velocidade e o tom do Morse. As duas mensagens são a última enviada, com que as caixas do `#` abrem. Gravar o APRS e o Morse precisa da V3.0T, e o EQ, da V3.1T.
- **Settings → Banks:** um bloco por banco, de **BANK 1** a **BANK 16**:

  | Linha | O que é |
  |---|---|
  | **Name** | o nome do banco (até 12 caracteres), editável aqui também |
  | **Channels** | quantos canais gravados o banco tem (só leitura, contados quando a página abre) |
  | **Enabled** | liga ou desliga o banco |

  Banco sem nenhum canal gravado **não liga**: se você marcar, o CHIRP desmarca na hora. E tirar
  o último canal de um banco o desliga sozinho.
- **Settings → VFO A/B/C Channel:** o **WATCH** (SINGLE, DUAL ou TRIPLE), o **TRIPLE WATCH** e o
  **TX SIDE** em cima, e um bloco para cada lado (A, B e C), cada um com o seu SHOW e o scramble. O **VFO C** tem os mesmos
  campos do A e do B, mais o Work Mode e o canal da C (**CH-C**). Com a C em evidência no rádio, os blocos B e C
  ficam só de leitura: mude pelo rádio, ou aperte A/B nele e baixe de novo.
- **Memories (FM / AM / HF):** as outras faixas, como no driver de fábrica.

> [!NOTE]
> Depois de marcar e desmarcar um checkbox, o CHIRP o desenha em cinza e o rótulo em negrito. Isso
> é só o desenho do CHIRP para "mudou" (acontece com qualquer checkbox, até os do driver de
> fábrica); o valor gravado é o desmarcado.

**6. Grave e reinicie o rádio.** Menu **Radio** → **Upload to radio...**. Depois de gravar,
**desligue e ligue o rádio**: o filtro dos bancos é montado na partida.

Guarde uma cópia do primeiro download (**File** → **Save**) antes de mudar qualquer coisa.

## Como o driver do CHIRP funciona

O arquivo é [chirp/bj8300_v4.1TEMPRA.py](../../chirp/bj8300_v4.1TEMPRA.py). O nome é proposital: só
funciona com **este** firmware, e assim ninguém com um BJ-8300 de fábrica o usa por engano. Ele
deriva do `radtel_rt900.py` do CHIRP, que já trata este rádio como família RT-920/BJ7800, e
acrescenta:

- A aba **Banks** e o grupo **Settings → Banks** (acima). Banco ativo sem canal gravado nunca
  sobe para o rádio: o upload o desliga antes de gravar, como o próprio rádio faz.
- Download e upload da área dos bancos pelo comando estendido, junto com os canais e **numa sessão
  só** (o firmware responde ao `E` de saída com um ACK e só fecha a sessão depois); no upload, só
  os setores que mudaram.
- O roger com as seis opções do rádio e o scramble com as nove, gravando os mesmos valores que o
  menu do rádio grava.
- O WATCH, o CH-C, o SHOW C e o VFO C, gravados no registro do WATCH (`0xD2000`) e no setor dos
  VFOs da C (`0xD3000`). O upload só grava o que muda e confere lendo de volta, e o estado do rádio
  (a C no lado B, e o canal, o modo e o VFO do B) vem do rádio, não da imagem. SINGLE desliga o
  TRIPLE WATCH, como no firmware.
- O **Noise reduction** lido e gravado no byte que o rádio usa (`0x9032`; o driver de base usa
  `0x9031`), e a **Voice Privacy do VFO A**, que o driver de base não gravava.
- O **Mic Gain** no byte `0x9033` do bloco de ajustes, o mesmo que o item MIC GAIN do rádio usa
  (0 HIGH, 1 MID, 2 LOW, 3 DEFAULT; outro valor o rádio lê como DEFAULT), com a lista na ordem do rádio, que sai da
  calibração da imagem, e o **TX SIDE** nos 2 bits do TX-A/B
  (`0x9013`), com o C (3).
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
