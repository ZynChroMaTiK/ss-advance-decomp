"""Whole 05680/055BC/05548/03464 with real math/BSP/non-stopping wall sweep.

Only line activation 1C0D4 and switch 1C518 are controlled effect boundaries.
Unicorn is test-only. All sixteen actual maps and ordered callback states.
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
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes(); SHA1=hashlib.sha1(ROM).hexdigest()
assert SHA1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world ctypes','exec'))
class Inventory(C.Structure):
 _fields_=[('weapons',C.c_uint16),('ammo',C.c_int16*10),('pending_count',C.c_uint16),('pending_weapons',C.c_uint32*10),('health',C.c_int16),('stat_c4',C.c_int16),('stat_c6',C.c_int16)]
class Hazards(C.Structure):
 _fields_=[('entity',EP),('movement',C.POINTER(Movement)),('inventory',C.POINTER(Inventory)),('weapon_action',C.c_uint32),('facing',C.c_uint8)]
class Game(C.Structure):
 _fields_=[('frame_counter',C.c_uint32),('hazard_sectors',C.POINTER(C.c_int32)),('hazard_count',C.c_size_t),('level_counter',C.c_uint16),('local_flags',C.c_uint16)]
LINE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Hazards),C.c_uint,C.c_uint16)
class Ops(C.Structure): _fields_=[('line',LINE),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in ('player_hazards.c','movement.c','geometry.c','map_assets.c','physics.c')],'-o',str(BUILD/'libssa_player_hazards.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_player_hazards.so'))
for name,args in {
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[C.POINTER(Map)],
 'ssa_map_collision_init':[C.POINTER(Collision),C.POINTER(Map)], 'ssa_map_collision_free':[C.POINTER(Collision)],
 'ssa_map_sector':[C.POINTER(Map),C.c_int32,C.c_int32],
 'ssa_math_tables_load':[C.POINTER(Tables),C.c_void_p,C.c_size_t],
 'ssa_player_hazard_sample':[C.POINTER(Map),EP,C.POINTER(Game)],
 'ssa_player_interaction_query':[C.POINTER(Collision),EP,C.c_uint8,C.POINTER(Tables)],
 'ssa_player_interaction_query_at':[C.POINTER(Collision),EP,C.c_int32,C.c_int32],
 'ssa_player_hazards_apply':[C.POINTER(Hazards),C.POINTER(Map),C.POINTER(Game)],
 'ssa_player_hazards_step':[C.POINTER(Hazards),C.POINTER(Collision),C.POINTER(Tables),C.POINTER(Game),C.POINTER(Ops)],
}.items(): getattr(lib,name).argtypes=args
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
BASE,SCRATCH,ENTITY,PLAYER,GAME,WEAPON,LEVEL,STOP=0x02000000,0x02040000,0x02070000,0x02071000,0x02072000,0x02073000,0x02074000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
def run(a,*args):
 for reg,arg in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2),args): uc.reg_write(reg,arg&0xffffffff)
 uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(a|1,STOP,count=300000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(a)
 return C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
put(0x03004010,'<I',GAME); put(0x03004648,'<I',PLAYER)
put(0x0300464e,'<H',1); put(0x0300572c,'<I',LEVEL)
rng=random.Random(0x5680)
report={'rom_sha1':SHA1,'whole_frames':0,'sample_cases':0,'query_cases':0,'ordered_effects':0,'native_guards':0,'maps':16,
 'scope':'whole 05680 including 055BC,05548,03464,real BSP/trigonometry/non-stopping 0429C wall sweep; only line activation1C0D4/switch1C518 effects controlled; full player/body bytes and ordered callback inputs; not full scripts/gameplay'}

def unpack(raw,action):
 p,e=raw; ent=Entity(); mov=Movement(); inv=Inventory()
 ent.body.x,ent.body.y,ent.body.z=struct.unpack_from('<iii',e)
 ent.radius=struct.unpack_from('<i',e,0x28)[0]; ent.sector=struct.unpack_from('<h',e,0x6a)[0]
 mov.keys=struct.unpack_from('<I',p,0xa4)[0]; mov.flags=struct.unpack_from('<H',p,0xbc)[0]
 inv.health=struct.unpack_from('<h',e,0x32)[0]
 inv.stat_c4,inv.stat_c6=struct.unpack_from('<hh',p,0xc4)
 return Hazards(C.pointer(ent),C.pointer(mov),C.pointer(inv),action,e[0x3f])
def pack(h,raw):
 p,e=map(bytearray,raw); ent=h.entity.contents; inv=h.inventory.contents
 struct.pack_into('<iii',e,0,ent.body.x,ent.body.y,ent.body.z)
 struct.pack_into('<i',e,0x28,ent.radius); struct.pack_into('<h',e,0x6a,ent.sector)
 struct.pack_into('<h',e,0x32,inv.health); e[0x3f]=h.facing
 struct.pack_into('<I',p,0xa4,h.movement.contents.keys); struct.pack_into('<H',p,0xbc,h.movement.contents.flags)
 struct.pack_into('<hh',p,0xc4,inv.stat_c4,inv.stat_c6)
 return bytes(p),bytes(e)
def effect(kind,line,h):
 calls.append((kind,line,pack(h,base),h.weapon_action))
 # Exercise keys reloaded for each line and weapon eligibility latched once.
 if changes:
  h.movement.contents.keys^=0x10000
  h.weapon_action^=0xdc00
  h.inventory.contents.health=C.c_int16(h.inventory.contents.health+7).value
  h.inventory.contents.stat_c4=C.c_int16(h.inventory.contents.stat_c4-3).value
 return -1 if failure else 0
def hook(machine,address,size,data):
 raw=(bytes(uc.mem_read(PLAYER,256)),bytes(uc.mem_read(ENTITY,0x74)))
 h=unpack(raw,get(WEAPON+16,'<I'))
 line=(uc.reg_read(UC_ARM_REG_R0)-BASE-m.lines)//40
 effect(1 if address==0x0801c0d4 else 2,line,h)
 p,e=pack(h,raw); uc.mem_write(PLAYER,p); uc.mem_write(ENTITY,e); put(WEAPON+16,'<I',h.weapon_action)
 uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
for a in (0x0801c0d4,0x0801c518): uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
@LINE
def line_callback(ctx,h,kind,line): return effect(kind,line,h.contents)
ops=Ops(line_callback,None)
changes=failure=False; calls=[]

for level in range(16):
 m=Map(); c=Collision()
 assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
 assert lib.ssa_map_collision_init(C.byref(c),C.byref(m))
 data=bytearray(C.string_at(m.data,m.size))
 def install(raw):
  C.memmove(m.data,bytes(raw),m.size)
  original=bytearray(raw)
  for off in range(0x1c,0x48,4): struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
  uc.mem_write(BASE,bytes(original))
  ox,oy,width,height=struct.unpack_from('<hhhh',raw,m.blockmap)
  put(0x03004930,'<I',BASE); put(0x03004944,'<iihh',ox*65536,oy*65536,width,height)
  put(0x03004954,'<I',SCRATCH)
 install(data)
 vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]))
 checked_error=False
 for iteration in range(512):
  # Second half keeps authentic geometry but marks eight nearby lines as
  # contacts. Original contact storage has only ten slots before live globals;
  # indiscriminately marking every wall would test original RAM corruption.
  working=bytearray(data)
  vx,vy=rng.choice(vertices); x=vx*65536+rng.randrange(-32,33)*65536; y=vy*65536+rng.randrange(-32,33)*65536
  if iteration>=256:
   distances=[]
   for n in range(m.line_count):
    vertex=struct.unpack_from('<H',working,m.lines+n*40)[0]; px,py=vertices[vertex]
    distances.append(((px*65536-x)**2+(py*65536-y)**2,n))
    struct.pack_into('<H',working,m.lines+n*40+36,0)
   for _,n in sorted(distances)[:8]: struct.pack_into('<H',working,m.lines+n*40+36,1+(n&1))
  install(working)
  put(0x03004648,'<I',PLAYER); put(0x0300464e,'<H',1)
  sector=lib.ssa_map_sector(C.byref(m),x,y); assert sector>=0
  p=bytearray(rng.randbytes(256)); e=bytearray(rng.randbytes(0x74))
  floor=struct.unpack_from('<h',working,m.sectors+20*sector)[0]
  z=floor*65536+rng.choice((0,1,65535,65536,-1))
  struct.pack_into('<iii',e,0,x,y,z); struct.pack_into('<i',e,0x28,rng.choice((0,16,32,64))*65536)
  struct.pack_into('<h',e,0x6a,sector); e[0x3f]=iteration&255
  struct.pack_into('<I',p,0xac,ENTITY); struct.pack_into('<I',p,0x2c,WEAPON)
  action=0xdc00 if iteration%3 else 0xdc01
  struct.pack_into('<I',p,0xa4,rng.getrandbits(32))
  base=(bytes(p),bytes(e)); h=unpack(base,action)
  hazard_values=([sector] if iteration%4==0 else [rng.randrange(m.sector_count) for _ in range(iteration%7)])
  hazard_array=(C.c_int32*len(hazard_values))(*hazard_values)
  g=Game(rng.getrandbits(32),hazard_array,len(hazard_values),rng.choice((0,5,6,65535)),8 if iteration%8==0 else 0)
  put(GAME,'<I',g.frame_counter); put(LEVEL+16,'<H',g.level_counter); put(PLAYER+256+0xbc,'<H',g.local_flags)
  put(0x030057a8,'<B',len(hazard_values))
  if hazard_values: put(0x03005780,'<'+'i'*len(hazard_values),*hazard_values)
  uc.mem_write(PLAYER,base[0]); uc.mem_write(ENTITY,base[1]); put(WEAPON+16,'<I',action)
  # Test isolated sample as well, so every frame mask is seen on every map.
  put(GAME,'<I',iteration); g.frame_counter=iteration
  expected=run(0x08003464,ENTITY)
  assert lib.ssa_player_hazard_sample(C.byref(m),h.entity,C.byref(g))==expected,(level,iteration,'sample')
  report['sample_cases']+=1
  # Mark a variety of floor hazards; exact high Z halfword comparison matters.
  tex=rng.choice((0,35,255,254)); m.data[m.sectors+sector*20+10]=tex
  uc.mem_write(BASE+m.sectors+sector*20+10,bytes((tex,)))
  changes=iteration%3==1; calls=[]
  run(0x08005680,PLAYER)
  expected=(bytes(uc.mem_read(PLAYER,256)),bytes(uc.mem_read(ENTITY,0x74)))
  expected_calls=calls; expected_action=get(WEAPON+16,'<I')
  expected_lines=[(get(0x03004620+4*i,'<I')-BASE-m.lines)//40 for i in range(get(0x03004458,'<I'))] if not g.local_flags&8 else None
  calls=[]
  assert lib.ssa_player_hazards_step(C.byref(h),C.byref(c),C.byref(tables),C.byref(g),C.byref(ops))==0,(level,iteration)
  actual=pack(h,base)
  assert actual==expected,(level,iteration,[(k,[(i,a,b) for i,(a,b) in enumerate(zip(actual[k],expected[k])) if a!=b]) for k in range(2)])
  assert h.weapon_action==expected_action
  assert calls==expected_calls,(level,iteration,calls,expected_calls)
  if expected_lines is not None: assert list(c.special_lines[:c.special_count])==expected_lines,(level,iteration,'lines')
  report['whole_frames']+=1; report['ordered_effects']+=len(calls)
  if calls and not checked_error:
   # Failed external effects stop before later contacts or hazard clearing.
   error_h=unpack(base,action); initial_health=error_h.inventory.contents.health
   calls=[]; failure=True
   assert lib.ssa_player_hazards_step(C.byref(error_h),C.byref(c),C.byref(tables),C.byref(g),C.byref(ops))==-1
   assert len(calls)==1
   assert error_h.inventory.contents.health==C.c_int16(initial_health+(7 if changes else 0)).value
   failure=False; report['native_guards']+=1
   @LINE
   def replacement(ctx,player,kind,line):
    player.contents.entity=EP()
    return 0
   bad_ops=Ops(replacement,None); error_h=unpack(base,action)
   assert lib.ssa_player_hazards_step(C.byref(error_h),C.byref(c),C.byref(tables),C.byref(g),C.byref(bad_ops))==-1
   report['native_guards']+=1; checked_error=True
  if iteration<256:
   # Direct 0429C comparison independently covers all 256 headings.
   # Restore entity before potential callbacks and call original direction.
   uc.mem_write(ENTITY,base[1]); run(0x080094e8,iteration<<8,SCRATCH+0x200,SCRATCH+0x204)
   nx=C.c_int32(x+(get(SCRATCH+0x200,'<i')<<6)).value
   ny=C.c_int32(y+(get(SCRATCH+0x204,'<i')<<6)).value
   run(0x0800429c,ENTITY,nx,ny)
   expected_lines=[(get(0x03004620+4*i,'<I')-BASE-m.lines)//40 for i in range(get(0x03004458,'<I'))]
   assert lib.ssa_player_interaction_query(C.byref(c),h.entity,iteration,C.byref(tables))==0
   assert list(c.special_lines[:c.special_count])==expected_lines
   report['query_cases']+=1
   # Original automap0429C destination uses the body-facing64-unit vector
   # multiplied by its current zoom. Verify that generalized sweep too.
   zoom=(4096,8192,32768,65536)[iteration%4]
   zx=C.c_int32(x+((C.c_int32(nx-x).value*zoom)>>16)).value
   zy=C.c_int32(y+((C.c_int32(ny-y).value*zoom)>>16)).value
   run(0x0800429c,ENTITY,zx,zy)
   expected_lines=[(get(0x03004620+4*i,'<I')-BASE-m.lines)//40 for i in range(get(0x03004458,'<I'))]
   assert lib.ssa_player_interaction_query_at(C.byref(c),h.entity,zx,zy)==0
   assert list(c.special_lines[:c.special_count])==expected_lines
   report['automap_query_cases']=report.get('automap_query_cases',0)+1
 # Isolated sampler arithmetic on wrapped coordinates/radii and signed halves.
 for n in range(256):
  ent=Entity(); ent.body.x=C.c_int32(rng.getrandbits(32)).value; ent.body.y=C.c_int32(rng.getrandbits(32)).value
  ent.radius=rng.choice((-2147483648,-65537,-1,0,1,65537,2147483647))
  g.frame_counter=n; put(GAME,'<I',n)
  values=(C.c_int32*1)(n%m.sector_count); g.hazard_sectors=values; g.hazard_count=1
  put(0x03005780,'<i',values[0]); put(0x030057a8,'<B',1)
  put(ENTITY,'<ii',ent.body.x,ent.body.y); put(ENTITY+0x28,'<i',ent.radius)
  expected=run(0x08003464,ENTITY)
  assert lib.ssa_player_hazard_sample(C.byref(m),C.byref(ent),C.byref(g))==expected,(level,n,'wrapped sample')
  report['sample_cases']+=1
 assert checked_error
 print(f'PASS level {level}: 512 whole frames, 768 samples, 256 interaction queries',flush=True)
 # Guards leave authoritative health unchanged before any effect.
 saved=pack(h,base)
 for bad in (None,):
  assert lib.ssa_player_hazards_step(bad,C.byref(c),C.byref(tables),C.byref(g),C.byref(ops))==-1; report['native_guards']+=1
 old=g.hazard_count; g.hazard_count=256
 assert lib.ssa_player_hazards_step(C.byref(h),C.byref(c),C.byref(tables),C.byref(g),C.byref(ops))==-1
 assert pack(h,base)==saved; g.hazard_count=old; report['native_guards']+=1
 old=h.entity.contents.sector; h.entity.contents.sector=-1
 assert lib.ssa_player_hazards_apply(C.byref(h),C.byref(m),C.byref(g))==-1
 h.entity.contents.sector=old; report['native_guards']+=1
 old=c.line_capacity; c.line_capacity=0
 assert lib.ssa_player_interaction_query(C.byref(c),h.entity,0,C.byref(tables))==-1
 c.line_capacity=old; report['native_guards']+=1
 lib.ssa_map_collision_free(C.byref(c)); lib.ssa_map_assets_free(C.byref(m))
report['rom_comparisons']=sum(report[k] for k in ('whole_frames','sample_cases','query_cases','automap_query_cases'))
report['source_sha256']=hashlib.sha256((ROOT/'src/player_hazards.c').read_bytes()).hexdigest()
(ROOT/'analysis/native-player-hazards-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
