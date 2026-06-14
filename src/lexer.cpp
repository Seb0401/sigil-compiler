// ============================================================
//  lexer.cpp — Analizador léxico (implementación)
//
//  Lee el código fuente carácter a carácter y lo agrupa en
//  tokens. Reconoce los operadores nuevos de Sigil:
//    <=>  **  ..  |>  %%  ><  &&  ||
//  además de los clásicos.
// ============================================================
#include "lexer.hpp"
#include <cctype>
#include <algorithm>

namespace sigil {

ResultadoLexico analizarLexico(const std::string& fuente) {
    ResultadoLexico R;
    const int N = static_cast<int>(fuente.size());
    int i = 0, linea = 1, colBase = 0; // colBase = índice donde empieza la línea

    auto col = [&](int idx) { return idx - colBase + 1; };
    auto push = [&](const std::string& lex, const std::string& tipo, int idx) {
        R.tokens.push_back({lex, tipo, linea, col(idx)});
    };
    auto error = [&](int idx, const std::string& msg) {
        R.errores.push_back("Error lexico [linea " + std::to_string(linea) +
                            ", col " + std::to_string(col(idx)) + "]: " + msg);
        R.ok = false;
    };

    while (i < N) {
        char c = fuente[i];

        // Salto de línea
        if (c == '\n') { linea++; colBase = i + 1; i++; continue; }
        // Otros espacios en blanco
        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

        // Comentario de línea  //
        if (c == '/' && i + 1 < N && fuente[i + 1] == '/') {
            while (i < N && fuente[i] != '\n') i++;
            continue;
        }
        // Comentario de bloque  /* ... */  (multilínea)
        if (c == '/' && i + 1 < N && fuente[i + 1] == '*') {
            int ini = i; i += 2; bool cerrado = false;
            while (i < N) {
                if (fuente[i] == '\n') { linea++; colBase = i + 1; i++; continue; }
                if (fuente[i] == '*' && i + 1 < N && fuente[i + 1] == '/') {
                    i += 2; cerrado = true; break;
                }
                i++;
            }
            if (!cerrado) error(ini, "Comentario de bloque no cerrado.");
            continue;
        }

        // Cadena "..."
        if (c == '"') {
            int ini = i; std::string t = "\""; i++;
            while (i < N && fuente[i] != '"' && fuente[i] != '\n') {
                if (fuente[i] == '\\' && i + 1 < N) { t += fuente[i]; t += fuente[i + 1]; i += 2; }
                else t += fuente[i++];
            }
            if (i >= N || fuente[i] != '"') { error(ini, "Cadena no cerrada."); continue; }
            t += '"'; i++;
            push(t, "LITERAL_STRING", ini);
            continue;
        }

        // Carácter '...'
        if (c == '\'') {
            int ini = i; std::string t = "'"; i++;
            while (i < N && fuente[i] != '\'' && fuente[i] != '\n') {
                if (fuente[i] == '\\' && i + 1 < N) { t += fuente[i]; t += fuente[i + 1]; i += 2; }
                else t += fuente[i++];
            }
            if (i >= N || fuente[i] != '\'') { error(ini, "Caracter no cerrado."); continue; }
            t += '\''; i++;
            push(t, "LITERAL_CHAR", ini);
            continue;
        }

        // Identificadores, palabras reservadas y directivas (#include)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '#') {
            int ini = i; std::string p;
            while (i < N && (std::isalnum(static_cast<unsigned char>(fuente[i])) ||
                             fuente[i] == '_' || fuente[i] == '#'))
                p += fuente[i++];
            if (esKeyword(p)) push(p, "KEYWORD", ini);
            else {
                push(p, "IDENTIFIER", ini);
                if (std::find(R.tabla.begin(), R.tabla.end(), p) == R.tabla.end())
                    R.tabla.push_back(p);
            }
            continue;
        }

        // Números (enteros y flotantes). Ojo con '..' (rango): no se consume.
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int ini = i; std::string num; bool esF = false;
            while (i < N) {
                char d = fuente[i];
                if (std::isdigit(static_cast<unsigned char>(d))) { num += d; i++; }
                else if (d == '.') {
                    // ¿es el operador de rango '..'? entonces detener el número
                    if (i + 1 < N && fuente[i + 1] == '.') break;
                    if (esF) { error(i, "Flotante malformado (dos puntos)."); break; }
                    esF = true; num += d; i++;
                } else break;
            }
            push(num, esF ? "LITERAL_FLOAT" : "LITERAL_INT", ini);
            continue;
        }

        // ── Operadores de 3 caracteres ───────────────────────
        if (c == '<' && i + 2 < N && fuente[i + 1] == '=' && fuente[i + 2] == '>') {
            push("<=>", "OPERATOR", i); i += 3; continue;
        }

        // ── Operadores de 2 caracteres ───────────────────────
        {
            static const char* op2[] = {
                "**", "..", "|>", "%%", "><", "&&", "||",
                "<<", ">>", "==", "!=", "<=", ">=",
                "++", "--", "+=", "-=", "*=", "/=", nullptr
            };
            bool hecho = false;
            if (i + 1 < N) {
                for (int k = 0; op2[k]; ++k) {
                    if (c == op2[k][0] && fuente[i + 1] == op2[k][1]) {
                        push(op2[k], "OPERATOR", i); i += 2; hecho = true; break;
                    }
                }
            }
            if (hecho) continue;
        }

        // ── Operadores de 1 carácter ─────────────────────────
        if (std::string("=+-*/<>!&|%").find(c) != std::string::npos) {
            push(std::string(1, c), "OPERATOR", i); i++; continue;
        }

        // ── Delimitadores ────────────────────────────────────
        if (std::string(";(){}[],:.").find(c) != std::string::npos) {
            push(std::string(1, c), "DELIMITER", i); i++; continue;
        }

        // Carácter no reconocido
        error(i, std::string("Caracter invalido '") + c + "'.");
        i++;
    }

    return R;
}

} // namespace sigil
