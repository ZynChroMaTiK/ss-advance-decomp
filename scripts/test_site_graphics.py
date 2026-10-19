"""Check unique pictures, content grouping and unchanged original artwork."""
from pathlib import Path
from collections import Counter
import hashlib, json, re, struct, zipfile, zlib
from graphics_brightness import BRIGHTNESS_SETS
from graphics_content import pixel_digest, is_sheet, actor_variant
from graphics_usage import selected_actor_frames, referenced_textures, SECTIONS
from export_graphics_archive import png

# Same visible picture, different unused/transparent palette colours.
a = png(2,1,b'\0\1',[0,31,123],True)
b = png(2,1,b'\0\1',[32767,31,456],True)
assert a != b and pixel_digest(a) == pixel_digest(b)
assert pixel_digest(a) != pixel_digest(png(2,1,b'\0\1',[0,32,123],True))
assert pixel_digest(a) != pixel_digest(png(2,1,b'\0\1',[0,31,123],False))
assert pixel_digest(a) != pixel_digest(png(1,2,b'\0\1',[0,31,123],True))

layout={'id':4,'directions':5,'frames_per_direction':6,'resolution_flags':30}
variant=actor_variant(layout,61)
assert (variant['animationFrame'],variant['view'],variant['mip'],variant['resolutionPercent']) == (1,1,2,50)
assert variant['variantTitle']=='Frame 1 · View 1' and variant['resolutionLabel']=='Half resolution'
assert actor_variant(layout,37)['view']==2
pickup=actor_variant({'id':32,'directions':1,'frames_per_direction':28,'resolution_flags':10},29)
assert pickup['mip']==1 and pickup['resolutionPercent']==50 and 'animationFrame' not in pickup
assert 'Frame' not in pickup['variantTitle']
assert actor_variant({'id':6,'directions':5,'frames_per_direction':5,'resolution_flags':28},1)['resolutionPercent']==75
assert actor_variant({'id':15,'directions':5,'frames_per_direction':1,'resolution_flags':2},6)=={'variantTitle':'Stored variant 1'}
for index in range(1,6):
    rocket=actor_variant({'id':15,'directions':5,'frames_per_direction':1,'resolution_flags':2},index)
    assert rocket['mipCount']==1 and rocket['resolutionLabel']=='Single resolution' and rocket['view']==index
    assert 'animationFrame' not in rocket
for index in range(1,6):
    effect=actor_variant({'id':92,'directions':1,'frames_per_direction':5,'resolution_flags':2},index)
    assert effect['mipCount']==1 and effect['mip']==0 and effect['animationFrame']==index

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
    if entry['category'] not in ('actors','textures'):
        assert not any(k in entry for k in ('mip','mipCount','resolutionLabel')),'screen artwork is not distance mipmapped'
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
    assert entry['mipCount']==3 and entry['mip']==entry['resolution'] and entry['resolutionPercent']==100//(2**entry['mip'])
    assert entry['resolutionLabel'] not in entry['title']
for id in ('actor-032-029','actor-096-004'):
    assert by_id[id]['resolutionLabel']=='Half resolution' and 'Frame' not in by_id[id]['title']
assert all('resolution' not in e['title'].lower() and '(mip' not in e['title'].lower() for e in entries)

def texture_light(raw):
    """Read opaque indexed texture pixels for independent brightness checks."""
    pos=8; compressed=b''
    while pos<len(raw):
        size,kind=struct.unpack_from('>I4s',raw,pos); payload=raw[pos+8:pos+8+size]; pos+=size+12
        if kind==b'IHDR': width,height=struct.unpack_from('>II',payload)
        if kind==b'PLTE': palette=[sum(payload[i:i+3])/3 for i in range(0,len(payload),3)]
        if kind==b'IDAT': compressed+=payload
    rows=zlib.decompress(compressed)
    pixels=[palette[i] for y in range(height) for i in rows[y*(width+1)+1:(y+1)*(width+1)]]
    return width,height,pixels

brightness_count=0
for family,groups in BRIGHTNESS_SETS.items():
    indices=[i for group in groups for i in group]
    assert len(indices)==len(set(indices))
    for group in groups:
        for mip in range(3):
            variants=[by_id[f'texture-{family}-{mip}-{i:03}'] for i in group]
            levels=['Light','Dark'] if len(group)==2 else ['Light','Medium','Dark']
            means=[]
            for entry,level in zip(variants,levels):
                assert entry['brightnessLabel']==level+' variant'
                assert [by_id[v['id']]['id'] for v in entry['brightnessVariants']]==[v['id'] for v in variants]
                w,h,pixels=texture_light(original_bytes[Path(entry['images'][0]['path']).name])
                means.append(sum(pixels)/len(pixels))
                brightness_count+=1
            assert all(a>b+3 for a,b in zip(means,means[1:])),(family,group,mip,means)
# Similar filenames can depict different motifs, switch states or mirrored art.
for family,indices in [(0,[25,26,27,44,45,58,59,63,64,68,69,70,96,97,98,99,100]),(1,[3,63,64,79,80])]:
    for index in indices:
        assert 'brightnessLabel' not in by_id[f'texture-{family}-0-{index:03}']
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
audited_layouts = Counter()
if archive_path.exists():
    with zipfile.ZipFile(archive_path) as archive:
        assert set(sources)|set(manifest['excluded']) == {p for p in archive.namelist() if p.endswith('.png')}
        assert hashlib.sha256(archive.read('manifest.json')).hexdigest() == manifest['source_manifest_sha256']
        original=json.loads(archive.read('manifest.json'))
        # Independently flatten EVERY original layout, including aliases, rather
        # than validating only the resource that survived picture deduplication.
        for resource in original['actor_resources']:
            if not resource['images']: continue
            scales=[percent for flag,percent in [(1,150),(2,100),(4,75),(8,50),(16,25)] if resource['resolution_flags']&flag]
            stride=(len(resource['images'])//resource['directions']//len(scales))&255
            audited_layouts['resources']+=1
            for image in resource['images']:
                offset=image['index']-1
                entry=by_id[f'actor-{resource["id"]:03}-{image["index"]:03}']
                if offset>=stride*resource['directions']*len(scales):
                    assert 'mip' not in entry and 'resolutionLabel' not in entry
                    audited_layouts['outside_layout']+=1
                    continue
                mip=offset//(stride*resource['directions'])
                assert (entry['mip'],entry['mipCount'],entry['resolutionPercent'])==(mip,len(scales),scales[mip]),(resource['id'],image['index'])
                assert (entry['resolutionLabel']=='Single resolution')==(len(scales)==1)
                audited_layouts['single_resolution' if len(scales)==1 else 'multiple_resolutions']+=1
        for entry in entries:
            if entry['category']!='actors': continue
            resource,index=map(int,entry['id'].split('-')[1:])
            variant=actor_variant(original['actor_resources'][resource],index)
            for key,value in variant.items():
                if key!='variantTitle': assert entry[key]==value,(entry['id'],key)
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
    'unique_files':len(delivered), 'brightness_texture_records':brightness_count, 'actor_layout_audit':dict(audited_layouts), 'compared_with_original_export':verified_originals}))
