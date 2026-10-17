"""Convert original 16px GBA glyphs into selectable, wrapping color web fonts.

No smoothing or substitute typeface: each source pixel becomes a square outline.
Source sheets and character metrics are committed; rebuilding needs no ROM.
"""
from pathlib import Path
import hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build/font-tools'))
from PIL import Image
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.colorLib.builder import buildCOLR,buildCPAL
from fontTools.ttLib import TTFont

OUT=ROOT/'site/assets/fonts'
UNIT=64

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

def verify_pixels(font, name, expected):
    """Decode the saved WOFF's layer rectangles and compare every RGBA pixel."""
    actual=[[(0,0,0,0)]*len(expected[0]) for _ in range(16)]
    palette=font['CPAL'].palettes[0]
    for layer in font['COLR'].ColorLayers.get(name,[]):
        c=palette[layer.colorID];color=(c.red,c.green,c.blue,c.alpha)
        glyph=font['glyf'][layer.name];coordinates,ends,_=glyph.getCoordinates(font['glyf']);start=0
        for end in ends:
            points=list(coordinates[start:end+1]);start=end+1
            assert len(points)==4
            x0=min(x for x,y in points)//UNIT;x1=max(x for x,y in points)//UNIT
            y0=14-max(y for x,y in points)//UNIT;y1=14-min(y for x,y in points)//UNIT
            for y in range(y0,y1):
                for x in range(x0,x1):actual[y][x]=color
    assert actual==expected,name

def main():
    OUT.mkdir(exist_ok=True)
    metadata=json.loads((OUT/'gba-16px-metrics.json').read_text())
    report={'source':'Original Serious Sam Advance 16px font, exported ROM character map and palettes 03/04','validation':{},'files':{}}
    for variant in ('white','green'):
        sheet=Image.open(ROOT/'site/assets/theme'/f'{variant}.png').convert('RGBA')
        palette=sorted({pixel for pixel in sheet.get_flattened_data() if pixel[3]})
        glyphs={'.notdef':TTGlyphPen(None).glyph(),'space':TTGlyphPen(None).glyph()}
        advances={'.notdef':(8*UNIT,0),'space':(8*UNIT,0)};layers={};source_pixels={}
        for item in metadata['glyphs']:
            i=item['id'];offset,width=item['metric_bytes'];width=(1 if width==205 else width)+1
            name=f'gba{i:03}';pixels=[[sheet.getpixel((16*(i%16)+offset+x,16*(i//16)+y)) if offset+x<16 else (0,0,0,0) for x in range(width)] for y in range(16)]
            # Normalize transparent palette entries before comparing RGBA.
            pixels=[[p if p[3] else (0,0,0,0) for p in row] for row in pixels]
            glyphs[name]=outline(pixels);advances[name]=(width*UNIT,0);source_pixels[name]=pixels
            colors=sorted({p for row in pixels for p in row if p[3]})
            if colors:layers[name]=[]
            for color in colors:
                layer=f'{name}.color{palette.index(color)}';glyphs[layer]=outline(pixels,color);advances[layer]=(width*UNIT,0);layers[name].append((layer,palette.index(color)))
        charmap={code:f'gba{metadata["character_to_glyph"][code]:03}' for code in list(range(33,127))+list(range(160,256))}
        charmap[32]='space';charmap[160]='space'
        for code,ascii_code in {0x2018:39,0x2019:39,0x201c:34,0x201d:34,0x2013:45,0x2014:45,0x2011:45}.items():charmap[code]=charmap[ascii_code]
        fb=FontBuilder(1024,isTTF=True);fb.setupGlyphOrder(list(glyphs));fb.setupCharacterMap(charmap);fb.setupGlyf(glyphs);fb.setupHorizontalMetrics(advances);fb.setupHorizontalHeader(ascent=896,descent=-128,lineGap=0)
        family='Serious Advance '+variant.title()
        fb.setupNameTable({'familyName':family,'styleName':'Regular','uniqueFontIdentifier':family+' 1.0','fullName':family,'psName':'SeriousAdvance'+variant.title(),'version':'Version 1.0','copyright':'Original glyph artwork from Serious Sam Advance. Unofficial web conversion.'})
        fb.setupOS2(sTypoAscender=896,sTypoDescender=-128,sTypoLineGap=0,usWinAscent=896,usWinDescent=128);fb.setupPost()
        fb.font['COLR']=buildCOLR(layers,version=0);fb.font['CPAL']=buildCPAL([[tuple(c/255 for c in color) for color in palette]])
        fb.font['head'].created=fb.font['head'].modified=2082844800;fb.font.recalcTimestamp=False;fb.font.flavor='woff'
        path=OUT/f'serious-advance-{variant}.woff';fb.save(path)
        saved=TTFont(path)
        for name,pixels in source_pixels.items():verify_pixels(saved,name,pixels);assert saved['hmtx'][name][0]==len(pixels[0])*UNIT
        assert saved.getBestCmap()==charmap
        report['validation'][variant]={'glyphs_pixel_exact':len(source_pixels),'character_mappings':len(charmap),'original_advances':'PASS','rgba_palette':'PASS'}
        report['files'][path.name]={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bytes':path.stat().st_size}
    report['inputs']={str(p.relative_to(ROOT/'site')):hashlib.sha256(p.read_bytes()).hexdigest() for p in [OUT/'gba-16px-metrics.json',ROOT/'site/assets/theme/white.png',ROOT/'site/assets/theme/green.png']}
    (OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['validation']))

if __name__=='__main__':main()
