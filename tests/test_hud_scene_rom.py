"""Whole original F84C initialization and F85C/E460/DBDC/FCE4/E358 frames.

Uses actual original handlers, animation, cache allocation, sorting and transfer
routines. Only BIOS/DMA memory copies are modeled. Input-state3 deliberately
selects original F150's no-aim/fire branch. Original undefined previous-frame
sentinel cache probes observe explicitly zeroed unrelated memory. Native queue
submission is outside hud_scene's scope; it is not compared here.
"""
from pathlib import Path
import ast,ctypes as C,hashlib,json,math,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'));fixture=ROOT/'tests/test_hud_scene_rom.c'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(fixture),*map(str,sources),'-o',str(B/'libssa_hud_scene.so')],check=True)
lib=C.CDLL(str(B/'libssa_hud_scene.so'));lib.start.argtypes=[C.c_void_p,C.c_size_t,C.c_uint]
lib.step.argtypes=[C.c_uint];lib.output.argtypes=[C.c_uint];lib.output.restype=C.c_void_p
lib.display.argtypes=[C.c_void_p,C.c_uint,C.c_void_p]
def native(kind,size):return C.string_at(lib.output(kind),size)
nodes=[n for n in ast.parse((ROOT/'tests/test_display.py').read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='reference']
exec(compile(ast.Module(nodes,type_ignores=[]),'independent hardware pixel reference','exec'))
class Entry(C.Structure):_fields_=[('a0',C.c_uint16),('a1',C.c_uint16),('a2',C.c_uint16),('affine',C.c_uint16)]
class Palettes:pass
assets=Palettes();shapes=[[(8,8),(16,16),(32,32),(64,64)],[(16,8),(32,8),(32,16),(64,32)],[(8,16),(8,32),(16,32),(32,64)]]
# Controlled indexed background exercises transparency/priority. Real-map RGB
# composition is tested separately by test_hud_scene.py, without emulation.
world=bytes((x//5+y//7)%256 for y in range(120) for x in range(120));pixels=(C.c_uint16*38400)()
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(ROM)),(0x02000000,0x100000),(0x03000000,0x10000),(0x04000000,0x10000),(0x06000000,0x20000)):u.mem_map(at,size)
u.mem_write(0x08000000,ROM)
OBJ,ACTIVE,FREE,ENTRIES,CBFREE,CBUSED,IMAGES,RING,GAME,PLAYER,LEVEL,BODY,OUT=(0x02000000,0x02002000,0x02002400,0x02003000,0x02005000,0x02005400,0x02005800,0x02005c00,0x02006000,0x02007000,0x02008000,0x02009000,0x0200a000)
STOP=0x087ff000
def put(at,fmt,*v):u.mem_write(at,struct.pack('<'+fmt,*v))
def get(at,fmt='I'):return struct.unpack('<'+fmt,u.mem_read(at,struct.calcsize('<'+fmt)))[0]
def copy(src,dst,control):
    unit=4 if control&0x4000000 else 2;n=(control&0x1fffff)*unit
    raw=bytes(u.mem_read(src,unit))*(n//unit) if control&0x1000000 else bytes(u.mem_read(src,n))
    if 0x06010000<=dst and dst+n<=0x06020000:
        for at in range(0,n,64):u.mem_write(0x06010000+((dst-0x06010000+at)&32767),raw[at:at+64])
    else:u.mem_write(dst,raw)
def bios(m,at,size,ctx):
    copy(*[m.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)])
    m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
def dma(m,access,at,size,v,ctx):
    if v&0x80000000:copy(get(0x040000d4),get(0x040000d8),v&0x7fffffff)
u.hook_add(UC_HOOK_CODE,bios,begin=0x080c5320,end=0x080c5320)
u.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000dc)
def call(at):
    u.reg_write(UC_ARM_REG_SP,0x03007e00);u.reg_write(UC_ARM_REG_LR,STOP|1)
    u.emu_start(at|1,STOP,count=3000000)
    assert u.reg_read(UC_ARM_REG_PC)==STOP,hex(u.reg_read(UC_ARM_REG_PC))
def initialize(level):
    u.mem_write(0x02000000,bytes(0x100000));u.mem_write(0x03000000,bytes(0x8000));u.mem_write(0x06000000,bytes(0x20000))
    u.mem_write(0x030000f0,ROM[0x370:0x380])
    for at,val in ((0x0300559c,OBJ),(0x030055c0,ACTIVE),(0x030055ac,FREE),(0x03004010,GAME),(0x03004648,PLAYER),(0x0300572c,LEVEL),(0x03005580,ENTRIES),(0x03005574,CBFREE),(0x03005570,CBUSED),(0x03005578,IMAGES),(0x03005568,RING),(0x03005560,RING),(0x03005584,RING),(0x03005594,OUT)):
        put(at,'I',val)
    put(GAME+16,'B',1);put(PLAYER+188,'H',4);put(PLAYER+172,'I',BODY);put(BODY+50,'h',100);put(LEVEL+16,'H',level)
    # Exact D9F8 cache initial state; allocations are host-owned in native code.
    put(CBFREE,'HH',160,512);put(0x0300556c,'H',1);put(0x03005588,'I',10000)
    put(0x03004304,'I',3)
    call(0x0800fa78);call(0x0800f84c)
    assert lib.start(ROM,len(ROM),level)==1
def normalized_objects():
    raw=bytearray(u.mem_read(OBJ,4032))
    for i in range(84):
        v=struct.unpack_from('<I',raw,i*48)[0]
        if v&0x80000000:v=0x80000000|(((v&0x7fffffff)-OBJ)//48+1)
        elif v:v=(v-0x030055d0)//4+1
        struct.pack_into('<I',raw,i*48,v)
    return bytes(raw)
def equal(kind,expected,label):
    actual=native(kind,len(expected))
    assert actual==expected,(level,frame,label,[(i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20])
report={'whole_initializations':0,'whole_original_frames':0,'objects_compared':0,'original_oam_bytes':0,'original_obj_tile_bytes':0,'independent_original_rgb_frames':0,'scope':__doc__}
for level in range(16):
    frame=-1;initialize(level)
    equal(0,normalized_objects(),'initial objects');equal(1,bytes(u.mem_read(ACTIVE,336)),'initial active');equal(2,bytes(u.mem_read(FREE,336)),'initial free');report['whole_initializations']+=1
    for frame in range(128):
        put(BODY+50,'h',100-frame%101);put(PLAYER+196,'h',frame%101)
        put(GAME+136,'I',frame*12345);put(GAME+12,'I',frame*1234)
        put(PLAYER+188,'H',8 if frame%17==3 else 0);put(GAME+4,'H',1 if frame%13==7 else 0);put(GAME+7,'B',1)
        for at in (0x0800f85c,0x0800e460,0x0800dbdc,0x0800fce4,0x0800e358):call(at)
        assert lib.step(frame)==1,(level,frame)
        equal(0,normalized_objects(),'objects');equal(1,bytes(u.mem_read(ACTIVE,336)),'active');equal(2,bytes(u.mem_read(FREE,336)),'free')
        for kind,at,size,label in ((3,ENTRIES,1244,'entries'),(4,CBFREE,208,'free blocks'),(5,CBUSED,208,'used blocks'),(6,IMAGES,104,'used images'),(7,RING,100,'ring'),(8,0x06010000,32768,'OBJ bytes'),(9,OUT,1024,'OAM')):equal(kind,bytes(u.mem_read(at,size)),label)
        nums=b''.join(bytes(u.mem_read(at,size)) for at,size in ((0x03006aa8,6),(0x03006ab0,6),(0x03006b70,16),(0x03006b80,16),(0x03006bb0,10),(0x03006bc0,10)))
        equal(10,nums,'numeric scratch')
        state=[get(0x030055c4),get(0x030055c8),get(0x030055b0),get(0x0300556c,'H'),get(0x03005564,'H'),get(0x0300557c,'H'),(get(0x03005584)-RING)//2,(get(0x03005560)-RING)//2,get(0x0300555c,'H'),get(0x03005588),get(PLAYER+40),get(PLAYER+48),get(PLAYER+120,'H'),get(PLAYER+52,'H'),get(PLAYER+122,'H'),get(GAME+4,'H')]+[get(PLAYER+54+2*i,'H') for i in range(10)]
        equal(11,struct.pack('<26I',*state),'state')
        if frame in (0,1,63,127):
            family=0 if level<8 else 1
            # Actual palette family is immaterial to timing/state, but test
            # both original palette tables across the original OAM chain.
            assets.palette=struct.unpack('<256H',native(12,512));bg=struct.unpack('<256H',native(13+family,512))
            base=[bg[world[(y*3//4)*120+x//2]]&32767 for y in range(160) for x in range(240)]
            expected=reference((Entry*128).from_buffer_copy(bytes(u.mem_read(OUT,1024))),bytes(u.mem_read(0x06010000,32768)))
            assert lib.display(world,family,pixels)==1 and list(pixels)==expected,(level,frame,'original OAM pixel chain')
            report['independent_original_rgb_frames']+=1
        report['whole_original_frames']+=1;report['objects_compared']+=84;report['original_oam_bytes']+=1024;report['original_obj_tile_bytes']+=32768
    print('PASS whole original HUD frame chain level',level,flush=True)
report['rom_sha1']=hashlib.sha1(ROM).hexdigest()
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
report['fixture_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (fixture,Path(__file__))}
report['pixel_reference_sha256']=hashlib.sha256((ROOT/'tests/test_display.py').read_bytes()).hexdigest()
(ROOT/'analysis/native-hud-scene-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if not k.endswith('sha256')},indent=2))
