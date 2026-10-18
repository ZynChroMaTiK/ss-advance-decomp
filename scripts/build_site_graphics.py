"""Build a browseable catalog of every PNG in the verified graphics export.

Only lossless PNGs are published. Identical files share storage; every original
file remains reachable through an entry and, for actor frames, a palette choice.
Run after export_graphics_archive.py. No ROM is needed for this step.
"""
from pathlib import Path
from collections import Counter, defaultdict
import hashlib, json, re, struct, zipfile

ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = {
    'menus': 'Menus and backgrounds', 'netricsa': 'NETRICSA pictures',
    'story': 'Story pictures', 'splashes': 'Logos and hidden pictures',
    'sprites': 'Weapons and HUD', 'actors': 'Enemies, items and effects',
    'textures': 'Wall textures', 'sky': 'Skies', 'fonts': 'Fonts',
    'palettes': 'Colour palettes',
}

def main():
    destination = ROOT / 'site/assets/graphics'
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(ROOT / 'build/exports/ssa-graphics.zip') as archive:
        manifest = json.loads(archive.read('manifest.json'))
        assert manifest['rom_sha1'] == '4ff48d8c18b70a5184758f1917343ae85a70858e'
        source_pngs = {p for p in manifest['files'] if p.endswith('.png')}
        assert source_pngs == {p for p in archive.namelist() if p.endswith('.png')}
        used = set(); files = {}; entries = []
        names = defaultdict(set)
        for path in sorted((ROOT/'site/data').glob('level-*.json')):
            for marker in json.loads(path.read_text(encoding='utf8'))['markers']:
                match = re.search(r'-r(\d+)', marker.get('sprite', ''))
                if match: names[int(match[1])].add(marker['name'])
        wiki = json.loads((ROOT/'scripts/wiki_entities.json').read_text(encoding='utf8'))
        portraits = {Path(e['image']).name: e['title'] for e in wiki}
        portraits['netricsa-22.png'] = 'NETRICSA'

        def image(path):
            assert path in source_pngs and path not in used, path
            used.add(path)
            raw = archive.read(path)
            digest = hashlib.sha256(raw).hexdigest()
            assert digest == manifest['files'][path]['sha256'], path
            assert raw[:8] == b'\x89PNG\r\n\x1a\n'
            width, height = struct.unpack('>II', raw[16:24])
            name = digest + '.png'
            if name not in files:
                (destination/name).write_bytes(raw)
                files[name] = {'sha256': digest, 'bytes': len(raw), 'width': width, 'height': height}
            return {'path': 'assets/graphics/'+name, 'source': path, 'width': width, 'height': height}

        for resource in manifest['actor_resources']:
            group = resource['id']; aliases = sorted(names[group])
            label = ', '.join(aliases) if 0 < len(aliases) <= 3 else 'Shared item sprites' if aliases else f'Resource {group:03}'
            for frame in resource['images']:
                entries.append({'id': f'actor-{group:03}-{frame["index"]:03}', 'category': 'actors',
                    'title': f'{label} · frame {frame["index"]}', 'group': group,
                    'groupLabel': f'{group:03} · {label}', 'frame': frame['index'],
                    'tags': ' '.join(aliases), 'origin': [frame['origin_x'], frame['origin_y']],
                    'images': [dict(image(p), label=label) for p,label in zip(frame['png'], ['Egypt colours', 'Rome colours'])]})
        for texture in manifest['textures']:
            family = ['Egypt', 'Rome'][texture['family']]
            entries.append({'id': f'texture-{texture["family"]}-{texture["mip"]}-{texture["index"]:03}',
                'title': texture['name'], 'category': 'textures', 'family': family,
                'resolution': texture['mip'], 'tags': f'{family} resolution {texture["mip"]}',
                'images': [dict(image(texture['png']), label=f'{family} · resolution {texture["mip"] + 1} of 3')]})
        for sprite in manifest['sprites']:
            entries.append({'id': f'sprite-{sprite["id"]:03}', 'title': f'Sprite {sprite["id"]:03}',
                'category': 'sprites', 'origin': [sprite['min_x'], sprite['min_y']],
                'images': [dict(image(sprite['png']), label='Original colours')]})
        for path in sorted(source_pngs-used):
            parts = Path(path).parts; category = parts[0]
            assert category in CATEGORIES, path
            title = Path(path).stem.replace('-', ' ').replace('_', ' ').capitalize()
            tags = ''
            if category == 'netricsa': title = portraits.get(Path(path).name, title)
            if category == 'fonts':
                title = f'{parts[1]} · '+('Glyph '+Path(path).stem if 'glyphs' in parts else title)
                if 'glyphs' in parts:
                    metrics = json.loads(archive.read(f'fonts/{parts[1]}/metrics.json'))
                    values = metrics['glyphs'][int(Path(path).stem)]['character_bytes']
                    tags = ' '.join(chr(c) for c in values if 32 <= c < 127)
                    if tags: title += f' · {tags}'
            if category == 'sky': title = ('Egypt' if 'family-0' in path else 'Rome')+' · '+title
            if category == 'palettes' and 'world-bank' in path: title = f'World palette {Path(path).stem}'
            note = ''
            if path in ('story/story-06.png', 'story/story-07.png'):
                note = 'The placeholder text is part of the original picture.'
            if 'hidden-photo' in path: note = 'One of the hidden pictures stored in the game.'
            entries.append({'id': re.sub(r'[^a-zA-Z0-9-]', '-', path.removesuffix('.png')),
                'title': title, 'category': category, 'tags': tags, 'note': note,
                'images': [dict(image(path), label='Original colours')]})
        assert used == source_pngs
        assert len({e['id'] for e in entries}) == len(entries)
        order = {c:i for i,c in enumerate(CATEGORIES)}
        entries.sort(key=lambda e: (order[e['category']], e.get('group', 0), e.get('frame', 0), e['id']))
        counts = Counter(e['category'] for e in entries)
        catalog = {'version': 1, 'categories': [{'id': c, 'title': title, 'count': counts[c]} for c,title in CATEGORIES.items()],
            'entryCount': len(entries), 'imageCount': len(source_pngs), 'uniqueFiles': len(files), 'entries': entries}
        (ROOT/'site/data/graphics.json').write_text(json.dumps(catalog,ensure_ascii=False,separators=(',', ':'))+'\n',encoding='utf8')
        report = {'rom_sha1': manifest['rom_sha1'], 'source_pngs': len(source_pngs),
            'entries': len(entries), 'categories': dict(counts), 'files': files,
            'source_manifest_sha256': hashlib.sha256(archive.read('manifest.json')).hexdigest()}
        (destination/'manifest.json').write_text(json.dumps(report,separators=(',', ':'))+'\n',encoding='utf8')
        print(json.dumps({'entries':len(entries), 'source_pngs':len(source_pngs),
            'unique_pngs':len(files), 'image_bytes':sum(f['bytes'] for f in files.values()), 'categories':dict(counts)}))

if __name__ == '__main__': main()
