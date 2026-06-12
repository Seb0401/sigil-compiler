// ============================================================
//  token.hpp — Definición de Token y utilidades léxicas
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <algorithm>

namespace sigil {

// Un Token es la unidad mínima con significado del lenguaje.
struct Token {
    std::string lexema;   // texto exacto del código fuente
    std::string tipo;     // KEYWORD, IDENTIFIER, LITERAL_INT, ...
    int linea = 0;        // línea (1-based) para reportar errores
    int col   = 0;        // columna (1-based)
};

// Palabras reservadas del lenguaje Sigil.
inline const std::vector<std::string>& keywords() {
    static const std::vector<std::string> k = {
        // tipos
        "int", "float", "bool", "char", "string", "void",
        // control de flujo
        "return", "if", "else", "while", "for", "in",
        "break", "continue",
        // literales / declaración
        "true", "false", "func",
        // E/S incorporada
        "print", "println",
        // compatibilidad con directivas tipo C++ del proyecto original
        // (cout/endl se dejan como IDENTIFIER para soportar 'cout << x << endl;')
        "#include", "using", "namespace", "std"
    };
    return k;
}

inline bool esKeyword(const std::string& p) {
    const auto& k = keywords();
    return std::find(k.begin(), k.end(), p) != k.end();
}

// ¿El lexema denota un tipo de dato? (usado por parser y semántico)
inline bool esTipoDato(const std::string& lex) {
    return lex == "int" || lex == "float" || lex == "void" ||
           lex == "bool" || lex == "char" || lex == "string";
}

} // namespace sigil
