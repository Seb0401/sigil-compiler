# NOTES.md — Memoria del proyecto Sigil

> Archivo de trabajo para recordar decisiones, estado y pendientes.
> (El entregable académico es la doc en Word/PDF; esto es interno.)

## Qué es

Compilador **didáctico** para un lenguaje propio llamado **Sigil** (archivos `.sg`,
binario `sigilc`). Sintaxis tipo C++ + operadores nuevos. Evoluciona el proyecto
original (lexer + parser en un solo `.txt`) hacia un pipeline completo con UI web.

Integrantes (del PDF): Barreda Bejarano Sebastian Raúl, Collado Cárdenas Marco Antonio,
Larota Ochoa Samith Yowao, Larota Ochoa Linkol Yowao. Curso: Compiladores 2026.

## Decisiones tomadas (2026-06-21)

- Pipeline completo: **Léxico → Sintáctico → Semántico → Código intermedio (TAC) → Intérprete**.
- Sintaxis **tipo C++ + operadores nuevos**.
- Proyecto **multi-archivo** C++17.
- **UI web en localhost**: servidor Python (stdlib, sin pip) que invoca `sigilc` y sirve la web.
- Compilación del usuario: **Visual Studio / VS Code** (CMake). En este entorno NO hay g++,
  así que el C++ no se compila aquí; sí se puede probar el servidor Python.
- Nombre **Sigil** es provisional/renombrable (constante `LANG_NAME` en `src/config.hpp`).

## Lenguaje Sigil — especificación rápida

### Tipos
`int`, `float`, `bool`, `char`, `string`, `void`

### Palabras reservadas
`int float bool char string void return if else while for in break continue true false
func print println`
(+ se toleran `#include`, `using`, `namespace`, `std`, `cout`, `endl` por compatibilidad
con los ejemplos viejos tipo C++.)

### Operadores
- Aritméticos: `+ - * / %`
- **Nuevos**:
  - `**`  potencia          `2 ** 8`  → 256
  - `..`  rango             `1..10`   (se usa en `for x in 1..10`)
  - `|>`  pipe              `x |> doble`  ≡ `doble(x)`
  - `%%`  divisible (bool)  `n %% 3`  → true si n%3==0
  - `><`  swap (sentencia)  `a >< b;` intercambia valores
  - `<=>` comparación 3 vías `a <=> b` → -1 / 0 / 1
- Relacionales: `== != < > <= >=`
- Lógicos: `&& || !`
- Asignación: `= += -= *= /=`
- Incr/decr: `++ --`

### Gramática (resumen)
```
programa     → (def_func | sentencia)*
def_func     → ("func"|tipo) IDENT "(" params? ")" bloque
sentencia    → decl | asignacion | swap | if | while | for | forin
             | return | break | continue | bloque | print | llamada ";"
decl         → tipo IDENT ("=" expr)? ";"
swap         → IDENT "><" IDENT ";"
forin        → "for" IDENT "in" expr ".." expr bloque
expr (prec, de menor a mayor):
  or   → and ("||" and)*
  and  → cmp ("&&" cmp)*
  cmp  → suma (rel suma)?        rel = == != < > <= >= <=>
  suma → term (("+"|"-") term)*
  term → pot  (("*"|"/"|"%"|"%%") pot)*
  pot  → unary ("**" unary)*     (asoc. derecha)
  unary→ ("-"|"!") unary | pipe
  pipe → factor ("|>" IDENT)*
  factor → INT|FLOAT|STR|CHAR|true|false | IDENT | llamada | "(" expr ")"
```

## Estructura de archivos
```
src/
  config.hpp      LANG_NAME, versión
  token.hpp       struct Token, helpers de keywords/tipos
  lexer.hpp/.cpp  análisis léxico
  ast.hpp         Nodo genérico (tipo,valor,hijos,dtype,linea) + print + toJSON
  parser.hpp/.cpp parser descendente recursivo (CST + AST)
  symbol.hpp      tabla de símbolos con scopes
  semantic.hpp/.cpp  chequeo de tipos / declaraciones / errores
  tac.hpp/.cpp    código de tres direcciones + constant folding
  interpreter.hpp/.cpp  ejecución (tree-walking)
  json.hpp        utilidades de serialización JSON
  main.cpp        CLI:  sigilc <archivo.sg> [--json|--pretty]
web/   index.html, style.css, app.js
server/ server.py  (http.server, sirve web/ y ejecuta sigilc)
examples/ *.sg
CMakeLists.txt
```

## Convenciones de diseño
- AST/CST = **nodo genérico** `{tipo, valor, hijos, dtype, linea}` (como el original,
  menos código y menos riesgo de compilar). Las fases recorren ese árbol.
- `dtype` lo rellena el semántico (tipo inferido de cada expr).
- Cada fase NO aborta: acumula errores y sigue (mejor para mostrar en la UI).
- CLI imprime JSON con: tokens, tablaSimbolos, cst, ast, erroresLexicos,
  erroresSintacticos, erroresSemanticos, tac, salida (stdout del intérprete).

## Estado / pendientes
- [x] Lexer, parser, semántico, TAC, intérprete, CLI+JSON, UI web + servidor — TODO escrito.
- [x] Servidor Python probado OK (sirve web, lista/lee ejemplos, /api/compile avisa si falta binario).
- [ ] PENDIENTE CLAVE: **compilar el C++**. Aquí NO hay compilador (VS2022 está sin la
      carga "Desarrollo escritorio con C++"; no hay cl.exe ni g++). El usuario debe
      compilar en su entorno. El C++ se revisó estáticamente pero no se compiló.
- Probar con Visual Studio: instalar workload C++ → abrir carpeta → CMake → compilar `sigilc`.
- Lanzar UI:  `python server/server.py`  → http://localhost:8000
- Si aparecen errores de compilación, pedir al usuario que los pegue y corregir.

## Mejoras hechas (2026-07-01)
- Compilador compilado y verificado en la máquina (MSVC). `bin/sigilc.exe` OK.
- Modo consola **`--resumen`** (`-r`) + `sigil.bat` para correr `programa.sg` u otro archivo.
- Web renovada: logo SVG, **resaltado de sintaxis** por token (capa <pre> tras <textarea>),
  árboles **CST y AST** tipo organigrama colapsables con zoom, tabla de símbolos/tokens,
  TAC con temporales resaltados. Highlighter verificado en Node (maneja `1..5`, `|>`, etc.).
- Build directo: `cmd //c build_manual.bat` (o `build_manual.bat` en Windows).

## Mejoras hechas (2026-07-01, 2ª tanda)
- Fix layout: `min-width:0` en panes para que CST/AST no invadan el editor.
- Editor con **marcado de errores** por línea (capa #errlayer/#errinner + tooltip en gutter).
- **Arrays**: `int[]`, literal `[..]`, `arr[i]`, `arr[i]=x`, `for x in arr`, `len(arr)`.
  (nodos: ArrayLiteral, Indexacion, AsignacionIndice, SentenciaForEach; tipos "T[]").
- **Builtins string**: upper, lower, substr, charAt, indexOf (+ len acepta string/array).
- **Optimizador TAC**: eliminación de temporales redundantes (copy prop) + código muerto.
  JSON expone `tacOpt`, `propagaciones`, `eliminadas`. UI muestra generado vs optimizado.
- `iniciar_web.bat` (launcher 1 clic). Ejemplo `examples/06_arrays.sg`.
- Todo recompilado y probado con MSVC; sin regresiones en ejemplos 01-06.

## Posibles siguientes pasos (cuando se retome)
- Más builtins (input/lectura, funciones de string).
- Optimizaciones extra del TAC (eliminación de código muerto, copy propagation).
- Resaltado de sintaxis en el editor web (CodeMirror) y marcado de errores por línea.
- Generación de la documentación final (Word/PDF, >=16 pág) según el DOCX.

## Notas para retomar
- Si se renombra el lenguaje: cambiar `LANG_NAME`/`LANG_EXT` en `src/config.hpp`
  y el título en `web/index.html`. La lógica no depende del nombre.
- No hay compilador C++ en este entorno (revisado 2026-06-21). El servidor Python sí corre.
