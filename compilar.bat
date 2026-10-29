@echo off
rem Compila o firmware do BJ-8300 com dois cliques (BJ-8300 firmware build, double-click).
rem Precisa do Git para Windows e do Python 3.8 ou mais novo; o compilador da JieLi ja vem no
rem repositorio (tools\jl-toolchain-2.5.3). O resultado sai em firmware\build\BJ8300_BANK.anc.
setlocal
cd /d "%~dp0"

rem o bash do Git para Windows, que roda o firmware\build.sh
set "BASH="
for %%P in ("%ProgramFiles%\Git\bin\bash.exe" "%ProgramFiles(x86)%\Git\bin\bash.exe" "%LocalAppData%\Programs\Git\bin\bash.exe") do (
    if not defined BASH if exist %%P set "BASH=%%~P"
)
if not defined BASH (
    for /f "delims=" %%G in ('where git 2^>nul') do (
        if not defined BASH if exist "%%~dpG..\bin\bash.exe" set "BASH=%%~dpG..\bin\bash.exe"
    )
)
if not defined BASH (
    echo.
    echo Nao achei o Git para Windows. Instale de https://git-scm.com/download/win
    echo e rode o compilar.bat de novo.
    echo Git for Windows not found: install it from the link above and run compilar.bat again.
    goto fim
)

python -c "import sys; sys.exit(sys.version_info < (3, 8))" >nul 2>&1
if errorlevel 1 (
    echo.
    echo Nao achei o Python 3.8 ou mais novo. Instale de https://www.python.org/downloads/
    echo marcando "Add python.exe to PATH" na primeira tela, e rode o compilar.bat de novo.
    echo Python 3.8+ not found: install it from the link above, ticking "Add python.exe to PATH".
    goto fim
)

"%BASH%" firmware/build.sh
if errorlevel 1 (
    echo.
    echo *** A COMPILACAO FALHOU / THE BUILD FAILED: veja as mensagens acima. ***
    goto fim
)
echo.
echo Pronto. O firmware esta em: %~dp0firmware\build\BJ8300_BANK.anc
echo Done. The firmware is at the path above. Para gravar, veja o README (Gravar).

:fim
if not "%SEM_PAUSA%"=="1" pause
endlocal
