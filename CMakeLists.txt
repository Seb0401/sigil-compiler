# ============================================================
#  CMakeLists.txt — Compilador Sigil
#  Compila el ejecutable `sigilc` a partir de src/*.cpp
#
#  Visual Studio:  Archivo > Abrir > Carpeta  (detecta CMake)
#  VS Code:        extension CMake Tools  ->  Build
#  Terminal:       cmake -B build && cmake --build build
# ============================================================
cmake_minimum_required(VERSION 3.15)
project(SigilCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Coloca el ejecutable en la raiz del proyecto para que el
# servidor web lo encuentre facilmente (sigilc / sigilc.exe).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(sigilc ${SOURCES})

if(MSVC)
    target_compile_options(sigilc PRIVATE /utf-8 /W3)
else()
    target_compile_options(sigilc PRIVATE -Wall -Wextra)
endif()
