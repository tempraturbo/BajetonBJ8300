---
name: bj8300-mapa-do-codigo
description: "Mapa do app.bin do BJ-8300 no Ghidra (pi32v2 da quarkslab instalado em C:\\tools\\ghidra\\Ghidra\\Processors\\JieLi), tabela de menus de 53 itens, handlers da zona/CH-NAME e as funcoes de leitura/escrita de canal."
metadata:
  node_type: memory
  type: project
  originSessionId: 3e4d7b62-afe4-46cc-974a-76214a9ac1a8
  modified: 2026-10-22T17:12:55.941Z
---

Etapa 2 dos bancos (2026-10-22): o codigo do BJ-8300 **e legivel** no Ghidra 12.1.4 com o modulo
pi32v2 da quarkslab/ghidra-jieli. Ele foi copiado para `C:\tools\ghidra_12.1.4_PUBLIC\Ghidra\Processors\JieLi`,
com os .sla recompilados via `support\sleigh.bat -a` (o .sla que vem no repo e de versao antiga).
O REA nao serve aqui: o Ghidra P0 do Windows so aceita PE e 6502. Uso analyzeHeadless direto, com
scripts Java em `C:\temp\bj8300-firmware\ghidra-scripts` (SetupMemory, SeedCalls, DumpFunc, Trace...). O projeto
Ghidra foi apagado: refazer importando com -preScript SetupMemory.java -preScript SeedCalls.java. Cuidados: o .bat do Ghidra come `%`, `!` e `=` dos argumentos (passar
termos por arquivo e usar `max:N`); o Write preserva o BOM de um .java ja existente, e o javac recusa.

Mapa (lido na partida em 0x01e00100): app.bin roda em **0x01e00100**; copia 0xd4a8 bytes de
app.bin+0x6ac0c para a RAM em 0x0 (as chamadas usam o apelido 0x0200xxxx); .bss em 0xdd20+0x11154;
pilha 0xdcd0; main em 0x01e53a0c. Referencia indispensavel: dank0i/cyclone2 firmware/README.md (mesmo
BR23): 4 bugs do SLEIGH (o load de byte via base insere um nibble no deslocamento; o store de 4 bytes
mascara o registrador fonte em 3 bits...), call = jaddr23 (`b[1]==0xEA`), patch so com padrao unico
e decodificando antes de gravar.

**Qual tabela de menu o radio usa (resolvido em 2026-10-23):** o menu principal e a tabela em
**0x1E62F98** (53 itens; o item 0 e o SQL), carregada no laco da UI em 0x1E4CBB2 como
`r0 = 13368; r5 = r6 + r0` com r6 = 0x1E5FB60 - por ser soma de registradores, procurar o valor
pronto no desmontado nao acha nada. A quantidade de itens sai de FUN_01E4A81A: a tabela acaba no
primeiro item com os 28 bytes zerados. Os menus curtos vem do vetor de ponteiros em 0x1E5E7A0
(ids 1..10, tabelas de 2 itens; o id 10 e o ZONE solto em 0x1E5E76C) e de 0x1E61168/0x1E6137C
(2/5/6 itens, escolhidos por b[0x12A5B]); FUN_01E4B3A0(id) abre pelo vetor. O byte +0x1B do item
e o numero do aviso falado (FUN_01E38BD4, so fala com VOICE = b[0x14B73] ligado).

**Motor de menus FUN_01E489E0 (lido inteiro em 2026-10-23):** ponteiro da tabela em `[0x14BF4]`,
item em `b[0x14BFA]`, item = 28 B `{flags(1) | max(u16) | get | set | nome_en | nome_cn |
exibe-ou-opcoes_en | opcoes_cn | id(1)}`. Valor em edicao `h[0x14BFC]` (salvo em r8 e **devolvido**
depois de desenhar, por isso set() le dele), cursor `h[0x14C18]`, numero de linhas `h[0x14C0E]`
(vem do max), copia do escolhido em `[0x14C04]`, 1a linha visivel `b[0x14C10]`, idioma `b[0x14B74]`.
flags: 0x80 = +0x13 e funcao de exibicao (chamada por linha); 0x20 = o nome vem de funcao em +0xB;
0x07 = editor de texto. Caso especial do firmware: se o get do item e 0x1E38A1E (MEMCH/DELCH) e
`b[0x14BD4]`=Full Channel, o max vira 960 - prova de que u16 ate 960 funciona.

**Memoria de dados (chip SPI externo, FUN_01E390AC manda 0x03 + endereco de 24 bits, sem base):**
0x0000-0x77FF 960 canais x 32 B; 0x8000 61 registros de 66 B; 0x9000 config; 0xA000; 0xB000;
0xC000; 0xD000; 0xE000; 0xF000-0xF24F dados de fabrica; 0xC0000 64 registros de 60 B (lido na
partida e **apagado** por 0x1E3FECC - o chip e grande). **0x10000 esta livre** e e onde os bancos
moram. Mapas na RAM: 0x15D10 zona-tem-canal, 0x15D12 busca (120 B), 0x15D8B existe (120 B),
0x15E06 zona-tem-canal (igual ao 0x15D10), 0x15E08 zona-tem-busca, 0x15E0C/0x15E10 canal A/B,
0x15E3C canal atual. So FUN_01E39FCE le os mapas; FUN_01E3A43E(mapa, indice, valor) poe bit.

**Menu = tabela de 53 itens de 28 bytes**, compactada, em 0x01e62f9b..0x01e63567 (app.bin 0x62e9b):
`{get(), set(), nome_en*, nome_cn*(GBK), opcoes_en*|funcao_exibe, opcoes_cn*, flags}`. Script:
`C:\temp\bj8300-firmware\scripts\menu_table.py`. Itens chave: #10 Zone Or Channel, #11 ZONE (get 0x1e3e17a,
set 0x1e3e18a, exibe 0x1e3e140 = sprintf "ZONE %d"), #12 MEMCH (set 0x1e3a906), #19 CH-NAME (get
0x1e3c732, set 0x1e3c7be, exibe 0x1e3c6e8), #35 SCAN ADD (0x1e3cbe6/0x1e3cc08).

**Partida e recuperacao (verificado no codigo em 2026-10-22):** main do app = FUN_01e57472
(inicializacao de hardware + tabelas de initcall em 0x01e1b01c..0x01e1b044, depois o laco de estados
em 0x14688; nos estados 5/10 ele chama FUN_01e56bb6). FUN_01e56bb6 = inicializacao do app: **le as
teclas (FUN_01e001ba(8/0x15/7)) ANTES de carregar config/canais (FUN_01e56084)**. Com o combo
PTT2+atalho -> FUN_01e432fc (desenha "UPDATE...") e `DAT_0001533e = 5`. O tick periodico FUN_01e4dea8
so chama o laco da UI FUN_01e4b416 `if (DAT_0001533e != 5)`: no modo UPDATE nao rodam menus nem a tela
inicial. FUN_01e4b416 = laco principal da UI + protocolo do cabo ('D','U'+"UPDATE",'F','E','W'...).
Regras de seguranca dos patches: nunca tocar o caminho anterior a checagem nem o modo UPDATE
(FUN_01e57472, o comeco de FUN_01e56bb6, FUN_01e432fc, FUN_01e4dea8, FUN_01e3cfd2, FUN_01e55af4); e
guardar os bancos numa area que o firmware original nao le, porque o update nao apaga a memoria de
canais/config. Cobertura: SeedCalls.java (destinos de call) -> 2327 funcoes, 97 mil instrucoes.

**Compilador:** JL toolchain 2.5.3 oficial (instalador Inno assinado pela ZhuHai JieLi, extraido com
innoextract 1.9, GPG do autor conferido, sem executar o instalador) em `C:\tools\jl-toolchain-2.5.3\C$\JL\pi32\bin`:
`clang -target pi32v2 -integrated-as -Os -fno-builtin -c` funciona sem licenca, e `llvm-objdump -d`
desmonta pi32v2 com a sintaxe da JieLi (melhor que o SLEIGH para conferir patches). ABI: argumentos
em r0,r1,r2..., retorno em r0, `rts`.
**Memoria de canais = flash SPI EXTERNA:** FUN_01e390ac(addr,buf,len) manda o comando 0x03 com endereco de
24 bits (o mesmo espaco do CHIRP). A gravacao FUN_01e3a2ba(canal,dados20,nome12) le o setor de 4 KB
para o buffer RAM 0x10190, apaga (FUN_01e3a126) e regrava (FUN_01e3a25a). **960 canais** (0x3c0 = 15
zonas x 64). Mapas de bits na RAM (120 B) montados na partida por FUN_01e56084: 0x15d8b = canal existe,
0x15d12 = scan (bit 0x04 do byte 0xF do canal). Teste FUN_01e39fce(canal, qual 0/1): 15 chamadas
(callers.py). FUN_01e45734(qual, ...) = busca do canal anterior (navegacao/scan); FUN_01e3a700
revalida o canal atual. Flag Full Channel 0x14bd4 (1 = sem zonas) aparece em ~18 lugares e a zona
0x15e15 em ~22: o plano e forcar Full Channel e por o BANK no lugar dos itens de menu 10/11.
Espaco: depois do app.bin vem o diretorio de recursos (`tone`, index.idx, C0..C15.wtg, ~120 KB); folga
no fim da imagem ~3 KB (0x991dc..0x9a000 na imagem de 0x9a000).

Zona: `canal = zona*0x40 + indice` (zona em RAM 0x15e15, por A/B em 0x14bdb[ab], ab em 0x15e04;
canal/indice atual em h[0x15e3c]; 0x14bd4 = modo full-channel; idioma em 0x14b74). Cuidado com
`h[base+N<<1]`: o endereco e base+2N (o 0x1493e anotado aqui antes era esse erro; o certo e 0x14bfc). Memoria de canais (o mesmo espaco
de enderecos do CHIRP RT-920): leitura `func_0x01e390ac(endereco, buf, len)`; o nome do canal fica em
`canal*32+0x14` (12 B); escrita de canal provavelmente via `func_0x01e3a2ba`.
Ferramentas sem Ghidra (2026-10-23): `scripts/desmonta.sh` desmonta um trecho no endereco real
(o llvm-objdump 4.0.1 da toolchain nao tem -b binary: o app.bin entra num .elf por .incbin) e
`scripts/chamadas.py` lista chamadores/chamados decodificando `call jaddr23`. Foi so com isso que
a v2 dos bancos foi conferida - mais rapido que remontar o projeto do Ghidra.
Relacionado: [[firmware-baojie-bj8300-jieli]], [[bj8300-bancos-progresso]].
