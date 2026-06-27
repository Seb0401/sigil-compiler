// ============================================================
//  main.cpp — Driver del compilador Sigil
//
//  Uso:
//     sigilc <archivo.sg> [--json | --pretty]
//     sigilc -            (lee el codigo fuente desde stdin)
//
//  --pretty (por defecto): salida legible en consola.
//  --json:                 salida JSON (la consume la UI web).
// ============================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "json.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include "tac.hpp"
#include "interpreter.hpp"

using namespace sigil;

static std::string leerTodo(std::istream& in) {
    std::stringstream ss; ss << in.rdbuf(); return ss.str();
}

static std::string arr(const std::vector<std::string>& v, bool comoStrings = true) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ",";
        s += comoStrings ? ("\"" + jsonEscape(v[i]) + "\"") : v[i];
    }
    return s + "]";
}

// ── Salida JSON completa (para la web) ───────────────────────
static void emitirJSON(const ResultadoLexico& lex,
                       const ResultadoSintactico& sin,
                       const ResultadoSemantico& sem,
                       const ResultadoTAC& tac,
                       const ResultadoEjecucion& eje) {
    std::string o = "{";
    o += jstr("lang", LANG_NAME) + ",";
    o += jstr("version", LANG_VER) + ",";

    // tokens
    o += "\"tokens\":[";
    for (size_t i = 0; i < lex.tokens.size(); ++i) {
        if (i) o += ",";
        const Token& t = lex.tokens[i];
        o += "{" + jstr("lexema", t.lexema) + "," + jstr("tipo", t.tipo) +
             ",\"linea\":" + std::to_string(t.linea) +
             ",\"col\":" + std::to_string(t.col) + "}";
    }
    o += "],";

    o += "\"tablaLexica\":" + arr(lex.tabla) + ",";
    o += "\"erroresLexicos\":" + arr(lex.errores) + ",";

    o += "\"cst\":" + (sin.cst ? arbolJSON(sin.cst) : std::string("null")) + ",";
    o += "\"ast\":" + (sin.ast ? arbolJSON(sin.ast) : std::string("null")) + ",";
    o += "\"erroresSintacticos\":" + arr(sin.errores) + ",";

    // tabla de simbolos (semantica)
    o += "\"tablaSimbolos\":[";
    for (size_t i = 0; i < sem.simbolos.size(); ++i) {
        if (i) o += ",";
        const Simbolo& s = sem.simbolos[i];
        o += "{" + jstr("nombre", s.nombre) + "," + jstr("tipo", s.tipo) + "," +
             jstr("categoria", s.categoria) + ",\"nivel\":" + std::to_string(s.nivel) +
             ",\"linea\":" + std::to_string(s.linea) + "}";
    }
    o += "],";
    o += "\"erroresSemanticos\":" + arr(sem.errores) + ",";
    o += "\"advertencias\":" + arr(sem.advertencias) + ",";

    o += "\"tac\":" + arr(tac.codigo) + ",";
    o += "\"tacOpt\":" + arr(tac.optimizado) + ",";
    o += "\"optimizaciones\":" + arr(tac.optimizaciones) + ",";
    o += "\"pliegues\":" + std::to_string(tac.pliegues) + ",";
    o += "\"propagaciones\":" + std::to_string(tac.propagaciones) + ",";
    o += "\"eliminadas\":" + std::to_string(tac.eliminadas) + ",";

    o += "\"erroresEjecucion\":" + arr(eje.errores) + ",";
    o += jstr("salida", eje.salida) + ",";

    bool ok = lex.errores.empty() && sin.errores.empty() && sem.errores.empty();
    o += "\"ok\":" + std::string(ok ? "true" : "false");
    o += "}";
    std::cout << o << std::endl;
}

// ── Salida legible en consola ────────────────────────────────
static void emitirTexto(const ResultadoLexico& lex,
                        const ResultadoSintactico& sin,
                        const ResultadoSemantico& sem,
                        const ResultadoTAC& tac,
                        const ResultadoEjecucion& eje) {
    std::cout << "=================================================\n";
    std::cout << "   COMPILADOR " << LANG_NAME << " v" << LANG_VER << "\n";
    std::cout << "=================================================\n";

    std::cout << "\n[FASE 1] ANALISIS LEXICO\n" << std::string(49, '-') << "\n";
    for (auto& t : lex.tokens)
        printf("  %-20s -> [%s] (lin %d, col %d)\n", t.lexema.c_str(), t.tipo.c_str(), t.linea, t.col);
    std::cout << "\n  Tabla de simbolos (lexica):\n";
    for (auto& s : lex.tabla) std::cout << "    " << s << "\n";
    for (auto& e : lex.errores) std::cout << "  " << e << "\n";

    std::cout << "\n[FASE 2] ANALISIS SINTACTICO\n" << std::string(49, '-') << "\n";
    if (sin.errores.empty()) std::cout << "  Sin errores sintacticos.\n";
    for (auto& e : sin.errores) std::cout << "  " << e << "\n";
    std::cout << "\n  AST:\n";
    if (sin.ast) for (size_t i = 0; i < sin.ast->hijos.size(); ++i)
        imprimirArbol(sin.ast->hijos[i], "", i == sin.ast->hijos.size() - 1);

    std::cout << "\n[FASE 3] ANALISIS SEMANTICO\n" << std::string(49, '-') << "\n";
    std::cout << "  Tabla de simbolos (tipos / alcance):\n";
    for (auto& s : sem.simbolos)
        printf("    %-14s : %-7s [%s, nivel %d, lin %d]\n",
               s.nombre.c_str(), s.tipo.c_str(), s.categoria.c_str(), s.nivel, s.linea);
    if (sem.errores.empty()) std::cout << "  Sin errores semanticos.\n";
    for (auto& e : sem.errores) std::cout << "  " << e << "\n";
    for (auto& w : sem.advertencias) std::cout << "  " << w << "\n";

    std::cout << "\n[FASE 4] CODIGO INTERMEDIO (TAC)\n" << std::string(49, '-') << "\n";
    for (auto& l : tac.codigo) std::cout << "    " << l << "\n";
    std::cout << "  Optimizacion (constant folding): " << tac.pliegues << " plegado(s).\n";
    for (auto& o : tac.optimizaciones) std::cout << "    " << o << "\n";

    std::cout << "\n[FASE 5] EJECUCION (INTERPRETE)\n" << std::string(49, '-') << "\n";
    std::cout << "--- salida del programa ---\n" << eje.salida;
    if (!eje.salida.empty() && eje.salida.back() != '\n') std::cout << "\n";
    std::cout << "---------------------------\n";
    for (auto& e : eje.errores) std::cout << "  " << e << "\n";

    std::cout << "\n=================================================\n";
}

// ── Salida RESUMIDA en consola (versión compacta de la web) ──
static void emitirResumen(const std::string& archivo,
                          const ResultadoLexico& lex,
                          const ResultadoSintactico& sin,
                          const ResultadoSemantico& sem,
                          const ResultadoTAC& tac,
                          const ResultadoEjecucion& eje) {
    int nLex = (int)lex.errores.size(), nSin = (int)sin.errores.size(),
        nSem = (int)sem.errores.size(), nEje = (int)eje.errores.size();
    bool ok = nLex == 0 && nSin == 0 && nSem == 0;

    std::cout << "+-----------------------------------------------+\n";
    std::cout << "|  " << LANG_NAME << " v" << LANG_VER << " - resumen de compilacion\n";
    std::cout << "+-----------------------------------------------+\n";
    if (!archivo.empty() && archivo != "-")
        std::cout << "  Archivo : " << archivo << "\n";
    std::cout << "  Tokens  : " << lex.tokens.size()
              << "   Simbolos: " << sem.simbolos.size()
              << "   TAC: " << tac.codigo.size() << " -> " << tac.optimizado.size()
              << " instr (opt)\n";
    std::cout << "  Optimiz.: " << tac.pliegues << " plegado(s), "
              << tac.propagaciones << " temp. propagados, "
              << tac.eliminadas << " linea(s) muerta(s)\n";
    std::cout << "  Errores : lexicos " << nLex << " | sintacticos " << nSin
              << " | semanticos " << nSem << " | ejecucion " << nEje << "\n";

    std::cout << "\n  Tabla de simbolos:\n";
    if (sem.simbolos.empty()) std::cout << "    (ninguno)\n";
    for (auto& s : sem.simbolos)
        printf("    %-14s : %-7s (%s, nivel %d)\n",
               s.nombre.c_str(), s.tipo.c_str(), s.categoria.c_str(), s.nivel);

    if (nLex + nSin + nSem + nEje > 0) {
        std::cout << "\n  Errores detectados:\n";
        for (auto& e : lex.errores) std::cout << "    - " << e << "\n";
        for (auto& e : sin.errores) std::cout << "    - " << e << "\n";
        for (auto& e : sem.errores) std::cout << "    - " << e << "\n";
        for (auto& w : sem.advertencias) std::cout << "    ! " << w << "\n";
        for (auto& e : eje.errores) std::cout << "    - " << e << "\n";
    }

    std::cout << "\n  --- salida del programa ---\n";
    if (eje.salida.empty()) std::cout << "  (sin salida)\n";
    else {
        // indenta cada linea de la salida
        std::string s = eje.salida; size_t ini = 0;
        while (ini < s.size()) {
            size_t fin = s.find('\n', ini);
            if (fin == std::string::npos) fin = s.size();
            std::cout << "  " << s.substr(ini, fin - ini) << "\n";
            ini = fin + 1;
        }
    }
    std::cout << "\n  Estado  : " << (ok ? "EXITOSO" : "CON ERRORES") << "\n";
    std::cout << "+-----------------------------------------------+\n";
}

int main(int argc, char** argv) {
    std::string archivo, modo = "--pretty";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json" || a == "--pretty" || a == "--resumen") modo = a;
        else if (a == "-r") modo = "--resumen";
        else if (a == "-j") modo = "--json";
        else archivo = a;
    }

    std::string fuente;
    if (archivo.empty() || archivo == "-") {
        fuente = leerTodo(std::cin);
    } else {
        std::ifstream f(archivo);
        if (!f.is_open()) {
            if (modo == "--json")
                std::cout << "{\"error\":\"No se pudo abrir " << jsonEscape(archivo) << "\"}\n";
            else
                std::cerr << "Error: no se pudo abrir '" << archivo << "'.\n";
            return 1;
        }
        fuente = leerTodo(f);
    }

    // Pipeline completo (cada fase acumula errores pero no aborta).
    ResultadoLexico    lex = analizarLexico(fuente);
    Parser parser(lex.tokens);
    ResultadoSintactico sin = parser.parsear();
    Semantic semantico;
    ResultadoSemantico  sem = semantico.analizar(sin.ast);
    TAC generador;
    ResultadoTAC        tac = generador.generar(sin.ast);

    // El intérprete solo corre si no hay errores que impidan ejecutar.
    ResultadoEjecucion eje;
    bool ejecutable = lex.errores.empty() && sin.errores.empty() && sem.errores.empty();
    if (ejecutable) {
        Interprete interp;
        eje = interp.ejecutarPrograma(sin.ast);
    } else {
        eje.errores.push_back("No se ejecuta: corrija los errores de fases anteriores.");
    }

    if (modo == "--json")         emitirJSON(lex, sin, sem, tac, eje);
    else if (modo == "--resumen") emitirResumen(archivo, lex, sin, sem, tac, eje);
    else                          emitirTexto(lex, sin, sem, tac, eje);
    return 0;
}
