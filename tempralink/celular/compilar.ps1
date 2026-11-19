<#
.SYNOPSIS
  Compila o Tempra Link do celular (Android64) sem a IDE e, com -Instala, põe no celular pelo adb e o abre.
.DESCRIPTION
  Sai em Debug, um APK assinado com a chave de depuração do Delphi: no Android64 o Delphi só faz APK em Debug (o
  Release sai .aab). O caminho de busca vai curto e explícito: o library path da IDE estoura a linha de comando do
  dcc. As duas pastas da RTL, release antes de debug (a primeira é a que fornece os .dcu).
  O .deployproj é o que põe no APK a libtelabj.so (a tela do rádio, compilada pelo firmware/previa/tela_pc.py), o
  menu do rádio (menu-bj8300.txt) e o rádio simulado da demonstração (testes/radio.bin e um retrato).
  Com -Windows, compila só o app para o PC (Win64\Debug\TempraLinkCelular.exe), que roda a demonstração com os
  arquivos da pasta do Tempra Link: é para testar as telas sem o celular.
.EXAMPLE
  .\compilar.ps1 -Instala -Aparelho <ip-do-celular>:5555
  .\compilar.ps1 -Windows
#>
param(
  [switch]$Instala,
  # o celular pelo adb do Wi-Fi (ip:porta); vazio, o único ligado pelo cabo
  [string]$Aparelho = '',
  [switch]$Windows
)
$ErrorActionPreference = 'Stop'
$bds = (Get-ItemProperty 'HKCU:\Software\Embarcadero\BDS\23.0').RootDir.TrimEnd('\')
$comum = Join-Path $env:PUBLIC 'Documents\Embarcadero\Studio\23.0'
$adb = Join-Path $comum 'CatalogRepository\Android-SDK\platform-tools\adb.exe'
$projeto = Join-Path $PSScriptRoot 'TempraLinkCelular.dproj'

function MsBuild([string]$Plataforma, [string]$Alvos, [string]$Caminho) {
  $rsp = Join-Path $env:TEMP 'tempralink-celular.rsp'
  @("`"$projeto`"", '/nologo', '/v:minimal', "/t:$Alvos", '/p:Config=Debug', "/p:Platform=$Plataforma",
    "/p:DelphiLibraryPath=`"$Caminho`"") | Set-Content -Path $rsp -Encoding ASCII
  try {
    cmd /c "call `"$bds\bin\rsvars.bat`" && msbuild @`"$rsp`""
    if ($LASTEXITCODE -ne 0) { throw "msbuild falhou ($LASTEXITCODE)" }
  } finally { Remove-Item $rsp -ErrorAction SilentlyContinue }
}

$inicio = Get-Date
if ($Windows) {
  MsBuild 'Win64' 'Build' "$bds\lib\Win64\release;$bds\lib\Win64\debug"
  Write-Host ("Windows em {0:n0} s: {1}" -f ((Get-Date) - $inicio).TotalSeconds,
              (Join-Path $PSScriptRoot 'Win64\Debug\TempraLinkCelular.exe'))
  exit 0
}
MsBuild 'Android64' 'Build;Deploy' "$bds\lib\Android64\release;$bds\lib\Android64\debug;$comum\Dcp\Android64"
$apk = Join-Path $PSScriptRoot 'Android64\Debug\TempraLinkCelular\bin\TempraLinkCelular.apk'
Write-Host ("APK em {0:n0} s: {1} ({2:n1} MB)" -f ((Get-Date) - $inicio).TotalSeconds, $apk, ((Get-Item $apk).Length / 1MB))
if ($Instala) {
  # o adb escreve o progresso no stderr, e com o Stop o PowerShell 5.1 faz disso erro; quem diz se falhou é o código
  # de saída
  $ErrorActionPreference = 'Continue'
  $alvo = @()
  if ($Aparelho -ne '') {
    & $adb connect $Aparelho 2>&1 | Write-Host
    $alvo = @('-s', $Aparelho)
  }
  & $adb @alvo install -r $apk 2>&1 | Write-Host
  if ($LASTEXITCODE -ne 0) { throw "adb install falhou ($LASTEXITCODE)" }
  & $adb @alvo shell monkey -p tempralink.bj8300 -c android.intent.category.LAUNCHER 1 2>&1 | Out-Null
  Write-Host 'Instalado e aberto.'
}
