"""Whole 037F4 with ordered network, sound and reward boundaries controlled."""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import itertools
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_spawn_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef): nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or
        isinstance(t,ast.Name) and t.id=='EP' for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'entity declarations','exec'))
class Inventory(C.Structure):
    _fields_=[('weapons',C.c_uint16),('ammo',C.c_int16*10),('pending_count',C.c_uint16),
              ('pending_weapons',C.c_uint32*10),('health',C.c_int16),('stat_c4',C.c_int16),('stat_c6',C.c_int16)]
class Target(C.Structure):
    _fields_=[('entity',EP),('health',C.POINTER(C.c_int16)),('timer',C.POINTER(C.c_uint16)),('inventory',C.POINTER(Inventory))]
class Game(C.Structure):
    _fields_=[('source',EP),('source_owner',EP),('shot_hit_count',C.POINTER(C.c_uint32)),
        ('enemy_hits',C.c_uint32*12),('enemy_kills',C.c_uint32*12)]+[(n,C.c_uint32) for n in
        ('frame_counter','hurt_flash','credit_player')]+[('credited_kills',C.POINTER(C.c_uint8)),
        ('local_player',C.c_uint16),('local_flags',C.c_uint16)]+[(n,C.c_uint8) for n in
        ('player_count','winner','difficulty','god_mode','kill_limit')]+[('updating',C.c_uint)]
EVENT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Target),C.c_int,C.c_uint32,C.c_uint32)
class Ops(C.Structure): _fields_=[('event',EVENT),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
 str(ROOT/'src/combat_damage.c'),'-o',str(BUILD/'libssa_combat_damage.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_combat_damage.so')); run=lib.ssa_combat_damage
run.argtypes=[C.POINTER(Game),C.POINTER(Target),C.c_uint32,C.POINTER(Ops)]; run.restype=C.c_int
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000); uc.mem_map(0x03000000,0x10000)
E,S,O,G,P,SET,STOP=0x02000000,0x02000100,0x02000200,0x02001000,0x02002000,0x02003000,0x087ff000
def put(a,fmt,*values): uc.mem_write(a,struct.pack('<'+fmt,*values))
def get(a,fmt): return struct.unpack('<'+fmt,uc.mem_read(a,struct.calcsize('<'+fmt)))[0]
def raw_snapshot(): return tuple(bytes(uc.mem_read(a,n)) for a,n in ((E,0x74),(G,0x200),(P,0x400),(SET,0x60),(0x030000e0,4)))
events=[]
def hook(machine,address,size,user):
    kind={0x08005850:0,0x08008d5c:1,0x080125e0:2}[address]
    a=machine.reg_read(UC_ARM_REG_R0) if kind!=2 else 0
    b=machine.reg_read(UC_ARM_REG_R1) if kind==0 else 0
    if kind==2: assert machine.reg_read(UC_ARM_REG_R0)==E
    events.append((kind,a,b,raw_snapshot()))
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for address in (0x08005850,0x08008d5c,0x080125e0): uc.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(0x37f4)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':0,'callback_reentry_guards':0,
 'native_guards':0,'callback_failure_cases':0,'events':[0,0,0],
 'scope':'Complete original037F4; original unsigned division executes; only5850 network,8D5C sound,125E0 reward controlled; full target/game/player/settings bytes and ordered effect snapshots'}

def compare(**kw):
    raw=bytearray(rng.randbytes(0x74)); gr=bytearray(rng.randbytes(0x200)); pr=bytearray(rng.randbytes(0x400)); settings=bytearray(rng.randbytes(0x60))
    e,s,o=Entity(),Entity(),Entity(); g=Game(); inv=Inventory()
    damage=kw.get('damage',rng.getrandbits(32)); null=kw.get('null',False)
    flags=kw.get('flags',rng.choice([0,0x10,0x800,0x810,1,0xffff])); e.body.flags=flags
    e.type=kw.get('type',rng.choice([0xcc,0xcd,0xcf,0xd6,0xd7,0xd8,0xd9,0xe1,0xffff]))
    e.player_index=kw.get('target_index',rng.randrange(4)); health=C.c_int16(kw.get('health',rng.randrange(-32768,32768)))
    timer=C.c_uint16(rng.randrange(65536)); inv.stat_c4=kw.get('c4',rng.randrange(-32768,32768)); inv.stat_c6=kw.get('c6',rng.randrange(-32768,32768))
    target=Target(C.pointer(e),C.pointer(health),C.pointer(timer),C.pointer(inv))
    g.player_count=kw.get('players',rng.choice([0,1,2,4])); g.winner=kw.get('winner',0)
    g.difficulty=kw.get('difficulty',rng.choice([0,1,2,3,255])); g.god_mode=kw.get('god',rng.choice([0,0,0,1,255]))
    g.credit_player=kw.get('credit',rng.randrange(5)); g.kill_limit=kw.get('limit',rng.randrange(256))
    g.local_player=kw.get('local',rng.randrange(4)); g.local_flags=kw.get('local_flags',rng.randrange(65536))
    g.frame_counter=rng.getrandbits(32); g.hurt_flash=kw.get('flash',rng.choice([0,0,9,0xffffffff]))
    credited=C.c_uint8(kw.get('kills',rng.randrange(256))); g.credited_kills=C.pointer(credited)
    shotindex=kw.get('shot',rng.randrange(11)); shot=C.c_uint32(kw.get('counter',rng.getrandbits(32)))
    if shotindex:g.shot_hit_count=C.pointer(shot)
    source_mode=kw.get('source',rng.choice(['direct','owner','none','neither']))
    # The original crashes for NULL source on the <=1-player player path.
    if source_mode=='none' and g.player_count<=1:source_mode='direct'
    s.type=kw.get('source_type',rng.choice([0xcc,0xcd,0xd6,0xd7,0xd8,0xd9,0xe1])); o.type=kw.get('owner_type',rng.choice([0xcc,0xcd,0xd6,0xd7,0xd8,0xd9]))
    s.player_index=kw.get('source_index',rng.randrange(4)); o.player_index=kw.get('owner_index',rng.randrange(4))
    s.body.flags=0x10 if source_mode=='direct' else 0; o.body.flags=0x10 if source_mode=='owner' else 0
    if source_mode!='none':g.source=C.pointer(s)
    if source_mode in ('owner','neither'):g.source_owner=C.pointer(o)
    for i in range(12):g.enemy_hits[i]=kw.get('counter',rng.getrandbits(32));g.enemy_kills[i]=kw.get('counter',rng.getrandbits(32))
    def project():
        a,b,c,d=bytearray(raw),bytearray(gr),bytearray(pr),bytearray(settings)
        for off,fmt,val in ((0x30,'H',e.body.flags),(0x32,'h',health.value),(0x3c,'H',e.type),(0x4e,'H',timer.value),(0x54,'B',e.player_index)):struct.pack_into('<'+fmt,a,off,val)
        for off,fmt,val in ((0,'I',g.frame_counter),(0x10,'B',g.player_count),(0x7d,'B',g.winner)):struct.pack_into('<'+fmt,b,off,val)
        if g.credit_player:struct.pack_into('<B',b,0x83+g.credit_player,credited.value)
        if shotindex:struct.pack_into('<I',b,0x9c+(shotindex-1)*16,shot.value)
        for i in range(12):struct.pack_into('<II',b,0x134+i*12,g.enemy_hits[i],g.enemy_kills[i])
        struct.pack_into('<H',c,g.local_player*256+0xbc,g.local_flags)
        struct.pack_into('<hh',c,e.player_index*256+0xc4,inv.stat_c4,inv.stat_c6)
        for off,val in ((0x1a,g.difficulty),(0x26,g.god_mode),(0x3c,g.kill_limit)):d[off]=val
        return bytes(a),bytes(b),bytes(c),bytes(d),struct.pack('<I',g.hurt_flash)
    initial=project()
    for (address,_),data in zip(((E,0x74),(G,0x200),(P,0x400),(SET,0x60),(0x030000e0,4)),initial):uc.mem_write(address,data)
    for address,obj,owner in ((S,s,O if g.source_owner else 0),(O,o,0)):
        sr=bytearray(rng.randbytes(0x74));struct.pack_into('<I',sr,0x20,owner);struct.pack_into('<H',sr,0x30,obj.body.flags);struct.pack_into('<H',sr,0x3c,obj.type);sr[0x54]=obj.player_index;uc.mem_write(address,bytes(sr))
    for address,val in ((0x03004010,G),(0x0300572c,SET),(0x03004648,P),(0x0300443c,S if g.source else 0),(0x03003f4c,shotindex),(0x03003f40,g.credit_player)):put(address,'I',val)
    put(0x0300464e,'H',g.local_player)
    events.clear()
    for reg,val in ((UC_ARM_REG_R0,0 if null else E),(UC_ARM_REG_R1,damage),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,val)
    uc.emu_start(0x080037f5,STOP,count=2500);assert uc.reg_read(UC_ARM_REG_PC)==STOP
    expected=raw_snapshot(); oracle_events=list(events); native_events=[]; reentries=[]
    @EVENT
    def callback(ctx,vp,kind,a,b):
        native_events.append((kind,a,b,project()))
        reentries.extend([run(C.byref(g),vp,damage,C.byref(ops)),run(C.byref(g),None,0,C.byref(ops))])
        return 0
    ops=Ops(callback,None)
    assert run(C.byref(g),None if null else C.byref(target),damage,C.byref(ops))==0
    actual=project()
    assert actual==expected,(kw,damage,[(j,[(i,x,y) for i,(x,y) in enumerate(zip(a,b)) if x!=y][:20]) for j,(a,b) in enumerate(zip(actual,expected)) if a!=b])
    assert native_events==oracle_events,(kw,[(x[:3],y[:3]) for x,y in zip(native_events,oracle_events)])
    assert not g.updating and all(x==-1 for x in reentries)
    report['rom_comparisons']+=1;report['callback_reentry_guards']+=len(reentries)
    for kind,*_ in native_events:report['events'][kind]+=1

# Boundary grid for unsigned scaling, low-halfword armor and health wrap.
for players,difficulty,flags,damage in itertools.product([0,1,2,4],[0,1,2,3,255],[0,1,0x10,0x11,0x800,0x810],
 [0,1,2,3,5,99,100,32767,32768,65535,65536,0x7fffffff,0x80000000,0xffffffff]):
    compare(players=players,difficulty=difficulty,flags=flags,damage=damage,god=0,source='none' if players>1 else 'direct',local=0,target_index=0,local_flags=0,flash=0)
for c4,c6,damage in itertools.product([-32768,-1,0,1,2,99,100,32767],[-32768,-1,0,1,2,99,100,32767],[0,1,2,3,32767,32768,65535,0xffffffff]):
    compare(players=1,flags=0x810,difficulty=3,god=0,c4=c4,c6=c6,damage=damage,local=0,target_index=0,local_flags=0,flash=0)
for source,source_index,owner_index,local in itertools.product(['direct','owner','none','neither'],range(4),range(4),range(4)):
    compare(players=4,flags=0x10,source=source,source_index=source_index,owner_index=owner_index,local=local)
for typ,owner,players in itertools.product(range(0xcc,0xda),range(0xcc,0xda),[0,1]):
    compare(players=players,flags=0x10,source='owner',source_type=typ,owner_type=owner)
for players,kills,limit,credit in itertools.product([1,2,4],[0,1,254,255],[0,1,2,255],[0,1,4]):
    compare(players=players,flags=0x810,source='none',damage=1000,health=1,c4=0,c6=0,god=0,difficulty=3,kills=kills,limit=limit,credit=credit)
for flags,health,damage in itertools.product([0,0x10,0x800,0x810],[-32768,-1,0,1,2,32767],[0,1,2,32767,32768,65535,65536,0xffffffff]):
    compare(players=1,flags=flags,health=health,damage=damage,c4=0,c6=0,god=0,difficulty=3,counter=0xffffffff,shot=1,credit=1,source_type=0xcd,type=0xcd)
for _ in range(10000):compare()
for players in [0,1,2,4,255]:
    compare(players=players,null=True)
    compare(players=players,winner=255,flags=0x810)

e,s=Entity(),Entity();health=C.c_int16(100);timer=C.c_uint16(7);inv=Inventory();g=Game();g.source=C.pointer(s);g.player_count=1;g.difficulty=3
target=Target(C.pointer(e),C.pointer(health),C.pointer(timer),C.pointer(inv));shot=C.c_uint32(0);g.shot_hit_count=C.pointer(shot)
@EVENT
def ok(ctx,vp,kind,a,b):return 0
@EVENT
def fail(ctx,vp,kind,a,b):return -1
ops=Ops(ok,None);badops=Ops();failops=Ops(fail,None)
for check in [lambda:run(None,C.byref(target),1,C.byref(ops)),lambda:run(C.byref(g),C.byref(target),1,None),lambda:run(C.byref(g),C.byref(target),1,C.byref(badops))]:
    assert check()==-1;report['native_guards']+=1
for field in ['entity','health','timer','inventory']:
    e.body.flags=0x10;bad=Target(target.entity,target.health,target.timer,target.inventory);setattr(bad,field,type(getattr(bad,field))())
    assert run(C.byref(g),C.byref(bad),1,C.byref(ops))==-1;report['native_guards']+=1
g.source=EP();assert run(C.byref(g),C.byref(target),1,C.byref(ops))==-1;report['native_guards']+=1;g.source=C.pointer(s)
g.player_count=2;g.credit_player=1;assert run(C.byref(g),C.byref(target),1,C.byref(ops))==-1;report['native_guards']+=1;g.credit_player=0
g.updating=1;assert run(C.byref(g),None,0,C.byref(ops))==-1;report['native_guards']+=1;g.updating=0
# Stop at each failing boundary, preserving precisely earlier effects.
s.body.flags=0x10;s.player_index=0;e.player_index=1;g.local_player=0
before=(e.body.flags,health.value,timer.value)
assert run(C.byref(g),C.byref(target),9,C.byref(failops))==-1
assert (e.body.flags,health.value,timer.value)==before and not g.updating;report['callback_failure_cases']+=1
g.player_count=1;e.player_index=0;g.hurt_flash=0;e.body.flags=0x810;inv.stat_c6=100;inv.stat_c4=0
assert run(C.byref(g),C.byref(target),10,C.byref(failops))==-1
assert (health.value,timer.value,inv.stat_c6,g.hurt_flash,e.body.flags,g.updating)==(100,30,80,9,0x810,0);report['callback_failure_cases']+=1
e.body.flags=0x800;e.type=0xcd;health.value=1;g.credit_player=1
assert run(C.byref(g),C.byref(target),10,C.byref(failops))==-1
assert (health.value,e.body.flags,g.enemy_kills[0],g.updating)==(0,0x800,1,0);report['callback_failure_cases']+=1
report['source_sha256']=hashlib.sha256((ROOT/'src/combat_damage.c').read_bytes()).hexdigest();report['status']='passed'
(ROOT/'analysis/native-combat-damage-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
