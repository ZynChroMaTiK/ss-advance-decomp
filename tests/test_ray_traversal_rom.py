"""Whole A240 wall DDA and 9C00 ordered traversal; cell contents controlled.
Original fixed division and multiplication execute. No entity tracing.
"""
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
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['ray_traversal.c','slide.c','geometry.c','map_assets.c','movement.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(BUILD/'libssa_ray_traversal.so')],check=True)
class Ray(C.Structure): _fields_=[(n,C.c_int32) for n in ('x','y','dx','dy')]
class Hit(C.Structure): _fields_=[('fraction',C.c_int32),('line',C.c_uint)]
class State(C.Structure):
    _fields_=[('ray',Ray),('intercepts',C.POINTER(Hit)),('count',C.c_size_t),('capacity',C.c_size_t),('blocking_fraction',C.c_int32),('updating',C.c_uint)]
PRED=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Hit))
COLLECT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(State),C.c_int32,C.c_int32)
lib=C.CDLL(str(BUILD/'libssa_ray_traversal.so'))
order=lib.ssa_ray_traverse_intercepts; order.argtypes=[C.POINTER(State),C.c_int32,PRED,C.c_void_p]
trace=lib.ssa_ray_trace; trace.argtypes=[C.POINTER(State)]+[C.c_int32]*6+[COLLECT,PRED,C.c_void_p]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]); uc.mem_write(0x030003a4,ROM[0x430:0x638])
STOP,CALLBACK,BUFFER=0x087ff000,0x087ff100,0x03004770
REGS=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
rng=random.Random(0xa240)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':{'ordered':0,'dda':0},'native_guards':0,'reentry_guards':0,'callback_failures':0,'scope':__doc__}
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
events=[]; cells=[]; reject=0xffffffff; fail_cell=-1; inserted=[]
def callback(machine,address,size,user):
    p=machine.reg_read(UC_ARM_REG_R0)
    f,line=get(p,'<i'),get(p+8,'<I'); events.append((f,line))
    machine.reg_write(UC_ARM_REG_R0,int(line!=reject)); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,callback,begin=CALLBACK,end=CALLBACK)
def cell_hook(machine,address,size,user):
    coords=tuple(signed(machine.reg_read(r)) for r in REGS[:2]); cells.append(coords)
    assert machine.reg_read(REGS[2])==0x08009a89
    if len(cells)==1:
        for i,(f,line) in enumerate(inserted): put(BUFFER+i*12,'<iII',f,1,line)
        put(0x030054dc,'<I',BUFFER+12*len(inserted))
    machine.reg_write(REGS[0],int(len(cells)!=fail_cell)); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
ch=uc.hook_add(UC_HOOK_CODE,cell_hook,begin=0x0800975c,end=0x0800975c)
hits=(Hit*32)(); state=State(intercepts=hits,capacity=32)
native_events=[]; native_cells=[]
@PRED
def pred(context,p):
    native_events.append((p.contents.fraction,p.contents.line))
    assert order(C.byref(state),65536,pred,None)==-1
    report['reentry_guards']+=1
    return int(p.contents.line!=reject)
@COLLECT
def collect(context,p,x,y):
    native_cells.append((x,y))
    assert trace(p,0,0,0,0,0,0,collect,pred,None)==-1
    report['reentry_guards']+=1
    if len(native_cells)==1:
        for i,(f,line) in enumerate(inserted): p.contents.intercepts[i]=Hit(f,line)
        p.contents.count=len(inserted)
    return int(len(native_cells)!=fail_cell)
edges=(-2147483648,-2147483647,-65537,-65536,-1,0,1,65535,65536,65537,2147483646,2147483647)
for i in range(6000):
    count=rng.randrange(32); maximum=rng.choice(edges[:-1]) if i<2000 else signed(rng.getrandbits(32))
    reject=rng.randrange(count+2)
    initial=signed(rng.getrandbits(32)); state.blocking_fraction=initial; state.count=count
    for j in range(count):
        f=rng.choice(edges) if i%3 else signed(rng.getrandbits(32))
        put(BUFFER+j*12,'<iII',f,1,j); hits[j]=Hit(f,j)
    put(0x030054dc,'<I',BUFFER+count*12); put(0x0300505c,'<i',initial)
    events.clear(); native_events.clear()
    expected=run(0x08009c00,CALLBACK|1,maximum)
    actual=order(C.byref(state),maximum,pred,None)
    assert actual==expected and native_events==events,(i,actual,expected,native_events,events)
    assert state.blocking_fraction==get(0x0300505c,'<i')
    assert [hits[j].fraction for j in range(count)]==[get(BUFFER+j*12,'<i') for j in range(count)]
    assert not state.updating
    report['rom_comparisons']['ordered']+=1
print('PASS 6,000 original ordered intercept traversals',flush=True)
for i in range(5000):
    args=[signed(rng.getrandbits(32)) for _ in range(6)]
    ox,oy,x,y,ex,ey=args
    if i%3==0: x=signed(ox+rng.randrange(-256,256)*0x800000)
    if i%5==0: y=signed(oy+rng.randrange(-256,256)*0x800000)
    if i%7==0: ex=x
    if i%11==0: ey=y
    if i%13==0: ex=x; ey=y
    args=[ox,oy,x,y,ex,ey]
    inserted=[(rng.choice((0,1,32768,65536,65537,2147483647)),j) for j in range(rng.randrange(12))]
    reject=rng.randrange(len(inserted)+2); fail_cell=rng.choice((-1,-1,-1,1,2,16,64))
    initial=signed(rng.getrandbits(32)); state.blocking_fraction=initial
    put(0x03004944,'<ii',ox,oy); put(0x0300505c,'<i',initial)
    events.clear(); cells.clear(); native_events.clear(); native_cells.clear()
    expected=run(0x0800a240,x,y,ex,ey,stack=(1,CALLBACK|1))
    actual=trace(C.byref(state),*args,collect,pred,None)
    assert actual==expected and native_events==events and native_cells==cells,(i,args,actual,expected,native_cells,cells)
    assert bytes(state.ray)==bytes(uc.mem_read(0x03004900,16))
    assert state.blocking_fraction==get(0x0300505c,'<i')
    assert [hits[j].fraction for j in range(state.count)]==[get(BUFFER+j*12,'<i') for j in range(state.count)]
    assert not state.updating
    report['rom_comparisons']['dda']+=1
print('PASS 5,000 original DDA traversals, complete cells/ray/intercepts/result',flush=True)
for invalid in (None,):
    assert order(invalid,65536,pred,None)==-1; report['native_guards']+=1
    assert trace(invalid,0,0,0,0,0,0,collect,pred,None)==-1; report['native_guards']+=1
state.count=1; hits[0]=Hit(2147483647,0)
assert order(C.byref(state),2147483647,pred,None)==-1; report['native_guards']+=1
state.count=33
assert order(C.byref(state),65536,pred,None)==-1; report['native_guards']+=1
state.count=0
@COLLECT
def failed_cell(context,p,x,y): return -2
assert trace(C.byref(state),0,0,0,0,0,0,failed_cell,pred,None)==-1
report['callback_failures']+=1
@PRED
def failed_pred(context,p): return -2
state.count=1; hits[0]=Hit(42,0); state.blocking_fraction=123
assert order(C.byref(state),65536,failed_pred,None)==-1 and hits[0].fraction==42 and state.blocking_fraction==123
report['callback_failures']+=1
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/native-ray-traversal-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
