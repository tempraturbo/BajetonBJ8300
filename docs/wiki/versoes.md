# 🗂️ As versões

<p align="center">
  <a href="versoes.en.md"><img src="../imagens/botao-english.svg" alt="English version" height="56"></a>
</p>

[📖 Wiki](README.md) · [🏠 Página inicial](../../README.md)

## Versões anteriores

A última fica em [firmware/releases/](../../firmware/releases/). As lançadas estão na
[página de Releases](https://github.com/tempraturbo/BajetonBJ8300/releases), e todas, as de teste inclusive, no
histórico do git. A história completa, com o que quebrou e por quê, está no [histórico](../historico.md).

| Arquivo | Estado |
|---|---|
| `BJ8300_BANK_v4.2T.anc` | **estável, a recomendada** — a v177: a V3.7T + as memórias do rádio de AM, FM e SSB com nome (SAVE, DELETE e NAME na pasta CH MEMORY), o menu do rádio em duas pastas, a etiqueta VFO do modo frequência, a seta segurada no espectro, o controle remoto pelo cabo e pelo Bluetooth (com a tela do rádio no PC e a tela apagada a pedido), o ROGER CUSTOM, a busca a cada 60 ms com o THRESHOLD, o número do canal na busca e o `*` curto que a para, o Bluetooth que fica no TX, o cabo a 460800, a versão pelo cabo e pelo Bluetooth, o SCR em todas as frequências do scramble e o VERSION que abre com o firmware e o hardware e a linha do espectro que sobe sempre e volta ao padrão na entrada; use com o Tempra Link e o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v4.1T.anc` | nunca lançada — a v172 com o nome; as memórias do rádio de AM, FM e SSB (v173 a v177) entraram antes do lançamento, como V4.2T |
| `BJ8300_BANK_v4.0T.anc` | nunca lançada — a v171 com o nome; a correção da linha do espectro (v172) entrou antes do lançamento, como V4.1T |
| `BJ8300_BANK_v3.7T.anc` | estável anterior — a v154: a V3.6T + a tela MESSAGES do Morse (o que chega, o histórico e a caixa SEND), a tela SENDING, as caixas abrindo com a última mensagem, as telas caladas, o MESSAGES num clique, o MONITOR no lado em evidência, o PF3 LONG e a lanterna de tela; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v3.6T.anc` | estável anterior — a v142: a V3.5T + o CH MEMORY, os BANKS refeitos, os avisos de modo, as listas com o cursor no meio, os nomes claros, o TRIPLE WATCH com DW e TW na barra e a pasta APRS (SMS); use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v3.5T.anc` | estável anterior — a v139: a V3.2T + o SAVE que já pede o nome, as etiquetas do tom de TX e AIR, o MIC GAIN com DEFAULT, o teto de 90% no PA, o CALLSIGN antes do envio, o OK que grava o canal e um EXIT só nos itens de texto; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v3.2T.anc` | estável anterior — a v115: a V3.1T + a tela MESSAGES no lugar do RX, do MESSAGE e do SEND do APRS (o `#` abre a caixa da mensagem e o `#` segurado a do TO, e a tela volta sozinha depois do envio), o rodapé com o P e o RX GAIN na CALIBRATION; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v3.1T.anc` | estável anterior — a v111: a V3.0T + o EQ no CHIRP (que a V3.0T voltava a DEFAULT a cada gravação pelo cabo), o APRS RX com o FILTER pondo os graves em −3 dB, os agudos em +4 dB e o SAVE desligado, a tecla 0 que liga e desliga o FILTER, o rodapé com S, B e F, o MORE embaixo da mensagem e a limpeza do firmware; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v3.0T.anc` | estável anterior — a v110: a V2.9T + as pastas APRS (o envio pela entrada do microfone, a recepção com o FILTER, o histórico e o ack) e MORSE, o EQ de graves e agudos na recepção e na transmissão, a barra S de 24 segmentos, a pasta AUDIO (EXPANDER, NOISE GATE e DE-EMPHASIS), a NOISE REDUCTION com níveis, o LIGHT PWM, o MODEM GAIN, o NO TX e cada ajuste gravado no OK; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v2.9T.anc` | estável anterior — a v64: a V2.8T + a pasta MIC (COMPRESSOR, MIC AGC e o MIC GAIN, que saiu do TX), a pasta CALIBRATION com o CRYSTAL FINE, o BRIGHTNESS no DISPLAY, quem recebe na barra de cima com o menu aberto (sem o bloco antigo por cima do menu) e o driver do CHIRP em Bajeton; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v2.8T.anc` | estável anterior — a v58: a V2.7T com o fundo âmbar do 3º item de cada pasta e do menu do rádio inteiro; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v2.7T.anc` | o menu em pastas com o fundo do 3º item pela metade — a v57: a V2.6T + o menu em 13 pastas (com o valor do item, a rolagem, OK abrindo já na edição, BANK e CHANNEL de marcar vários e o VERSION só leitura), a letra do menu maior, o menu do rádio na mesma janela, sem o item CH-C, o editor de nome contando só as letras e só com ABC e 123, e a tensão do `0` segurado no tema novo, mas o fundo âmbar do 3º item sai pela metade; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v2.6T.anc` | a escuta tripla com o menu numa lista só — a v51: a v41 + o WATCH (SINGLE, DUAL e TRIPLE), o lado C completo (VFO ou canal), o TX-A/B/C, o MIC GAIN, a luz acesa no espectro, o TTR e a caixa do DTMF no tema novo; use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v2.0T.anc` | a tela nova sem o WATCH — a v41 com o nome da versão (mesmo arquivo); use com o driver do CHIRP do mesmo release |
| `BJ8300_BANK_v45.anc` | v41 + o menu `WATCH` (SINGLE: um lado em tela cheia; DUAL: o A/B de sempre; TRIPLE: uma terceira frequência, a C, escolhida no `CH-C` e mostrada como o `MDF-C` pede, que a dupla escuta vigia com o mesmo tempo das outras e a tecla A/B põe em evidência) — testado no rádio |
| `BJ8300_BANK_v41.anc` | v39 + o espectro no tema B, os ajustes do rádio conferidos na entrada e na troca de modulação, a troca de modulação ~2,5 s mais rápida e o S da fonte média — testado no rádio |
| `BJ8300_BANK_v39.anc` | v36 + as outras telas no tema B (menu com a fonte média, editor de nome, rádio, scan QT, DTMF e a tensão na partida), sem o idioma chinês, os dois lados no banco e o menu `VERSION` em `V2.0T` — testado no rádio (um travamento na troca para USB/LSB vinha do que estava salvo nele: [histórico](../historico.md), v40) |
| `BJ8300_BANK_v36.anc` | v34 + a tela principal nova (tema B): cartão do lado em evidência com a frequência grande e o medidor, etiquetas coloridas de potência, modulação e tons, e a barra de cima só com o que está ligado; no idioma chinês a tela fica a de fábrica — testado no rádio |
| `BJ8300_BANK_v34.anc` | v32 + o espectro no espaço das vozes, o pico em verde e o dBm inteiro na troca de AM/FM; a última com a tela principal de fábrica — testado no rádio |
| `BJ8300_BANK_v32.anc` | v27 sem as vozes (e sem o item VOICE), roger COIN (a moeda), leitura de memória pelo cabo; abre ~112 KB de espaço para o tema novo da tela — testado no rádio |
| `BJ8300_BANK_v27.anc` | v1.0T + espectro com a barra atual em branco, a frequência que roda, a linha do pico e o dBm à direita; roger BEEP 3 (a moeda, chamada de COIN desde a v31) — testado no rádio |
| `BJ8300_BANK_v1.0T.anc` | a primeira estável, com a tela de fábrica — a v25 com o nome da versão (mesmo arquivo) |
| `BJ8300_BANK_v24.anc` | v23 + scramble 2900 Hz (o do Baofeng UV-17L no modo 3) — testado no rádio |
| `BJ8300_BANK_v23.anc` | v21 + roger BEEP 2 — testado no rádio |
| `BJ8300_BANK_v21.anc` | v19 + espectro duas vezes mais rápido, passo de 10 kHz, 320 e 640 pontos e pico seguro; verde do realce mais escuro (a v20 sem o bootloop; a v20 foi retirada) — testado no rádio |
| `BJ8300_BANK_v19.anc` | v18 + a seta continua o espectro parado num achado; a tabela de menus no lugar da original — testado no rádio |
| `BJ8300_BANK_v18.anc` | v17 com o fundo verde também na caixinha do RX/TX — não testado no rádio |
| `BJ8300_BANK_v17.anc` | realce do bloco do VFO sem as faixas pretas ([mapa da tela](../mapa-da-tela.md)) — testado no rádio |
| `BJ8300_BANK_v16.anc` | primeiro realce funcionando; sobram faixas finas pretas entre as linhas do bloco |
| `BJ8300_BANK_v14.anc` | tentativa anterior do realce: o verde só saía na barra de sinal |
| `BJ8300_BANK_v12.anc` | bancos, setas, textos, correção do scan e o cabo de 24 bits — testado no rádio |
| `firmware/original/BJ8300_V0.12.anc` | firmware de fábrica, para voltar atrás |
