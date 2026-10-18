"""Complete player initialization vs 08005E4C, with real map/grid code."""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
# Reuse only ctypes declarations, without executing the spawn suite or compiler.
tree=ast.parse((ROOT/'tests/test_spawn_rom.py').read_text())
nodes=[]
for n in tree.body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id=='EP' for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'spawn ctypes','exec'))
class Movement(C.Structure):
 _fields_=[(n,C.c_uint32) for n in ('keys','weapon_flags')]+[(n,C.c_int32) for n in ('turn_velocity','forward_velocity','strafe_velocity','impulse','velocity_x','velocity_y')]+[(n,C.c_uint16) for n in ('heading','move_heading','flags','modifier_ticks','impulse_heading')]
class Vertical(C.Structure):
 _fields_=[('eye_offset',C.c_int32),('eye_velocity',C.c_int32),('flags',C.c_uint16),('lowered',C.c_uint16)]
class Inventory(C.Structure):
 _fields_=[('weapons',C.c_uint16),('ammo',C.c_int16*10),('pending_count',C.c_uint16),('pending_weapons',C.c_uint32*10),('health',C.c_int16),('stat_c4',C.c_int16),('stat_c6',C.c_int16)]
class Motion(C.Structure):
 _fields_=[('movement',Movement)]+[(n,C.c_int32) for n in ('x','y','z','velocity_z')]+[('facing',C.c_uint8)]
class Player(C.Structure):
 _fields_=[('entity',Entity),('motion',Motion),('vertical',Vertical),('inventory',Inventory)]+[('entity_health',C.c_int16),('animation',C.c_uint16)]+[(n,C.c_uint8) for n in ('animation_ticks','active')]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in ('player_spawn.c','spawn.c','world.c','geometry.c','map_assets.c','horizontal.c','slide.c','movement.c','physics.c')],'-o',str(BUILD/'libssa_player_spawn.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_player_spawn.so'))
for name,args in {
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[C.POINTER(Map)],
 'ssa_spawn_definitions_load':[C.POINTER(Definitions),C.c_void_p,C.c_size_t],
 'ssa_spawn_plan_load':[C.POINTER(Plan),C.POINTER(Map),C.POINTER(Definitions),C.c_uint,C.c_uint,C.c_size_t],
 'ssa_spawn_plan_insert':[C.POINTER(Plan),C.POINTER(World)],'ssa_spawn_plan_free':[C.POINTER(Plan)],
 'ssa_world_init':[C.POINTER(World),C.POINTER(Map),C.c_size_t],'ssa_world_free':[C.POINTER(World)],
 'ssa_players_spawn':[C.POINTER(Player),C.POINTER(World),C.POINTER(Plan),C.c_uint,C.c_uint]
}.items(): getattr(lib,name).argtypes=args
defs=Definitions(); assert lib.ssa_spawn_definitions_load(C.byref(defs),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000); uc.mem_map(0x04000000,0x1000)
BASE,GRID,POOL,GAME,SETTINGS,PLAYERS,STOP=0x02000000,0x02050000,0x02060000,0x02070000,0x02071000,0x02072000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
def ret(): uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
def hook(machine,address,size,user):
 if address==0x080033d8:
  assert uc.reg_read(UC_ARM_REG_R0)==1024
  uc.mem_write(PLAYERS,bytes([0xa5])*1024); uc.reg_write(UC_ARM_REG_R0,PLAYERS); ret()
 elif address==0x080c5320:
  source,destination,control=[uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)]
  assert control==0x05000100
  uc.mem_write(destination,bytes(uc.mem_read(source,4))*256); ret()
 elif address==0x0800d1b4: ret()
 elif address==0x0800f9b4:
  uc.reg_write(UC_ARM_REG_R0,0x02073000);ret()
for addr in (0x080033d8,0x080c5320,0x0800d1b4,0x0800f9b4): uc.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
def dma(machine,access,address,size,value,user):
 if address==0x040000dc:
  assert value==0x85000100
  source=get(0x040000d4,'<I'); destination=get(0x040000d8,'<I')
  uc.mem_write(destination,bytes(uc.mem_read(source,4))*256)
uc.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000df)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_initializer_comparisons':0,'player_records':0,'body_records':0,'grid_snapshots':0,'native_guards':0,'scope':'Entire 08005E4C including original scanner, allocator, player/body synchronization, animation setup and BSP/grid insertion. Heap allocation, BIOS/DMA zero-fill and D1B4 sector-pool allocation are explicit test platform boundaries. No weapon setup, respawn or player frame.'}
report['construction_restore_comparisons']=0
report['construction_restore_scope']='512 additional actual05E4C+1240C+123C4 compositions on all16maps; only sprite allocation/platform memory services controlled. New body health100, player protection/queue zero; saved weapons/ammo restored. Not the complete outer campaign dispatcher.'
entry_rng=random.Random(0x123c4)
rng=random.Random(0x5e4c)

def compare(m,level,count,local,random_markers=False,missing_start=False):
 plan=Plan(); w=World(); players=(Player*4)()
 assert lib.ssa_spawn_plan_load(C.byref(plan),C.byref(m),C.byref(defs),level,count,0)
 markers=None; savedptr=C.cast(C.cast(plan.player_starts,C.c_void_p).value,C.POINTER(Marker)); savedcount=plan.player_count
 if count>1 and (plan.player_count<count or random_markers):
  markers=(Marker*4)()
  for marker in markers:
   marker.x=rng.randrange(-32768,32768); marker.y=rng.randrange(-32768,32768)
   marker.z=rng.randrange(-32768,32768); marker.heading=rng.randrange(65536)
  plan.player_starts=markers; plan.player_count=4
 assert lib.ssa_world_init(C.byref(w),C.byref(m),count)
 assert lib.ssa_players_spawn(players,C.byref(w),C.byref(plan),count,local)
 raw=bytearray(C.string_at(m.data,m.size))
 for off in range(0x1c,0x48,4): struct.pack_into('<I',raw,off,BASE+struct.unpack_from('<I',raw,off)[0])
 struct.pack_into('<H',raw,6,m.thing_count)
 uc.mem_write(BASE,bytes(raw)); put(0x03004930,'<I',BASE)
 put(0x03004944,'<iihh',w.origin_x,w.origin_y,w.width,w.height); put(0x03004640,'<I',GRID)
 put(0x03004954,'<I',0x02040000)
 uc.mem_write(GRID,bytes(w.width*w.height*4)); uc.mem_write(POOL,bytes(100*0x74))
 put(0x03004644,'<I',POOL); put(0x0300433c,'<I',0x03004540); put(0x03004348,'<H',count)
 put(0x03004454,'<I',0x03004470); put(0x03004354,'<H',0)
 put(0x03004540,'<'+'H'*100,*range(100)); put(0x03004010,'<I',GAME); put(GAME+0x10,'<B',count); put(GAME+0x68,'<B',local)
 if count>1: uc.mem_write(0x03003f90,C.string_at(plan.player_starts,count*8))
 uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(0x08005e4d,STOP,count=1000000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert get(0x0300464c,'<HH')==local and get(0x0300464e,'<H')==local
 assert get(0x03004650,'<B')==count
 pointers={C.addressof(p.entity):i for i,p in enumerate(players)}
 def index(q): return pointers[C.addressof(q.contents)] if q else -1
 def romindex(q): return (q-POOL)//0x74 if q else -1
 assert get(0x03004354,'<H')==w.entity_count==sum(p.active for p in players)
 for i,p in enumerate(players):
  expected=bytearray(256)
  if p.active:
   e=p.entity
   struct.pack_into('<iii',expected,0,p.motion.x,p.motion.y,p.motion.z); struct.pack_into('<I',expected,0xac,POOL+0x74*i)
   struct.pack_into('<H',expected,0xb8,p.motion.movement.heading); struct.pack_into('<H',expected,0xbc,p.vertical.flags)
   struct.pack_into('<i',expected,0xd0,p.vertical.eye_offset)
   body=bytearray(0x74); struct.pack_into('<iii',body,0,e.body.x,e.body.y,e.body.z)
   struct.pack_into('<iiHh',body,0x28,e.radius,e.body.height,e.body.flags,p.entity_health)
   struct.pack_into('<HHH',body,0x38,i,i,e.type); body[0x3f]=p.motion.facing; body[0x54]=e.player_index
   struct.pack_into('<H',body,0x58,p.animation); body[0x5e]=p.animation_ticks
   struct.pack_into('<II',body,0x60,BASE+m.sectors+20*e.sector,BASE+m.leaves+8*e.leaf)
   struct.pack_into('<h',body,0x6a,e.sector); struct.pack_into('<hhh',body,0x6e,e.leaf,e.body.ceiling,e.body.floor)
   for off,ptr in ((0x18,e.block_next),(0x1c,e.block_previous)):
    ix=index(ptr); struct.pack_into('<I',body,off,0 if ix<0 else POOL+ix*0x74)
   actual=bytes(uc.mem_read(POOL+i*0x74,0x74))
   assert body==actual,(level,count,local,i,'body',[(k,a,b) for k,(a,b) in enumerate(zip(body,actual)) if a!=b])
   assert not any(bytes(p.inventory)) and p.motion.movement.flags==4
   report['body_records']+=1
  actual=bytes(uc.mem_read(PLAYERS+i*256,256))
  assert expected==actual,(level,count,local,i,'player',[(k,a,b) for k,(a,b) in enumerate(zip(expected,actual)) if a!=b])
  report['player_records']+=1
 assert [index(w.cells[i]) for i in range(w.width*w.height)]==[romindex(x[0]) for x in struct.iter_unpack('<I',uc.mem_read(GRID,w.width*w.height*4))]
 report['grid_snapshots']+=1
 if count==1 and players[0].active:
  # Compose the actual original constructor,1240C and123C4. Only sprite
  # allocation is controlled; no vitality write or restore instruction is hooked.
  put(0x0300572c,'<I',SETTINGS);uc.mem_write(SETTINGS,bytes(64));uc.mem_write(0x02073000,bytes(48))
  uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
  uc.emu_start(0x0801240d,STOP,count=100000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
  original_body=bytes(uc.mem_read(POOL,0x74))
  for case in range(32):
   saved=bytearray(entry_rng.randbytes(0x60));current=1+case%10
   struct.pack_into('<I',saved,0,current);struct.pack_into('<H',saved,12,0x3ff)
   ammo=[entry_rng.randrange(1000) for _ in range(10)];struct.pack_into('<10h',saved,14,*ammo)
   uc.mem_write(0x03005680,bytes(saved));uc.reg_write(UC_ARM_REG_R0,PLAYERS+0x28)
   uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
   uc.emu_start(0x080123c5,STOP,count=100000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
   assert bytes(uc.mem_read(POOL,0x74))==original_body and get(POOL+0x32,'<h')==100
   assert get(PLAYERS+0xc4,'<H')==0 and get(PLAYERS+0xc6,'<H')==0
   assert get(PLAYERS+0x7a,'<H')==0 and not any(uc.mem_read(PLAYERS+0x7c,40))
   assert get(PLAYERS+0x28,'<I')==current and get(PLAYERS+0x78,'<H')==current
   assert get(PLAYERS+0x34,'<H')==0x3ff and list(struct.unpack('<10h',uc.mem_read(PLAYERS+0x36,20)))==ammo
   report['construction_restore_comparisons']+=1
 if w.entity_count:
  assert not lib.ssa_players_spawn(players,C.byref(w),C.byref(plan),count,local); report['native_guards']+=1
 lib.ssa_world_free(C.byref(w))
 if markers is not None: plan.player_starts=savedptr; plan.player_count=savedcount
 assert lib.ssa_spawn_plan_free(C.byref(plan))
 report['whole_initializer_comparisons']+=1

for level in range(16):
 m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
 for count in range(1,5):
  for local in range(count): compare(m,level,count,local)
 for case in range(32):
  count=rng.randrange(2,5); compare(m,level,count,rng.randrange(count),True)
 # Missing single-player start: original clears records but allocates no body.
 saved=m.thing_count; m.thing_count=0; compare(m,level,1,0,missing_start=True); m.thing_count=saved
 lib.ssa_map_assets_free(C.byref(m)); print('PASS player initialization map',level,flush=True)

m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),0)
p=Plan(); assert lib.ssa_spawn_plan_load(C.byref(p),C.byref(m),C.byref(defs),0,1,0)
w=World(); assert lib.ssa_world_init(C.byref(w),C.byref(m),1)
players=(Player*4)(); before=bytes(players)
for count,local in ((0,0),(5,0),(1,1),(4,0)):
 assert not lib.ssa_players_spawn(players,C.byref(w),C.byref(p),count,local)
 assert bytes(players)==before and w.entity_count==0; report['native_guards']+=1
w.entity_count=1
assert not lib.ssa_players_spawn(players,C.byref(w),C.byref(p),1,0); report['native_guards']+=1
w.entity_count=0; w.query_active=1
assert not lib.ssa_players_spawn(players,C.byref(w),C.byref(p),1,0); report['native_guards']+=1
w.query_active=0
# Geometry failure rolls back earlier players and all intrusive list links.
markers=(Marker*4)(); savedptr=C.cast(C.cast(p.player_starts,C.c_void_p).value,C.POINTER(Marker)); savedcount=p.player_count; p.player_starts=markers; p.player_count=4
lib.ssa_world_free(C.byref(w)); assert lib.ssa_world_init(C.byref(w),C.byref(m),4)
saved=m.sectors; m.sectors=m.size
assert not lib.ssa_players_spawn(players,C.byref(w),C.byref(p),4,0) and w.entity_count==0
assert not any(q.entity.owner for q in players); report['native_guards']+=1
m.sectors=saved; p.player_starts=savedptr; p.player_count=savedcount
lib.ssa_world_free(C.byref(w)); lib.ssa_spawn_plan_free(C.byref(p)); lib.ssa_map_assets_free(C.byref(m))
report['source_sha256']=hashlib.sha256((ROOT/'src/player_spawn.c').read_bytes()).hexdigest()
report['test_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
(ROOT/'analysis/native-player-spawn-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
