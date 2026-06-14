// ============================================================
//  lexer.hpp — Analizador léxico (declaración)
// ============================================================
#pragma once
#include <string>
#include <vector>
#include "token.hpp"

namespace sigil {

// Resultado del análisis léxico.
struct ResultadoLexico {
    std::vector<Token>       tokens;   // flujo de tokens reconocidos
    std::vector<std::string> tabla;    // tabla de símbolos (identificadores únicos)
    std::vector<std::string> errores;  // errores léxicos
    bool ok = true;
};

// Analiza el código fuente (texto completo) y produce tokens.
ResultadoLexico analizarLexico(const std::string& fuente);

} // namespace sigil
