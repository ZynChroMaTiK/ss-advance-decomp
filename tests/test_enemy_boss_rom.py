"""Whole D7/D8 boss dispatchers; original math, creation, animation and lifecycle.

Group queues and presentation are ordered effect boundaries. No ROM instruction
execution belongs to the game runtime; Unicorn is this test's oracle only.
"""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef):nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets):nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world declarations','exec'))
for filename,names in [('test_enemy_cd_rom.py',('Activation','ActivationGame','Navigation')),('test_enemy_ranged_rom.py',('Control','Game','Event'))]:
    nodes=[n for n in ast.parse((ROOT/'tests'/filename).read_text()).body if isinstance(n,ast.ClassDef) and n.name in names]
    exec(compile(ast.Module(nodes,type_ignores=[]),filename,'exec'))
class Ops(C.Structure):_fields_=[('navigation',C.c_void_p),('aim',C.c_void_p),('event',C.c_void_p),('context',C.c_void_p)]
i32,u32,u16,u8=C.c_int32,C.c_uint32,C.c_uint16,C.c_uint8
nodes=[n for n in ast.parse((ROOT/'tests/test_projectile_create_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name in ('Projectile','Creator','Request')]
exec(compile(ast.Module(nodes,type_ignores=[]),'projectile declarations','exec'))
PP=C.POINTER(Projectile)
class Boss(C.Structure):_fields_=[('control',C.POINTER(Control)),('animation_index',u8),('summon_cycles',u8)]
class Rules(C.Structure):_fields_=[('groups',(u8*2)*2),('scripts',(u8*2)*2),('loaded',C.c_uint)]
EVENT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Control),C.POINTER(Event))
CREATE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Boss),C.POINTER(Request),C.POINTER(PP))
COMMIT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Boss),PP)
ALLOC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(PP));INSERT=C.CFUNCTYPE(C.c_int,C.c_void_p,PP)
class CreateOps(C.Structure):_fields_=[('allocate',ALLOC),('insert',INSERT),('context',C.c_void_p)]
class BossOps(C.Structure):_fields_=[('control',C.POINTER(Ops)),('create',CREATE),('commit',COMMIT),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['enemy_boss.c','enemy_activation.c','entity_timer.c','enemy_lifecycle.c','enemy_motion.c','projectile_create.c','player_hazards.c','movement.c','world.c','physics.c','geometry.c','slide.c','horizontal.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_enemy_boss.so')],check=True)
lib=C.CDLL(str(B/'libssa_enemy_boss.so'));step=lib.ssa_enemy_boss_step
step.argtypes=[C.POINTER(Boss),C.POINTER(Game),C.POINTER(Rules),C.POINTER(u8),C.c_size_t,C.POINTER(Tables),C.c_uint,C.POINTER(BossOps)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
lib.ssa_enemy_boss_rules_load.argtypes=[C.POINTER(Rules),C.c_void_p,C.c_size_t]
lib.ssa_projectile_create.argtypes=[C.POINTER(Creator),C.POINTER(Request),C.POINTER(Tables),C.POINTER(CreateOps),C.POINTER(PP)]
tables=Tables();rules=Rules();assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))==1
assert lib.ssa_enemy_boss_rules_load(C.byref(rules),ROM,len(ROM))==0
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(ROM)),(0x02000000,0x10000),(0x03000000,0x10000)):u.mem_map(at,size)
u.mem_write(0x08000000,ROM);u.mem_write(0x030000f0,ROM[0x370:0x380])
u.mem_write(0x030003a4,ROM[0x430:0x638])
E,P,T,SHOT,G,RECORD,ALIVE,STOP=0x02000000,0x02000100,0x02000200,0x02000300,0x02001000,0x02002000,0x02003000,0x087ff000
def put(at,fmt,*v):u.mem_write(at,struct.pack('<'+fmt,*v))
def get(at,fmt='I'):return struct.unpack('<'+fmt,u.mem_read(at,struct.calcsize('<'+fmt)))[0]
put(0x03004010,'I',G);put(0x03004648,'I',RECORD);put(RECORD+0xac,'I',P);put(0x030057b0,'I',ALIVE)
def snapshot():return bytes(u.mem_read(E,0x74)),bytes(u.mem_read(G,0x200)),get(0x03005710),get(0x03005714,'B'),bytes(u.mem_read(SHOT,0x74))
oracle=[]
def hook(m,at,size,ctx):
    r=[m.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)];sp=m.reg_read(UC_ARM_REG_SP)
    if at==0x08002c88:
        a,b=C.c_int32(r[0]).value,C.c_int32(r[1]).value;assert b
        q=(abs(a)//abs(b))*(-1 if (a<0)!=(b<0) else 1)
        m.reg_write(UC_ARM_REG_R0,q&0xffffffff);m.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff);m.reg_write(UC_ARM_REG_R3,abs(q))
    elif at==0x08005170:u.mem_write(SHOT,bytes(0x74));m.reg_write(UC_ARM_REG_R0,SHOT)
    elif at==0x0800a5d0:pass
    else:
        kind={0x08005394:0,0x08008d5c:1,0x0801c6b0:4,0x080037e0:5,0x08008eb0:8,0x0800e914:9,0x0801c670:11,0x08010830:100}[at]
        args=[0]*6
        if kind==0:put(E+0x5f,'B',0);args[:2]=r[1:3]
        elif kind in (1,8,11):args[0]=r[0]
        elif kind==9:args[:4]=r
        elif kind==100:args=[r[0],r[2],r[3],get(sp),get(sp+4),0]
        oracle.append((kind,tuple(C.c_int32(v).value for v in args),snapshot()))
        if kind in (0,100):return
        if kind==5:u.mem_write(E,bytes(0x74))
    m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
for at in (0x08002c88,0x08005170,0x0800a5d0,0x08005394,0x08008d5c,0x0801c6b0,0x080037e0,0x08008eb0,0x0800e914,0x0801c670,0x08010830):u.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
tree=ast.parse((ROOT/'tests/test_enemy_cd_rom.py').read_text())
nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('getfield','setfield') or isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id in ('fields','afields','nfields','cfields') for t in n.targets)]
exec(compile(ast.Module(nodes,type_ignores=[]),'scalar projections','exec'))
nfields=[f for f in nfields if f[1]!=0x48];afields.append(('saved_facing',0x48,'H'))
pfields=[('speed',0x24,'i'),('health',0x32,'h'),('parameter34',0x34,'H'),('state',0x3e,'B'),('facing',0x3f,'B'),('timer',0x4e,'H'),('owner',0x54,'B'),('weapon',0x55,'B')]
rng=random.Random(0xd714ab8);report={'result':'pass','comparisons':{},'reentry_guards':0,'failure_cases':0,'projectiles':0,'script_requests':0}
def compare(typ,state=1,ticks=1,overrides=(),phase=0,group_alive=0,fail=None,pose=None,label='random'):
    raw=bytearray(rng.randbytes(0x74));gr=bytearray(rng.randbytes(0x200));raw[0x3e]=state;struct.pack_into('<H',raw,0x3c,typ);gr[7]=ticks
    struct.pack_into('<I',raw,0x20,T)
    for off,fmt,value in overrides:struct.pack_into('<'+fmt,raw,off,value)
    e,p,t,shot=Entity(),Entity(),Entity(),Entity();a,n,c,ag,g=Activation(),Navigation(),Control(),ActivationGame(),Game()
    b=Boss();projectile=Projectile();creator=Creator();alive=(u8*256)(*([group_alive]*256))
    p.body.x,p.body.y,p.body.z,p.body.height=pose or (0,1000*65536,0,100*65536)
    rx,ry=p.body.x+12345,p.body.y-9876
    for obj,fs in ((e,fields),(a,afields),(n,nfields),(c,cfields)):
        for path,off,fmt in fs:setfield(obj,path,struct.unpack_from('<'+fmt,raw,off)[0])
    a.entity=C.pointer(e);n.entity=C.pointer(e);n.target=C.pointer(t);c.activation=C.pointer(a);c.navigation=C.pointer(n)
    b.control=C.pointer(c);b.animation_index=raw[0x5c];b.summon_cycles=raw[0x52];ag.boss=C.pointer(e);ag.boss_phase=phase
    for i in range(12):ag.spawned[i]=struct.unpack_from('<I',gr,0x130+i*12)[0]
    g.activation=C.pointer(ag);g.player=C.pointer(p);g.player_x,g.player_y=rx,ry
    projectile.entity=C.pointer(shot);created=False;actual=[];reentries=[]
    def addr(ptr):
        if not ptr:return 0
        return next(at for at,obj in ((E,e),(P,p),(T,t),(SHOT,shot)) if C.addressof(ptr.contents)==C.addressof(obj))
    def project():
        out=bytearray(raw);game=bytearray(gr);shotraw=bytearray(0x74)
        for obj,fs in ((e,fields),(a,afields),(n,nfields),(c,cfields)):
            for path,off,fmt in fs:struct.pack_into('<'+fmt,out,off,getfield(obj,path))
        out[0x5c]=b.animation_index;out[0x52]=b.summon_cycles
        struct.pack_into('<I',out,0x20,T)
        for i in range(12):struct.pack_into('<I',game,0x130+i*12,ag.spawned[i])
        if created:
            for obj,fs in ((shot,fields),(projectile,pfields)):
                for path,off,fmt in fs:struct.pack_into('<'+fmt,shotraw,off,getfield(obj,path))
            struct.pack_into('<I',shotraw,0x20,E)
        return bytes(0x74) if c.destroy_requested else bytes(out),bytes(game),addr(ag.boss),ag.boss_phase,bytes(shotraw)
    u.mem_write(E,bytes(raw));u.mem_write(G,bytes(gr));u.mem_write(RECORD,struct.pack('<ii',rx,ry));u.mem_write(P,bytes(0x74));put(P,'iii',p.body.x,p.body.y,p.body.z);put(P+0x2c,'i',p.body.height)
    u.mem_write(SHOT,bytes(0x74));u.mem_write(ALIVE,bytes(alive));put(0x03005710,'I',E);put(0x03005714,'B',phase)
    oracle.clear();u.reg_write(UC_ARM_REG_R0,E);u.reg_write(UC_ARM_REG_SP,0x03007e00);u.reg_write(UC_ARM_REG_LR,STOP|1)
    try:u.emu_start((0x08014ab8 if typ==0xd7 else 0x08014648)|1,STOP,count=200000)
    except Exception:
        print('oracle failure',hex(typ),state,ticks,label,hex(u.reg_read(UC_ARM_REG_PC)),hex(u.reg_read(UC_ARM_REG_LR)),flush=True);raise
    assert u.reg_read(UC_ARM_REG_PC)==STOP
    expected=snapshot();events=list(oracle)
    def log(kind,args):
        actual.append((kind,tuple(args),project()));reentries.append(step(C.byref(b),C.byref(g),C.byref(rules),alive,256,C.byref(tables),ticks,C.byref(ops)))
        return -1 if kind==fail else 0
    @EVENT
    def event(ctx,cp,ep):
        ev=ep.contents;rc=log(ev.kind,ev.args)
        if rc<0:return rc
        if ev.kind==0:
            struct.pack_into('<H',raw,0x58,ev.args[0]);raw[0x5d]=0;raw[0x5e]=ev.args[1];b.animation_index=0
        return 0
    @ALLOC
    def allocate(ctx,out):out[0]=C.pointer(projectile);return 0
    @INSERT
    def insert(ctx,pp):return 0
    cop=CreateOps(allocate,insert,None)
    @CREATE
    def create(ctx,bp,rp,out):
        nonlocal created
        q=rp.contents;rc=log(100,(q.type,q.forward,q.side,q.z_offset,q.heading,0))
        if rc<0:return rc
        rc=lib.ssa_projectile_create(C.byref(creator),rp,C.byref(tables),C.byref(cop),out);created=True;return rc
    @COMMIT
    def commit(ctx,bp,pp):return 0
    common=Ops(None,None,C.cast(event,C.c_void_p),None);ops=BossOps(C.pointer(common),create,commit,None)
    result=step(C.byref(b),C.byref(g),C.byref(rules),alive,256,C.byref(tables),ticks,C.byref(ops))
    assert all(x==-1 for x in reentries) and not c.updating and not a.updating;report['reentry_guards']+=len(reentries)
    if fail is not None:
        index=next(i for i,x in enumerate(events) if x[0]==fail)
        assert result==-1 and actual==events[:index+1],(hex(typ),state,label,'failure events',actual,events)
        assert project()==events[index][2],(hex(typ),state,label,'failure projection');report['failure_cases']+=1;return
    assert result==int(bool(c.destroy_requested)),(hex(typ),state,label,'return',result)
    assert project()==expected,(hex(typ),state,label,'state',[(i,x,y) for i,(x,y) in enumerate(zip(project()[0],expected[0])) if x!=y],[(i,x,y) for i,(x,y) in enumerate(zip(project()[4],expected[4])) if x!=y],project()[2:4],expected[2:4])
    assert actual==events,(hex(typ),state,label,'events',[(a,b) for a,b in zip(actual,events) if a!=b],len(actual),len(events))
    report['comparisons'][hex(typ)]=report['comparisons'].get(hex(typ),0)+1
    report['projectiles']+=sum(x[0]==100 for x in events);report['script_requests']+=sum(x[0]==11 for x in events)
for typ in (0xd7,0xd8):
    for state in range(10):
        for ticks in (0,1,2,119,255):
            for done in (0,1,0xfa,0xfe,0xff):compare(typ,state,ticks,[(0x5f,'B',done)],group_alive=1,label='state/marker/tick')
    for _ in range(2048):compare(typ,rng.randrange(10),rng.randrange(256),phase=rng.randrange(2),group_alive=rng.randrange(2))
    for timer in (0,1,60,119,120,255,359,360,719,720,0x7fff,0x8000,0xffff):
        for state in range(8):compare(typ,state,1,[(0x4e,'H',timer),(0x30,'H',1),(0x32,'h',1)],label='timer/hurt')
    for phase in (0,1):
        for state in (4,5):
            for done in (0,1):
                for alive in (0,1,255):compare(typ,state,1,[(0x5f,'B',done),(0x30,'H',0)],phase=phase,group_alive=alive,label='group phases')
    for index in (0,6,7,8,9,255):
        for flags in (0,1,0x1000,0x1001):
            for done in (0,1):compare(typ,2,1,[(0x5c,'B',index),(0x30,'H',flags),(0x5f,'B',done),(0x32,'h',100)],label='attack markers')
    for fail in (0,1,4,8):compare(typ,5,1,[(0x30,'H',1),(0x32,'h',0)],group_alive=1,fail=fail,label='death effect failure')
    for fail in (0,8,9):compare(typ,0,1,[(0x30,'H',0),(0x4e,'H',0)],fail=fail,label='activation failure')
    compare(typ,4,1,[(0x5f,'B',1)],fail=11,label='script failure')
    compare(typ,7,1,[(0x4e,'H',0)],fail=5,label='destruction failure')
    for fail in (100,1):compare(typ,2,1,[(0x5c,'B',9),(0x30,'H',0)],fail=fail,label='shot failure')
    print('PASS whole boss',hex(typ),report['comparisons'][hex(typ)],flush=True)
report['total_rom_comparisons']=sum(report['comparisons'].values());report['test_only_arm_execution']=True
paths=[ROOT/'src'/s for s in sources]+[ROOT/'include/ssa/enemy_boss.h',ROOT/'include/ssa/enemy_control.h',Path(__file__)]
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
(ROOT/'analysis/enemy-boss-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
