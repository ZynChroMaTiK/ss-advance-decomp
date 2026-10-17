"""Wall traces, projection and full sliding against original AENP ROM code."""
from pathlib import Path
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
SHA1='4ff48d8c18b70a5184758f1917343ae85a70858e'
assert hashlib.sha1(ROM).hexdigest()==SHA1
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
    '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
    *[str(ROOT/'src'/f) for f in ('slide.c','geometry.c','map_assets.c','movement.c')],
    '-o',str(BUILD/'libssa_slide.so')],check=True)
class Map(C.Structure):
    _fields_=[('data',C.POINTER(C.c_uint8)),('size',C.c_size_t)]+[(n+'_count',C.c_uint16) for n in
        ('thing','texture','sector','node','leaf','segment','side','line','vertex')]+[(n,C.c_uint32) for n in
        ('things','textures','sectors','nodes','leaves','segments','sides','lines','vertices','reject','blockmap','rom_offset')]
class Collision(C.Structure):
    _fields_=[('map',C.POINTER(Map)),('visited',C.POINTER(C.c_uint8)),('special_lines',C.POINTER(C.c_uint16)),
        ('special_count',C.c_size_t),('line_capacity',C.c_size_t)]+[(n,C.c_int32) for n in ('floor','ceiling','low_floor','ceiling_line','blocking_line')]
class Body(C.Structure):
    _fields_=[(n,C.c_int32) for n in ('x','y','z','velocity_x','velocity_y','velocity_z','height')]+[
        ('flags',C.c_uint16),('floor',C.c_int16),('ceiling',C.c_int16),('hover_offset',C.c_int8),('step_adjust',C.c_int8)]
class Vector(C.Structure):
    _fields_=[(n,C.c_int32) for n in ('x','y','dx','dy')]
class Hit(C.Structure):
    _fields_=[('fraction',C.c_int32),('line',C.c_uint16)]
class Slide(C.Structure):
    _fields_=[('map',C.POINTER(Map)),('visited',C.POINTER(C.c_uint8)),('intercepts',C.POINTER(Hit)),('capacity',C.c_size_t),('count',C.c_size_t)]
class Tables(C.Structure):
    _fields_=[('sine',C.c_uint16*16384),('direction',C.c_uint16*32769)]
MOVE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Body),C.c_int32,C.c_int32)
lib=C.CDLL(str(BUILD/'libssa_slide.so'))
for name,args in {
    'ssa_math_tables_load':[C.POINTER(Tables),C.c_void_p,C.c_size_t],
    'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
    'ssa_map_assets_free':[C.POINTER(Map)],
    'ssa_map_collision_init':[C.POINTER(Collision),C.POINTER(Map)],
    'ssa_map_collision_free':[C.POINTER(Collision)],
    'ssa_map_try_move':[C.POINTER(Collision),C.POINTER(Body)]+[C.c_int32]*3,
    'ssa_map_sector':[C.POINTER(Map),C.c_int32,C.c_int32],
    'ssa_slide_init':[C.POINTER(Slide),C.POINTER(Map)],
    'ssa_slide_free':[C.POINTER(Slide)],
    'ssa_trace_divide':[C.c_int32,C.c_int32],
    'ssa_trace_side':[C.c_int32,C.c_int32,C.POINTER(Vector)],
    'ssa_trace_intercept':[C.POINTER(Vector),C.POINTER(Vector)],
    'ssa_slide_trace':[C.POINTER(Slide)]+[C.c_int32]*4,
    'ssa_slide_project':[C.POINTER(Map),C.c_uint16,C.c_int32,C.c_int32,C.POINTER(Tables),C.POINTER(C.c_int32),C.POINTER(C.c_int32)],
    'ssa_slide_move':[C.POINTER(Slide),C.POINTER(Body),C.c_int32,C.POINTER(Tables),MOVE,C.c_void_p],
}.items(): getattr(lib,name).argtypes=args
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]); uc.mem_write(0x030003a4,ROM[0x430:0x638])
BASE,SCRATCH,GRID,ENTITY,PARAM,STOP,CALLBACK=0x02000000,0x02040000,0x02050000,0x02070000,0x02071000,0x087ff000,0x087ff100
REGS=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
rng=random.Random(0x4a38)
report={'rom_sha1':SHA1,'divide_cases':0,'trace_side_cases':0,'intercept_cases':0,'projection_cases':0,'trace_cases':0,'slide_cases':0,'guard_cases':0}
def signed(n): return C.c_int32(n).value
def put(addr,fmt,*values): uc.mem_write(addr,struct.pack(fmt,*values))
def get(addr,fmt): return struct.unpack(fmt,uc.mem_read(addr,struct.calcsize(fmt)))[0]
def run(addr,*args,stack=()):
    for reg,arg in zip(REGS,args): uc.reg_write(reg,arg&0xffffffff)
    if stack: put(0x03007e00,'<'+'I'*len(stack),*stack)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(addr|1,STOP,count=500000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(addr)
    return signed(uc.reg_read(UC_ARM_REG_R0))
def biosdiv(machine,address,size,user):
    a=signed(machine.reg_read(UC_ARM_REG_R0)); b=signed(machine.reg_read(UC_ARM_REG_R1))
    q=abs(a)//abs(b) if b else (1 if a<0 else -1)
    if b and (a<0)!=(b<0): q=-q
    machine.reg_write(UC_ARM_REG_R0,q&0xffffffff)
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,biosdiv,begin=0x08002c88,end=0x08002c88)
hits=[]; links=[]
sliding_active=False; original_moves=[]; native_moves=[]; native_accepts=[]
coverage={hex(a):0 for a in (0x08004ae4,0x08004b14,0x08004b20,0x08004b8e,0x08004bb4)}
def slide_hook(machine,address,size,user):
    if not sliding_active: return
    if address==0x080043d0:
        original_moves.append((signed(machine.reg_read(UC_ARM_REG_R1)),signed(machine.reg_read(UC_ARM_REG_R2)),
            get(ENTITY+12,'<i'),get(ENTITY+16,'<i'),get(ENTITY+20,'<i')))
    else: coverage[hex(address)]+=1
for address in (0x080043d0,*[int(a,16) for a in coverage]):
    uc.hook_add(UC_HOOK_CODE,slide_hook,begin=address,end=address)
def callback(machine,address,size,user):
    p=machine.reg_read(UC_ARM_REG_R0)
    hits.append((get(p,'<i'),get(p+8,'<I')))
    machine.reg_write(UC_ARM_REG_R0,1); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,callback,begin=CALLBACK,end=CALLBACK)
def link_hook(machine,address,size,user):
    links.append(address); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,link_hook,begin=0x0800a65c,end=0x0800a65c)
uc.hook_add(UC_HOOK_CODE,link_hook,begin=0x0800a5d0,end=0x0800a5d0)
edge=(-2147483648,-2147483647,-65537,-65536,-1,0,1,65535,65536,2147483647)
for i in range(5000):
    a,b=(rng.choice(edge) if i<1000 else signed(rng.getrandbits(32)) for _ in range(2))
    expected=run(0x0800970c,a,b)
    assert lib.ssa_trace_divide(a,b)==expected,('divide',a,b,expected,lib.ssa_trace_divide(a,b))
    report['divide_cases']+=1
    t=Vector(*[signed(rng.getrandbits(32)) for _ in range(4)])
    if i%4==0: t.dx=0
    if i%4==1: t.dy=0
    uc.mem_write(PARAM,bytes(t))
    assert lib.ssa_trace_side(a,b,C.byref(t))==run(0x080099f8,a,b,PARAM),('side',i)
    report['trace_side_cases']+=1
    w=Vector(*[signed(rng.getrandbits(32)) for _ in range(4)])
    uc.mem_write(PARAM+16,bytes(w))
    actual=lib.ssa_trace_intercept(C.byref(t),C.byref(w)); expected=run(0x0800995c,PARAM,PARAM+16)
    assert actual==expected,('intercept',i,actual,expected)
    report['intercept_cases']+=1
print('PASS fixed division, trace sides and intercepts: 15,000 ROM cases',flush=True)

for level in range(16):
    m=Map(); c=Collision(); s=Slide()
    assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
    assert lib.ssa_map_collision_init(C.byref(c),C.byref(m))
    assert lib.ssa_slide_init(C.byref(s),C.byref(m))
    data=bytearray(C.string_at(m.data,m.size)); original=bytearray(data)
    for off in range(0x1c,0x48,4): struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
    uc.mem_write(BASE,bytes(original))
    ox,oy,width,height=struct.unpack_from('<hhhh',data,m.blockmap)
    put(0x03004930,'<I',BASE); put(0x03004944,'<iihh',ox*65536,oy*65536,width,height)
    put(0x03004954,'<I',SCRATCH); put(0x03004640,'<I',GRID); uc.mem_write(GRID,bytes(width*height*4))
    vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]))
    for i in range(250):
        vx,vy=rng.choice(vertices)
        x=vx*65536+rng.randrange(-64*65536,64*65536); y=vy*65536+rng.randrange(-64*65536,64*65536)
        line=rng.randrange(m.line_count)
        dx,dy=(rng.randrange(-30*65536,30*65536) for _ in range(2))
        if i<10: dx,dy=rng.choice(edge),rng.choice(edge)
        put(ENTITY,'<ii',x,y); put(0x03004350,'<I',ENTITY); put(0x0300460c,'<ii',dx,dy)
        run(0x08004930,BASE+m.lines+line*40)
        a,b=C.c_int32(dx),C.c_int32(dy)
        assert lib.ssa_slide_project(C.byref(m),line,x,y,C.byref(tables),C.byref(a),C.byref(b))==1
        expected=(get(0x0300460c,'<i'),get(0x03004610,'<i'))
        assert (a.value,b.value)==expected,('projection',level,i,line,dx,dy,(a.value,b.value),expected)
        report['projection_cases']+=1
        # Include exact block boundaries, short and long traces, and outside-map cells.
        if i%5==0: x=(ox+rng.randrange(-1,width+1)*128)*65536
        if i%7==0: y=(oy+rng.randrange(-1,height+1)*128)*65536
        span=30 if i%3 else 512
        ex=x+rng.randrange(-span*65536,span*65536); ey=y+rng.randrange(-span*65536,span*65536)
        hits.clear(); run(0x0800a240,x,y,ex,ey,stack=(1,CALLBACK|1))
        expected=[(f,(p-BASE-m.lines)//40) for f,p in hits]
        assert lib.ssa_slide_trace(C.byref(s),x,y,ex,ey)==1
        actual=sorted([(s.intercepts[j].fraction,s.intercepts[j].line) for j in range(s.count)],key=lambda h:h[0])
        assert actual==expected,('trace',level,i,x,y,ex,ey,actual,expected)
        report['trace_cases']+=1
    radius=32*65536
    @MOVE
    def native_move(context,body,x,y):
        native_moves.append((x,y,body.contents.velocity_x,body.contents.velocity_y,body.contents.velocity_z))
        result=lib.ssa_map_try_move(C.byref(c),body,radius,x,y)
        if result==1: native_accepts.append((x,y))
        return result
    for i in range(400):
        vx,vy=rng.choice(vertices)
        x=vx*65536+rng.randrange(-40*65536,40*65536); y=vy*65536+rng.randrange(-40*65536,40*65536)
        sector=lib.ssa_map_sector(C.byref(m),x,y)
        assert sector>=0
        floor,ceil=struct.unpack_from('<hh',data,m.sectors+sector*20)
        b=Body(x=x,y=y,z=(floor+rng.choice((0,0,0,20,49)))*65536,
            velocity_x=rng.randrange(-30*65536,30*65536),velocity_y=rng.randrange(-30*65536,30*65536),
            velocity_z=rng.randrange(-600000,600000),height=rng.choice((48,64,100))*65536,flags=rng.choice((0,4,8,16)),floor=floor,ceiling=ceil)
        if i%5==0: b.velocity_x=0
        if i%7==0: b.velocity_y=0
        raw=bytearray(rng.randbytes(0x74))
        fields=((0,'x'),(4,'y'),(8,'z'),(12,'velocity_x'),(16,'velocity_y'),(20,'velocity_z'),(44,'height'))
        for off,name in fields: struct.pack_into('<i',raw,off,getattr(b,name))
        struct.pack_into('<i',raw,0x28,radius); struct.pack_into('<H',raw,0x30,b.flags)
        struct.pack_into('<hh',raw,0x70,b.ceiling,b.floor)
        uc.mem_write(ENTITY,bytes(raw)); links.clear(); original_moves.clear(); native_moves.clear(); native_accepts.clear()
        sliding_active=True
        run(0x08004a38,ENTITY)
        sliding_active=False
        result=lib.ssa_slide_move(C.byref(s),C.byref(b),radius,C.byref(tables),native_move,None)
        assert result>=0,('invalid slide',level,i)
        assert original_moves==native_moves,('move order',level,i,original_moves,native_moves)
        assert links==[0x0800a65c,0x0800a5d0]*len(native_accepts),('commit count',level,i)
        for off,name in fields: struct.pack_into('<i',raw,off,getattr(b,name))
        struct.pack_into('<hh',raw,0x70,b.ceiling,b.floor)
        expected=bytes(uc.mem_read(ENTITY,0x74))
        assert bytes(raw)==expected,('slide',level,i,[(off,struct.unpack_from('<i',raw,off)[0],struct.unpack_from('<i',expected,off)[0]) for off,name in fields])
        report['slide_cases']+=1
    # Workspace and bounds guards are native policy, not original behavior.
    assert lib.ssa_slide_trace(None,0,0,0,0)==-1
    assert lib.ssa_slide_move(C.byref(s),C.byref(b),-1,C.byref(tables),native_move,None)==-1
    old=s.capacity; s.capacity=0
    assert lib.ssa_slide_trace(C.byref(s),0,0,0,0)==-1
    s.capacity=old
    a,bv=C.c_int32(123),C.c_int32(456)
    assert lib.ssa_slide_project(C.byref(m),m.line_count,0,0,C.byref(tables),C.byref(a),C.byref(bv))==-1 and (a.value,bv.value)==(123,456)
    report['guard_cases']+=4
    lib.ssa_slide_free(C.byref(s)); lib.ssa_map_collision_free(C.byref(c)); lib.ssa_map_assets_free(C.byref(m))
    print(f'PASS level {level}: 250 projection, 250 trace, 400 full-slide cases',flush=True)
report['scope']='Original wall-only trace/projection/full slide; original fixed arithmetic and trigonometry execute; BIOS integer divide serviced; dynamic grid empty and spatial unlink/relink hooked in full-slide oracle.'
assert all(coverage.values()),coverage
report['slide_branch_visits']=coverage
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src/slide.c',ROOT/'include/ssa/slide.h',Path(__file__))}
(ROOT/'analysis/native-sliding-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
