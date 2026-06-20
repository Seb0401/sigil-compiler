// ============================================================
//  semantic.cpp — Analizador semántico (implementación)
//
//  Responsabilidades:
//   - Construir la tabla de símbolos con alcances (scopes).
//   - Verificar tipos en expresiones, asignaciones y llamadas.
//   - Validar declaraciones (no usar/redeclarar mal).
//   - Resolver alcance de variables.
//   - Detectar errores semánticos (acumulándolos, sin abortar).
// ============================================================
#include "semantic.hpp"
#include <algorithm>

namespace sigil {

static Nodo hijoPorTipo(const Nodo& n, const std::string& tipo) {
    for (auto& h : n->hijos) if (h && h->tipo == tipo) return h;
    return nullptr;
}

void Semantic::err(int linea, const std::string& m) {
    errores.push_back("Error semantico [linea " + std::to_string(linea) + "]: " + m);
}
void Semantic::warn(int linea, const std::string& m) {
    advertencias.push_back("Advertencia [linea " + std::to_string(linea) + "]: " + m);
}

bool Semantic::esNumerico(const std::string& t) { return t == "int" || t == "float"; }

// Utilidades para tipos arreglo ("int[]", "string[]", ...).
static bool esArreglo(const std::string& t) {
    return t.size() >= 2 && t.compare(t.size() - 2, 2, "[]") == 0;
}
static std::string tipoElem(const std::string& t) {
    return esArreglo(t) ? t.substr(0, t.size() - 2) : t;
}

bool Semantic::compatibleAsignar(const std::string& destino, const std::string& origen) {
    if (destino == "error" || origen == "error" || origen.empty()) return true; // evita cascada
    if (destino == "any" || origen == "any") return true;
    if (destino == origen) return true;
    if (destino == "float" && origen == "int") return true;  // ensanchamiento
    if (esArreglo(destino) && esArreglo(origen)) {            // arreglos
        std::string ed = tipoElem(destino), eo = tipoElem(origen);
        return ed == eo || eo == "any" || ed == "any" ||
               (ed == "float" && eo == "int");
    }
    return false;
}

void Semantic::registrarBuiltins() {
    auto add = [&](const std::string& n, const std::string& ret,
                   std::vector<std::string> ps, bool var = false) {
        Funcion f; f.nombre = n; f.retorno = ret; f.params = std::move(ps);
        f.builtin = true; f.variadic = var; funcs[n] = f;
    };
    add("abs",      "float", {"float"});
    add("sqrt",     "float", {"float"});
    add("pow",      "float", {"float", "float"});
    add("max",      "float", {"float", "float"});
    add("min",      "float", {"float", "float"});
    add("len",      "int",   {"any"});               // string o arreglo
    add("toInt",    "int",   {"any"});
    add("toFloat",  "float", {"any"});
    add("toString", "string",{"any"});
    // funciones de cadena
    add("upper",    "string",{"string"});
    add("lower",    "string",{"string"});
    add("substr",   "string",{"string", "int", "int"});
    add("charAt",   "char",  {"string", "int"});
    add("indexOf",  "int",   {"string", "string"});
}

void Semantic::prePasoFunciones(const Nodo& programa) {
    for (auto& h : programa->hijos) {
        if (!h || h->tipo != "DefinicionFuncion") continue;
        Funcion f;
        f.nombre = h->valor;
        auto tr = hijoPorTipo(h, "TipoRetorno");
        f.retorno = tr ? tr->valor : "void";
        auto ps = hijoPorTipo(h, "Parametros");
        if (ps) for (auto& p : ps->hijos) if (p->tipo == "Parametro") f.params.push_back(p->valor);
        if (funcs.count(f.nombre))
            err(h->linea, "Funcion '" + f.nombre + "' ya declarada.");
        else
            funcs[f.nombre] = f;
    }
}

// ── Tipo de una expresión ────────────────────────────────────
std::string Semantic::tipoDe(const Nodo& n) {
    if (!n) return "error";
    const std::string& t = n->tipo;

    if (t == "NumeroEntero")   { n->dtype = "int";    return "int"; }
    if (t == "NumeroFlotante") { n->dtype = "float";  return "float"; }
    if (t == "LiteralString")  { n->dtype = "string"; return "string"; }
    if (t == "LiteralChar")    { n->dtype = "char";   return "char"; }
    if (t == "LiteralBool")    { n->dtype = "bool";   return "bool"; }
    if (t == "Error")          { return "error"; }

    if (t == "Grupo") { n->dtype = tipoDe(n->hijos.empty() ? nullptr : n->hijos[0]); return n->dtype; }

    if (t == "Identificador") {
        Simbolo* s = tabla.buscar(n->valor);
        if (!s) { err(n->linea, "Variable '" + n->valor + "' no declarada."); n->dtype = "error"; return "error"; }
        n->dtype = s->tipo;
        return s->tipo;
    }

    if (t == "LlamadaFuncion") { n->dtype = tipoLlamada(n); return n->dtype; }

    if (t == "ArrayLiteral") {
        if (n->hijos.empty()) { n->dtype = "any[]"; return "any[]"; }
        std::string elem = tipoDe(n->hijos[0]);
        for (size_t i = 1; i < n->hijos.size(); ++i) {
            std::string ti = tipoDe(n->hijos[i]);
            if (elem == "int" && ti == "float") elem = "float";      // promociona
            else if (!compatibleAsignar(elem, ti) && ti != "error")
                err(n->linea, "Elementos de tipo distinto en el arreglo ('" +
                    elem + "' y '" + ti + "').");
        }
        n->dtype = elem + "[]"; return n->dtype;
    }

    if (t == "Indexacion") {
        std::string at = tipoDe(n->hijos[0]);
        std::string it = tipoDe(n->hijos[1]);
        if (it != "int" && it != "error")
            err(n->linea, "El indice de un arreglo debe ser int, no '" + it + "'.");
        if (!esArreglo(at) && at != "error") {
            err(n->linea, "No se puede indexar un valor '" + at + "' (no es un arreglo).");
            n->dtype = "error"; return "error";
        }
        n->dtype = (at == "error") ? "error" : tipoElem(at);
        return n->dtype;
    }

    if (t == "NegacionUnaria") {
        std::string x = tipoDe(n->hijos[0]);
        if (x != "error" && !esNumerico(x))
            err(n->linea, "El operador unario '-' requiere un numero, no '" + x + "'.");
        n->dtype = esNumerico(x) ? x : "error"; return n->dtype;
    }
    if (t == "NegacionLogica") {
        std::string x = tipoDe(n->hijos[0]);
        if (x != "error" && x != "bool")
            err(n->linea, "El operador '!' requiere un bool, no '" + x + "'.");
        n->dtype = "bool"; return "bool";
    }

    // Operadores binarios
    if (n->hijos.size() == 2) {
        std::string L = tipoDe(n->hijos[0]);
        std::string R = tipoDe(n->hijos[1]);
        std::string op = n->valor;
        auto numRes = [&]() { return (L == "float" || R == "float") ? "float" : "int"; };
        auto ambosNum = esNumerico(L) && esNumerico(R);
        bool hayErr = (L == "error" || R == "error");

        if (t == "Or" || t == "And") {
            if (!hayErr && (L != "bool" || R != "bool"))
                err(n->linea, "El operador '" + op + "' requiere operandos bool.");
            n->dtype = "bool"; return "bool";
        }
        if (t == "Igual" || t == "Distinto") {
            if (!hayErr && !(ambosNum || L == R))
                err(n->linea, "No se pueden comparar '" + L + "' y '" + R + "' con '" + op + "'.");
            n->dtype = "bool"; return "bool";
        }
        if (t == "Menor" || t == "Mayor" || t == "MenorIgual" || t == "MayorIgual") {
            if (!hayErr && !(ambosNum || (L == R && (L == "char" || L == "string"))))
                err(n->linea, "No se pueden ordenar '" + L + "' y '" + R + "' con '" + op + "'.");
            n->dtype = "bool"; return "bool";
        }
        if (t == "Comparacion3") {
            if (!hayErr && !(ambosNum || L == R))
                err(n->linea, "'<=>' requiere operandos comparables.");
            n->dtype = "int"; return "int";
        }
        if (t == "Suma") {
            if (ambosNum) { n->dtype = numRes(); return n->dtype; }
            if (L == "string" && R == "string") { n->dtype = "string"; return "string"; }
            if (!hayErr) err(n->linea, "No se puede sumar '" + L + "' y '" + R + "'.");
            n->dtype = "error"; return "error";
        }
        if (t == "Resta" || t == "Multiplicacion" || t == "Division" || t == "Potencia") {
            if (ambosNum) { n->dtype = numRes(); return n->dtype; }
            if (!hayErr) err(n->linea, "El operador '" + op + "' requiere numeros.");
            n->dtype = "error"; return "error";
        }
        if (t == "Modulo") {
            if (L == "int" && R == "int") { n->dtype = "int"; return "int"; }
            if (!hayErr) err(n->linea, "'%' requiere enteros.");
            n->dtype = "error"; return "error";
        }
        if (t == "Divisible") {
            if (L == "int" && R == "int") { n->dtype = "bool"; return "bool"; }
            if (!hayErr) err(n->linea, "'%%' requiere enteros.");
            n->dtype = "bool"; return "bool";
        }
    }

    n->dtype = "error";
    return "error";
}

std::string Semantic::tipoLlamada(const Nodo& n) {
    const std::string& nombre = n->valor;
    auto it = funcs.find(nombre);
    // argumentos
    std::vector<std::string> args;
    for (auto& a : n->hijos) args.push_back(tipoDe(a));

    if (it == funcs.end()) {
        err(n->linea, "Funcion '" + nombre + "' no declarada.");
        return "error";
    }
    const Funcion& f = it->second;
    if (!f.variadic && args.size() != f.params.size()) {
        err(n->linea, "La funcion '" + nombre + "' espera " +
            std::to_string(f.params.size()) + " argumento(s) pero recibio " +
            std::to_string(args.size()) + ".");
    } else {
        size_t lim = std::min(args.size(), f.params.size());
        for (size_t i = 0; i < lim; ++i)
            if (!compatibleAsignar(f.params[i], args[i]))
                err(n->linea, "Argumento " + std::to_string(i + 1) + " de '" + nombre +
                    "': se esperaba '" + f.params[i] + "' pero se recibio '" + args[i] + "'.");
    }
    return f.retorno;
}

// ── Bloques / sentencias ─────────────────────────────────────
void Semantic::analizarBloque(const Nodo& n, bool nuevoScope) {
    if (nuevoScope) tabla.push();
    for (auto& h : n->hijos) analizarSent(h);
    if (nuevoScope) tabla.pop();
}

void Semantic::analizarSent(const Nodo& n) {
    if (!n) return;
    const std::string& t = n->tipo;

    if (t == "Declaracion") {
        std::string tipo = n->valor;
        auto id = hijoPorTipo(n, "Identificador");
        std::string nombre = id ? id->valor : "?";
        if (tipoElem(tipo) == "void") err(n->linea, "No se puede declarar la variable '" + nombre + "' de tipo void.");
        Simbolo s; s.nombre = nombre; s.tipo = tipo; s.categoria = "variable"; s.linea = n->linea;
        auto ini = hijoPorTipo(n, "Inicializacion");
        if (ini && !ini->hijos.empty()) {
            std::string vt = tipoDe(ini->hijos[0]);
            if (!compatibleAsignar(tipo, vt))
                err(n->linea, "No se puede inicializar '" + nombre + "' (" + tipo +
                    ") con un valor '" + vt + "'.");
            s.inicializado = true;
        }
        if (!tabla.declarar(s))
            err(n->linea, "La variable '" + nombre + "' ya fue declarada en este alcance.");
        return;
    }

    if (t == "Asignacion" || t == "AsignacionCompuesta") {
        auto id = n->hijos.size() > 0 ? n->hijos[0] : nullptr;
        std::string nombre = id ? id->valor : "?";
        Simbolo* s = tabla.buscar(nombre);
        std::string vt = n->hijos.size() > 1 ? tipoDe(n->hijos[1]) : "error";
        if (!s) { err(n->linea, "Variable '" + nombre + "' no declarada."); return; }
        if (t == "AsignacionCompuesta") {
            bool okNum = esNumerico(s->tipo) && esNumerico(vt);
            bool okStr = (s->tipo == "string" && vt == "string" && n->valor == "+=");
            if (!okNum && !okStr && vt != "error")
                err(n->linea, "Operacion '" + n->valor + "' no valida entre '" +
                    s->tipo + "' y '" + vt + "'.");
        } else if (!compatibleAsignar(s->tipo, vt)) {
            err(n->linea, "No se puede asignar un '" + vt + "' a '" + nombre + "' (" + s->tipo + ").");
        }
        s->inicializado = true;
        return;
    }

    if (t == "AsignacionIndice") {
        auto id = n->hijos[0];
        Simbolo* s = tabla.buscar(id->valor);
        std::string it = tipoDe(n->hijos[1]);
        std::string vt = tipoDe(n->hijos[2]);
        if (!s) { err(n->linea, "Variable '" + id->valor + "' no declarada."); return; }
        if (!esArreglo(s->tipo)) { err(n->linea, "'" + id->valor + "' no es un arreglo."); return; }
        if (it != "int" && it != "error") err(n->linea, "El indice debe ser int, no '" + it + "'.");
        if (!compatibleAsignar(tipoElem(s->tipo), vt))
            err(n->linea, "No se puede asignar '" + vt + "' a un elemento de '" +
                s->tipo + "'.");
        return;
    }

    if (t == "SentenciaForEach") {
        tabla.push();
        auto id = n->hijos[0];
        std::string ct = tipoDe(n->hijos[1]);
        if (!esArreglo(ct) && ct != "error")
            err(n->linea, "'for " + id->valor + " in ...' requiere un arreglo, no '" + ct + "'.");
        Simbolo s; s.nombre = id->valor; s.tipo = (ct == "error") ? "error" : tipoElem(ct);
        s.categoria = "variable"; s.linea = n->linea; s.inicializado = true;
        tabla.declarar(s);
        loopDepth++;
        auto bloque = hijoPorTipo(n, "Bloque");
        if (bloque) analizarBloque(bloque, false);
        loopDepth--;
        tabla.pop();
        return;
    }

    if (t == "Swap") {
        auto a = n->hijos[0], b = n->hijos[1];
        Simbolo* sa = tabla.buscar(a->valor);
        Simbolo* sb = tabla.buscar(b->valor);
        if (!sa) err(n->linea, "Variable '" + a->valor + "' no declarada.");
        if (!sb) err(n->linea, "Variable '" + b->valor + "' no declarada.");
        if (sa && sb && sa->tipo != sb->tipo)
            err(n->linea, "No se pueden intercambiar '" + a->valor + "' (" + sa->tipo +
                ") y '" + b->valor + "' (" + sb->tipo + "): tipos distintos.");
        return;
    }

    if (t == "IncrementoDecremento") {
        auto id = n->hijos[0];
        Simbolo* s = tabla.buscar(id->valor);
        if (!s) err(n->linea, "Variable '" + id->valor + "' no declarada.");
        else if (!esNumerico(s->tipo))
            err(n->linea, "'" + n->valor + "' requiere una variable numerica, no '" + s->tipo + "'.");
        return;
    }

    if (t == "SentenciaIf" || t == "SentenciaWhile") {
        std::string ct = tipoDe(n->hijos[0]);
        // Se permite bool o numerico (estilo C/C++: distinto de cero = verdadero).
        if (ct != "bool" && ct != "error" && !esNumerico(ct))
            err(n->linea, "La condicion debe ser bool o numerica, no '" + ct + "'.");
        if (t == "SentenciaWhile") loopDepth++;
        auto bloque = hijoPorTipo(n, "Bloque");
        if (bloque) analizarBloque(bloque, true);
        if (t == "SentenciaWhile") loopDepth--;
        auto el = hijoPorTipo(n, "Else");
        if (el && !el->hijos.empty()) {
            auto inner = el->hijos[0];
            if (inner->tipo == "Bloque") analizarBloque(inner, true);
            else analizarSent(inner); // else if
        }
        return;
    }

    if (t == "SentenciaFor") {
        tabla.push();
        auto ini = hijoPorTipo(n, "For_Init");
        if (ini && !ini->hijos.empty()) analizarSent(ini->hijos[0]);
        auto cond = hijoPorTipo(n, "For_Condicion");
        if (cond && !cond->hijos.empty()) {
            std::string ct = tipoDe(cond->hijos[0]);
            if (ct != "bool" && ct != "error" && !esNumerico(ct))
                err(n->linea, "La condicion del for debe ser bool o numerica, no '" + ct + "'.");
        }
        auto paso = hijoPorTipo(n, "For_Paso");
        if (paso && !paso->hijos.empty()) analizarSent(paso->hijos[0]);
        loopDepth++;
        auto bloque = hijoPorTipo(n, "Bloque");
        if (bloque) analizarBloque(bloque, true);
        loopDepth--;
        tabla.pop();
        return;
    }

    if (t == "SentenciaForIn") {
        tabla.push();
        auto id = n->hijos[0];
        auto rango = n->hijos.size() > 1 ? n->hijos[1] : nullptr;
        Simbolo s; s.nombre = id->valor; s.tipo = "int"; s.categoria = "variable";
        s.linea = n->linea; s.inicializado = true;
        tabla.declarar(s);
        if (rango && rango->hijos.size() == 2) {
            std::string d = tipoDe(rango->hijos[0]);
            std::string h = tipoDe(rango->hijos[1]);
            if (d != "int" && d != "error") err(n->linea, "El inicio del rango debe ser int.");
            if (h != "int" && h != "error") err(n->linea, "El fin del rango debe ser int.");
        }
        loopDepth++;
        auto bloque = hijoPorTipo(n, "Bloque");
        if (bloque) analizarBloque(bloque, false);
        loopDepth--;
        tabla.pop();
        return;
    }

    if (t == "SentenciaReturn") {
        if (n->hijos.empty()) {
            if (retornoActual != "void" && !retornoActual.empty())
                err(n->linea, "La funcion debe retornar un '" + retornoActual + "'.");
        } else {
            std::string rt = tipoDe(n->hijos[0]);
            if (retornoActual == "void")
                err(n->linea, "Una funcion void no puede retornar un valor.");
            else if (!compatibleAsignar(retornoActual, rt))
                err(n->linea, "Se retorna '" + rt + "' pero la funcion es '" + retornoActual + "'.");
        }
        return;
    }

    if (t == "Break" || t == "Continue") {
        if (loopDepth == 0) err(n->linea, "'" + (t == "Break" ? std::string("break") : std::string("continue")) +
                                "' solo puede usarse dentro de un bucle.");
        return;
    }

    if (t == "Print" || t == "Println") {
        for (auto& h : n->hijos) tipoDe(h);
        return;
    }
    if (t == "SentenciaStream") {
        // 'cout << x << endl;' es compatibilidad tipo C++. Se tipan los
        // operandos salvo 'endl'/'cout', que no son variables reales.
        for (auto& h : n->hijos) {
            if (h->tipo != "OperandoStream" || h->hijos.empty()) continue;
            Nodo op = h->hijos[0];
            if (op->tipo == "Identificador" && (op->valor == "endl" || op->valor == "cout")) continue;
            tipoDe(op);
        }
        return;
    }

    if (t == "LlamadaFuncion") { tipoLlamada(n); return; }

    if (t == "Bloque") { analizarBloque(n, true); return; }

    if (t == "Directiva" || t == "Directiva_Include") return;

    // Cualquier otra cosa: intentar tiparla como expresión
    tipoDe(n);
}

// ── Entrada principal ────────────────────────────────────────
ResultadoSemantico Semantic::analizar(const Nodo& programa) {
    registrarBuiltins();
    prePasoFunciones(programa);

    for (auto& h : programa->hijos) {
        if (!h) continue;
        if (h->tipo == "DefinicionFuncion") {
            auto tr = hijoPorTipo(h, "TipoRetorno");
            retornoActual = tr ? tr->valor : "void";
            tabla.push();
            auto ps = hijoPorTipo(h, "Parametros");
            if (ps) for (auto& p : ps->hijos) {
                if (p->tipo != "Parametro") continue;
                auto pid = hijoPorTipo(p, "Identificador");
                Simbolo s; s.nombre = pid ? pid->valor : "?"; s.tipo = p->valor;
                s.categoria = "parametro"; s.linea = p->linea; s.inicializado = true;
                if (!tabla.declarar(s))
                    err(p->linea, "Parametro '" + s.nombre + "' duplicado.");
            }
            auto bloque = hijoPorTipo(h, "Bloque");
            if (bloque) analizarBloque(bloque, false);
            tabla.pop();
            retornoActual.clear();
        } else {
            analizarSent(h);
        }
    }

    ResultadoSemantico R;
    R.errores = errores;
    R.advertencias = advertencias;
    R.simbolos = tabla.todos();
    R.funciones = funcs;
    return R;
}

} // namespace sigil
