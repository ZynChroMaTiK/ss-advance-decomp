"""Original0750C setup tail + actual0787C VCOUNT interrupts vs native pixels."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
def classes(path,names):
    nodes=[n for n in ast.parse((ROOT/path).read_text()).body if isinstance(n,ast.ClassDef) and n.name in names]
    exec(compile(ast.Module(nodes,type_ignores=[]),path,'exec'),globals())
classes('tests/test_display.py',['Registers'])
classes('tests/test_sprite_oam_rom.py',['Entry','Oam'])
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
    '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
    str(ROOT/'src/display.c'),'-o',str(B/'libssa_hud_colors.so')],check=True)
lib=C.CDLL(str(B/'libssa_hud_colors.so'))
lib.ssa_display_registers_default.argtypes=[C.POINTER(Registers)]
lib.ssa_display_compose_gameplay.argtypes=[C.POINTER(Registers),C.c_void_p,C.c_size_t,
    C.c_void_p,C.c_void_p,C.POINTER(Oam),C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_int16,C.c_int16]
r=Registers();assert lib.ssa_display_registers_default(C.byref(r))==1
o=Oam();o.loaded=1;o.count=3
for e in o.entries:e.a0=0x200
for i in range(3):o.entries[i].a0=0x2000+i*64;o.entries[i].a1=0xc000
# Include neighboring unmodified palette entries and transparent pixels too.
tiles=bytes([0 if x%9==0 else 137+x%9 for y in range(8) for x in range(8)])*512
world=bytes(14400);bg=(C.c_uint16*256)();obj=(C.c_uint16*256).from_buffer_copy(ROM[0x79effc:0x79f1fc])
original=bytes(obj);out=(C.c_uint16*38400)()
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,n in ((0x08000000,len(ROM)),(0x02000000,0x10000),(0x03000000,0x10000),(0x04000000,0x10000),(0x05000000,0x1000)):u.mem_map(at,n)
u.mem_write(0x08000000,ROM)
P,BODY,STATE,STOP=0x02000000,0x02001000,0x02002000,0x087ff000
def put(at,fmt,*v):u.mem_write(at,struct.pack('<'+fmt,*v))
def get(at,fmt):return struct.unpack('<'+fmt,u.mem_read(at,struct.calcsize('<'+fmt)))[0]
put(0x030000c0,'I',STATE);put(0x03004648,'I',P);put(0x0300464e,'H',0);put(P+0xac,'I',BODY)
report={'cases':0,'original_scanline_interrupts':0,'pixels_compared':0,'scope':__doc__}
for health in (-1,0,1,99,100,101,200,32767):
    for armor in (-1,0,1,100):
        put(BODY+0x32,'h',health);put(P+0xc4,'h',armor)
        u.mem_write(0x05000200,original);put(0x04000004,'H',0x8220)
        u.reg_write(UC_ARM_REG_R4,0);u.reg_write(UC_ARM_REG_R5,STATE);u.reg_write(UC_ARM_REG_R6,P)
        u.emu_start(0x0800780b,0x0800786e,count=1000)
        assert u.reg_read(UC_ARM_REG_PC)==0x0800786e
        rows=[]
        for y in range(160):
            status=get(0x04000004,'H')
            if status&32 and status>>8==y:
                put(0x04000006,'H',y);u.reg_write(UC_ARM_REG_LR,STOP|1)
                u.emu_start(0x0800787d,STOP,count=1000)
                assert u.reg_read(UC_ARM_REG_PC)==STOP
                report['original_scanline_interrupts']+=1
            rows.append(struct.unpack('<256H',u.mem_read(0x05000200,512)))
        assert lib.ssa_display_compose_gameplay(C.byref(r),world,len(world),bg,obj,C.byref(o),tiles,len(tiles),out,38400,health,armor)==1
        for y in range(160):
            for x in range(240):
                index=tiles[(y%64//8*8+x//8)*64+(y%8)*8+x%8] if x<64 else 0
                expected=rows[y][index]&32767 if index else 0
                assert out[y*240+x]==expected,(health,armor,x,y,index,out[y*240+x],expected)
        assert bytes(obj)==original
        report['cases']+=1;report['pixels_compared']+=38400
report['rom_sha1']=hashlib.sha1(ROM).hexdigest()
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
    (ROOT/'src/display.c',ROOT/'include/ssa/display.h',ROOT/'src/native_game.c',ROOT/'src/pc_renderer.c',Path(__file__))}
(ROOT/'analysis/hud-colors-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
