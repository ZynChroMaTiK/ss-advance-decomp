"""Validate and package the ROM-derived map atlas after PNG rendering."""
from pathlib import Path
import hashlib, json, struct, zipfile
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/exports/ssa-topmaps'
maps=[json.loads((OUT/'data'/f'level-{i:02}.json').read_text()) for i in range(1,17)]
report=json.loads((OUT/'validation.json').read_text())
assert hashlib.sha1((ROOT/'orig/SSA.gba').read_bytes()).hexdigest()==report['rom_sha1']
sprites=json.loads((OUT/'sprite-manifest.json').read_text())
theme=json.loads((OUT/'theme-manifest.json').read_text())
with zipfile.ZipFile(ROOT/'build/exports/ssa-graphics.zip') as original:
    for asset in theme.values():
        data=(OUT/asset['path']).read_bytes()
        assert hashlib.sha256(data).hexdigest()==asset['sha256']
        assert data==original.read(asset['source'])
    for s in sprites.values():
        icon=(OUT/s['path']).read_bytes()
        assert hashlib.sha256(icon).hexdigest()==s['sha256']
        if len(s['layers'])==1:
            a=s['layers'][0]
            path=f'actors/palette-{s["palette"]}/resource-{a["resource"]:03}/frame-{a["image"]:03}.png'
            assert icon==original.read(path),path
sources=[]
for i,m in enumerate(maps,1):
    tree=ET.parse(OUT/'svg'/f'level-{i:02}.svg')
    refs={e.get('data-ref') for e in tree.iter()}
    assert not any(e.tag.endswith('circle') and (e.get('data-ref') or '').startswith('M') for e in tree.iter())
    marker_ids={p['id'] for p in m['markers']}
    script_ids={s['id'] for s in m['scripts']}
    for p in m['markers']:
        assert 0<=p['sector']<len(m['sectors'])
        # Coincident markers share a glyph but preserve every record in JSON.
        assert any(q['id'] in refs for q in m['markers'] if (q['x'],q['y'],q['group'],q['code'])==(p['x'],p['y'],p['group'],p['code']))
        if p['origin']=='script':assert p['script'] in script_ids
    for d in m['movers']:
        assert 'door-'+str(d['id']) in refs
        for c in d['scripts']:assert c['script'] in script_ids
    for sw in m['switches']:assert sw['script'] in script_ids
    for s in m['scripts']:
        for op in s['ops']:
            assert all(k in marker_ids for k in op.get('markers',[]))
            if op['op'] in (4,5,6,7):assert op['args'][1]<len(m['movers'])
    for s in m['secrets']:
        p=next(p for p in m['markers'] if p['id']==s['marker'])
        assert (p['x'],p['y'])==(s['x'],s['y']) and s['id'] in refs
        if 'access' in s:
            a=s['access'];assert s['id']+'-access' in refs
            script=next(x for x in m['scripts'] if x['id']==a['script'])
            door=next(d for d in m['movers'] if d['label']==a['door'])
            assert any(o['op']==4 and o['args'][1]==door['id'] for o in script['ops'])
            if 'sector' in a:assert m['sectors'][a['sector']]['flags']==256+a['script']
            if 'switch' in a:assert next(sw for sw in m['switches'] if 'B'+str(sw['id'])==a['switch'])['script']==a['script']
    png=(OUT/'png'/f'level-{i:02}.png').read_bytes()
    assert png[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack_from('>II',png,16)==(1760,1900)
    assert tree.getroot().get('viewBox')=='0 0 1760 1900'
    assert not {'MAP KEY','DOORS & LIFTS','SECRETS / COVERAGE'} & {e.text for e in tree.iter()}
    sources.append(dict(level=i,title=m['title'],secret_status=m['secret_status'],guide=m['source'],secrets=m['secrets']))
assert sum(len(m['scripts']) for m in maps)==116
viewer=(OUT/'index.html').read_text()
assert all(token not in viewer for token in ['__DATA__','__STYLE__','__SCRIPT__']) and '<!doctype html>' in viewer
assert 'data-focus' in viewer and 'onpointermove' in viewer
(OUT/'sources-and-secret-coverage.json').write_text(json.dumps(sources,indent=2))
readme=(ROOT/'scripts/topmap_readme.txt').read_text(encoding='utf8')
(OUT/'README.txt').write_text(readme)
report.update(scripts=116,svg_xml_valid=16,png_dimensions_verified=16,sheet_legends_removed=16,
              original_theme_assets_byte_identical=len(theme),
              sprite_png_hashes_verified=len(sprites),original_single_layer_sprites_byte_identical=True,
              no_dot_object_markers=True,
              script_mover_references_valid=True,secret_pickups_and_access_triggers_valid=True,
              rom_unchanged=True,static_visual_review=True,interactive_browser_qa='unavailable: no connected browser',
              secret_coverage='5 documented secrets; other unclassified areas remain fully visible')
(OUT/'validation.json').write_text(json.dumps(report,indent=2))
manifest={str(p.relative_to(OUT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
          for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='manifest-sha256.json'}
(OUT/'manifest-sha256.json').write_text(json.dumps(manifest,indent=2))
archive=OUT.with_suffix('.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(OUT.rglob('*')):
        if p.is_file():z.write(p,p.relative_to(OUT))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert len([p for p in z.namelist() if p.startswith('png/')])==16
    assert len([p for p in z.namelist() if p.startswith('svg/')])==16
print(json.dumps(dict(archive=str(archive),bytes=archive.stat().st_size,
                     sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),files=len(manifest)+1,
                     validation=report),indent=2))
