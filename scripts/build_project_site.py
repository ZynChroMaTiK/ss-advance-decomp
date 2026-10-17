"""Assemble the static GitHub Pages site from verified local exports.

No ROM is copied. Run export_topmaps.py/package_topmaps.py and
export_site_levels.py first when the recovered inputs change.
"""
from pathlib import Path
import html,json,re,shutil,zipfile,hashlib
ROOT=Path(__file__).resolve().parents[1]
SITE=ROOT/'site'
ATLAS=ROOT/'build/exports/ssa-topmaps'

def inline(s):
    s=html.escape(s)
    def link(m):
        label,url=m.groups()
        if url.startswith(('http://','https://')): return f'<a href="{url}">{label}</a>'
        if '.md' in url:
            target=url.split('#')[0].split('/')[-1].replace('.md','.html')
            if (ROOT/'analysis'/target.replace('.html','.md')).is_file() or target=='README.html':return f'<a href="{target}">{label}</a>'
        return label+' <code>'+url+'</code>'
    s=re.sub(r'\[([^\]]+)\]\(([^)]+)\)',link,s)
    s=re.sub(r'`([^`]+)`',r'<code>\1</code>',s)
    return re.sub(r'\*\*([^*]+)\*\*',r'<strong>\1</strong>',s)

def document(path):
    content=[];code=False;para=[]
    def flush():
        if para:content.append('<p>'+inline(' '.join(para))+'</p>');para.clear()
    for line in path.read_text(encoding='utf8').splitlines():
        if line.startswith('```'):
            flush();content.append('</code></pre>' if code else '<pre><code>');code=not code
        elif code:content.append(html.escape(line)+'\n')
        elif not line.strip():flush()
        elif re.match(r'^#{1,6} ',line):
            flush();level=min(len(line.split(' ')[0])+1,6);content.append(f'<h{level}>{inline(line.split(" ",1)[1])}</h{level}>')
        elif line.startswith(('- ','* ')):
            flush();content.append('<p class="bullet">• '+inline(line[2:])+'</p>')
        else:para.append(line)
    flush()
    if code:content.append('</code></pre>')
    title=path.stem.replace('-',' ').title()
    return f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{title} — Advance Project</title><link rel="stylesheet" href="../styles.css"><style>article{{max-width:920px;margin:auto;padding:40px 24px 90px}}article p{{font-size:14px;color:#b7c7b0}}article h2{{font-size:32px;margin-top:45px}}article h3{{margin-top:30px;font-size:24px}}article a{{color:var(--lime);text-decoration:underline}}pre{{padding:20px;background:#091009;overflow:auto;font-size:12px;line-height:1.65;border:1px solid var(--border)}}code{{font-family:Consolas,monospace;font-size:.9em;overflow-wrap:anywhere}}.bullet{{margin-left:14px}}</style></head><body><header class="site-header"><a href="../#development">← Development</a><span class="eyebrow">PROJECT REFERENCE / 17 OCT 2026</span><a href="{path.stem}.md" download>Markdown ↓</a></header><article>{''.join(content)}</article></body></html>'''

def main():
    shutil.copytree(ATLAS,SITE/'atlas',dirs_exist_ok=True)
    index=(SITE/'atlas/index.html').read_text(encoding='utf8')
    embed='''<style>body{background:#0c120d}.masthead{display:none}.workspace{height:100dvh;padding:0 16px 16px;gap:12px}.frame{border:1px solid #30422a;box-shadow:none;border-radius:8px;overflow:hidden}.level-rail,.intel{background:#151e16}.map-heading{background:#182319}.map-frame{background:#0a130b}.chapter-heading{background-blend-mode:multiply}.level-link{padding-top:12px;padding-bottom:12px}.panel-tabs{background:#152016}.map-heading h2{font-family:Segoe UI,Arial,sans-serif;font-weight:500}.frame-title{padding:17px}.map-toolbar{background:#121d13}button,select{border-radius:4px}@media(max-width:1180px){.workspace{height:100dvh}}@media(max-width:680px){.workspace{height:auto;padding:0 8px 12px}.map-frame{height:580px;min-height:0}.intel{min-height:430px;max-height:480px}}</style>'''
    index=index.replace('</head>',embed+'</head>')
    index=index.replace("location.hash='level-'+(n+1);","location.hash='level-'+(n+1);if(parent!==window&&window.__ssaReady)parent.postMessage({type:'ssa:level',level:n},location.origin);")
    bridge="""<script>window.__ssaReady=true;addEventListener('message',e=>{if(e.source===parent&&e.origin===location.origin&&e.data?.type==='ssa:set-level'&&Number.isInteger(e.data.level)&&e.data.level>=0&&e.data.level<16&&e.data.level!==current)choose(e.data.level)});</script>"""
    (SITE/'atlas/index.html').write_text(index.replace('</body>',bridge+'</body>'),encoding='utf8')
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
    for path in [ROOT/'README.md',*sorted((ROOT/'analysis').glob('*.md'))]:
        (SITE/'docs'/f'{path.stem}.html').write_text(document(path),encoding='utf8')
        shutil.copy2(path,SITE/'docs'/path.name)
    from build_wiki import main as build_wiki
    build_wiki()
    (SITE/'.nojekyll').touch()
    print('Site assembled:',len(list(SITE.rglob('*'))),'paths')

if __name__=='__main__':main()
