"""Whole 1288C/12850 and 1271C/083F0 vs native with controlled boundaries.
Original 093E8/094E8 and IWRAM fixed multiply run. BIOS division is substituted.
The pathfinder, visibility, nearest selection, motion and finish are explicit
boundaries in navigation tests; dual-path tests run the native combiner with
controlled A240(flags=1,A041) rays. No complete AI or ray/world claim.
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

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_spawn_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef): nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id=='EP' for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'entity declarations','exec'))
class Tables(C.Structure):
    _fields_=[('sine',C.c_uint16*16384),('direction',C.c_uint16*32769)]
class Nav(C.Structure):
    _fields_=[('entity',EP),('target',EP),('speed',C.c_int32)]+[(n,C.c_uint16) for n in ('phase','distance','heading','waypoint_x','waypoint_y','target_timer')]+[(n,C.c_uint8) for n in ('desired_facing','move_result','navigation_flags')]+[('updating',C.c_uint)]
NP=C.POINTER(Nav); IP=C.POINTER(C.c_int); I32P=C.POINTER(C.c_int32)
VIS=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_int,IP)
NEAR=C.CFUNCTYPE(C.c_int,C.c_void_p,EP,C.POINTER(EP))
PATH=C.CFUNCTYPE(C.c_int,C.c_void_p,NP,*([C.c_int32]*5),C.c_uint,C.c_uint,IP,I32P,I32P)
TURN=C.CFUNCTYPE(C.c_int,C.c_void_p,NP,C.c_int32,I32P)
MOVE=C.CFUNCTYPE(C.c_int,C.c_void_p,NP,I32P)
FINISH=C.CFUNCTYPE(C.c_int,C.c_void_p,NP)
class Ops(C.Structure):
    _fields_=[('visibility',VIS),('nearest',NEAR),('path',PATH),('turn',TURN),('move',MOVE),('finish',FINISH),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/enemy_navigation.c'),str(ROOT/'src/movement.c'),'-o',str(BUILD/'libssa_enemy_navigation.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_enemy_navigation.so'))
step=lib.ssa_enemy_navigation_step
step.argtypes=[NP,EP,C.POINTER(Tables),C.c_uint,C.c_int32,C.POINTER(Ops)]
reset=lib.ssa_enemy_navigation_reset; reset.argtypes=[NP]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
lib.ssa_enemy_navigation_speeds.argtypes=[I32P,C.c_void_p,C.c_size_t]
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
speeds=(C.c_int32*12)(); assert lib.ssa_enemy_navigation_speeds(speeds,ROM,len(ROM))==1
assert list(speeds)==[struct.unpack_from('<i',ROM,0x44bfd4+i*8)[0] for i in range(12)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
E,P,T,G,RECORD,STOP=0x02000000,0x02000100,0x02000200,0x02001000,0x02002000,0x087ff000
uc.mem_write(0x03004010,struct.pack('<I',G)); uc.mem_write(0x03004648,struct.pack('<I',RECORD)); uc.mem_write(RECORD+0xac,struct.pack('<I',P))
regs=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
def signed(x): return x if x<0x80000000 else x-0x100000000
def bios(machine,address,size,data):
    a,b=[signed(machine.reg_read(r)) for r in regs[:2]]
    assert b
    q=abs(a)//abs(b)*(-1 if (a<0)!=(b<0) else 1)
    machine.reg_write(regs[0],q&0xffffffff); machine.reg_write(regs[1],(a-q*b)&0xffffffff)
    machine.reg_write(regs[3],abs(q)); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,bios,begin=0x08002c88,end=0x08002c88)
hooks={0x0800b18c:'vis',0x080036c4:'near',0x0801271c:'path',0x08004614:'turn',0x08004498:'move',0x0800354c:'finish'}
oracle=[]; config={}; path_index=0
def raw_snapshot(): return bytes(uc.mem_read(E,0x74))
def hook(machine,address,size,data):
    global path_index
    kind=hooks[address]; args=[machine.reg_read(r) for r in regs]
    if kind=='vis': args=[signed(x) for x in args[1:3]]; result=config['blocked']
    elif kind=='near': args=args[:1]; result=config['near_enemy'] if args[0]==E else config['near_player']
    elif kind=='path':
        args=[signed(x) for x in args]+list(struct.unpack('<iII',machine.mem_read(machine.reg_read(UC_ARM_REG_SP),12)))
        result=config['masks'][min(path_index,1)]; path_index+=1
    elif kind=='turn': args=args[:2]; result=config['delta']
    elif kind=='move': args=args[:1]; result=config['moved']
    else: args=args[:1]; result=0
    oracle.append((kind,args,raw_snapshot()))
    if kind=='path':
        machine.mem_write(0x03005708,struct.pack('<i',config['ox'])); machine.mem_write(0x03005704,struct.pack('<i',config['oy']))
    if kind=='move':
        for offset,k in ((0,'dx'),(4,'dy'),(8,'dz')):
            v=struct.unpack('<i',machine.mem_read(E+offset,4))[0]
            machine.mem_write(E+offset,struct.pack('<I',(v+config[k])&0xffffffff))
    if kind=='finish':
        v=struct.unpack('<I',machine.mem_read(E+12,4))[0]
        machine.mem_write(E+12,struct.pack('<I',(v+1)&0xffffffff))
    machine.reg_write(regs[0],result&0xffffffff); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
hook_handles={addr:uc.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr) for addr in hooks}
fields=[('body.x',0,'i'),('body.y',4,'i'),('body.z',8,'i'),('body.velocity_x',12,'i'),('radius',0x28,'i'),('body.flags',0x30,'H'),('type',0x3c,'H'),('sector',0x6a,'h')]
nf=[('speed',0x24,'i'),('phase',0x44,'H'),('distance',0x46,'H'),('heading',0x48,'H'),('waypoint_x',0x4a,'H'),('waypoint_y',0x4c,'H'),('target_timer',0x56,'H'),('desired_facing',0x40,'B'),('move_result',0x41,'B'),('navigation_flags',0x50,'B')]
def setfield(o,p,v):
    parts=p.split('.')
    for part in parts[:-1]: o=getattr(o,part)
    setattr(o,parts[-1],v)
def getfield(o,p):
    for part in p.split('.'): o=getattr(o,part)
    return o
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':{},'callback_reentry_guards':0,'native_guards':0,'callback_failure_cases':0,'scope':__doc__}
rng=random.Random(0x1288c)
def compare(phase,flags,ticks,delta,masks=(3,3),blocked=0,moved=1,coords=None,heading=None,distance=None,target_timer=None,target_kind=T,target_flags=0,near_enemy=T,near_player=E,label='navigation'):
    global config,path_index
    raw=bytearray(rng.randbytes(0x74)); pr=bytearray(rng.randbytes(0x74)); tr=bytearray(rng.randbytes(0x74))
    typ=0xcd+rng.randrange(12)
    for off,fmt,val in ((0x44,'H',phase),(0x30,'H',flags),(0x3c,'H',typ),(0x20,'I',target_kind)):
        struct.pack_into('<'+fmt,raw,off,val)
    struct.pack_into('<H',tr,0x30,target_flags)
    if coords:
        for arr,values in ((raw,coords[:2]),(pr,coords[2:4])):
            for off,val in zip((0,4),values): struct.pack_into('<i',arr,off,val)
    for off,val in ((0x48,heading),(0x46,distance),(0x56,target_timer)):
        if val is not None: struct.pack_into('<H',raw,off,val)
    e,p,t=Entity(),Entity(),Entity(); nav=Nav(); ptrs={0:EP(),E:C.pointer(e),P:C.pointer(p),T:C.pointer(t)}
    for obj,data in ((e,raw),(p,pr),(t,tr)):
        for path,off,fmt in fields: setfield(obj,path,struct.unpack_from('<'+fmt,data,off)[0])
    nav.entity=ptrs[E]; nav.target=ptrs[target_kind]
    for path,off,fmt in nf: setattr(nav,path,struct.unpack_from('<'+fmt,raw,off)[0])
    config={'delta':delta,'masks':masks,'blocked':blocked,'moved':moved,'near_enemy':near_enemy,'near_player':near_player,
            **{n:signed(rng.getrandbits(32)) for n in ('ox','oy','dx','dy','dz')}}
    for addr,data in ((E,raw),(P,pr),(T,tr)): uc.mem_write(addr,bytes(data))
    uc.mem_write(G+7,bytes([ticks])); uc.mem_write(0x03004760,b'\0'*4)
    oracle.clear(); path_index=0
    for r,val in ((regs[0],E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)): uc.reg_write(r,val)
    uc.emu_start(0x0801288d,STOP,count=10000); assert uc.reg_read(UC_ARM_REG_PC)==STOP
    expected=raw_snapshot(); expected_calls=list(oracle); actual=[]; pi=0
    def addr(pointer): return next(a for a,b in ptrs.items() if (C.addressof(b.contents) if b else 0)==(C.addressof(pointer.contents) if pointer else 0))
    def snapshot():
        data=bytearray(raw)
        for path,off,fmt in fields: struct.pack_into('<'+fmt,data,off,getfield(e,path))
        for path,off,fmt in nf: struct.pack_into('<'+fmt,data,off,getattr(nav,path))
        struct.pack_into('<I',data,0x20,addr(nav.target)); return bytes(data)
    def record(kind,args):
        actual.append((kind,args,snapshot()))
        assert step(C.byref(nav),C.byref(p),C.byref(tables),ticks,speeds[typ-0xcd],C.byref(ops))==-1
        assert reset(C.byref(nav))==-1
        report['callback_reentry_guards']+=2
    @VIS
    def vis(ctx,a,b,out): record('vis',[a,b]); out[0]=blocked; return 0
    @NEAR
    def near(ctx,source,out):
        a=addr(source); record('near',[a]); out[0]=ptrs[near_enemy if a==E else near_player]; return 0
    @PATH
    def path(ctx,v,x0,y0,x1,y1,radius,cf,mode,mask,ox,oy):
        nonlocal pi
        record('path',[x0,y0,x1,y1,radius,cf,mode]); mask[0]=masks[min(pi,1)]; pi+=1
        ox[0]=config['ox']; oy[0]=config['oy']; return 0
    @TURN
    def turn(ctx,v,rate,out): record('turn',[E,rate]); out[0]=delta; return 0
    @MOVE
    def move(ctx,v,out):
        record('move',[E]); out[0]=moved
        for name in ('x','y','z'): setattr(e.body,name,signed((getattr(e.body,name)+config['d'+name])&0xffffffff))
        return 0
    @FINISH
    def finish(ctx,v): record('finish',[E]); e.body.velocity_x=signed((e.body.velocity_x+1)&0xffffffff); return 0
    ops=Ops(vis,near,path,turn,move,finish,None)
    assert step(C.byref(nav),C.byref(p),C.byref(tables),ticks,speeds[typ-0xcd],C.byref(ops))==1
    assert snapshot()==expected, (label,phase,flags,delta,masks,[(hex(i),a,b) for i,(a,b) in enumerate(zip(snapshot(),expected)) if a!=b])
    assert actual==expected_calls,(label,phase,flags,delta,masks,actual,expected_calls)
    assert not nav.updating
    report['rom_comparisons'][label]=report['rom_comparisons'].get(label,0)+1
    return nav,e,p,ops

for phase in (0,1,2,3,65535):
    for flags in (0,0x400,0x800,0xc00):
        for masks in ((0,0),(1,0),(2,0),(3,0),(3,1),(3,2),(3,3)):
            for delta in (-32768,-2048,-2047,-1024,-1023,0,1023,1024,2047,2048,32768):
                compare(phase,flags,1,delta,masks,moved=0,label='phase_mask_boundaries')
for ticks in (0,1,2,127,255):
    for timer in (0,1,127,255,32767,32768,32769,65535):
        for target in (0,T,P,E):
            for dead in (0,0x20):
                compare(2,0x400,ticks,0,target_timer=timer,target_kind=target,target_flags=dead,near_enemy=0,label='retarget_boundaries')
for x in (-2147483648,-2147483647,-65536,-1,0,1,65536,2147483647):
    for y in (-2147483648,-65536,0,65536,2147483647):
        for phase in (0,1,2): compare(phase,0,1,0,coords=(x,y,0,0),label='coordinate_boundaries')
for _ in range(4096):
    compare(rng.choice((0,1,2,3)),rng.randrange(0x10000),rng.randrange(256),signed(rng.getrandbits(32)),
            (rng.randrange(4),rng.randrange(4)),rng.randrange(4)==0,rng.randrange(257),label='random')

for heading in (0,1,0xefff,0xf000,0xffff):
    for distance in (0,1,0x2bf,0x2c0,0x2ff,0x7fff,0x8000,0xffff):
        raw=bytearray(rng.randbytes(0x74)); n=Nav(); e=Entity(); n.entity=C.pointer(e)
        n.phase=rng.randrange(65536); n.heading=heading; n.distance=distance
        for path,off,fmt in nf: struct.pack_into('<'+fmt,raw,off,getattr(n,path))
        uc.mem_write(E,bytes(raw))
        for r,val in ((regs[0],E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)): uc.reg_write(r,val)
        uc.emu_start(0x08012851,STOP,count=100)
        assert reset(C.byref(n))==1
        for path,off,fmt in nf: assert getattr(n,path)==struct.unpack_from('<'+fmt,uc.mem_read(E,0x74),off)[0]
        report['rom_comparisons']['reset']=report['rom_comparisons'].get('reset',0)+1

# Guards precede mutations. Borrow simple successful callbacks for error tests.
nav,e,p,ops=compare(0,0,1,0,label='guard_setup')
def guard(np,pp,mp,tick,op):
    before=(bytes(nav),bytes(e),bytes(p)); assert step(np,pp,mp,tick,0,op)==-1
    assert before==(bytes(nav),bytes(e),bytes(p)); report['native_guards']+=1
base=[C.byref(nav),C.byref(p),C.byref(tables),1,C.byref(ops)]
for i in (0,1,2,4):
    args=base.copy(); args[i]=None; guard(*args)
for ticks in (256,0xffffffff): args=base.copy(); args[3]=ticks; guard(*args)
for typ in (0,0xcc,0xd9,0xffff): e.type=typ; guard(*base)
e.type=0xcd
for field in ('visibility','nearest','path','turn','move','finish'):
    missing=Ops(); C.memmove(C.byref(missing),C.byref(ops),C.sizeof(ops)); setattr(missing,field,type(getattr(missing,field))())
    guard(*base[:4],C.byref(missing))
w=World(); w.query_active=1
e.owner=C.pointer(w); guard(*base); e.owner=C.POINTER(World)()
p.owner=C.pointer(w); guard(*base); p.owner=C.POINTER(World)()
nav.updating=1; guard(*base); nav.updating=0
nav.entity=EP(); guard(*base); nav.entity=C.pointer(e)
assert reset(None)==-1; report['native_guards']+=1
for out,rom,size in ((None,ROM,len(ROM)),(speeds,None,len(ROM)),(speeds,ROM,0x44bfd4+95)):
    assert lib.ssa_enemy_navigation_speeds(out,rom,size)==-1; report['native_guards']+=1

# Stop at each external failure; earlier navigation mutations are retained.
for fail in ('vis','near','path','turn','move','finish'):
    n=Nav(); ent=Entity(); player=Entity(); ent.type=0xcd; n.entity=C.pointer(ent); calls=[]
    def record_failure(name): calls.append(name); return -1 if name==fail else 0
    @VIS
    def fv(ctx,a,b,out): out[0]=0; return record_failure('vis')
    @NEAR
    def fn(ctx,source,out): out[0]=EP(); return record_failure('near')
    @PATH
    def fp(ctx,v,a,b,c,d,r,cf,mode,mask,ox,oy): mask[0]=0; ox[0]=oy[0]=0; return record_failure('path')
    @TURN
    def ft(ctx,v,rate,out): out[0]=0; return record_failure('turn')
    @MOVE
    def fm(ctx,v,out): out[0]=0; return record_failure('move')
    @FINISH
    def ff(ctx,v): return record_failure('finish')
    failing=Ops(fv,fn,fp,ft,fm,ff,None)
    assert step(C.byref(n),C.byref(player),C.byref(tables),1,0,C.byref(failing))==-1
    assert calls[-1]==fail and not n.updating
    report['callback_failure_cases']+=1

# Whole dual-ray 1271C, executing the original 083F0 segment append helper.
uc.hook_del(hook_handles[0x0801271c])
class PathState(C.Structure):
    _fields_=[('offset_x',C.c_int32),('offset_y',C.c_int32),('trace_mode',C.c_uint32),('segment_count',C.c_uint32),('segments',(C.c_int32*4)*10),('updating',C.c_uint)]
PSP=C.POINTER(PathState)
RAY=C.CFUNCTYPE(C.c_int,C.c_void_p,PSP,*([C.c_int32]*4),IP)
query=lib.ssa_enemy_path_query
query.argtypes=[PSP,C.POINTER(Tables),*([C.c_int32]*5),C.c_uint,C.c_uint,RAY,C.c_void_p,IP]
ray_expected=[]; rays=(0,0)
def path_snapshot():
    return (bytes(uc.mem_read(0x03005708,4)),bytes(uc.mem_read(0x03005704,4)),bytes(uc.mem_read(0x030056fc,4)),bytes(uc.mem_read(0x030000b0,4)),bytes(uc.mem_read(0x03000010,160)))
def ray_hook(machine,address,size,data):
    args=[signed(machine.reg_read(r)) for r in regs]
    assert struct.unpack('<II',machine.mem_read(machine.reg_read(UC_ARM_REG_SP),8))==(1,0x0800a041)
    ray_expected.append((args,path_snapshot()))
    machine.reg_write(regs[0],rays[len(ray_expected)-1]&0xffffffff)
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,ray_hook,begin=0x0800a240,end=0x0800a240)
def path_compare(coords,radius,cf,mode,count,results):
    global rays
    rays=results; state=PathState()
    state.offset_x=signed(rng.getrandbits(32)); state.offset_y=signed(rng.getrandbits(32))
    state.trace_mode=rng.getrandbits(32); state.segment_count=count
    for segment in state.segments:
        for i in range(4): segment[i]=signed(rng.getrandbits(32))
    def snapshot(): return (struct.pack('<i',state.offset_x),struct.pack('<i',state.offset_y),struct.pack('<I',state.trace_mode),struct.pack('<I',state.segment_count),bytes(state.segments))
    for a,value in zip((0x03005708,0x03005704,0x030056fc,0x030000b0,0x03000010),snapshot()): uc.mem_write(a,value)
    for r,v in zip(regs,coords): uc.reg_write(r,v&0xffffffff)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.mem_write(0x03007e00,struct.pack('<iII',radius,cf,mode)); ray_expected.clear()
    uc.emu_start(0x0801271d,STOP,count=8000); assert uc.reg_read(UC_ARM_REG_PC)==STOP
    expected=path_snapshot(); expected_result=uc.reg_read(regs[0]); actual=[]; output=C.c_int(-5)
    @RAY
    def ray(ctx,p,a,b,c,d,clear):
        actual.append(([a,b,c,d],snapshot())); clear[0]=results[len(actual)-1]
        assert query(C.byref(state),C.byref(tables),*coords,radius,cf,mode,ray,None,C.byref(output))==-1
        report['callback_reentry_guards']+=1
        return 0
    assert query(C.byref(state),C.byref(tables),*coords,radius,cf,mode,ray,None,C.byref(output))==1
    assert output.value==expected_result and snapshot()==expected and actual==ray_expected,(coords,radius,cf,mode,count)
    report['rom_comparisons']['dual_path']=report['rom_comparisons'].get('dual_path',0)+1
for radius in (-2147483648,-65537,-1,0,1,65536,0x100000,2147483647):
    for cf in (0,1):
        for mode in (0,1):
            for count in (0,8,9,10):
                for results in ((0,0),(1,0),(0,1),(1,1)):
                    path_compare((0,0,0x100000,0),radius,cf,mode,count,results)
for _ in range(4096):
    path_compare(tuple(signed(rng.getrandbits(32)) for _ in range(4)),signed(rng.getrandbits(32)),rng.randrange(2),rng.randrange(2),rng.randrange(11),tuple(signed(rng.getrandbits(32)) if rng.randrange(2) else 0 for _ in range(2)))
state=PathState(); output=C.c_int(42)
@RAY
def clear_ray(ctx,p,a,b,c,d,clear): clear[0]=1; return 0
base=[C.byref(state),C.byref(tables),0,0,0,0,0,0,0,clear_ray,None,C.byref(output)]
for index,value in ((0,None),(1,None),(9,RAY()),(11,None)):
    args=base.copy(); args[index]=value; before=bytes(state)
    assert query(*args)==-1 and bytes(state)==before and output.value==42
    report['native_guards']+=1
for field,value in (('segment_count',11),('segment_count',0xffffffff),('updating',1)):
    setattr(state,field,value); before=bytes(state)
    assert query(*base)==-1 and bytes(state)==before
    setattr(state,field,0); report['native_guards']+=1
for failure_index in (0,1):
    state=PathState(); output.value=42; state.offset_x=17; state.offset_y=23; count=[0]
    @RAY
    def failed_ray(ctx,p,a,b,c,d,clear):
        result=-1 if count[0]==failure_index else 0; count[0]+=1; clear[0]=1; return result
    assert query(C.byref(state),C.byref(tables),0,0,0x100000,0,0x10000,1,1,failed_ray,None,C.byref(output))==-1
    assert state.segment_count==2 and state.trace_mode==1 and not state.updating
    assert (state.offset_x,state.offset_y,output.value)==(17,23,42) and count[0]==failure_index+1
    report['callback_failure_cases']+=1
report['total_rom_comparisons']=sum(report['rom_comparisons'].values())
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('src/enemy_navigation.c','include/ssa/enemy_navigation.h','tests/test_enemy_navigation_rom.py')}
(ROOT/'analysis/native-enemy-navigation-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
