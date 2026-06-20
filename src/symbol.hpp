// ============================================================
//  symbol.hpp — Tabla de símbolos con manejo de alcances (scopes)
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace sigil {

// Una entrada de la tabla de símbolos.
struct Simbolo {
    std::string nombre;
    std::string tipo;       // int, float, bool, char, string, void
    std::string categoria;  // "variable" | "parametro"
    int  linea = 0;
    int  nivel = 0;         // nivel de anidamiento del scope (0 = global)
    bool inicializado = false;
};

// Firma de una función.
struct Funcion {
    std::string nombre;
    std::string retorno;
    std::vector<std::string> params;  // tipos de cada parámetro
    bool builtin = false;
    bool variadic = false;            // acepta nº variable de args (builtins)
};

// Pila de scopes. Permite declarar, buscar (de adentro hacia afuera)
// y mantiene un histórico para poder mostrar la tabla completa.
class TablaSimbolos {
    std::vector<std::unordered_map<std::string, Simbolo>> scopes;
    std::vector<Simbolo> historico;
public:
    TablaSimbolos() { push(); } // scope global

    void push() { scopes.emplace_back(); }
    void pop()  { if (!scopes.empty()) scopes.pop_back(); }
    int  nivel() const { return (int)scopes.size() - 1; }

    // Declara en el scope actual. Devuelve false si ya existía ahí.
    bool declarar(const Simbolo& s) {
        auto& cur = scopes.back();
        if (cur.count(s.nombre)) return false;
        Simbolo copia = s; copia.nivel = nivel();
        cur[s.nombre] = copia;
        historico.push_back(copia);
        return true;
    }

    bool existeEnActual(const std::string& n) const {
        return scopes.back().count(n) > 0;
    }

    // Busca de adentro hacia afuera.
    Simbolo* buscar(const std::string& n) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto f = it->find(n);
            if (f != it->end()) return &f->second;
        }
        return nullptr;
    }

    const std::vector<Simbolo>& todos() const { return historico; }
};

} // namespace sigil
