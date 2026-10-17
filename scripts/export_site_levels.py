"""Build static, ROM-derived Three.js geometry from the verified overhead atlas.

No ROM code executes. Walls reproduce pc_renderer_pitch.inc's directed spans,
texture binding, offsets and two-world-units-per-texel mapping. The open state
is an inspection preview, not game simulation or script execution.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import zipfile

from export_presentation_assets import decompress

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'site'
SHA1 = '4ff48d8c18b70a5184758f1917343ae85a70858e'
u16 = lambda data, at: struct.unpack_from('<H', data, at)[0]
s16 = lambda data, at: struct.unpack_from('<h', data, at)[0]
u32 = lambda data, at: struct.unpack_from('<I', data, at)[0]
sha = lambda data: hashlib.sha256(data).hexdigest()


def fold_name(data):
    return data[:8].split(b'\0', 1)[0].lower()


def clean_loop(points):
    """Remove redundant vertices without changing a convex leaf's boundary."""
    result = []
    for p in points:
        if not result or math.dist(p, result[-1]) > 1e-7:
            result.append(p)
    if len(result) > 1 and math.dist(result[0], result[-1]) <= 1e-7:
        result.pop()
    changed = True
    while changed and len(result) >= 3:
        changed = False
        for i, b in enumerate(result):
            a, c = result[i-1], result[(i+1) % len(result)]
            cross = (b[0]-a[0])*(c[1]-b[1]) - (b[1]-a[1])*(c[0]-b[0])
            if abs(cross) <= 1e-7:
                result.pop(i)
                changed = True
                break
    return result if len(result) >= 3 else []


def main():
    rom = (ROOT / 'orig/SSA.gba').read_bytes()
    assert hashlib.sha1(rom).hexdigest() == SHA1
    (OUT / 'data').mkdir(parents=True, exist_ok=True)
    texture_dir = OUT / 'assets/textures'
    texture_dir.mkdir(parents=True, exist_ok=True)
    reports = json.loads((ROOT / 'analysis/native-map-assets-validation.json').read_text())
    texture_reports = json.loads((ROOT / 'analysis/native-texture-assets-validation.json').read_text())
    native_texture_hashes = {(t['family'], t['mip'], t['index']): t for t in texture_reports['images_sha256']}
    texture_manifest = {'rom_sha1': SHA1, 'textures': [], 'levels': []}
    catalog = []
    families = [[], []]
    with zipfile.ZipFile(ROOT / 'build/exports/ssa-graphics.zip') as archive:
        source = json.loads(archive.read('manifest.json'))
        assert source['rom_sha1'] == SHA1
        for t in source['textures']:
            if t['mip'] != 0:
                continue
            data = archive.read(t['png'])
            assert sha(data) == source['files'][t['png']]['sha256']
            assert struct.unpack_from('>II', data, 16) == (t['width'], t['height'])
            pixels = rom[t['rom_offset']:t['rom_offset']+t['width']*t['height']]
            native = native_texture_hashes[t['family'], 0, t['index']]
            assert sha(pixels) == native['sha256']
            relative = f"assets/textures/family-{t['family']}/{Path(t['png']).name}"
            target = OUT / relative
            target.parent.mkdir(exist_ok=True)
            target.write_bytes(data)
            entry = {**t, 'source': t['png'], 'path': relative, 'sha256': sha(data), 'pixels_sha256': sha(pixels)}
            texture_manifest['textures'].append(entry)
            families[t['family']].append(entry)
    assert len(texture_manifest['textures']) == 222
    totals = {'maps': 0, 'sectors': 0, 'leaf_polygons': 0, 'floor_triangles': 0,
              'initial_faces': 0, 'open_faces': 0, 'textured_initial_faces': 0,
              'textured_open_faces': 0, 'nonfinite_values': 0, 'degenerate_triangles': 0,
              'initial_objects': 0, 'scripted_placements': 0, 'movers': 0, 'secrets': 0}
    for level in range(16):
        stem = f'level-{level+1:02}'
        model = json.loads((ROOT / f'build/exports/ssa-topmaps/data/{stem}.json').read_text())
        raw, _ = decompress(rom, u32(rom, 0x44aef4+level*4)-0x8000000)
        assert sha(raw) == reports['maps'][level]['decoded_sha256'] == model['rom_sha256']
        family = 0 if level <= 5 or level in (12, 13) else 1
        palette = struct.unpack_from('<256H', rom, 0x454a5c+family*512)
        def color(index):
            value = palette[index] if index else 0
            return '#' + ''.join(f'{((value >> (5*k) & 31) << 3) | ((value >> (5*k) & 31) >> 2):02x}' for k in range(3))
        texture_lookup = {}
        for t in families[family]:
            texture_lookup.setdefault(fold_name(t['name'].encode('ascii')), t)
        bindings = []
        for index in range(u16(raw, 8)):
            at = u32(raw, 0x20)+16*index
            bindings.append(texture_lookup.get(fold_name(raw[at:at+8])))
        for sector in model['sectors']:
            at = u32(raw, 0x24)+20*sector['id']
            sector.update(floor_rgb=color(raw[at+10]), ceiling_rgb=color(raw[at+11]), ceiling_color=raw[at+11],
                          initial_floor=sector['floor'], initial_ceiling=sector['ceiling'], open_ceiling=sector['ceiling'])
            # Retain source loops, and provide triangulation-safe convex mesh loops.
            sector['mesh_loops'] = [p for loop in sector['loops'] if (p := clean_loop(loop))]
            for points in sector['mesh_loops']:
                totals['leaf_polygons'] += 1
                for b, c in zip(points[1:], points[2:]):
                    a = points[0]
                    assert abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])) > 1e-7
                    assert all(math.isfinite(v) for p in (a, b, c) for v in p)
                    totals['floor_triangles'] += 1
        door_sectors = set()
        for mover in model['movers']:
            sector = model['sectors'][mover['sector']]
            if mover['flags'] & 256:
                sector['initial_floor'] = mover['upper'] if mover['flags'] & 1 else mover['lower']
            else:
                door_sectors.add(mover['sector'])
                sector['open_ceiling'] = ((sector['initial_floor']+224+32768) & 65535)-32768
        def walls(opened):
            result = []
            for line in model['lines']:
                p = u32(raw, 0x38)+line['id']*40
                for direction in range(2 if line['flags'] & 4 else 1):
                    si, sec = u16(raw, p+8+direction*2), u16(raw, p+12+direction*2)
                    assert si < u16(raw, 18) and sec < len(model['sectors'])
                    a, b = model['vertices'][line['a']], model['vertices'][line['b']]
                    if direction:
                        a, b = b, a
                    length = math.dist(a, b)
                    if length <= 1e-7:
                        continue
                    side = u32(raw, 0x34)+12*si
                    along, offset = s16(raw, side), s16(raw, side+2)
                    sector = model['sectors'][sec]
                    ff, fc = sector['initial_floor'], sector['open_ceiling' if opened else 'initial_ceiling']
                    back_id = u16(raw, p+12+(direction ^ 1)*2) if line['flags'] & 4 else None
                    def image(layer):
                        index = u16(raw, side+4+2*layer)
                        if index == 65535:
                            return None
                        assert index < len(bindings) and bindings[index] is not None, (level, si, layer, index)
                        return bindings[index]
                    def emit(layer, low, high, origin):
                        if high <= low:
                            return
                        im = image(layer)
                        w, h = (im['width'], im['height']) if im else (1, 1)
                        u0, u1 = along/(2*w), (along+length)/(2*w)
                        v0, v1 = 1-(origin+offset-low)/(2*h), 1-(origin+offset-high)/(2*h)
                        positions = [a[0], low, -a[1], b[0], low, -b[1], b[0], high, -b[1], a[0], high, -a[1]]
                        uvs = [u0, v0, u1, v0, u1, v1, u0, v1]
                        assert all(math.isfinite(v) for v in positions+uvs)
                        assert length*(high-low) > 1e-7
                        result.append({'line': line['id'], 'sector': sec, 'layer': ['upper', 'middle', 'lower'][layer],
                                       'texture': im['path'] if im else None, 'positions': positions, 'uvs': uvs,
                                       'door': sec in door_sectors or back_id in door_sectors})
                    if not line['flags'] & 4:
                        im = image(1)
                        origin = ff+im['height']*2 if line['flags'] & 16 and im else fc
                        emit(1, ff, fc, origin)
                    else:
                        assert back_id < len(model['sectors'])
                        back = model['sectors'][back_id]
                        bf, bc = back['initial_floor'], back['open_ceiling' if opened else 'initial_ceiling']
                        if bc < fc:
                            im = image(0)
                            origin = fc if line['flags'] & 8 or not line['kind'] else bc+(im['height']*2 if im else 0)
                            emit(0, max(bc, ff), fc, origin)
                        if bf > ff:
                            emit(2, ff, min(bf, fc), bf)
            return result
        model['walls'] = {'initial': walls(False), 'open': walls(True)}
        model['viewer'] = {'schema': 1, 'coordinate_system': '[ROM x, ROM height, -ROM y]; divide by64 in viewer',
                           'texture_family': family, 'texture_flip_y': True, 'texture_wrap': 'repeat',
                           'texture_filter': 'nearest', 'original_units_per_texel': 2,
                           'door_preview': 'ceiling doors at floor+224; floor lifts retain initialized heights',
                           'geometry_source': 'src/pc_renderer_pitch.inc', 'floor_mesh_field': 'mesh_loops'}
        catalog.append({'level': model['level'], 'title': model['title'], 'path': f'data/{stem}.json',
                        'sectors': len(model['sectors']), 'markers': len(model['markers']), 'secrets': len(model['secrets']),
                        'bounds': {'min_x': min(p[0] for p in model['vertices']), 'max_x': max(p[0] for p in model['vertices']),
                                   'min_y': min(p[1] for p in model['vertices']), 'max_y': max(p[1] for p in model['vertices']),
                                   'min_height': min(s['initial_floor'] for s in model['sectors']),
                                   'max_height': max(max(s['initial_ceiling'], s['open_ceiling']) for s in model['sectors'])}})
        # All existing marker fields (including icons/... paths) remain intact.
        for m in model['markers']:
            m['initial_floor'] = model['sectors'][m['sector']]['initial_floor']
            if m.get('sprite'):
                assert (ROOT / 'build/exports/ssa-topmaps' / m['sprite']).is_file()
            totals['initial_objects' if m['origin'] == 'map' else 'scripted_placements'] += 1
        for state in ('initial', 'open'):
            totals[f'{state}_faces'] += len(model['walls'][state])
            totals[f'textured_{state}_faces'] += sum(f['texture'] is not None for f in model['walls'][state])
        totals['maps'] += 1
        for key in ('sectors', 'movers', 'secrets'):
            totals[key] += len(model[key])
        data = json.dumps(model, separators=(',', ':'), ensure_ascii=False, allow_nan=False).encode('utf8')
        (OUT / f'data/{stem}.json').write_bytes(data)
        texture_manifest['levels'].append({'path': f'data/{stem}.json', 'sha256': sha(data), 'decoded_map_sha256': sha(raw),
                                           'initial_faces': len(model['walls']['initial']), 'open_faces': len(model['walls']['open'])})
    assert totals['maps'] == 16 and totals['sectors'] == 942
    assert hashlib.sha1((ROOT / 'orig/SSA.gba').read_bytes()).hexdigest() == SHA1
    texture_manifest['validation'] = {**totals, 'textures': len(texture_manifest['textures']), 'status': 'PASS',
                                      'source_archive_sha256': sha((ROOT / 'build/exports/ssa-graphics.zip').read_bytes()),
                                      'script_sha256': sha(Path(__file__).read_bytes())}
    (texture_dir / 'manifest.json').write_text(json.dumps(texture_manifest, indent=2)+'\n', encoding='utf8')
    (OUT / 'data/catalog.json').write_text(json.dumps(catalog, indent=2)+'\n', encoding='utf8')
    print(json.dumps(texture_manifest['validation'], indent=2))


if __name__ == '__main__':
    main()
