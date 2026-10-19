"""Check website content, reference links and the 3D-only map browser without a ROM."""
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlsplit,unquote
import hashlib,json,re
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
            route=target==SITE/'index.html' and url.fragment.split('/')[0] in ('project','features','levels','wiki','graphics','development')
            assert route or unquote(url.fragment) in pages[target].ids,f'Missing anchor: {path}: {ref}'
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
for p in (SITE/'data').glob('level-*.json'):
    assert '200 cap' not in p.read_text()
    for marker in json.loads(p.read_text())['markers']:
        if marker.get('sprite'):
            assert (SITE/'assets/map-icons'/marker['sprite'].removeprefix('icons/')).is_file()
assert not (SITE/'atlas').exists(), 'The 2D viewer must not be deployed'
assert not (SITE/'downloads/ssa-topmaps.zip').exists(), 'The offline 2D download must not be deployed'
assert not any(token in index for token in ('atlas-frame','data-mode=','/2d','ssa-topmaps.zip'))
assert 'postMessage' not in (SITE/'app.js').read_text(), 'No obsolete iframe bridge'
secrets=[e for e in entries if e['category']=='secrets']
assert len(secrets)==5
assert all(any(s['title']=='Walkthrough' and s['href'].startswith('https://') for s in e['sources']) for e in secrets)
for name,info in json.loads((SITE/'assets/fonts/manifest.json').read_text())['files'].items():
    assert hashlib.sha256((SITE/'assets/fonts'/name).read_bytes()).hexdigest()==info['sha256']
print(json.dumps({'status':'PASS','html_pages':len(pages),'local_links_and_fragments':checked,'references':sum(p.parent==SITE/'docs' for p in pages),'tables':sum(p.tables for p in pages.values()),'native_support_boundary':limit,'wiki_entries':len(entries),'maps':'3D assets available; 2D viewer and download absent','font_hashes':'PASS'}))
