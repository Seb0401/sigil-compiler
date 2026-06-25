// ============================================================
//  interpreter.hpp — Intérprete (tree-walking) del lenguaje
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include "ast.hpp"

namespace sigil {

// Valor en tiempo de ejecución.
struct Valor {
    enum Tipo { INT, FLOAT, BOOL, CHAR, STR, ARR, VOID } tipo = VOID;
    long long  i = 0;
    double     f = 0;
    bool       b = false;
    std::string s;
    std::vector<Valor> arr;   // para tipo ARR (arreglos)

    static Valor I(long long v) { Valor x; x.tipo = INT; x.i = v; return x; }
    static Valor F(double v)    { Valor x; x.tipo = FLOAT; x.f = v; return x; }
    static Valor B(bool v)      { Valor x; x.tipo = BOOL; x.b = v; return x; }
    static Valor C(const std::string& v) { Valor x; x.tipo = CHAR; x.s = v; return x; }
    static Valor S(const std::string& v) { Valor x; x.tipo = STR; x.s = v; return x; }
    static Valor A(std::vector<Valor> v) { Valor x; x.tipo = ARR; x.arr = std::move(v); return x; }
    static Valor Nada()         { Valor x; x.tipo = VOID; return x; }

    bool esNum() const { return tipo == INT || tipo == FLOAT; }
    double num() const { return tipo == FLOAT ? f : (tipo == INT ? (double)i : (b ? 1 : 0)); }
    std::string toStr() const;
};

struct ResultadoEjecucion {
    std::string salida;                 // lo que imprimió el programa
    std::vector<std::string> errores;   // errores en tiempo de ejecución
};

class Interprete {
    enum class Flujo { NORMAL, RETORNO, BREAK, CONTINUE };

    std::unordered_map<std::string, Valor> globals;
    std::vector<std::unordered_map<std::string, Valor>> locales;
    std::map<std::string, Nodo> funcs;   // funciones del usuario

    std::string salida;
    std::vector<std::string> errores;
    Flujo flujo = Flujo::NORMAL;
    Valor valRetorno;
    long long pasos = 0;                  // contador anti bucle-infinito
    bool abortado = false;

    Valor* buscar(const std::string& n);
    void   declarar(const std::string& n, const Valor& v);
    void   asignar(const std::string& n, const Valor& v, int linea);

    Valor evalExpr(const Nodo& n);
    Valor evalBinario(const Nodo& n);
    Valor llamar(const std::string& nombre, std::vector<Valor>& args, int linea);

    void ejecutar(const Nodo& n);
    void ejecutarBloque(const Nodo& n, bool nuevoScope);

    void rerror(int linea, const std::string& m);
    bool paso(int linea);

public:
    ResultadoEjecucion ejecutarPrograma(const Nodo& programa);
};

} // namespace sigil
