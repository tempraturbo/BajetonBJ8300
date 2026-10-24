---
name: firmware-baojie-bj8300-jieli
description: "Firmware do radio Baojie BJ-8300 (C:\\temp\\BJ8300_V0.12.anc) totalmente decifrado em 2026-10-22: camada KDH (KDHT/RBGI) + UFW JieLi AC695X com chave do chip 0xC70F; o firmware ja tem Zonas."
metadata:
  node_type: memory
  type: project
  originSessionId: 3e4d7b62-afe4-46cc-974a-76214a9ac1a8
  modified: 2026-10-23T05:12:02.231Z
---

O `.anc` do radio HT Baojie BJ-8300 (fabricante real "KDH", vendido tambem como BinTalk; parecido
com Radtel) e um **UFW da JieLi AC695X** (CPU pi32v2, SDK soundbox, PID `YHD15C8_soundbox`) com uma
camada extra da KDH. O gravador e `C:\temp\BinTe ANC Bootloader.exe`: .NET 2.0, namespace
`KDH_Bootloader`, grava pela serial (57600/115200), tem botao de recuperacao e faz POST de
autorizacao (`ancupdate`/`Anc20240311`).

Camadas, todas verificadas pelos CRCs do proprio arquivo (todos conferem) e todas involucoes:
1. KDH (`BootHelper.Decrypt`, lido em IL): blocos de 1 KB, os 2 primeiros e os 2 ultimos intactos;
   bloco%3 -> intacto / XOR `KDHT` / XOR `RBGI`, exceto bytes 0x00, 0xFF, a chave e o inverso dela.
   O indice da chave so anda nos blocos processados. `THDK` existe no .exe, mas nao e usada.
2. UFW: cabecalho e tabela em ENC 0xFFFF; corpos em SFC com a **chave do chip 0xC70F**
   (bloco = chave ^ (offset>>2)); a imagem de flash tem JLFS com CRC por entrada.
O metodo do gamesir-fw (`isd_config.ini` comecando com 32 '#') nao vale aqui. A chave sai do CRC da
entrada `app_area_head`. Ferramenta: `jlfw.py unpack|repack`, em `C:\temp\bj8300-firmware\scripts`. O repack
troca o app.bin nas 6 imagens (3 variantes x EOFFSET; diretorio em 0x3000 ou 0x2200, app.bin
identico nas seis) e recalcula todos os CRCs. A ida e volta sai byte a byte identica ao original.
CRCs: cabecalho[0]=crc(cab[2:]), cab[2]=crc(tabela **cifrada**), entrada UFW = crc(imagem),
JLFS[0]=crc(entrada[2:32]), app_area_head = crc(area em claro [0x20:tam]).
Desmontador de IL sem instalar nada: `il-dump.ps1` (ReflectionOnlyLoadFrom).
**Pipeline comprovado no radio em 2026-10-22**: `C:\temp\BJ8300_V0.1M_teste.anc` (V0.12 -> V0.1M,
app.bin@0x5fc10) gravado; o menu VERSION mostra "Firmware: V0.1M / Hardware: V01T32". Gravacao (o
usuario grava em outro PC, perfil "tempr", porta COM3): ligar o radio segurando **PTT2 + atalho** ->
tela "UPDATE" (desenhada pelo app.bin, string em 0x5fc40) -> botao **Update** normal. O botao "Can't
Work" so escuta 0xAA sem mandar nada e **nao conecta** nesse modo; deve ser para o bootloader com o app
morto (hipotese, nao testada). Log: Update Mode -> Baudrate OK -> Update 3% (envia o loader, provavel
ota.bin) -> Jump To Loader -> Progress 100% -> Finish. Quem grava a flash e o loader na RAM. Se ele
regrava o uboot ainda nao se sabe: da para medir capturando a serial. O medo do usuario e quebrar o
bootloader. Para os bancos, o usuario quis 16 bancos com nome de 12 caracteres
(tamanho do CH-NAME) - entregue assim, criados e editados pelo radio, substituindo as zonas. O CHIRP le o BJ-8300
como Radtel RT-920 (driver radtel_rt900.py): 990 canais x 32 B em 0x0000, settings em 0x9000,
zone_or_channel em 0x9028. Nomes de zona do RT-920 em 0xC800 (zonas fixas de 99, nao livres).
O usuario acha que o Radtel RT-660 tem bancos livres, mas ele nao esta no CHIRP; pedido o firmware dele.

O app.bin (491 KB) **ja tem Zonas e canais de memoria**: menus "Zone Mode", "Full Channel",
"Zone Or Channel", ZONE%02d, CH-%03d, MEMCH, DELCH, SCAN ADD. Tem tambem "GMRS MODE", "SUPER MODE",
"144-146/430-440", e as strings "+IM_AUTH_OK/NG" e "Unauthorized". O REA so serviu no .exe
(inspect_managed_*); o Ghidra nao decompila pi32v2 (kagaimiq/ghidra-jieli esta em estagio inicial).
**Status em 2026-10-23: a feature dos bancos esta pronta e aguarda o teste no radio** (ver
[[bj8300-bancos-progresso]]). De 22 a 23/10 varias sessoes foram cortadas pelo filtro de seguranca
no meio da analise (planejamento, leitura do motor de menus, guia LEIAME); tres /feedback enviados
(recibos d65fe9e1, 76ad1050 e o de 22/10) e e-mail ao Safeguards sem resposta humana. Em 23/10, com
Opus 5, a analise e a correcao sairam inteiras sem corte. Nao tentar contornar o filtro (o usuario
sugeriu trocar a palavra "banco"; recusado) - se voltar a cortar, parar e registrar.
O REA so serviu no .exe (inspect_managed_*); o Ghidra nao decompila pi32v2 bem, e para conferir
patch o melhor e o `llvm-objdump` da JieLi via `scripts/desmonta.sh`.
Relacionado: [[rea-reverse-engineering-mcp]], [[bj8300-mapa-do-codigo]].
