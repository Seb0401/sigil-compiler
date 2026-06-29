@echo off
REM ============================================================
REM  iniciar_web.bat - Arranca todo con un clic:
REM   1) compila sigilc si aun no existe
REM   2) abre el navegador en http://localhost:8000
REM   3) lanza el servidor web (Ctrl+C para detener)
REM ============================================================
cd /d "%~dp0"
if not exist "bin\sigilc.exe" (
  echo [1/3] Compilando el compilador por primera vez...
  call build_manual.bat
  if not exist "bin\sigilc.exe" (
    echo.
    echo No se pudo compilar. Revisa que Visual Studio con C++ este instalado.
    pause
    exit /b 1
  )
)
echo [2/3] Abriendo el navegador...
start "" http://localhost:8000
echo [3/3] Iniciando servidor (Ctrl+C para detener)...
python server\server.py
