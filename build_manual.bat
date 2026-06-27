@echo off
set "MSVC=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433"
set "SDK=C:\Program Files (x86)\Windows Kits\10"
set "SDKV=10.0.22621.0"
set "PATH=%MSVC%\bin\Hostx64\x64;%PATH%"
set "INCLUDE=%MSVC%\include;%SDK%\Include\%SDKV%\ucrt;%SDK%\Include\%SDKV%\shared;%SDK%\Include\%SDKV%\um"
set "LIB=%MSVC%\lib\x64;%SDK%\Lib\%SDKV%\ucrt\x64;%SDK%\Lib\%SDKV%\um\x64"
if not exist bin mkdir bin
cl /nologo /EHsc /std:c++17 /utf-8 /W3 src\*.cpp /Fe:bin\sigilc.exe /Fo:bin\
