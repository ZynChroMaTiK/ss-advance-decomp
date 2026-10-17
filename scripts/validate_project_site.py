"""Validate the actual viewer geometry helpers against all sixteen exports."""
from pathlib import Path
import hashlib,json,math,re,sys,zipfile
from html.parser import HTMLParser
ROOT=Path(__file__).resolve().parents[1]; SITE=ROOT/'site'
sys.path.insert(0,str(ROOT/'build/topmap-validation-tools'))
import quickjs,tinycss2

class Page(HTMLParser):
    def __init__(self):super().__init__();self.refs=[];self.ids=[];self.scripts=[];self.script=False
    def handle_starttag(self,tag,attrs):
        a=dict(attrs)
        if 'id' in a:self.ids.append(a['id'])
        for k in ('src','href'):
            if a.get(k):self.refs.append(a[k])
        if tag=='script' and not a.get('src'):self.script=True
    def handle_endtag(self,tag):
        if tag=='script':self.script=False
    def handle_data(self,data):
        if self.script:self.scripts.append(data)

def main():
    context=quickjs.Context();errors=[];checks=0
    for path in SITE.rglob('*.html'):
        p=Page();p.feed(path.read_text(encoding='utf8'))
        assert len(p.ids)==len(set(p.ids)),f'Duplicate ID: {path}'
        for ref in p.refs:
            if ref.startswith(('http:','https:','data:','#','mailto:')):continue
            target=(path.parent/ref.split('#')[0].split('?')[0]).resolve()
            assert target.is_relative_to(SITE.resolve()) and target.exists(),f'Missing link {path}: {ref}'
            checks+=1
        for script in p.scripts:context.eval('new Function('+json.dumps(script)+')');checks+=1
    page=Page();page.feed((SITE/'index.html').read_text())
    for filename in ['app.js','viewer3d.js','geometry.js','wiki.js']:
        source=(SITE/filename).read_text();plain=re.sub(r'^import .*?;\s*','',source,flags=re.M);plain=plain.replace('export function ','function ')
        context.eval('new Function('+json.dumps('return (async()=>{'+plain+'})')+')')
        for dom_id in re.findall(r"\$\('([^']+)'\)",source):assert dom_id in page.ids,dom_id
        for ref in re.findall(r"from '([^']+)'",source):assert (SITE/ref).is_file(),ref
    for css in [SITE/'styles.css',SITE/'wiki.css']:
        rules=tinycss2.parse_stylesheet(css.read_text(),skip_comments=True,skip_whitespace=True)
        assert not [r for r in rules if r.type=='error']
    context.eval((SITE/'geometry.js').read_text().replace('export function ','function '))
    totals={'levels':0,'floor_triangles':0,'wall_triangles':0,'markers':0,'secrets':0}
    for entry in json.loads((SITE/'data/catalog.json').read_text()):
        d=json.loads((SITE/entry['path']).read_text()); totals['levels']+=1;totals['markers']+=len(d['markers']);totals['secrets']+=len(d['secrets'])
        context.set('model',json.dumps(d));result=json.loads(context.eval('''JSON.stringify((()=>{const d=JSON.parse(model);let floor=0,walls=0;function check(p,idx){if(!p.every(Number.isFinite))throw Error('Nonfinite mesh');for(let j=0;j<idx.length;j+=3){const a=idx[j]*3,b=idx[j+1]*3,c=idx[j+2]*3,u=[0,1,2].map(k=>p[b+k]-p[a+k]),v=[0,1,2].map(k=>p[c+k]-p[a+k]);const cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]];if(Math.hypot(...cross)<1e-9)throw Error('Degenerate triangle')}}for(const s of d.sectors){const p=floorPositions(s.mesh_loops,s.initial_floor);check(p,Array.from({length:p.length/3},(_,i)=>i));floor+=p.length/9}for(const state of ['initial','open']){const b=wallBuffers(d.walls[state]);check(b.positions,b.indices);if(b.uvs.length!==b.positions.length/3*2)throw Error('UV mismatch');walls+=b.indices.length/3}return {floor,walls}})())'''))
        totals['floor_triangles']+=result['floor'];totals['wall_triangles']+=result['walls']
        for m in d['markers']:
            if m.get('sprite'):assert (SITE/'atlas'/m['sprite']).is_file()
        for f in d['walls']['initial']+d['walls']['open']:
            if f['texture']:assert (SITE/f['texture']).is_file()
    assert totals['levels']==16 and totals['markers']==1541 and totals['secrets']==5
    for name,digest in json.loads((SITE/'atlas/manifest-sha256.json').read_text()).items():assert hashlib.sha256((SITE/'atlas'/name).read_bytes()).hexdigest()==digest,name
    for name,info in json.loads((SITE/'vendor/manifest.json').read_text())['files'].items():assert hashlib.sha256((SITE/'vendor'/name).read_bytes()).hexdigest()==info['sha256']
    assert "from './three.module.js'" in (SITE/'vendor/OrbitControls.js').read_text()
    for p in SITE.rglob('*'):
        assert p.suffix.lower() not in ('.gba','.exe','.dll','.sav'),p
        if p.is_file():assert p.stat().st_size<100*1024*1024,p
    assert hashlib.sha1((ROOT/'orig/SSA.gba').read_bytes()).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
    http=json.loads((ROOT/'build/exports/ssa-site-http-validation.json').read_text())
    result={'status':'PASS','checks':{'html_links_and_inline_scripts':checks,'viewer_javascript':'syntax and DOM bindings','geometry_helpers':'executed against all exported levels','css':'parsed','vendor':'pinned SHA256 verified','embedded_atlas_hashes':'PASS','rom':'unchanged'},**totals,'http':http,'browser_webgl_visual_qa':'unavailable: no browser connected'}
    (SITE/'data/site-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    archive=ROOT/'build/exports/ssa-project-site.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(SITE.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(ROOT).as_posix())
        z.write(ROOT/'.github/workflows/pages.yml','.github/workflows/pages.yml')
        for name in ['build_project_site.py','build_wiki.py','wiki_entities.json','export_site_levels.py','fetch_site_vendor.py','validate_project_site.py','test_site.mjs','test_site_routes.mjs','test_site_wiki.mjs','test_site_http.py']:z.write(ROOT/'scripts'/name,'scripts/'+name)
        assert z.testzip() is None
    result['archive_sha256']=hashlib.sha256(archive.read_bytes()).hexdigest();result['archive_bytes']=archive.stat().st_size
    (ROOT/'build/exports/ssa-project-site-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
