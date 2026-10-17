"""Decode SSA presentation art, maps and supporting graphics tables.

Addresses describe the verified AENP ROM, not guesses from generic signatures.
Only data are interpreted; the exporter never executes ROM instructions.
"""
import json
import struct
from pathlib import Path


def decompress(rom, offset):
    """Bounded BIOS type-10 LZ77/type-30 RL decode; return bytes and consumed span."""
    kind = rom[offset]
    assert kind in (0x10, 0x30), hex(offset)
    length = int.from_bytes(rom[offset+1:offset+4], 'little')
    assert 0 < length <= 0x400000
    out = bytearray(); p = offset+4
    while len(out) < length:
        control = rom[p]; p += 1
        if kind == 0x30:
            n = (control & 127) + (3 if control & 128 else 1)
            assert len(out)+n <= length
            if control & 128:
                out.extend(bytes([rom[p]])*n); p += 1
            else:
                assert p+n <= len(rom)
                out.extend(rom[p:p+n]); p += n
        else:
            for bit in range(7, -1, -1):
                if len(out) == length: break
                if control & (1 << bit):
                    pair = int.from_bytes(rom[p:p+2], 'big'); p += 2
                    distance = (pair & 4095)+1; n = (pair >> 12)+3
                    assert distance <= len(out) and len(out)+n <= length
                    for _ in range(n): out.append(out[-distance])
                else:
                    out.append(rom[p]); p += 1
    return bytes(out), p-offset


def untile(data, width, height, bpp=8):
    assert width % 8 == height % 8 == 0
    assert len(data) == width*height*bpp//8
    if bpp == 4:
        data = bytes(v for b in data for v in (b & 15, b >> 4))
    assert bpp in (4, 8)
    return bytes(data[((y//8)*(width//8)+x//8)*64+(y%8)*8+x%8]
                 for y in range(height) for x in range(width))


def tilemap(data, entries, width, height):
    assert len(entries) == width*height
    out = bytearray(width*height*64)
    for index, entry in enumerate(entries):
        tile = entry & 1023
        assert (tile+1)*64 <= len(data), (tile, len(data))
        for y in range(8):
            for x in range(8):
                sx = 7-x if entry & 1024 else x
                sy = 7-y if entry & 2048 else y
                out[(index//width*8+y)*width*8+index%width*8+x] = data[tile*64+sy*8+sx]
    return bytes(out)


def export(rom, put, manifest, png):
    u32 = lambda p: struct.unpack_from('<I', rom, p)[0]
    pointer = lambda p: u32(p)-0x08000000
    palette = list(struct.unpack_from('<256H', rom, 0xa8168))
    records = []; blocks = {}
    manifest['presentation'] = records

    def save_png(path, w, h, pixels, pal=palette, transparent=True):
        put(path, png(w, h, pixels, pal, transparent))
        return path

    def block(off):
        if off not in blocks:
            data, consumed = decompress(rom, off)
            path = f'original/presentation/{off+0x08000000:08X}'
            put(path+'.compressed', rom[off:off+consumed])
            put(path+'.decoded', data)
            rec = dict(rom_offset=off, compression='lz77' if rom[off]==16 else 'rl',
                       compressed_bytes=consumed, decoded_bytes=len(data),
                       compressed=path+'.compressed', decoded=path+'.decoded')
            records.append(rec); blocks[off] = data, rec
        return blocks[off]

    def sheet(path, images, w, h, columns, pal=palette, transparent=True):
        rows = (len(images)+columns-1)//columns
        pixels = bytearray(columns*w*rows*h)
        for i, item in enumerate(images):
            for y in range(h):
                start = ((i//columns*h+y)*columns+i%columns)*w
                pixels[start:start+w] = item[y*w:(y+1)*w]
        save_png(path, columns*w, rows*h, pixels, pal, transparent)

    for category, table, count, dim in [('netricsa',0x44cf48,23,64),('story',0x44cfa4,8,128)]:
        pictures = []
        put(f'original/tables/{category}-pointers.bin', rom[table:table+count*4])
        for i in range(count):
            off = pointer(table+i*4); data, rec = block(off)
            pixels = untile(data,dim,dim); pictures.append(pixels)
            path = save_png(f'{category}/{category}-{i:02}.png',dim,dim,pixels)
            rec.update(category=category, id=i, width=dim, height=dim, png=path, layout='8bpp 8x8 tiles, row-major')
            if category=='story' and i in (6,7):
                rec['note']='Original ROM placeholder artwork containing repeated intro picture #1 text; not a decoding error.'
        sheet(f'{category}/contact-sheet.png',pictures,dim,dim,6 if dim==64 else 4)

    splash_offsets = [0xa5df4,0xa4df4,0xa45c4,0xa3ef8,0xb7fa4,0xbd4dc]
    splash_names = ['boot-logo','global-star','croteam','climax','hidden-photo-1','hidden-photo-2']
    pictures = []
    for i,(off,name) in enumerate(zip(splash_offsets,splash_names)):
        data,rec = block(off); assert len(data)==240*160; pictures.append(data)
        path=save_png(f'splashes/{i:02}-{name}.png',240,160,data,transparent=False)
        rec.update(category='splashes',id=i,width=240,height=160,png=path,layout='linear 8bpp')
    sheet('splashes/contact-sheet.png',pictures,240,160,3,transparent=False)

    for off in (0xa8368,0xaaeec,0xaaf38,0xab39c,0xafe4c,0xb4c20,0xb4eec,0xb5434,0xb697c,0xb7a60):
        block(off)
    for off,name in [(0xab39c,'egypt'),(0xafe4c,'rome')]:
        data,rec = block(off)
        entries=struct.unpack('<600H',block(0xb4eec)[0])
        rec.update(category='menus',width=240,height=160,tilemap_rom_offset=0xb4eec,
                   png=save_png(f'menus/{name}-background.png',240,160,tilemap(data,entries,30,20),transparent=False))
        save_png(f'menus/{name}-unmapped-tiles.png',240,160,untile(data,240,160),transparent=False)
    frame,rec = block(0xaaf38)
    entries=struct.unpack('<600H',block(0xb4c20)[0])
    rec.update(category='menus',tilemap_rom_offset=0xb4c20,
               png=save_png('menus/netricsa-frame.png',240,160,tilemap(frame,entries,30,20)))
    net_palette = palette.copy(); net_palette[60]=0x1fe0
    net_palette[61:64]=[0x1280]*3
    save_png('menus/netricsa-frame-runtime-palette.png',240,160,tilemap(frame,entries,30,20),net_palette)
    save_png('menus/frame-tile-atlas.png',72,48,untile(frame,72,48))
    blank,rec = block(0xaaeec)
    rec.update(category='menus',png=save_png('menus/single-tile.png',8,8,untile(blank,8,8)))
    background,rec = block(0xa8368)
    entries=[((8-band)*8+j)&511 for band in range(8) for j in range(512)]
    put('original/presentation/menu-generated-map.u16le',struct.pack('<4096H',*entries))
    rec.update(category='menus',png=save_png('menus/scrolling-background-512x512.png',512,512,
                                          tilemap(background,entries,64,64),transparent=False))
    save_png('menus/scrolling-background-tile-atlas.png',256,128,untile(background,256,128),transparent=False)
    logo,rec = block(0xb5434)
    pieces=[untile(logo[i*4096:(i+1)*4096],64,64) for i in range(3)]
    path='menus/title-logo.png'; sheet(path,pieces,64,64,3)
    rec.update(category='menus',width=192,height=64,png=path,layout='three 64x64 tiled OBJ pieces')

    charmap=rom[0x44c264:0x44c364]
    put('original/fonts/character-map.bin',charmap)
    for off,dim,metric in [(0xb697c,16,0xb7dac),(0xb7a60,8,0xb7ea8)]:
        data,rec = block(off); size=dim*dim//2
        assert len(data)==126*size
        glyphs=[untile(data[i*size:(i+1)*size],dim,dim,4) for i in range(126)]
        metrics=rom[metric:metric+252]
        put(f'original/fonts/{dim}px-metrics.bin',metrics)
        metadata=dict(character_to_glyph=list(charmap),metrics_rom_offset=metric,glyphs=[])
        for bank in range(16):
            colored=[bytes(v+bank*16 if v else 0 for v in glyph) for glyph in glyphs]
            sheet(f'fonts/{dim}px/palette-{bank:02}-sheet.png',colored,dim,dim,16)
        for i,glyph in enumerate(glyphs):
            path=save_png(f'fonts/{dim}px/glyphs/{i:03}.png',dim,dim,bytes(v+64 if v else 0 for v in glyph))
            metadata['glyphs'].append(dict(id=i,metric_bytes=list(metrics[2*i:2*i+2]),
                                          character_bytes=[j for j,v in enumerate(charmap) if v==i],png=path))
        put(f'fonts/{dim}px/metrics.json',json.dumps(metadata,indent=2))
        rec.update(category='fonts',bpp=4,glyph_count=126,glyph_width=dim,glyph_height=dim)

    # Three complete 252-entry language tables; retain exact bytes/control codes.
    languages=[]
    for language,table in [('english',0x44c364),('german',0x44c754),('french',0x44cb44)]:
        items=[]
        for i in range(252):
            off=pointer(table+4*i); end=rom.index(0,off)
            data=rom[off:end]
            items.append(dict(id=i,rom_offset=off,bytes_hex=data.hex(),byte_preserving_latin1=data.decode('latin1')))
        put(f'text/{language}.json',json.dumps(items,indent=2,ensure_ascii=True))
        put(f'original/tables/{language}-pointers.bin',rom[table:table+1008])
        languages.append(dict(language=language,strings=len(items),rom_offset=table))
    manifest['languages']=languages

    # Preserve every palette in the contiguous bank, not just the two base palettes.
    for name,off,colors in [('menu',0xa8168,palette),('netricsa-runtime',None,net_palette)]+[
            (f'world-bank/{i:03}',0x454a5c+i*512,list(struct.unpack_from('<256H',rom,0x454a5c+i*512))) for i in range(111)]:
        raw=struct.pack('<256H',*colors)
        put(f'palettes/{name}.rgb555le',raw)
        put(f'palettes/{name}.json',json.dumps(dict(rom_offset=off,rgb555=colors),indent=2))
        rgb=[tuple(((c>>(5*k)&31)<<3)|((c>>(5*k)&31)>>2) for k in range(3)) for c in colors]
        put(f'palettes/{name}.gpl','GIMP Palette\nName: SSA '+name+'\nColumns: 16\n#\n'+''.join(
            f'{r:3} {g:3} {b:3}\tIndex {i}\n' for i,(r,g,b) in enumerate(rgb)))
        save_png(f'palettes/{name}.png',192,192,bytes((y//12)*16+x//12 for y in range(192) for x in range(192)),colors,False)
        manifest['palettes'].append(dict(name=name,rom_offset=off))

    for family,off in enumerate((0x46285c,0x46dc5c)):
        colors=list(struct.unpack_from('<256H',rom,0x454a5c+family*512)); colors[0]=0
        data=rom[off:off+768*120]
        put(f'original/sky/family-{family}-sampling-window.idx8',data)
        save_png(f'sky/family-{family}-sampling-window.png',768,120,data,colors,False)
        save_png(f'sky/family-{family}-first-60-rows.png',768,60,data[:768*60],colors,False)
    put('sky/README.txt','These are the complete original A98C readable sampling windows, 768 bytes per row.\n'
        'The two 120-row ROM windows overlap by 60 rows. They are not two independent sky pictures.\n'
        'The first-60-rows previews are crops, not a claim about a separate resource format.\n')

    animations={}
    for name,base,words,table,stride,count in [('actors',0x449728,772,0x449d30,12,142),
                                             ('sprites',0x44af4c,528,0x44b36c,4,233)]:
        data=rom[base:base+words*2]; sequences=[]
        put(f'original/animations/{name}.u16le',data)
        for i in range(count):
            off=pointer(table+i*stride); p=off; values=[]
            assert base<=off<base+len(data)
            while p<base+len(data):
                v=struct.unpack_from('<H',rom,p)[0]; p+=2; values.append(v)
                if v>=0xfffe: break
            assert values[-1]>=0xfffe
            sequences.append(dict(id=i,rom_offset=off,words=values))
        put(f'animations/{name}.json',json.dumps(sequences,indent=2)); animations[name]=count
    manifest['animations']=animations
    put('animations/README.txt','Sequences retain original 16-bit words. 0xFFFF loops; 0xFFFE holds.\n'
        '0xFFFA..0xFFFD are event markers. Actor frames and hardware sprite frames use separate tables.\n'
        'Frame period is runtime state, not encoded in these sequences.\n')

    maps=[]
    sections=[('things',6,0x1c,12),('textures',8,0x20,16),('sectors',10,0x24,20),
              ('nodes',12,0x28,32),('leaves',14,0x2c,8),('segments',16,0x30,16),
              ('sides',18,0x34,12),('lines',20,0x38,40),('vertices',22,0x3c,4)]
    for i in range(16):
        off=pointer(0x44aef4+4*i); data,consumed=decompress(rom,off)
        assert data[:4]==b'WOD0' and struct.unpack_from('<H',data,4)[0]*4==len(data)
        put(f'original/maps/map-{i:02}.lz77',rom[off:off+consumed])
        put(f'maps/map-{i:02}.wod0',data)
        metadata=dict(id=i,rom_offset=off,bytes=len(data),sections={})
        for name,cn,ad,size in sections:
            n=struct.unpack_from('<H',data,cn)[0]; pos=struct.unpack_from('<I',data,ad)[0]
            assert pos+n*size<=len(data)
            metadata['sections'][name]=dict(count=n,offset=pos,record_bytes=size)
        t=metadata['sections']['textures']
        metadata['texture_names']=[data[t['offset']+j*16:t['offset']+j*16+8].rstrip(b'\0').decode('ascii') for j in range(t['count'])]
        put(f'maps/map-{i:02}.json',json.dumps(metadata,indent=2)); maps.append(metadata)
    manifest['maps']=maps
    unused=[]
    for family in range(2):
        levels=([0,1,2,3,4,5,12,13] if family==0 else [6,7,8,9,10,11,14,15])
        names={name.upper() for i in levels for name in maps[i]['texture_names']}
        for texture in manifest['textures']:
            if texture['family']==family and texture['mip']==0 and texture['name'].upper() not in names:
                unused.append(dict(family=family,name=texture['name'],png=texture['png']))
    put('investigation/map-unreferenced-textures.json',json.dumps(dict(
        meaning='Absent from all original map texture-name tables in this family. This alone does not prove a scrapped feature.',
        candidates=unused),indent=2))
    for name,off,length in [('actor-descriptors',0x449d2c,142*12),('sprite-pointers',0x44b710,311*4),
                            ('map-and-texture-pointers',0x44aef4,22*4),('sprite-animation-pointers',0x44b36c,233*4)]:
        put(f'original/tables/{name}.bin',rom[off:off+length])

    # Keep inter-resource bytes, including alignment and font metrics, inspectable.
    # This is preservation of a bounded asset region, not a substitute for decoding it.
    region_start=0x8ca28; region_end=0xc3168
    put('original/presentation/0808CA28-080C3168.region.bin',rom[region_start:region_end])
    spans=[(off,off+rec['compressed_bytes'],'compressed-resource') for off,(_,rec) in blocks.items()]
    spans += [(0xa8168,0xa8368,'palette'),(0xb7dac,0xb7fa4,'font-metrics')]
    spans.sort(); gaps=[]; cursor=region_start
    for lo,hi,kind in spans:
        assert lo>=cursor, (hex(lo),hex(cursor))
        if lo>cursor:
            padding=rom[cursor:lo]
            gaps.append(dict(rom_offset=cursor,bytes=len(padding),hex=padding.hex() if len(padding)<=32 else None,
                             classification='alignment' if len(padding)<=3 else 'unclassified-preserved'))
        cursor=hi
    if cursor<region_end:
        padding=rom[cursor:region_end]
        gaps.append(dict(rom_offset=cursor,bytes=len(padding),hex=padding.hex(),
                         classification='alignment' if len(padding)<=3 else 'unclassified-preserved'))
    assert all(g['classification']=='alignment' and set(bytes.fromhex(g['hex']))<={0} for g in gaps)
    manifest['presentation_coverage']=dict(start=region_start,end=region_end,blocks=len(blocks),gaps=gaps,
        note='All bytes of this graphics region are preserved; gaps are listed explicitly. Address ranges are end-exclusive.')
    manifest['counts'].update(presentation_compressed_blocks=len(blocks),netricsa_illustrations=23,
        story_pictures=8,splash_pictures=6,font_glyphs=252,font_palette_sheets=32,
        palettes=len(manifest['palettes']),maps=16,actor_animation_sequences=142,sprite_animation_sequences=233)
    put('export_presentation_assets.py',Path(__file__).read_bytes())
