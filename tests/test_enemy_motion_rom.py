"""Whole enemy motion helpers; only walk collision and BIOS division controlled."""
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
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'ctypes world','exec'))
class Motion(C.Structure):
 _fields_=[('entity',EP),('speed',C.c_int32),('facing',C.c_uint8),('desired_facing',C.c_uint8),('updating',C.c_uint)]
class Game(C.Structure):
 _fields_=[('frame_counter',C.c_uint32),('hazard_sectors',C.POINTER(C.c_int32)),('hazard_count',C.c_size_t),('level_counter',C.c_uint16),('local_flags',C.c_uint16)]
MOVE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Motion),C.c_int32,C.c_int32)
class Ops(C.Structure): _fields_=[('move',MOVE),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['enemy_motion.c','player_hazards.c','movement.c','world.c','physics.c','geometry.c','slide.c','horizontal.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(BUILD/'libssa_enemy_motion.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_enemy_motion.so'))
for name,args in {
 'ssa_enemy_face':[C.POINTER(Motion),EP,C.POINTER(Tables)],
 'ssa_enemy_turn':[C.POINTER(Motion),C.c_uint,C.POINTER(C.c_int32)],
 'ssa_enemy_distance':[EP,EP,C.POINTER(Tables),C.POINTER(C.c_int32)],
 'ssa_enemy_walk':[C.POINTER(Motion),C.POINTER(Tables),C.c_uint,C.POINTER(C.c_uint32),C.POINTER(Ops)],
 'ssa_enemy_hazards':[EP,C.POINTER(C.c_int16),C.POINTER(Map),C.POINTER(Game)],
 'ssa_enemy_walk_world':[C.POINTER(Motion),C.POINTER(Tables),C.c_uint,C.POINTER(C.c_uint32),WP],
 'ssa_world_init':[WP,C.POINTER(Map),C.c_size_t], 'ssa_world_free':[WP],
 'ssa_world_insert':[WP,EP], 'ssa_world_remove':[WP,EP],
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
 'ssa_map_assets_free':[C.POINTER(Map)],
 'ssa_math_tables_load':[C.POINTER(Tables),C.c_void_p,C.c_size_t],
}.items(): getattr(lib,name).argtypes=args
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
BASE,E,T,G,L,STOP=0x02000000,0x02070000,0x02070100,0x02072000,0x02074000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
put(0x03004010,'<I',G); put(0x0300572c,'<I',L)
def run(addr,a=E,b=T):
 for reg,value in ((UC_ARM_REG_R0,a),(UC_ARM_REG_R1,b),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)): uc.reg_write(reg,value&0xffffffff)
 uc.emu_start(addr|1,STOP,count=10000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
 return C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
def bios(machine,address,size,ctx):
 a,b=[C.c_int32(machine.reg_read(r)).value for r in (UC_ARM_REG_R0,UC_ARM_REG_R1)]
 assert b
 q=abs(a)//abs(b)*(-1 if (a<0)!=(b<0) else 1)
 machine.reg_write(UC_ARM_REG_R0,q&0xffffffff); machine.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff)
 machine.reg_write(UC_ARM_REG_R3,abs(q)); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,bios,begin=0x08002c88,end=0x08002c88)
rng=random.Random(0x4498)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'turn':0,'face':0,'distance':0,'walk':0,'hazards':0,'guards':0,'reentry':0,'callback_failures':0,'native_world_frames':0,'native_world_moved':0,'native_world_blocked':0,'scope':'whole 04614/0456C/045C4/04498/0354C; real trig/fixed multiply/vertical and hazard BSP; 043D0 collision controlled in walk oracle; BIOS division service replaced; native world walk separately on all16maps; no enemy AI claim'}
e,target=Entity(),Entity(); motion=Motion(C.pointer(e),0,0,0,0); delta=C.c_int32()
for facing in range(256):
 for desired in range(256):
  ticks=(0,1,2,8,127,255)[(facing+desired)%6]
  motion.facing=facing; motion.desired_facing=desired
  raw=bytearray(rng.randbytes(0x74)); raw[0x3f]=facing; raw[0x40]=desired
  uc.mem_write(E,bytes(raw)); put(G+7,'<B',ticks)
  expected=run(0x08004614,b=0x10000)
  assert lib.ssa_enemy_turn(C.byref(motion),ticks,C.byref(delta))==0
  raw[0x3f]=motion.facing
  assert expected==delta.value and bytes(raw)==bytes(uc.mem_read(E,0x74)),(facing,desired,ticks)
  report['turn']+=1
print('PASS turn: all 65,536 facing pairs',flush=True)
axes=[-2147483648,-2147483647,-65536,-1,0,1,65536,2147483646,2147483647]
vectors=[(x,y,0,0) for x in axes for y in axes]+[tuple(rng.randrange(-2147483648,2147483648) for _ in range(4)) for _ in range(8192)]
for x,y,tx,ty in vectors:
 e.body.x=x; e.body.y=y; target.body.x=tx; target.body.y=ty
 raw=bytearray(rng.randbytes(0x74)); struct.pack_into('<ii',raw,0,x,y)
 uc.mem_write(E,bytes(raw)); put(T,'<ii',tx,ty)
 run(0x0800456c)
 assert lib.ssa_enemy_face(C.byref(motion),C.byref(target),C.byref(tables))==0
 raw[0x40]=motion.desired_facing
 assert bytes(raw)==bytes(uc.mem_read(E,0x74))
 report['face']+=1
 expected=run(0x080045c4)
 assert lib.ssa_enemy_distance(C.byref(e),C.byref(target),C.byref(tables),C.byref(delta))==0
 assert delta.value==expected,(x,y,tx,ty,delta.value,expected)
 report['distance']+=1
print('PASS face/distance',flush=True)
fields=[(n,o,f) for n,o,f in [('x',0,'i'),('y',4,'i'),('z',8,'i'),('velocity_x',12,'i'),('velocity_y',16,'i'),('velocity_z',20,'i'),('height',44,'i'),('flags',48,'H'),('hover_offset',67,'b'),('step_adjust',83,'b'),('ceiling',112,'h'),('floor',114,'h')]]
def move_hook(machine,address,size,ctx):
 x,y=[C.c_int32(machine.reg_read(r)).value for r in (UC_ARM_REG_R1,UC_ARM_REG_R2)]
 oracle.append((x,y,bytes(machine.mem_read(E,0x74))))
 if moved: put(E,'<ii',x,y)
 machine.reg_write(UC_ARM_REG_R0,moved); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,move_hook,begin=0x080043d0,end=0x080043d0)
@MOVE
def native_move(ctx,m,x,y):
 actual.append((x,y,project()))
 assert lib.ssa_enemy_walk(m,C.byref(tables),ticks,C.byref(counter),C.byref(ops))==-1
 report['reentry']+=1
 if failing: return -1
 if moved: e.body.x=x; e.body.y=y
 return moved
ops=Ops(native_move,None); failing=False
def project():
 result=bytearray(raw)
 for n,o,f in fields: struct.pack_into('<'+f,result,o,getattr(e.body,n))
 return bytes(result)
for i in range(4096):
 raw=bytearray(rng.randbytes(0x74)); raw[48]&=~16
 for n,o,f in fields: setattr(e.body,n,struct.unpack_from('<'+f,raw,o)[0])
 motion.speed=struct.unpack_from('<i',raw,0x24)[0]; motion.facing=raw[0x3f]
 ticks=(0,1,2,8,255)[i%5]; moved=i%2; counter=C.c_uint32(rng.randrange(2**32))
 put(G+7,'<B',ticks); put(0x03004358,'<I',counter.value); uc.mem_write(E,bytes(raw))
 oracle=[]; actual=[]; expected=run(0x08004498)
 assert lib.ssa_enemy_walk(C.byref(motion),C.byref(tables),ticks,C.byref(counter),C.byref(ops))==expected
 assert oracle==actual and project()==bytes(uc.mem_read(E,0x74)) and counter.value==get(0x03004358,'<I')
 report['walk']+=1
failing=True; before=project(); actual=[]
assert lib.ssa_enemy_walk(C.byref(motion),C.byref(tables),ticks,C.byref(counter),C.byref(ops))==-1
assert before==project() and not motion.updating
report['callback_failures']+=1
print('PASS directional walking and real vertical physics',flush=True)
for level in range(16):
 mapdata=Map(); assert lib.ssa_map_assets_load(C.byref(mapdata),ROM,len(ROM),level)
 data=bytearray(C.string_at(mapdata.data,mapdata.size))
 world=World(); assert lib.ssa_world_init(C.byref(world),C.byref(mapdata),4)
 marker=None
 for thing in range(mapdata.thing_count):
  record=struct.unpack_from('<hhHHHH',data,mapdata.things+thing*12)
  if record[3]==1:
   marker=record; break
 assert marker
 actor=Entity(); actor.body.x=marker[0]*65536; actor.body.y=marker[1]*65536
 actor.body.height=64*65536; actor.radius=16*65536; actor.body.flags=2; actor.type=0xcd
 assert lib.ssa_world_insert(C.byref(world),C.byref(actor))
 actor.body.z=actor.body.floor*65536
 walking=Motion(C.pointer(actor),4*65536,0,0,0); steps=C.c_uint32()
 for frame in range(256):
  walking.facing=(frame//16)*16
  old=(actor.body.x,actor.body.y)
  result=lib.ssa_enemy_walk_world(C.byref(walking),C.byref(tables),1,C.byref(steps),C.byref(world))
  assert result in (0,1) and world.entity_count==1 and not world.query_active and steps.value==frame+1
  assert actor.owner and C.addressof(actor.owner.contents)==C.addressof(world)
  if not result: assert old==(actor.body.x,actor.body.y)
  report['native_world_frames']+=1
  report['native_world_moved' if result else 'native_world_blocked']+=1
 world.query_active=1
 assert lib.ssa_enemy_walk_world(C.byref(walking),C.byref(tables),1,C.byref(steps),C.byref(world))==-1
 world.query_active=0; report['guards']+=1
 assert lib.ssa_world_remove(C.byref(world),C.byref(actor)); lib.ssa_world_free(C.byref(world))
 for i in range(128):
  sector=i%mapdata.sector_count; off=mapdata.sectors+sector*20
  floor=struct.unpack_from('<h',data,off)[0]
  data[off+10]=(255,35,1,0)[i%4]
  C.memmove(mapdata.data,bytes(data),mapdata.size)
  relocated=bytearray(data)
  for ptr in range(0x1c,0x48,4): struct.pack_into('<I',relocated,ptr,BASE+struct.unpack_from('<I',relocated,ptr)[0])
  uc.mem_write(BASE,bytes(relocated)); put(0x03004930,'<I',BASE)
  ox,oy,width,height=struct.unpack_from('<hhhh',data,mapdata.blockmap)
  put(0x03004944,'<iihh',ox*65536,oy*65536,width,height)
  put(0x03004954,'<I',0x02040000)
  # Authentic map vertices and radius sample resolve real BSP sectors.
  vi=i%mapdata.vertex_count; x,y=struct.unpack_from('<hh',data,mapdata.vertices+vi*4)
  e.body.x=x*65536; e.body.y=y*65536; e.body.z=(floor+(i%3==0))*65536
  e.radius=(i%5)*65536; e.sector=sector
  hazard=(C.c_int32*4)(sector,(sector+1)%mapdata.sector_count,0,mapdata.sector_count-1)
  game=Game(i,hazard,0 if i%3==0 else 4,i%8,0)
  health=C.c_int16(rng.randrange(-32768,32768))
  raw=bytearray(rng.randbytes(0x74)); struct.pack_into('<iii',raw,0,e.body.x,e.body.y,e.body.z)
  struct.pack_into('<i',raw,0x28,e.radius); struct.pack_into('<h',raw,0x6a,sector); struct.pack_into('<h',raw,0x32,health.value)
  uc.mem_write(E,bytes(raw)); put(L+16,'<H',game.level_counter)
  put(G,'<I',i)
  # Globals for original 03464; same locations as recovered hazard tests.
  put(0x030057a8,'<B',game.hazard_count)
  for j in range(4): put(0x03005780+j*4,'<i',hazard[j])
  run(0x0800354c)
  assert lib.ssa_enemy_hazards(C.byref(e),C.byref(health),C.byref(mapdata),C.byref(game))==0
  struct.pack_into('<h',raw,0x32,health.value)
  assert bytes(raw)==bytes(uc.mem_read(E,0x74)),(level,i,health.value,get(E+0x32,'<h'))
  report['hazards']+=1
 lib.ssa_map_assets_free(C.byref(mapdata))
print('PASS hazards on all 16 maps',flush=True)
for test in [lambda:lib.ssa_enemy_turn(None,1,C.byref(delta)),lambda:lib.ssa_enemy_turn(C.byref(motion),256,C.byref(delta)),lambda:lib.ssa_enemy_turn(C.byref(motion),1,None),lambda:lib.ssa_enemy_face(None,C.byref(target),C.byref(tables)),lambda:lib.ssa_enemy_face(C.byref(motion),None,C.byref(tables)),lambda:lib.ssa_enemy_face(C.byref(motion),C.byref(target),None),lambda:lib.ssa_enemy_distance(None,C.byref(target),C.byref(tables),C.byref(delta)),lambda:lib.ssa_enemy_distance(C.byref(e),None,C.byref(tables),C.byref(delta)),lambda:lib.ssa_enemy_distance(C.byref(e),C.byref(target),None,C.byref(delta)),lambda:lib.ssa_enemy_distance(C.byref(e),C.byref(target),C.byref(tables),None),lambda:lib.ssa_enemy_walk(None,C.byref(tables),1,C.byref(counter),C.byref(ops)),lambda:lib.ssa_enemy_walk(C.byref(motion),None,1,C.byref(counter),C.byref(ops)),lambda:lib.ssa_enemy_walk(C.byref(motion),C.byref(tables),256,C.byref(counter),C.byref(ops)),lambda:lib.ssa_enemy_walk(C.byref(motion),C.byref(tables),1,None,C.byref(ops)),lambda:lib.ssa_enemy_walk(C.byref(motion),C.byref(tables),1,C.byref(counter),None)]:
 assert test()==-1; report['guards']+=1
report['rom_comparisons']=sum(report[k] for k in ('turn','face','distance','walk','hazards'))
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/native-enemy-motion-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
