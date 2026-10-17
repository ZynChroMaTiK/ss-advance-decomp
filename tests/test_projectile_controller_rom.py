"""Whole five projectile controllers vs ROM, with controlled motion/effects."""
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
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef): nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world ctypes','exec'))
class Projectile(C.Structure):
    _fields_=[('entity',EP),('source',EP),('speed',C.c_int32),('health',C.c_int16),('timer',C.c_uint16),('parameter34',C.c_uint16),('animation',C.c_uint16),('state',C.c_uint8),('facing',C.c_uint8),('owner',C.c_uint8),('weapon',C.c_uint8),('animation_done',C.c_uint8),('animation_period',C.c_uint8),('updating',C.c_uint)]
class Controller(C.Structure): _fields_=[('projectile',C.POINTER(Projectile)),('updating',C.c_uint),('destroy_requested',C.c_uint8)]
class Game(C.Structure): _fields_=[('credit',C.c_uint32),('hit_weapon',C.c_uint32)]
MOTION=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Projectile))
EVENT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Controller),C.c_int,C.c_int32,C.c_int32,C.c_int32)
class Ops(C.Structure): _fields_=[('motion',MOTION),('event',EVENT),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/projectile_controller.c'),'-o',str(BUILD/'libssa_projectile_controller.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_projectile_controller.so'))
step=lib.ssa_projectile_controller_step
step.argtypes=[C.POINTER(Controller),C.POINTER(Game),C.POINTER(Ops)];step.restype=C.c_int
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
E,STOP=0x02000000,0x087ff000
DISPATCH={0x65:(0x08005078,0x08010e5c),0x66:(0x08005080,0x08010ef0),0x67:(0x08005098,0x080111c0),0x69:(0x080050a8,0x080113c4),0x6b:(0x08005090,0x08011120)}
for typ,(branch,fn) in DISPATCH.items():
    assert struct.unpack_from('<I',ROM,0x4e68+4*(typ-0x64))[0]==branch
    # Decode actual Thumb BL immediately after adds r0,r2 at jump-table target.
    hi,lo=struct.unpack_from('<HH',ROM,branch-0x08000000+2)
    off=((hi&0x7ff)<<12)|((lo&0x7ff)<<1)
    if off&(1<<22):off-=1<<23
    assert branch+6+off==fn
def put(a,fmt,*v):uc.mem_write(a,struct.pack('<'+fmt,*v))
def get(a,fmt):return struct.unpack('<'+fmt,uc.mem_read(a,struct.calcsize('<'+fmt)))[0]
def snapshot():return bytes(uc.mem_read(E,0x74)),get(0x03003f40,'I'),get(0x03003f4c,'I')
oracle=[]; motion_result=1
def hook(machine,address,size,ctx):
    r=[machine.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
    if address==0x08010a20:
        oracle.append((4,(0,0,0),snapshot()))
        put(E,'i',C.c_int32(get(E,'i')+0x13579).value)
        put(E+0x24,'i',C.c_int32(get(E+0x24,'i')-0x1234).value)
        machine.reg_write(UC_ARM_REG_R0,motion_result)
    else:
        kind={0x08005394:0,0x080035a8:1,0x08014b60:2,0x080037e0:3}[address]
        if kind==0:
            put(E+0x58,'H',r[1]);put(E+0x5c,'BBBB',0,0,r[2],0)
            args=(r[1],r[2],0)
        elif kind==1:args=tuple(r[1:])
        elif kind==2:
            assert r[1:]==[get(E+off,'I') for off in (0,4,8)]
            args=(r[0],0,0)
        else:args=(0,0,0)
        oracle.append((kind,args,snapshot()))
        if kind==3:uc.mem_write(E,bytes(0x74))
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for a in (0x08010a20,0x08005394,0x080035a8,0x08014b60,0x080037e0):uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
efields=[('body.x',0,'i'),('body.y',4,'i'),('body.z',8,'i'),('body.velocity_x',12,'i'),('body.velocity_y',16,'i'),('body.velocity_z',20,'i'),('radius',0x28,'i'),('body.height',0x2c,'i'),('body.flags',0x30,'H'),('type',0x3c,'H'),('body.hover_offset',0x43,'b'),('body.step_adjust',0x53,'b'),('sector',0x6a,'h'),('leaf',0x6e,'h'),('body.ceiling',0x70,'h'),('body.floor',0x72,'h')]
pfields=[('speed',0x24,'i'),('health',0x32,'h'),('parameter34',0x34,'H'),('state',0x3e,'B'),('facing',0x3f,'B'),('timer',0x4e,'H'),('owner',0x54,'B'),('weapon',0x55,'B'),('animation',0x58,'H'),('animation_period',0x5e,'B'),('animation_done',0x5f,'B')]
def setfield(obj,path,v):
    parts=path.split('.')
    for part in parts[:-1]:obj=getattr(obj,part)
    setattr(obj,parts[-1],v)
def getfield(obj,path):
    for part in path.split('.'):obj=getattr(obj,part)
    return obj
rng=random.Random(0x10e5c)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'scope':'Whole 65/66/67/69/6B controllers, verified 04E04 dispatch. 10A20 motion controlled with body/speed mutations; animation setter, explosion, particle and release effects controlled. All projected fields and ordered callbacks compared. Runtime world/impact integration is separate.','rom_comparisons':{},'callback_reentry_guards':0,'callback_failures':0,'native_guards':0,'dispatch_types':[hex(t) for t in DISPATCH]}
def compare(typ,state,speed,moved,owner=None,weapon=None,fail=None):
    global motion_result
    raw=bytearray(rng.randbytes(0x74));struct.pack_into('<H',raw,0x3c,typ);raw[0x3e]=state;struct.pack_into('<i',raw,0x24,speed)
    if owner is not None:raw[0x54]=owner
    if weapon is not None:raw[0x55]=weapon
    e=Entity();p=Projectile();c=Controller();g=Game(rng.getrandbits(32),rng.getrandbits(32))
    for obj,fields in ((e,efields),(p,pfields)):
        for field,off,fmt in fields:setfield(obj,field,struct.unpack_from('<'+fmt,raw,off)[0])
    p.entity=C.pointer(e);c.projectile=C.pointer(p)
    def project():
        out=bytearray(raw)
        for obj,fields in ((e,efields),(p,pfields)):
            for field,off,fmt in fields:struct.pack_into('<'+fmt,out,off,getfield(obj,field))
        if c.destroy_requested:out=bytearray(0x74)
        return bytes(out),g.credit,g.hit_weapon
    uc.mem_write(E,bytes(raw));put(0x03003f40,'I',g.credit);put(0x03003f4c,'I',g.hit_weapon)
    motion_result=moved;oracle.clear()
    for reg,value in ((UC_ARM_REG_R0,E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,value)
    uc.emu_start(DISPATCH[typ][1]|1,STOP,count=1000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
    expected=snapshot();events=list(oracle);actual=[];reentries=[]
    @MOTION
    def motion(ctx,pp):
        actual.append((4,(0,0,0),project()))
        reentries.append(step(C.byref(c),C.byref(g),C.byref(ops)))
        if fail==4:return -1
        e.body.x=C.c_int32(e.body.x+0x13579).value;p.speed=C.c_int32(p.speed-0x1234).value
        return moved
    @EVENT
    def event(ctx,cc,kind,a,b,d):
        if kind==0:raw[0x5c]=raw[0x5d]=0
        actual.append((kind,(a,b,d),project()))
        reentries.append(step(C.byref(c),C.byref(g),C.byref(ops)))
        return -1 if kind==fail else 0
    ops=Ops(motion,event,None)
    result=step(C.byref(c),C.byref(g),C.byref(ops))
    assert not c.updating and not p.updating
    assert all(x==-1 for x in reentries);report['callback_reentry_guards']+=len(reentries)
    if fail is not None:
        ix=next(i for i,v in enumerate(events) if v[0]==fail)
        assert result==-1 and actual==events[:ix+1],(typ,state,fail,actual,events)
        target=events[ix][2]
        if fail==1:target=(target[0],0,0)
        assert project()==target,(typ,state,fail,project(),target)
        assert not c.destroy_requested
        report['callback_failures']+=1
    else:
        assert result==int(bool(c.destroy_requested))
        assert project()==expected,(typ,state,speed,moved,project(),expected)
        assert actual==events,(typ,state,speed,moved,actual,events)
        key=hex(typ);report['rom_comparisons'][key]=report['rom_comparisons'].get(key,0)+1
for typ in DISPATCH:
    for state in range(256):
        for speed in (0,1,-1,0x10000,0x7fffffff,-0x80000000):
            for moved in (0,1):compare(typ,state,speed,moved)
    for owner in range(256):
        for state in (0,1):
            for moved in (0,1):compare(typ,state,1,moved,owner,255-owner)
    for fail in (0,3,4) if typ==0x69 else range(5):compare(typ,0,1,0,fail=fail)
    for _ in range(1024):compare(typ,rng.randrange(2),rng.randrange(-0x80000000,0x80000000),rng.randrange(2))
# Invalid calls reject without mutation, including unsupported adjacent types.
e=Entity();p=Projectile();p.entity=C.pointer(e);c=Controller();c.projectile=C.pointer(p);g=Game()
@MOTION
def no_motion(ctx,p):raise AssertionError('invalid call invoked motion')
@EVENT
def no_event(ctx,c,k,a,b,d):raise AssertionError('invalid call invoked event')
ops=Ops(no_motion,no_event,None)
for typ in (0,0x64,0x68,0x6a,0xffff):
    e.type=typ;assert step(C.byref(c),C.byref(g),C.byref(ops))==-1;report['native_guards']+=1
e.type=0x65
for obj,field in ((p,'updating'),(c,'updating'),(c,'destroy_requested')):
    setattr(obj,field,1);assert step(C.byref(c),C.byref(g),C.byref(ops))==-1;setattr(obj,field,0);report['native_guards']+=1
for args in ((None,C.byref(g),C.byref(ops)),(C.byref(c),None,C.byref(ops)),(C.byref(c),C.byref(g),None)):
    assert step(*args)==-1;report['native_guards']+=1
w=World();w.query_active=1;e.owner=C.pointer(w)
assert step(C.byref(c),C.byref(g),C.byref(ops))==-1;report['native_guards']+=1
report['total_rom_comparisons']=sum(report['rom_comparisons'].values())
report['source_sha256']=hashlib.sha256((ROOT/'src/projectile_controller.c').read_bytes()).hexdigest()
(ROOT/'analysis/native-projectile-controller-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
