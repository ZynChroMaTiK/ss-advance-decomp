"""Pin the viewer runtime locally: no browser CDN dependency."""
import hashlib,json,urllib.request,sys
from pathlib import Path
p=Path('site/vendor'); p.mkdir(parents=True,exist_ok=True)
files={'three.module.js':'build/three.module.js','three.core.js':'build/three.core.js','OrbitControls.js':'examples/jsm/controls/OrbitControls.js','LICENSE':'LICENSE'}
manifest={'project':'Three.js','version':'0.180.0','license':'MIT','files':{}}
for name,source in files.items():
    url='https://raw.githubusercontent.com/mrdoob/three.js/r180/'+source
    if '--local' in sys.argv:
        data=(p/name).read_bytes()
    else:
        with urllib.request.urlopen(url,timeout=45) as response:
            chunks=[]
            while chunk:=response.read(32768):chunks.append(chunk)
            data=b''.join(chunks)
    if name=='OrbitControls.js': data=data.replace(b"from 'three'",b"from './three.module.js'")
    if not data:raise ValueError('Empty dependency: '+name)
    (p/name).write_bytes(data)
    manifest['files'][name]={'url':url,'sha256':hashlib.sha256(data).hexdigest()}
    print(name,len(data),flush=True)
(p/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
