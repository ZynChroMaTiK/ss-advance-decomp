"""Whole A4F4 aim and flags3 A240/A09C including original entity block lists.
No intercepted math, traversal, collector or predicate calls. Real maps and
mutable sector heights, stable ties, dynamic entity order and persistent state.
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
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef): nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'ctypes world','exec'))
nodes=[n for n in ast.parse((ROOT/'tests/test_enemy_ray_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name in ('Ray','Hit','State','Walls','Opening')]
exec(compile(ast.Module(nodes,type_ignores=[]),'ctypes rays','exec'))
class Aim(C.Structure):
    _fields_=[('world',WP),('math',C.POINTER(Tables)),('walls',Walls),('traversal',State),('opening',Opening),('entities',C.POINTER(EP)),('entity_count',C.c_size_t),('entity_capacity',C.c_size_t),('source',EP),('target',EP)]+[(n,C.c_int32) for n in ('marker','top_slope','bottom_slope','shoot_z','range','start_x','start_y','end_x','end_y')]+[('updating',C.c_uint)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['weapon_aim.c','enemy_ray.c','ray_intercepts.c','ray_traversal.c','world.c','horizontal.c','slide.c','movement.c','geometry.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(BUILD/'libssa_weapon_aim.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_weapon_aim.so'))
for name,args in {
    'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[C.POINTER(Map)],
    'ssa_world_init':[WP,C.POINTER(Map),C.c_size_t], 'ssa_world_free':[WP], 'ssa_world_insert':[WP,EP], 'ssa_world_remove':[WP,EP],
    'ssa_math_tables_load':[C.POINTER(Tables),C.c_void_p,C.c_size_t], 'ssa_sincos':[C.POINTER(Tables),C.c_uint16,C.POINTER(C.c_int32),C.POINTER(C.c_int32)],
    'ssa_weapon_aim_init':[C.POINTER(Aim),WP,C.POINTER(Tables)], 'ssa_weapon_aim_free':[C.POINTER(Aim)],
    'ssa_weapon_aim_query':[C.POINTER(Aim),EP]+[C.c_int32]*3+[C.c_uint16,C.c_int32,C.POINTER(C.c_int32)],
}.items(): getattr(lib,name).argtypes=args
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]); uc.mem_write(0x030003a4,ROM[0x430:0x638])
BASE,SCRATCH,CELLS,ENTITIES,STOP,BUFFER=0x02000000,0x02040000,0x02050000,0x02070000,0x087ff000,0x03004770
REGS=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
OPEN=(0x03004910,0x030048f0,0x030054c0,0x030048f4)
rng=random.Random(0xa4f4)
def signed(x): return C.c_int32(x).value
def put(addr,fmt,*v): uc.mem_write(addr,struct.pack(fmt,*v))
def get(addr,fmt): return struct.unpack(fmt,uc.mem_read(addr,struct.calcsize(fmt)))[0]
def pointer(e): return C.addressof(e.contents) if e else 0
original_hits=[]
def observe(machine,address,size,user):
    count=(get(0x030054dc,'<I')-BUFFER)//12
    assert count<=32,('original scratch overflow',count)
    original_hits[:]=[struct.unpack('<iII',uc.mem_read(BUFFER+j*12,12)) for j in range(count)]
uc.hook_add(UC_HOOK_CODE,observe,begin=0x08009c00,end=0x08009c00)
def run(source,sx,sy,z,heading,range_):
    for r,a in zip(REGS,(source,sx,sy,z)): uc.reg_write(r,a&0xffffffff)
    put(0x03007e00,'<II',heading,range_&0xffffffff)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x0800a4f5,STOP,count=500000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    return signed(uc.reg_read(UC_ARM_REG_R0))
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_aim':0,'maps':16,'targets':0,'no_targets':0,'entity_intercepts':0,'wall_intercepts':0,'native_guards':0,'maximum_intercepts':0,'scope':__doc__}
for level in range(16):
    m=Map(); w=World(); a=Aim(); entities=(Entity*9)()
    assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
    assert lib.ssa_world_init(C.byref(w),C.byref(m),len(entities))==1
    assert lib.ssa_weapon_aim_init(C.byref(a),C.byref(w),C.byref(tables))==1
    data=bytearray(C.string_at(m.data,m.size)); original=bytearray(data)
    for off in range(0x1c,0x48,4): struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
    uc.mem_write(BASE,bytes(original)); uc.mem_write(SCRATCH,bytes(m.line_count*4))
    put(0x03004930,'<I',BASE); put(0x03004944,'<iihh',w.origin_x,w.origin_y,w.width,w.height); put(0x03004954,'<I',SCRATCH); put(0x03004640,'<I',CELLS)
    vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]))
    two=[j for j in range(m.line_count) if struct.unpack_from('<H',data,m.lines+j*40+26)[0]&4]
    native_index={C.addressof(e):j for j,e in enumerate(entities)}
    def eptr(e): return ENTITIES+native_index[pointer(e)]*0x100 if e else 0
    for i in range(512):
        for e in entities:
            if e.owner: assert lib.ssa_world_remove(C.byref(w),C.byref(e))==1
        vx,vy=rng.choice(vertices)
        heading=rng.randrange(65536) if i%3 else rng.choice((0,16384,32768,49152))
        sine,cosine=C.c_int32(),C.c_int32(); lib.ssa_sincos(C.byref(tables),heading,C.byref(sine),C.byref(cosine))
        x=vx*65536+rng.randrange(-64*65536,64*65536); y=vy*65536+rng.randrange(-64*65536,64*65536)
        if i%13==0: x=w.origin_x+rng.randrange(w.width)*128*65536
        if i%17==0: y=w.origin_y+rng.randrange(w.height)*128*65536
        z=rng.choice((-32,0,16,32,96))*65536
        for j,e in enumerate(entities):
            d=0 if j==0 else rng.randrange(-8,128)
            e.body.x=signed(x+sine.value*d); e.body.y=signed(y+cosine.value*d)
            if j>1 and i%4: e.body.x=signed(e.body.x+rng.randrange(-24,24)*65536)
            if i%11==0 and j==2: e.body.x=entities[1].body.x; e.body.y=entities[1].body.y
            e.body.z=z+rng.randrange(-32,32)*65536; e.body.height=rng.choice((0,8,32,64,96))*65536
            e.radius=rng.choice((0,1,8,16,32))*65536; e.type=rng.choice((0xcd,0xd6,0xd7,0xd8,0xe1,0x67))
            e.body.flags=rng.choice((0,2,2,2,0x22)); assert lib.ssa_world_insert(C.byref(w),C.byref(e))==1
        # Native insertion is head-first; project the complete linked order,
        # rather than silently assuming array order equals block-list order.
        rawcells=bytearray(w.width*w.height*4)
        for cell in range(w.width*w.height): struct.pack_into('<I',rawcells,cell*4,eptr(w.cells[cell]))
        uc.mem_write(CELLS,bytes(rawcells))
        for j,e in enumerate(entities):
            raw=bytearray(0x74); struct.pack_into('<iii',raw,0,e.body.x,e.body.y,e.body.z)
            struct.pack_into('<I',raw,0x18,eptr(e.block_next)); struct.pack_into('<iiH',raw,0x28,e.radius,e.body.height,e.body.flags); struct.pack_into('<H',raw,0x3c,e.type)
            uc.mem_write(ENTITIES+j*0x100,bytes(raw))
        if i>=256 and i%4==0:
            line=rng.choice(two); front,back=struct.unpack_from('<hh',data,m.lines+line*40+12)
            for sector in (front,back):
                raw=struct.pack('<hh',rng.choice((-32768,-32,0,16,32,96,32767)),rng.choice((-32768,0,32,96,128,32767)))
                off=m.sectors+sector*20; C.memmove(C.addressof(m.data.contents)+off,raw,4); uc.mem_write(BASE+off,raw)
        # Seed marker/fraction only once per triple to cover controller retries.
        if i%3==0:
            a.marker=signed(rng.getrandbits(32)); a.traversal.blocking_fraction=signed(rng.getrandbits(32))
            put(0x03005074,'<i',a.marker); put(0x0300505c,'<i',a.traversal.blocking_fraction)
        for (name,_),addr in zip(Opening._fields_,OPEN):
            value=signed(rng.getrandbits(32)); setattr(a.opening,name,value); put(addr,'<i',value)
        sx=rng.choice((0,1,4,-4))*65536+rng.randrange(65536); sy=rng.choice((0,1,4,-4))*65536+rng.randrange(65536)
        offset=rng.choice((-32,0,16,32,48,96))*65536; range_=rng.choice((0,1,16,32,64,128,256,512))*65536+rng.randrange(65536)
        w.blocking_entity=C.pointer(entities[8]); blocker=pointer(w.blocking_entity)
        original_hits.clear(); expected=run(ENTITIES,sx,sy,offset,heading,range_)
        result=C.c_int32(); actual=lib.ssa_weapon_aim_query(C.byref(a),C.byref(entities[0]),sx,sy,offset,heading,range_,C.byref(result))
        assert actual==1 and result.value==expected,(level,i,actual,result.value,expected)
        assert eptr(a.target)==get(0x0300491c,'<I'),(level,i,'target')
        for field,addr in [('marker',0x03005074),('top_slope',0x03004764),('bottom_slope',0x03005070),('shoot_z',0x03005048),('range',0x03004920),('start_x',0x03005050),('start_y',0x03005054),('end_x',0x030054d0),('end_y',0x030054d4)]:
            assert getattr(a,field)==get(addr,'<i'),(level,i,field,getattr(a,field),get(addr,'<i'))
        assert bytes(a.traversal.ray)==bytes(uc.mem_read(0x03004900,16)),(level,i,'ray')
        assert a.traversal.blocking_fraction==get(0x0300505c,'<i')
        assert [getattr(a.opening,n) for n,_ in Opening._fields_]==[get(addr,'<i') for addr in OPEN]
        assert a.traversal.count==len(original_hits),(level,i,a.traversal.count,len(original_hits))
        for j,(fraction,kind,p) in enumerate(original_hits):
            native=a.traversal.intercepts[j]
            expected_pointer=BASE+m.lines+native.line*40 if native.line<m.line_count else eptr(a.entities[native.line-m.line_count])
            assert native.fraction==get(BUFFER+j*12,'<i') and expected_pointer==p,(level,i,j)
            report['wall_intercepts' if kind else 'entity_intercepts']+=1
        assert pointer(w.blocking_entity)==blocker and not w.query_active and not a.updating
        report['whole_aim']+=1; report['targets' if a.target else 'no_targets']+=1
        report['maximum_intercepts']=max(report['maximum_intercepts'],a.traversal.count)
    for obj,field in ((a,'updating'),(a.traversal,'updating'),(a.walls,'updating'),(w,'query_active')):
        setattr(obj,field,1); assert lib.ssa_weapon_aim_query(C.byref(a),C.byref(entities[0]),0,0,0,0,0,C.byref(result))==-1
        assert lib.ssa_weapon_aim_free(C.byref(a))==-1; setattr(obj,field,0); report['native_guards']+=2
    assert lib.ssa_weapon_aim_init(C.byref(a),C.byref(w),C.byref(tables))==-1; report['native_guards']+=1
    assert lib.ssa_weapon_aim_free(C.byref(a))==1; lib.ssa_world_free(C.byref(w)); lib.ssa_map_assets_free(C.byref(m))
    print(f'PASS map {level}: 512 whole A4F4 queries with native entity/wall traces',flush=True)
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/native-weapon-aim-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
