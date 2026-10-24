# Confere a memoria de dados do BJ-8300 pelo cabo, SEM ESCREVER NADA.
#
# Uso (no PC do radio, com o radio ligado em modo normal e o CHIRP fechado):
#   powershell -ExecutionPolicy Bypass -File ler_memoria.ps1 -Porta COM3
#
# O que faz: entra em modo de programacao como o CHIRP faz (driver Radtel RT-920), le os 64 KB que
# o protocolo alcanca (o comando 'R' usa endereco de 16 bits), decifra e responde tres perguntas:
#   1. as marcas dos bancos ("BNK3"/"BCO1") aparecem nos 64 KB baixos? Se aparecerem, o endereco
#      0xC1000 esta espelhando em cima de area util - e o unico jeito de a gravacao fazer estrago;
#   2. os canais continuam inteiros (conta os gravados e mostra os primeiros nomes);
#   3. quais setores de 4 KB tem conteudo, para comparar com o mapa que levantamos no firmware.
#
# So usa os comandos de leitura: magic, 'F' (identificacao), 'S' (cifra), 'R' (ler) e 'E' (sair).
# Nunca manda 'W'. Pode rodar quantas vezes quiser.
param(
    [string]$Porta = 'COM3',
    [string]$Saida = 'dump_bj8300_0000_FFFF.bin'
)
$ErrorActionPreference = 'Stop'

$MAGIC = [byte[]][char[]]'PROGRAMBT11U'
$CRYPTSETUP = [byte[]]@(0x53,0x45,0x4E,0x44,0x20,0x01,0x01) + [byte[]]@(0) * 18
$ACK = 0x06
$BLOCO = 0x40
$FIM = 0x10000            # o comando 'R' leva endereco de 16 bits: nao alcanca alem disto
$CIFRA_ATE = 0xF000       # acima disto o radio entrega os bytes crus (area de calibracao)
$CHAVE = [byte[]][char[]]'CO 7'

function Le-Exato($porta, $quantos) {
    $buf = New-Object byte[] $quantos
    $lidos = 0
    while ($lidos -lt $quantos) {
        $n = $porta.Read($buf, $lidos, $quantos - $lidos)
        if ($n -le 0) { throw "o radio parou de responder (faltaram $($quantos - $lidos) bytes)" }
        $lidos += $n
    }
    return $buf
}

function Decifra($dados) {
    # Mesma regra do driver do CHIRP: XOR com a chave, pulando 0x00, 0xFF, espaco na chave, e os
    # bytes iguais a chave ou ao inverso dela. O indice da chave anda a cada byte.
    $saida = New-Object byte[] $dados.Length
    for ($i = 0; $i -lt $dados.Length; $i++) {
        $b = $dados[$i]
        $k = $CHAVE[$i % 4]
        if ($b -ne 0x00 -and $b -ne 0xFF -and $k -ne 0x20 -and $b -ne $k -and $b -ne ($k -bxor 0xFF)) {
            $saida[$i] = $b -bxor $k
        } else {
            $saida[$i] = $b
        }
    }
    return $saida
}

function Acha($dados, $texto) {
    $alvo = [byte[]][char[]]$texto
    $onde = @()
    for ($i = 0; $i -le $dados.Length - $alvo.Length; $i++) {
        $bate = $true
        for ($j = 0; $j -lt $alvo.Length; $j++) {
            if ($dados[$i + $j] -ne $alvo[$j]) { $bate = $false; break }
        }
        if ($bate) { $onde += $i }
    }
    return $onde
}

$p = New-Object System.IO.Ports.SerialPort $Porta, 57600, 'None', 8, 'One'
$p.ReadTimeout = 1500
$p.WriteTimeout = 1500
Write-Host "abrindo $Porta a 57600..."
$p.Open()
try {
    $p.DiscardInBuffer(); $p.DiscardOutBuffer()

    # 1. modo de programacao: o magic ate cinco vezes, esperando o ACK
    $ok = $false
    for ($tentativa = 1; $tentativa -le 5 -and -not $ok; $tentativa++) {
        $p.Write($MAGIC, 0, $MAGIC.Length)
        try { $ok = (Le-Exato $p 1)[0] -eq $ACK } catch { $ok = $false }
    }
    if (-not $ok) { throw 'o radio nao aceitou o modo de programacao (ligado? cabo? CHIRP aberto?)' }

    # 2. identificacao e 3. configuracao da cifra
    $p.Write([byte[]][char[]]'F', 0, 1)
    $ident1 = Le-Exato $p 8
    $ident2 = Le-Exato $p 8
    $p.Write($CRYPTSETUP, 0, $CRYPTSETUP.Length)
    if ((Le-Exato $p 1)[0] -ne $ACK) { throw 'o radio recusou a configuracao da cifra' }
    Write-Host ('identificacao: ' + (($ident1 | ForEach-Object { '{0:X2}' -f $_ }) -join ' '))
    Write-Host ('   secundaria: ' + (($ident2 | ForEach-Object { '{0:X2}' -f $_ }) -join ' ') +
                '   (esperado 02 00 02 60 01 03 30 04)')

    # 4. leitura dos 64 KB
    $cru = New-Object byte[] $FIM
    for ($endereco = 0; $endereco -lt $FIM; $endereco += $BLOCO) {
        $cmd = [byte[]]@(0x52, [byte](($endereco -shr 8) -band 0xFF), [byte]($endereco -band 0xFF), [byte]$BLOCO)
        $p.Write($cmd, 0, 4)
        $resposta = Le-Exato $p (4 + $BLOCO)
        for ($j = 0; $j -lt 4; $j++) {
            if ($resposta[$j] -ne $cmd[$j]) { throw ('resposta fora de passo em {0:X4}' -f $endereco) }
        }
        [Array]::Copy($resposta, 4, $cru, $endereco, $BLOCO)
        if (($endereco % 0x2000) -eq 0) { Write-Host ('  lendo {0:X4}...' -f $endereco) }
    }
    $p.Write([byte[]][char[]]'E', 0, 1)      # sai do modo de programacao
} finally {
    if ($p.IsOpen) { $p.Close() }
}

# decifra so a parte que o radio cifra
$dados = New-Object byte[] $FIM
[Array]::Copy((Decifra $cru[0..($CIFRA_ATE - 1)]), 0, $dados, 0, $CIFRA_ATE)
[Array]::Copy($cru, $CIFRA_ATE, $dados, $CIFRA_ATE, $FIM - $CIFRA_ATE)
[IO.File]::WriteAllBytes($Saida, $dados)
Write-Host "`ndump salvo em $Saida ($FIM bytes, ja decifrado)`n"

# --- 1. as marcas dos bancos aparecem aqui? (seria espelhamento de endereco) ---
$achados = @()
foreach ($marca in 'BNK3', 'BCO1') {
    foreach ($onde in (Acha $dados $marca)) { $achados += ('{0} em {1:X4}' -f $marca, $onde) }
    foreach ($onde in (Acha $cru $marca))   { $achados += ('{0} (cru) em {1:X4}' -f $marca, $onde) }
}
if ($achados.Count -eq 0) {
    Write-Host '1. marcas dos bancos nos 64 KB baixos: NENHUMA -> 0xC1000 nao espelha aqui (bom)'
} else {
    Write-Host '1. ATENCAO: marcas dos bancos encontradas nos 64 KB baixos:'
    $achados | ForEach-Object { Write-Host "     $_" }
    Write-Host '   isso significa que a gravacao dos bancos esta caindo em area util. PARAR DE USAR.'
}

# --- 2. canais inteiros? ---
$gravados = 0
$nomes = @()
for ($c = 0; $c -lt 960; $c++) {
    $o = $c * 32
    if ($dados[$o] -ne 0xFF -and $dados[$o + 3] -ne 0) {
        $gravados++
        if ($nomes.Count -lt 8) {
            $n = ($dados[($o + 0x14)..($o + 0x1F)] | Where-Object { $_ -ge 32 -and $_ -lt 127 } |
                  ForEach-Object { [char]$_ }) -join ''
            $nomes += ('{0:d3}="{1}"' -f ($c + 1), $n)
        }
    }
}
Write-Host "2. canais gravados: $gravados"
Write-Host ('   primeiros nomes: ' + ($nomes -join '  '))
Write-Host '   (se os nomes sairem legiveis, a decifragem esta certa e os canais estao intactos)'

# --- 3. mapa de ocupacao ---
Write-Host '3. setores de 4 KB com conteudo:'
for ($s = 0; $s -lt $FIM; $s += 0x1000) {
    $vazio = $true
    for ($i = $s; $i -lt $s + 0x1000; $i++) { if ($cru[$i] -ne 0xFF) { $vazio = $false; break } }
    if (-not $vazio) { Write-Host ('     {0:X5}' -f $s) }
}
Write-Host "`nconferencia terminada. Nada foi escrito no radio."
