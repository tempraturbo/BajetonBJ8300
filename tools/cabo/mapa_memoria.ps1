# Mapeia a memoria de dados do BJ-8300 inteira pelo cabo, usando a leitura estendida do firmware
# dos bancos (v12 em diante). SO LE: nenhuma funcao daqui manda 'W'.
#
# Uso (radio ligado em modo normal, CHIRP/CPS fechados):
#   powershell -ExecutionPolicy Bypass -File mapa_memoria.ps1 -Porta COM3
#
# Responde quatro perguntas:
#   1. a leitura de 24 bits funciona? (o cabecalho dos bancos em 0xC1000 tem que comecar com BNK3)
#   2. os dados vem crus ou cifrados? (compara o mesmo endereco pelo comando de fabrica e pelo novo)
#   3. qual o tamanho DE VERDADE do chip? (procura em que potencia de 2 o conteudo comeca a
#      espelhar o endereco 0, que tem canal gravado e por isso nao e 0xFF)
#   4. o que existe acima de 64 KB? (varre setor por setor ate o tamanho achado e lista o que tem
#      conteudo - e assim que se acha a imagem de abertura que o CPS grava)
param(
    [string]$Porta = 'COM3',
    [string]$Saida = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'cabo.ps1')

$p = $null
try {
    $p = Abre-Cabo $Porta

    # --- 1. a leitura estendida funciona? ---
    $cab = Le-Bloco $p 0x0C1000 -Estendido
    $marca = Texto $cab[0..3]
    Write-Host ''
    Write-Host ("1. 0xC1000 (cabecalho dos bancos): " + (Hex $cab[0..11]) + "   |$marca|")
    if ($marca -eq 'BNK3') {
        $ativos = $cab[4] + 256 * $cab[5]
        $ligados = (0..15 | Where-Object { ($ativos -shr $_) -band 1 } | ForEach-Object { $_ + 1 }) -join ','
        Write-Host ("   leitura de 24 bits OK. bancos ligados: " + $(if ($ligados) { $ligados } else { 'nenhum' }) +
                    "; banco escolhido: $($cab[6] + 1)")
    } else {
        Write-Host '   NAO veio BNK3: ou o firmware nao e o v12+, ou nenhum banco foi gravado ainda.'
    }

    # --- 2. crus ou cifrados? ---
    $fab = Le-Bloco $p 0x0000
    $est = Le-Bloco $p 0x000000 -Estendido
    $igual = -not (Compare-Object $fab $est -SyncWindow 0)
    Write-Host ''
    Write-Host ('2. endereco 0x0000 de fabrica: ' + (Hex $fab[0..15]))
    Write-Host ('            pelo novo comando: ' + (Hex $est[0..15]))
    Write-Host ('   ' + $(if ($igual) { 'iguais -> o radio entrega sempre o byte cru; a cifra esta na propria memoria' }
                          else { 'diferentes -> o caminho de fabrica cifra a resposta; o comando novo entrega cru' }))

    # --- 3. tamanho real do chip ---
    $zero = Le-Bloco $p 0x000000 -Estendido
    $tamanho = 0
    foreach ($t in 0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000) {
        $b = Le-Bloco $p $t -Estendido
        $espelha = -not (Compare-Object $zero $b -SyncWindow 0)
        Write-Host ('3. {0,9:X6}: {1}  {2}' -f $t, (Hex $b[0..7]), $(if ($espelha) { '= endereco 0 (ESPELHO)' } else { '' }))
        if ($espelha) { $tamanho = $t; break }
    }
    if ($tamanho) {
        Write-Host ("   o chip tem $($tamanho / 1024) KB: dai para cima o endereco volta ao inicio")
    } else {
        Write-Host '   nenhum espelho ate 4 MB: o chip tem 4 MB ou mais (ou os blocos lidos sao todos 0xFF)'
        $tamanho = 0x400000
    }

    # --- 4. o que existe acima de 64 KB ---
    Write-Host ''
    Write-Host '4. setores de 4 KB com conteudo (le os 64 primeiros bytes de cada um):'
    $ocupados = @()
    for ($s = 0x10000; $s -lt $tamanho; $s += 0x1000) {
        $b = Le-Bloco $p $s -Estendido
        if ($b | Where-Object { $_ -ne 0xFF }) { $ocupados += $s }
    }
    if ($ocupados.Count -eq 0) {
        Write-Host '   nada acima de 64 KB (tirando o que estiver em offset != 0 dentro do setor)'
    } else {
        # agrupa em faixas corridas para a lista nao ficar quilometrica
        $ini = $ocupados[0]; $ant = $ini
        foreach ($s in $ocupados[1..($ocupados.Count - 1)] + @(-1)) {
            if ($s -ne $ant + 0x1000) {
                Write-Host ('   {0:X6}..{1:X6}  ({2} setor(es))' -f $ini, ($ant + 0xFFF), (($ant - $ini) / 0x1000 + 1))
                $ini = $s
            }
            $ant = $s
        }
    }

    # --- opcional: despeja tudo num arquivo ---
    if ($Saida) {
        Write-Host ''
        Write-Host "despejando 0x0 ate $('{0:X6}' -f $tamanho) em $Saida ..."
        $fs = [IO.File]::Create($Saida)
        try {
            for ($a = 0; $a -lt $tamanho; $a += 0x40) {
                $fs.Write((Le-Bloco $p $a -Estendido), 0, 0x40)
                if (($a % 0x10000) -eq 0) { Write-Host ('   {0:X6}...' -f $a) }
            }
        } finally { $fs.Close() }
        Write-Host "pronto: $Saida"
    }
    Write-Host ''
    Write-Host 'nada foi escrito no radio.'
} finally { Fecha-Cabo $p }
