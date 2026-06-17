// ============================================================
//  parser.hpp — Analizador sintáctico descendente recursivo
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <utility>
#include "token.hpp"
#include "ast.hpp"

namespace sigil {

// Resultado: raíz del CST, raíz del AST y errores sintácticos.
struct ResultadoSintactico {
    Nodo cst;
    Nodo ast;
    std::vector<std::string> errores;
};

class Parser {
    std::vector<Token> toks;
    int pos = 0;
    std::vector<std::string> errs;

    using Par = std::pair<Nodo, Nodo>;

    // Utilidades de cursor
    Token act() const;
    Token sig(int k = 1) const;
    void  av();
    bool  ver(const std::string& tp, const std::string& lx = "") const;
    Nodo  con(const std::string& tp, const std::string& lx = ""); // consume o error

    // Nombre semántico para un operador
    static std::string nombreOp(const std::string& op);
    Par mkBin(const std::string& op, Par L, Par R, int linea);

    // Expresiones (de menor a mayor precedencia)
    Par expr();        // = exprOr
    Par exprOr();
    Par exprAnd();
    Par exprCmp();
    Par exprSuma();
    Par exprTerm();
    Par exprPot();
    Par exprPipe();
    Par exprUnary();
    Par factor();
    Par llamada(const std::string& nom, int lin);

    // Sentencias
    Par sentencia();
    Par bloque();
    Par decl();
    Par declSinPunto();
    Par asig(const std::string& nom);
    Par asigSinPunto(const std::string& nom);
    Par asigIndice(const std::string& nom);
    std::string sufijoArray(const Nodo& cst); // consume '[' ']' -> "[]"
    Par swapStmt(const std::string& nom);
    Par sentIf();
    Par sentWhile();
    Par sentFor();
    Par sentForIn();
    Par sentReturn();
    Par printStmt(const std::string& kw);

    // Funciones
    Par params();
    Par defFuncion();
    bool esDefFuncion();

public:
    explicit Parser(std::vector<Token> t) : toks(std::move(t)) {}
    ResultadoSintactico parsear();
};

} // namespace sigil
