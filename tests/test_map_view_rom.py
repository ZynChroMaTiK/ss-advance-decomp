"""Original pre-level map draw submissions and fade/input, isolated ROM oracle.

ARM execution is test-only. Production decodes artwork and runs native C.
"""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC

r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes();b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/map_view.c'),
 str(r/'src/map_assets.c'),'-o',str(b/'libssa_map_view.so')],check=True)
class Glyph(C.Structure):
    _fields_=[('x',C.c_int16),('y',C.c_int16),('glyph',C.c_uint8),('font',C.c_uint8),('palette',C.c_uint8)]
class View(C.Structure):
    _fields_=[('rom',C.c_void_p),('rom_size',C.c_size_t),('background',C.c_uint8*38400),('fonts',C.c_uint8*20160),
      ('palette',C.c_uint16*256),('glyphs',Glyph*128),('loaded',C.c_uint),('level',C.c_uint),('language',C.c_uint),
      ('difficulty',C.c_uint),('glyph_count',C.c_uint),('frame',C.c_uint32),('fade_volume',C.c_int),
      ('fade_step',C.c_int),('fading',C.c_uint),('leaving',C.c_uint),('done',C.c_uint)]
lib=C.CDLL(str(b/'libssa_map_view.so'));raw=C.create_string_buffer(rom)
lib.ssa_map_view_init.argtypes=[C.POINTER(View),C.c_void_p,C.c_size_t,C.c_uint,C.c_uint,C.c_uint]
lib.ssa_map_view_step.argtypes=[C.POINTER(View),C.c_uint]
lib.ssa_map_view_draw.argtypes=[C.POINTER(View),C.POINTER(C.c_uint16),C.c_size_t]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for address,size in ((0x08000000,len(rom)),(0x02000000,0x40000),(0x03000000,0x10000),
                     (0x04000000,0x1000),(0x05000000,0x1000),(0x06000000,0x20000),(0x07000000,0x1000)):
    uc.mem_map(address,size)
uc.mem_write(0x08000000,rom)
heap=0x02002000;stop=0x087ff000;events=[]
def word(at):return struct.unpack('<I',uc.mem_read(at,4))[0]
def cstring(at):
    data=bytearray()
    while True:
        c=bytes(uc.mem_read(at,1));at+=1
        if c==b'\0':return bytes(data)
        data+=c
def lz(at):
    data=bytearray();length=word(at)>>8;at+=4
    while len(data)<length:
        flags=uc.mem_read(at,1)[0];at+=1
        for bit in range(7,-1,-1):
            if len(data)==length:break
            if flags&(1<<bit):
                a,c=uc.mem_read(at,2);at+=2;distance=(((a&15)<<8)|c)+1
                for _ in range((a>>4)+3):data.append(data[-distance])
            else:data+=uc.mem_read(at,1);at+=1
    return bytes(data)
def hook(u,pc,size,ctx):
    global heap
    if pc not in (0x080033d8,0x08003444,0x080033fc,0x0800341c,0x080c531c,0x080c5320,0x080c5324,0x080c5764,0x080c56c8,0x08003028,0x08008eb0,0x08008e74):return
    a=u.reg_read(UC_ARM_REG_R0);v=u.reg_read(UC_ARM_REG_R1)
    if pc in (0x080033d8,0x08003444):
        u.reg_write(UC_ARM_REG_R0,heap);heap+=(a+3)&~3;assert heap<0x0203e000
    elif pc==0x080c5324:u.mem_write(v,lz(a))
    elif pc in (0x080c531c,0x080c5320):
        mode=u.reg_read(UC_ARM_REG_R2);unit=4 if pc==0x080c5320 or mode&(1<<26) else 2;count=mode&0x1fffff
        u.mem_write(v,bytes(u.mem_read(a,unit))*count if mode&(1<<24) else bytes(u.mem_read(a,unit*count)))
    elif pc==0x080c5764:u.mem_write(a,cstring(v)+b'\0')
    elif pc==0x080c56c8:
        if v in (0x080cc2dc,0x080cc32c):
            count=3 if v==0x080cc2dc else 18
            args=[u.reg_read(UC_ARM_REG_R2),u.reg_read(UC_ARM_REG_R3)]
            args+=list(struct.unpack('<'+str(count-2)+'I',u.mem_read(u.reg_read(UC_ARM_REG_SP),4*(count-2))))
            values=tuple(cstring(x) if count==3 else x if x<0x80000000 else x-0x100000000 for x in args)
            u.mem_write(a,cstring(v)%values+b'\0')
        else:u.mem_write(a,cstring(u.reg_read(UC_ARM_REG_R2))+b': '+cstring(u.reg_read(UC_ARM_REG_R3))+b'\0')
    elif pc in (0x08008eb0,0x08008e74):events.append((pc,a))
    u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
def dma(u,access,address,size,value,ctx):
    if address!=0x040000dc:return
    count=value&0xffff;unit=4 if value&(1<<26) else 2
    source=word(0x040000d4);dest=word(0x040000d8)
    data=bytes(u.mem_read(source,unit))*(count if (value>>23)&3==2 else 1)
    if (value>>23)&3!=2:data=bytes(u.mem_read(source,count*unit))
    u.mem_write(dest,data)
for address in (0x080033d8,0x08003444,0x080033fc,0x0800341c,0x080c531c,0x080c5320,0x080c5324,0x080c5764,0x080c56c8,0x08003028,0x08008eb0,0x08008e74):
    uc.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
uc.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000dc)
def call(pc,*args):
    uc.reg_write(UC_ARM_REG_SP,0x03007c00);uc.reg_write(UC_ARM_REG_LR,stop|1)
    for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(reg,value)
    uc.mem_write(0x03007c00,struct.pack('<4I',*(list(args[4:])+[0]*4)[:4]))
    uc.emu_start(pc|1,stop,count=1000000)
    assert uc.reg_read(UC_ARM_REG_PC)==stop,hex(uc.reg_read(UC_ARM_REG_PC))
    return uc.reg_read(UC_ARM_REG_R0)
def setup(level,language,difficulty):
    global heap
    heap=0x02002000
    uc.mem_write(0x02000000,bytes(0x40000));uc.mem_write(0x03000000,bytes(0x10000))
    uc.mem_write(0x0300572c,struct.pack('<I',0x02000000))
    uc.mem_write(0x02000000,struct.pack('<H',language));uc.mem_write(0x02000010,struct.pack('<H',level))
    uc.mem_write(0x0200001a,bytes([difficulty]));uc.mem_write(0x0200002c,b'\1')
    call(0x08015a14);call(0x08016060)
    call(0x08018774,0x080b697c,0x080b7dac,16,0)
    call(0x08018774,0x080b7a60,0x080b7ea8,8,1)
    call(0x0801a9a4)
cases=background_pixels=fade_cases=composed_pixels=0;out=(C.c_uint16*38400)()
for level in range(12):
 for language in range(3):
  for difficulty in range(3):
    setup(level,language,difficulty);v=View()
    assert lib.ssa_map_view_init(C.byref(v),raw,len(rom),level,language,difficulty)==1
    # Original setup's VRAM tilemap/tile data and palettes independently establish every background pixel.
    vram=bytes(uc.mem_read(0x06000000,0x10000));pal=struct.unpack('<256H',uc.mem_read(0x05000000,512))
    expected=[]
    for y in range(160):
     for x in range(240):
      entry=struct.unpack_from('<H',vram,0xe000+2*((y//8)*32+x//8))[0];tx=x&7;ty=y&7
      if entry&1024:tx=7-tx
      if entry&2048:ty=7-ty
      expected.append(vram[(entry&1023)*64+ty*8+tx])
    assert list(v.background)==expected and list(v.palette)==list(pal)
    background_pixels+=38400
    for frame in (1,31,32,63,64,319,320,352):
      v.frame=frame;v.fade_volume=0;v.fading=0
      assert lib.ssa_map_view_draw(C.byref(v),out,38400)==1
      uc.mem_write(0x02001000,struct.pack('<I',frame-1));uc.mem_write(0x03005730,b'\0')
      call(0x0801aaa0,0x02001000)
      count=uc.mem_read(0x03005730,1)[0];records=bytes(uc.mem_read(word(0x03005740),count*20))
      native=[(g.x&65535,g.y&65535,(504+g.glyph if g.font else g.glyph*4),g.palette) for g in v.glyphs[:v.glyph_count]]
      original=[(*struct.unpack_from('<HH',records,n*20),struct.unpack_from('<H',records,n*20+14)[0],(struct.unpack_from('<H',records,n*20+16)[0]>>4)&15) for n in range(count)]
      assert native==original,(level,language,difficulty,frame,native,original)
      if level in (0,1,6) and difficulty==1 and frame in (32,320):
        # Independent display reference from original hardware OAM/VRAM, not
        # native font metrics or native glyph rasterization.
        call(0x08015b54);call(0x08015c2c)
        oam=bytes(uc.mem_read(0x07000000,1024));tiles=bytes(uc.mem_read(0x06010000,32768))
        obj=struct.unpack('<256H',uc.mem_read(0x05000200,512));image=[pal[c] for c in expected]
        for n in range(127,-1,-1):
          a0,a1,a2=struct.unpack_from('<3H',oam,n*8)
          if a0&0x300==0x200:continue
          assert not a0&0x2300 and a0>>14==0,(hex(a0),hex(a1),hex(a2))
          size=8<<(a1>>14);sx=a1&511;sy=a0&255
          if sx>=240:sx-=512
          if sy>=160:sy-=256
          for dy in range(size):
           for dx in range(size):
            x=sx+dx;y=sy+dy
            if not (0<=x<240 and 0<=y<160):continue
            tx=size-1-dx if a1&4096 else dx;ty=size-1-dy if a1&8192 else dy
            at=(a2&1023)*32+((ty//8)*(size//8)+tx//8)*32+(ty&7)*4+(tx&7)//2
            c=(tiles[at]>>((tx&1)*4))&15
            if c:image[y*240+x]=obj[((a2>>12)&15)*16+c]
        assert list(out)==image,(level,language,difficulty,frame,'display mismatch')
        composed_pixels+=38400
      cases+=1
    if language==0 and difficulty==1:
      # Replay the real fade/input ordering, with a repeated held/pressed fixture and long wait.
      for fire_at in (0,20,80,400):
        v.frame=0;v.fade_volume=0xf00;v.fade_step=-64;v.fading=1;v.leaving=v.done=0
        uc.mem_write(0x02001000,bytes(4));uc.mem_write(0x02000019,b'\0');call(0x08015614,2,64,17)
        for tick in range(fire_at+63):
          fire=tick>=fire_at;events.clear()
          call(0x08015698);uc.mem_write(0x030042f0,struct.pack('<I',int(fire)<<16));uc.mem_write(0x03005730,b'\0')
          call(0x0801aaa0,0x02001000)
          first=lib.ssa_map_view_step(C.byref(v),fire)
          state=struct.unpack('<5H',uc.mem_read(0x03005720,10))
          assert (v.fade_volume,v.fading,v.leaving)==(state[2],state[4],uc.mem_read(0x02000019,1)[0])
          assert first==(tick==fire_at) and events==([(0x08008eb0,13),(0x08008e74,5)] if first else [])
          assert v.done==(tick==fire_at+61 or tick>fire_at+61)
          fade_cases+=1
          if v.done:break
      v.frame=320;v.fade_volume=0;v.fading=v.leaving=v.done=0
      assert lib.ssa_map_view_draw(C.byref(v),out,38400)==1
      if level in (0,1,6):
        rgb=bytes(c for pixel in out for c in ((pixel&31)*255//31,((pixel>>5)&31)*255//31,((pixel>>10)&31)*255//31))
        (b/f'map-view-{level}.ppm').write_bytes(b'P6\n240 160\n255\n'+rgb)
report={'result':'pass','draw_submission_rom_cases':cases,'background_rom_pixels':background_pixels,
 'composed_rom_pixels':composed_pixels,'fade_input_rom_frames':fade_cases,'test_only_arm_execution':True,'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest()
 for p in (r/'src/map_view.c',r/'include/ssa/map_view.h',r/'src/map_assets.c',Path(__file__))}}
(r/'analysis/map-view-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
