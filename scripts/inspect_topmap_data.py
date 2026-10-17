from pathlib import Path
import struct, json, importlib.util
from collections import Counter
from export_presentation_assets import decompress
ROOT=Path(__file__).resolve().parents[1]
r=(ROOT/'orig/SSA.gba').read_bytes()
u16=lambda b,o:struct.unpack_from('<H',b,o)[0]
u32=lambda b,o:struct.unpack_from('<I',b,o)[0]
def text(i):
 p=u32(r,0x44c364+4*i)-0x8000000
 return r[p:r.index(0,p)].decode('latin1')
print('MODULES', {n:bool(importlib.util.find_spec(n)) for n in ['PIL','cairosvg','matplotlib','shapely']})
print('TEXT',[(i,text(i)) for i in range(252) if i in range(48,64) or i in range(139,172) or 'secret' in text(i).lower()])
print('DEFS',[(i,struct.unpack_from('<4H',r,0x44a3d4+8*i)) for i in range(64)])
for level in range(16):
 d,_=decompress(r,u32(r,0x44aef4+4*level)-0x8000000)
 sec=u32(d,0x24); line=u32(d,0x38); thing=u32(d,0x1c)
 print('MAP',level,'things',Counter(u16(d,thing+12*i+6) for i in range(u16(d,6))),
       'sector fields',[(off,dict(Counter(u16(d,sec+20*i+off) for i in range(u16(d,10))))) for off in [4,6,16,18]],
       'line flags',dict(Counter(u16(d,line+40*i+26) for i in range(u16(d,20)))),
       'special',dict(Counter(u16(d,line+40*i+36) for i in range(u16(d,20)))))
