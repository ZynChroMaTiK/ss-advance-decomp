"""Complete 08001984 map construction vs native spawn plans and insertion."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
 *[str(ROOT/'src'/f) for f in ('spawn.c','world.c','geometry.c','map_assets.c','horizontal.c','slide.c','movement.c','physics.c')],
 '-o',str(BUILD/'libssa_spawn.so')],check=True)

class Body(C.Structure):
 _fields_=[(n,C.c_int32) for n in ('x','y','z','velocity_x','velocity_y','velocity_z','height')]+[
 ('flags',C.c_uint16),('floor',C.c_int16),('ceiling',C.c_int16),('hover_offset',C.c_int8),('step_adjust',C.c_int8)]
class Map(C.Structure):
 _fields_=[('data',C.POINTER(C.c_uint8)),('size',C.c_size_t)]+[(n+'_count',C.c_uint16) for n in
 ('thing','texture','sector','node','leaf','segment','side','line','vertex')]+[(n,C.c_uint32) for n in
 ('things','textures','sectors','nodes','leaves','segments','sides','lines','vertices','reject','blockmap','rom_offset')]
class Collision(C.Structure):
 _fields_=[('map',C.POINTER(Map)),('visited',C.c_void_p),('special_lines',C.c_void_p),('special_count',C.c_size_t),('line_capacity',C.c_size_t)]+[
 (n,C.c_int32) for n in ('floor','ceiling','low_floor','ceiling_line','blocking_line')]
class Entity(C.Structure): pass
class World(C.Structure): pass
EP=C.POINTER(Entity)
Entity._fields_=[('body',Body),('radius',C.c_int32),('type',C.c_uint16),('player_index',C.c_uint8),
 ('leaf',C.c_int),('sector',C.c_int),('cell',C.c_size_t),('owner',C.POINTER(World))]+[
 (n,EP) for n in ('block_next','block_previous','sector_next','sector_previous')]
World._fields_=[('collision',Collision)]+[(n,C.POINTER(EP)) for n in ('cells','sectors','special_contacts')]+[
 (n,C.c_size_t) for n in ('entity_count','capacity','special_count')]+[(n,C.c_int32) for n in ('origin_x','origin_y')]+[
 ('width',C.c_uint),('height',C.c_uint),('blocking_entity',EP),('observe_special',C.c_void_p),('contact_context',C.c_void_p),
 ('execute_special',C.c_void_p),('execute_context',C.c_void_p),('query_active',C.c_uint),('executing_special',EP)]
class Definition(C.Structure):
 _fields_=[(n,C.c_uint16) for n in ('type','variant','animation','animation_ticks')]
class Definitions(C.Structure): _fields_=[('thing',Definition*64)]
class Object(C.Structure):
 _fields_=[('entity',Entity)]+[(n,C.c_uint16) for n in ('thing_index','animation','trigger')]+[
 (n,C.c_uint8) for n in ('variant','facing','animation_ticks')]
class Marker(C.Structure):
 _fields_=[('x',C.c_int16),('y',C.c_int16),('z',C.c_int16),('heading',C.c_uint16)]
class Plan(C.Structure):
 _fields_=[('map',C.POINTER(Map)),('objects',C.POINTER(Object)),('player_starts',C.POINTER(Marker)),('teleport_starts',C.POINTER(Marker))]+[
 (n,C.c_size_t) for n in ('object_count','player_count','teleport_count','dropped_count')]
lib=C.CDLL(str(BUILD/'libssa_spawn.so'))
for name,args in {
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[C.POINTER(Map)],
 'ssa_spawn_definitions_load':[C.POINTER(Definitions),C.c_void_p,C.c_size_t],
 'ssa_spawn_plan_load':[C.POINTER(Plan),C.POINTER(Map),C.POINTER(Definitions),C.c_uint,C.c_uint,C.c_size_t],
 'ssa_spawn_plan_insert':[C.POINTER(Plan),C.POINTER(World)],'ssa_spawn_plan_free':[C.POINTER(Plan)],
 'ssa_world_init':[C.POINTER(World),C.POINTER(Map),C.c_size_t],'ssa_world_free':[C.POINTER(World)]
}.items(): getattr(lib,name).argtypes=args
defs=Definitions(); assert lib.ssa_spawn_definitions_load(C.byref(defs),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
BASE,GRID,POOL,GAME,SETTINGS,STOP=0x02000000,0x02050000,0x02060000,0x02070000,0x02071000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
def sector_hook(machine,address,size,user): machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,sector_hook,begin=0x0800d1b4,end=0x0800d1b4)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_spawn_runs':0,'entity_comparisons':0,'marker_comparisons':0,'grid_snapshots':0,'native_guards':0,
 'scope':'Entire 08001984 with original 05170 allocator, B14C scanner, 5394 animation setter and A5D0/A6CC BSP/grid insertion. Only D1B4 sector-pool allocation is hooked. Native host sector lists are separate. AI activation and player-state initialization excluded.'}
rng=random.Random(0x1984)

def compare(m,level,players,limit):
 p=Plan(); w=World()
 assert lib.ssa_spawn_plan_load(C.byref(p),C.byref(m),C.byref(defs),level,players,limit)
 assert lib.ssa_world_init(C.byref(w),C.byref(m),100)
 assert lib.ssa_spawn_plan_insert(C.byref(p),C.byref(w))
 raw=bytearray(C.string_at(m.data,m.size))
 for off in range(0x1c,0x48,4): struct.pack_into('<I',raw,off,BASE+struct.unpack_from('<I',raw,off)[0])
 struct.pack_into('<H',raw,6,m.thing_count)
 uc.mem_write(BASE,bytes(raw)); put(0x03004930,'<I',BASE)
 put(0x03004944,'<iihh',w.origin_x,w.origin_y,w.width,w.height); put(0x03004640,'<I',GRID)
 put(0x03004954,'<I',0x02040000)
 uc.mem_write(GRID,bytes(w.width*w.height*4)); uc.mem_write(POOL,bytes(100*0x74))
 put(0x03004644,'<I',POOL); put(0x0300433c,'<I',0x03004540); put(0x03004348,'<H',limit)
 put(0x03004454,'<I',0x03004470); put(0x03004354,'<H',0)
 put(0x03004540,'<'+'H'*100,*range(100)); put(0x03004010,'<I',GAME); put(GAME+0x10,'<B',players)
 put(0x0300572c,'<I',SETTINGS); put(SETTINGS+0x10,'<H',level)
 uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
 try: uc.emu_start(0x08001985,STOP,count=4000000)
 except Exception:
  print('oracle fault',level,players,limit,hex(uc.reg_read(UC_ARM_REG_PC)),flush=True)
  raise
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert p.object_count==get(0x03004354,'<H') and p.object_count==w.entity_count
 assert p.player_count==get(0x03003f80,'<I') and p.teleport_count==get(0x03004014,'<I')
 pointers={C.addressof(p.objects[i].entity):i for i in range(p.object_count)}
 def index(q): return pointers[C.addressof(q.contents)] if q else -1
 def romindex(q): return (q-POOL)//0x74 if q else -1
 for i in range(p.object_count):
  o=p.objects[i]; e=o.entity; oracle=uc.mem_read(POOL+0x74*i,0x74)
  expected=bytearray(0x74)
  struct.pack_into('<iii',expected,0,e.body.x,e.body.y,e.body.z)
  struct.pack_into('<HHHBB',expected,0x38,i,i,e.type,o.variant,o.facing)
  struct.pack_into('<H',expected,0x58,o.animation); expected[0x5e]=o.animation_ticks
  struct.pack_into('<H',expected,0x68,o.trigger)
  struct.pack_into('<h',expected,0x6a,e.sector); struct.pack_into('<hhh',expected,0x6e,e.leaf,e.body.ceiling,e.body.floor)
  struct.pack_into('<II',expected,0x60,BASE+m.sectors+20*e.sector,BASE+m.leaves+8*e.leaf)
  for off,ptr in ((0x18,e.block_next),(0x1c,e.block_previous)):
   ix=index(ptr); struct.pack_into('<I',expected,off,0 if ix<0 else POOL+ix*0x74)
  assert bytes(expected)==bytes(oracle),(level,players,limit,i,[(k,a,b) for k,(a,b) in enumerate(zip(expected,oracle)) if a!=b])
  assert not any((e.body.velocity_x,e.body.velocity_y,e.body.velocity_z,e.body.height,e.body.flags,e.radius))
  report['entity_comparisons']+=1
 for ptr,count,address in ((p.player_starts,p.player_count,0x03003f90),(p.teleport_starts,p.teleport_count,0x03004040)):
  assert C.string_at(ptr,count*8)==bytes(uc.mem_read(address,count*8))
  report['marker_comparisons']+=count
 assert [index(w.cells[i]) for i in range(w.width*w.height)]==[
  romindex(x[0]) for x in struct.iter_unpack('<I',uc.mem_read(GRID,w.width*w.height*4))]
 report['grid_snapshots']+=1
 if p.object_count:
  assert not lib.ssa_spawn_plan_free(C.byref(p)); assert not lib.ssa_spawn_plan_insert(C.byref(p),C.byref(w)); report['native_guards']+=2
 lib.ssa_world_free(C.byref(w)); assert lib.ssa_spawn_plan_free(C.byref(p))
 report['whole_spawn_runs']+=1

for level in range(16):
 m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
 for players in (1,4):
  for limit in (0,1,7,100): compare(m,level,players,limit)
 lib.ssa_map_assets_free(C.byref(m))
 print(f'PASS original map {level}',flush=True)

# Synthetic records use real map geometry and original allocation/scanning code.
# Limit marker counts to original fixed arrays (four player, eight teleport).
m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),10)
original_count=m.thing_count
for case in range(128):
 n=min(original_count,80); m.thing_count=n
 for i in range(n):
  typ=rng.choice(tuple(range(5,17))+tuple(range(18,68)))
  x,y=rng.randrange(-32768,32768),rng.randrange(-32768,32768)
  if i<2: x,y=(2240,3008)[i],-2304
  raw=struct.pack('<hh4H',x,y,rng.randrange(65536),typ,rng.randrange(65536),rng.randrange(65536))
  C.memmove(C.addressof(m.data.contents)+m.things+12*i,raw,12)
 compare(m,10 if case%2 else 9,1 if case%3 else 4,rng.choice((0,3,17,100)))
lib.ssa_map_assets_free(C.byref(m))

p=Plan(); m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),0)
for level,players,limit in ((16,1,100),(0,0,100),(0,5,100),(0,1,101)):
 assert not lib.ssa_spawn_plan_load(C.byref(p),C.byref(m),C.byref(defs),level,players,limit); report['native_guards']+=1
assert not lib.ssa_spawn_definitions_load(C.byref(defs),ROM,0x44a5d3); report['native_guards']+=1
assert lib.ssa_spawn_plan_load(C.byref(p),C.byref(m),C.byref(defs),0,1,100)
assert not lib.ssa_spawn_plan_load(C.byref(p),C.byref(m),C.byref(defs),0,1,100); report['native_guards']+=1
w=World(); assert lib.ssa_world_init(C.byref(w),C.byref(m),1)
assert p.object_count>1 and not lib.ssa_spawn_plan_insert(C.byref(p),C.byref(w)); assert w.entity_count==0; report['native_guards']+=1
lib.ssa_world_free(C.byref(w))
assert lib.ssa_world_init(C.byref(w),C.byref(m),100)
p.objects[1].entity.radius=-1
assert not lib.ssa_spawn_plan_insert(C.byref(p),C.byref(w)) and w.entity_count==0
assert not any(p.objects[i].entity.owner for i in range(p.object_count)); report['native_guards']+=1
lib.ssa_world_free(C.byref(w)); lib.ssa_spawn_plan_free(C.byref(p))
saved=m.things; m.things=m.size
assert not lib.ssa_spawn_plan_load(C.byref(p),C.byref(m),C.byref(defs),0,1,100); report['native_guards']+=1
m.things=saved; saved=m.sectors; m.sectors=m.size
assert not lib.ssa_spawn_plan_load(C.byref(p),C.byref(m),C.byref(defs),0,1,100); report['native_guards']+=1
m.sectors=saved; lib.ssa_map_assets_free(C.byref(m))
report['source_sha256']=hashlib.sha256((ROOT/'src/spawn.c').read_bytes()).hexdigest()
(ROOT/'analysis/native-spawn-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
