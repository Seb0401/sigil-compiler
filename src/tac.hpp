// ============================================================
//  tac.hpp — Código intermedio de tres direcciones (declaración)
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <utility>
#include "ast.hpp"

namespace sigil {

struct ResultadoTAC {
    std::vector<std::string> codigo;        // TAC generado (con constant folding)
    std::vector<std::string> optimizado;    // TAC tras optimizar (copy prop + DCE)
    int pliegues = 0;                       // nº de constantes plegadas
    int propagaciones = 0;                  // temporales redundantes eliminados
    int eliminadas = 0;                     // líneas de código muerto eliminadas
    std::vector<std::string> optimizaciones;// detalle de cada plegado
};

// Genera código de tres direcciones a partir del AST.
// Aplica una optimización de "plegado de constantes" (constant folding).
class TAC {
    std::vector<std::string> cod;
    int nt = 0, nl = 0, pliegues = 0;
    std::vector<std::string> opt;
    std::vector<std::pair<std::string, std::string>> bucles; // (inicio, fin)

    std::string nuevoTemp()  { return "t" + std::to_string(++nt); }
    std::string nuevaEtiq()  { return "L" + std::to_string(++nl); }
    void emit(const std::string& s) { cod.push_back(s); }

    std::string opBase(const std::string& comp); // "+=" -> "+"

    std::string genExpr(const Nodo& n);
    void genStmt(const Nodo& n);

public:
    // Pública para que el plegado de constantes (función auxiliar) la use.
    static bool esNumero(const std::string& s, bool& esFloat);
    ResultadoTAC generar(const Nodo& programa);
};

} // namespace sigil
