"""Split the online atlas into a small shell, per-level data and shared assets."""
from pathlib import Path
import json,re,base64,hashlib
ROOT=Path(__file__).resolve().parents[1]
def extract_data(text):
    start=text.index('const atlas=')+len('const atlas=')
    return json.loads(text[start:text.index(';\nconst $=',start)])
def write_online_atlas(folder, offline):
    data=extract_data(offline)
    assets={hashlib.sha256(p.read_bytes()).hexdigest():p.relative_to(folder).as_posix() for p in (folder/'icons').glob('*.png')}
    def shared(match):
        raw=base64.b64decode(match[1]);path=assets.get(hashlib.sha256(raw).hexdigest())
        assert path,'Unknown embedded sprite'
        return 'href="'+path+'"'
    (folder/'views').mkdir(exist_ok=True)
    for n,svg in enumerate(data['svgs']):
        svg=re.sub(r'href="data:image/png;base64,([^"]+)"',shared,svg)
        (folder/'views'/f'level-{n+1:02}.svg').write_text(svg,encoding='utf8')
    manifest=json.loads((folder/'theme-manifest.json').read_text())
    theme={key:value['path'] for key,value in manifest.items()}
    theme['font']={'character_to_glyph':data['theme']['font']['character_to_glyph'],'glyphs':[{'metric_bytes':g['metric_bytes']} for g in data['theme']['font']['glyphs']]}
    meta={'maps':[{'level':m['level'],'title':m['title']} for m in data['maps']],'theme':theme,'svgs':[]}
    script=(ROOT/'scripts/topmap_viewer.js').read_text(encoding='utf8').replace('__DATA__',json.dumps(meta,separators=(',',':'),ensure_ascii=False))
    script="import {createLevelLoader} from './loader.js';\n"+script.replace('const $=id=>',"atlas.loadLevel=createLevelLoader();\ndocument.documentElement.classList.toggle('embedded',parent!==window);\nconst $=id=>")
    script+="\nwindow.__ssaReady=true;addEventListener('message',e=>{if(e.source===parent&&e.origin===location.origin&&e.data?.type==='ssa:set-level'&&Number.isInteger(e.data.level)&&e.data.level>=0&&e.data.level<16&&e.data.level!==requested)choose(e.data.level)});\n"
    script=script.replace("location.hash='level-'+(n+1);","location.hash='level-'+(n+1);if(parent!==window&&window.__ssaReady)parent.postMessage({type:'ssa:level',level:n},location.origin);")
    (folder/'viewer.js').write_text(script,encoding='utf8')
    (folder/'loader.js').write_text((ROOT/'scripts/topmap_loader.mjs').read_text(),encoding='utf8')
    template=(ROOT/'scripts/topmap_viewer.html').read_text(encoding='utf8')
    template=template.replace('<style>__STYLE__</style>','<link rel="stylesheet" href="viewer.css">').replace('<script>__SCRIPT__</script>','<script type="module" src="viewer.js"></script>')
    template=template.replace('It runs offline; no external fonts or services are required.','The online viewer loads only selected levels. Download the ZIP for fully offline use; no external fonts or services are required.')
    (folder/'index.html').write_text(template,encoding='utf8')
    css=(ROOT/'scripts/topmap_viewer.css').read_text(encoding='utf8')+'\n'+(ROOT/'scripts/topmap_embed.css').read_text(encoding='utf8')
    (folder/'viewer.css').write_text(css,encoding='utf8')
