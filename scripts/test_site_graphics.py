"""Check unique pictures, content grouping and unchanged original artwork."""
from pathlib import Path
from collections import Counter
import hashlib, json, re, struct, zipfile
from graphics_content import pixel_digest, is_sheet
from export_graphics_archive import png

# Same visible picture, different unused/transparent palette colours.
a = png(2,1,b'\0\1',[0,31,123],True)
b = png(2,1,b'\0\1',[32767,31,456],True)
assert a != b and pixel_digest(a) == pixel_digest(b)
assert pixel_digest(a) != pixel_digest(png(2,1,b'\0\1',[0,32,123],True))
assert pixel_digest(a) != pixel_digest(png(2,1,b'\0\1',[0,31,123],False))
assert pixel_digest(a) != pixel_digest(png(1,2,b'\0\1',[0,31,123],True))

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
sources = manifest['sources']; delivered = set(); pairs = set()
by_id = {e['id']:e for e in entries}
for alias, target in catalog['aliases'].items():
    assert alias not in by_id and target in by_id
    by_id[alias] = by_id[target]
assert all(not is_sheet(p) for p in sources)
assert manifest['excluded'] and all(is_sheet(p) for p in manifest['excluded'])
assert not set(sources).intersection(manifest['excluded'])
assert len({info['pixels_sha256'] for info in manifest['files'].values()}) == len(manifest['files'])
for entry in entries:
    assert entry['title'] and entry['images']
    assert entry['collection'] and entry['collectionTitle']
    assert len(entry['images']) == (2 if entry['category'] == 'actors' else 1)
    pair = tuple(dict.fromkeys(i['path'] for i in entry['images']))
    assert pair not in pairs, entry['id']
    pairs.add(pair)
    for image in entry['images']:
        assert re.fullmatch(r'assets/graphics/[0-9a-f]{64}\.png', image['path'])
        name = Path(image['path']).name
        info = manifest['files'][name]
        assert [image['width'], image['height']] == [info['width'], info['height']]
        assert image['label'] and image['width'] > 0 and image['height'] > 0
        assert sources[image['source']] == name
        delivered.add(name)
assert len(sources) == catalog['imageCount'] == manifest['source_pngs']
assert len(delivered) == catalog['uniqueFiles']
assert delivered == set(manifest['files']) == {p.name for p in (SITE/'assets/graphics').glob('*.png')}
assert {p.suffix for p in (SITE/'assets/graphics').iterdir()} == {'.json','.png'}
original_bytes = {}
for name, info in manifest['files'].items():
    raw = (SITE/'assets/graphics'/name).read_bytes()
    original_bytes[name] = raw
    assert len(raw) == info['bytes']
    assert hashlib.sha256(raw).hexdigest() == info['sha256'] == name[:-4]
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', raw[16:24]) == (info['width'], info['height'])
    assert pixel_digest(raw) == info['pixels_sha256']

# Regressions: every resource 32..63 used to show ALL pickups under one name.
for resource in range(32,64):
    for frame,collection,title in [(1,'pickup-01','Pistol'),(2,'pickup-02','Shotgun'),
            (14,'pickup-14','25 health'),(28,'pickup-28','Ankh')]:
        for size in (0,28):
            entry = by_id[f'actor-{resource:03}-{frame+size:03}']
            assert entry['collection'] == collection and title in entry['title']
for resource in (96,97,98):
    for frame,title in enumerate(['Piano','Anvil','Safe','Piano','Anvil','Safe'],1):
        assert by_id[f'actor-{resource:03}-{frame:03}']['collectionTitle'] == title
for resource in (4,5,30,67):
    assert by_id[f'actor-{resource:03}-001']['collectionTitle'] == 'Cyclops'
assert by_id['actor-101-001']['collectionTitle'] == 'Sam'
assert by_id['sprite-006']['collection'] == 'weapon-2'
assert by_id['sprite-286']['collection'] == 'hud-weapons'
assert by_id['sprite-279']['collection'] == 'hud-ammo'
for index in (295,296,297,298): assert by_id[f'sprite-{index:03}']['collection'] == 'crosshairs'
assert by_id['sprite-142']['collection'] == 'button-prompts'
assert by_id['sprite-235']['collection'] == 'character-shapes'
assert by_id['sprite-304']['collection'] == 'screen-effects'
assert all(not e['collectionTitle'].startswith(('Resource ','Shared item')) for e in entries)
archive_path = ROOT/'build/exports/ssa-graphics.zip'
verified_originals = False
if archive_path.exists():
    with zipfile.ZipFile(archive_path) as archive:
        assert set(sources)|set(manifest['excluded']) == {p for p in archive.namelist() if p.endswith('.png')}
        assert hashlib.sha256(archive.read('manifest.json')).hexdigest() == manifest['source_manifest_sha256']
        for name, info in manifest['files'].items():
            assert archive.read(info['source']) == original_bytes[name], info['source']
        for source, name in sources.items():
            assert pixel_digest(archive.read(source)) == manifest['files'][name]['pixels_sha256'], source
    verified_originals = True
print(json.dumps({'status':'PASS', 'entries':len(entries), 'original_pngs':len(sources),
    'unique_files':len(delivered), 'compared_with_original_export':verified_originals}))
