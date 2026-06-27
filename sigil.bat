@echo off
REM ============================================================
REM  sigil.bat - Ejecuta un archivo Sigil y muestra un resumen.
REM     sigil            -> ejecuta programa.sg
REM     sigil archivo.sg -> ejecuta ese archivo
REM ============================================================
setlocal
set "EXE=%~dp0bin\sigilc.exe"
if not exist "%EXE%" (
  echo No se encontro bin\sigilc.exe
  echo Compila primero:  build_manual.bat   ^(o abre la carpeta en Visual Studio^)
  exit /b 1
)
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=%~dp0programa.sg"
"%EXE%" "%ARCH%" --resumen
