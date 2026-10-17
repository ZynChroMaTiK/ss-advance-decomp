"""Embed original 16px GBA glyph bitmaps in selectable, wrapping web fonts.

PNG strikes avoid independently rasterized color masks. Solid outlines provide
a clean fallback at sizes without a bitmap strike.
Source sheets and character metrics are committed; rebuilding needs no ROM.
"""
from pathlib import Path
import hashlib,json,sys,io
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build/font-tools'))
from PIL import Image,ImageDraw,ImageFont
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont,newTable
from fontTools.ttLib.tables.sbixStrike import Strike
from fontTools.ttLib.tables.sbixGlyph import Glyph

OUT=ROOT/'site/assets/fonts'
UNIT=64
STRIKES=(16,20,24,32,40,48,60,64,72,80,96,128,144,192)

def outline(pixels, color=None):
    pen=TTGlyphPen(None)
    for y,row in enumerate(pixels):
        x=0
        while x<len(row):
            if row[x][3]==0 or (color is not None and row[x]!=color):x+=1;continue
            start=x;x+=1
            while x<len(row) and row[x][3] and (color is None or row[x]==color):x+=1
            left,right=start*UNIT,x*UNIT;top=(14-y)*UNIT;bottom=top-UNIT
            pen.moveTo((left,bottom));pen.lineTo((left,top));pen.lineTo((right,top));pen.lineTo((right,bottom));pen.closePath()
    return pen.glyph()

def bitmap(pixels):
    image=Image.new('RGBA',(len(pixels[0]),16));image.putdata([p for row in pixels for p in row]);return image

def verify_rendering(font, source_pixels):
    """Rasterize the actual saved font, not a reconstruction of its outlines."""
    # Test every source glyph through private-use aliases so Unicode layout
    # controls (notably soft hyphen) cannot suppress or reshape the test text.
    test_map={0xe000+i:name for i,name in enumerate(source_pixels)}
    for table in font['cmap'].tables:
        if table.isUnicode():table.cmap.update(test_map)
    font.flavor=None;buffer=io.BytesIO();font.save(buffer);count=0
    for size in (16,32,48,64):
        face=ImageFont.truetype(io.BytesIO(buffer.getvalue()),size)
        for code,name in test_map.items():
            source=bitmap(source_pixels[name]);source=source.resize((source.width*size//16,size),Image.Resampling.NEAREST)
            expected=Image.new('RGBA',(192,192));expected.paste(source,(16,128-14*size//16))
            actual=Image.new('RGBA',(192,192));ImageDraw.Draw(actual).text((16,128),chr(code),font=face,anchor='ls',embedded_color=True)
            assert actual.tobytes()==expected.tobytes(),f'Rendered glyph mismatch: {code} at {size}px'
            count+=1
    return count

def main():
    OUT.mkdir(exist_ok=True)
    metadata=json.loads((OUT/'gba-16px-metrics.json').read_text())
    report={'source':'Original Serious Sam Advance 16px font, exported ROM character map and palettes 03/04','validation':{},'files':{}}
    for variant in ('white','green'):
        sheet=Image.open(ROOT/'site/assets/theme'/f'{variant}.png').convert('RGBA')
        glyphs={'.notdef':TTGlyphPen(None).glyph(),'space':TTGlyphPen(None).glyph()}
        advances={'.notdef':(8*UNIT,0),'space':(8*UNIT,0)};source_pixels={}
        for item in metadata['glyphs']:
            i=item['id'];offset,width=item['metric_bytes'];width=(1 if width==205 else width)+1
            name=f'gba{i:03}';pixels=[[sheet.getpixel((16*(i%16)+offset+x,16*(i//16)+y)) if offset+x<16 else (0,0,0,0) for x in range(width)] for y in range(16)]
            # Normalize transparent palette entries before comparing RGBA.
            pixels=[[p if p[3] else (0,0,0,0) for p in row] for row in pixels]
            glyphs[name]=outline(pixels);advances[name]=(width*UNIT,0);source_pixels[name]=pixels
        charmap={code:f'gba{metadata["character_to_glyph"][code]:03}' for code in list(range(33,127))+list(range(160,256))}
        charmap[32]='space';charmap[160]='space'
        for code,ascii_code in {0x2018:39,0x2019:39,0x201c:34,0x201d:34,0x2013:45,0x2014:45,0x2011:45}.items():charmap[code]=charmap[ascii_code]
        fb=FontBuilder(1024,isTTF=True);fb.setupGlyphOrder(list(glyphs));fb.setupCharacterMap(charmap);fb.setupGlyf(glyphs)
        # Bearings must agree with outline bounds; zero bearings shifted masks.
        advances={name:(advance,getattr(glyphs[name],'xMin',0)) for name,(advance,_) in advances.items()}
        fb.setupHorizontalMetrics(advances);fb.setupHorizontalHeader(ascent=896,descent=-128,lineGap=0)
        family='Serious Advance '+variant.title()
        fb.setupNameTable({'familyName':family,'styleName':'Regular','uniqueFontIdentifier':family+' 2.0','fullName':family,'psName':'SeriousAdvance'+variant.title(),'version':'Version 2.0','copyright':'Original glyph artwork from Serious Sam Advance. Unofficial web conversion.'})
        fb.setupOS2(sTypoAscender=896,sTypoDescender=-128,sTypoLineGap=0,usWinAscent=896,usWinDescent=128);fb.setupPost()
        fb.font['sbix']=newTable('sbix')
        for size in STRIKES:
            strike=Strike(ppem=size);fb.font['sbix'].strikes[size]=strike
            for name,pixels in source_pixels.items():
                source=bitmap(pixels);data=io.BytesIO();source.resize((source.width*size//16,size),Image.Resampling.NEAREST).save(data,format='PNG')
                # sbix offsets are relative to the outline's lower-left origin.
                origin=(-128-getattr(glyphs[name],'yMin',0))*size//1024
                strike.glyphs[name]=Glyph(glyphName=name,originOffsetX=0,originOffsetY=origin,graphicType='png ',imageData=data.getvalue())
        fb.font['head'].created=fb.font['head'].modified=2082844800;fb.font.recalcTimestamp=False;fb.font.flavor='woff'
        path=OUT/f'serious-advance-{variant}.woff';fb.save(path)
        saved=TTFont(path)
        for size in STRIKES:
            for name,pixels in source_pixels.items():
                source=bitmap(pixels);expected=source.resize((source.width*size//16,size),Image.Resampling.NEAREST)
                decoded=Image.open(io.BytesIO(saved['sbix'].strikes[size].glyphs[name].imageData)).convert('RGBA')
                assert decoded.tobytes()==expected.tobytes(),name
                assert saved['hmtx'][name][0]==len(pixels[0])*UNIT
        assert saved.getBestCmap()==charmap
        rendered=verify_rendering(saved,source_pixels)
        report['validation'][variant]={'glyphs_pixel_exact':len(source_pixels),'bitmap_strikes':list(STRIKES),'rasterized_glyphs_pixel_exact':rendered,'character_mappings':len(charmap),'original_advances':'PASS','rgba_palette':'PASS','layered_color_masks':False}
        report['files'][path.name]={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bytes':path.stat().st_size}
    report['inputs']={str(p.relative_to(ROOT/'site')):hashlib.sha256(p.read_bytes()).hexdigest() for p in [OUT/'gba-16px-metrics.json',ROOT/'site/assets/theme/white.png',ROOT/'site/assets/theme/green.png']}
    (OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['validation']))

if __name__=='__main__':main()
