"""Check complete gallery delivery and lossless original artwork, without a ROM."""
from pathlib import Path
from collections import Counter
import hashlib, json, re, struct, zipfile

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'site'
catalog = json.loads((SITE/'data/graphics.json').read_text(encoding='utf8'))
manifest = json.loads((SITE/'assets/graphics/manifest.json').read_text(encoding='utf8'))
entries = catalog['entries']
assert catalog['version'] == 1
assert len(entries) == catalog['entryCount'] == manifest['entries']
assert len({e['id'] for e in entries}) == len(entries)
assert all(re.fullmatch(r'[a-zA-Z0-9-]+', e['id']) for e in entries)
counts = dict(Counter(e['category'] for e in entries))
assert counts == manifest['categories'] == {c['id']:c['count'] for c in catalog['categories']}
assert set(counts) == {'actors','textures','sprites','menus','netricsa','story','splashes','fonts','sky','palettes'}
sources = {}; delivered = set()
for entry in entries:
    assert entry['title'] and entry['images']
    assert entry['collection'] and entry['collectionTitle']
    assert len(entry['images']) == (2 if entry['category'] == 'actors' else 1)
    for image in entry['images']:
        assert image['source'] not in sources, image['source']
        assert re.fullmatch(r'assets/graphics/[0-9a-f]{64}\.png', image['path'])
        name = Path(image['path']).name
        info = manifest['files'][name]
        assert [image['width'], image['height']] == [info['width'], info['height']]
        assert image['label'] and image['width'] > 0 and image['height'] > 0
        sources[image['source']] = image['path']; delivered.add(name)
assert len(sources) == catalog['imageCount'] == manifest['source_pngs']
assert len(delivered) == catalog['uniqueFiles']
assert delivered == set(manifest['files']) == {p.name for p in (SITE/'assets/graphics').glob('*.png')}
assert {p.suffix for p in (SITE/'assets/graphics').iterdir()} == {'.json','.png'}
for name, info in manifest['files'].items():
    raw = (SITE/'assets/graphics'/name).read_bytes()
    assert len(raw) == info['bytes']
    assert hashlib.sha256(raw).hexdigest() == info['sha256'] == name[:-4]
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', raw[16:24]) == (info['width'], info['height'])
archive_path = ROOT/'build/exports/ssa-graphics.zip'
verified_originals = False
if archive_path.exists():
    with zipfile.ZipFile(archive_path) as archive:
        assert set(sources) == {p for p in archive.namelist() if p.endswith('.png')}
        assert hashlib.sha256(archive.read('manifest.json')).hexdigest() == manifest['source_manifest_sha256']
        for source, path in sources.items():
            assert archive.read(source) == (SITE/path).read_bytes(), source
    verified_originals = True
print(json.dumps({'status':'PASS', 'entries':len(entries), 'original_pngs':len(sources),
    'unique_files':len(delivered), 'compared_with_original_export':verified_originals}))
