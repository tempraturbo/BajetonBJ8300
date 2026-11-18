@echo off
rem Compila o TempraLink-BJ8300.exe na raiz do repositorio, depois de passar nos testes (Tempra Link build).
rem Precisa do Delphi; testado com o RAD Studio 12. Outra versao: defina BDS com a pasta dela antes.
rem Com o Python e o CHIRP instalados, os testes conferem tambem contra o driver do CHIRP (testes\fixtures.py); sem
rem eles, os testes usam o radio.bin e o esperado.txt que ja estao no repositorio.
setlocal
cd /d "%~dp0"
if not defined BDS set "BDS=%ProgramFiles(x86)%\Embarcadero\Studio\23.0"
if not exist "%BDS%\bin\dcc32.exe" (
    echo Nao achei o Delphi em "%BDS%". Defina BDS com a pasta dele e rode de novo.
    goto erro
)
if not exist dcu mkdir dcu
set CHIRP=
python -I -c "import chirp" >nul 2>&1 && set CHIRP=1

copy /y "%BDS%\Redist\styles\vcl\Windows10Dark.vsf" estilo.vsf >nul || goto erro
rem o cgrc (o rc.exe da Microsoft), porque o brcc32 nao aceita o icone com PNG dentro
"%BDS%\bin\cgrc.exe" -r -foRecursos.res Recursos.rc >nul || goto erro
if defined CHIRP python -I testes\fixtures.py gera || goto erro
"%BDS%\bin\dcc32.exe" -B -Q -NUdcu -Edcu -Utestes testes\TesteTempraLink.dpr || goto erro
dcu\TesteTempraLink.exe . || goto erro
if defined CHIRP python -I testes\fixtures.py confere || goto erro
"%BDS%\bin\dcc32.exe" -B -Q -NUdcu -Edcu TempraLink.dpr || goto erro
copy /y dcu\TempraLink.exe ..\TempraLink-BJ8300.exe >nul || goto erro
echo.
echo Pronto: ..\TempraLink-BJ8300.exe
goto fim

:erro
echo.
echo FALHOU.
:fim
if "%~1"=="" pause
