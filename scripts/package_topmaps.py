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
readme='''SERIOUS SAM ADVANCE - ALL 16 LEVEL TOP MAPS

Open index.html in a modern browser. It works offline and embeds all map data.
Use the level selector, layer checkboxes, wheel zoom and drag-to-pan. Click
items, doors, wall edges and rooms for their coordinates and script details.
Desktop level navigation is grouped into Egypt, Rome and Multiplayer. On
smaller screens it becomes a dropdown. The NETRICSA panel has Layers, Secrets
and Intel tabs; selecting a map object opens Intel automatically. Arrow keys
move between focused tabs. Map guide opens the legend and coverage notes.
Search finds pickups and doors in the selected level. Pink secret buttons
focus the reward, and the access buttons focus known switches / trigger areas.
The PNG / SVG / Map data links require the accompanying folders to stay beside
index.html. Extract the ZIP first.

WEB THEME
The interface uses the original title logo, green stone backdrop, fixed grid,
Egypt/Rome chapter artwork and bitmap font sheets. Green framed panels and
white selection states follow the game's menus and NETRICSA presentation.
The interactive map uses a matching green palette while keeping the sprite
colors and category colors. Printable PNG/SVG sheets retain their export palette.
Sheet legends are provided by the HTML interface; PNG/SVG maps omit the sidebar.
All visual assets and scripts are embedded in index.html for offline use.

CONTENTS
  index.html                 Offline interactive atlas, all 16 levels
  all-levels.png             Overview/contact sheet
  png/level-01.png ... 16    1760 x 1900 annotated maps, without legend sidebars
  svg/level-01.svg ... 16    Scalable annotated maps
  data/level-01.json ... 16  All map geometry, placements, scripts and metadata
  icons/                     Original sprite PNGs used on the maps
  sprite-manifest.json        Original resource/frame/palette for every icon
  theme/                     Original menu art and bitmap font sheets
  theme-manifest.json        Source paths and hashes for all web theme assets
  sources-and-secret-coverage.json  Guide URLs and exact secret evidence
  render-scenes.json        Drawing primitives used to generate the maps
  validation.json           Extraction and package validation results
  manifest-sha256.json      Content hashes (excluding this manifest)

COVERAGE
All 16 decoded map hashes match the existing native map validation. All BSP
leaves were processed. The atlas includes 7,632 line records, 942 sectors,
484 initial objects, 1,057 scripted placements from 116 scripts, 130 movers
(doors and lifts), and 8 switches. Initial and scripted enemies are optional
viewer layers. The four multiplayer maps include every D9 pickup respawn point.

Every room is revealed, including concealed rooms. Five documented secrets
are marked in levels 1-5. Levels 7, 8, 10 and 11 have no secrets according to
the cited level guides. Secret classification remains unverified for levels
6, 9, 12 and the multiplayer maps. No dedicated ROM secret flag has been
established. These pins do not prove exhaustive discovery of every easter egg.
The Pyramid Entrance reward is 25 armor in this ROM, although the guide calls
it Medium Armor. Its room, guardian, door and trigger match ROM script 8.

READING THE MAPS
Coordinates are original map units, +X right and +Y up. D labels number movers
from one; raw mover indices and sectors in JSON start at zero. B marks switches.
S1 is a secret; the S prefix on an item amount means shield. P1-P4 are starts.
Original ROM sprites replace item, enemy and player-start dots. An asterisk (*)
marks scripted-only appearances. R marks a rotating multiplayer spawn site.
Leader lines connect displaced labels to exact positions.
Repeated identical placements at one position share a glyph; all individual
records remain in the JSON and viewer details. Items are not all present at once.
H100 adds 100 health with a 200 cap. Red floors are lethal surfaces or pits.
Height differences and stairs remain visible as sector boundaries; floor and
ceiling heights are available by clicking a room. Doors show initial lock state.
Scripts can open, close, unlock or lock them later. The viewer exposes these
commands and the switches / first-entry sectors / pickups that request them.

ROM AND REPRODUCTION
Original ROM SHA-1: 4ff48d8c18b70a5184758f1917343ae85a70858e
The original ROM is unchanged and is not included in this archive.
Map table 0844AEF4; object translation 0844A3D4; script table 0845280C;
mover counts/table 0844DD3C/0844DD4C. Geometry uses BSP half-plane clipping,
following the native renderer, instead of guessing closed outlines from lines.
The level-11 original object-coordinate correction is applied to item locations.
Multiplayer R locations are possible rotating spawn sites, not fixed item types.
Sprites use each resource's first animation sequence frame (not simply image 1),
the highest available resolution, and the level's original world palette.
Player starts compose the original Sam base and shirt overlays using ROM origins;
campaign starts use white, multiplayer starts use the four original variants.
Sprites are map icons with normalized display size, not world-scale footprints.
SVGs and the viewer embed PNGs, so their sprite artwork works offline.

Workspace generators:
  scripts/export_topmaps.py (uses export_presentation_assets.decompress)
  scripts/topmap_viewer.html, topmap_viewer.css, topmap_viewer.js
  scripts/topmap_secrets.json
  scripts/render_topmaps.ps1
  scripts/package_topmaps.py
Run the Python generator, then the PowerShell renderer, then the package script.
The static maps were visually inspected. Interactive browser QA was unavailable
in the generation session; see validation.json for the checks actually run.

LEVEL INDEX
'''
readme+='\n'.join(f'{i:02}  {m["title"]}' for i,m in enumerate(maps,1))+'\n'
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
