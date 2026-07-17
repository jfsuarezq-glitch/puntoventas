@echo off
REM ==========================================================================
REM  Compila POS Quiosco a PosQuiosco.exe (Windows, requiere g++ / MinGW)
REM ==========================================================================
setlocal

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] No se encontro g++. Instala MinGW-w64 o MSYS2 y agrega g++ al PATH.
    echo         Alternativa con Visual Studio:
    echo             cl /std:c++17 /EHsc /O2 main.cpp /Fe:PosQuiosco.exe
    pause
    exit /b 1
)

echo Compilando PosQuiosco.exe ...
g++ -std=c++17 -O2 -static -o PosQuiosco.exe main.cpp
if errorlevel 1 (
    echo [ERROR] Fallo la compilacion.
    pause
    exit /b 1
)

echo.
echo [OK] Listo. Se genero PosQuiosco.exe
echo Ejecutalo con: PosQuiosco.exe
pause
