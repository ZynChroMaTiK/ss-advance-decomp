"""Complete A240(flags=1,A041) vs native on all 16 maps; no ray hooks.
Opening 9C98 and A040 wall predicates are also compared with mutable floors,
ceilings, signed wrap and the 16/17-unit threshold. Original math executes.
"""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['enemy_ray.c','ray_intercepts.c','ray_traversal.c','slide.c','geometry.c','map_assets.c','movement.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(BUILD/'libssa_enemy_ray.so')],check=True)
nodes=[n for n in ast.parse((ROOT/'tests/test_slide_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name=='Map']
exec(compile(ast.Module(nodes,type_ignores=[]),'map declaration','exec'))
class Ray(C.Structure): _fields_=[(n,C.c_int32) for n in ('x','y','dx','dy')]
class Hit(C.Structure): _fields_=[('fraction',C.c_int32),('line',C.c_uint)]
class State(C.Structure):
    _fields_=[('ray',Ray),('intercepts',C.POINTER(Hit)),('count',C.c_size_t),('capacity',C.c_size_t),('blocking_fraction',C.c_int32),('updating',C.c_uint)]
class Walls(C.Structure):
    _fields_=[('map',C.POINTER(Map)),('visited',C.POINTER(C.c_uint8)),('capacity',C.c_size_t),('updating',C.c_uint)]
class Opening(C.Structure): _fields_=[(n,C.c_int32) for n in ('top','bottom','low_floor','range')]
class EnemyRay(C.Structure):
    _fields_=[('walls',Walls),('traversal',State),('opening',Opening),('origin_x',C.c_int32),('origin_y',C.c_int32),('updating',C.c_uint)]
lib=C.CDLL(str(BUILD/'libssa_enemy_ray.so'))
for name,args in {
    'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
    'ssa_map_assets_free':[C.POINTER(Map)],
    'ssa_enemy_ray_init':[C.POINTER(EnemyRay),C.POINTER(Map)],
    'ssa_enemy_ray_free':[C.POINTER(EnemyRay)],
    'ssa_enemy_ray_query':[C.POINTER(EnemyRay)]+[C.c_int32]*4,
    'ssa_ray_opening':[C.POINTER(Map),C.c_uint,C.POINTER(Opening)],
    'ssa_enemy_ray_wall':[C.POINTER(Map),C.c_uint,C.POINTER(Opening)],
}.items(): getattr(lib,name).argtypes=args
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]); uc.mem_write(0x030003a4,ROM[0x430:0x638])
BASE,SCRATCH,STOP,RECORD,BUFFER=0x02000000,0x02040000,0x087ff000,0x02070000,0x03004770
REGS=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
OPEN=(0x03004910,0x030048f0,0x030054c0,0x030048f4)
rng=random.Random(0xa041)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':{'opening':0,'wall':0,'whole_ray':0},'native_guards':0,'maps':16,'clear_rays':0,'blocked_rays':0,'maximum_intercepts':0,'scope':__doc__}
def signed(x): return C.c_int32(x).value
def put(addr,fmt,*values): uc.mem_write(addr,struct.pack(fmt,*values))
def get(addr,fmt): return struct.unpack(fmt,uc.mem_read(addr,struct.calcsize(fmt)))[0]
def run(addr,*args,stack=()):
    for r,a in zip(REGS,args): uc.reg_write(r,a&0xffffffff)
    if stack: put(0x03007e00,'<'+'I'*len(stack),*stack)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(addr|1,STOP,count=500000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    return signed(uc.reg_read(UC_ARM_REG_R0))
def seed(o):
    values=[signed(rng.getrandbits(32)) for _ in range(4)]
    for (name,_),a,v in zip(Opening._fields_,OPEN,values): setattr(o,name,v); put(a,'<i',v)
def compare_opening(o): assert [getattr(o,n) for n,_ in Opening._fields_]==[get(a,'<i') for a in OPEN]
original_hits=[]; calls=[]
def observe(machine,address,size,user):
    if address==0x08009c00:
        count=(get(0x030054dc,'<I')-BUFFER)//12
        assert count<=32,('original scratch overflow',count)
        original_hits[:]=[(get(BUFFER+j*12,'<i'),get(BUFFER+j*12+8,'<I')) for j in range(count)]
    else:
        p=machine.reg_read(REGS[0]); calls.append((get(p,'<i'),get(p+8,'<I')))
for a in (0x08009c00,0x0800a040): uc.hook_add(UC_HOOK_CODE,observe,begin=a,end=a)
height_cases=[(0,0),(0,16),(0,17),(17,0),(-17,0),(-16,0),(-32768,0),(-32768,32767),(-32768,-1),(32766,32767),(-100,100)]
for level in range(16):
    m=Map(); r=EnemyRay(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
    assert lib.ssa_enemy_ray_init(C.byref(r),C.byref(m))==1
    data=bytearray(C.string_at(m.data,m.size)); original=bytearray(data)
    for off in range(0x1c,0x48,4): struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
    uc.mem_write(BASE,bytes(original)); uc.mem_write(SCRATCH,bytes(m.line_count*4))
    ox,oy,width,height=struct.unpack_from('<hhhh',data,m.blockmap)
    put(0x03004930,'<I',BASE); put(0x03004944,'<iihh',ox*65536,oy*65536,width,height); put(0x03004954,'<I',SCRATCH)
    vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]))
    two=[j for j in range(m.line_count) if struct.unpack_from('<H',data,m.lines+j*40+26)[0]&4]
    assert two
    def heights(line,af,bf,ac,bc):
        front,back=struct.unpack_from('<hh',data,m.lines+line*40+12)
        for sector,f,c in ((front,af,ac),(back,bf,bc)):
            raw=struct.pack('<hh',f,c); off=m.sectors+sector*20
            C.memmove(C.addressof(m.data.contents)+off,raw,4); uc.mem_write(BASE+off,raw)
    for i in range(256):
        line=rng.choice(two) if i%4 else rng.randrange(m.line_count)
        if line in two:
            af,bf=height_cases[i%len(height_cases)] if i<128 else (rng.randrange(-32768,32768),rng.randrange(-32768,32768))
            heights(line,af,bf,rng.randrange(-32768,32768),rng.randrange(-32768,32768))
        ptr=BASE+m.lines+line*40
        seed(r.opening); run(0x08009c98,ptr)
        assert lib.ssa_ray_opening(C.byref(m),line,C.byref(r.opening))==1
        compare_opening(r.opening); report['rom_comparisons']['opening']+=1
        seed(r.opening); put(RECORD,'<iII',rng.randrange(65537),1,ptr)
        expected=run(0x0800a040,RECORD)
        actual=lib.ssa_enemy_ray_wall(C.byref(m),line,C.byref(r.opening))
        assert actual==expected,(level,i,line,actual,expected)
        compare_opening(r.opening); report['rom_comparisons']['wall']+=1
    # Restore original heights before actual-map runs; mutate live floors for
    # half the cases, with the same writes in original and native map storage.
    raw=bytes(data[m.sectors:m.sectors+m.sector_count*20])
    C.memmove(C.addressof(m.data.contents)+m.sectors,raw,len(raw)); uc.mem_write(BASE+m.sectors,raw)
    for i in range(512):
        if i>=256:
            line=rng.choice(two); af,bf=height_cases[i%len(height_cases)]
            heights(line,af,bf,rng.choice((-32768,-1,0,16,128,32767)),rng.choice((-32768,-1,0,16,128,32767)))
        vx,vy=rng.choice(vertices)
        x=vx*65536+rng.randrange(-64*65536,64*65536); y=vy*65536+rng.randrange(-64*65536,64*65536)
        if i%5==0: x=(ox+rng.randrange(-1,width+1)*128)*65536
        if i%7==0: y=(oy+rng.randrange(-1,height+1)*128)*65536
        span=30 if i%3 else 512
        ex=x+rng.randrange(-span*65536,span*65536); ey=y+rng.randrange(-span*65536,span*65536)
        if i%11==0: ex=x
        if i%13==0: ey=y
        seed(r.opening); initial=signed(rng.getrandbits(32)); r.traversal.blocking_fraction=initial; put(0x0300505c,'<i',initial)
        calls.clear(); original_hits.clear()
        expected=run(0x0800a240,x,y,ex,ey,stack=(1,0x0800a041))
        actual=lib.ssa_enemy_ray_query(C.byref(r),x,y,ex,ey)
        assert actual==expected,(level,i,(x,y,ex,ey),actual,expected)
        assert bytes(r.traversal.ray)==bytes(uc.mem_read(0x03004900,16))
        assert r.traversal.count==len(original_hits)
        native=[(r.traversal.intercepts[j].fraction,r.traversal.intercepts[j].line) for j in range(r.traversal.count)]
        expected_hits=[(get(BUFFER+j*12,'<i'),(p-BASE-m.lines)//40) for j,(f,p) in enumerate(original_hits)]
        assert native==expected_hits,(level,i,native,expected_hits)
        assert r.traversal.blocking_fraction==get(0x0300505c,'<i')
        compare_opening(r.opening)
        assert not r.updating and not r.traversal.updating and not r.walls.updating
        report['rom_comparisons']['whole_ray']+=1
        report['clear_rays' if actual else 'blocked_rays']+=1
        report['maximum_intercepts']=max(report['maximum_intercepts'],r.traversal.count)
    old=bytes(r.opening)
    assert lib.ssa_enemy_ray_wall(C.byref(m),m.line_count,C.byref(r.opening))==-1 and bytes(r.opening)==old
    assert lib.ssa_ray_opening(C.byref(m),m.line_count,C.byref(r.opening))==-1 and bytes(r.opening)==old
    assert lib.ssa_enemy_ray_init(C.byref(r),C.byref(m))==-1
    for obj in (r,r.traversal,r.walls):
        obj.updating=1
        assert lib.ssa_enemy_ray_query(C.byref(r),0,0,0,0)==-1
        assert lib.ssa_enemy_ray_free(C.byref(r))==-1
        obj.updating=0
    report['native_guards']+=9
    assert lib.ssa_enemy_ray_free(C.byref(r))==1; lib.ssa_map_assets_free(C.byref(m))
    print(f'PASS map {level}: 512 whole enemy rays, 256 openings, 256 wall predicates',flush=True)
assert lib.ssa_enemy_ray_query(None,0,0,0,0)==-1; report['native_guards']+=1
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/native-enemy-ray-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
