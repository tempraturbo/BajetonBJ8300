# Baixa JDK 21, Ghidra 12.1.4 e o JAR do jadx-headless-mcp, confere o SHA-256 de cada um
# e extrai/copia para C:\tools. Os zips ficam em $dl e sao apagados depois de extraidos.
param([string]$dl, [string]$tools = 'C:\tools')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$itens = @(
  @{ nome = 'jdk';    url = 'https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.12.1%2B1/OpenJDK21U-jdk_x64_windows_hotspot_21.0.12.1_1.zip'
     sha = 'f9d6e191ab098c0d416e7d588a24420a8621cd2f4720dab2459b8b7b2d2d8b4e'; arq = 'jdk21.zip' },
  @{ nome = 'ghidra'; url = 'https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.4_build/ghidra_12.1.4_PUBLIC_20260921.zip'
     sha = 'ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db'; arq = 'ghidra.zip' },
  @{ nome = 'jadx';   url = 'https://github.com/1013503897/jadx-headless-mcp/releases/download/v0.7.1/jadx-headless-mcp-0.7.1-all.jar'
     sha = '6e5eacf500b64292bfb73c49797c1958f6ee44646e43e868039ae7feb573ff75'; arq = 'jadx-headless-mcp-0.7.1-all.jar' }
)

New-Item -ItemType Directory -Force $dl, $tools | Out-Null

# Os tres downloads em paralelo: sao independentes e o gargalo e a rede.
$jobs = foreach ($i in $itens) {
  Start-Job -ArgumentList $i.url, (Join-Path $dl $i.arq) -ScriptBlock {
    param($u, $f)
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $u -OutFile $f -UseBasicParsing
  }
}
$jobs | Wait-Job | Receive-Job
$jobs | Remove-Job

foreach ($i in $itens) {
  $f = Join-Path $dl $i.arq
  $h = (Get-FileHash -Algorithm SHA256 $f).Hash.ToLowerInvariant()
  if ($h -ne $i.sha) { throw "$($i.nome): SHA-256 nao confere ($h)" }
  "$($i.nome): SHA-256 ok"
}

Expand-Archive (Join-Path $dl 'jdk21.zip') $tools -Force
Expand-Archive (Join-Path $dl 'ghidra.zip') $tools -Force
$jadxDir = Join-Path $tools 'jadx-headless-mcp'
New-Item -ItemType Directory -Force $jadxDir | Out-Null
Move-Item -Force (Join-Path $dl 'jadx-headless-mcp-0.7.1-all.jar') $jadxDir

Remove-Item -Recurse -Force $dl
Get-ChildItem $tools | Select-Object Name
