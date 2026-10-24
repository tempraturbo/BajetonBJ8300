# Baojie BJ-8300 — bancos de canais, driver do CHIRP e ferramentas

> 🇬🇧 **English version: [README.en.md](README.en.md)**

Firmware modificado para o rádio **Baojie/Bajeton BJ-8300** (JieLi AC695X, CPU pi32v2), com
**16 bancos de canais de verdade**, um **driver do CHIRP** que edita esses bancos e um conjunto de
ferramentas para falar com o rádio pelo cabo de programação.

Tudo aqui foi obtido por engenharia reversa do firmware de fábrica **V0.12**, sem documentação do
fabricante. O que foi descoberto está em [docs/engenharia-reversa.md](docs/engenharia-reversa.md) —
inclusive o mapa da memória de dados, o protocolo do cabo e os endereços das rotinas usadas.

---

## ⚠️ Antes de qualquer coisa

Gravar firmware modificado tem risco. Este projeto foi feito com duas redes de segurança, e as
duas foram **testadas no rádio**:

1. **Combo de recuperação** — ligar o rádio segurando **PTT2 + o botão de atalho** mostra `UPDATE`
   na tela e deixa ele esperando gravação. Esse caminho é lido na inicialização, **antes** de
   qualquer coisa que a modificação toca (o script de montagem recusa escrever nas funções
   envolvidas; ver `PROTEGIDAS` em [firmware/scripts/montar.py](firmware/scripts/montar.py)).
2. **Recuperação pelo bootloader** — o botão *"Click Here If Your Radio Can't Work"* do BinTe ANC
   Bootloader, que não depende do app estar bom.

Durante o desenvolvimento o rádio entrou em bootloop uma vez (ver
[docs/historico.md](docs/historico.md)) e voltou pelo combo, sem abrir o aparelho.

**Guarde o firmware de fábrica**: está em
[firmware/original/BJ8300_V0.12.anc](firmware/original/). É só regravá-lo para voltar ao estado
original — os canais e a calibração ficam em outro chip e não são tocados.

---

## O que o firmware modificado faz

### Bancos de canais (o motivo do projeto)

- **16 bancos**, cada um com **nome de 12 caracteres**.
- Um canal pode estar em **quantos bancos quiser**.
- **Vários bancos ligados ao mesmo tempo**: o rádio passa a ver a união deles.
- Com algum banco ligado, **sobe/desce e busca só enxergam os canais dos bancos ligados**; sem
  nenhum ligado, o rádio funciona exatamente como de fábrica.
- Os bancos ficam no chip de dados (0xC1000 em diante), **fora** da área de canais: regravar os
  canais pelo CHIRP ou pelo CPS não apaga banco nenhum.

### Outras correções e melhorias

| O que | Detalhe |
|---|---|
| **Realce do VFO selecionado** | O bloco do lado ativo (nome, frequência e barra de sinal) ganha fundo verde; o outro continua preto |
| **Setas do menu invertidas** | A seta de cima passa a subir na lista, em vez de avançar o valor |
| **Barra de sinal no scan** | Durante a varredura a barra não fica mais congelada no último canal achado, e o número do canal não descasa do nome |
| **Textos em inglês revisados** | 25 cadeias corrigidas/padronizadas (ex.: `VOICEPRI` → `ENCRYPT`); versão do menu agora é `V1.0T` |
| **Editor de nome** | Sair do editor não deixa mais o `abc` do teclado preso no cabeçalho do menu |
| **Comando estendido no cabo** | Leitura de 24 bits (chip inteiro) e gravação na área dos bancos — é o que o driver do CHIRP usa |
| **Menus de zona removidos** | `ZONE` e `Zone Or Channel` saíram do menu, por escolha explícita: o banco faz o papel deles e os 56 bytes liberados são o que fez o realce caber |

A **função** de zona continua no firmware; o que saiu foram os dois itens de menu.

---

## Como gravar

1. Abra `tools/anc-bootloader/BinTe ANC Bootloader.exe`.
2. Escolha o arquivo `.anc`, a porta COM e **115200**.

   | Arquivo | Estado |
   |---|---|
   | `firmware/releases/BJ8300_BANK_v12.anc` | **recomendado** — testado e íntegro |
   | `firmware/releases/BJ8300_BANK_v18.anc` | v17 com o fundo verde também na caixinha do RX/TX; **ainda não testado no rádio** |
   | `firmware/releases/BJ8300_BANK_v17.anc` | **realce do bloco do VFO em evidência** sem as faixas pretas e com verde mais escuro ([mapa da tela](docs/mapa-da-tela.md)) — **testado no rádio**; só o RX/TX fica com fundo preto |
   | `firmware/releases/BJ8300_BANK_v16.anc` | primeiro realce testado e funcionando; sobram faixas finas pretas entre as linhas do bloco |
   | `firmware/releases/BJ8300_BANK_v14.anc` | tentativa anterior do realce: parou o reset da v13, mas o verde só saía na barra de sinal |
   | `firmware/original/BJ8300_V0.12.anc` | firmware de fábrica, para voltar atrás |
3. Ligue o rádio **segurando PTT2 + o botão de atalho** até aparecer `UPDATE` (mais confiável), ou
   deixe ele na tela normal e clique **Update**.
4. Clique **Update** e espere terminar.

Se der `Retry1..5 / Failure!`: desligue e ligue o rádio, confira que nenhum outro programa está com
a porta COM (CHIRP, CPS, scripts deste repositório) e tente de novo. O driver do cabo CH340 está em
[tools/ch340-driver/](tools/ch340-driver/) — instale com
`pnputil /add-driver CH341SER.INF /install` num PowerShell **elevado**.

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

Na tela inicial a aparência não muda: a numeração continua **absoluta, de 1 a 960** (o canal 500
continua sendo o 500, não "o 3º do banco") e o nome do banco ligado não aparece ali. O que muda é
só o que **o sobe/desce e a busca enxergam** — com banco ligado, eles pulam direto de um canal
marcado para o outro.

---

## Driver do CHIRP

O arquivo é [chirp/bj8300_v1.0TEMPRA.py](chirp/bj8300_v1.0TEMPRA.py). O nome é proposital: só
funciona com **este** firmware modificado, e assim ninguém com um BJ-8300 de fábrica o usa por
engano.

1. No CHIRP, ligue o **modo desenvolvedor** (`Help` → `Enable Developer Functions`) e reinicie.
2. `File` → `Load Module...` → escolha o `.py`.
3. `Radio` → `Download From Radio`, com o modelo **Baojie BJ8300_v1.0TEMPRA**.

O que ele acrescenta ao driver de fábrica (deriva do `radtel_rt900.py`, que já trata este rádio
como família RT-920/BJ7800):

- Aba **Banks** com uma coluna por banco: marcar e desmarcar canal, e renomear banco.
- Em **Settings → Bancos**: ligar/desligar cada banco e escolher qual deles os itens do rádio
  editam.
- Download e upload da área dos bancos pelo comando estendido, junto com os canais.
- Recusa funcionar se o rádio **não** tiver este firmware (compara uma leitura de 24 bits com a de
  16 bits; iguais = firmware de fábrica), para nunca gravar banco em cima de canal.

Dois conjuntos de teste, os dois sem rádio:

- `python -I tests/teste_bj8300.py` — 27 conferências do miolo (formato gravado, soma batendo com
  a que o próprio rádio gravou, pertinência múltipla), com o CHIRP stubado;
- `python -I tests/teste_chirp_real.py` — 20 conferências **contra o CHIRP instalado de verdade**:
  se a classe encaixa na API (modelo de bancos, `apply_to` + callbacks dos ajustes), se imagem
  salva pelo driver RT-920 abre sem estourar, e se o que foi editado termina nos bytes da imagem.

O que nenhum dos dois cobre é a conversa com o rádio pelo cabo — essa parte foi validada à mão com
as ferramentas de `tools/cabo/` contra o rádio real.

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

Ele compila `firmware/src/bancos.c`, monta o `.anc` a partir do original e **confere** o
resultado: CRCs das 12 entradas, código e tabela presentes nas 6 imagens de flash, itens de menu,
todos os ganchos, textos, e que nenhum byte foi alterado fora dos remendos previstos — nem perto
das áreas protegidas. A saída termina em `RESULTADO: tudo certo`.

O caminho da toolchain está em `firmware/build.sh` (`C:/tools/jl-toolchain-2.5.3/...`).

**Orçamento de espaço:** a única área livre nas 6 imagens é `0x1E96121..0x1E96DFF` = **3295
bytes**. Hoje são 1768 de código + 1512 da tabela de menus copiada. Quem mexer aqui vai esbarrar
nisso — [docs/engenharia-reversa.md](docs/engenharia-reversa.md) explica como medir e onde já foi
economizado.

---

## Estrutura do repositório

```
firmware/
  src/          bancos.c (a feature inteira) e bancos.ld
  scripts/      montagem (.anc), conferência, desassemblador, análise
  original/     BJ8300_V0.12.anc — firmware de fábrica
  releases/     .anc prontos para gravar
  build.sh      compila, monta e confere
chirp/          driver do CHIRP (bj8300_v1.0TEMPRA.py)
tools/
  anc-bootloader/  gravador BinTe ANC Bootloader
  ch340-driver/    driver do cabo USB
  cabo/            ferramentas PowerShell que falam com o rádio
  ghidra-scripts/  scripts usados na análise
logo/           imagem de abertura (BMP) e conversor
tests/          testes do driver do CHIRP
docs/           documentação (engenharia reversa, histórico) e notas de trabalho
```

---

## Créditos e licença

- O driver do CHIRP **deriva** do `chirp/drivers/radtel_rt900.py` (Copyright 2025, 2026 Fred
  Trimble; 2024 Pavel Moravec, OK2MOP; 2023 Jim Unroe), distribuído sob **GPL-2.0-or-later** — a
  mesma licença se aplica a ele.
- O firmware de fábrica e o gravador são do fabricante e estão aqui só para reprodutibilidade e
  recuperação.
- O restante (código dos bancos, scripts e documentação) é de **tempraturbo**. Se for publicar,
  vale escolher uma licença explícita para esta parte.

Sem garantia de espécie alguma. Você é responsável pelo que grava no seu rádio.
