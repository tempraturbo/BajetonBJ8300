# Le (e, com -Gravar, escreve) a imagem de abertura do BJ-8300 pelo cabo.
#
#   powershell -ExecutionPolicy Bypass -File logo.ps1 -Porta COM3 -Saida ..\..\logo\atual.bin
#   powershell -ExecutionPolicy Bypass -File logo.ps1 -Porta COM3 -Gravar ..\..\logo\novo.bin
#
# A imagem fica no chip de dados em 0x10000, fora dos 64 KB que o protocolo de fabrica alcanca:
# 160 x 138 pixels em RGB565 = 44160 bytes (0xAC80), ou seja 11 setores de 4 KB (0x10000..0x1AFFF).
# Quem grava isso de fabrica e o CPS11; aqui vai pelo comando estendido do firmware modificado.
#
# LEITURA funciona em qualquer endereco. GRAVACAO so se o firmware permitir a faixa do logo
# (v13 em diante); no v12 a gravacao estendida so entra na area dos bancos e o radio IGNORA o
# resto - o script avisa se os bytes nao mudarem.
param(
    [string]$Porta = 'COM3',
    [string]$Saida = '',
    [string]$Gravar = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'cabo.ps1')

$LOGO = 0x010000
$TAM = 160 * 138 * 2            # 44160

$p = $null
try {
    $p = Abre-Cabo $Porta

    if ($Gravar) {
        $dados = [IO.File]::ReadAllBytes((Resolve-Path $Gravar))
        if ($dados.Length -ne $TAM) { throw "a imagem tem $($dados.Length) bytes; o logo sao $TAM" }
        Write-Host "gravando $Gravar ($TAM bytes) em 0x10000..."
        for ($i = 0; $i -lt $TAM; $i += 0x40) {
            $bloco = New-Object byte[] 0x40
            [Array]::Copy($dados, $i, $bloco, 0, [Math]::Min(0x40, $TAM - $i))
            Grava-Bloco $p ($LOGO + $i) $bloco
            if (($i % 0x2000) -eq 0) { Write-Host ('   {0:X6}...' -f ($LOGO + $i)) }
        }
        $conferir = Le-Bloco $p $LOGO -Estendido
        $igual = $true
        for ($i = 0; $i -lt 0x40; $i++) { if ($conferir[$i] -ne $dados[$i]) { $igual = $false; break } }
        Write-Host $(if ($igual) { 'gravado e conferido: desligue e ligue o radio para ver' }
                     else { 'O RADIO IGNOROU a gravacao: este firmware nao libera a faixa do logo (precisa do v13)' })
    }

    if ($Saida -or -not $Gravar) {
        if (-not $Saida) { $Saida = Join-Path $PSScriptRoot '..\..\logo\atual.bin' }
        Write-Host "lendo 0x10000..0x$('{0:X}' -f ($LOGO + $TAM - 1)) ($TAM bytes)..."
        $fs = [IO.File]::Create((New-Item -ItemType File -Force -Path $Saida).FullName)
        try {
            for ($i = 0; $i -lt $TAM; $i += 0x40) {
                $b = Le-Bloco $p ($LOGO + $i) -Estendido
                $fs.Write($b, 0, [Math]::Min(0x40, $TAM - $i))
                if (($i % 0x2000) -eq 0) { Write-Host ('   {0:X6}...' -f ($LOGO + $i)) }
            }
        } finally { $fs.Close() }
        Write-Host "salvo em $Saida"
    }
} finally { Fecha-Cabo $p }
