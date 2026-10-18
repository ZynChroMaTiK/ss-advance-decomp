"""Assemble the static GitHub Pages site from verified local exports.

No ROM is copied. Run export_topmaps.py/package_topmaps.py and
export_site_levels.py first when the recovered inputs change.
"""
from pathlib import Path
import json,re,shutil,hashlib
ROOT=Path(__file__).resolve().parents[1]
SITE=ROOT/'site'
ATLAS=ROOT/'build/exports/ssa-topmaps'

def document(path):
    from site_documents import render_document
    available={'README.md','DEVELOPMENT.md'}|{'analysis/'+p.name for p in (ROOT/'analysis').glob('*.md')}
    return render_document(path.read_text(encoding='utf8'),path.relative_to(ROOT).as_posix(),available)


def embed_atlas(index):
    embed='''<style>body{background:#0c120d}.masthead{display:none}.workspace{height:100dvh;padding:0 16px 16px;gap:12px}.frame{border:1px solid #30422a;box-shadow:none;border-radius:8px;overflow:hidden}.level-rail,.intel{background:#151e16}.map-heading{background:#182319}.map-frame{background:#0a130b}.chapter-heading{background-blend-mode:multiply}.level-link{padding-top:12px;padding-bottom:12px}.panel-tabs{background:#152016}.map-heading h2{font-family:Segoe UI,Arial,sans-serif;font-weight:500}.frame-title{padding:17px}.map-toolbar{background:#121d13}button,select{border-radius:4px}@media(max-width:1180px){.workspace{height:100dvh}}@media(max-width:680px){.workspace{height:auto;padding:0 8px 12px}.map-frame{height:580px;min-height:0}.intel{min-height:430px;max-height:480px}}</style>'''
    index=index.replace('</head>',embed+'</head>')
    index=index.replace("location.hash='level-'+(n+1);","location.hash='level-'+(n+1);if(parent!==window&&window.__ssaReady)parent.postMessage({type:'ssa:level',level:n},location.origin);")
    bridge="""<script>window.__ssaReady=true;addEventListener('message',e=>{if(e.source===parent&&e.origin===location.origin&&e.data?.type==='ssa:set-level'&&Number.isInteger(e.data.level)&&e.data.level>=0&&e.data.level<16&&e.data.level!==current)choose(e.data.level)});</script>"""
    return index.replace('</body>',bridge+'</body>')

def main():
    shutil.copytree(ATLAS,SITE/'atlas',dirs_exist_ok=True)
    (SITE/'atlas/index.html').write_text(embed_atlas((SITE/'atlas/index.html').read_text(encoding='utf8')),encoding='utf8')
    hashes=json.loads((SITE/'atlas/manifest-sha256.json').read_text())
    hashes['index.html']=hashlib.sha256((SITE/'atlas/index.html').read_bytes()).hexdigest()
    (SITE/'atlas/manifest-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
    shutil.copytree(ATLAS/'theme',SITE/'assets/theme',dirs_exist_ok=True)
    hero=re.sub(r'viewBox="[^"]+"','viewBox="65 340 1630 1230"',(ATLAS/'svg/level-01.svg').read_text(encoding='utf8'),count=1)
    hero=re.sub(r'width="\d+" height="\d+"','width="1630" height="1230"',hero,count=1)
    for old,new in {'#101822':'#101b12','#334555':'#2c422d','#26323f':'#172719','#172330':'#152418','#23303e':'#203523','#c5d3e2':'#abc294','#6b8196':'#52734b','#dfe7f2':'#dfebd3','#5ed8d0':'#c0ef78'}.items():hero=hero.replace(old,new)
    (SITE/'assets/theme/hero-map.svg').write_text(hero,encoding='utf8')
    page=SITE/'index.html'
    page.write_text(page.read_text(encoding='utf8').replace('src="atlas/png/level-01.png"','src="assets/theme/hero-map.svg"'),encoding='utf8')
    (SITE/'downloads').mkdir(exist_ok=True)
    shutil.copy2(ROOT/'build/exports/ssa-topmaps.zip',SITE/'downloads/ssa-topmaps.zip')
    manifest=json.loads((SITE/'assets/textures/manifest.json').read_text())
    (SITE/'data/export-validation.json').write_text(json.dumps(manifest['validation'],indent=2)+'\n')
    (SITE/'docs').mkdir(exist_ok=True)
    for path in [ROOT/'README.md',ROOT/'DEVELOPMENT.md',*sorted((ROOT/'analysis').glob('*.md'))]:
        (SITE/'docs'/f'{path.stem}.html').write_text(document(path),encoding='utf8')
        shutil.copy2(path,SITE/'docs'/path.name)
    from build_wiki import main as build_wiki
    build_wiki()
    from refresh_atlas_ui import main as refresh_atlas_ui
    refresh_atlas_ui()
    (SITE/'.nojekyll').touch()
    print('Site assembled:',len(list(SITE.rglob('*'))),'paths')

if __name__=='__main__':main()
