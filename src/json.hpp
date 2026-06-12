// ============================================================
//  json.hpp — Utilidades mínimas para emitir JSON (sin libs)
// ============================================================
#pragma once
#include <string>
#include <sstream>
#include <cstdio>

namespace sigil {

// Escapa una cadena para incrustarla en JSON de forma segura.
inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// Devuelve "campo" : "valor"  (valor escapado entre comillas)
inline std::string jstr(const std::string& key, const std::string& val) {
    return "\"" + key + "\":\"" + jsonEscape(val) + "\"";
}

} // namespace sigil
