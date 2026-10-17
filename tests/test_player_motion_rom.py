"""Whole 080064B8 with controlled horizontal callback and real vertical/math."""
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
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes(); SHA1=hashlib.sha1(ROM).hexdigest()
assert SHA1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
# Share ctypes declarations only: do not execute another suite or its build.
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world ctypes','exec'))
HORIZONTAL=C.CFUNCTYPE(C.c_int,C.c_void_p,EP)
class MotionOps(C.Structure): _fields_=[('horizontal',HORIZONTAL),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in ('player_motion.c','movement.c','physics.c','world.c','geometry.c','horizontal.c','slide.c')],'-o',str(BUILD/'libssa_player_motion.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_player_motion.so'))
lib.ssa_player_motion_step.argtypes=[C.POINTER(Motion),EP,C.POINTER(Player),C.POINTER(Tables),C.POINTER(MotionFrame),C.POINTER(MotionOps),C.POINTER(MotionOptions)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x40000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]); uc.mem_write(0x030003a4,ROM[0x430:0x638])
GAME,PLAYERS,ENTITIES,STOP=0x02001000,0x02002000,0x02003000,0x087ff000
def ea(i): return ENTITIES+0x80*i
def pa(i): return PLAYERS+256*i
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
def signed(v): return C.c_int32(v).value
put(0x03004010,'<I',GAME); put(0x03004648,'<I',PLAYERS)
def bios(machine,address,size,user):
 a=signed(uc.reg_read(UC_ARM_REG_R0)); b=signed(uc.reg_read(UC_ARM_REG_R1)); assert b
 q=(abs(a)//abs(b))*(-1 if (a<0)!=(b<0) else 1)
 uc.reg_write(UC_ARM_REG_R0,q&0xffffffff); uc.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff); uc.reg_write(UC_ARM_REG_R3,abs(q)&0xffffffff)
 uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,bios,begin=0x08002c88,end=0x08002c88)
rng=random.Random(0x64b8)
fields={'keys':(0xa4,'I'),'weapon_flags':(0x30,'I'),'turn_velocity':(0x24,'i'),'forward_velocity':(0xb0,'i'),'strafe_velocity':(0xb4,'i'),'impulse':(0x20,'i'),'velocity_x':(12,'i'),'velocity_y':(16,'i'),'heading':(0xb8,'H'),'move_heading':(0xba,'H'),'flags':(0xbc,'H'),'modifier_ticks':(0xc2,'H'),'impulse_heading':(0xdc,'H')}
bodyfields={'x':(0,'i'),'y':(4,'i'),'z':(8,'i'),'velocity_x':(12,'i'),'velocity_y':(16,'i'),'velocity_z':(20,'i'),'height':(44,'i'),'flags':(0x30,'H'),'hover_offset':(0x43,'b'),'step_adjust':(0x53,'b'),'ceiling':(0x70,'h'),'floor':(0x72,'h')}
def packbody(raw,e,facing=None):
 for n,(off,fmt) in bodyfields.items(): struct.pack_into('<'+fmt,raw,off,getattr(e.body,n))
 struct.pack_into('<i',raw,0x28,e.radius); struct.pack_into('<H',raw,0x3c,e.type); raw[0x54]=e.player_index
 if facing is not None: raw[0x3f]=facing
 return raw
def packplayer(raw,m,v):
 for n,(off,fmt) in fields.items(): struct.pack_into('<'+fmt,raw,off,getattr(m.movement,n))
 struct.pack_into('<iii',raw,0,m.x,m.y,m.z); struct.pack_into('<i',raw,20,m.velocity_z)
 struct.pack_into('<iiH',raw,0xd0,v.eye_offset,v.eye_velocity,v.lowered)
 return raw
rom_events=[]; native_events=[]; callback_errors=[]; changes={}; current_slot=0
def rom_horizontal(machine,address,size,user):
 address=uc.reg_read(UC_ARM_REG_R0); assert address==ea(current_slot)
 rom_events.append((bytes(uc.mem_read(address,0x74)),bytes(uc.mem_read(pa(current_slot),256))))
 for n,value in changes.items():
  off,fmt=bodyfields[n]; put(address+off,'<'+fmt,value)
 uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,rom_horizontal,begin=0x08004bdc,end=0x08004bdc)
report={'rom_sha1':SHA1,'boundary_cases':0,'random_cases':0,'sequence_frames':0,'native_guard_checks':0,'scope':'Whole 080064B8 including multiplayer push, actual original math, player/body synchronization and 4CE0 vertical physics. Horizontal 4BDC is a controlled state-mutating callback; BIOS division is provided by the test. Full player/body bytes, peer preservation and horizontal callback entry state/order compared. Native jump/retained-heading options excluded.'}

def check(m,v,es,count,slot,local,ticks,mutate,category):
 global current_slot,changes
 current_slot=slot; changes=mutate
 e=es[slot]; peers=(EP*count)(*[C.pointer(es[i]) for i in range(count)])
 frame=MotionFrame(peers,ticks,count,local,slot)
 rawplayers=[bytearray(rng.randbytes(256)) for _ in range(count)]
 rawentities=[bytearray(rng.randbytes(0x74)) for _ in range(count)]
 for i in range(count):
  es[i].player_index=i
  struct.pack_into('<I',rawplayers[i],0xac,ea(i))
  packbody(rawentities[i],es[i]); uc.mem_write(ea(i),bytes(rawentities[i]))
  if i==slot: packplayer(rawplayers[i],m,v)
  uc.mem_write(pa(i),bytes(rawplayers[i]))
 put(GAME+7,'<B',ticks); put(GAME+0x10,'<B',count); put(GAME+0x68,'<B',local)
 original_raw=bytes(rawentities[slot]); original_player=bytes(rawplayers[slot])
 @HORIZONTAL
 def native_horizontal(context,entity):
  try:
   assert C.addressof(entity.contents)==C.addressof(e)
   native_events.append((bytes(packbody(bytearray(original_raw),entity.contents,m.facing)),bytes(packplayer(bytearray(original_player),m,v))))
   for n,value in changes.items(): setattr(entity.contents.body,n,value)
   return 0
  except BaseException as error: callback_errors.append(error); return -1
 ops=MotionOps(native_horizontal,None)
 rom_events.clear(); native_events.clear(); callback_errors.clear()
 uc.reg_write(UC_ARM_REG_R0,pa(slot)); uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(0x080064b9,STOP,count=100000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 result=lib.ssa_player_motion_step(C.byref(m),C.byref(e),C.byref(v),C.byref(tables),C.byref(frame),C.byref(ops),None)
 assert not callback_errors,callback_errors
 assert result==0
 assert len(rom_events)==len(native_events)==1
 assert native_events==rom_events,(category,report[category],'horizontal entry',[(k,a,b) for k,(a,b) in enumerate(zip(native_events[0][1],rom_events[0][1])) if a!=b])
 packbody(rawentities[slot],e,m.facing); packplayer(rawplayers[slot],m,v)
 for i in range(count):
  a=bytes(rawplayers[i]); b=bytes(uc.mem_read(pa(i),256))
  assert a==b,(category,report[category],i,'player',[(k,x,y) for k,(x,y) in enumerate(zip(a,b)) if x!=y])
  a=bytes(rawentities[i]); b=bytes(uc.mem_read(ea(i),0x74))
  assert a==b,(category,report[category],i,'entity',[(k,x,y) for k,(x,y) in enumerate(zip(a,b)) if x!=y])
 assert v.flags==m.movement.flags
 report[category]+=1

def state(count,slot):
 es=(Entity*count)()
 for i,e in enumerate(es):
  e.body=Body(x=signed(rng.getrandbits(32)),y=signed(rng.getrandbits(32)),z=rng.randrange(-200,200)*65536,
    height=rng.choice((0,24,100,152))*65536,flags=rng.choice((0x10,0x18,0x110,0xffff)),floor=rng.randrange(-200,100),ceiling=rng.randrange(100,400),
    hover_offset=rng.randrange(-128,128),step_adjust=rng.randrange(-128,128))
  e.type=0xc9+i; e.radius=32*65536; e.player_index=i
 m=Motion(Movement(keys=rng.getrandbits(32),weapon_flags=rng.getrandbits(32),turn_velocity=signed(rng.getrandbits(32)),
  forward_velocity=signed(rng.getrandbits(32)),strafe_velocity=signed(rng.getrandbits(32)),impulse=signed(rng.getrandbits(32)),
  heading=rng.randrange(65536),move_heading=rng.randrange(65536),flags=rng.randrange(65536),modifier_ticks=rng.randrange(65536),impulse_heading=rng.randrange(65536)),
  es[slot].body.x,es[slot].body.y,es[slot].body.z,signed(rng.getrandbits(32)),rng.randrange(256))
 v=Player(signed(rng.getrandbits(32)),signed(rng.getrandbits(32)),m.movement.flags,rng.randrange(65536))
 return m,v,es

# Threshold/axis/wrapping vectors, including all quadrants and co-location.
vectors=[(0,0),(79,0),(80,0),(81,0),(0,79),(0,80),(0,81),(55,55),(56,56),(57,57),(79,1),(80,1),
         (-32768,0),(0,-32768),(-32768,-32768),(-32768,1),(1,-32768),(32767,32767)]
vectors=list(dict.fromkeys((sx*x,sy*y) for x,y in vectors for sx,sy in itertools.product((-1,1),repeat=2) if -32768<=sx*x<=32767 and -32768<=sy*y<=32767))
for dx,dy in vectors:
 for slot,local,ticks in itertools.product(range(2),range(2),(0,1,255)):
  m,v,es=state(2,slot); es[slot].body.x=0x7fffffff; es[slot].body.y=-0x80000000
  es[1-slot].body.x=signed(es[slot].body.x-dx*65536); es[1-slot].body.y=signed(es[slot].body.y-dy*65536)
  check(m,v,es,2,slot,local,ticks,{},'boundary_cases')
# Ordered nearby peers: the last eligible peer replaces, rather than adds to,
# an earlier impulse. Exercise every current slot and alternating peer order.
for slot,order,ticks in itertools.product(range(4),itertools.permutations(((1,0),(0,79),(80,0))),(0,1,2,255)):
 m,v,es=state(4,slot); es[slot].body.x=es[slot].body.y=0
 for i,(dx,dy) in zip((i for i in range(4) if i!=slot),order): es[i].body.x=dx*65536; es[i].body.y=dy*65536
 check(m,v,es,4,slot,slot,ticks,{},'boundary_cases')
print('PASS multiplayer thresholds, axis wrapping and peer ordering',flush=True)

for case in range(6000):
 count=rng.randrange(1,5); slot=rng.randrange(count); m,v,es=state(count,slot)
 if case%2:
  for i in range(count):
   if i!=slot:
    es[i].body.x=signed(es[slot].body.x+rng.randrange(-90*65536,90*65536))
    es[i].body.y=signed(es[slot].body.y+rng.randrange(-90*65536,90*65536))
 changeset={}
 if case%3:
  changeset={n:signed(rng.getrandbits(32)) for n in ('x','y','velocity_x','velocity_y','velocity_z')}
  changeset.update(floor=rng.randrange(-32768,32768),ceiling=rng.randrange(-32768,32768),step_adjust=rng.randrange(-128,128))
 check(m,v,es,count,slot,rng.randrange(count),rng.choice((0,1,2,3,8,255)),changeset,'random_cases')
print('PASS 6,000 randomized whole movement calls',flush=True)

for sequence in range(8):
 count=4; slot=sequence%4; m,v,es=state(count,slot)
 for i in range(256):
  m.movement.keys=rng.randrange(1024)|(rng.randrange(1024)<<16)
  e=es[slot]
  for j in range(count):
   if j!=slot:
    es[j].body.x=signed(e.body.x+rng.randrange(-90,91)*65536)
    es[j].body.y=signed(e.body.y+rng.randrange(-90,91)*65536)
  check(m,v,es,count,slot,slot,1,{'x':signed(e.body.x+e.body.velocity_x),'y':signed(e.body.y+e.body.velocity_y)},'sequence_frames')
print('PASS 2,048 consecutive movement frames',flush=True)

# Native integration errors do not execute the callback or mutate state before
# validation. Callback failures propagate after controls/body synchronization.
m,v,es=state(2,0); peers=(EP*2)(C.pointer(es[0]),C.pointer(es[1])); frame=MotionFrame(peers,1,2,0,0)
calls=[]
@HORIZONTAL
def success(context,entity): calls.append(1); return 0
ops=MotionOps(success,None)
args=[C.byref(m),C.byref(es[0]),C.byref(v),C.byref(tables),C.byref(frame),C.byref(ops),None]
for i in range(6):
 bad=args.copy(); bad[i]=None; before=(bytes(m),bytes(v),bytes(es))
 assert lib.ssa_player_motion_step(*bad)==-1 and before==(bytes(m),bytes(v),bytes(es)) and not calls
 report['native_guard_checks']+=1
empty=MotionOps(); bad=args.copy(); bad[5]=C.byref(empty)
assert lib.ssa_player_motion_step(*bad)==-1 and not calls; report['native_guard_checks']+=1
for badframe in (MotionFrame(peers,1,5,0,0),MotionFrame(None,1,2,0,0),MotionFrame(peers,1,2,0,2),MotionFrame(peers,1,2,0,1)):
 bad=args.copy(); bad[4]=C.byref(badframe); before=(bytes(m),bytes(v),bytes(es))
 assert lib.ssa_player_motion_step(*bad)==-1 and before==(bytes(m),bytes(v),bytes(es)) and not calls
 report['native_guard_checks']+=1
peers[1]=EP(); before=(bytes(m),bytes(v),bytes(es))
assert lib.ssa_player_motion_step(*args)==-1 and before==(bytes(m),bytes(v),bytes(es)) and not calls; report['native_guard_checks']+=1
peers[1]=C.pointer(es[1])
for errorcode in (-1,1):
 @HORIZONTAL
 def fail(context,entity): calls.append(1); entity.contents.body.z=12345; return errorcode
 ops.horizontal=fail; beforez=m.z; beforevertical=bytes(v)
 assert lib.ssa_player_motion_step(*args)==-1 and es[0].body.z==12345 and m.z==beforez and bytes(v)==beforevertical
 report['native_guard_checks']+=1
report['total_rom_comparisons']=sum(report[k] for k in ('boundary_cases','random_cases','sequence_frames'))
report['source_sha256']={n:hashlib.sha256((ROOT/'src'/n).read_bytes()).hexdigest() for n in ('player_motion.c','movement.c','physics.c')}
(ROOT/'analysis/native-player-motion-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
