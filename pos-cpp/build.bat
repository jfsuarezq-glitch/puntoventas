@echo off
REM ==========================================================================
REM  Compila POS Quiosco (requiere g++ / MinGW-w64 en el PATH)
REM    - PosQuioscoGUI.exe  -> version con interfaz grafica (ventana)
REM    - PosQuiosco.exe     -> version de consola
REM ==========================================================================
setlocal

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] No se encontro g++. Instala MinGW-w64 o MSYS2 y agrega g++ al PATH.
    pause
    exit /b 1
)

echo Compilando PosQuioscoGUI.exe (interfaz grafica) ...
g++ -std=c++17 -O2 -municode -mwindows -static -o PosQuioscoGUI.exe gui_win32.cpp -lcomctl32 -lgdi32 -luser32
if errorlevel 1 (
    echo [ERROR] Fallo la compilacion de la version grafica.
    pause
    exit /b 1
)

echo Compilando PosQuiosco.exe (consola) ...
g++ -std=c++17 -O2 -static -o PosQuiosco.exe main.cpp
if errorlevel 1 (
    echo [ADVERTENCIA] Fallo la version de consola, pero la grafica ya esta lista.
)

echo.
echo [OK] Listo.
echo   - PosQuioscoGUI.exe  (interfaz grafica, recomendada)
echo   - PosQuiosco.exe     (consola)
pause
