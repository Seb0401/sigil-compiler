// ============================================================
//  parser.cpp — Analizador sintáctico (implementación)
//
//  Construye simultáneamente:
//    - CST: árbol concreto (todos los tokens como nodos)
//    - AST: árbol abstracto (solo lo semánticamente relevante)
//
//  Precedencia de expresiones (de menor a mayor):
//    ||  →  &&  →  (== != < > <= >= <=>)  →  + -  →  * / % %%
//    →  ** (der.)  →  |>  →  unario(- !)  →  factor
// ============================================================
#include "parser.hpp"

namespace sigil {

// ── Utilidades de cursor ─────────────────────────────────────
Token Parser::act() const {
    return pos < (int)toks.size() ? toks[pos] : Token{"FIN", "FIN", -1, -1};
}
Token Parser::sig(int k) const {
    int j = pos + k;
    return j < (int)toks.size() ? toks[j] : Token{"FIN", "FIN", -1, -1};
}
void Parser::av() { if (pos < (int)toks.size()) pos++; }

bool Parser::ver(const std::string& tp, const std::string& lx) const {
    Token t = act();
    return lx.empty() ? t.tipo == tp : (t.tipo == tp && t.lexema == lx);
}

Nodo Parser::con(const std::string& tp, const std::string& lx) {
    Token t = act();
    bool ok = lx.empty() ? t.tipo == tp : (t.tipo == tp && t.lexema == lx);
    if (ok) { av(); auto n = mkN("Token_" + tp, t.lexema); n->linea = t.linea; return n; }
    std::string esp = lx.empty() ? tp : ("'" + lx + "'");
    std::string enc = t.tipo == "FIN" ? "fin de archivo" : ("'" + t.lexema + "'");
    errs.push_back("Error sintactico [linea " + std::to_string(t.linea) +
                   "]: Se esperaba " + esp + " pero se encontro " + enc + ".");
    return mkN("Error", t.lexema);
}

// ── Nombres semánticos de operadores ─────────────────────────
std::string Parser::nombreOp(const std::string& op) {
    if (op == "||") return "Or";
    if (op == "&&") return "And";
    if (op == "==") return "Igual";
    if (op == "!=") return "Distinto";
    if (op == "<")  return "Menor";
    if (op == ">")  return "Mayor";
    if (op == "<=") return "MenorIgual";
    if (op == ">=") return "MayorIgual";
    if (op == "<=>") return "Comparacion3";
    if (op == "+")  return "Suma";
    if (op == "-")  return "Resta";
    if (op == "*")  return "Multiplicacion";
    if (op == "/")  return "Division";
    if (op == "%")  return "Modulo";
    if (op == "%%") return "Divisible";
    if (op == "**") return "Potencia";
    return "OpBin";
}

Parser::Par Parser::mkBin(const std::string& op, Par L, Par R, int linea) {
    auto cst = mkN("CST_" + nombreOp(op), op);
    cst->agregar(L.first);
    cst->agregar(mkN("Token_OPERATOR", op));
    cst->agregar(R.first);
    auto ast = mkN(nombreOp(op), op);   // valor = símbolo del operador
    ast->linea = linea;
    ast->agregar(L.second);
    ast->agregar(R.second);
    return {cst, ast};
}

// ── Expresiones ──────────────────────────────────────────────
Parser::Par Parser::expr() { return exprOr(); }

Parser::Par Parser::exprOr() {
    auto L = exprAnd();
    while (ver("OPERATOR", "||")) {
        int ln = act().linea; av();
        auto R = exprAnd();
        L = mkBin("||", L, R, ln);
    }
    return L;
}

Parser::Par Parser::exprAnd() {
    auto L = exprCmp();
    while (ver("OPERATOR", "&&")) {
        int ln = act().linea; av();
        auto R = exprCmp();
        L = mkBin("&&", L, R, ln);
    }
    return L;
}

Parser::Par Parser::exprCmp() {
    auto L = exprSuma();
    Token t = act();
    if (t.tipo == "OPERATOR" &&
        (t.lexema == "==" || t.lexema == "!=" || t.lexema == "<" ||
         t.lexema == ">"  || t.lexema == "<=" || t.lexema == ">=" ||
         t.lexema == "<=>")) {
        int ln = t.linea; av();
        auto R = exprSuma();
        return mkBin(t.lexema, L, R, ln);
    }
    return L;
}

Parser::Par Parser::exprSuma() {
    auto L = exprTerm();
    while (ver("OPERATOR", "+") || ver("OPERATOR", "-")) {
        Token op = act(); av();
        auto R = exprTerm();
        L = mkBin(op.lexema, L, R, op.linea);
    }
    return L;
}

Parser::Par Parser::exprTerm() {
    auto L = exprPot();
    while (ver("OPERATOR", "*") || ver("OPERATOR", "/") ||
           ver("OPERATOR", "%") || ver("OPERATOR", "%%")) {
        Token op = act(); av();
        auto R = exprPot();
        L = mkBin(op.lexema, L, R, op.linea);
    }
    return L;
}

// Potencia: asociativa por la derecha (2 ** 3 ** 2 = 2 ** (3 ** 2))
Parser::Par Parser::exprPot() {
    auto L = exprPipe();
    if (ver("OPERATOR", "**")) {
        Token op = act(); av();
        auto R = exprPot();           // recursión a la derecha
        return mkBin("**", L, R, op.linea);
    }
    return L;
}

// Pipe:  x |> f       ≡  f(x)
//        x |> f(2)    ≡  f(x, 2)
Parser::Par Parser::exprPipe() {
    auto L = exprUnary();
    while (ver("OPERATOR", "|>")) {
        int ln = act().linea; av();
        Token fn = act();
        if (fn.tipo != "IDENTIFIER") {
            errs.push_back("Error sintactico [linea " + std::to_string(ln) +
                           "]: Tras '|>' se esperaba el nombre de una funcion.");
            return L;
        }
        av();
        auto cst = mkN("CST_Pipe", fn.lexema);
        cst->agregar(L.first);
        cst->agregar(mkN("Token_OPERATOR", "|>"));
        cst->agregar(mkN("Token_IDENTIFIER", fn.lexema));
        auto ast = mkN("LlamadaFuncion", fn.lexema);
        ast->linea = ln;
        ast->agregar(L.second);                       // primer argumento = valor canalizado
        if (ver("DELIMITER", "(")) {                  // argumentos extra
            cst->agregar(con("DELIMITER", "("));
            if (!ver("DELIMITER", ")")) {
                auto a = expr(); cst->agregar(a.first); ast->agregar(a.second);
                while (ver("DELIMITER", ",")) {
                    cst->agregar(con("DELIMITER", ","));
                    auto b = expr(); cst->agregar(b.first); ast->agregar(b.second);
                }
            }
            cst->agregar(con("DELIMITER", ")"));
        }
        L = {cst, ast};
    }
    return L;
}

Parser::Par Parser::exprUnary() {
    Token t = act();
    if (t.tipo == "OPERATOR" && (t.lexema == "-" || t.lexema == "!")) {
        av();
        auto sub = exprUnary();
        auto cst = mkN("CST_Unario", t.lexema);
        cst->agregar(mkN("Token_OPERATOR", t.lexema));
        cst->agregar(sub.first);
        auto ast = mkN(t.lexema == "-" ? "NegacionUnaria" : "NegacionLogica", t.lexema);
        ast->linea = t.linea;
        ast->agregar(sub.second);
        return {cst, ast};
    }
    return factor();
}

Parser::Par Parser::factor() {
    Token t = act();

    // Literales
    if (t.tipo == "LITERAL_INT" || t.tipo == "LITERAL_FLOAT" ||
        t.tipo == "LITERAL_STRING" || t.tipo == "LITERAL_CHAR") {
        av();
        std::string ta = t.tipo == "LITERAL_INT"   ? "NumeroEntero" :
                         t.tipo == "LITERAL_FLOAT" ? "NumeroFlotante" :
                         t.tipo == "LITERAL_STRING" ? "LiteralString" : "LiteralChar";
        auto ast = mkN(ta, t.lexema); ast->linea = t.linea;
        return {mkN("Factor_" + t.tipo, t.lexema), ast};
    }

    // Booleanos
    if (t.tipo == "KEYWORD" && (t.lexema == "true" || t.lexema == "false")) {
        av();
        auto ast = mkN("LiteralBool", t.lexema); ast->linea = t.linea;
        return {mkN("Factor_Bool", t.lexema), ast};
    }

    // Identificador, llamada a función o indexación de arreglo
    if (t.tipo == "IDENTIFIER") {
        av();
        if (ver("DELIMITER", "(")) return llamada(t.lexema, t.linea);
        if (ver("DELIMITER", "[")) {                       // arr[indice]
            auto cst = mkN("CST_Indexacion");
            cst->agregar(mkN("Token_IDENTIFIER", t.lexema));
            cst->agregar(con("DELIMITER", "["));
            auto idx = expr(); cst->agregar(idx.first);
            cst->agregar(con("DELIMITER", "]"));
            auto ast = mkN("Indexacion"); ast->linea = t.linea;
            auto id = mkN("Identificador", t.lexema); id->linea = t.linea;
            ast->agregar(id); ast->agregar(idx.second);
            return {cst, ast};
        }
        auto ast = mkN("Identificador", t.lexema); ast->linea = t.linea;
        return {mkN("Factor_ID", t.lexema), ast};
    }

    // Literal de arreglo:  [ expr, expr, ... ]
    if (t.tipo == "DELIMITER" && t.lexema == "[") {
        auto cst = mkN("CST_ArrayLiteral");
        cst->agregar(con("DELIMITER", "["));
        auto ast = mkN("ArrayLiteral"); ast->linea = t.linea;
        if (!ver("DELIMITER", "]")) {
            auto e = expr(); cst->agregar(e.first); ast->agregar(e.second);
            while (ver("DELIMITER", ",")) {
                cst->agregar(con("DELIMITER", ","));
                auto b = expr(); cst->agregar(b.first); ast->agregar(b.second);
            }
        }
        cst->agregar(con("DELIMITER", "]"));
        return {cst, ast};
    }

    // Subexpresión agrupada
    if (t.tipo == "DELIMITER" && t.lexema == "(") {
        auto cst = mkN("Factor_Grupo");
        cst->agregar(con("DELIMITER", "("));
        auto e = expr();
        cst->agregar(e.first);
        cst->agregar(con("DELIMITER", ")"));
        auto ast = mkN("Grupo"); ast->linea = t.linea; ast->agregar(e.second);
        return {cst, ast};
    }

    errs.push_back("Error sintactico [linea " + std::to_string(t.linea) +
                   "]: Factor invalido '" + t.lexema + "'.");
    av();
    return {mkN("Error_Factor", t.lexema), mkN("Error", t.lexema)};
}

Parser::Par Parser::llamada(const std::string& nom, int lin) {
    auto cst = mkN("LlamadaFuncion");
    cst->agregar(mkN("Token_IDENTIFIER", nom));
    cst->agregar(con("DELIMITER", "("));
    auto ast = mkN("LlamadaFuncion", nom); ast->linea = lin;
    if (!ver("DELIMITER", ")")) {
        auto a = expr(); cst->agregar(a.first); ast->agregar(a.second);
        while (ver("DELIMITER", ",")) {
            cst->agregar(con("DELIMITER", ","));
            auto b = expr(); cst->agregar(b.first); ast->agregar(b.second);
        }
    }
    cst->agregar(con("DELIMITER", ")"));
    return {cst, ast};
}

// ── Bloque ───────────────────────────────────────────────────
Parser::Par Parser::bloque() {
    auto cst = mkN("Bloque"), ast = mkN("Bloque");
    cst->agregar(con("DELIMITER", "{"));
    while (!ver("DELIMITER", "}") && act().tipo != "FIN") {
        auto s = sentencia();
        cst->agregar(s.first);
        if (s.second) ast->agregar(s.second);
    }
    cst->agregar(con("DELIMITER", "}"));
    return {cst, ast};
}

// Consume un par '[' ']' tras un tipo y devuelve el sufijo "[]" (o "").
std::string Parser::sufijoArray(const Nodo& cst) {
    if (ver("DELIMITER", "[")) {
        cst->agregar(con("DELIMITER", "["));
        cst->agregar(con("DELIMITER", "]"));
        return "[]";
    }
    return "";
}

// ── Declaración de variable ──────────────────────────────────
Parser::Par Parser::decl() {
    Token tp = act(); av();
    auto cst = mkN("Declaracion");
    cst->agregar(mkN("Token_KEYWORD", tp.lexema));
    std::string tipo = tp.lexema + sufijoArray(cst);   // soporta int[] float[] ...
    Token nm = act();
    cst->agregar(con("IDENTIFIER"));
    auto ast = mkN("Declaracion", tipo); ast->linea = tp.linea; ast->dtype = tipo;
    auto id = mkN("Identificador", nm.lexema); id->linea = nm.linea;
    ast->agregar(id);
    if (ver("OPERATOR", "=")) {
        cst->agregar(con("OPERATOR", "="));
        auto e = expr(); cst->agregar(e.first);
        auto ini = mkN("Inicializacion"); ini->agregar(e.second);
        ast->agregar(ini);
    }
    cst->agregar(con("DELIMITER", ";"));
    return {cst, ast};
}

Parser::Par Parser::declSinPunto() {
    Token tp = act(); av();
    auto cst = mkN("Declaracion_Init");
    cst->agregar(mkN("Token_KEYWORD", tp.lexema));
    std::string tipo = tp.lexema + sufijoArray(cst);
    Token nm = act();
    cst->agregar(con("IDENTIFIER"));
    auto ast = mkN("Declaracion", tipo); ast->linea = tp.linea; ast->dtype = tipo;
    auto id = mkN("Identificador", nm.lexema); id->linea = nm.linea;
    ast->agregar(id);
    if (ver("OPERATOR", "=")) {
        cst->agregar(con("OPERATOR", "="));
        auto e = expr(); cst->agregar(e.first);
        auto ini = mkN("Inicializacion"); ini->agregar(e.second);
        ast->agregar(ini);
    }
    return {cst, ast};
}

// ── Asignación ───────────────────────────────────────────────
Parser::Par Parser::asig(const std::string& nom) {
    Token op = act(); av();
    auto cst = mkN("Asignacion", op.lexema);
    cst->agregar(mkN("Token_IDENTIFIER", nom));
    cst->agregar(mkN("Token_OPERATOR", op.lexema));
    auto e = expr(); cst->agregar(e.first);
    cst->agregar(con("DELIMITER", ";"));
    std::string ta = op.lexema == "=" ? "Asignacion" : "AsignacionCompuesta";
    auto ast = mkN(ta, op.lexema); ast->linea = op.linea;
    auto id = mkN("Identificador", nom); id->linea = op.linea;
    ast->agregar(id);
    ast->agregar(e.second);
    return {cst, ast};
}

Parser::Par Parser::asigSinPunto(const std::string& nom) {
    Token op = act(); av();
    auto cst = mkN("Asignacion_" + op.lexema);
    cst->agregar(mkN("Token_IDENTIFIER", nom));
    cst->agregar(mkN("Token_OPERATOR", op.lexema));
    auto e = expr(); cst->agregar(e.first);
    std::string ta = op.lexema == "=" ? "Asignacion" : "AsignacionCompuesta";
    auto ast = mkN(ta, op.lexema); ast->linea = op.linea;
    auto id = mkN("Identificador", nom); id->linea = op.linea;
    ast->agregar(id);
    ast->agregar(e.second);
    return {cst, ast};
}

// ── Asignación a índice:  arr[i] = expr ;  ───────────────────
Parser::Par Parser::asigIndice(const std::string& nom) {
    int ln = act().linea;
    auto cst = mkN("CST_AsignacionIndice");
    cst->agregar(mkN("Token_IDENTIFIER", nom));
    cst->agregar(con("DELIMITER", "["));
    auto idx = expr(); cst->agregar(idx.first);
    cst->agregar(con("DELIMITER", "]"));
    cst->agregar(con("OPERATOR", "="));
    auto val = expr(); cst->agregar(val.first);
    cst->agregar(con("DELIMITER", ";"));
    auto ast = mkN("AsignacionIndice"); ast->linea = ln;
    auto id = mkN("Identificador", nom); id->linea = ln;
    ast->agregar(id); ast->agregar(idx.second); ast->agregar(val.second);
    return {cst, ast};
}

// ── Swap:  a >< b ;  ─────────────────────────────────────────
Parser::Par Parser::swapStmt(const std::string& nom) {
    int ln = act().linea; av(); // consume '><'
    Token b = act();
    auto cst = mkN("CST_Swap", "><");
    cst->agregar(mkN("Token_IDENTIFIER", nom));
    cst->agregar(mkN("Token_OPERATOR", "><"));
    cst->agregar(con("IDENTIFIER"));
    cst->agregar(con("DELIMITER", ";"));
    auto ast = mkN("Swap", "><"); ast->linea = ln;
    auto a = mkN("Identificador", nom); a->linea = ln;
    auto c = mkN("Identificador", b.lexema); c->linea = b.linea;
    ast->agregar(a); ast->agregar(c);
    return {cst, ast};
}

// ── if / else ────────────────────────────────────────────────
Parser::Par Parser::sentIf() {
    auto cst = mkN("SentenciaIf"), ast = mkN("SentenciaIf");
    ast->linea = act().linea;
    cst->agregar(con("KEYWORD", "if"));
    cst->agregar(con("DELIMITER", "("));
    auto c = expr();
    cst->agregar(c.first); cst->agregar(con("DELIMITER", ")"));
    ast->agregar(c.second);
    auto b = bloque(); cst->agregar(b.first); ast->agregar(b.second);
    if (ver("KEYWORD", "else")) {
        cst->agregar(con("KEYWORD", "else"));
        auto el = mkN("Else");
        if (ver("KEYWORD", "if")) {            // else if encadenado
            auto e = sentIf(); cst->agregar(e.first); el->agregar(e.second);
        } else {
            auto e = bloque(); cst->agregar(e.first); el->agregar(e.second);
        }
        ast->agregar(el);
    }
    return {cst, ast};
}

// ── while ────────────────────────────────────────────────────
Parser::Par Parser::sentWhile() {
    auto cst = mkN("SentenciaWhile"), ast = mkN("SentenciaWhile");
    ast->linea = act().linea;
    cst->agregar(con("KEYWORD", "while"));
    cst->agregar(con("DELIMITER", "("));
    auto c = expr();
    cst->agregar(c.first); cst->agregar(con("DELIMITER", ")"));
    ast->agregar(c.second);
    auto b = bloque(); cst->agregar(b.first); ast->agregar(b.second);
    return {cst, ast};
}

// ── for clásico ──────────────────────────────────────────────
Parser::Par Parser::sentFor() {
    auto cst = mkN("SentenciaFor"), ast = mkN("SentenciaFor");
    ast->linea = act().linea;
    cst->agregar(con("KEYWORD", "for"));
    cst->agregar(con("DELIMITER", "("));

    // init
    auto iC = mkN("For_Init"), iA = mkN("For_Init");
    if (!ver("DELIMITER", ";")) {
        if (act().tipo == "KEYWORD" && esTipoDato(act().lexema)) {
            auto d = declSinPunto(); iC->agregar(d.first); iA->agregar(d.second);
        } else if (act().tipo == "IDENTIFIER") {
            std::string nm = act().lexema; av();
            auto a = asigSinPunto(nm); iC->agregar(a.first); iA->agregar(a.second);
        }
    }
    cst->agregar(iC); cst->agregar(con("DELIMITER", ";")); ast->agregar(iA);

    // condición
    auto cC = mkN("For_Condicion"), cA = mkN("For_Condicion");
    if (!ver("DELIMITER", ";")) {
        auto x = expr(); cC->agregar(x.first); cA->agregar(x.second);
    }
    cst->agregar(cC); cst->agregar(con("DELIMITER", ";")); ast->agregar(cA);

    // paso
    auto pC = mkN("For_Paso"), pA = mkN("For_Paso");
    if (!ver("DELIMITER", ")")) {
        if (act().tipo == "IDENTIFIER") {
            std::string nm = act().lexema; av();
            if (ver("OPERATOR", "++") || ver("OPERATOR", "--")) {
                std::string op = act().lexema; int ln = act().linea; av();
                auto cn = mkN("IncrDecr_" + op, op);
                cn->agregar(mkN("Token_IDENTIFIER", nm));
                cn->agregar(mkN("Token_OPERATOR", op));
                pC->agregar(cn);
                auto an = mkN("IncrementoDecremento", op); an->linea = ln;
                auto id = mkN("Identificador", nm); id->linea = ln;
                an->agregar(id); pA->agregar(an);
            } else {
                auto a = asigSinPunto(nm); pC->agregar(a.first); pA->agregar(a.second);
            }
        }
    }
    cst->agregar(pC); cst->agregar(con("DELIMITER", ")")); ast->agregar(pA);

    auto b = bloque(); cst->agregar(b.first); ast->agregar(b.second);
    return {cst, ast};
}

// ── for-in:  for i in 1..10 {..}   |   for x in arreglo {..}  ─
Parser::Par Parser::sentForIn() {
    int ln = act().linea;
    auto cst = mkN("SentenciaForIn");
    cst->agregar(con("KEYWORD", "for"));
    Token nm = act();
    cst->agregar(con("IDENTIFIER"));
    cst->agregar(con("KEYWORD", "in"));
    auto primero = expr();
    cst->agregar(primero.first);
    auto id = mkN("Identificador", nm.lexema); id->linea = nm.linea;

    if (ver("OPERATOR", "..")) {                 // rango:  desde .. hasta
        cst->agregar(con("OPERATOR", ".."));
        auto hasta = expr();
        cst->agregar(hasta.first);
        auto ast = mkN("SentenciaForIn"); ast->linea = ln;
        auto rango = mkN("Rango", "..");
        rango->agregar(primero.second); rango->agregar(hasta.second);
        ast->agregar(id); ast->agregar(rango);
        auto b = bloque(); cst->agregar(b.first); ast->agregar(b.second);
        return {cst, ast};
    }
    // foreach sobre un arreglo:  for x in arr { ... }
    auto ast = mkN("SentenciaForEach"); ast->linea = ln;
    ast->agregar(id); ast->agregar(primero.second);
    auto b = bloque(); cst->agregar(b.first); ast->agregar(b.second);
    return {cst, ast};
}

// ── return ───────────────────────────────────────────────────
Parser::Par Parser::sentReturn() {
    auto cst = mkN("SentenciaReturn"), ast = mkN("SentenciaReturn");
    ast->linea = act().linea;
    cst->agregar(con("KEYWORD", "return"));
    if (!ver("DELIMITER", ";")) {
        auto e = expr(); cst->agregar(e.first); ast->agregar(e.second);
    }
    cst->agregar(con("DELIMITER", ";"));
    return {cst, ast};
}

// ── print / println ──────────────────────────────────────────
Parser::Par Parser::printStmt(const std::string& kw) {
    int ln = act().linea; av(); // consume print/println
    auto cst = mkN(kw == "print" ? "CST_Print" : "CST_Println");
    cst->agregar(mkN("Token_KEYWORD", kw));
    cst->agregar(con("DELIMITER", "("));
    auto ast = mkN(kw == "print" ? "Print" : "Println"); ast->linea = ln;
    if (!ver("DELIMITER", ")")) {
        auto a = expr(); cst->agregar(a.first); ast->agregar(a.second);
        while (ver("DELIMITER", ",")) {
            cst->agregar(con("DELIMITER", ","));
            auto b = expr(); cst->agregar(b.first); ast->agregar(b.second);
        }
    }
    cst->agregar(con("DELIMITER", ")"));
    cst->agregar(con("DELIMITER", ";"));
    return {cst, ast};
}

// ── Dispatcher de sentencias ─────────────────────────────────
Parser::Par Parser::sentencia() {
    Token t = act();

    if (t.tipo == "KEYWORD" && esTipoDato(t.lexema)) return decl();
    if (t.tipo == "KEYWORD" && t.lexema == "if")     return sentIf();
    if (t.tipo == "KEYWORD" && t.lexema == "while")  return sentWhile();
    if (t.tipo == "KEYWORD" && t.lexema == "return") return sentReturn();
    if (t.tipo == "KEYWORD" && (t.lexema == "print" || t.lexema == "println"))
        return printStmt(t.lexema);

    // for clásico vs for-in
    if (t.tipo == "KEYWORD" && t.lexema == "for") {
        if (sig(1).tipo == "IDENTIFIER" && sig(2).tipo == "KEYWORD" && sig(2).lexema == "in")
            return sentForIn();
        return sentFor();
    }

    // break / continue
    if (t.tipo == "KEYWORD" && (t.lexema == "break" || t.lexema == "continue")) {
        av();
        auto cst = mkN("Token_KEYWORD", t.lexema);
        cst->agregar(con("DELIMITER", ";"));
        auto ast = mkN(t.lexema == "break" ? "Break" : "Continue"); ast->linea = t.linea;
        return {cst, ast};
    }

    // bloque anónimo
    if (t.tipo == "DELIMITER" && t.lexema == "{") return bloque();

    // Empieza con identificador
    if (t.tipo == "IDENTIFIER") {
        std::string nm = t.lexema; int ln = t.linea; av();

        if (ver("DELIMITER", "(")) {                   // llamada como sentencia
            auto f = llamada(nm, ln);
            auto cst = mkN("SentenciaLlamada");
            cst->agregar(f.first); cst->agregar(con("DELIMITER", ";"));
            return {cst, f.second};
        }
        if (ver("DELIMITER", "[")) return asigIndice(nm); // arr[i] = ...
        if (ver("OPERATOR", "><")) return swapStmt(nm);  // swap

        if (ver("OPERATOR", "<<") || ver("OPERATOR", ">>")) { // streams (compat C++)
            auto cst = mkN("SentenciaStream", nm), ast = mkN("SentenciaStream", nm);
            ast->linea = ln;
            cst->agregar(mkN("Token_IDENTIFIER", nm));
            while (ver("OPERATOR", "<<") || ver("OPERATOR", ">>")) {
                std::string op = act().lexema; av();
                cst->agregar(mkN("Token_OPERATOR", op));
                auto e = expr(); cst->agregar(e.first);
                auto o = mkN("OperandoStream", op); o->agregar(e.second);
                ast->agregar(o);
            }
            cst->agregar(con("DELIMITER", ";"));
            return {cst, ast};
        }
        if (ver("OPERATOR", "=")  || ver("OPERATOR", "+=") || ver("OPERATOR", "-=") ||
            ver("OPERATOR", "*=") || ver("OPERATOR", "/="))
            return asig(nm);

        if (ver("OPERATOR", "++") || ver("OPERATOR", "--")) {
            std::string op = act().lexema; av();
            auto cst = mkN("IncrDecr", op);
            cst->agregar(mkN("Token_IDENTIFIER", nm));
            cst->agregar(mkN("Token_OPERATOR", op));
            cst->agregar(con("DELIMITER", ";"));
            auto ast = mkN("IncrementoDecremento", op); ast->linea = ln;
            auto id = mkN("Identificador", nm); id->linea = ln;
            ast->agregar(id);
            return {cst, ast};
        }

        errs.push_back("Error sintactico [linea " + std::to_string(ln) +
                       "]: Sentencia invalida con '" + nm + "'.");
        return {mkN("Error", nm), mkN("Error", nm)};
    }

    // #include ...
    if (t.tipo == "KEYWORD" && t.lexema == "#include") {
        int ln = t.linea;
        while (act().tipo != "FIN" && act().linea == ln) av();
        return {mkN("Directiva_Include"), nullptr};
    }

    // using namespace std; etc.
    if (t.tipo == "KEYWORD") {
        while (act().tipo != "FIN" && act().lexema != ";") av();
        if (ver("DELIMITER", ";")) av();
        return {mkN("Directiva", t.lexema), nullptr};
    }

    errs.push_back("Error sintactico [linea " + std::to_string(t.linea) +
                   "]: Token inesperado '" + t.lexema + "'.");
    av();
    return {mkN("Error", t.lexema), mkN("Error", t.lexema)};
}

// ── Parámetros y definición de función ───────────────────────
Parser::Par Parser::params() {
    auto cst = mkN("Parametros"), ast = mkN("Parametros");
    if (!ver("DELIMITER", ")")) {
        auto agr = [&]() {
            Token tp = act(); av();
            auto pC = mkN("Parametro");
            pC->agregar(mkN("Token_KEYWORD", tp.lexema));
            std::string tipo = tp.lexema + sufijoArray(pC);   // parámetros array
            Token nm = act();
            pC->agregar(con("IDENTIFIER"));
            cst->agregar(pC);
            auto pA = mkN("Parametro", tipo); pA->dtype = tipo; pA->linea = nm.linea;
            auto id = mkN("Identificador", nm.lexema); id->linea = nm.linea;
            pA->agregar(id);
            ast->agregar(pA);
        };
        agr();
        while (ver("DELIMITER", ",")) { cst->agregar(con("DELIMITER", ",")); agr(); }
    }
    return {cst, ast};
}

Parser::Par Parser::defFuncion() {
    Token tp = act(); av();        // tipo de retorno o 'func'
    Token nm = act();
    auto cst = mkN("DefinicionFuncion");
    cst->agregar(mkN("Token_KEYWORD", tp.lexema));
    cst->agregar(con("IDENTIFIER"));
    cst->agregar(con("DELIMITER", "("));
    auto ast = mkN("DefinicionFuncion", nm.lexema); ast->linea = nm.linea;
    ast->agregar(mkN("TipoRetorno", tp.lexema == "func" ? "void" : tp.lexema));
    auto p = params();
    cst->agregar(p.first); cst->agregar(con("DELIMITER", ")")); ast->agregar(p.second);
    auto b = bloque(); cst->agregar(b.first); ast->agregar(b.second);
    return {cst, ast};
}

// Mira adelante para distinguir  tipo IDENT (   de   tipo IDENT ; / =
bool Parser::esDefFuncion() {
    bool tipoOk = (act().tipo == "KEYWORD" && (esTipoDato(act().lexema) || act().lexema == "func"));
    if (!tipoOk) return false;
    if (sig(1).tipo != "IDENTIFIER") return false;
    return sig(2).lexema == "(";
}

// ── Punto de entrada ─────────────────────────────────────────
ResultadoSintactico Parser::parsear() {
    auto rC = mkN("Programa"), rA = mkN("Programa");
    while (act().tipo != "FIN") {
        if (esDefFuncion()) {
            auto f = defFuncion(); rC->agregar(f.first); rA->agregar(f.second);
        } else {
            auto s = sentencia(); rC->agregar(s.first); if (s.second) rA->agregar(s.second);
        }
    }
    return {rC, rA, errs};
}

} // namespace sigil
