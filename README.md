# Sigil — Compilador didáctico

Compilador completo para un lenguaje propio (**Sigil**, archivos `.sg`) con
sintaxis tipo C++ y operadores nuevos. Incluye las cinco fases clásicas y una
**interfaz web** para visualizarlas.

```
Léxico  →  Sintáctico (CST + AST)  →  Semántico  →  Código intermedio (TAC)  →  Ejecución
```

## Operadores propios de Sigil

| Operador | Significado            | Ejemplo                       | Resultado |
|----------|------------------------|-------------------------------|-----------|
| `**`     | potencia               | `2 ** 8`                      | `256`     |
| `..`     | rango (en `for in`)    | `for i in 1..5 { }`           | 1,2,3,4,5 |
| `\|>`    | pipe (encadenar)       | `5 \|> doble \|> incrementar` | `11`      |
| `%%`     | divisible (bool)       | `10 %% 5`                     | `true`    |
| `><`     | swap (intercambio)     | `a >< b;`                     | —         |
| `<=>`    | comparación 3 vías     | `3 <=> 7`                     | `-1`      |
| `&& \|\| !` | lógicos             | `a > 0 && b > 0`              | `bool`    |

**Arreglos:** `int[] v = [1,2,3];  v[0]=9;  for x in v { }  len(v)`
**Funciones de cadena:** `upper, lower, substr(s,i,n), charAt(s,i), indexOf(s,sub), len(s)`
**Matemáticas:** `sqrt, abs, pow, max, min, toInt, toFloat, toString`

## Estructura

```
src/        código C++ del compilador (lexer, parser, semántico, TAC, intérprete)
web/        interfaz web (HTML/CSS/JS)
server/     servidor local en Python (invoca el binario y sirve la web)
examples/   programas de ejemplo (.sg)
CMakeLists.txt
```

## 1) Compilar el compilador

### Opción A — Visual Studio 2022
1. Instala la carga de trabajo **"Desarrollo para el escritorio con C++"**
   (Visual Studio Installer → Modificar). *(Tu VS actual no la tiene aún.)*
2. `Archivo → Abrir → Carpeta…` y elige esta carpeta. VS detecta el `CMakeLists.txt`.
3. Selecciona el objetivo **sigilc** y pulsa *Compilar*.

### Opción B — VS Code
1. Extensiones: **C/C++** y **CMake Tools**.
2. Abre la carpeta → `CMake: Configure` → `CMake: Build`.

### Opción C — Terminal (si tienes g++ o cl)
```bash
cmake -B build
cmake --build build
```
El ejecutable queda en `bin/sigilc` (o `bin/sigilc.exe`).

### Probar el compilador por consola
```bash
bin/sigilc examples/01_basico.sg            # salida legible (todas las fases)
bin/sigilc examples/01_basico.sg --resumen  # resumen compacto de todas las fases
bin/sigilc examples/01_basico.sg --json     # salida JSON (la usa la web)

sigil.bat                                   # ejecuta programa.sg (resumen)
sigil.bat examples/06_arrays.sg             # ejecuta otro archivo
```

## 2) Lanzar la interfaz web

**Un clic:** ejecuta `iniciar_web.bat` (compila si hace falta, abre el navegador y lanza el servidor).

**Manual:**
```bash
python server/server.py
```
Abre **http://localhost:8000**. Escribe código (o carga un ejemplo) y pulsa
**Compilar y ejecutar** (o `Ctrl+Enter`). Verás pestañas con Tokens, Símbolos,
AST, Código Intermedio, Salida y Errores.

> El servidor solo usa la biblioteca estándar de Python (no requiere `pip`).
> Busca el binario `sigilc` en `bin/`, `build/`, etc. Si no lo encuentra, te
> avisa para que lo compiles primero.

## Renombrar el lenguaje

Cambia `LANG_NAME` / `LANG_EXT` en `src/config.hpp` y el título en
`web/index.html`. La lógica no depende del nombre.
