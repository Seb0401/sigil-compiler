// ============================================================
//  ast.hpp — Nodo genérico de árbol (compartido por CST y AST)
//
//  Se usa un único tipo de nodo {tipo, valor, hijos, dtype, linea}
//  en lugar de una jerarquía de clases. Esto reduce muchísimo el
//  código y todas las fases (semántico, TAC, intérprete) recorren
//  el mismo árbol.
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "json.hpp"

namespace sigil {

struct NodoArbol;
using Nodo = std::shared_ptr<NodoArbol>;

struct NodoArbol {
    std::string tipo;     // categoría sintáctica/semántica del nodo
    std::string valor;    // lexema o valor asociado (puede estar vacío)
    std::string dtype;    // tipo inferido por el semántico ("", "int", "float"...)
    int linea = 0;        // línea de origen (para errores y depuración)
    std::vector<Nodo> hijos;

    NodoArbol(std::string t, std::string v = "")
        : tipo(std::move(t)), valor(std::move(v)) {}

    void agregar(const Nodo& h) { if (h) hijos.push_back(h); }
};

inline Nodo mkN(const std::string& t, const std::string& v = "") {
    return std::make_shared<NodoArbol>(t, v);
}

// Impresión con sangría visual (consola).
inline void imprimirArbol(const Nodo& n, const std::string& ind = "", bool ult = true) {
    if (!n) return;
    std::cout << ind << (ult ? "`-- " : "|-- ") << "[" << n->tipo;
    if (!n->valor.empty()) std::cout << ": " << n->valor;
    if (!n->dtype.empty()) std::cout << " <" << n->dtype << ">";
    std::cout << "]\n";
    std::string sig = ind + (ult ? "    " : "|   ");
    for (size_t i = 0; i < n->hijos.size(); ++i)
        imprimirArbol(n->hijos[i], sig, i == n->hijos.size() - 1);
}

// Serializa un árbol a JSON recursivamente (para la UI web).
inline std::string arbolJSON(const Nodo& n) {
    if (!n) return "null";
    std::string s = "{";
    s += jstr("tipo", n->tipo) + ",";
    s += jstr("valor", n->valor) + ",";
    s += jstr("dtype", n->dtype) + ",";
    s += "\"linea\":" + std::to_string(n->linea) + ",";
    s += "\"hijos\":[";
    for (size_t i = 0; i < n->hijos.size(); ++i) {
        if (i) s += ",";
        s += arbolJSON(n->hijos[i]);
    }
    s += "]}";
    return s;
}

} // namespace sigil
