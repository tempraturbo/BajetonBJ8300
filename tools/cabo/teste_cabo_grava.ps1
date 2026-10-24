# Prova o comando estendido de GRAVACAO do firmware dos bancos (v12+), sem deixar rastro.
#
# Uso: powershell -ExecutionPolicy Bypass -File teste_cabo_grava.ps1 -Porta COM3 [-Banco 16]
#
# Escreve um registro de banco completo (marca BCO1 + nome + mapa de canais + soma) no setor do
# banco escolhido, le de volta para conferir byte a byte, e no fim apaga o setor (um bloco de 0xFF
# no inicio do setor faz o firmware apagar e nao gravar nada util). O padrao e o banco 16, que o
# mapa mostrou vazio; qualquer outro banco com conteudo seria sobrescrito, por isso o aviso.
param(
    [string]$Porta = 'COM3',
    [int]$Banco = 16,
    [string]$Nome = 'TESTE CABO',
    [switch]$NaoLimpar
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'cabo.ps1')

$SETOR = 0x000C2000 + ($Banco - 1) * 0x1000
$TAM_NOME = 12
$TAM_MAPA = 120
$TAM_REG = 4 + $TAM_NOME + $TAM_MAPA + 2      # 138, igual ao struct banco do firmware

function Soma-De($dados, $n) {
    # Mesma conta do firmware (soma_de): comeca em 0x1234 e vai somando com deslocamento.
    $soma = 0x1234
    for ($i = 0; $i -lt $n; $i++) { $soma = (($soma -shl 1) + $dados[$i]) -band 0xFFFF }
    return $soma
}

$p = $null
try {
    $p = Abre-Cabo $Porta
    Write-Host ''
    Write-Host ("setor do banco {0}: {1:X6}" -f $Banco, $SETOR)

    $antes = Le-Bloco $p $SETOR -Estendido
    Write-Host ('1. como esta agora: ' + (Hex $antes[0..11]) + '   |' + (Texto $antes[0..11]) + '|')
    if ($antes | Where-Object { $_ -ne 0xFF }) {
        Write-Host '   ATENCAO: o setor tem conteudo; o teste vai sobrescrever este banco.'
    }

    # monta o registro: BCO1 + nome (12) + mapa zerado (120) + soma
    $reg = New-Object byte[] $TAM_REG
    [byte[]][char[]]'BCO1' | ForEach-Object -Begin { $i = 0 } -Process { $reg[$i++] = $_ }
    $n = [byte[]][char[]]$Nome
    for ($i = 0; $i -lt [Math]::Min($n.Length, $TAM_NOME); $i++) { $reg[4 + $i] = $n[$i] }
    $soma = Soma-De $reg ($TAM_REG - 2)
    $reg[$TAM_REG - 2] = $soma -band 0xFF
    $reg[$TAM_REG - 1] = ($soma -shr 8) -band 0xFF

    # grava em blocos de 64 B: o primeiro bloco apaga o setor
    $blocos = [Math]::Ceiling($TAM_REG / 64)
    for ($b = 0; $b -lt $blocos; $b++) {
        $bloco = New-Object byte[] 64
        for ($i = 0; $i -lt 64; $i++) {
            $o = $b * 64 + $i
            $bloco[$i] = if ($o -lt $TAM_REG) { $reg[$o] } else { 0xFF }
        }
        Grava-Bloco $p ($SETOR + $b * 64) $bloco
    }
    Write-Host "2. gravados $blocos blocos de 64 B (registro de $TAM_REG B, soma $('0x{0:X4}' -f $soma))"

    # le de volta e compara
    $erros = 0
    for ($b = 0; $b -lt $blocos; $b++) {
        $lido = Le-Bloco $p ($SETOR + $b * 64) -Estendido
        for ($i = 0; $i -lt 64; $i++) {
            $o = $b * 64 + $i
            $esperado = if ($o -lt $TAM_REG) { $reg[$o] } else { 0xFF }
            if ($lido[$i] -ne $esperado) { $erros++ }
        }
        if ($b -eq 0) { Write-Host ('3. lido de volta:  ' + (Hex $lido[0..11]) + '   |' + (Texto $lido[0..11]) + '|') }
    }
    Write-Host $(if ($erros -eq 0) { "   confere byte a byte: GRAVACAO PELO CABO FUNCIONA" }
                 else { "   $erros bytes diferentes: a gravacao NAO esta certa" })

    # confere que o firmware recusa gravar fora da area dos bancos
    $alvo = 0x000B0000
    $antes_fora = Le-Bloco $p $alvo -Estendido
    Grava-Bloco $p $alvo (New-Object byte[] 64)
    $depois_fora = Le-Bloco $p $alvo -Estendido
    $intacto = -not (Compare-Object $antes_fora $depois_fora -SyncWindow 0)
    Write-Host ('4. tentativa de gravar em {0:X6} (fora da area dos bancos): ' -f $alvo) -NoNewline
    Write-Host $(if ($intacto) { 'ignorada, memoria intacta - a trava funciona' } else { 'ESCREVEU! a trava falhou' })

    if (-not $NaoLimpar) {
        $ff = New-Object byte[] 64
        for ($i = 0; $i -lt 64; $i++) { $ff[$i] = 0xFF }
        Grava-Bloco $p $SETOR $ff                      # apaga o setor e nao grava nada util
        $final = Le-Bloco $p $SETOR -Estendido
        $limpo = -not ($final | Where-Object { $_ -ne 0xFF })
        Write-Host ('5. limpeza: ' + $(if ($limpo) { 'setor de volta apagado, nada ficou no radio' } else { 'o setor NAO voltou a apagado' }))
    } else {
        Write-Host "5. -NaoLimpar: o banco $Banco ficou gravado como `"$Nome`" (da para ver no menu BANK)"
    }
} finally { Fecha-Cabo $p }
