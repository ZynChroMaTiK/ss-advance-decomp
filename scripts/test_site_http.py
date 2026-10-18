"""Serve the actual directory under a Pages-style prefix and check resources."""
import functools,http.server,json,threading,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];SITE=ROOT/'site'
class Handler(http.server.SimpleHTTPRequestHandler):
    def translate_path(self,path):
        assert path.startswith('/ss-advance-decomp/')
        return str(SITE/path[len('/ss-advance-decomp/'):].split('?')[0])
    def log_message(self,*args):pass
with http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler) as server:
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        paths=['index.html','styles.css','app.js','viewer3d.js','geometry.js','vendor/three.module.js','vendor/three.core.js','vendor/OrbitControls.js','atlas/index.html','assets/theme/native-render.png','assets/theme/hero-map.svg','docs/native-game.html','downloads/ssa-topmaps.zip']
        paths += [e['path'] for e in json.loads((SITE/'data/catalog.json').read_text())]
        paths += [str(p.relative_to(SITE)) for p in (SITE/'assets/textures').rglob('*.png')]
        paths += [str(p.relative_to(SITE)) for p in (SITE/'atlas/icons').glob('*.png')]
        paths += ['wiki.js','wiki.css','data/wiki.json']
        paths += ['graphics.js','graphics.css','data/graphics.json','assets/graphics/manifest.json']
        graphics=json.loads((SITE/'data/graphics.json').read_text(encoding='utf8'))
        paths += list(dict.fromkeys(e['images'][0]['path'] for e in graphics['entries'] if e['category']!='actors'))[:30]
        for category in graphics['categories']:
            entry = next(e for e in graphics['entries'] if e['category']==category['id'])
            paths += [image['path'] for image in entry['images']]
        paths += ['flight_controls.js','viewer_objects.js','reference.js','atlas/loader.js','atlas/viewer.js','atlas/viewer.css']
        paths += [f'atlas/{folder}/level-{i:02}.{suffix}' for folder,suffix in [('views','svg'),('data','json')] for i in range(1,17)]
        paths += ['gba-font.css','accessibility.css','reference.css','assets/fonts/serious-advance-white.woff','assets/fonts/serious-advance-green.woff']
        paths += [str(p.relative_to(SITE)) for p in (SITE/'assets/netricsa').glob('*.png')]
        for path in paths:
            with urllib.request.urlopen(f'http://127.0.0.1:{server.server_port}/ss-advance-decomp/'+path) as response:
                assert response.status==200
                assert len(response.read())==(SITE/path).stat().st_size
                if path.endswith('.js'):assert 'javascript' in response.headers['Content-Type']
        result={'status':'PASS','prefix':'/ss-advance-decomp/','http_resources':len(paths),'javascript_mime':'PASS'}
        (ROOT/'build/exports').mkdir(parents=True,exist_ok=True)
        (ROOT/'build/exports/ssa-site-http-validation.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result))
    finally:server.shutdown()
