"""Export recovered SSA graphics to a self-contained ZIP (Python stdlib only)."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import zipfile
import zlib
from export_presentation_assets import export as export_presentation

ROOT = Path(__file__).resolve().parents[1]
SHA1 = '4ff48d8c18b70a5184758f1917343ae85a70858e'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def png(width, height, pixels, palette, transparent):
    assert len(pixels) == width * height
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    rgb = bytes(((c >> (5*k) & 31) << 3) | ((c >> (5*k) & 31) >> 2) for c in palette for k in range(3))
    rows = b''.join(b'\0' + pixels[y*width:(y+1)*width] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 3, 0, 0, 0))
            + chunk(b'PLTE', rgb) + (chunk(b'tRNS', b'\0') if transparent else b'')
            + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, default=ROOT/'orig/SSA.gba')
    parser.add_argument('--output', type=Path, default=ROOT/'build/exports/ssa-graphics.zip')
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    assert hashlib.sha1(rom).hexdigest() == SHA1, 'Unexpected ROM'
    def take(offset, size):
        assert 0 <= offset <= len(rom) and 0 <= size <= len(rom)-offset
        return rom[offset:offset+size]
    def u16(offset): return struct.unpack('<H', take(offset, 2))[0]
    def u32(offset): return struct.unpack('<I', take(offset, 4))[0]
    def ptr(offset):
        value = u32(offset)-0x8000000
        assert 0 <= value < len(rom)
        return value
    def rowmajor(pixels, w, h):
        return bytes(pixels[x*h+y] for y in range(h) for x in range(w))
    reports = {name: json.loads((ROOT/f'analysis/native-{name}-assets-validation.json').read_text())
               for name in ('actor', 'sprite', 'texture')}
    manifest = dict(rom_sha1=SHA1, palettes=[], textures=[], actor_resources=[], sprites=[], files={})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists():
        raise FileExistsError(args.output)
    with zipfile.ZipFile(args.output, 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        def put(name, data):
            if isinstance(data, str): data = data.encode('utf-8')
            assert name not in manifest['files'], name
            archive.writestr(name, data)
            manifest['files'][name] = dict(bytes=len(data), sha256=sha(data))
        palettes = []
        for name, offset in [('world-0', 0x454a5c), ('world-1', 0x454c5c), ('sprites', 0x79effc)]:
            raw = take(offset, 512)
            original = list(struct.unpack('<256H', raw))
            colors = original.copy(); colors[0] = 0
            palettes.append(colors)
            put(f'palettes/{name}.rgb555le', raw)
            rgb = [[((c >> (5*k) & 31) << 3) | ((c >> (5*k) & 31) >> 2) for k in range(3)] for c in colors]
            put(f'palettes/{name}.json', json.dumps(dict(rom_offset=offset, original_rgb555=original, runtime_rgb8=rgb), indent=2))
            put(f'palettes/{name}.gpl', 'GIMP Palette\nName: SSA '+name+'\nColumns: 16\n# Runtime palette, index 0 cleared\n' + ''.join(f'{r:3} {g:3} {b:3}\tIndex {i}\n' for i,(r,g,b) in enumerate(rgb)))
            swatch = bytes((y//12)*16+x//12 for y in range(192) for x in range(192))
            put(f'palettes/{name}.png', png(192, 192, swatch, colors, False))
            manifest['palettes'].append(dict(name=name, rom_offset=offset))

        expected_textures = iter(reports['texture']['images_sha256'])
        for family in range(2):
            for mip in range(3):
                off = ptr(0x44aef4 + (16+family*3+mip)*4)
                assert take(off, 4) == b'WOD0'
                count, records = u16(off+8), u32(off+32)
                put(f'original/textures/family-{family}-mip-{mip}.wod0', take(off, records+16*count))
                for i in range(count):
                    rec = off+records+16*i
                    name = take(rec, 8).rstrip(b'\0').decode('ascii')
                    w,h,payload = u16(rec+8),u16(rec+10),u32(rec+12)
                    data = take(off+payload, w*h)
                    expected = next(expected_textures)
                    assert (family,mip,i,w,h,sha(data)) == (expected['family'],expected['mip'],expected['index'],expected['width'],expected['height'],expected['sha256'])
                    safe = re.sub(r'[^a-zA-Z0-9_-]', '_', name.strip())
                    path = f'textures/family-{family}/mip-{mip}/{i:03}-{safe}.png'
                    put(path, png(w,h,rowmajor(data,w,h),palettes[family],False))
                    manifest['textures'].append(dict(family=family,mip=mip,index=i,name=name,width=w,height=h,rom_offset=off+payload,png=path))

        actor_hash = hashlib.sha256(); unique = {}; references = 0
        for resource in range(142):
            d = 0x449d2c+12*resource
            record = dict(id=resource,animation_rom_offset=ptr(d+4),directions=u16(d+8),resolution_flags=u16(d+10),images=[])
            manifest['actor_resources'].append(record)
            if not u32(d): continue
            table = ptr(d); count = u32(table)
            record['frames_per_direction'] = (count//record['directions']//record['resolution_flags'].bit_count()) & 255
            for i in range(1,count+1):
                q = ptr(table+4*i)
                if q not in unique:
                    assert take(q,4) == b'AGB*'
                    ox,oy,w,h = struct.unpack('<hhHH',take(q+24,8))
                    base = q+32+2*w; end = base; columns = bytearray()
                    for x in range(w):
                        pos = base+u16(q+32+2*x); col = bytearray()
                        while len(col)<h:
                            packet = take(pos,1)[0]; pos += 1; n = packet & 127
                            assert n and len(col)+n <= h
                            if packet & 128: col.extend(bytes(n))
                            else: col.extend(take(pos,n)); pos += n
                        columns.extend(col); end = max(end,pos)
                    rawpath = f'original/actors/{q+0x8000000:08X}.agb'
                    put(rawpath,take(q,end-q))
                    unique[q] = (ox,oy,w,h,bytes(columns),rawpath)
                ox,oy,w,h,columns,rawpath = unique[q]
                actor_hash.update(columns); references += 1
                paths = []
                rows = rowmajor(columns,w,h)
                for family in range(2):
                    path = f'actors/palette-{family}/resource-{resource:03}/frame-{i:03}.png'
                    put(path,png(w,h,rows,palettes[family],True)); paths.append(path)
                record['images'].append(dict(index=i,rom_offset=q,origin_x=ox,origin_y=oy,width=w,height=h,original=rawpath,png=paths))
        assert actor_hash.hexdigest() == reports['actor']['decoded_sha256']
        assert references == 5407 and len(unique) == 2607

        total_parts = total_tiles = 0
        for index in range(311):
            q = ptr(0x44b710+4*index)
            assert take(q,4) == b'AGB!'
            n,tiles = u16(q+4),u16(q+6); parts = []
            for i in range(n):
                a0,a1,t,w,h = struct.unpack('<HHHBB',take(q+28+8*i,8))
                x,y = a1 & 511,a0 & 511
                if x>=256: x-=512
                if y>=256: y-=512
                parts.append(dict(x=x,y=y,tile=t,width=w,height=h,attribute0=a0,attribute1=a1))
            left=min(p['x'] for p in parts); top=min(p['y'] for p in parts)
            w=max(p['x']+p['width'] for p in parts)-left
            h=max(p['y']+p['height'] for p in parts)-top
            data=take(q+28+8*n,tiles*64); rows=bytearray(w*h)
            for p in parts:
                for y in range(p['height']):
                    for x in range(p['width']):
                        tile=p['tile']+(y//8)*(p['width']//8)+x//8
                        v=data[tile*64+(y%8)*8+x%8]
                        dst=(p['y']-top+y)*w+p['x']-left+x
                        if not rows[dst]: rows[dst]=v
            assert sha(rows) == reports['sprite']['image_sha256'][index]
            path=f'sprites/image-{index:03}.png'; original=f'original/sprites/image-{index:03}.agb'
            put(path,png(w,h,rows,palettes[2],True)); put(original,take(q,28+8*n+tiles*64))
            manifest['sprites'].append(dict(id=index,rom_offset=q,width=w,height=h,min_x=left,min_y=top,parts=parts,png=path,original=original))
            total_parts+=n; total_tiles+=tiles
        assert total_parts==1261 and total_tiles==6730
        manifest['counts']=dict(texture_images=666,actor_resources=142,nonempty_actor_resources=138,actor_frame_references=references,unique_actor_images=len(unique),actor_pngs=references*2,sprite_images=311,sprite_parts=total_parts,sprite_tiles=total_tiles,palettes=3)
        export_presentation(rom,put,manifest,png)
        signatures = {}
        for magic, expected in [(b'AGB!', {item['rom_offset'] for item in manifest['sprites']}),
                                (b'AGB*', set(unique))]:
            hits = {m.start() for m in re.finditer(re.escape(magic),rom)}
            extras = hits-expected
            assert expected <= hits
            assert extras == ({0xe020} if magic==b'AGB*' else set())
            signatures[magic.decode('ascii')] = dict(total_hits=len(hits),decoded_images=len(expected),
                non_resource_hits=[dict(rom_offset=x,reason='magic comparison constant in executable code') for x in sorted(extras)])
        manifest['signature_audit'] = signatures
        evidence_path=ROOT/'build/rom-leftovers/evidence.json'
        if evidence_path.exists():
            evidence=json.loads(evidence_path.read_text())
            assert evidence['rom_sha1']==SHA1
            put('investigation/rom-leftovers-evidence.json',evidence_path.read_bytes())
        validation_path=ROOT/'build/exports/presentation-validation.json'
        if validation_path.exists():
            validation=json.loads(validation_path.read_text())
            assert validation['status']=='PASS' and validation['rom_sha1']==SHA1
            assert validation['decoder_sha256']==sha((ROOT/'scripts/export_presentation_assets.py').read_bytes())
            put('validation/presentation.json',validation_path.read_bytes())
            put('validate_presentation_export.py',(ROOT/'scripts/validate_presentation_export.py').read_bytes())
        put('README.txt', '''Serious Sam Advance - original graphics export

Source: user-supplied European AENP ROM. See manifest.json for source SHA-1.
Expanded export: world art, hardware sprites, presentation art, fonts, palettes,
animation sequences, text and maps. This is a graphics/resource archive, not a
complete ROM dump: executable code and audio are outside its scope.

menus/: mapped Egypt/Rome backgrounds, NETRICSA frame, scrolling menu backdrop,
title logo and original tile atlases. Text is drawn at runtime using fonts/.
netricsa/: all 23 64x64 illustrations, plus a contact sheet.
story/: all eight 128x128 intro/story/ending pictures, plus a contact sheet.
Pictures 06 and 07 contain the ROM's actual placeholder text; this is not a decode error.
splashes/: all six full-screen logos/photos, including both hidden photographs.
fonts/: both fonts, all 252 glyphs, 16 palette-bank sheets per font, metrics and
byte-to-glyph map. Individual glyph PNGs use menu palette bank 4.
text/: all 252 strings for each of English/German/French, including control bytes.
sky/: original readable sky sampling windows and 60-row crops; see its README.
animations/: all 142 actor and 233 hardware-sprite sequences, including markers.
maps/: all 16 losslessly decompressed WOD0 maps and section/texture-name indexes.
investigation/: textures absent from the map tables, and prior hidden-feature
evidence if available. Unreferenced assets alone do not prove a scrapped feature.

textures/: 666 PNGs, two families and three original resolution levels.
actors/: all 5,407 frame references under BOTH world palette families. Shared
images intentionally repeat under resource IDs for convenient browsing.
sprites/: 311 assembled weapon/HUD/other hardware sprite images.
palettes/: exact original RGB555 little-endian data, JSON, GIMP palettes and swatches;
includes all 111 contiguous world palettes, menu palette and NETRICSA runtime edit.
original/: six original WOD0 archives, 2,607 unique compressed actor image spans,
and 311 original tiled sprite records, compressed/decoded presentation resources,
font metadata, animation words, pointer tables and compressed maps. Actor spans
run through the last consumed column packet; intervening padding is retained.
The whole bounded presentation asset region is also preserved, with gaps explicitly
inventoried in the manifest. These are original data, not PNGs.
manifest.json: dimensions, origins, resource/frame indices, sprite pieces, ROM
offsets, references to shared data and SHA-256 checksums for all payload files.

PNGs are lossless 8-bit indexed images at original resolution. Actor/sprite and
presentation overlay palette index 0 is transparent. Full-screen backgrounds,
splashes, sky and textures are opaque. RGB555 channels expand by bit replication.
World/sprite runtime palette entry 0 is cleared; raw palette data is exact.
World family 0: levels 0..5,12,13. Family 1: levels 6..11,14,15.
Actor origin offsets and sprite min_x/min_y are in the manifest; PNGs are not
aligned to a common canvas. Player base and clothing are separate resources;
the game composites them. PNGs are individual images, not animation playback.
Numeric IDs avoid inventing names for unidentified artwork. Stored sequences
are exported, but gameplay-dependent composition/timing is not animation playback.
Presentation decoding follows BIOS LZ77/RL, tiled 4bpp/8bpp OBJ layouts and BG maps
with horizontal/vertical flip flags. No smoothing, AI reconstruction or upscaling.

Validation: every texture and assembled sprite matches the existing native
validation hashes; all decoded actor frames match the native aggregate hash.
The complete ZIP is checked for CRC errors and every PNG is round-trip decoded.
All AGB! and AGB* image signatures are accounted for; the sole non-image AGB* hit
is a comparison constant in code. Presentation coverage and any preserved gaps
are recorded in manifest.json. This does not prove that every arbitrary byte
elsewhere in the ROM has been assigned a semantic meaning.
The supplied ROM is unchanged. Both included exporter Python files reproduce
this export in this project's scripts directory with Python 3.10+.
''')
        put('export_graphics_archive.py',Path(__file__).read_bytes())
        archive.writestr('manifest.json',json.dumps(manifest,indent=2))
    # Validate the final artifact itself, including decompressed PNG data lengths.
    png_count=0
    with zipfile.ZipFile(args.output) as archive:
        assert archive.testzip() is None
        assert len(archive.namelist()) == len(set(archive.namelist()))
        for name, metadata in manifest['files'].items():
            data=archive.read(name)
            assert len(data)==metadata['bytes'] and sha(data)==metadata['sha256']
            if not name.endswith('.png'): continue
            png_count+=1; pos=8; compressed=bytearray()
            w,h=struct.unpack_from('>II',data,16)
            while pos<len(data):
                size=struct.unpack_from('>I',data,pos)[0]; kind=data[pos+4:pos+8]; payload=data[pos+8:pos+8+size]
                assert zlib.crc32(kind+payload)&0xffffffff == struct.unpack_from('>I',data,pos+8+size)[0]
                if kind==b'IDAT': compressed.extend(payload)
                pos+=12+size
            rows=zlib.decompress(compressed)
            assert len(rows)==(w+1)*h and all(rows[y*(w+1)]==0 for y in range(h))
    assert hashlib.sha1(args.rom.read_bytes()).hexdigest()==SHA1
    checksum=sha(args.output.read_bytes())
    args.output.with_suffix('.zip.sha256').write_text(f'{checksum}  {args.output.name}\n')
    print(json.dumps(dict(archive=str(args.output),bytes=args.output.stat().st_size,pngs=png_count,sha256=checksum,counts=manifest['counts'],validation='PASS'),indent=2))


if __name__ == '__main__':
    main()
