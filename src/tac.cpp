// ============================================================
//  tac.cpp — Generación de código intermedio (TAC) + optimización
//
//  El código de tres direcciones (Three-Address Code) descompone
//  expresiones complejas en instrucciones simples del estilo:
//       t1 = a + b
//       t2 = t1 * c
//  Optimización aplicada: plegado de constantes (constant folding):
//       t1 = 2 + 3      ===>     (se sustituye por 5 directamente)
// ============================================================
#include "tac.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <cctype>

namespace sigil {

// ── Optimizador de TAC ───────────────────────────────────────
//  Aplica dos técnicas clásicas sobre el código ya generado:
//   (1) Eliminación de temporales redundantes (una forma de copy
//       propagation): si  t = EXPR  y t se usa una sola vez en
//       x = t, se sustituye por  x = EXPR  y se borra la definición.
//   (2) Eliminación de código muerto: se borran asignaciones a
//       temporales que nunca se usan.
namespace {

bool esTemp(const std::string& s) {
    if (s.size() < 2 || s[0] != 't') return false;
    for (size_t i = 1; i < s.size(); ++i) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}
bool tieneCall(const std::string& s) { return s.find("call ") != std::string::npos; }

// Separa una línea "dst = rhs" -> devuelve true y llena dst/rhs.
bool esAsignacion(const std::string& l, std::string& dst, std::string& rhs) {
    size_t p = l.find(" = ");
    if (p == std::string::npos) return false;
    dst = l.substr(0, p);
    rhs = l.substr(p + 3);
    // dst debe ser un identificador simple (sin espacios ni corchetes)
    if (dst.empty() || dst.find(' ') != std::string::npos ||
        dst.find('[') != std::string::npos) return false;
    return true;
}

// Extrae los identificadores/temporales alfanuméricos de un texto.
std::vector<std::string> palabras(const std::string& s) {
    std::vector<std::string> out; std::string cur;
    for (char c : s) {
        if (isalnum((unsigned char)c) || c == '_') cur += c;
        else { if (!cur.empty()) out.push_back(cur), cur.clear(); }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// ¿La línea rompe el bloque basico o puede tener efectos (no cruzar)?
bool esFrontera(const std::string& l) {
    if (l.empty()) return false;
    if (l.back() == ':') return true;                       // etiqueta
    if (l.rfind("goto ", 0) == 0 || l.rfind("ifFalse ", 0) == 0) return true;
    if (l.rfind("func ", 0) == 0 || l.rfind("endfunc", 0) == 0) return true;
    if (l.rfind("return", 0) == 0) return true;
    if (l.find("call ") != std::string::npos) return true;  // una llamada puede tocar globales
    return false;
}

// Es seguro inlinar  t = rhs  (definido en dLine) en su uso (uLine) solo si
// entre ambas lineas no hay fronteras ni se reasigna ningun operando de rhs.
bool inlineSeguro(const std::vector<std::string>& L, int dLine, int uLine, const std::string& rhs) {
    if (dLine >= uLine) return false;
    std::vector<std::string> ops = palabras(rhs);
    for (int k = dLine + 1; k < uLine; ++k) {
        if (esFrontera(L[k])) return false;
        std::string dd, rr;
        if (esAsignacion(L[k], dd, rr))
            for (auto& o : ops) if (o == dd) return false;  // un operando fue reasignado
    }
    return true;
}

std::vector<std::string> optimizar(const std::vector<std::string>& in, int& propag, int& elim) {
    std::vector<std::string> L(in);
    propag = 0; elim = 0;
    bool cambio = true; int guard = 0;
    while (cambio && guard++ < 500) {
        cambio = false;

        // definiciones de temporales y conteo de usos
        std::unordered_map<std::string, int> defLinea;
        std::unordered_map<std::string, std::string> defRhs;
        std::unordered_map<std::string, int> usos;
        for (int i = 0; i < (int)L.size(); ++i) {
            std::string dst, rhs;
            if (esAsignacion(L[i], dst, rhs) && esTemp(dst)) { defLinea[dst] = i; defRhs[dst] = rhs; }
        }
        for (auto& l : L) {
            std::string dst, rhs;
            std::string parte = esAsignacion(l, dst, rhs) ? rhs : l;
            for (auto& w : palabras(parte)) if (esTemp(w)) usos[w]++;
        }

        // (1) eliminación de temporales redundantes
        for (int i = 0; i < (int)L.size() && !cambio; ++i) {
            std::string dst, rhs;
            if (!esAsignacion(L[i], dst, rhs)) continue;
            if (!esTemp(rhs)) continue;              // rhs debe ser exactamente un temporal
            std::string t = rhs;
            if (defLinea.count(t) && usos[t] == 1 && !tieneCall(defRhs[t]) &&
                inlineSeguro(L, defLinea[t], i, defRhs[t])) {
                L[i] = dst + " = " + defRhs[t];      // inlina la expresión
                L.erase(L.begin() + defLinea[t]);    // borra la definición
                propag++; cambio = true;
            }
        }
        if (cambio) continue;

        // (2) eliminación de código muerto
        for (int i = 0; i < (int)L.size() && !cambio; ++i) {
            std::string dst, rhs;
            if (!esAsignacion(L[i], dst, rhs)) continue;
            if (esTemp(dst) && usos[dst] == 0 && !tieneCall(rhs)) {
                L.erase(L.begin() + i);
                elim++; cambio = true;
            }
        }
    }
    return L;
}

} // namespace anónimo

static std::string fmtNum(double v, bool entero) {
    if (entero) return std::to_string((long long)llround(v));
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v);
    return std::string(buf);
}

bool TAC::esNumero(const std::string& s, bool& esFloat) {
    if (s.empty()) return false;
    size_t i = 0; if (s[0] == '-') i = 1;
    if (i >= s.size()) return false;
    bool punto = false, dig = false;
    for (; i < s.size(); ++i) {
        if (isdigit((unsigned char)s[i])) dig = true;
        else if (s[i] == '.' && !punto) punto = true;
        else return false;
    }
    esFloat = punto;
    return dig;
}

std::string TAC::opBase(const std::string& comp) {
    if (!comp.empty()) return std::string(1, comp[0]); // "+="->"+", etc.
    return "+";
}

// Intenta plegar a op b si ambos son constantes numéricas.
static bool plegar(const std::string& op, const std::string& a, const std::string& b,
                   std::string& out) {
    bool fa, fb;
    if (!TAC::esNumero(a, fa) || !TAC::esNumero(b, fb)) return false;
    bool entero = !fa && !fb;
    double x = atof(a.c_str()), y = atof(b.c_str()), r = 0;
    if (op == "+") r = x + y;
    else if (op == "-") r = x - y;
    else if (op == "*") r = x * y;
    else if (op == "/") { if (y == 0) return false; r = entero ? (double)((long long)x / (long long)y) : x / y; }
    else if (op == "%") { if (!entero || (long long)y == 0) return false; r = (double)((long long)x % (long long)y); }
    else if (op == "**") { r = std::pow(x, y); if (entero && r != std::floor(r)) entero = false; }
    else return false;
    out = fmtNum(r, entero);
    return true;
}

std::string TAC::genExpr(const Nodo& n) {
    if (!n) return "0";
    const std::string& t = n->tipo;

    if (t == "NumeroEntero" || t == "NumeroFlotante" ||
        t == "LiteralChar" || t == "LiteralString") return n->valor;
    if (t == "LiteralBool") return n->valor == "true" ? "1" : "0";
    if (t == "Identificador") return n->valor;
    if (t == "Grupo") return genExpr(n->hijos.empty() ? nullptr : n->hijos[0]);

    if (t == "NegacionUnaria") {
        std::string a = genExpr(n->hijos[0]);
        bool f; if (esNumero(a, f)) { return fmtNum(-atof(a.c_str()), !f); }
        std::string r = nuevoTemp(); emit(r + " = - " + a); return r;
    }
    if (t == "NegacionLogica") {
        std::string a = genExpr(n->hijos[0]);
        std::string r = nuevoTemp(); emit(r + " = ! " + a); return r;
    }

    if (t == "LlamadaFuncion") {
        std::vector<std::string> args;
        for (auto& h : n->hijos) args.push_back(genExpr(h));
        for (auto& a : args) emit("param " + a);
        std::string r = nuevoTemp();
        emit(r + " = call " + n->valor + ", " + std::to_string(args.size()));
        return r;
    }

    if (t == "ArrayLiteral") {
        std::string lst = "[";
        for (size_t i = 0; i < n->hijos.size(); ++i) {
            if (i) lst += ", ";
            lst += genExpr(n->hijos[i]);
        }
        lst += "]";
        std::string r = nuevoTemp();
        emit(r + " = " + lst);
        return r;
    }

    if (t == "Indexacion") {
        std::string a = genExpr(n->hijos[0]);
        std::string idx = genExpr(n->hijos[1]);
        std::string r = nuevoTemp();
        emit(r + " = " + a + "[" + idx + "]");
        return r;
    }

    // Operadores binarios
    if (n->hijos.size() == 2 && !n->valor.empty()) {
        std::string a = genExpr(n->hijos[0]);
        std::string b = genExpr(n->hijos[1]);
        std::string op = n->valor;
        std::string folded;
        if (plegar(op, a, b, folded)) {
            pliegues++;
            opt.push_back("Plegado: (" + a + " " + op + " " + b + ") => " + folded);
            return folded;
        }
        std::string r = nuevoTemp();
        emit(r + " = " + a + " " + op + " " + b);
        return r;
    }
    return "0";
}

void TAC::genStmt(const Nodo& n) {
    if (!n) return;
    const std::string& t = n->tipo;

    if (t == "Declaracion") {
        std::string nombre;
        for (auto& h : n->hijos) if (h->tipo == "Identificador") nombre = h->valor;
        for (auto& h : n->hijos) if (h->tipo == "Inicializacion" && !h->hijos.empty()) {
            std::string v = genExpr(h->hijos[0]);
            emit(nombre + " = " + v);
        }
        return;
    }
    if (t == "Asignacion") {
        std::string v = genExpr(n->hijos[1]);
        emit(n->hijos[0]->valor + " = " + v);
        return;
    }
    if (t == "AsignacionCompuesta") {
        std::string v = genExpr(n->hijos[1]);
        std::string id = n->hijos[0]->valor;
        emit(id + " = " + id + " " + opBase(n->valor) + " " + v);
        return;
    }
    if (t == "IncrementoDecremento") {
        std::string id = n->hijos[0]->valor;
        emit(id + " = " + id + (n->valor == "++" ? " + 1" : " - 1"));
        return;
    }
    if (t == "Swap") {
        std::string a = n->hijos[0]->valor, b = n->hijos[1]->valor;
        std::string tmp = nuevoTemp();
        emit(tmp + " = " + a); emit(a + " = " + b); emit(b + " = " + tmp);
        return;
    }
    if (t == "SentenciaIf") {
        std::string c = genExpr(n->hijos[0]);
        Nodo bloque = nullptr, elseN = nullptr;
        for (auto& h : n->hijos) { if (h->tipo == "Bloque") bloque = h; if (h->tipo == "Else") elseN = h; }
        std::string Lelse = nuevaEtiq();
        emit("ifFalse " + c + " goto " + Lelse);
        if (bloque) genStmt(bloque);
        if (elseN && !elseN->hijos.empty()) {
            std::string Lend = nuevaEtiq();
            emit("goto " + Lend);
            emit(Lelse + ":");
            genStmt(elseN->hijos[0]);
            emit(Lend + ":");
        } else {
            emit(Lelse + ":");
        }
        return;
    }
    if (t == "SentenciaWhile") {
        std::string Lini = nuevaEtiq(), Lfin = nuevaEtiq();
        emit(Lini + ":");
        std::string c = genExpr(n->hijos[0]);
        emit("ifFalse " + c + " goto " + Lfin);
        bucles.push_back({Lini, Lfin});
        for (auto& h : n->hijos) if (h->tipo == "Bloque") genStmt(h);
        bucles.pop_back();
        emit("goto " + Lini);
        emit(Lfin + ":");
        return;
    }
    if (t == "SentenciaFor") {
        for (auto& h : n->hijos) if (h->tipo == "For_Init" && !h->hijos.empty()) genStmt(h->hijos[0]);
        std::string Lini = nuevaEtiq(), Lfin = nuevaEtiq();
        emit(Lini + ":");
        for (auto& h : n->hijos) if (h->tipo == "For_Condicion" && !h->hijos.empty()) {
            std::string c = genExpr(h->hijos[0]);
            emit("ifFalse " + c + " goto " + Lfin);
        }
        bucles.push_back({Lini, Lfin});
        for (auto& h : n->hijos) if (h->tipo == "Bloque") genStmt(h);
        bucles.pop_back();
        for (auto& h : n->hijos) if (h->tipo == "For_Paso" && !h->hijos.empty()) genStmt(h->hijos[0]);
        emit("goto " + Lini);
        emit(Lfin + ":");
        return;
    }
    if (t == "SentenciaForIn") {
        std::string iv = n->hijos[0]->valor;
        Nodo rango = n->hijos[1];
        std::string desde = genExpr(rango->hijos[0]);
        std::string hasta = genExpr(rango->hijos[1]);
        emit(iv + " = " + desde);
        std::string Lini = nuevaEtiq(), Lfin = nuevaEtiq();
        emit(Lini + ":");
        std::string c = nuevoTemp();
        emit(c + " = " + iv + " <= " + hasta);
        emit("ifFalse " + c + " goto " + Lfin);
        bucles.push_back({Lini, Lfin});
        for (auto& h : n->hijos) if (h->tipo == "Bloque") genStmt(h);
        bucles.pop_back();
        emit(iv + " = " + iv + " + 1");
        emit("goto " + Lini);
        emit(Lfin + ":");
        return;
    }
    if (t == "AsignacionIndice") {
        std::string idx = genExpr(n->hijos[1]);
        std::string v = genExpr(n->hijos[2]);
        emit(n->hijos[0]->valor + "[" + idx + "] = " + v);
        return;
    }
    if (t == "SentenciaForEach") {
        std::string iv = n->hijos[0]->valor;
        std::string coll = genExpr(n->hijos[1]);
        std::string idx = nuevoTemp();
        emit(idx + " = 0");
        std::string Lini = nuevaEtiq(), Lfin = nuevaEtiq();
        emit(Lini + ":");
        std::string ln = nuevoTemp(); emit(ln + " = len " + coll);
        std::string c = nuevoTemp(); emit(c + " = " + idx + " < " + ln);
        emit("ifFalse " + c + " goto " + Lfin);
        emit(iv + " = " + coll + "[" + idx + "]");
        bucles.push_back({Lini, Lfin});
        for (auto& h : n->hijos) if (h->tipo == "Bloque") genStmt(h);
        bucles.pop_back();
        emit(idx + " = " + idx + " + 1");
        emit("goto " + Lini);
        emit(Lfin + ":");
        return;
    }
    if (t == "SentenciaReturn") {
        if (n->hijos.empty()) emit("return");
        else emit("return " + genExpr(n->hijos[0]));
        return;
    }
    if (t == "Break") {
        if (!bucles.empty()) emit("goto " + bucles.back().second);
        return;
    }
    if (t == "Continue") {
        if (!bucles.empty()) emit("goto " + bucles.back().first);
        return;
    }
    if (t == "Print" || t == "Println") {
        std::vector<std::string> args;
        for (auto& h : n->hijos) args.push_back(genExpr(h));
        for (auto& a : args) emit("param " + a);
        emit("call " + std::string(t == "Print" ? "print" : "println") + ", " + std::to_string(args.size()));
        return;
    }
    if (t == "SentenciaStream") {
        for (auto& h : n->hijos)
            if (h->tipo == "OperandoStream" && !h->hijos.empty()) {
                std::string a = genExpr(h->hijos[0]);
                emit("param " + a); emit("call print, 1");
            }
        return;
    }
    if (t == "LlamadaFuncion") { genExpr(n); return; }
    if (t == "Bloque") { for (auto& h : n->hijos) genStmt(h); return; }
}

ResultadoTAC TAC::generar(const Nodo& programa) {
    for (auto& h : programa->hijos) {
        if (!h) continue;
        if (h->tipo == "DefinicionFuncion") {
            emit("");
            emit("func " + h->valor + ":");
            for (auto& p : h->hijos)
                if (p->tipo == "Parametros")
                    for (auto& par : p->hijos)
                        if (par->tipo == "Parametro" && !par->hijos.empty())
                            emit("  param_in " + par->hijos[0]->valor);
            for (auto& b : h->hijos) if (b->tipo == "Bloque") genStmt(b);
            emit("endfunc");
        } else {
            genStmt(h);
        }
    }
    ResultadoTAC R;
    R.codigo = cod;
    R.pliegues = pliegues;
    R.optimizaciones = opt;
    R.optimizado = optimizar(cod, R.propagaciones, R.eliminadas);
    return R;
}

} // namespace sigil
