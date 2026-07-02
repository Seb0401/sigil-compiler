// ============================================================
//  app.js - UI del compilador Sigil
//  Resaltado de sintaxis + árboles CST/AST + render de fases
// ============================================================
const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);
const esc = (s) => String(s ?? "").replace(/[&<>"]/g, (c) =>
  ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

// ── Conjuntos de palabras para el resaltado ─────────────────
const KW_TYPE = new Set(["int", "float", "bool", "char", "string", "void"]);
const KW_CTRL = new Set(["if", "else", "while", "for", "in", "return", "break",
  "continue", "func", "true", "false", "using", "namespace", "std"]);
const BUILTIN = new Set(["print", "println", "sqrt", "abs", "pow", "max", "min",
  "len", "toInt", "toFloat", "toString"]);
const OPS2 = ["<=>", "**", "..", "|>", "%%", "><", "&&", "||", "<<", ">>",
  "==", "!=", "<=", ">=", "++", "--", "+=", "-=", "*=", "/="];

// ── Resaltador (mini-lexer en JS) ───────────────────────────
function resaltar(code) {
  let i = 0, n = code.length, out = "";
  const put = (cls, txt) => { out += cls ? `<span class="${cls}">${esc(txt)}</span>` : esc(txt); };
  const isId = (c) => /[A-Za-z0-9_#]/.test(c);
  while (i < n) {
    const c = code[i];
    if (/\s/.test(c)) { let j = i; while (j < n && /\s/.test(code[j])) j++; put(null, code.slice(i, j)); i = j; continue; }
    if (c === "/" && code[i + 1] === "/") { let j = i; while (j < n && code[j] !== "\n") j++; put("tk-com", code.slice(i, j)); i = j; continue; }
    if (c === "/" && code[i + 1] === "*") { let j = i + 2; while (j < n && !(code[j] === "*" && code[j + 1] === "/")) j++; j = Math.min(n, j + 2); put("tk-com", code.slice(i, j)); i = j; continue; }
    if (c === '"') { let j = i + 1; while (j < n && code[j] !== '"' && code[j] !== "\n") { if (code[j] === "\\") j++; j++; } j = Math.min(n, j + 1); put("tk-str", code.slice(i, j)); i = j; continue; }
    if (c === "'") { let j = i + 1; while (j < n && code[j] !== "'" && code[j] !== "\n") { if (code[j] === "\\") j++; j++; } j = Math.min(n, j + 1); put("tk-str", code.slice(i, j)); i = j; continue; }
    if (/[A-Za-z_#]/.test(c)) {
      let j = i; while (j < n && isId(code[j])) j++;
      const w = code.slice(i, j);
      let cls;
      if (w[0] === "#") cls = "tk-pre";
      else if (KW_TYPE.has(w)) cls = "tk-type";
      else if (KW_CTRL.has(w)) cls = "tk-key";
      else if (BUILTIN.has(w)) cls = "tk-builtin";
      else { let k = j; while (k < n && /\s/.test(code[k])) k++; cls = code[k] === "(" ? "tk-fn" : "tk-var"; }
      put(cls, w); i = j; continue;
    }
    if (/[0-9]/.test(c)) { let j = i; while (j < n && /[0-9.]/.test(code[j])) { if (code[j] === "." && code[j + 1] === ".") break; j++; } put("tk-num", code.slice(i, j)); i = j; continue; }
    let m = OPS2.find((op) => code.startsWith(op, i));
    if (m) { put("tk-op", m); i += m.length; continue; }
    if ("=+-*/<>!&|%".includes(c)) { put("tk-op", c); i++; continue; }
    if (";(){}[],:.".includes(c)) { put("tk-punct", c); i++; continue; }
    put(null, c); i++;
  }
  return out;
}

const codeEl = $("#code");
const hlEl = $("#highlight").querySelector("code");
const gutInner = $("#gutterinner");
function actualizarGutter(v) {
  const n = (v.match(/\n/g) || []).length + 1;
  let s = "";
  for (let i = 1; i <= n; i++) s += i + "\n";
  gutInner.textContent = s;
}
function pintarEditor() {
  const v = codeEl.value;
  hlEl.innerHTML = resaltar(v) + (v.endsWith("\n") ? " " : "");
  actualizarGutter(v);
}
function syncScroll() {
  const pre = $("#highlight");
  pre.scrollTop = codeEl.scrollTop; pre.scrollLeft = codeEl.scrollLeft;
  gutInner.style.transform = `translateY(${-codeEl.scrollTop}px)`;
  const inner = $("#errinner");
  if (inner) inner.style.transform = `translate(${-codeEl.scrollLeft}px, ${-codeEl.scrollTop}px)`;
}
codeEl.addEventListener("input", () => { pintarEditor(); $("#errinner").innerHTML = ""; });
codeEl.addEventListener("scroll", syncScroll);
codeEl.addEventListener("keydown", (e) => {
  if (e.key === "Tab") {
    e.preventDefault();
    const s = codeEl.selectionStart, en = codeEl.selectionEnd;
    codeEl.value = codeEl.value.slice(0, s) + "    " + codeEl.value.slice(en);
    codeEl.selectionStart = codeEl.selectionEnd = s + 4;
    pintarEditor();
  }
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { e.preventDefault(); compilar(); }
});
function setCode(txt) { codeEl.value = txt; pintarEditor(); syncScroll(); }

// ── Pestañas ────────────────────────────────────────────────
$$(".tab").forEach((tab) => tab.addEventListener("click", () => {
  $$(".tab").forEach((t) => t.classList.remove("active"));
  $$(".panel").forEach((p) => p.classList.remove("active"));
  tab.classList.add("active");
  $("#panel-" + tab.dataset.tab).classList.add("active");
}));

// ── Ejemplos ────────────────────────────────────────────────
async function cargarEjemplos() {
  try {
    const { ejemplos } = await (await fetch("/api/examples")).json();
    const sel = $("#ejemplos");
    sel.innerHTML = '<option value="">— ejemplos —</option>';
    (ejemplos || []).forEach((n) => sel.add(new Option(n, n)));
  } catch { /* sin ejemplos */ }
}
$("#ejemplos").addEventListener("change", async (e) => {
  if (!e.target.value) return;
  const { contenido } = await (await fetch("/api/example?name=" + encodeURIComponent(e.target.value))).json();
  if (contenido != null) setCode(contenido);
});

// ── Compilar ────────────────────────────────────────────────
async function compilar() {
  const btn = $("#run"); btn.disabled = true; setStatus("Compilando…", "");
  try {
    const r = await fetch("/api/compile", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source: codeEl.value }),
    });
    const d = await r.json();
    if (d.error) { setStatus(d.error, "err"); errorGlobal(d.error); return; }
    render(d);
  } catch (e) { setStatus("No se pudo contactar al servidor: " + e.message, "err"); }
  finally { btn.disabled = false; }
}
$("#run").addEventListener("click", compilar);
function setStatus(msg, cls) { const s = $("#status"); s.textContent = msg; s.className = "status " + (cls || ""); }

// ── Render principal ────────────────────────────────────────
function render(d) {
  const nErr = (d.erroresLexicos?.length || 0) + (d.erroresSintacticos?.length || 0) +
               (d.erroresSemanticos?.length || 0) + (d.erroresEjecucion?.length || 0);
  $("#salida").textContent = d.salida || "(sin salida)";
  $("#panel-tokens").innerHTML = tablaTokens(d.tokens || []);
  $("#panel-simbolos").innerHTML = tablaSimbolos(d.tablaSimbolos || []);
  montarArbol("astHost", d.ast);
  montarArbol("cstHost", d.cst);
  $("#panel-tac").innerHTML = pintarTAC(d);
  $("#panel-errores").innerHTML = pintarErrores(d);
  marcarErrores(d);
  const badge = $("#errBadge");
  if (nErr) { badge.textContent = nErr; badge.classList.add("show"); } else badge.classList.remove("show");
  if (d.ok && nErr === 0) setStatus("✓ Compilación exitosa — programa ejecutado.", "ok");
  else setStatus(nErr + " error(es). Revisa la pestaña Errores.", "err");
}

function tablaTokens(toks) {
  if (!toks.length) return '<p class="empty">Sin tokens.</p>';
  let h = "<table><thead><tr><th>#</th><th>Lexema</th><th>Tipo</th><th>Lín</th><th>Col</th></tr></thead><tbody>";
  toks.forEach((t, i) => {
    h += `<tr><td>${i + 1}</td><td class="mono">${esc(t.lexema)}</td>` +
         `<td><span class="pill ${esc(t.tipo)}">${esc(t.tipo)}</span></td><td>${t.linea}</td><td>${t.col}</td></tr>`;
  });
  return h + "</tbody></table>";
}
function tablaSimbolos(sim) {
  if (!sim.length) return '<p class="empty">Sin símbolos declarados.</p>';
  let h = "<table><thead><tr><th>Nombre</th><th>Tipo</th><th>Categoría</th><th>Nivel (scope)</th><th>Línea</th></tr></thead><tbody>";
  sim.forEach((s) => {
    h += `<tr><td class="mono">${esc(s.nombre)}</td><td><span class="pill">${esc(s.tipo)}</span></td>` +
         `<td>${esc(s.categoria)}</td><td>${s.nivel}</td><td>${s.linea}</td></tr>`;
  });
  return h + "</tbody></table>";
}

// ── Árbol (organigrama) ─────────────────────────────────────
const OPSET = new Set(["Suma", "Resta", "Multiplicacion", "Division", "Modulo", "Divisible",
  "Potencia", "Igual", "Distinto", "Menor", "Mayor", "MenorIgual", "MayorIgual",
  "Comparacion3", "And", "Or", "NegacionUnaria", "NegacionLogica", "Swap"]);
function categoria(tipo) {
  if (tipo === "Programa") return "cat-root";
  if (tipo.startsWith("Token_")) return "cat-token";
  if (tipo === "Identificador") return "cat-id";
  if (tipo.startsWith("Numero") || tipo.startsWith("Literal")) return "cat-lit";
  if (OPSET.has(tipo)) return "cat-op";
  return "";
}
function contarDesc(n) { let c = 0; (n.hijos || []).forEach((h) => c += 1 + contarDesc(h)); return c; }

function nodoLi(n) {
  const li = document.createElement("li");
  const node = document.createElement("div");
  node.className = "node " + categoria(n.tipo);
  let label = `<span class="nt">${esc(n.tipo)}</span>`;
  if (n.valor) label += ` <span class="nv">${esc(n.valor)}</span>`;
  if (n.dtype) label += ` <span class="nd">:${esc(n.dtype)}</span>`;
  node.innerHTML = label;
  const hijos = n.hijos || [];
  if (hijos.length) {
    node.classList.add("has-children");
    const caret = document.createElement("span");
    caret.className = "caret"; caret.textContent = "▾";
    node.appendChild(caret);
    node.addEventListener("click", (e) => {
      e.stopPropagation();
      li.classList.toggle("collapsed");
      const col = li.classList.contains("collapsed");
      caret.textContent = col ? "▸" : "▾";
      let badge = node.querySelector(".count");
      if (col) { if (!badge) { badge = document.createElement("span"); badge.className = "count"; node.appendChild(badge); } badge.textContent = contarDesc(n); }
      else if (badge) badge.remove();
    });
  }
  li.appendChild(node);
  if (hijos.length) {
    const ul = document.createElement("ul");
    hijos.forEach((h) => ul.appendChild(nodoLi(h)));
    li.appendChild(ul);
  }
  return li;
}
const zoom = { ast: 1, cst: 1 };
function montarArbol(hostId, raiz) {
  const host = document.getElementById(hostId);
  host.innerHTML = "";
  if (!raiz) { host.innerHTML = '<p class="empty">Sin árbol.</p>'; return; }
  const chart = document.createElement("div");
  chart.className = "chart";
  const ul = document.createElement("ul");
  ul.appendChild(nodoLi(raiz));
  chart.appendChild(ul);
  host.appendChild(chart);
  const key = hostId === "astHost" ? "ast" : "cst";
  chart.style.transform = `scale(${zoom[key]})`;
}

// Toolbar de árboles (expandir/colapsar/zoom)
$$(".tree-toolbar .mini").forEach((b) => b.addEventListener("click", () => {
  const key = b.dataset.tree, act = b.dataset.act;
  const host = document.getElementById(key === "ast" ? "astHost" : "cstHost");
  const chart = host.querySelector(".chart");
  if (!chart) return;
  if (act === "expand" || act === "collapse") {
    host.querySelectorAll("li").forEach((li) => {
      const node = li.querySelector(".node");
      if (!node || !node.classList.contains("has-children")) return;
      const caret = node.querySelector(".caret");
      let badge = node.querySelector(".count");
      if (act === "collapse") { li.classList.add("collapsed"); if (caret) caret.textContent = "▸"; }
      else { li.classList.remove("collapsed"); if (caret) caret.textContent = "▾"; if (badge) badge.remove(); }
    });
  } else if (act === "zoomin") { zoom[key] = Math.min(2, zoom[key] + 0.15); chart.style.transform = `scale(${zoom[key]})`; }
  else if (act === "zoomout") { zoom[key] = Math.max(0.4, zoom[key] - 0.15); chart.style.transform = `scale(${zoom[key]})`; }
}));

// ── TAC (generado vs optimizado) ────────────────────────────
function pintarTAC(d) {
  const gen = d.tac || [], opt = d.tacOpt || [];
  if (!gen.length) return '<p class="empty">Sin código intermedio.</p>';
  const optSet = new Set(opt.map((s) => s.trim()).filter(Boolean));
  const fmt = (l, i, markGone) => {
    if (!l.trim()) return `<div><span class="num"></span></div>`;
    const etiq = /^(L\d+:|func |endfunc)/.test(l);
    const body = etiq ? `<span class="lbl">${esc(l)}</span>`
                      : esc(l).replace(/\b(t\d+)\b/g, '<span class="tmp">$1</span>');
    const gone = markGone && !optSet.has(l.trim());
    return `<div class="${gone ? "gone" : ""}"><span class="num">${i}</span>${body}</div>`;
  };
  const colGen = gen.map((l, i) => fmt(l, i, true)).join("");
  const colOpt = opt.map((l, i) => fmt(l, i, false)).join("");
  const red = gen.filter((x) => x.trim()).length - opt.filter((x) => x.trim()).length;
  return `<div class="taccols">
      <div class="taccol"><h4>Generado</h4><div class="tacbox"><div class="tac">${colGen}</div></div></div>
      <div class="taccol"><h4>Optimizado <span class="tag">✓</span></h4><div class="tacbox"><div class="tac">${colOpt}</div></div></div>
    </div>
    <div class="opt"><b>Optimizaciones aplicadas:</b>
      plegado de constantes: ${d.pliegues || 0} · temporales propagados: ${d.propagaciones || 0} · líneas muertas: ${d.eliminadas || 0}
      ${red > 0 ? `<br>Reducción total: <b>${red}</b> instrucción(es) menos.` : ""}
    </div>`;
}

// ── Marcado de errores en el editor ─────────────────────────
function marcarErrores(d) {
  const inner = $("#errinner"); inner.innerHTML = "";
  const cs = getComputedStyle(codeEl);
  const lh = parseFloat(cs.lineHeight) || 22;
  const padTop = parseFloat(cs.paddingTop) || 16;
  const byLine = {};
  const add = (arr) => (arr || []).forEach((e) => {
    const m = /l[ií]nea\s+(\d+)/i.exec(e); if (!m) return;
    const ln = +m[1]; (byLine[ln] = byLine[ln] || []).push(e);
  });
  add(d.erroresLexicos); add(d.erroresSintacticos); add(d.erroresSemanticos); add(d.erroresEjecucion);
  Object.keys(byLine).forEach((ln) => {
    const top = padTop + (ln - 1) * lh;
    const row = document.createElement("div");
    row.className = "eln"; row.style.top = top + "px"; row.style.height = lh + "px";
    const gut = document.createElement("div");
    gut.className = "egut"; gut.style.top = top + "px"; gut.style.height = lh + "px";
    gut.title = byLine[ln].join("\n");
    inner.appendChild(row); inner.appendChild(gut);
  });
  inner.style.transform = `translate(${-codeEl.scrollLeft}px, ${-codeEl.scrollTop}px)`;
}

// ── Errores ─────────────────────────────────────────────────
function pintarErrores(d) {
  const grupos = [
    ["Léxicos", d.erroresLexicos, "e"], ["Sintácticos", d.erroresSintacticos, "e"],
    ["Semánticos", d.erroresSemanticos, "e"], ["Advertencias", d.advertencias, "w"],
    ["Ejecución", d.erroresEjecucion, "e"],
  ];
  let h = "", total = 0;
  grupos.forEach(([t, lista, cls]) => {
    if (!lista || !lista.length) return;
    total += lista.length;
    h += `<div class="err-group"><h3>${t} (${lista.length})</h3>`;
    lista.forEach((e) => h += `<div class="err-item ${cls}">${esc(e)}</div>`);
    h += "</div>";
  });
  return total ? h : '<p class="ok-msg">✓ Sin errores. Todo en orden.</p>';
}
function errorGlobal(msg) {
  $("#panel-errores").innerHTML = `<div class="err-item e">${esc(msg)}</div>`;
  $$(".tab").forEach((t) => t.classList.remove("active"));
  $$(".panel").forEach((p) => p.classList.remove("active"));
  $('[data-tab="errores"]').classList.add("active");
  $("#panel-errores").classList.add("active");
}

// ── Código inicial ──────────────────────────────────────────
setCode(
`// Bienvenido a Sigil — pulsa "Compilar & ejecutar" (o Ctrl+Enter)
int a = 10;
int b = 5;

println("a + b   =", a + b);
println("a ** 2  =", a ** 2);
println("a %% b  =", a %% b);

a >< b;
println("tras swap: a =", a, " b =", b);

int suma = 0;
for i in 1..5 {
    suma += i;
}
println("suma 1..5 =", suma);
`);
cargarEjemplos();

// ── Modo demo (para enlaces compartibles y capturas) ────────
//   ?run=1  -> compila automáticamente al cargar
//   #ast, #cst, #tac, ...  -> abre esa pestaña
(function demo() {
  const abrirTab = () => {
    const h = location.hash.replace("#", "");
    if (h) { const b = $(`.tab[data-tab="${h}"]`); if (b) b.click(); }
  };
  if (new URLSearchParams(location.search).get("run") === "1") {
    Promise.resolve(compilar()).then(abrirTab);
  } else { abrirTab(); }
})();
