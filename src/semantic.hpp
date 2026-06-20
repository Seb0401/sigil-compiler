// ============================================================
//  semantic.hpp — Analizador semántico (declaración)
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <map>
#include "ast.hpp"
#include "symbol.hpp"

namespace sigil {

struct ResultadoSemantico {
    std::vector<std::string> errores;
    std::vector<std::string> advertencias;
    std::vector<Simbolo>     simbolos;   // tabla final (para mostrar)
    std::map<std::string, Funcion> funciones;
};

// Recorre el AST, rellena node->dtype y reporta errores semánticos.
class Semantic {
    std::vector<std::string> errores;
    std::vector<std::string> advertencias;
    TablaSimbolos tabla;
    std::map<std::string, Funcion> funcs;
    std::string retornoActual;   // tipo de retorno de la función en curso
    int loopDepth = 0;

    void err(int linea, const std::string& m);
    void warn(int linea, const std::string& m);

    static bool esNumerico(const std::string& t);
    bool compatibleAsignar(const std::string& destino, const std::string& origen);

    void registrarBuiltins();
    void prePasoFunciones(const Nodo& programa);

    std::string tipoDe(const Nodo& n);            // tipo de una expresión
    std::string tipoLlamada(const Nodo& n);       // tipo de una llamada
    void analizarSent(const Nodo& n);             // una sentencia
    void analizarBloque(const Nodo& n, bool nuevoScope);

public:
    ResultadoSemantico analizar(const Nodo& programa);
};

} // namespace sigil
