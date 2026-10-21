"""Whole CE/D3/D4/D5/D6 controllers with controlled navigation/world/effects.

Original shared activation, contact gate, drain, math, reaction, attack gate,
corpse and sound logic execute.1288C navigation and04498/0354C world boundaries
are controlled without changing state on both sides. A4F4 returns a controlled
target, and1097C emits a projectile request. No fabricated AI runs in production.
"""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess,os
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef):nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets):nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world ctypes','exec'))
nodes=[n for n in ast.parse((ROOT/'tests/test_enemy_cd_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name in ('Activation','ActivationGame','Navigation')]
exec(compile(ast.Module(nodes,type_ignores=[]),'enemy projections','exec'))
class Control(C.Structure):
    _fields_=[('activation',C.POINTER(Activation)),('navigation',C.POINTER(Navigation)),('drain_period',C.c_uint16),('drain_countdown',C.c_uint16),('animation_done',C.c_uint8),('destroy_requested',C.c_uint8),('bob_direction',C.c_uint8),('updating',C.c_uint)]
class Game(C.Structure):
    _fields_=[('activation',C.POINTER(ActivationGame)),('player',EP),('blocking_entity',EP),('loop_source',EP),('player_x',C.c_int32),('player_y',C.c_int32),('attack_period',C.c_uint16*12),('prion_targets',C.c_uint)]
class Event(C.Structure):_fields_=[('kind',C.c_int),('other',EP),('args',C.c_int32*6)]
EVENT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Control),C.POINTER(Event))
AIM=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Control),C.c_int32,C.c_uint16,C.c_int32,C.POINTER(EP))
MOVE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Navigation),C.POINTER(C.c_int32))
FINISH=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Navigation))
class NavOps(C.Structure):_fields_=[(n,C.c_void_p) for n in ('visibility','nearest','path','turn')]+[('move',MOVE),('finish',FINISH),('context',C.c_void_p)]
class Ops(C.Structure):_fields_=[('navigation',C.POINTER(NavOps)),('aim',AIM),('event',EVENT),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
stub=B/'enemy_ranged_navigation_stub.c'
stub.write_text('''#include "ssa/enemy_navigation.h"
int ssa_enemy_navigation_step(SsaEnemyNavigation*n,SsaEntity*p,const SsaMathTables*m,
 unsigned t,int32_t s,const SsaEnemyNavigationOps*o){(void)n;(void)p;(void)m;(void)t;(void)s;return o?1:-1;}
''')
sources=['enemy_ranged.c','enemy_control.c','enemy_activation.c','entity_timer.c','enemy_lifecycle.c','enemy_motion.c','player_hazards.c','movement.c','world.c','physics.c','geometry.c','slide.c','horizontal.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],str(stub),'-o',str(B/'libssa_enemy_ranged.so')],check=True)
lib=C.CDLL(str(B/'libssa_enemy_ranged.so'));step=lib.ssa_enemy_ranged_step
step.argtypes=[C.POINTER(Control),C.POINTER(Game),C.POINTER(Tables),C.c_uint,C.c_int32,C.POINTER(Ops)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
tables=Tables();assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))==1
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(ROM)),(0x02000000,0x10000),(0x03000000,0x10000)):u.mem_map(at,size)
u.mem_write(0x08000000,ROM);u.mem_write(0x030000f0,ROM[0x370:0x380])
E,P,T,BLOCK,G,RECORD,STOP=0x02000000,0x02000100,0x02000200,0x02000300,0x02001000,0x02002000,0x087ff000
def put(at,fmt,*v):u.mem_write(at,struct.pack('<'+fmt,*v))
def get(at,fmt='I'):return struct.unpack('<'+fmt,u.mem_read(at,struct.calcsize('<'+fmt)))[0]
put(0x03004010,'I',G);put(0x03004648,'I',RECORD)
def snapshot():return bytes(u.mem_read(E,0x74)),bytes(u.mem_read(G,0x200)),get(0x03004674),bytes(u.mem_read(RECORD,256))
oracle=[];aim_flags=0
def hook(m,at,size,ctx):
    r=[m.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)];sp=m.reg_read(UC_ARM_REG_SP)
    if at==0x08002c88:
        a,b=C.c_int32(r[0]).value,C.c_int32(r[1]).value;assert b
        q=(abs(a)//abs(b))*(-1 if (a<0)!=(b<0) else 1)
        m.reg_write(UC_ARM_REG_R0,q&0xffffffff);m.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff);m.reg_write(UC_ARM_REG_R3,abs(q))
    elif at==0x0801288c:pass
    else:
        kind={0x08005394:0,0x08008d5c:1,0x08008dc0:2,0x080037f4:3,0x0801c6b0:4,0x080037e0:5,0x0801097c:7,0x08013568:10,0x08004498:100,0x0800354c:101,0x0800a4f4:102}[at]
        other=0;args=[0]*6
        if kind==0:put(E+0x5f,'B',0);args[:2]=r[1:3]
        elif kind in (1,2):args[0]=r[0]
        elif kind==3:other=r[0];args[0]=r[1]
        elif kind==7:args=[r[0],r[2],r[3],get(sp),get(sp+4),1]
        elif kind==10:other=P;args[:2]=[r[0],m.reg_read(UC_ARM_REG_R6)]
        elif kind==102:args[:3]=[r[3],get(sp)&65535,get(sp+4)];put(0x0300491c,'I',T)
        args=tuple(C.c_int32(v).value for v in args)
        oracle.append((kind,other,args,snapshot()))
        if kind in (0,10):return
        if kind==5:u.mem_write(E,bytes(0x74))
    m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
for at in (0x08002c88,0x0801288c,0x08005394,0x08008d5c,0x08008dc0,0x080037f4,0x0801c6b0,0x080037e0,0x0801097c,0x08013568,0x08004498,0x0800354c,0x0800a4f4):u.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
# Reuse only scalar projection declarations, not CD behavior or test execution.
tree=ast.parse((ROOT/'tests/test_enemy_cd_rom.py').read_text())
nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('getfield','setfield') or isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id in ('fields','afields','nfields','cfields') for t in n.targets)]
exec(compile(ast.Module(nodes,type_ignores=[]),'scalar field mappings','exec'))
cfields.append(('bob_direction',0x42,'B'))
entry={0xce:0x135c4,0xd3:0x13fa4,0xd4:0x141e8,0xd5:0x13db0,0xd6:0x13780}
rng=random.Random(0xce135c4)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'scope':__doc__,'comparisons':{},'callback_reentry_guards':0,'callback_failure_cases':0,'native_guards':0,'projectile_requests':0,'knockbacks':0}
def compare(typ,state=1,ticks=1,overrides=(),xy=None,pxy=None,pz=0,rxy=None,loop=0,block=True,aim_player=True,fail=None,label='random'):
    raw=bytearray(rng.randbytes(0x74));gr=bytearray(rng.randbytes(0x200));record=bytearray(rng.randbytes(256))
    struct.pack_into('<H',raw,0x3c,typ);raw[0x3e]=state;struct.pack_into('<I',raw,0x20,T);gr[7]=ticks
    for off,fmt,val in overrides:struct.pack_into('<'+fmt,raw,off,val)
    if xy:struct.pack_into('<ii',raw,0,*xy)
    if pxy is None:pxy=(rng.randrange(-0x80000000,0x80000000),rng.randrange(-0x80000000,0x80000000))
    if rxy is None:rxy=pxy
    struct.pack_into('<ii',record,0,*rxy);struct.pack_into('<I',record,0xac,P)
    e,p,t,b=Entity(),Entity(),Entity(),Entity();a,n,c,ag,g=Activation(),Navigation(),Control(),ActivationGame(),Game()
    p.body.x,p.body.y=pxy;p.body.z=pz;t.body.flags=16 if aim_player else 0;b.body.flags=16 if block else 0
    for obj,fs in ((e,fields),(a,afields),(n,nfields),(c,cfields)):
        for path,off,fmt in fs:setfield(obj,path,struct.unpack_from('<'+fmt,raw,off)[0])
    a.entity=C.pointer(e);n.entity=C.pointer(e);n.target=C.pointer(t);c.activation=C.pointer(a);c.navigation=C.pointer(n)
    for i in range(12):ag.spawned[i]=struct.unpack_from('<I',gr,0x130+12*i)[0];g.attack_period[i]=struct.unpack_from('<H',ROM,0x44bfd8+8*i)[0]
    g.activation=C.pointer(ag);g.player=C.pointer(p);g.blocking_entity=C.pointer(b);g.player_x,g.player_y=rxy
    pointers={0:EP(),E:C.pointer(e),P:C.pointer(p),T:C.pointer(t),BLOCK:C.pointer(b)};g.loop_source=pointers[loop]
    def addr(ptr):
        if not ptr:return 0
        return next(at for at,obj in ((E,e),(P,p),(T,t),(BLOCK,b)) if C.addressof(ptr.contents)==C.addressof(obj))
    def project():
        out=bytearray(raw);game=bytearray(gr)
        for obj,fs in ((e,fields),(a,afields),(n,nfields),(c,cfields)):
            for path,off,fmt in fs:struct.pack_into('<'+fmt,out,off,getfield(obj,path))
        struct.pack_into('<I',out,0x20,addr(n.target))
        for i in range(12):struct.pack_into('<I',game,0x130+12*i,ag.spawned[i])
        if c.destroy_requested:out=bytes(0x74)
        return bytes(out),bytes(game),addr(g.loop_source),bytes(record)
    u.mem_write(E,bytes(raw));u.mem_write(G,bytes(gr));u.mem_write(RECORD,bytes(record));put(P,'iii',*pxy,pz);put(T+0x30,'H',t.body.flags);put(BLOCK+0x30,'H',b.body.flags)
    put(0x03004328,'I',BLOCK);put(0x03004674,'I',loop)
    oracle.clear();u.reg_write(UC_ARM_REG_R0,E);u.reg_write(UC_ARM_REG_SP,0x03007e00);u.reg_write(UC_ARM_REG_LR,STOP|1)
    u.emu_start(0x08000001+entry[typ],STOP,count=100000);assert u.reg_read(UC_ARM_REG_PC)==STOP
    expected=snapshot();events=list(oracle);actual=[];reentries=[]
    def log(kind,other,args):
        actual.append((kind,other,tuple(args),project()))
        reentries.append(step(C.byref(c),C.byref(g),C.byref(tables),ticks,0x4000,C.byref(ops)))
        return -1 if kind==fail else 0
    @EVENT
    def event(ctx,cp,ep):
        ev=ep.contents;rc=log(ev.kind,addr(ev.other),ev.args)
        if rc<0:return rc
        if ev.kind==0:struct.pack_into('<H',raw,0x58,ev.args[0]);raw[0x5c]=raw[0x5d]=0;raw[0x5e]=ev.args[1]
        if ev.kind==10:struct.pack_into('<H',record,0xdc,ev.args[1]);struct.pack_into('<i',record,0x20,ev.args[0])
        return 0
    @AIM
    def aim(ctx,cp,z,heading,range_,out):out[0]=C.pointer(t);return log(102,0,(z,heading,range_,0,0,0))
    @MOVE
    def move(ctx,np,out):out[0]=1;return log(100,0,(0,0,0,0,0,0))
    @FINISH
    def finish(ctx,np):return log(101,0,(0,0,0,0,0,0))
    nav=NavOps(None,None,None,None,move,finish,None);ops=Ops(C.pointer(nav),aim,event,None)
    result=step(C.byref(c),C.byref(g),C.byref(tables),ticks,0x4000,C.byref(ops))
    assert not c.updating and not a.updating and not n.updating
    assert all(x==-1 for x in reentries);report['callback_reentry_guards']+=len(reentries)
    if fail is not None:
        index=next(i for i,event in enumerate(events) if event[0]==fail)
        assert result==-1 and actual==events[:index+1],(hex(typ),state,label,'failure',actual,events)
        assert project()==events[index][3],(hex(typ),state,label,'failure mutations');report['callback_failure_cases']+=1;return
    assert result==int(bool(c.destroy_requested)),(hex(typ),state,label,'return',result)
    assert project()==expected,(hex(typ),state,label,'state',[(i,x,y) for i,(x,y) in enumerate(zip(project()[0],expected[0])) if x!=y],project()[2],expected[2],[(i,x,y) for i,(x,y) in enumerate(zip(project()[3],expected[3])) if x!=y])
    assert actual==events,(hex(typ),state,label,'events',[(a,b) for a,b in zip(actual,events) if a!=b],len(actual),len(events))
    report['comparisons'][hex(typ)]=report['comparisons'].get(hex(typ),0)+1
    report['projectile_requests']+=sum(ev[0]==7 for ev in events);report['knockbacks']+=sum(ev[0]==10 for ev in events)
for typ in entry:
    for state in range(9):
        for ticks in (0,1,2,119,255):
            for done in (0,1,0xfa,0xfe,0xff):
                compare(typ,state,ticks,[(0x5f,'B',done)],label='state/marker/tick')
    for _ in range(2048):compare(typ,rng.randrange(9),rng.randrange(256),loop=rng.choice([0,E,T]),aim_player=bool(rng.randrange(2)),block=bool(rng.randrange(2)))
    print('PASS whole controller',hex(typ),report['comparisons'][hex(typ)],flush=True)
for typ in entry:
    for health in (-32768,-1,0,1,32767):
        for flags in (0,1,0x400,0x401):
            for phase in (0,2):
                compare(typ,1,1,[(0x30,'H',flags),(0x32,'h',health),(0x34,'H',0),(0x44,'H',phase),(0x41,'B',0),(0x50,'B',1),(0x4e,'H',0)],xy=(0,0),pxy=(0,1000*65536),label='hurt/contact/shot')
for distance in (0,1,255,256,399,400,401,511,512,767,768,32767,32768):
    x=distance*65536 if distance<32768 else -0x80000000
    for state in (1,4):compare(0xd5,state,1,[(0x30,'H',0),(0x34,'H',0),(8,'i',0),(0x72,'h',0),(0x44,'H',2),(0x46,'H',distance)],xy=(0,0),pxy=(x,0),label='burrow thresholds')
    for loop in (0,E,T):compare(0xce,255,0,[(0x44,'H',2),(0x46,'H',distance)],loop=loop,label='loop thresholds')
for facing in range(256):
    compare(0xce,1,1,[(0x30,'H',0),(0x34,'H',0),(0x3f,'B',facing),(0x44,'H',2),(0x41,'B',0),(0x24,'i',0x30000)],xy=(0,0),pxy=(0,100*65536),label='knockback headings')
for flags in (0,1,0x400):
    for aim in (False,True):
        for typ in (0xd3,0xd4,0xd6):
            for timer in (0,1,30,0x7fff,0x8000,0xffff):
                compare(typ,1,1,[(0x30,'H',flags),(0x34,'H',0),(0x50,'B',1),(0x4e,'H',timer)],aim_player=aim,label='aim gate')
for fail in (0,1,3,4):compare(0xce,1,1,[(0x30,'H',1),(0x32,'h',0),(0x34,'H',0),(0x44,'H',2),(0x41,'B',0)],fail=fail,label='callback failure')
for fail in (5,2):compare(0xce,3,1,[(0x4e,'H',0)],loop=E,fail=fail,label='destroy failure')
compare(0xce,1,1,[(0x30,'H',0),(0x34,'H',0),(0x3f,'B',128),(0x44,'H',2),(0x41,'B',0)],xy=(0,0),pxy=(0,100*65536),fail=10,label='knockback failure')
for fail in (7,102):compare(0xd3,1,1,[(0x30,'H',0),(0x34,'H',0),(0x50,'B',1),(0x4e,'H',0)],fail=fail,label='shot failure')
compare(0xd5,2,1,fail=100,label='walk failure')
compare(0xd4,1,1,[(8,'i',0x800000)],pz=0,fail=101,label='hazard failure')
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],str(stub),str(ROOT/'tests/test_enemy_ranged.c'),'-o',str(B/'test_enemy_ranged')],check=True)
sanitized=subprocess.run([str(B/'test_enemy_ranged'),str(ROOT/'orig/SSA.gba')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
if sanitized.returncode:print(sanitized.stdout);print(sanitized.stderr);sanitized.check_returncode()
report.update(json.loads(sanitized.stdout));report['native_guards']=20480*4
report['total_rom_comparisons']=sum(report['comparisons'].values());report['source_sha256']={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources}
report['fixture_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();report['navigation_stub_sha256']=hashlib.sha256(stub.read_bytes()).hexdigest()
report['sanitizer_fixture_sha256']=hashlib.sha256((ROOT/'tests/test_enemy_ranged.c').read_bytes()).hexdigest()
(ROOT/'analysis/native-enemy-ranged-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
