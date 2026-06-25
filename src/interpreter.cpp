// ============================================================
//  interpreter.cpp — Intérprete tree-walking (implementación)
//
//  Recorre el AST y EJECUTA el programa, produciendo salida real.
//  Soporta los operadores nuevos de Sigil: ** .. |> %% >< <=>
// ============================================================
#include "interpreter.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <algorithm>

namespace sigil {

static const long long LIMITE_PASOS = 5000000; // protección anti bucle infinito

// ── Valor::toStr ─────────────────────────────────────────────
std::string Valor::toStr() const {
    switch (tipo) {
        case INT:   return std::to_string(i);
        case BOOL:  return b ? "true" : "false";
        case CHAR:  return s;
        case STR:   return s;
        case VOID:  return "";
        case FLOAT: {
            char buf[64]; snprintf(buf, sizeof(buf), "%g", f); return std::string(buf);
        }
        case ARR: {
            std::string o = "[";
            for (size_t k = 0; k < arr.size(); ++k) { if (k) o += ", "; o += arr[k].toStr(); }
            return o + "]";
        }
    }
    return "";
}

// Quita comillas exteriores y procesa escapes (\n \t \\ \" \').
static std::string desescapar(const std::string& lit) {
    if (lit.size() < 2) return lit;
    std::string s = lit.substr(1, lit.size() - 2);
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[++i];
            switch (c) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '0': out += '\0'; break;
                case '\\': out += '\\'; break;
                case '"': out += '"'; break;
                case '\'': out += '\''; break;
                default: out += c;
            }
        } else out += s[i];
    }
    return out;
}

void Interprete::rerror(int linea, const std::string& m) {
    errores.push_back("Error de ejecucion [linea " + std::to_string(linea) + "]: " + m);
}
bool Interprete::paso(int linea) {
    if (++pasos > LIMITE_PASOS && !abortado) {
        abortado = true;
        rerror(linea, "Limite de ejecucion excedido (posible bucle infinito).");
        flujo = Flujo::RETORNO;
    }
    return !abortado;
}

// ── Entorno ──────────────────────────────────────────────────
Valor* Interprete::buscar(const std::string& n) {
    for (auto it = locales.rbegin(); it != locales.rend(); ++it) {
        auto f = it->find(n);
        if (f != it->end()) return &f->second;
    }
    auto g = globals.find(n);
    if (g != globals.end()) return &g->second;
    return nullptr;
}
void Interprete::declarar(const std::string& n, const Valor& v) {
    if (!locales.empty()) locales.back()[n] = v;
    else globals[n] = v;
}
void Interprete::asignar(const std::string& n, const Valor& v, int linea) {
    Valor* p = buscar(n);
    if (p) *p = v;
    else rerror(linea, "Asignacion a variable no declarada '" + n + "'.");
}

// ── Evaluación de expresiones ────────────────────────────────
Valor Interprete::evalExpr(const Nodo& n) {
    if (!n || abortado) return Valor::Nada();
    const std::string& t = n->tipo;

    if (t == "NumeroEntero")   return Valor::I(atoll(n->valor.c_str()));
    if (t == "NumeroFlotante") return Valor::F(atof(n->valor.c_str()));
    if (t == "LiteralString")  return Valor::S(desescapar(n->valor));
    if (t == "LiteralChar")    return Valor::C(desescapar(n->valor));
    if (t == "LiteralBool")    return Valor::B(n->valor == "true");
    if (t == "Grupo")          return evalExpr(n->hijos.empty() ? nullptr : n->hijos[0]);

    if (t == "Identificador") {
        Valor* p = buscar(n->valor);
        if (!p) { rerror(n->linea, "Variable '" + n->valor + "' no definida."); return Valor::I(0); }
        return *p;
    }

    if (t == "NegacionUnaria") {
        Valor v = evalExpr(n->hijos[0]);
        return v.tipo == Valor::FLOAT ? Valor::F(-v.f) : Valor::I(-(long long)v.num());
    }
    if (t == "NegacionLogica") {
        Valor v = evalExpr(n->hijos[0]);
        return Valor::B(!(v.tipo == Valor::BOOL ? v.b : v.num() != 0));
    }

    if (t == "LlamadaFuncion") {
        std::vector<Valor> args;
        for (auto& h : n->hijos) args.push_back(evalExpr(h));
        return llamar(n->valor, args, n->linea);
    }

    if (t == "ArrayLiteral") {
        std::vector<Valor> v;
        for (auto& h : n->hijos) v.push_back(evalExpr(h));
        return Valor::A(std::move(v));
    }

    if (t == "Indexacion") {
        Valor a = evalExpr(n->hijos[0]);
        long long idx = (long long)evalExpr(n->hijos[1]).num();
        if (a.tipo != Valor::ARR) { rerror(n->linea, "El valor indexado no es un arreglo."); return Valor::I(0); }
        if (idx < 0 || idx >= (long long)a.arr.size()) {
            rerror(n->linea, "Indice fuera de rango (" + std::to_string(idx) + ").");
            return Valor::I(0);
        }
        return a.arr[idx];
    }

    if (n->hijos.size() == 2) return evalBinario(n);

    return Valor::Nada();
}

Valor Interprete::evalBinario(const Nodo& n) {
    const std::string& t = n->tipo;

    // Cortocircuito lógico
    if (t == "And") {
        Valor a = evalExpr(n->hijos[0]);
        bool ab = a.tipo == Valor::BOOL ? a.b : a.num() != 0;
        if (!ab) return Valor::B(false);
        Valor b = evalExpr(n->hijos[1]);
        return Valor::B(b.tipo == Valor::BOOL ? b.b : b.num() != 0);
    }
    if (t == "Or") {
        Valor a = evalExpr(n->hijos[0]);
        bool ab = a.tipo == Valor::BOOL ? a.b : a.num() != 0;
        if (ab) return Valor::B(true);
        Valor b = evalExpr(n->hijos[1]);
        return Valor::B(b.tipo == Valor::BOOL ? b.b : b.num() != 0);
    }

    Valor a = evalExpr(n->hijos[0]);
    Valor b = evalExpr(n->hijos[1]);
    bool flt = (a.tipo == Valor::FLOAT || b.tipo == Valor::FLOAT);

    // Concatenación de cadenas / comparación de texto
    bool texto = (a.tipo == Valor::STR || a.tipo == Valor::CHAR) &&
                 (b.tipo == Valor::STR || b.tipo == Valor::CHAR);

    if (t == "Suma") {
        if (texto) return Valor::S(a.toStr() + b.toStr());
        return flt ? Valor::F(a.num() + b.num()) : Valor::I((long long)a.num() + (long long)b.num());
    }
    if (t == "Resta") return flt ? Valor::F(a.num() - b.num()) : Valor::I((long long)a.num() - (long long)b.num());
    if (t == "Multiplicacion") return flt ? Valor::F(a.num() * b.num()) : Valor::I((long long)a.num() * (long long)b.num());
    if (t == "Division") {
        if (b.num() == 0) { rerror(n->linea, "Division por cero."); return Valor::I(0); }
        return flt ? Valor::F(a.num() / b.num()) : Valor::I((long long)a.num() / (long long)b.num());
    }
    if (t == "Modulo") {
        if ((long long)b.num() == 0) { rerror(n->linea, "Modulo por cero."); return Valor::I(0); }
        return Valor::I((long long)a.num() % (long long)b.num());
    }
    if (t == "Divisible") {
        if ((long long)b.num() == 0) { rerror(n->linea, "Divisibilidad por cero."); return Valor::B(false); }
        return Valor::B(((long long)a.num() % (long long)b.num()) == 0);
    }
    if (t == "Potencia") {
        double r = std::pow(a.num(), b.num());
        return flt ? Valor::F(r) : Valor::I((long long)llround(r));
    }
    if (t == "Comparacion3") {
        if (texto) { int c = a.toStr().compare(b.toStr()); return Valor::I(c < 0 ? -1 : (c > 0 ? 1 : 0)); }
        double x = a.num(), y = b.num();
        return Valor::I(x < y ? -1 : (x > y ? 1 : 0));
    }

    // Relacionales (devuelven bool)
    if (texto) {
        int c = a.toStr().compare(b.toStr());
        if (t == "Igual")      return Valor::B(c == 0);
        if (t == "Distinto")   return Valor::B(c != 0);
        if (t == "Menor")      return Valor::B(c < 0);
        if (t == "Mayor")      return Valor::B(c > 0);
        if (t == "MenorIgual") return Valor::B(c <= 0);
        if (t == "MayorIgual") return Valor::B(c >= 0);
    } else {
        double x = a.num(), y = b.num();
        if (t == "Igual")      return Valor::B(x == y);
        if (t == "Distinto")   return Valor::B(x != y);
        if (t == "Menor")      return Valor::B(x < y);
        if (t == "Mayor")      return Valor::B(x > y);
        if (t == "MenorIgual") return Valor::B(x <= y);
        if (t == "MayorIgual") return Valor::B(x >= y);
    }
    return Valor::Nada();
}

// ── Llamadas (builtins + usuario) ────────────────────────────
Valor Interprete::llamar(const std::string& nombre, std::vector<Valor>& args, int linea) {
    auto arg = [&](size_t k) { return k < args.size() ? args[k] : Valor::I(0); };

    if (nombre == "abs")      return Valor::F(std::fabs(arg(0).num()));
    if (nombre == "sqrt")     return Valor::F(std::sqrt(arg(0).num()));
    if (nombre == "pow")      return Valor::F(std::pow(arg(0).num(), arg(1).num()));
    if (nombre == "max")      return Valor::F(std::max(arg(0).num(), arg(1).num()));
    if (nombre == "min")      return Valor::F(std::min(arg(0).num(), arg(1).num()));
    if (nombre == "len") {
        Valor a = arg(0);
        return Valor::I(a.tipo == Valor::ARR ? (long long)a.arr.size() : (long long)a.toStr().size());
    }
    if (nombre == "toInt")    return Valor::I((long long)arg(0).num());
    if (nombre == "toFloat")  return Valor::F(arg(0).num());
    if (nombre == "toString") return Valor::S(arg(0).toStr());
    if (nombre == "upper" || nombre == "lower") {
        std::string s = arg(0).toStr();
        for (char& ch : s) ch = (nombre == "upper") ? (char)toupper((unsigned char)ch)
                                                     : (char)tolower((unsigned char)ch);
        return Valor::S(s);
    }
    if (nombre == "substr") {
        std::string s = arg(0).toStr();
        long long i0 = (long long)arg(1).num(), len = (long long)arg(2).num();
        if (i0 < 0) i0 = 0;
        if (i0 > (long long)s.size()) i0 = s.size();
        if (len < 0) len = 0;
        return Valor::S(s.substr((size_t)i0, (size_t)len));
    }
    if (nombre == "charAt") {
        std::string s = arg(0).toStr();
        long long k = (long long)arg(1).num();
        if (k < 0 || k >= (long long)s.size()) { rerror(linea, "charAt: indice fuera de rango."); return Valor::S(""); }
        return Valor::C(std::string(1, s[(size_t)k]));
    }
    if (nombre == "indexOf") {
        std::string s = arg(0).toStr(), sub = arg(1).toStr();
        size_t p = s.find(sub);
        return Valor::I(p == std::string::npos ? -1 : (long long)p);
    }

    auto it = funcs.find(nombre);
    if (it == funcs.end()) { rerror(linea, "Funcion '" + nombre + "' no definida."); return Valor::Nada(); }

    Nodo def = it->second;
    Nodo params = nullptr, cuerpo = nullptr;
    for (auto& h : def->hijos) {
        if (h->tipo == "Parametros") params = h;
        if (h->tipo == "Bloque")     cuerpo = h;
    }

    // Nuevo marco de ejecución aislado (las funciones ven globals, no locales del llamador).
    auto guardado = std::move(locales);
    locales.clear();
    locales.emplace_back();
    if (params) {
        size_t k = 0;
        for (auto& p : params->hijos) {
            if (p->tipo != "Parametro") continue;
            std::string pn = p->hijos.empty() ? "?" : p->hijos[0]->valor;
            locales.back()[pn] = arg(k++);
        }
    }

    Flujo flujoPrev = flujo; Valor retPrev = valRetorno;
    flujo = Flujo::NORMAL; valRetorno = Valor::Nada();
    if (cuerpo) for (auto& s : cuerpo->hijos) {
        ejecutar(s);
        if (flujo != Flujo::NORMAL) break;
    }
    Valor ret = (flujo == Flujo::RETORNO) ? valRetorno : Valor::Nada();

    locales = std::move(guardado);
    flujo = abortado ? Flujo::RETORNO : flujoPrev;
    valRetorno = retPrev;
    return ret;
}

// ── Ejecución de sentencias ──────────────────────────────────
void Interprete::ejecutarBloque(const Nodo& n, bool nuevoScope) {
    if (nuevoScope) locales.emplace_back();
    for (auto& h : n->hijos) {
        ejecutar(h);
        if (flujo != Flujo::NORMAL) break;
    }
    if (nuevoScope && !locales.empty()) locales.pop_back();
}

void Interprete::ejecutar(const Nodo& n) {
    if (!n || abortado || flujo != Flujo::NORMAL) return;
    if (!paso(n->linea)) return;
    const std::string& t = n->tipo;

    if (t == "Declaracion") {
        std::string nombre; Valor v = Valor::Nada();
        std::string tipo = n->valor;
        for (auto& h : n->hijos) if (h->tipo == "Identificador") nombre = h->valor;
        // valor por defecto según el tipo
        bool esArr = tipo.size() >= 2 && tipo.compare(tipo.size() - 2, 2, "[]") == 0;
        if (esArr) v = Valor::A({});
        else if (tipo == "float") v = Valor::F(0); else if (tipo == "bool") v = Valor::B(false);
        else if (tipo == "string") v = Valor::S(""); else if (tipo == "char") v = Valor::C("");
        else v = Valor::I(0);
        for (auto& h : n->hijos) if (h->tipo == "Inicializacion" && !h->hijos.empty())
            v = evalExpr(h->hijos[0]);
        declarar(nombre, v);
        return;
    }
    if (t == "Asignacion") { asignar(n->hijos[0]->valor, evalExpr(n->hijos[1]), n->linea); return; }
    if (t == "AsignacionCompuesta") {
        std::string id = n->hijos[0]->valor;
        Valor* p = buscar(id);
        if (!p) { rerror(n->linea, "Variable '" + id + "' no declarada."); return; }
        Valor rhs = evalExpr(n->hijos[1]);
        char op = n->valor[0];
        bool flt = (p->tipo == Valor::FLOAT || rhs.tipo == Valor::FLOAT);
        if (p->tipo == Valor::STR && op == '+') { *p = Valor::S(p->s + rhs.toStr()); return; }
        double a = p->num(), b = rhs.num(), r = 0;
        switch (op) { case '+': r=a+b; break; case '-': r=a-b; break;
                      case '*': r=a*b; break; case '/': if(b==0){rerror(n->linea,"Division por cero.");return;} r=a/b; break; }
        *p = flt ? Valor::F(r) : Valor::I((long long)r);
        return;
    }
    if (t == "IncrementoDecremento") {
        Valor* p = buscar(n->hijos[0]->valor);
        if (!p) { rerror(n->linea, "Variable no declarada."); return; }
        long long d = (n->valor == "++") ? 1 : -1;
        *p = (p->tipo == Valor::FLOAT) ? Valor::F(p->f + d) : Valor::I(p->i + d);
        return;
    }
    if (t == "Swap") {
        Valor* a = buscar(n->hijos[0]->valor);
        Valor* b = buscar(n->hijos[1]->valor);
        if (a && b) std::swap(*a, *b);
        else rerror(n->linea, "Swap con variable no declarada.");
        return;
    }
    if (t == "AsignacionIndice") {
        Valor* p = buscar(n->hijos[0]->valor);
        if (!p) { rerror(n->linea, "Variable '" + n->hijos[0]->valor + "' no declarada."); return; }
        if (p->tipo != Valor::ARR) { rerror(n->linea, "'" + n->hijos[0]->valor + "' no es un arreglo."); return; }
        long long idx = (long long)evalExpr(n->hijos[1]).num();
        Valor v = evalExpr(n->hijos[2]);
        if (idx < 0 || idx >= (long long)p->arr.size()) { rerror(n->linea, "Indice fuera de rango (" + std::to_string(idx) + ")."); return; }
        p->arr[(size_t)idx] = v;
        return;
    }
    if (t == "SentenciaForEach") {
        Valor coll = evalExpr(n->hijos[1]);
        if (coll.tipo != Valor::ARR) { rerror(n->linea, "'for ... in' requiere un arreglo."); return; }
        std::string iv = n->hijos[0]->valor;
        Nodo bloque = nullptr; for (auto& h : n->hijos) if (h->tipo == "Bloque") bloque = h;
        locales.emplace_back();
        for (size_t k = 0; k < coll.arr.size() && !abortado; ++k) {
            locales.back()[iv] = coll.arr[k];
            if (bloque) ejecutarBloque(bloque, true);
            if (flujo == Flujo::BREAK) { flujo = Flujo::NORMAL; break; }
            if (flujo == Flujo::RETORNO) break;
            if (flujo == Flujo::CONTINUE) flujo = Flujo::NORMAL;
            if (!paso(n->linea)) break;
        }
        if (!locales.empty()) locales.pop_back();
        return;
    }
    if (t == "SentenciaIf") {
        Valor c = evalExpr(n->hijos[0]);
        bool cond = (c.tipo == Valor::BOOL ? c.b : c.num() != 0);
        Nodo bloque = nullptr, elseN = nullptr;
        for (auto& h : n->hijos) { if (h->tipo == "Bloque") bloque = h; if (h->tipo == "Else") elseN = h; }
        if (cond) { if (bloque) ejecutarBloque(bloque, true); }
        else if (elseN && !elseN->hijos.empty()) {
            Nodo inner = elseN->hijos[0];
            if (inner->tipo == "Bloque") ejecutarBloque(inner, true);
            else ejecutar(inner); // else if
        }
        return;
    }
    if (t == "SentenciaWhile") {
        Nodo bloque = nullptr; for (auto& h : n->hijos) if (h->tipo == "Bloque") bloque = h;
        while (!abortado) {
            Valor c = evalExpr(n->hijos[0]);
            if (!(c.tipo == Valor::BOOL ? c.b : c.num() != 0)) break;
            if (bloque) ejecutarBloque(bloque, true);
            if (flujo == Flujo::BREAK) { flujo = Flujo::NORMAL; break; }
            if (flujo == Flujo::CONTINUE) { flujo = Flujo::NORMAL; continue; }
            if (flujo == Flujo::RETORNO) break;
            if (!paso(n->linea)) break;
        }
        return;
    }
    if (t == "SentenciaFor") {
        locales.emplace_back();
        Nodo ini=nullptr,cond=nullptr,paso2=nullptr,bloque=nullptr;
        for (auto& h : n->hijos) {
            if (h->tipo=="For_Init") ini=h; else if (h->tipo=="For_Condicion") cond=h;
            else if (h->tipo=="For_Paso") paso2=h; else if (h->tipo=="Bloque") bloque=h;
        }
        if (ini && !ini->hijos.empty()) ejecutar(ini->hijos[0]);
        while (!abortado) {
            if (cond && !cond->hijos.empty()) {
                Valor c = evalExpr(cond->hijos[0]);
                if (!(c.tipo == Valor::BOOL ? c.b : c.num() != 0)) break;
            }
            if (bloque) ejecutarBloque(bloque, true);
            if (flujo == Flujo::BREAK) { flujo = Flujo::NORMAL; break; }
            if (flujo == Flujo::RETORNO) break;
            if (flujo == Flujo::CONTINUE) flujo = Flujo::NORMAL;
            if (paso2 && !paso2->hijos.empty()) ejecutar(paso2->hijos[0]);
            if (!paso(n->linea)) break;
        }
        if (!locales.empty()) locales.pop_back();
        return;
    }
    if (t == "SentenciaForIn") {
        locales.emplace_back();
        std::string iv = n->hijos[0]->valor;
        Nodo rango = n->hijos[1], bloque = nullptr;
        for (auto& h : n->hijos) if (h->tipo == "Bloque") bloque = h;
        long long desde = (long long)evalExpr(rango->hijos[0]).num();
        long long hasta = (long long)evalExpr(rango->hijos[1]).num();
        locales.back()[iv] = Valor::I(desde);
        for (long long k = desde; k <= hasta && !abortado; ++k) {
            locales.back()[iv] = Valor::I(k);
            if (bloque) ejecutarBloque(bloque, true);
            if (flujo == Flujo::BREAK) { flujo = Flujo::NORMAL; break; }
            if (flujo == Flujo::RETORNO) break;
            if (flujo == Flujo::CONTINUE) flujo = Flujo::NORMAL;
            if (!paso(n->linea)) break;
        }
        if (!locales.empty()) locales.pop_back();
        return;
    }
    if (t == "SentenciaReturn") {
        valRetorno = n->hijos.empty() ? Valor::Nada() : evalExpr(n->hijos[0]);
        flujo = Flujo::RETORNO;
        return;
    }
    if (t == "Break")    { flujo = Flujo::BREAK; return; }
    if (t == "Continue") { flujo = Flujo::CONTINUE; return; }

    if (t == "Print" || t == "Println") {
        std::string linea;
        for (size_t k = 0; k < n->hijos.size(); ++k) {
            if (k) linea += " ";
            linea += evalExpr(n->hijos[k]).toStr();
        }
        salida += linea;
        if (t == "Println") salida += "\n";
        return;
    }
    if (t == "SentenciaStream") {
        for (auto& h : n->hijos) if (h->tipo == "OperandoStream" && !h->hijos.empty()) {
            if (h->hijos[0]->tipo == "Identificador" && h->hijos[0]->valor == "endl") { salida += "\n"; continue; }
            salida += evalExpr(h->hijos[0]).toStr();
        }
        return;
    }
    if (t == "LlamadaFuncion") { std::vector<Valor> a; for (auto& h:n->hijos) a.push_back(evalExpr(h)); llamar(n->valor, a, n->linea); return; }
    if (t == "Bloque") { ejecutarBloque(n, true); return; }
    // Directivas y otros: se ignoran en ejecución.
}

// ── Programa ─────────────────────────────────────────────────
ResultadoEjecucion Interprete::ejecutarPrograma(const Nodo& programa) {
    // 1) registrar funciones
    for (auto& h : programa->hijos)
        if (h && h->tipo == "DefinicionFuncion") funcs[h->valor] = h;

    // 2) ejecutar sentencias globales
    for (auto& h : programa->hijos) {
        if (!h || h->tipo == "DefinicionFuncion") continue;
        ejecutar(h);
        if (abortado) break;
    }

    // 3) si existe main(), ejecutarla
    if (funcs.count("main")) {
        std::vector<Valor> sin;
        llamar("main", sin, 0);
    }

    ResultadoEjecucion R;
    R.salida = salida;
    R.errores = errores;
    return R;
}

} // namespace sigil
