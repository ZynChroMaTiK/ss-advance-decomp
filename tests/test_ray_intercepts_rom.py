"""09A88 wall intercepts and 0975C blockmap traversal, original math intact."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
SHA1='4ff48d8c18b70a5184758f1917343ae85a70858e'
assert hashlib.sha1(ROM).hexdigest()==SHA1
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
    '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
    *[str(ROOT/'src'/f) for f in ('ray_intercepts.c','slide.c','geometry.c','map_assets.c','movement.c')],
    '-o',str(BUILD/'libssa_ray_intercepts.so')],check=True)
class Map(C.Structure):
    _fields_=[('data',C.POINTER(C.c_uint8)),('size',C.c_size_t)]+[(n+'_count',C.c_uint16) for n in
        ('thing','texture','sector','node','leaf','segment','side','line','vertex')]+[(n,C.c_uint32) for n in
        ('things','textures','sectors','nodes','leaves','segments','sides','lines','vertices','reject','blockmap','rom_offset')]
class Ray(C.Structure):
    _fields_=[(n,C.c_int32) for n in ('x','y','dx','dy')]
class Hit(C.Structure):
    _fields_=[('fraction',C.c_int32),('line',C.c_uint)]
class Traversal(C.Structure):
    _fields_=[('ray',Ray),('intercepts',C.POINTER(Hit)),('count',C.c_size_t),('capacity',C.c_size_t),
              ('blocking_fraction',C.c_int32),('updating',C.c_uint)]
class Collector(C.Structure):
    _fields_=[('map',C.POINTER(Map)),('visited',C.POINTER(C.c_uint8)),('capacity',C.c_size_t),('updating',C.c_uint)]
lib=C.CDLL(str(BUILD/'libssa_ray_intercepts.so'))
for name,args in {
    'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
    'ssa_map_assets_free':[C.POINTER(Map)],
    'ssa_ray_walls_init':[C.POINTER(Collector),C.POINTER(Map)],
    'ssa_ray_walls_free':[C.POINTER(Collector)],
    'ssa_ray_walls_begin':[C.POINTER(Collector)],
    'ssa_ray_walls_line':[C.POINTER(Collector),C.POINTER(Traversal),C.c_uint],
    'ssa_ray_walls_cell':[C.POINTER(Collector),C.POINTER(Traversal),C.c_int32,C.c_int32],
}.items(): getattr(lib,name).argtypes=args
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]); uc.mem_write(0x030003a4,ROM[0x430:0x638])
BASE,HITS,STOP=0x02000000,0x02050000,0x087ff000
REGS=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
rng=random.Random(0x9a88)
report={'rom_sha1':SHA1,'line_cases':0,'cell_cases':0,'duplicate_cases':0,'guard_cases':0,'hits':0,'maps':16}
def put(addr,fmt,*values): uc.mem_write(addr,struct.pack(fmt,*values))
def get(addr,fmt): return struct.unpack(fmt,uc.mem_read(addr,struct.calcsize(fmt)))[0]
def run(addr,*args):
    for reg,arg in zip(REGS,args): uc.reg_write(reg,arg&0xffffffff)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(addr|1,STOP,count=500000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(addr)
    return C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
def expected(m):
    n=(get(0x030054dc,'<I')-HITS)//12
    rows=[struct.unpack('<iII',uc.mem_read(HITS+i*12,12)) for i in range(n)]
    assert all(kind==1 for _,kind,_ in rows)
    return [(f,(p-BASE-m.lines)//40) for f,kind,p in rows]
def actual(t): return [(t.intercepts[i].fraction,t.intercepts[i].line) for i in range(t.count)]

stamp=1
for level in range(16):
    m=Map(); c=Collector()
    assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
    assert lib.ssa_ray_walls_init(C.byref(c),C.byref(m))==1
    hits=(Hit*max(1,m.line_count))(); t=Traversal(intercepts=hits,capacity=m.line_count)
    data=bytearray(C.string_at(m.data,m.size)); original=bytearray(data)
    for off in range(0x1c,0x48,4): struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
    uc.mem_write(BASE,bytes(original))
    ox,oy,width,height=struct.unpack_from('<hhhh',data,m.blockmap)
    put(0x03004930,'<I',BASE); put(0x03004944,'<iihh',ox*65536,oy*65536,width,height)
    vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]))
    saved=None
    for i in range(600):
        line=rng.randrange(m.line_count)
        v1,v2=struct.unpack_from('<HH',data,m.lines+line*40)
        vx,vy=vertices[rng.choice((v1,v2))]
        if i%3==0:
            span=rng.choice((0x100000,0x100001,0xfffff))
            dx,dy=rng.choice((-span,span)),rng.choice((-span,span))
            x,y=vx*65536-dx//2,vy*65536-dy//2
        elif i%3==1:
            x,y=vx*65536,vy*65536
            dx,dy=(rng.randrange(-512,513)*65536 for _ in range(2))
        else:
            x,y=vx*65536+rng.randrange(-64,65)*65536,vy*65536+rng.randrange(-64,65)*65536
            dx,dy=(rng.randrange(-512,513)*65536 for _ in range(2))
        t.ray=Ray(x,y,dx,dy); t.count=0
        assert lib.ssa_ray_walls_begin(C.byref(c))==1
        put(0x03004900,'<iiii',x,y,dx,dy); put(0x030054dc,'<I',HITS)
        assert run(0x08009a88,BASE+m.lines+line*40)==1
        assert lib.ssa_ray_walls_line(C.byref(c),C.byref(t),line)==1
        assert actual(t)==expected(m),('line',level,i,line,actual(t),expected(m))
        if t.count: saved=(line,Ray.from_buffer_copy(bytes(t.ray))); report['hits']+=t.count
        report['line_cases']+=1
        if i%20==0:
            before=actual(t)
            assert lib.ssa_ray_walls_line(C.byref(c),C.byref(t),line)==1 and actual(t)==before
            report['duplicate_cases']+=1
    for i in range(400):
        bx,by=rng.randrange(-1,width+1),rng.randrange(-1,height+1)
        x,y=(ox+bx*128+rng.randrange(128))*65536,(oy+by*128+rng.randrange(128))*65536
        dx,dy=(rng.randrange(-512,513)*65536 for _ in range(2))
        t.ray=Ray(x,y,dx,dy); t.count=0
        assert lib.ssa_ray_walls_begin(C.byref(c))==1
        put(0x03004900,'<iiii',x,y,dx,dy); put(0x030054dc,'<I',HITS)
        stamp+=1; put(0x03004918,'<I',stamp)
        assert run(0x0800975c,bx,by,0x08009a89)==1
        assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),bx,by)==1
        assert actual(t)==expected(m),('cell',level,i,bx,by,actual(t),expected(m))
        report['cell_cases']+=1; report['hits']+=t.count
        # Retain original validcount and native visited across repeat cells.
        assert run(0x0800975c,bx,by,0x08009a89)==1
        assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),bx,by)==1
        assert actual(t)==expected(m)
        report['duplicate_cases']+=1
    assert saved
    line,ray=saved; t.ray=ray; t.count=0; t.capacity=0
    assert lib.ssa_ray_walls_begin(C.byref(c))==1
    assert lib.ssa_ray_walls_line(C.byref(c),C.byref(t),line)==-1 and t.count==0
    t.capacity=m.line_count
    assert lib.ssa_ray_walls_line(None,C.byref(t),0)==-1
    assert lib.ssa_ray_walls_line(C.byref(c),None,0)==-1
    assert lib.ssa_ray_walls_line(C.byref(c),C.byref(t),m.line_count)==-1
    c.updating=1
    assert lib.ssa_ray_walls_line(C.byref(c),C.byref(t),0)==-1
    assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),0,0)==-1
    assert lib.ssa_ray_walls_begin(C.byref(c))==-1
    assert lib.ssa_ray_walls_free(C.byref(c))==-1
    c.updating=0
    t.count=t.capacity+1
    assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),0,0)==-1
    t.count=0
    oldsize=m.size; m.size=0
    assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),0,0)==-1
    m.size=oldsize
    oldcapacity=c.capacity; c.capacity=0
    assert lib.ssa_ray_walls_begin(C.byref(c))==-1
    c.capacity=oldcapacity
    assert lib.ssa_ray_walls_init(C.byref(c),C.byref(m))==-1
    # Malformed input is rejected before host reads beyond the loaded map.
    address=C.addressof(m.data.contents)
    line_address=address+m.lines+line*40
    oldvertex=C.string_at(line_address,2)
    C.memmove(line_address,struct.pack('<H',m.vertex_count),2)
    assert lib.ssa_ray_walls_begin(C.byref(c))==1
    assert lib.ssa_ray_walls_line(C.byref(c),C.byref(t),line)==-1
    C.memmove(line_address,oldvertex,2)
    offset_address=address+m.blockmap+8
    oldoffset=C.string_at(offset_address,2)
    words=struct.unpack_from('<H',data,0x1a)[0]
    C.memmove(offset_address,struct.pack('<H',words),2)
    assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),0,0)==-1
    C.memmove(offset_address,oldoffset,2)
    first=struct.unpack('<H',oldoffset)[0]+1
    cell_address=address+m.blockmap+first*2
    oldline=C.string_at(cell_address,2)
    C.memmove(cell_address,struct.pack('<H',m.line_count),2)
    assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),0,0)==-1
    C.memmove(cell_address,oldline,2)
    dimensions=address+m.blockmap+4
    olddimensions=C.string_at(dimensions,4)
    C.memmove(dimensions,struct.pack('<hh',32767,32767),4)
    assert lib.ssa_ray_walls_cell(C.byref(c),C.byref(t),0,0)==-1
    C.memmove(dimensions,olddimensions,4)
    report['guard_cases']+=16
    assert lib.ssa_ray_walls_free(C.byref(c))==1
    lib.ssa_map_assets_free(C.byref(m))
    print(f'PASS map {level}: 600 wall, 400 cell, 430 duplicate comparisons',flush=True)
report['rom_comparisons']=report['line_cases']+report['cell_cases']+6400
report['scope']='Original 09A88 and 0975C execute with real map geometry and original fixed math; wall flags and mutable heights are consumed by the later predicate, outside collection. Dedicated visited storage replaces global validcount. Native malformed/reentry/capacity guards are additional policy.'
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src/ray_intercepts.c',ROOT/'include/ssa/ray_intercepts.h',Path(__file__))}
(ROOT/'analysis/native-ray-intercepts-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
