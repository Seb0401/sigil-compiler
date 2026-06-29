#!/usr/bin/env python3
# ============================================================
#  server.py - Servidor local para la UI web de Sigil
#
#  - Sirve los archivos de  web/
#  - Expone  POST /api/compile  que invoca el binario  sigilc
#    y devuelve su salida JSON.
#  - Expone  GET  /api/examples  con la lista de ejemplos
#    y  GET /api/example?name=...  con el contenido.
#
#  Uso:   python server/server.py        ->  http://localhost:8000
#  Solo usa la biblioteca estandar de Python (no requiere pip).
# ============================================================
import json
import os
import subprocess
import sys
import tempfile
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
EXAMPLES = os.path.join(ROOT, "examples")
PORT = int(os.environ.get("SIGIL_PORT", "8000"))


def encontrar_binario():
    """Busca el ejecutable sigilc en las ubicaciones tipicas de build."""
    nombres = ["sigilc.exe", "sigilc"]
    carpetas = [
        os.path.join(ROOT, "bin"),
        os.path.join(ROOT, "build"),
        os.path.join(ROOT, "build", "Debug"),
        os.path.join(ROOT, "build", "Release"),
        os.path.join(ROOT, "out", "build", "x64-Debug"),
        os.path.join(ROOT, "out", "build", "x64-Release"),
        ROOT,
    ]
    for c in carpetas:
        for n in nombres:
            p = os.path.join(c, n)
            if os.path.isfile(p):
                return p
    return None


def compilar(fuente: str) -> dict:
    binario = encontrar_binario()
    if not binario:
        return {
            "error": "No se encontro el binario 'sigilc'. Compilalo primero:\n"
                     "  cmake -B build && cmake --build build\n"
                     "o abre la carpeta en Visual Studio y compila el objetivo sigilc."
        }
    # Escribe el codigo a un archivo temporal .sg
    tmp = tempfile.NamedTemporaryFile("w", suffix=".sg", delete=False, encoding="utf-8")
    try:
        tmp.write(fuente)
        tmp.close()
        proc = subprocess.run(
            [binario, tmp.name, "--json"],
            capture_output=True, text=True, timeout=20
        )
        salida = proc.stdout.strip()
        if not salida:
            return {"error": "El compilador no devolvio datos.\n" + proc.stderr}
        try:
            return json.loads(salida)
        except json.JSONDecodeError as e:
            return {"error": "Salida JSON invalida del compilador: %s\n%s" % (e, salida[:500])}
    except subprocess.TimeoutExpired:
        return {"error": "Tiempo de compilacion/ejecucion agotado (posible bucle infinito)."}
    finally:
        try:
            os.unlink(tmp.name)
        except OSError:
            pass


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # silencioso

    def _json(self, obj, code=200):
        cuerpo = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(cuerpo)))
        self.end_headers()
        self.wfile.write(cuerpo)

    def _archivo(self, ruta, ctype):
        try:
            with open(ruta, "rb") as f:
                datos = f.read()
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(datos)))
            self.end_headers()
            self.wfile.write(datos)
        except OSError:
            self.send_error(404, "No encontrado")

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        ruta = url.path

        if ruta == "/" or ruta == "":
            return self._archivo(os.path.join(WEB, "index.html"), "text/html; charset=utf-8")
        if ruta == "/style.css":
            return self._archivo(os.path.join(WEB, "style.css"), "text/css; charset=utf-8")
        if ruta == "/app.js":
            return self._archivo(os.path.join(WEB, "app.js"), "application/javascript; charset=utf-8")

        if ruta == "/api/examples":
            try:
                files = sorted(f for f in os.listdir(EXAMPLES) if f.endswith(".sg"))
            except OSError:
                files = []
            return self._json({"ejemplos": files})

        if ruta == "/api/example":
            q = urllib.parse.parse_qs(url.query)
            nombre = (q.get("name") or [""])[0]
            # evita salir de la carpeta de ejemplos
            nombre = os.path.basename(nombre)
            p = os.path.join(EXAMPLES, nombre)
            if nombre.endswith(".sg") and os.path.isfile(p):
                with open(p, encoding="utf-8") as f:
                    return self._json({"contenido": f.read()})
            return self._json({"error": "Ejemplo no encontrado"}, 404)

        self.send_error(404, "No encontrado")

    def do_POST(self):
        url = urllib.parse.urlparse(self.path)
        if url.path != "/api/compile":
            return self.send_error(404, "No encontrado")
        n = int(self.headers.get("Content-Length", "0"))
        cuerpo = self.rfile.read(n).decode("utf-8") if n else "{}"
        try:
            fuente = json.loads(cuerpo).get("source", "")
        except json.JSONDecodeError:
            fuente = ""
        return self._json(compilar(fuente))


def main():
    binario = encontrar_binario()
    print("=" * 52)
    print("  Servidor Sigil")
    print("=" * 52)
    print("  Web:     http://localhost:%d" % PORT)
    print("  Binario: %s" % (binario or "NO ENCONTRADO (compila sigilc primero)"))
    print("  Ctrl+C para detener.")
    print("=" * 52)
    srv = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor detenido.")
        srv.server_close()


if __name__ == "__main__":
    main()
