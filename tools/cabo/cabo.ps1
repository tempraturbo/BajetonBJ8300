# Biblioteca do cabo do BJ-8300: entra em modo de programacao, le blocos, sai.
# Nao escreve nada no radio (nenhuma funcao manda 'W').
#
# Uso:  . .\cabo.ps1 ;  $p = Abre-Cabo COM3 ;  Le-Bloco $p 0x0010 0x40 ;  Fecha-Cabo $p
#
# Framing do protocolo (driver Radtel RT-920 do CHIRP): comando de 1 letra com tamanho fixo por
# letra -- 'R' = 4 bytes (letra, endereco de 16 bits BE, tamanho) e 'W' = 68 (os 4 mais 64 de
# dados). O tratador do radio dispara assim que esses bytes chegam e zera o contador depois, entao
# **nao existe pacote de 5 bytes**: o quinto byte chega tarde, fica pendente e engole o comando
# seguinte (foi o que travou o radio em 2026-10-24).
#
# PAGA CARO: nao mandar NADA antes do magic. O casador do magic compara desde o inicio do buffer
# recebido, entao qualquer byte de enchimento ('E', zeros) faz todas as tentativas seguintes
# falharem - e o radio so volta a aceitar depois de desligar e ligar.
#
# Leitura/gravacao estendida (firmware dos bancos, v12 em diante): **bit 7 do tamanho** ligado, com
# os outros 7 bits valendo o terceiro byte do endereco; o bloco e sempre de 64 bytes. A resposta
# volta com o eco de 4 bytes (ja com 0x40 no lugar do tamanho) mais os 64 bytes.

$MAGIC = [byte[]][char[]]'PROGRAMBT11U'
$CRYPTSETUP = [byte[]]@(0x53, 0x45, 0x4E, 0x44, 0x20, 0x01, 0x01) + [byte[]]@(0) * 18
$ACK = 0x06
$CHAVE = [byte[]][char[]]'CO 7'

function Le-Exato($p, $quantos) {
    $buf = New-Object byte[] $quantos
    $lidos = 0
    while ($lidos -lt $quantos) {
        $n = $p.Read($buf, $lidos, $quantos - $lidos)
        if ($n -le 0) { throw "o radio parou de responder (faltaram $($quantos - $lidos) bytes)" }
        $lidos += $n
    }
    return $buf
}

function Hex($b) { ($b | ForEach-Object { '{0:X2}' -f $_ }) -join ' ' }

function Texto($b) {
    -join ($b | ForEach-Object { if ($_ -ge 32 -and $_ -lt 127) { [char]$_ } else { '.' } })
}

function Decifra($dados) {
    # Mesma regra do CHIRP: XOR com "CO 7", pulando 0x00, 0xFF, espaco na chave, e os bytes iguais
    # a chave ou ao inverso dela.
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

function Entra-Programacao($p) {
    # So o magic, nada mais: ver o aviso no alto do arquivo.
    #
    # PAGA CARO (2026-10-24): insistir no magic quando o radio nao esta ouvindo e perigoso. A
    # cadeia termina em **'U'**, que e comando do modo UPDATE; se o buffer do radio estiver fora de
    # sincronia o casamento falha e esse 'U' sobra como comando solto - o radio entra em UPDATE
    # sozinho. Por isso: tres tentativas no maximo, e qualquer resposta que nao seja o ACK para
    # tudo na hora (o radio esta em outro estado; a saida e desligar e ligar).
    $p.DiscardInBuffer(); $p.DiscardOutBuffer()
    for ($i = 1; $i -le 3; $i++) {
        $p.Write($MAGIC, 0, $MAGIC.Length)
        try {
            $b = (Le-Exato $p 1)[0]
            if ($b -eq $ACK) { return "modo de programacao ok (tentativa $i)" }
            throw ('o radio respondeu 0x{0:X2} em vez do ACK: ele nao esta na tela normal. ' -f $b) +
                  'Desligue e ligue o radio (sem segurar botao) e rode de novo.'
        } catch [TimeoutException] { $p.DiscardInBuffer() }
    }
    throw 'o radio nao respondeu ao magic: desligue e ligue o radio (sem segurar botao), confira ' +
          'que a tela normal apareceu, e feche CHIRP/CPS/BinTalk antes de rodar de novo.'
}

function Identifica($p) {
    $p.Write([byte[]][char[]]'F', 0, 1)
    $id1 = Le-Exato $p 8
    $id2 = Le-Exato $p 8
    $p.Write($CRYPTSETUP, 0, $CRYPTSETUP.Length)
    if ((Le-Exato $p 1)[0] -ne $ACK) { throw 'o radio recusou a configuracao da cifra' }
    return @{ id1 = $id1; id2 = $id2 }
}

function Abre-Cabo($Porta = 'COM3') {
    $p = New-Object System.IO.Ports.SerialPort $Porta, 57600, 'None', 8, 'One'
    $p.ReadTimeout = 900
    $p.WriteTimeout = 1500
    $p.Open()
    $msg = Entra-Programacao $p
    $p.ReadTimeout = 2000
    $ids = Identifica $p
    Write-Host $msg
    Write-Host ('  id1: ' + (Hex $ids.id1) + '   id2: ' + (Hex $ids.id2))
    return $p
}

function Le-Bloco($p, $endereco, [switch]$Estendido) {
    # Estendido: bit 7 do tamanho ligado + os 7 bits do endereco alto, bloco de 64 B (alcanca o chip
    # inteiro, dados crus). De fabrica: endereco de 16 bits, 64 B, dados cifrados abaixo de 0xF000.
    $alto = ($endereco -shr 16) -band 0xFF
    if (-not $Estendido -and $alto -ne 0) { throw ('{0:X6} passa de 16 bits: use -Estendido' -f $endereco) }
    $tamanho = if ($Estendido) { 0x80 -bor $alto } else { 0x40 }
    $cmd = [byte[]]@(0x52, [byte](($endereco -shr 8) -band 0xFF), [byte]($endereco -band 0xFF), [byte]$tamanho)
    $p.Write($cmd, 0, 4)
    $r = Le-Exato $p 68
    # o eco repete os tres primeiros bytes do comando; o quarto volta 0x40 no modo estendido
    if ($r[0] -ne $cmd[0] -or $r[1] -ne $cmd[1] -or $r[2] -ne $cmd[2]) {
        throw ('resposta fora de passo em {0:X6} (eco ' + (Hex $r[0..3]) + ')' -f $endereco)
    }
    return $r[4..67]
}

function Grava-Bloco($p, $endereco, $dados) {
    # SO para a area dos bancos (0xC1000..0xD1FFF): o firmware ignora qualquer outro endereco, e
    # apaga o setor quando o bloco comeca nele. Exige 64 bytes exatos.
    if ($dados.Length -ne 0x40) { throw "Grava-Bloco quer 64 bytes, recebeu $($dados.Length)" }
    $cmd = [byte[]]@(0x57, [byte](($endereco -shr 8) -band 0xFF), [byte]($endereco -band 0xFF),
                     [byte](0x80 -bor (($endereco -shr 16) -band 0xFF))) + $dados
    $p.Write($cmd, 0, $cmd.Length)
    if ((Le-Exato $p 1)[0] -ne $ACK) { throw ('o radio nao confirmou a gravacao em {0:X6}' -f $endereco) }
}

function Fecha-Cabo($p) {
    if ($p -and $p.IsOpen) {
        try { $p.DiscardInBuffer(); $p.Write([byte[]][char[]]'E', 0, 1); Start-Sleep -Milliseconds 100 } catch {}
        $p.Close()
    }
}
