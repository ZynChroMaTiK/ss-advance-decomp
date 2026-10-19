"""Serve the actual directory under a Pages-style prefix and check resources."""
import functools,http.server,json,threading,urllib.request,urllib.error
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
        paths=['index.html','styles.css','app.js','viewer3d.js','geometry.js','vendor/three.module.js','vendor/three.core.js','vendor/OrbitControls.js','assets/theme/native-render.png','docs/native-game.html']
        paths += [e['path'] for e in json.loads((SITE/'data/catalog.json').read_text())]
        paths += [str(p.relative_to(SITE)) for p in (SITE/'assets/textures').rglob('*.png')]
        paths += [str(p.relative_to(SITE)) for p in (SITE/'assets/map-icons').glob('*.png')]
        paths += ['wiki.js','wiki.css','data/wiki.json','data/sources-and-secret-coverage.json']
        paths += ['graphics.js','graphics.css','data/graphics.json','assets/graphics/manifest.json','secret-sources.html']
        graphics=json.loads((SITE/'data/graphics.json').read_text(encoding='utf8'))
        paths += list(dict.fromkeys(e['images'][0]['path'] for e in graphics['entries'] if e['category']!='actors'))[:30]
        for category in graphics['categories']:
            entry = next(e for e in graphics['entries'] if e['category']==category['id'])
            paths += [image['path'] for image in entry['images']]
        paths += ['flight_controls.js','viewer_objects.js','reference.js']
        paths += ['gba-font.css','accessibility.css','reference.css','assets/fonts/serious-advance-white.woff','assets/fonts/serious-advance-green.woff']
        paths += [str(p.relative_to(SITE)) for p in (SITE/'assets/netricsa').glob('*.png')]
        for path in paths:
            with urllib.request.urlopen(f'http://127.0.0.1:{server.server_port}/ss-advance-decomp/'+path) as response:
                assert response.status==200
                assert len(response.read())==(SITE/path).stat().st_size
                if path.endswith('.js'):assert 'javascript' in response.headers['Content-Type']
        for path in ['atlas/index.html','atlas/viewer.js','downloads/ssa-topmaps.zip']:
            try: urllib.request.urlopen(f'http://127.0.0.1:{server.server_port}/ss-advance-decomp/'+path)
            except urllib.error.HTTPError as error: assert error.code==404
            else: raise AssertionError('Removed 2D resource is still served: '+path)
        result={'status':'PASS','prefix':'/ss-advance-decomp/','http_resources':len(paths),'javascript_mime':'PASS','removed_2d_resources':'404'}
        (ROOT/'build/exports').mkdir(parents=True,exist_ok=True)
        (ROOT/'build/exports/ssa-site-http-validation.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result))
    finally:server.shutdown()
