"""Local ROM audio reconnaissance; reads only the supplied original image."""
from pathlib import Path
import re,struct
import sys
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
r=Path(__file__).resolve().parents[1]
rom=(r/'orig/SSA.gba').read_bytes()
for m in re.finditer(rb'[ -~]{8,}',rom):
    if any(s in m.group().lower() for s in (b'audio',b'sound',b'gax',b'krawall',b'music',b'modul',b'mixer',b'sappy')):
        print(f'{m.start():08x} {m.group()!r}')
def u32(off):return struct.unpack_from('<I',rom,off)[0]
for song in range(160):
    row=0x3e99d0+8*song;ptr=u32(row)-0x08000000
    if not 0<=ptr<len(rom)-12:break
    count,blocks,priority,reverb=rom[ptr:ptr+4];tones=u32(ptr+4)-0x08000000
    if not count:continue
    if count>16 or not 0<=tones<len(rom)-12:break
    if len(sys.argv)>1 and song not in {int(n,0) for n in sys.argv[1:]}:continue
    print(f'song {song:02x}: header={ptr:06x} tracks={count} player={struct.unpack_from("<H",rom,row+4)[0]} priority={priority} reverb={reverb} bank={tones:06x}')
    for track in range(count):
        p=u32(ptr+8+track*4)-0x08000000
        print(f'  track{track} @{p:06x} {rom[p:p+52].hex(" ")}')
        # Early VOICE command is enough to identify most one-note SFX.
        data=rom[p:p+30]
        if 0xbd in data:
            voice=data[data.index(0xbd)+1];v=tones+12*voice;wave=u32(v+4)-0x08000000
            print(f'  voice{voice} @{v:06x}: {rom[v:v+12].hex(" ")} wave={wave:06x} header={rom[wave:wave+16].hex(" ") if 0<=wave<len(rom)-16 else "invalid"}')
print('table tail',rom[0x3e99d0+0x68*8:0x3e99d0+0x70*8].hex(' '))
print('level song IDs',struct.unpack_from('<16H',rom,0x44a9dc))
print('music player records',rom[0x3e99a0:0x3e99d0].hex(' '))
for i in range(136):
    h=u32(0x3e99d0+8*i)-0x08000000;player=struct.unpack_from('<H',rom,0x3e99d0+8*i+4)[0]
    if not 0<=h<len(rom)-4 or rom[h]>16 or player>=4:print('invalid row',i,hex(h),player)
    elif rom[h] and (not 0<=u32(h+4)-0x08000000<len(rom)-12 or any(not 0<=u32(h+8+4*j)-0x08000000<len(rom) for j in range(rom[h]))):print('invalid header',i,hex(h),rom[h:h+12].hex(' '))
dis=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
for off in range(0,0xc3000,2):
    a,b=struct.unpack_from('<HH',rom,off)
    if a&0xf800!=0xf000 or b&0xf800!=0xf800:continue
    displacement=((a&0x7ff)<<12)|((b&0x7ff)<<1)
    if displacement&0x400000:displacement-=0x800000
    if 0x08000000+off+4+displacement!=0x08008d30:continue
    print('music request caller',hex(0x08000000+off))
    for ins in dis.disasm(rom[off-20:off+4],0x08000000+off-20):print(f' {ins.address:08x}: {ins.mnemonic} {ins.op_str}')
