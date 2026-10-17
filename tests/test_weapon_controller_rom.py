"""Whole116E4/12160/12200 comparisons; attack, aim and presentation boundaries explicit."""
from pathlib import Path
import ctypes as C
import hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/weapon_controller.c'),str(ROOT/'src/weapon_animation.c'),'-o',str(B/'libssa_weapon_controller.so')],check=True)
lib=C.CDLL(str(B/'libssa_weapon_controller.so'))
u8,u16,u32,i16,i32=C.c_uint8,C.c_uint16,C.c_uint32,C.c_int16,C.c_int32
class Inventory(C.Structure):_fields_=[('weapons',u16),('ammo',i16*10),('pending_count',u16),('pending_weapons',u32*10),('health',i16),('stat_c4',i16),('stat_c6',i16)]
class Control(C.Structure):_fields_=[('current',u32),('flags',u32),('requested',u16),('keys',u16)]
class Body(C.Structure):_fields_=[('x',i32),('y',i32),('z',i32),('vx',i32),('vy',i32),('vz',i32),('height',i32),('flags',u16),('floor',i16),('ceiling',i16),('hover',C.c_int8),('step',C.c_int8)]
class Entity(C.Structure):pass
EP=C.POINTER(Entity)
Entity._fields_=[('body',Body),('radius',i32),('type',u16),('player_index',u8),('leaf',C.c_int),('sector',C.c_int),('cell',C.c_size_t),('owner',C.c_void_p),('bn',EP),('bp',EP),('sn',EP),('sp',EP)]
class Object(C.Structure):_fields_=[('link',u32),('type',u16),('subtype',u16),('slot',u16),('index',u16),('x',i32),('y',i32),('sx',i16),('sy',i16),('previous',u16),('frame',u16),('flags',u32),('animation',u16),('frame_index',u16),('period',u8),('elapsed',u8),('done',u8),('unknown27',u8),('scalex',u16),('scaley',u16),('unknown2c',u16),('layer',u8),('owner',u8)]
assert C.sizeof(Object)==48
class Aim(C.Structure):_fields_=[('target',EP),('slope',i32),('marker',i32),('fraction',i32)]
class Effect(C.Structure):_fields_=[('x',i32),('y',i32),('z',i32),('vx',i32),('vy',i32),('vz',i32),('travel',i32),('radius',i32),('target',EP),('type',u16),('flags',u16),('state',u8),('owner',u8),('weapon',u8)]
class State(C.Structure):_fields_=[('inventory',C.POINTER(Inventory)),('control',C.POINTER(Control)),('object',C.POINTER(Object)),('source',EP),('source_health',i16),('held',u16),('pressed',u16),('heading',u16),('cooldown',u16*10),('aim_slope',i32),('aim',Aim),('updating',C.c_uint)]
class Game(C.Structure):_fields_=[('frame',u32),('shots',u32*10),('flags',u16),('local_flags',u16),('ticks',u8),('players',u8),('motion_index',u8),('request',u32)]
class Event(C.Structure):_fields_=[('kind',C.c_int),('args',i32*8),('target',EP),('effect',C.POINTER(Effect)),('result',i32)]
AC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(State),u16,i32,C.POINTER(Aim))
EC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(State),C.POINTER(Event))
class Ops(C.Structure):_fields_=[('aim',AC),('event',EC),('context',C.c_void_p)]
class Rules(C.Structure):_fields_=[('continuous',u8*10),('period',u8*10),('range',u16*10)]
rules=Rules();lib.ssa_weapon_rules_load.argtypes=[C.POINTER(Rules),C.c_void_p,C.c_size_t]
assert lib.ssa_weapon_rules_load(C.byref(rules),ROM,len(ROM))==1
step=lib.ssa_weapon_controller_step;step.argtypes=[C.POINTER(State),C.POINTER(Game),C.POINTER(Rules),C.POINTER(Ops)]
switch=lib.ssa_weapon_controller_switch;switch.argtypes=[C.POINTER(State),u8]
cool=lib.ssa_weapon_controller_cooldowns;cool.argtypes=[C.POINTER(State),u8]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x20000);uc.mem_map(0x03000000,0x10000)
P,G,E,O,T,F,L,STOP=0x02000000,0x02001000,0x02002000,0x02003000,0x02004000,0x02005000,0x02006000,0x087ff000
uc.mem_write(0x03004010,struct.pack('<I',G));uc.mem_write(0x03004648,struct.pack('<I',L));uc.mem_write(0x0300464e,b'\0\0')
rng=random.Random(0x116e4);counts={};guards=failures=reentries=0;coverage={}
def rd(data,off,fmt='I'):return struct.unpack_from('<'+fmt,data,off)[0]
def wr(data,off,n,fmt='I'):struct.pack_into('<'+fmt,data,off,n&({'I':0xffffffff,'H':65535,'B':255}[fmt]))
def si(n):return C.c_int32(n).value
def mem32(a):return rd(uc.mem_read(a,4),0)
def getentity(data):
    e=Entity()
    for name,off in [('x',0),('y',4),('z',8),('vx',12),('vy',16),('vz',20),('height',44)]:setattr(e.body,name,rd(data,off,'i'))
    e.body.flags=rd(data,48,'H');e.type=rd(data,60,'H');e.player_index=data[84]
    return e
def geteffect(data):
    p=Effect()
    for name,off in [('x',0),('y',4),('z',8),('vx',12),('vy',16),('vz',20),('travel',36),('radius',40)]:setattr(p,name,rd(data,off,'i'))
    p.type=rd(data,60,'H');p.flags=rd(data,48,'H');p.state=data[62];p.owner=data[84];p.weapon=data[85]
    return p
def puteffect(data,e,created):
    if not created:return
    if created==1:
        wr(data,36,e.travel);wr(data,84,e.owner,'B');wr(data,85,e.weapon,'B')
    else:
        for name,off in [('x',0),('y',4),('z',8),('vx',12),('vy',16),('vz',20),('radius',40)]:wr(data,off,getattr(e,name))
        wr(data,32,T);wr(data,60,e.type,'H');wr(data,48,e.flags,'H');wr(data,62,e.state,'B');wr(data,85,e.weapon,'B')

def compare(mode='fire',weapon=None,forced=None):
    global reentries
    p,g,e,o,t,f=[bytearray(rng.randbytes(n)) for n in (256,512,116,48,116,116)]
    w=weapon or rng.randrange(1,11);flags=rng.choice([0,0x400,0x1400,0x2100,rng.getrandbits(32)])
    wr(p,40,w);wr(p,44,O);wr(p,48,flags);wr(p,120,rng.randrange(1,11),'H');wr(p,172,E)
    wr(p,164,rng.randrange(4),'H');wr(p,166,rng.randrange(4),'H');wr(p,188,rng.choice([0,4,16]),'H')
    wr(e,84,rng.randrange(3),'B');wr(e,50,rng.choice([0,1,100,65535]),'H')
    wr(g,104,rng.choice([e[84],0,1,2]),'B');wr(g,7,rng.choice([0,1,2,4,127,255]),'B');wr(g,16,rng.choice([1,2,4]),'B')
    wr(g,4,rng.choice([0,0x400,rng.getrandbits(16)]),'H');wr(o,16,rng.choice([0x5000,0xdc00,0xdbff,0x5001,0x4fff,rng.getrandbits(32)]))
    wr(o,32,rng.choice([w-1,w+9,10,20]),'H');wr(o,38,rng.choice([0,1,2,255]),'B')
    wr(t,60,rng.choice([0xcd,0xd6,0xd7,0x66,0xe1]),'H')
    for i in range(10):
        wr(p,54+i*2,rng.choice([0,1,2,3,10,65535,32768]),'H')
        wr(p,74+i*2,rng.choice([0,0,0,1,255,65535,32768]),'H')
    if forced:
        for area,off,val,fmt in forced:wr({'p':p,'g':g,'e':e,'o':o,'t':t}[area],off,val,fmt)
    inp=bytes(p);gin=bytes(g)
    inv=Inventory();control=Control(rd(p,40),rd(p,48),rd(p,120,'H'),rd(p,188,'H'))
    for i in range(10):inv.ammo[i]=rd(p,54+2*i,'h')
    obj=Object.from_buffer_copy(o);source=getentity(e);target=getentity(t);effect=geteffect(f)
    st=State(C.pointer(inv),C.pointer(control),C.pointer(obj),C.pointer(source),rd(e,50,'h'),rd(p,164,'H'),rd(p,166,'H'),rd(p,184,'H'))
    st.aim_slope=rd(p,228,'i')
    for i in range(10):st.cooldown[i]=rd(p,74+2*i,'H')
    game=Game();game.frame=rd(g,0);game.flags=rd(g,4,'H');game.ticks=g[7];game.players=g[16];game.motion_index=g[104];game.local_flags=rng.choice([0,0,8,0xffff]);game.request=rng.getrandbits(32)
    for i in range(10):game.shots[i]=rd(g,0x98+16*i)
    wrg=game.request
    hits=rng.randrange(4);aimdata=[(j==hits,rng.getrandbits(32),rng.choice([0x69696969,1,0xffffffff]),rng.choice([0x7fffffff,0,1,65536,0xffff0000,rng.getrandbits(32)])) for j in range(3)]
    logs=[[],[]];ai=[0,0];created=[0];allocated=rng.randrange(2);audio=rng.randrange(2)
    def nativeaim(ctx,ps,angle,distance,result):
        global reentries
        assert step(ps,C.byref(game),C.byref(rules),C.byref(ops))==-1;reentries+=1
        logs[0].append(('aim',angle,distance))
        yes,slope,marker,fraction=aimdata[ai[0]];ai[0]+=1
        result.contents.target=C.pointer(target) if yes else EP()
        result.contents.slope=si(slope);result.contents.marker=si(marker);result.contents.fraction=si(fraction)
        return 1
    def nativeevent(ctx,ps,pe):
        global reentries
        assert switch(ps,1)==-1 and cool(ps,1)==-1;reentries+=2
        ev=pe.contents;k=ev.kind;a=list(ev.args)
        coverage[str(k)]=coverage.get(str(k),0)+1
        if k==0:logs[0].append(('play',a[0]));ev.result=audio
        elif k==1:logs[0].append(('stop',a[0]))
        elif k==2:logs[0].append(('particle',*a[:4]))
        elif k==3:logs[0].append(('hit',*a[:7]))
        elif k==4:logs[0].append(('projectile',*a[:6]));ev.effect=C.pointer(effect);created[0]=1
        elif k==6:logs[0].append(('allocate',));ev.effect=C.pointer(effect) if allocated else C.POINTER(Effect)();created[0]=2 if allocated else 0
        elif k==7:logs[0].append(('effect_animation',*a[:2]))
        elif k==8:logs[0].append(('insert',))
        elif k==9:logs[0].append(('explosion',*a[:3]))
        return 1
    ac,ec=AC(nativeaim),EC(nativeevent);ops=Ops(ac,ec,None)
    for addr,data in [(P,p),(G,g),(E,e),(O,o),(T,t),(F,f)]:uc.mem_write(addr,bytes(data))
    uc.mem_write(L+188,struct.pack('<H',game.local_flags));uc.mem_write(0x030000e0,struct.pack('<I',game.request))
    if mode=='fire':assert step(C.byref(st),C.byref(game),C.byref(rules),C.byref(ops))==1;address=0x080116e5
    elif mode=='switch':assert switch(C.byref(st),game.ticks)==1;address=0x08012161
    else:assert cool(C.byref(st),game.ticks)==1;address=0x08012201
    def hook(uc,address,size,unused):
        nonlocal created
        regs=[uc.reg_read(reg) for reg in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]]
        stack=uc.reg_read(UC_ARM_REG_SP);r0,r1,r2,r3=regs;result=0
        if address==0x0800a4f4:
            logs[1].append(('aim',mem32(stack),si(mem32(stack+4))))
            yes,slope,marker,fraction=aimdata[ai[1]];ai[1]+=1;result=slope
            uc.mem_write(0x0300491c,struct.pack('<I',T if yes else 0));uc.mem_write(0x03005074,struct.pack('<I',marker));uc.mem_write(0x0300505c,struct.pack('<I',fraction))
        elif address==0x08008d5c:logs[1].append(('play',r0));result=audio
        elif address==0x08008dc0:logs[1].append(('stop',r0))
        elif address==0x08014b60:logs[1].append(('particle',*[si(x) for x in regs]))
        elif address==0x0800a444:
            assert r0==E
            logs[1].append(('hit',si(r1),si(r2),si(r3),mem32(stack),si(mem32(stack+4)),si(mem32(stack+8)),si(mem32(stack+12))))
        elif address==0x080109f0:
            assert r1==E
            logs[1].append(('projectile',r0,si(r2),si(r3),si(mem32(stack)),si(mem32(stack+4)),si(mem32(stack+8))));result=F
        elif address==0x08005170:logs[1].append(('allocate',));result=F if allocated else 0
        elif address==0x08005394:logs[1].append(('effect_animation',r1,r2))
        elif address==0x0800a5d0:logs[1].append(('insert',))
        elif address==0x080035a8:logs[1].append(('explosion',r1,r2,r3))
        elif address==0x0800036c:
            product=si(r1)*si(r2);result=(product>>16)&0xffffffff
        else:return
        uc.reg_write(UC_ARM_REG_R0,result&0xffffffff);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
    h=uc.hook_add(UC_HOOK_CODE,hook)
    for reg,val in [(UC_ARM_REG_R0,P),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)]:uc.reg_write(reg,val)
    uc.emu_start(address,STOP,count=10000);uc.hook_del(h)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    wr(p,40,control.current);wr(p,48,control.flags);wr(p,228,st.aim_slope)
    for i in range(10):wr(p,54+2*i,inv.ammo[i],'H');wr(p,74+2*i,st.cooldown[i],'H');wr(g,0x98+16*i,game.shots[i])
    wr(g,4,game.flags,'H');wr(t,48,target.body.flags,'H');puteffect(f,effect,created[0])
    for addr,data,name in [(P,p,'player'),(G,g,'game'),(E,e,'source'),(O,bytes(obj),'object'),(T,t,'target'),(F,f,'effect')]:
        actual=bytes(uc.mem_read(addr,len(data)))
        assert actual==bytes(data),(mode,w,name,[(i,a,b) for i,(a,b) in enumerate(zip(actual,data)) if a!=b],inp.hex(),gin.hex(),logs)
    assert logs[0]==logs[1],(mode,w,logs)
    assert mem32(0x030000e0)==game.request,(w,'request',wrg)
    if mode=='fire':
        assert mem32(0x03005074)==st.aim.marker&0xffffffff
        assert mem32(0x0300505c)==st.aim.fraction&0xffffffff
    counts[mode]=counts.get(mode,0)+1

# Every weapon: start, dry click, firing, finish, repeated fire and denied input.
for w in range(1,11):
    for phase in [0,0x400,0x1400]:
        for ammo in [0,1,2,3,32768,65535]:
            for local in [0,1]:
                for held in [0,1]:
                    for done in [0,1]:
                        compare('fire',w,[('p',48,phase,'I'),('p',54+2*(w-1),ammo,'H'),('p',74+2*(w-1),0,'H'),('p',164,held,'H'),('p',166,1,'H'),('o',16,0x5000,'I'),('o',38,done,'B'),('e',84,0,'B'),('g',104,local,'B'),('g',4,0,'H')])
for i in range(4000):compare()
for i in range(3000):compare('switch')
for i in range(3000):compare('cooldown')
for y in [0,0x4fff,0x5000,0x5001,0xdbfe,0xdbff,0xdc00,0x7fffffff,0x80000000,0xffffffff]:
    for ticks in [0,1,2,255]:
        for flags in [0,0x100,0x2100,0xffffffff]:
            compare('switch',forced=[('o',16,y,'I'),('g',7,ticks,'B'),('p',48,flags,'I'),('p',188,0,'H')])
for ticks in [0,1,127,255]:
    for value in [0,1,126,127,128,254,255,256,32767,32768,32769,65534,65535]:
        compare('cooldown',forced=[('g',7,ticks,'B')]+[('p',74+2*i,value,'H') for i in range(10)])
assert step(None,None,None,None)==-1;guards+=1
assert switch(None,0)==-1;guards+=1
assert cool(None,0)==-1;guards+=1
assert lib.ssa_weapon_rules_load(None,ROM,len(ROM))==-1;guards+=1
assert lib.ssa_weapon_rules_load(C.byref(rules),None,len(ROM))==-1;guards+=1
assert lib.ssa_weapon_rules_load(C.byref(rules),ROM,0x44bd03)==-1;guards+=1
inv=Inventory();inv.ammo[1]=10;ctl=Control(2,0,2,4);obj=Object();obj.y=0x5000;ent=Entity()
st=State(C.pointer(inv),C.pointer(ctl),C.pointer(obj),C.pointer(ent),100,1,1,0)
game=Game();game.ticks=1;game.players=1
def aimok(ctx,ps,a,r,out):out.contents.target=C.pointer(ent);out.contents.slope=0;return 1
def eventok(ctx,ps,ev):return 1
ac,ec=AC(aimok),EC(eventok);ops=Ops(ac,ec,None)
for field,kind in [('inventory',Inventory),('control',Control),('object',Object),('source',Entity)]:
    original=C.cast(getattr(st,field),C.c_void_p).value;setattr(st,field,C.POINTER(kind)())
    assert step(C.byref(st),C.byref(game),C.byref(rules),C.byref(ops))==-1;guards+=1
    assert switch(C.byref(st),1)==-1;guards+=1
    assert cool(C.byref(st),1)==-1;guards+=1
    setattr(st,field,C.cast(original,C.POINTER(kind)))
for current in [0,11,0xffffffff]:
    ctl.current=current
    assert step(C.byref(st),C.byref(game),C.byref(rules),C.byref(ops))==-1;guards+=1
    assert switch(C.byref(st),1)==-1;guards+=1
    assert cool(C.byref(st),1)==-1;guards+=1
ctl.current=2
for requested in [0,11,65535]:ctl.requested=requested;assert switch(C.byref(st),1)==-1;guards+=1
ctl.requested=2
for missing in [1,2,3]:
    assert step(C.byref(st),None if missing==1 else C.byref(game),None if missing==2 else C.byref(rules),None if missing==3 else C.byref(ops))==-1;guards+=1
for fail_at in [1,2,3]:
    counter=[0]
    def failaim(ctx,ps,a,r,out):
        counter[0]+=1;out.contents.target=EP();return -1 if counter[0]==fail_at else 1
    bad=Ops(AC(failaim),ec,None);ctl.flags=0
    assert step(C.byref(st),C.byref(game),C.byref(rules),C.byref(bad))==-1
    assert counter[0]==fail_at and st.updating==0 and ctl.flags==0;failures+=1
# Starting animation/flags survive a later particle failure; ammo has not fired.
def failevent(ctx,ps,ev):return -1
bad=Ops(ac,EC(failevent),None);ctl.flags=0;obj.y=0x5000
assert step(C.byref(st),C.byref(game),C.byref(rules),C.byref(bad))==-1
assert st.updating==0 and ctl.flags==0x400 and obj.animation==11 and inv.ammo[1]==10;failures+=1
# Partial shotgun volley: first request succeeds; second fails before ammo/stat.
counter=[0]
def failsecond(ctx,ps,ev):counter[0]+=1;return -1 if counter[0]==2 else 1
bad=Ops(ac,EC(failsecond),None);ctl.current=3;ctl.flags=0x400;inv.ammo[2]=9
assert step(C.byref(st),C.byref(game),C.byref(rules),C.byref(bad))==-1
assert counter[0]==2 and inv.ammo[2]==9 and game.shots[2]==0 and st.updating==0;failures+=1
# Projectile creation cannot silently succeed without a returned allocation.
ctl.current=6;ctl.flags=0x400
assert step(C.byref(st),C.byref(game),C.byref(rules),C.byref(ops))==-1
assert ctl.flags==0x400 and game.shots[5]==0 and st.updating==0;failures+=1
assert set(coverage)==set(map(str,range(11)))
report={'status':'passed','rom_sha1':hashlib.sha1(ROM).hexdigest(),'comparisons':counts,'total_rom_comparisons':sum(counts.values()),'native_guards':guards,'failure_paths':failures,'event_coverage':coverage,'reentry_checks':reentries,'source_sha256':hashlib.sha256((ROOT/'src/weapon_controller.c').read_bytes()).hexdigest(),'scope':'Whole116E4 with controlled aim, hitscan, allocation, explosion, audio and particle boundaries; realFC04/116BC. Whole12160 and12200. Full player/game/source/target/object/projectile records and ordered calls compared.'}
(ROOT/'analysis/native-weapon-controller-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
