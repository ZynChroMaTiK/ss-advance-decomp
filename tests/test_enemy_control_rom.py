"""Whole CF/D0/D1/D2 controllers with controlled navigator/effect boundaries.

The 1288C navigation call is replaced by a no-change result on both sides;
navigation has its own original-routine suite. Real native/ROM activation,
contact branch, drain, face/distance, death/corpse and sound postlude execute.
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
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef): nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world ctypes','exec'))
class Activation(C.Structure):
    _fields_=[('entity',EP),('health',C.c_int16),('timer',C.c_uint16),('saved_facing',C.c_uint16),('state',C.c_uint8),('facing',C.c_uint8),('boss_ticks',C.c_uint8),('updating',C.c_uint)]
class ActivationGame(C.Structure):
    _fields_=[('spawned',C.c_uint32*12),('boss',EP),('boss_phase',C.c_uint8)]
class Navigation(C.Structure):
    _fields_=[('entity',EP),('target',EP),('speed',C.c_int32),('phase',C.c_uint16),('distance',C.c_uint16),('heading',C.c_uint16),('waypoint_x',C.c_uint16),('waypoint_y',C.c_uint16),('target_timer',C.c_uint16),('desired_facing',C.c_uint8),('move_result',C.c_uint8),('navigation_flags',C.c_uint8),('updating',C.c_uint)]
class Cd(C.Structure):
    _fields_=[('activation',C.POINTER(Activation)),('navigation',C.POINTER(Navigation)),('drain_period',C.c_uint16),('drain_countdown',C.c_uint16),('animation_done',C.c_uint8),('destroy_requested',C.c_uint8),('bob_direction',C.c_uint8),('updating',C.c_uint)]
class Game(C.Structure):
    _fields_=[('activation',C.POINTER(ActivationGame)),('player',EP),('blocking_entity',EP),('loop_source',EP),('player_x',C.c_int32),('player_y',C.c_int32),('attack_period',C.c_uint16*12),('prion_targets',C.c_uint)]
class Event(C.Structure):
    _fields_=[('kind',C.c_int),('other',EP),('args',C.c_int32*6)]
EVENT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Cd),C.POINTER(Event))
AIM=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Cd),C.c_int32,C.c_uint16,C.c_int32,C.POINTER(EP))
class Ops(C.Structure): _fields_=[('navigation',C.c_void_p),('aim',AIM),('event',EVENT),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
stub=BUILD/'enemy_cd_navigation_stub.c'
stub.write_text('''#include "ssa/enemy_navigation.h"
int ssa_test_cd_navigation_failure;
int ssa_enemy_navigation_step(SsaEnemyNavigation *n,SsaEntity *p,
 const SsaMathTables *m,unsigned ticks,int32_t speed,const SsaEnemyNavigationOps *o) {
 (void)n;(void)p;(void)m;(void)ticks;(void)speed;
 return !o || ssa_test_cd_navigation_failure ? -1 : 1;
}
''')
sources=['enemy_control.c','enemy_activation.c','entity_timer.c','enemy_lifecycle.c','enemy_motion.c','player_hazards.c','movement.c','world.c','physics.c','geometry.c','slide.c','horizontal.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],str(stub),'-o',str(BUILD/'libssa_enemy_control.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_enemy_control.so'))
step=lib.ssa_enemy_basic_step
step.argtypes=[C.POINTER(Cd),C.POINTER(Game),C.POINTER(Tables),C.c_uint,C.c_int32,C.POINTER(Ops)]
step.restype=C.c_int
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
nav_failure=C.c_int.in_dll(lib,'ssa_test_cd_navigation_failure')
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
E,P,T,B,G,RECORD,STOP=0x02000000,0x02000100,0x02000200,0x02000300,0x02001000,0x02002000,0x087ff000
def put(addr,fmt,*values): uc.mem_write(addr,struct.pack('<'+fmt,*values))
def get(addr,fmt): return struct.unpack('<'+fmt,uc.mem_read(addr,struct.calcsize('<'+fmt)))[0]
put(0x03004010,'I',G);put(0x03004648,'I',RECORD);put(RECORD+0xac,'I',P)
oracle=[];nav_calls=0;aim_calls=[]
def snapshot(): return bytes(uc.mem_read(E,0x74)),bytes(uc.mem_read(G,0x200)),get(0x03004670,'I')
def hook(machine,address,size,ctx):
    global nav_calls
    r=[machine.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
    if address==0x08002c88:
        a,b=C.c_int32(r[0]).value,C.c_int32(r[1]).value;assert b
        q=(abs(a)//abs(b))*(-1 if (a<0)!=(b<0) else 1)
        machine.reg_write(UC_ARM_REG_R0,q&0xffffffff);machine.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff);machine.reg_write(UC_ARM_REG_R3,abs(q))
    elif address==0x0800a4f4:
        aim_calls.append(tuple(r[1:])+(get(machine.reg_read(UC_ARM_REG_SP),'I'),get(machine.reg_read(UC_ARM_REG_SP)+4,'I')))
        put(0x0300491c,'I',P)
    elif address==0x0801097c:
        oracle.append((7,0,(r[0],C.c_int32(r[2]).value,C.c_int32(r[3]).value,get(machine.reg_read(UC_ARM_REG_SP),'i'),get(machine.reg_read(UC_ARM_REG_SP)+4,'i'),1),snapshot()))
    elif address==0x0801288c:
        nav_calls+=1
    else:
        kind={0x08005394:0,0x08008d5c:1,0x08008dc0:2,0x080037f4:3,0x0801c6b0:4,0x080037e0:5,0x080035a8:6}[address]
        if kind==0:
            # Native event is after the controller reset of animation_done;
            # remaining original setter executes after this callback snapshot.
            put(E+0x5f,'B',0)
            args=(r[1],r[2],0,0,0,0);other=0
        elif kind in (1,2): args=(r[0],0,0,0,0,0);other=0
        elif kind==3: args=(r[1],0,0,0,0,0);other=r[0]
        elif kind==6: args=tuple(r[1:]);other=0
        else: args=(0,0,0,0,0,0);other=0
        oracle.append((kind,other,args,snapshot()))
        if kind==0:return
        if kind==5:uc.mem_write(E,bytes(0x74))
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for addr in [0x0800a4f4,0x0801097c,0x08002c88,0x0801288c,0x08005394,0x08008d5c,0x08008dc0,0x080037f4,0x0801c6b0,0x080037e0,0x080035a8]:
    uc.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
fields=[('body.x',0,'i'),('body.y',4,'i'),('body.z',8,'i'),('body.velocity_x',12,'i'),('body.velocity_y',16,'i'),('body.velocity_z',20,'i'),('radius',0x28,'i'),('body.height',0x2c,'i'),('body.flags',0x30,'H'),('type',0x3c,'H'),('body.hover_offset',0x43,'b'),('body.step_adjust',0x53,'b'),('player_index',0x54,'B'),('sector',0x6a,'h'),('leaf',0x6e,'h'),('body.ceiling',0x70,'h'),('body.floor',0x72,'h')]
afields=[('health',0x32,'h'),('state',0x3e,'B'),('facing',0x3f,'B'),('timer',0x4e,'H'),('boss_ticks',0x51,'B')]
nfields=[('speed',0x24,'i'),('desired_facing',0x40,'B'),('move_result',0x41,'B'),('phase',0x44,'H'),('distance',0x46,'H'),('heading',0x48,'H'),('waypoint_x',0x4a,'H'),('waypoint_y',0x4c,'H'),('navigation_flags',0x50,'B'),('target_timer',0x56,'H')]
cfields=[('bob_direction',0x42,'B'),('drain_period',0x34,'H'),('drain_countdown',0x36,'H'),('animation_done',0x5f,'B')]
def setfield(obj,path,val):
    parts=path.split('.')
    for p in parts[:-1]:obj=getattr(obj,p)
    setattr(obj,parts[-1],val)
def getfield(obj,path):
    for p in path.split('.'):obj=getattr(obj,p)
    return obj
rng=random.Random(0xcd13978)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'scope':'Whole13040/1319C/13B40/13350; A4F4 selected target controlled; projectile1097C request recorded. navigation1288C controlled no-change on both sides. Real activation,contact branch,drain,face/distance/math,death/corpse. Combat/audio/group/destruction controlled. Animation setter executes; native animation_done reset compared at defined event boundary. Deferred native destruction projected as cleared after request.', 'rom_comparisons':{},'native_guards':0,'callback_reentry_guards':0,'callback_failure_cases':0}
def compare(label,enemy_type=0xcf,state=1,ticks=1,overrides=(),loop=0,blocking=True,target=True,xy=None,playerxy=None,recordxy=None,targetxy=None,fail_event=None):
    raw=bytearray(rng.randbytes(0x74));game_raw=bytearray(rng.randbytes(0x200))
    raw[0x3e]=state;struct.pack_into('<H',raw,0x3c,enemy_type)
    for off,fmt,value in overrides:struct.pack_into('<'+fmt,raw,off,value)
    if xy:
        struct.pack_into('<ii',raw,0,*xy)
    pxy=playerxy if playerxy is not None else (rng.randrange(-0x80000000,0x80000000),rng.randrange(-0x80000000,0x80000000))
    rxy=recordxy if recordxy is not None else pxy
    txy=targetxy if targetxy is not None else (rng.randrange(-0x80000000,0x80000000),rng.randrange(-0x80000000,0x80000000))
    struct.pack_into('<I',raw,0x20,T if target else 0);game_raw[7]=ticks
    e,p,t,b=Entity(),Entity(),Entity(),Entity();a,n,cd,ag,g=Activation(),Navigation(),Cd(),ActivationGame(),Game()
    p.body.flags=0x10;p.body.x,p.body.y=pxy;t.body.x,t.body.y=txy;b.body.flags=0x10 if blocking else 0
    for obj,fs in ((e,fields),(a,afields),(n,nfields),(cd,cfields)):
        for path,off,fmt in fs:setfield(obj,path,struct.unpack_from('<'+fmt,raw,off)[0])
    a.entity=C.pointer(e);n.entity=C.pointer(e);n.target=C.pointer(t) if target else EP();cd.activation=C.pointer(a);cd.navigation=C.pointer(n)
    for i in range(12):ag.spawned[i]=struct.unpack_from('<I',game_raw,0x130+12*i)[0]
    g.attack_period[:]=[struct.unpack_from('<H',ROM,0x44bfd8+i*8)[0] for i in range(12)];g.activation=C.pointer(ag);g.player=C.pointer(p);g.blocking_entity=C.pointer(b);g.player_x,g.player_y=rxy
    pointers={0:EP(),E:C.pointer(e),P:C.pointer(p),T:C.pointer(t),B:C.pointer(b)}
    g.loop_source=pointers[loop]
    def ptraddr(ptr):
        if not ptr:return 0
        for addr,obj in ((E,e),(P,p),(T,t),(B,b)):
            if C.addressof(ptr.contents)==C.addressof(obj):return addr
        raise AssertionError('unknown native entity')
    def project():
        out=bytearray(raw);gr=bytearray(game_raw)
        for obj,fs in ((e,fields),(a,afields),(n,nfields),(cd,cfields)):
            for path,off,fmt in fs:struct.pack_into('<'+fmt,out,off,getfield(obj,path))
        struct.pack_into('<I',out,0x20,ptraddr(n.target))
        for i in range(12):struct.pack_into('<I',gr,0x130+12*i,ag.spawned[i])
        if cd.destroy_requested:out=bytearray(0x74)
        return bytes(out),bytes(gr),ptraddr(g.loop_source)
    uc.mem_write(E,bytes(raw));uc.mem_write(G,bytes(game_raw));put(P,'ii',*pxy);put(RECORD,'ii',*rxy);put(T,'ii',*txy);put(B+0x30,'H',b.body.flags)
    put(0x03004328,'I',B);put(0x03004670,'I',loop)
    oracle.clear();aim_calls.clear();put(P+0x30,'H',p.body.flags)
    for reg,val in ((UC_ARM_REG_R0,E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,val)
    uc.emu_start({0xcf:0x08013041,0xd0:0x0801319d,0xd1:0x08013b41,0xd2:0x08013351}[enemy_type],STOP,count=10000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    expected=snapshot();events=list(oracle);actual=[];reentry=[]
    @EVENT
    def callback(ctx,cp,event):
        ev=event.contents;kind,other=ev.kind,ev.other;x,y,z=ev.args[:3]
        actual.append((kind,ptraddr(other),tuple(ev.args),project()))
        reentry.append(step(cp,C.byref(g),C.byref(tables),ticks,0x4000,C.byref(ops)))
        if kind==fail_event:return -1
        if kind==0:
            struct.pack_into('<H',raw,0x58,x);raw[0x5c]=raw[0x5d]=0;raw[0x5e]=y
        return 0
    native_aim=[]
    @AIM
    def aim(ctx,cp,z,heading,distance,out):
        native_aim.append((0,0,z & 0xffffffff,heading,distance));out[0]=C.pointer(p);return 0
    ops=Ops(1,aim,callback,None)
    result=step(C.byref(cd),C.byref(g),C.byref(tables),ticks,0x4000,C.byref(ops))
    assert not cd.updating and not a.updating and not n.updating
    assert all(x==-1 for x in reentry);report['callback_reentry_guards']+=len(reentry)
    if fail_event is not None:
        index=next(i for i,event in enumerate(events) if event[0]==fail_event)
        assert result==-1 and actual==events[:index+1],(label,actual,events)
        # Every callback failure keeps all changes visible at its boundary.
        assert project()==events[index][3],(label,'failed callback changed state')
        report['callback_failure_cases']+=1
        return
    assert result==int(bool(cd.destroy_requested)),(label,result)
    assert project()==expected,(label,state,ticks,overrides,[(i,x,y) for i,(x,y) in enumerate(zip(project()[0],expected[0])) if x!=y],project()[2],expected[2])
    assert native_aim==aim_calls,(native_aim,aim_calls)
    assert actual==events,(label,actual,events)
    report['rom_comparisons'][label]=report['rom_comparisons'].get(label,0)+1


for typ in range(0xcf,0xd3):
    for state in range(6):
        for ticks in (0,1,2,119,255):
            for _ in range(128):
                compare('random_%x'%typ,enemy_type=typ,state=state,ticks=ticks)
    for flags in (0,1,0x400,0x401):
        for timer in (0,1,255,0x7fff,0x8000,0xffff):
            for health in (-1,0,1,100):
                compare('attack_hurt_%x'%typ,enemy_type=typ,overrides=[(0x30,'H',flags),(0x32,'h',health),(0x34,'H',0),(0x4e,'H',timer),(0x50,'B',1)])
for event in [0,1,3,4]:
    compare('failure',fail_event=event,overrides=[(0x30,'H',1),(0x32,'h',0),(0x34,'H',0),(0x44,'H',2),(0x41,'B',0)])
compare('failure',state=4,ticks=1,fail_event=5,overrides=[(0x4e,'H',0)])
compare('failure',fail_event=7,overrides=[(0x30,'H',0),(0x34,'H',0),(0x4e,'H',0),(0x50,'B',1)])
# Native invalid/reentry guards use independent initialized control projections.
e,p,ag,a,n,cd,g=Entity(),Entity(),ActivationGame(),Activation(),Navigation(),Cd(),Game()
e.type=0xcf;a.entity=C.pointer(e);n.entity=C.pointer(e);cd.activation=C.pointer(a);cd.navigation=C.pointer(n);g.activation=C.pointer(ag);g.player=C.pointer(p)
@EVENT
def ok(*args):return 0
@AIM
def aimok(ctx,cp,z,h,r,out):out[0]=C.pointer(p);return 0
ops=Ops(1,aimok,ok,None)
def invoke():return step(C.byref(cd),C.byref(g),C.byref(tables),0,0,C.byref(ops))
for obj,field,value in [(cd,'updating',1),(cd,'destroy_requested',1),(cd,'activation',C.POINTER(Activation)()),(cd,'navigation',C.POINTER(Navigation)()),(a,'updating',1),(n,'updating',1),(a,'entity',EP()),(n,'entity',C.pointer(p)),(e,'type',0xce),(g,'activation',C.POINTER(ActivationGame)()),(g,'player',EP()),(ops,'event',EVENT())]:
    old=getattr(obj,field)
    if isinstance(old,C._Pointer):old=C.cast(C.cast(old,C.c_void_p).value,type(old))
    setattr(obj,field,value);assert invoke()==-1,(field,value);setattr(obj,field,old);report['native_guards']+=1
for args in [(None,C.byref(g),C.byref(tables),0,0,C.byref(ops)),(C.byref(cd),None,C.byref(tables),0,0,C.byref(ops)),(C.byref(cd),C.byref(g),None,0,0,C.byref(ops)),(C.byref(cd),C.byref(g),C.byref(tables),256,0,C.byref(ops)),(C.byref(cd),C.byref(g),C.byref(tables),0,0,None)]:
    assert step(*args)==-1;report['native_guards']+=1
world=World();world.query_active=1;e.owner=C.pointer(world);assert invoke()==-1;report['native_guards']+=1;e.owner=WP()
a.state=1;ops.navigation=None;assert invoke()==-1;report['native_guards']+=1;ops.navigation=1
nav_failure.value=1;assert invoke()==-1 and not cd.updating;report['native_guards']+=1;nav_failure.value=0
san=BUILD/'test_enemy_control_sanitized'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_enemy_control.c'),*[str(ROOT/'src'/f) for f in sources],'-o',str(san)],check=True)
import os
sanrun=subprocess.run([str(san),str(ROOT/'orig/SSA.gba')],check=True,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
report['sanitizers']=json.loads(sanrun.stdout)
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
report['total_rom_comparisons']=sum(report['rom_comparisons'].values());report['status']='passed'
(ROOT/'analysis/native-enemy-control-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
