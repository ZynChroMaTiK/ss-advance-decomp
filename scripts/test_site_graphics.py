"""Check unique pictures, content grouping and unchanged original artwork."""
from pathlib import Path
from collections import Counter
import hashlib, json, re, struct, zipfile
from graphics_content import pixel_digest, is_sheet
from graphics_usage import selected_actor_frames, referenced_textures, SECTIONS
from export_graphics_archive import png

# Same visible picture, different unused/transparent palette colours.
a = png(2,1,b'\0\1',[0,31,123],True)
b = png(2,1,b'\0\1',[32767,31,456],True)
assert a != b and pixel_digest(a) == pixel_digest(b)
assert pixel_digest(a) != pixel_digest(png(2,1,b'\0\1',[0,32,123],True))
assert pixel_digest(a) != pixel_digest(png(2,1,b'\0\1',[0,31,123],False))
assert pixel_digest(a) != pixel_digest(png(1,2,b'\0\1',[0,31,123],True))

# Texture reachability must include the unused-looking switch-on image and
# multiplayer maps, but exclude spare descriptors and later duplicate names.
class MapFixture:
    def read(self,path):
        level=int(re.search(r'map-(\d+)',path)[1])
        names=['used','sw1_off ','sw1_on  ','spare','multi']
        if path.endswith('.json'):
            return json.dumps({'texture_names':names,'sections':{
                'sides':{'offset':0,'count':1},'lines':{'offset':12,'count':1}}}).encode()
        raw=bytearray(52)
        struct.pack_into('<3h',raw,4,4 if level==12 else 0,1,-1)
        struct.pack_into('<h',raw,20,0)
        struct.pack_into('<H',raw,48,2)
        return raw
fixture_textures=[{'family':f,'mip':m,'index':i,'name':name}
    for f in range(2) for m in range(3)
    for i,name in enumerate(['USED','sw1_off ','sw1_on  ','spare','multi','used'])]
fixture_used=referenced_textures(MapFixture(),{'textures':fixture_textures})
assert fixture_used == {(f,m,i) for f in range(2) for m in range(3) for i in (0,1,2)} | {(0,m,4) for m in range(3)}

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
assert {s['id'] for s in catalog['usageSections']} == set(SECTIONS)
assert 'unconfirmed' not in SECTIONS and SECTIONS['old-hud']=='Old HUD'
for entry in entries:
    assert isinstance(entry['unused'],bool)
    assert bool(entry.get('unusedReason')) == entry['unused']
for id,section in [('sprite-063','old-hud'),('sprite-089','old-hud'),('sprite-110','old-hud'),
        ('sprite-269','old-hud'),('sprite-025','weapons'),('actor-021-009','enemies'),
        ('actor-013-001','projectiles'),('actor-032-004','pickups'),('story-story-05','story'),
        ('splashes-04-hidden-photo-1','menus')]:
    assert by_id[id]['unused'] and {u['section'] for u in by_id[id]['uses']} == {section},id
for id in ['sprite-293','actor-015-001','actor-032-001','story-story-00']:
    assert not by_id[id]['unused'],id
# Every original texture, including duplicate aliases and unused records,
# belongs to its bank's setting. Wall/door roles and individual maps are not
# separate gallery categories. Shared images retain both settings if needed.
textures_by_path = {i['path']:e for e in entries if e['category']=='textures' for i in e['images']}
for source, name in sources.items():
    match = re.match(r'textures/family-([01])/', source)
    if not match: continue
    entry = textures_by_path['assets/graphics/'+name]
    section = 'textures-'+('egypt','rome')[int(match[1])]
    assert section in {u['section'] for u in entry['uses']}, source
for entry in textures_by_path.values():
    assert {u['section'] for u in entry['uses']} <= {'textures-egypt','textures-rome'}
    assert all(u['title'] in ('Egypt textures','Rome textures') for u in entry['uses'])
usage_sources=set()
for entry in entries:
    assert entry['uses'] and len({u['id'] for u in entry['uses']}) == len(entry['uses'])
    for usage in entry['uses']:
        assert usage['section'] in SECTIONS and usage['title'] and usage['source']
        if usage['section'] != 'unconfirmed':
            usage_sources.update(usage['source'].split('; '))
assert all((ROOT/p).is_file() for p in usage_sources)
def uses(id): return [u['title'] for u in by_id[id]['uses']]
assert uses('actor-032-001') == ['Pistol']
assert uses('actor-033-002') == ['Shotgun']
assert uses('actor-096-001') == ['Time Strike · Falling piano']
assert uses('actor-096-003') == ['Time Strike · Falling safe']
assert 'Pistol muzzle flash' in uses('actor-126-001')
assert any('Methug soldier · Attacking' in uses(e['id']) and 'Methug soldier · Taking damage' in uses(e['id']) for e in entries)
assert 'Sirian Sphinx projectile · In flight' in uses('actor-131-001')
assert 'Sirian Sphinx projectile impact' in uses('actor-131-005')
assert 'Sirian Sphinx projectile · In flight' not in uses('actor-131-005')
assert 'HUD · Player 1' in uses('sprite-279')
assert 'Egypt chapter' in uses('story-story-02')
assert 'Rome chapter' in uses('story-story-03')
assert 'Ending' in uses('story-story-04')
archive_path = ROOT/'build/exports/ssa-graphics.zip'
verified_originals = False
if archive_path.exists():
    with zipfile.ZipFile(archive_path) as archive:
        assert set(sources)|set(manifest['excluded']) == {p for p in archive.namelist() if p.endswith('.png')}
        assert hashlib.sha256(archive.read('manifest.json')).hexdigest() == manifest['source_manifest_sha256']
        original=json.loads(archive.read('manifest.json'))
        used_textures=referenced_textures(archive,original)
        used_paths={sources[t['png']] for t in original['textures'] if (t['family'],t['mip'],t['index']) in used_textures}
        for entry in entries:
            if entry['category']=='textures':
                assert entry['unused'] == (Path(entry['images'][0]['path']).name not in used_paths),entry['id']
        sequences=json.loads(archive.read('animations/actors.json'))
        assert selected_actor_frames(original['actor_resources'][32],sequences[32]['words']) == {1,29}
        assert selected_actor_frames(original['actor_resources'][33],sequences[33]['words']) == {2,30}
        # The rocket sequence selects only five of the seven stored images.
        assert selected_actor_frames(original['actor_resources'][15],sequences[15]['words']) == set(range(1,6))
        assert selected_actor_frames(original['actor_resources'][131],sequences[131]['words']) == {1,2,3,4}
        assert selected_actor_frames(original['actor_resources'][132],sequences[132]['words']) == set(range(5,12))
        for name, info in manifest['files'].items():
            assert archive.read(info['source']) == original_bytes[name], info['source']
        for source, name in sources.items():
            assert pixel_digest(archive.read(source)) == manifest['files'][name]['pixels_sha256'], source
    verified_originals = True
print(json.dumps({'status':'PASS', 'entries':len(entries), 'original_pngs':len(sources),
    'unique_files':len(delivered), 'compared_with_original_export':verified_originals}))
