"""Editeur web de retouches manuelles de la ville (POC).

Lance un petit serveur local : ouvre http://127.0.0.1:8765 dans le navigateur
pour afficher la ville generee + une palette de tuiles, peindre tuile par tuile,
sauver les retouches (city/overlay.json) et regenerer les data Gamebuino.

Usage (depuis la racine du projet) :
    python3 -m tools.map_editor [--port 8765] [--no-open]

Les retouches sont un calque applique PAR-DESSUS la generation procedurale :
voir tools/overlay.py. La regeneration n'inclut PAS la compilation du firmware
(.bin) : utilise la commande arduino-cli du README quand tu veux flasher.
"""
import argparse
import io
import json
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from PIL import Image

from tools import build_city, overlay
from tools.citydsl import compile_city, CityError

TILE = 8  # px par tuile (taille native des assets)


def _load_base():
    """Compile la ville SANS overlay -> (city, names, solid_index, tile_index)."""
    tile_index, solid_index, names = build_city.load_tiles(build_city.TILESET_CSV)
    with open(build_city.CITY_TXT) as f:
        city = compile_city(f.read(), tile_index, solid_index)
    return city, names, solid_index, tile_index


def _tilesheet(names):
    """Bande horizontale PNG : une tuile 8x8 par index, dans l'ordre du CSV."""
    sheet = Image.new("RGB", (len(names) * TILE, TILE))
    for i, n in enumerate(names):
        with Image.open("%s/%s.png" % (build_city.TILES8_DIR, n)) as im:
            sheet.paste(im.convert("RGB").resize((TILE, TILE), Image.Resampling.NEAREST),
                        (i * TILE, 0))
    buf = io.BytesIO()
    sheet.save(buf, "PNG")
    return buf.getvalue()


def _state():
    """Etat complet pour le client : base proc., overlay courant, metadonnees."""
    city, names, solid_index, _ = _load_base()
    data = overlay.load()
    fp = overlay.params_fingerprint(build_city.CITY_TXT)
    return {
        "w": city.w,
        "h": city.h,
        "tile": TILE,
        "names": names,
        "solid": sorted(solid_index),
        "base": city.grid,                    # grille procedurale a plat
        "edits": data.get("edits") or {},     # {"x,y": idx}
        "params": fp,
        "stale": bool(data.get("params") and data["params"] != fp),
    }


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):  # silence
        pass

    def _send(self, code, body, ctype="application/json"):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        n = int(self.headers.get("Content-Length", 0))
        return json.loads(self.rfile.read(n) or b"{}")

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/?"):
            self._send(200, PAGE, "text/html; charset=utf-8")
        elif self.path == "/tilesheet.png":
            _, names, _, _ = _load_base()
            self._send(200, _tilesheet(names), "image/png")
        elif self.path == "/api/state":
            self._send(200, json.dumps(_state()))
        else:
            self._send(404, "not found", "text/plain")

    def do_POST(self):
        try:
            payload = self._read_json()
            edits = {str(k): int(v) for k, v in (payload.get("edits") or {}).items()}
            city, _, _, _ = _load_base()
            fp = overlay.params_fingerprint(build_city.CITY_TXT)
            overlay.save(edits, fp, (city.w, city.h))
            if self.path == "/api/save":
                self._send(200, json.dumps({"ok": True, "count": len(edits)}))
            elif self.path == "/api/regen":
                build_city.main()  # relit overlay.json -> citymap_data.cpp + .png
                self._send(200, json.dumps({"ok": True, "count": len(edits),
                                            "msg": "citymap_data.cpp + citymap.png regeneres"}))
            else:
                self._send(404, "not found", "text/plain")
        except (CityError, ValueError, KeyError) as e:
            self._send(400, json.dumps({"ok": False, "error": str(e)}))


PAGE = r"""<!doctype html><html lang=fr><head><meta charset=utf-8>
<title>Editeur de map - GTA demake</title>
<style>
  :root{--bg:#1e1e24;--panel:#2a2a32;--accent:#e0b341;--ink:#e8e8ea}
  *{box-sizing:border-box}
  body{margin:0;font:13px system-ui,sans-serif;background:var(--bg);color:var(--ink);
       display:flex;height:100vh;overflow:hidden}
  #side{width:200px;flex:0 0 200px;background:var(--panel);padding:10px;
        display:flex;flex-direction:column;gap:8px;overflow:hidden}
  #stage{flex:1;overflow:auto;position:relative;background:#111}
  h1{font-size:14px;margin:0 0 4px}
  .row{display:flex;gap:6px;align-items:center}
  button{background:var(--accent);border:0;color:#1e1e24;font-weight:600;
         padding:6px 8px;border-radius:5px;cursor:pointer}
  button.sec{background:#444;color:var(--ink)}
  #palette{flex:1;overflow:auto;display:grid;grid-template-columns:repeat(4,1fr);
           gap:3px;align-content:start;border:1px solid #000;padding:4px;border-radius:5px}
  .pt{position:relative;aspect-ratio:1;image-rendering:pixelated;cursor:pointer;
      border:2px solid transparent;background-size:cover}
  .pt.sel{border-color:var(--accent)}
  .pt .sol{position:absolute;top:0;right:0;width:5px;height:5px;background:#e04141}
  #cv{image-rendering:pixelated;display:block;cursor:crosshair}
  #status{font-size:12px;min-height:16px;color:#9fd29f}
  #status.err{color:#e08080}
  .muted{color:#8a8a92;font-size:11px}
  label.small{font-size:11px;color:#b8b8c0}
  #selname{font-weight:600;color:var(--accent)}
</style></head><body>
<div id=side>
  <h1>Editeur de map</h1>
  <div class=muted id=meta>chargement…</div>
  <div class=row><label class=small>Zoom</label>
    <input id=zoom type=range min=2 max=16 value=6 style=flex:1>
  </div>
  <div class=row><span class=small>Pinceau :</span> <span id=selname>—</span></div>
  <label class=small><input id=showedits type=checkbox checked> Afficher le liseré des retouches</label>
  <div id=palette></div>
  <div class=row><button id=save class=sec>Sauver</button>
    <button id=regen>Régénérer</button></div>
  <div class=muted>Clic gauche = peindre · clic droit = effacer · clic molette = pipette</div>
  <div id=status></div>
</div>
<div id=stage><canvas id=cv></canvas></div>
<script>
const TS=new Image(); TS.src='/tilesheet.png';
let S=null, sel=1, scale=6, edits={}, painting=0, ctx, showEdits=true;
const cv=document.getElementById('cv');
const $=id=>document.getElementById(id);

function key(x,y){return x+','+y}
function tileAt(x,y){const k=key(x,y); return (k in edits)?edits[k]:S.base[y*S.w+x];}

function drawCell(x,y){
  const t=tileAt(x,y);
  ctx.drawImage(TS, t*S.tile,0,S.tile,S.tile, x*S.tile,y*S.tile,S.tile,S.tile);
  if(showEdits && (key(x,y) in edits)){ // liseré pour reperer les retouches
    ctx.strokeStyle='rgba(224,179,65,.9)'; ctx.lineWidth=0.6;
    ctx.strokeRect(x*S.tile+0.3,y*S.tile+0.3,S.tile-0.6,S.tile-0.6);
  }
}
function redraw(){
  ctx.imageSmoothingEnabled=false;
  for(let y=0;y<S.h;y++)for(let x=0;x<S.w;x++)drawCell(x,y);
}
function applyZoom(){
  cv.style.width=(S.w*S.tile*scale)+'px';
  cv.style.height=(S.h*S.tile*scale)+'px';
}
function evCell(e){
  const r=cv.getBoundingClientRect();
  const x=Math.floor((e.clientX-r.left)/(r.width)*S.w);
  const y=Math.floor((e.clientY-r.top)/(r.height)*S.h);
  if(x<0||y<0||x>=S.w||y>=S.h)return null;
  return [x,y];
}
function paint(x,y,erase){
  const k=key(x,y);
  if(erase){ if(k in edits){delete edits[k];} }
  else{ if(S.base[y*S.w+x]===sel){delete edits[k];} else {edits[k]=sel;} }
  drawCell(x,y);
  updateMeta();
}

function selectTile(i){
  sel=i; $('selname').textContent=S.names[i];
  const p=$('palette');
  [...p.children].forEach((c,j)=>c.classList.toggle('sel',j===i));
  const el=p.children[i]; if(el)el.scrollIntoView({block:'nearest'});
}
function buildPalette(){
  const p=$('palette'); p.innerHTML='';
  const solid=new Set(S.solid);
  S.names.forEach((n,i)=>{
    const d=document.createElement('div');
    d.className='pt'+(i===sel?' sel':''); d.title=n+' ('+i+')';
    d.style.backgroundImage='url(/tilesheet.png)';
    d.style.imageRendering='pixelated';
    d.style.backgroundPosition=(i/(S.names.length-1)*100)+'% 0';
    d.style.backgroundSize=(S.names.length*100)+'% 100%';
    if(solid.has(i)){const s=document.createElement('div');s.className='sol';d.appendChild(s);}
    d.onclick=()=>selectTile(i);
    p.appendChild(d);
  });
  selectTile(sel);
}
function updateMeta(){
  $('meta').innerHTML='Ville '+S.w+'×'+S.h+' · <b>'+Object.keys(edits).length+
    '</b> retouche(s)'+(S.stale?' · <span style="color:#e08080">city.txt a changé</span>':'');
}
function setStatus(msg,err){const s=$('status');s.textContent=msg;s.className=err?'err':'';}

async function load(){
  S=await (await fetch('/api/state')).json();
  edits=Object.assign({},S.edits);
  cv.width=S.w*S.tile; cv.height=S.h*S.tile; ctx=cv.getContext('2d');
  applyZoom(); buildPalette(); updateMeta();
  if(TS.complete)redraw(); else TS.onload=redraw;
  setStatus('prêt');
}
async function post(path){
  setStatus('…');
  const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({edits})});
  const j=await r.json();
  if(j.ok)setStatus((j.msg||'sauvé')+' ('+j.count+' retouches)');
  else setStatus('erreur: '+j.error,true);
  return j;
}

cv.addEventListener('contextmenu',e=>e.preventDefault());
cv.addEventListener('mousedown',e=>{const c=evCell(e);if(!c)return;
  if(e.button===1){selectTile(tileAt(c[0],c[1])); e.preventDefault(); return;} // pipette
  painting=e.button===2?2:1; paint(c[0],c[1],painting===2); e.preventDefault();});
cv.addEventListener('mousemove',e=>{if(!painting)return;const c=evCell(e);if(c)paint(c[0],c[1],painting===2);});
window.addEventListener('mouseup',()=>painting=0);
$('zoom').addEventListener('input',e=>{scale=+e.target.value;applyZoom();});
$('showedits').addEventListener('change',e=>{showEdits=e.target.checked;redraw();});
$('save').onclick=()=>post('/api/save');
$('regen').onclick=()=>post('/api/regen');
load();
</script></body></html>"""


def main():
    ap = argparse.ArgumentParser(description="Editeur web de retouches de la ville")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-open", action="store_true", help="ne pas ouvrir le navigateur")
    args = ap.parse_args()
    url = "http://127.0.0.1:%d/" % args.port
    srv = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print("Editeur de map : %s  (Ctrl-C pour arreter)" % url)
    if not args.no_open:
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\narret.")
        srv.shutdown()


if __name__ == "__main__":
    main()
