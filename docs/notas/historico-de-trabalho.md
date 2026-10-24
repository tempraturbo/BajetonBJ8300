---
name: bj8300-bancos-progresso
description: "Bancos de canais no BJ-8300 - v2 refeita em 2026-10-23 corrigindo os 5 achados da revisao; C:\\temp\\BJ8300_BANK_v2.anc pronto, aguardando o primeiro teste no radio."
metadata:
  node_type: memory
  type: project
  originSessionId: 8eeef6e4-9dd4-4008-9109-640fc6f273db
  modified: 2026-10-24T01:24:40.276Z
---

Projeto em `C:\temp\bj8300-firmware`; recompilar e conferir com `sh build.sh`. Entregue:
**`C:\temp\BJ8300_BANK_v2.anc`** (sha256 63ad641e..., 1674 B de codigo + 1568 B de tabela; v2 testada no radio, v3 corrige o que ela mostrou).
O `C:\temp\BJ8300_BANK.anc` de 22/10 e a versao REPROVADA - nao gravar.

**16 bancos** com nome de 12 caracteres, canal em quantos bancos quiser, vários ligados ao mesmo
tempo. **Nenhum item de menu e perdido** (o usuario pediu isso em 23/10, e depois pediu que os
itens ficassem juntos, nao largados no fim): os 53 de fabrica continuam, **inclusive o ZONE**, e
os tres dos bancos entram logo depois dele - **BANK** (12, flags 0x80, max 16), **BANK NAME** (13,
flags 0x07) e **BANK CH** (14, flags 0x80, max 960); o resto desce tres posicoes. Como:
- o menu principal e a tabela de 53 itens de 28 B em 0x01E62F98, carregada por UMA instrucao do
  laco da UI (0x01E4CBB2: `40e03834651c` = r0 = 13368; r5 = r6 + r0, r6 = 0x01E5FB60);
- o montador grava a **copia da tabela** no espaco livre (0x01E967C0, depois do codigo) e troca
  essas duas instrucoes por uma de 6 B (`c5ff` + endereco LE) que poe o endereco da copia em r5 -
  seguro porque a funcao chamada em seguida (0x01E3AC52) sobrescreve r0 na primeira instrucao;
- a contagem de itens (chamada a FUN_01E4A81A em 0x01E4CBE8, bytes `bfea17ee`) vira a constante
  `40e0` + total LE (4 B), em vez de depender de terminador - a varredura nunca sai para a area
  apagada e sobram 32 B de folga;
- os itens novos copiam o molde de um que funciona (ZONE para numero com exibe proprio, CH-NAME
  para o editor), herdando opcoes_cn=0 e aviso falado=0.
Dentro do app.bin mudam **13 bytes** (gancho 4 + ponteiro 6 + contagem 4); o resto fica igual ao de
fabrica, e o conferir.py recusa qualquer byte fora disso. Orcamento: 3264 B livres na menor imagem
= codigo (1660 B com -Oz) + tabela (1568 B). O motor **nao tem submenu**: o nibble baixo das flags
escolhe entre 7 telas (0 lista/valor, 2 CTCSS, 3 frequencia, 4 MEMCH/DELCH, 7 editor, 8 VERSION,
10 DCS) e nenhuma abre lista dentro de lista; para a estrutura em dois niveis que o usuario
desenhou (BANK -> banco -> RENAME/canais) seria preciso trocar a tabela corrente de dentro do
handler, como FUN_01E4B3A0(id) faz pelos menus rapidos - combinado deixar para depois do 1o teste.
O item **Zone Or Channel ficou de pe**: sem Full Channel o sobe/desce nao sai da zona. Nao forcar
o modo por codigo - a troca mexe em canal atual/A-B e o handler do firmware (0x1E3E25A) desenha
popup. Orcamento de espaco: 3264 B livres na menor imagem = codigo (1660 B com -Oz) + tabela
(1568 B); o `repack` recusa se nao couber ou se a area nao estiver apagada.

**O que a v2 corrigiu** (os 5 achados da revisao de 22/10, todos confirmados desmontando):
1. o blob ia **em claro**; a area depois do diretorio do app e lida pelo SFC, que decifra por
   endereco -> `jlfw.repack` grava `sfc(codigo, chave, pos-base)` e o `conferir.py` compara o
   resultado **decifrado**. O CRC de app_area_head cobre so 0x781DC bytes, bem antes do blob.
2. valor do item em edicao e `h[0x14BFC]` (nao 0x1493E): o motor guarda o escolhido, troca por
   linha enquanto desenha (por isso exibe() le dele) e **devolve antes de chamar set()**.
3. sem estado em RAM: para desfazer o filtro os mapas sao **remontados** da memoria de canais com
   o critério da partida, em vez de guardar copia (nao existe RAM livre comprovada). Rascunho da
   uniao no fim do buffer de 4 KB do firmware (BUFFER+0xC00), nao na pilha de 2 KB.
4. os bancos saíram de 0xF400 (setor dos dados de fabrica) para o **setor 0x10000**, 4 KB so
   nossos; o chip e usado pelo firmware ate 0xC0000. Gravacao conferida antes contra chip de
   64 KB (0x19000 espelharia 0x9000) - se espelhar, nao grava.
5. as mascaras por zona (0x15D10, 0x15E06 existe; 0x15E08 busca) sao recalculadas junto; banco
   ligado sem canal existente nao filtra nada (e o OK nem liga), para o radio nunca ficar sem
   canal (a busca sem achar poe 0xFFFF).

**Enderecos usados pelo codigo** (todos conferidos no desmontado): sprintf 0x01E19DCC (max 2
valores, como os itens do firmware), le_flash 0x01E390AC, apaga_setor 0x01E3A126, grava 0x01E3A25A,
valida_freq 0x01E38C3A (recebe os 4 bytes BCD decodificados do mais significativo para o menos),
revalida 0x01E3A700 (so age com b[0x15E12]==1), set_bit 0x01E3A43E, teste dos mapas 0x01E39FCE
(canal, 0=existe/1=busca) - **unico leitor** dos mapas, 15 chamadores. Buffer de setor do
firmware em **0x101D0** (o 0x10190 anotado antes estava errado). Editor de texto: 0x14A35 tamanho
atual, 0x14A36 maximo, 0x14A39 marca 0xAA, 0x14A3A texto, 0x12A74 zerado pelo get. Texto do menu
em 0x129B0 (>=104 B livres).

**Criterio de canal existente** (igual ao da partida): 1o byte != 0xFF, 4o byte != 0, frequencia
(4 bytes BCD) aceita por 0x01E38C3A; bit 0x04 do byte 15 = entra na busca.

**TESTE NO RADIO 2026-10-23/24: a v2 rodou** (menus abrem, BANK CH mostra `banco-canal`, ` IN`
grava e sobrevive) **mas o filtro nao pegava e aparecia uma faixa branca sobre o logo na partida.**
Causa achada e corrigida: **0x10190 NAO e buffer de setor, e a memoria de video** - 0x01E486C8
enche 10240 B dali com uma cor (limpa a tela) e 0x01E4AAF0 escreve pixels de 16 bits em
0x10190 + (y*largura + x)*2; o firmware so a reaproveita como buffer ao regravar setor de canal,
aceitando sujar a tela. A tabela dos bancos morava ali: a interface pintava por cima, o checksum
passava a falhar e o filtro via "nenhum banco ligado" - enquanto o display do menu, que le so o
cabecalho para a pilha, mostrava ` ON` e ` IN` certos (foi essa assimetria que denunciou).
**Regra: nunca usar buffer do firmware como rascunho.** A v3 guarda **um banco por setor de 4 KB**
(cabecalho em 0x10000, banco n em 0x11000 + n*0x1000) e le tudo em pedacos para a pilha (uniao de
120 B + pedacos de 24 B; reconstroi_mapas le canal por canal, as mesmas duas leituras da partida).
A trava do chip tambem compara 0x29000 (chip de 128 KB espelharia, porque o banco 15 fica em
0x20000). Entregue **C:\temp\BJ8300_BANK_v3.anc** sha256 63ad641e..., codigo 1674 B + tabela
1568 B (cabe exatamente nos 3264 B livres da menor imagem).
**Setas do menu invertidas a pedido** (2026-10-24): de fabrica a seta de cima avanca o valor, que
esta desenhado mais abaixo. As rotinas 0x01E49F60 (avanca) e 0x01E49FF4 (retrocede) atendem as
duas telas (lista de itens e lista de valores) e so mexem na estrutura 0x14BF4, entao inverter nao
afeta canal da tela inicial nem busca; o tratador as chama em 4 sitios (0x01E4D094, 0x01E4D12A ->
avanca; 0x01E4D0A4, 0x01E4D0AA -> retrocede) e o montar.py troca os quatro (SETAS_INVERTIDAS).
A bolinha ao lado de um canal no BANK CH e do firmware: o motor marca a linha cujo valor e igual a
[0x14C04], preenchido no get() ao abrir o item (no BANK CH e o canal atual = "voce esta aqui").

**FUNCIONANDO NO RADIO (v3, 2026-10-24):** o usuario ligou o banco 2 com um canal marcado e o
sobe/desce passou a andar so nele. Sobrou um sintoma: **faixa branca sobre o logo na partida**, que
CRESCEU na v3 - justamente porque o filtro passou a agir. Causa: `REVALIDA` (0x01E3A700) chama
0x01E39F20, que liga as flags de redesenho em 0x15367/0x15368/0x1536F; na partida isso faz a
interface repintar por cima do logo. Corrigido na v4: `aplica_filtro(reconstruir, revalidar)` e o
gancho da partida chama (0, 0) - filtra calado, e quem arruma o canal e a primeira mexida do
usuario (o canal atual continua valido, so pode estar fora do banco ate a primeira seta).

**Textos em ingles revisados (v4):** 23 cadeias, nenhum byte a mais - padrao CAIXA ALTA (que 90% ja
usava), unidade com grafia certa (Hz), sem espaco duplo. Corrige "Delet"->"DEL", "Super  Mode",
"Deep   Mode", "1000hz"->"1000Hz", "Zone Or Channel"->"ZONE OR CHANNEL", "2.5 K"->"2.5K", "All",
"Middle", "RX Modulation", "POWER ON MSG"->"POWER-ON MSG". **As cadeias nao tem folga** (vem
coladas): trocar no lugar so pode encurtar. Duas do STEP sao sufixo de outras ("2.5 K" dentro de
"12.5 K", "5.0 K" dentro de "25.0 K") - so a maior entra na lista e a menor sai corrigida junto.
Lista completa, padrao e o que ficou de fora (VOICEPRI = 语音加密 = criptografia, nome enganoso):
`notas/textos-revisao.md`. Ferramenta: `scripts/textos.py` (endereco, tamanho e folga de cada texto).
Entregue **C:\temp\BJ8300_BANK_v4.anc** sha256 8324c02b..., 1688 B de codigo.

**v10 (2026-10-24): sai do editor de texto como o firmware sai.** Sintoma que o usuario achou: ao
gravar um nome no BANK NAME, o cabecalho do menu passava a mostrar o "abc" (indicador do teclado)
em vez do canal, e ficava assim em todos os itens ate sair e entrar no menu. Causa: o `set` do
CH-NAME termina chamando **0x01E3ACBA(2)** (a mesma que o menu chama ao abrir) e **0x01E3C7AA**, e o
nosso `nome_grava` nao chamava - o estado do editor ficava ligado. Corrigido copiando as duas
chamadas. Para caber nos 1696 B, `marca_ok` virou laco e `setor_livre` ganhou retorno antecipado.
Entregue **C:\temp\BJ8300_BANK_v10.anc** sha256 abaixo, codigo 1690 B.
Cosmetico que FICA: a tela do editor mostra o canal atual no topo (e o cabecalho padrao dela, que o
CH-NAME usa porque edita o nome do canal); no BANK NAME isso nao quer dizer nada, mas trocar exige
mexer no desenho da tela de edicao.

**v11 (2026-10-24): leitura estendida pelo cabo - NAO FUNCIONOU, duas causas.** Gancho em
**0x01E4BB66** (a chamada que o tratador do 'R' faz para buscar os dados) com pacote de 5 bytes
(o quinto seria o terceiro byte do endereco). Testado no radio em COM3: as tres leituras estendidas
voltaram **identicas ao que o truncamento de 16 bits daria** e, logo depois, o radio **parou de
responder** ao magic ate ser desligado. As causas, achadas no desmontado (ver v12): o pacote nao
fica em 0x16691 e sim em **0x16671** (o gancho lia 0x20 bytes fora), e o tratador dispara com
**quatro** bytes no buffer, entao o quinto byte chega tarde e fica pendente, envenenando tudo o que
vem depois. Substituido pelo v12.
Para caber: **recuperei os 30 bytes perdidos no alinhamento** - `jlfw.sfc` agora cifra com a fase
certa para base fora do bloco de 32, entao o blob comeca em 0x1E96122 (primeiro byte par livre) em
vez de 0x1E96140; `reconstroi_mapas` passou a ler **16 bytes por canal numa so chamada** (metade das
leituras, codigo menor); `canal_le` devolve o canal atual sem checar limite (o motor ja zera o que
passa do maximo); e o editor de nome abre **vazio** quando o banco nao tem nome (antes preenchia
"BANK n"). Medir tamanho: somar .text + .rodata + .rodata.str1.1 do .o - **nao** incluir .rela.text.
Entregue **C:\temp\BJ8300_BANK_v11.anc** sha256 abaixo; codigo 1696 B + tabela 1568 B, 30 B de folga.

**v12 (2026-10-24): cabo de 24 bits para ler E gravar, em pacote de 4 bytes.** Tres achados no
desmontado, todos com evidencia:

1. **A base e 0x14680, nao 0x146A0.** Em `0x01E4B422` o tratador faz `r13 = 83584`; a rotina de
   dados `0x01E43970` carrega a mesma base em r6 e le o endereco alto em `b[r6+8178]`, o baixo em
   `b[r6+8179]` e os dados em `r6+8181`. Logo o pacote do cabo comeca em **0x16671** (letra,
   end_alto, end_baixo, tamanho; dados e resposta a partir de **0x16675**) e o comprimento recebido
   e **h[0x1666C]**. Era esse o erro silencioso do v11.
2. **Nao existe pacote de 5 bytes.** `0x01E4BB52` desvia se `h[0x1666C] < 4`: com quatro bytes o
   tratador ja responde e depois zera o contador (`0x01E4BB7C`), entao o quinto byte chega tarde,
   fica pendente no buffer e engole o comando seguinte - inclusive o magic, que o radio compara
   **desde o inicio do buffer**. Foi por isso que o radio travou no teste e nao voltou sem desligar.
   Corolario para qualquer ferramenta nossa: **nunca mandar byte algum antes do magic** (nem 'E',
   nem zeros de enchimento); se o handshake falhar, desligar e ligar o radio.
3. **'R' e 'W' chamam a MESMA rotina**, `0x01E43970`, com argumento 0 para ler e 1 para gravar
   (`0x01E4BB62` e `0x01E4C0F8`). Um gancho so, que recebe e repassa o argumento, atende aos dois -
   e o `'W'` de fabrica tem tamanho fixo de 68 bytes no buffer (`0x01E4C0E8`), entao o pacote
   estendido de gravacao tem exatamente o tamanho do de fabrica. O que o firmware chama depois do
   gancho no caminho do 'W' (`0x01E416C4`) so manda o ACK 0x06 e zera o contador: nao mexe em flash.

**Codificacao nova (4 bytes):** o **bit 7 do tamanho** liga o modo estendido e os outros 7 bits sao
o terceiro byte do endereco; o bloco e sempre de 64 B e o gancho reescreve o tamanho para 0x40
antes de sair, porque a resposta do 'R' e montada depois com `4 + tamanho`. Bit 7 em zero = fabrica,
byte a byte: o CHIRP nao ve diferenca. Alcance 23 bits = 8 MB.
**Gravacao** so em `0xC1000..0xD1FFF` (cabecalho + 16 bancos), com `setor_livre` tambem aqui (nunca
apaga setor de outro dono) e apagamento quando o bloco comeca no setor. Depois de gravar banco pelo
cabo: desligar e ligar o radio, porque o filtro e montado na partida.

**Para caber nos 1726 B** (o orcamento real: 3294 B na menor das 6 imagens, menos os 1568 da tabela;
a unica area livre nas 6 e 0x1E96121..0x1E96DFF, confirmado por varredura): a marca do registro
virou `u16 marca[2]` e um u32 (`MARCA_CAB`/`MARCA_BANCO`), comparada e gravada em dois acessos de
16 bits em vez de laco byte a byte - e sai da imagem como constante, nao como cadeia; e a marca
passou a ser **deduzida do endereco** (`MARCA_DE`), um argumento a menos em quatro chamadas. O
formato gravado nao mudou (10 B e 138 B, soma no fim), entao bancos ja gravados continuam valendo.
Resultado: 1702 B de codigo (24 de folga) com leitura **e** gravacao, contra 1696 so com leitura.
Ferramentas novas: `scripts/cabo.ps1` (biblioteca: handshake, `Le-Bloco`, `Grava-Bloco`) e
`scripts/mapa_memoria.ps1` (prova a leitura de 24 bits, diz se os dados vem crus, acha o tamanho
real do chip pelo espelhamento e lista o que existe acima de 64 KB).
Entregue **C:\temp\BJ8300_BANK_v12.anc** sha256 588d71aafbe1f38de506ebf6c2cd13d1e30886ff8eb51d35d618336cda2b8c0e

**RADIO LIGADO NA VM (2026-10-24):** o cabo e um **CH340** (VID_1A86/PID_7523) e a VM nao tinha o
driver. O pacote da internet (espelho no GitHub) foi reprovado pelo Windows ("hash fora do
catalogo"); o que funcionou foi **exportar o driver do proprio host**: no host, elevado,
`pnputil /enum-drivers | Select-String -Context 1,6 'ch341ser'` e
`pnputil /export-driver oemNN.inf C:\ch340export`, trazer a pasta e instalar na VM elevado com
`pnputil /add-driver C:\temp\ch340host\ch341ser.inf /install`. Resultado: **COM3 = USB-SERIAL CH340**.
Armadilhas: o `!` do chat roda no Git Bash e converte `/add-driver` em caminho (o pnputil so imprime
a ajuda) - usar `powershell -Command "..."`; e a instalacao precisa de elevacao (abrir com
`Start-Process powershell -Verb RunAs`). Primeiro contato pela VM ok: handshake, id1
`01 36 01 80 04 00 05 20`, id2 `02 00 02 60 01 03 30 04`, bloco 0x0000 (cifrado) e 0xF000 (cru).
Enquanto o cabo esta na VM, o host nao enxerga o radio.

**LAUDO DA MEMORIA (2026-10-24, dump real pelo cabo com ler_memoria.ps1):** identificacao
`01 36 01 80 04 00 05 20` + secundaria `02 00 02 60 01 03 30 04` (bate com o RT-920 do CHIRP).
**Nenhuma marca BNK3/BCO1/BNK1/BNK2 nos 64 KB baixos** -> 0xC1000 nao espelha ali. Como um chip de
64/128/256 KB faria 0xC1000 cair em 0x1000 (e as marcas apareceriam no dump), **o chip e maior que
256 KB**; se for de 512 KB nossa area cai em 0x41000, que o mapa mostra vazio - nos dois casos nada
de valor corre risco. 77 canais intactos (faixas 1-47, 100-103, 200-219, 322-327), nomes legiveis,
area de fabrica 0xF000-0xF24F intacta (260 B com conteudo; 0xF230 guarda os bytes da identificacao).
Mapa real (blocos de 256 B com conteudo): canais 0x0000-0x77FF; 0x8000-0x84FF; 0x9000-0x95FF;
0xA000; 0xB000-0xB3FF; 0xD000; 0xE000-0xE5FF; 0xF000-0xF2FF. **0xC000 esta vazio** (o firmware le
0xC010/0xC210, mas nada foi gravado ali).
**CONSEQUENCIA PARA O DRIVER DO CHIRP:** o comando 'R'/'W' do cabo tem endereco de **16 bits**, entao
o PC **nao alcanca os bancos em 0xC1000**. Para o driver funcionar seria preciso ou mover os bancos
para baixo de 0x10000 (nao ha setor livre garantido: 0x3000-0x7FFF e area de canal e 0xC000 o
firmware le) ou **ensinar um comando novo ao firmware** (estender o tratador do cabo no laco da UI
para ler/gravar a nossa area). A segunda e a boa, e e o proximo assunto.

**PROTOCOLO DO CABO (do driver radtel_rt900.py do CHIRP, confirmado pelo usuario: o radio e o
RT-920):** 57600 8N1; magic `PROGRAMBT11U` ate 5x esperando ACK 0x06; depois `F` -> 8 bytes de
identificacao, mais 8 bytes (esperado `02 00 02 60 01 03 30 04`), depois os 25 bytes
`SEND ` + 18 zeros -> ACK; leitura `R` + endereco **big-endian de 16 BITS** + tamanho
(0x40), resposta = eco de 4 bytes + dados; `W` grava; `E` sai. Os dados abaixo de 0xF000 vem
**cifrados** (XOR com a chave "CO 7", pulando 0x00, 0xFF, espaco na chave, e bytes iguais a chave ou
ao inverso dela - mesma familia da camada KDH do .anc); de 0xF000 para cima vem crus.
**O endereco e de 16 bits: o CHIRP (e qualquer ferramenta com esse protocolo) NAO alcanca 0x10000
nem a nossa area em 0xC1000** - o CPS deve usar outro caminho para gravar o logo. No firmware, 'S'
e a configuracao da cifra (nao leitura estendida); 'D'/'U' sao do modo UPDATE.
Ferramenta nova: `scripts/ler_memoria.ps1` (PowerShell, sem instalar nada, SO LE) - faz o handshake,
despeja 0x0000-0xFFFF, decifra e responde: (1) as marcas BNK3/BCO1 aparecem nos 64 KB baixos (seria
espelhamento de endereco, o unico jeito de a gravacao fazer estrago)? (2) os canais estao inteiros?
(3) mapa dos setores com conteudo.

**v9 CONFIRMADA NO RADIO (2026-10-24): bancos funcionam e o logo fica limpo.**
Vizinhanca do novo endereco, conferida: o setor 0xC0000 (e so ele) guarda um bitmap em 0xC0000 e
**64 registros de 60 B** em 0xC0008, cada um com soma de 16 bits nos 2 ultimos bytes (a partida
valida e copia 73 B para a RAM em 0x6EC0); quem apaga/grava e 0x01E3FE1A. Nossos setores comecam no
seguinte (0xC1000), fora dele. Varredura de todas as chamadas de leitura/apagar/gravar do firmware:
0x0-0x77FF canais, 0x8000, 0x9000, 0xA000, 0xB000, 0xC000, 0xD000, 0xE000, 0xF000-0xF24F e 0xC0000 -
nada entre 0x10000 e 0xBFFFF nem acima de 0xC1000. O que NAO da para garantir e o que as ferramentas
do fabricante escrevem (foi assim que o logo em 0x10000 nos pegou); por isso a trava `setor_livre()`
fica: o pior caso agora e "os bancos param de gravar", nao "comeu algo".

**A FAIXA ERA NOSSA, E PIOR DO QUE REPINTURA (2026-10-24, v9):** o **logo que o CPS grava mora na
memoria de dados, em 0x10000** - exatamente onde eu tinha posto os bancos. O cabecalho (0x10000) e o
banco 1 (0x11000) apagavam os primeiros **8 KB da imagem** = 4096 pixels = ~25 linhas do topo, que
e a faixa (flash apagada = 0xFF = branco em RGB565). Encaixa tudo: logo novo limpo -> v7 limpa ->
o usuario usou o BANK -> faixa voltou; e todas as versoes que mostraram faixa gravavam em 0x10000.
Correcao na v9: **cabecalho em 0xC1000 e bancos em 0xC2000 + n*0x1000** (acima da area que o proprio
firmware usa em 0xC0000, entao o chip comprovadamente alcanca, e longe de 0x10000), mais uma trava
nova que substitui a do espelho: `setor_livre()` **so grava em setor apagado ou que ja seja nosso**
(le 16 bytes e confere marca/0xFF). Essa trava teria evitado o estrago inteiro.
Entregue **C:\temp\BJ8300_BANK_v9.anc** sha256 abaixo; ordem de gravacao: **v9 primeiro, logo pelo
CPS depois** (senao o firmware antigo apaga a imagem de novo na primeira mexida em banco).
LICAO: antes de escolher endereco de gravacao, nao basta "o firmware nao le aqui" - o CPS/ferramentas
do fabricante tambem escrevem no chip. Conferir que o destino esta apagado e barato e decisivo.

**FAIXA BRANCA: (hipotese anterior, v8) (2026-10-24).** Com o logo novo subido pelo CPS (CPS11) a v7
inicializou LIMPA - e a faixa so voltou **depois de ligar um banco**. Ou seja: era o REVALIDA
mesmo (minha primeira suspeita, que eu tinha descartado porque a imagem estava corrompida e
mascarava o teste). Com banco ligado e o canal atual fora dele, REVALIDA troca o canal e
0x01E39F20 liga **b[0x15367]** (pedido de redesenho); a interface repinta uma banda por cima do
logo, que ja esta na tela quando o nosso gancho roda. Correcao: o gancho da partida mantem o
REVALIDA e **zera b[0x15367] no fim** - o canal fica arrumado e o pedido de repintura some (a tela
inicial e desenhada inteira quando o logo sai). Fora da partida o pedido continua valendo.
Entregue **C:\temp\BJ8300_BANK_v8.anc** sha256 3354e4db..., codigo 1694 B (teto 1696).
LICAO: evidencia colhida com hardware em estado sujo (imagem corrompida) desmente hipotese certa -
antes de descartar, conferir se o cenario de teste esta limpo.

**v7 (2026-10-24): tudo junto.** C:\temp\BJ8300_BANK_v7.anc sha256 9c274418..., codigo 1682 B
(teto 1696: alem disso a tabela de 1568 B nao cabe nos 3264 B livres - margem de 14 B). Entra o
**gancho do scan**: a chamada em **0x01E46EB6** (avanca o canal na varredura, logo depois de ler o
modo SC-REV em b[0x14B76]) passa pelo nosso gancho, que faz o que ela fazia e liga as flags de
redesenho **b[0x15367] e b[0x1536F]** - as mesmas que a rotina 0x01E41022 do firmware liga; a
terceira que ela mexe (b[0x1533C] = 2) ficou de fora porque troca o modo de tela. Isso ataca a
barra de sinal congelada no ultimo canal achado e o numero descasando do nome durante o scan.
FUN_01E458E4 nao recebe argumento e o retorno e descartado pelo chamador, entao o gancho e void.
**Ainda nao testado no radio** - se der tremedeira ou nao resolver, `SCAN_REDESENHA = False` desliga.
Tirados dois clamps redundantes nos exibe() (o motor so chama com valor < maximo) para caber.

**FAIXA BRANCA NO LOGO: NAO E NOSSA (2026-10-24).** Fechado por teste: o usuario gravou o
**firmware de fabrica** e a faixa continuou. A imagem de abertura (particao USERLOGO, 0xF5000 da
flash interna, 40960 B = 160x128 RGB565) esta danificada - flash apagada le 0xFF, e 0xFFFF em 16
bits e branco, por isso a faixa. Antes disso eu tinha descartado, com evidencia: o filtro (a faixa
aparece com todos os bancos desligados), o REVALIDA (a v5 nao o chamava e a faixa ficou), escrita
nossa na memoria de video (desde a v3 nao ha), a tabela de menus realocada (corte MENU_NOVO=False
manteve a faixa) e desvio caindo no meio de remendo (scripts/desvios.py: todos caem no inicio).
Detalhes uteis: a faixa so aparece com POWER-ON MSG = LOGO (com VOLTAGE nao, porque aquela tela e
repintada); some quando o logo sai. O `.anc` NAO carrega essa particao e o gravador so transmite -
quem escreve a flash e o ota.bin (carregador) na RAM. O logo tambem nao e arquivo do res/ (la so ha
tone/*, config.dat, md5.bin, cfg_tool.bin, eq_cfg_hw.bin). Caminho escolhido pelo usuario: subir
logo novo pelo CPS do fabricante. Imagem dele pronta em C:\temp: `bajeton_160x128_rgb565_be.raw`
(40960 B, big-endian conferido decodificando) e `bajeton_160x128.bmp` (24 bits, para o CPS).

**v5 (2026-10-24):** textos 25 cadeias - entram VOICEPRI->ENCRYPT (a pedido) e a versao do
firmware **V0.12 -> V1.0T** em 0x01E5FD10 (o hardware V01T32, em 0x01E5FD86, fica como esta).
Entregue **C:\temp\BJ8300_BANK_v5.anc** sha256 1a88010b...; a v4 foi apagada para nao confundir.
**v6 = v5 + REVALIDA de volta na partida** (o usuario preferiu assim: sem ele, trocar para canal
fora do banco deixaria o radio parado nele): C:\temp\BJ8300_BANK_v6.anc sha256 b80c5a9b...

**Roger beep (levantamento parcial, 2026-10-24):** ajuste em b[0x14B83] (0=OFF, 1=BEEP, 2=TONE1200);
o tocador fica em **0x01E44606**. No BEEP ele chama 0x01E43B4E duas vezes com valores lidos de uma
tabelinha (0x1E60DCC=10324 e 0x1E60DC8=8776), com espera de 40 entre elas - o laco em 0x1E44640 anda
de 4 em 4 bytes para tras e o numero de tons vem de constantes no codigo (r6=17, limite 16). No
TONE1200 ele programa o hardware com uma sequencia fixa de (registrador, valor) a partir de
0x01E44670. Perto da tabela ha uma tabela de notas em float (0x41DC225C = 27.5 = A0). Falta saber o
que 0x01E43B4E(r0, r1) faz com r0 (Hz? indice? palavra de DDS) antes de prometer mudar o som.

**Revisao de 2026-10-23 (double check pedido pelo usuario), tudo conferido no desmontado:**
- **Nao brica, provado:** a inicializacao do app le as teclas em 0x1E56C70/C7E/C8C, com o combo
  desenha UPDATE (0x1E56C9A), poe DAT_0001533E=5 e em **0x1E56CB0 da um goto que passa por cima
  da chamada 0x1E56CB6** - a unica que chama FUN_01E56084, dentro do qual fica o gancho. Em modo
  UPDATE o codigo novo nao executa um byte; travar na partida nao impede regravar.
- APAGA_SETOR manda 0x20 (setor de 4 KB) e GRAVA_FLASH fatia em paginas de 256 B (qualquer
  tamanho serve); as duas usam o mesmo auxiliar de SPI (0x1E101BA) e o mesmo bloco 0x1E0200 do
  LE_FLASH, ou seja **so falam com o chip de dados**, nunca com a flash do firmware.
- **Menu principal = tabela 0x1E62F98**, carregada no laco da UI em 0x1E4CBB2 (`r0 = 13368;
  r5 = r6 + r0`, com r6 = 0x1E5FB60) - por isso procurar o valor 0x1E62F98 ou "+ 13368" no
  desmontado nao acha nada. A contagem vem de FUN_01E4A81A: a tabela acaba no primeiro item com
  os 28 bytes zerados -> **53 itens**, e os nossos ficam nos indices 11, 31 e 47: todos visiveis.
  Os outros menus vem do vetor de ponteiros em 0x1E5E7A0 (ids 1..10, tabelas de 2 itens; id 10 =
  o ZONE solto em 0x1E5E76C, que continua funcionando) e de 0x1E61168/0x1E6137C (2/5/6 itens,
  escolhidos por b[0x12A5B]).
- O byte +0x1B do item e o numero do aviso falado (FUN_01E38BD4, so fala com VOICE ligado): fica
  herdado, cosmetico.
- Empacotamento: as entradas JLFS PRCT/BTIF/USER/USERLOGO tem crc=0xFFFF (sao descritores de
  particao, sem CRC de dados), o CRC de app_area_head cobre so [0x20:0x781DC] e o blob fica fora;
  o `diferencas.py` mostra que mudam apenas cabecalho/tabela do UFW, as 2 entradas JLFS do app, o
  gancho (3 B), os 3 itens (16+18+18 B) e a area do blob - **nada antes do diretorio (uboot)**.
- Corrigido na revisao: o OK no BANK nao gravava o banco escolhido quando o banco estava vazio, o
  que impedia encher qualquer banco diferente do 1; agora o OK sempre escolhe (e so grava se algo
  mudou). Depois disso o usuario pediu para nao perder funcao nenhuma, e a solucao passou a ser a
  copia da tabela de menus descrita acima.

Ferramentas novas: `scripts/diferencas.py` (o que mudou de um .anc para outro), `scripts/desmonta.sh` (llvm-objdump 4.0.1 nao le binario cru: o app.bin entra
num .elf por .incbin no endereco real; caminho em C:/... pelo cygpath) e `scripts/chamadas.py`
(quem chama / o que chama, decodificando `call jaddr23`, sem Ghidra).
Relacionado: [[bj8300-mapa-do-codigo]], [[firmware-baojie-bj8300-jieli]].
