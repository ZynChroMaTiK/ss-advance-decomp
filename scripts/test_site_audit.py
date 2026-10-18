"""Check website content, reference links and the downloadable atlas without a ROM."""
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlsplit,unquote
import base64,hashlib,json,re,zipfile
from build_online_atlas import extract_data
ROOT=Path(__file__).resolve().parents[1];SITE=ROOT/'site'
class Page(HTMLParser):
    def __init__(self,text):
        super().__init__();self.ids=[];self.refs=[];self.h1=0;self.tables=0;self.images=0;self.feed(text)
    def handle_starttag(self,tag,attrs):
        a=dict(attrs)
        if 'id' in a:self.ids.append(a['id'])
        self.h1+=tag=='h1';self.tables+=tag=='table'
        if tag=='img':assert 'alt' in a;self.images+=1
        for key in ('src','href'):
            if a.get(key):self.refs.append(a[key])
pages={p:Page(p.read_text(encoding='utf8')) for p in SITE.rglob('*.html')}
checked=0
for path,page in pages.items():
    assert len(page.ids)==len(set(page.ids)),f'Duplicate IDs: {path}'
    for ref in page.refs:
        url=urlsplit(ref)
        if url.scheme or url.netloc:continue
        target=(path.parent/unquote(url.path)).resolve() if url.path else path
        assert target.exists(),f'Missing resource: {path}: {ref}'
        if url.fragment and target in pages:
            route=target==SITE/'index.html' and url.fragment.split('/')[0] in ('project','features','levels','wiki','development')
            atlas=target==SITE/'atlas/index.html' and re.fullmatch(r'level-\d+',url.fragment)
            assert route or atlas or unquote(url.fragment) in pages[target].ids,f'Missing anchor: {path}: {ref}'
        checked+=1
for path,page in pages.items():
    if path.parent==SITE/'docs':assert page.h1==1,f'Reference needs one H1: {path}'
assert pages[SITE/'docs/README.html'].tables>0,'Markdown tables must stay structured'
readme=(SITE/'docs/README.html').read_text()
assert 'https://github.com/ZynChroMaTiK/ss-advance-decomp/blob/main/recovered/snapshot/README.md' in readme
index=(SITE/'index.html').read_text(encoding='utf8');limit=int(re.search(r'data-native-campaign-levels="(\d+)"',index)[1])
for name,text in [('homepage',index),('player guide',readme),('ROM instructions',(ROOT/'orig/README.md').read_text(encoding='utf8'))]:
    assert 'European' in text and 'ROM' in text,f'{name} must explain the required ROM region'
assert f'<strong>{limit}<span>/12</span>' in index
entries=json.loads((SITE/'data/wiki.json').read_text())['entries']
for entry in entries:
    if entry['category']=='levels':
        availability=next(s['text'] for s in entry['sections'] if s['title']=='Can I play it?')
        assert availability.startswith('Playable in')==(entry['level']<limit),entry['id']
    if entry['category']=='weapons':assert any(f[0]=='Maps with this pickup' for f in entry['facts'])
health=next(e for e in entries if e['id']=='items-health')
assert any('199 becomes 299' in s['text'] for s in health['sections'])
for folder in ['data','atlas/data']:
    for p in (SITE/folder).glob('level-*.json'):assert '200 cap' not in p.read_text()
for name,digest in json.loads((SITE/'atlas/manifest-sha256.json').read_text()).items():
    assert hashlib.sha256((SITE/'atlas'/name).read_bytes()).hexdigest()==digest,name
with zipfile.ZipFile(SITE/'downloads/ssa-topmaps.zip') as z:
    assert z.testzip() is None
    for name,digest in json.loads(z.read('manifest-sha256.json')).items():assert hashlib.sha256(z.read(name)).hexdigest()==digest,name
    for folder in ['data','svg','png']:
        for i in range(1,17):
            suffix={'data':'json','svg':'svg','png':'png'}[folder];name=f'{folder}/level-{i:02}.{suffix}'
            assert z.read(name)==(SITE/'atlas'/name).read_bytes(),name
    assert b'collect below 200' in z.read('index.html')
    assert b"$('map').onkeydown" in z.read('index.html')
    offline=extract_data(z.read('index.html').decode('utf8'))
    for i,original in enumerate(offline['svgs'],1):
        online=(SITE/f'atlas/views/level-{i:02}.svg').read_text()
        def embed(match):
            return 'href="data:image/png;base64,'+base64.b64encode((SITE/'atlas'/match[1]).read_bytes()).decode()+'"'
        restored=re.sub(r'href="(icons/[^"]+)"',embed,online)
        assert restored==original,f'Online map {i} lost artwork or records'
assert (SITE/'atlas/index.html').stat().st_size<12000,'Online shell must not embed every map'
assert (SITE/'atlas/loader.js').read_bytes()==(ROOT/'scripts/topmap_loader.mjs').read_bytes()
secrets=[e for e in entries if e['category']=='secrets']
assert len(secrets)==5
assert all(any(s['title']=='Walkthrough' and s['href'].startswith('https://') for s in e['sources']) for e in secrets)
for name,info in json.loads((SITE/'assets/fonts/manifest.json').read_text())['files'].items():
    assert hashlib.sha256((SITE/'assets/fonts'/name).read_bytes()).hexdigest()==info['sha256']
print(json.dumps({'status':'PASS','html_pages':len(pages),'local_links_and_fragments':checked,'references':sum(p.parent==SITE/'docs' for p in pages),'tables':sum(p.tables for p in pages.values()),'native_support_boundary':limit,'wiki_entries':len(entries),'offline_atlas':'CRC, hashes and all 48 map files agree','font_hashes':'PASS'}))
