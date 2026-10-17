"""Whole 035A8 with real reject/divide/fixed multiply; 037F4 is recorded."""
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
class Explosion(C.Structure): _fields_=[('updating',C.c_uint)]
DAMAGE=C.CFUNCTYPE(C.c_int,C.c_void_p,EP,C.c_uint32)
class Ops(C.Structure): _fields_=[('damage',DAMAGE),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['combat_explosion.c','enemy_targeting.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(BUILD/'libssa_combat_explosion.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_combat_explosion.so'))
lib.ssa_combat_explode.argtypes=[C.POINTER(Explosion),C.POINTER(Map),EP,C.POINTER(EP),C.c_size_t,C.c_uint8,C.c_uint32,C.c_uint32,C.c_int,C.POINTER(Ops)]
lib.ssa_map_assets_load.argtypes=[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint]
lib.ssa_map_assets_free.argtypes=[C.POINTER(Map)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
BASE,POOL,SOURCE,GAME,STOP=0x02000000,0x02080000,0x02090000,0x020a0000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
put(0x03004644,'<I',POOL); put(0x03004010,'<I',GAME); put(0x03004930,'<I',BASE)
entities=(Entity*100)(); source=Entity(); ordered=(EP*100)(); state=Explosion()
addresses={C.addressof(entities[i]):POOL+i*0x74 for i in range(100)}
rng=random.Random(0x35a8)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'type_eligibility':0,'random_arrays':0,'attenuation_edges':0,'real_maps':0,'live_mutations':0,'reentry':0,'guards':0,'callback_failures':0,'damage_requests':0,'maps':[], 'scope':'Whole 035A8; real B18C, unsigned division and IWRAM signed fixed multiply. 037F4 callback recorded, not real damage here. Stable active pool order/count; damage callbacks may mutate live body fields. UBSan enabled.'}
native=[]; oracle=[]; mutation=False; fail_at=0; current_args=None
def change_native(n):
 # A damage callback can mark a later entity dead and change the source type.
 if mutation and n==1:
  entities[2].body.flags|=0x20
  source.type=0x67
def hook(machine,address,size,ctx):
 target=machine.reg_read(UC_ARM_REG_R0); damage=machine.reg_read(UC_ARM_REG_R1)
 oracle.append((target,damage))
 if mutation and len(oracle)==1:
  flags=struct.unpack('<H',machine.mem_read(POOL+2*0x74+0x30,2))[0]
  put(POOL+2*0x74+0x30,'<H',flags|0x20); put(SOURCE+0x3c,'<H',0x67)
 machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook,begin=0x080037f4,end=0x080037f4)
@DAMAGE
def damage_callback(ctx,target,damage):
 native.append((addresses[C.addressof(target.contents)],damage))
 assert state.updating==1
 assert lib.ssa_combat_explode(*current_args)==-1
 report['reentry']+=1
 change_native(len(native))
 return -1 if fail_at and len(native)==fail_at else 0
ops=Ops(damage_callback,None)
def record(e):
 raw=bytearray(0x74)
 struct.pack_into('<ii',raw,0,e.body.x,e.body.y)
 struct.pack_into('<H',raw,0x30,e.body.flags)
 struct.pack_into('<H',raw,0x3c,e.type)
 struct.pack_into('<h',raw,0x6a,e.sector)
 return bytes(raw)
def case(m,indices,mode,radius,damage,include,key,source_index=None):
 global native,oracle,current_args
 native=[]; oracle=[]
 count=len(indices)
 uc.mem_write(POOL,b''.join(record(e) for e in entities)); uc.mem_write(SOURCE,record(source))
 for j,i in enumerate(indices): ordered[j]=C.pointer(entities[i])
 if indices: uc.mem_write(0x03004470,struct.pack('<'+'H'*count,*indices))
 put(0x03004354,'<H',count); put(GAME+0x10,'<B',mode)
 sp=C.pointer(source) if source_index is None else C.pointer(entities[source_index])
 addr=SOURCE if source_index is None else POOL+source_index*0x74
 for r,v in ((UC_ARM_REG_R0,addr),(UC_ARM_REG_R1,radius),(UC_ARM_REG_R2,damage),(UC_ARM_REG_R3,include),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)): uc.reg_write(r,v&0xffffffff)
 uc.emu_start(0x080035a9,STOP,count=200000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 current_args=(C.byref(state),C.byref(m) if m else None,sp,ordered,count,mode,radius,damage,include,C.byref(ops))
 assert lib.ssa_combat_explode(*current_args)==0
 assert not state.updating
 assert native==oracle,(key,mode,radius,damage,include,indices,native,oracle)
 assert b''.join(record(e) for e in entities)==bytes(uc.mem_read(POOL,100*0x74))
 assert record(source)==bytes(uc.mem_read(SOURCE,0x74))
 report[key]+=1; report['damage_requests']+=len(native)

# Every type with player and corpse gates in alternate combinations. Additional
# dedicated boundary loops below cover the full Cartesian flags near CD..D8.
source.type=0x67
for typ in range(65536):
 e=entities[0]; e.type=typ; e.body.flags=(0,0x10,0x20,0x30)[typ%4]
 case(None,[0],typ%2,1,0x80008003,typ%3,'type_eligibility')
print('PASS explosion all 65536 type values',flush=True)
for typ in (0,0x67,0xcc,0xcd,0xd6,0xd7,0xd8,0xd9,0xe1,0xffff):
 for flags in (0,0x10,0x20,0x30,0xffff):
  for include in (0,1,2):
   entities[0].type=typ; entities[0].body.flags=flags
   case(None,[0],1,1,65535,include,'type_eligibility')
axes=[-2**31,-2**31+1,-65537,-65536,-1,0,1,65535,65536,2**31-1]
damages=[0,1,3,4,10,255,32767,32768,65535,65536,0x7fffffff,0x80000000,0xffffffff]
radii=[1,2,16,100,65535,65536,0x100000,0x100001,0x7fffffff,0x80000000,0xffffffff]
for frame in range(4096):
 count=frame%101; indices=rng.sample(range(100),count)
 for e in entities:
  e.type=rng.choice([0x67,0xcc,0xcd,0xcf,0xd6,0xd7,0xd8,0xe1,0xffff])
  e.body.flags=rng.choice([0,0x10,0x20,0x30,0x80,0xffff])
  e.body.x=rng.choice(axes) if frame<512 else rng.randrange(-2**31,2**31)
  e.body.y=rng.choice(axes) if frame<512 else rng.randrange(-2**31,2**31)
 source.body.x=rng.randrange(-2**31,2**31); source.body.y=rng.randrange(-2**31,2**31)
 source.type=rng.choice([0x67,0xcd,0xd8,0xffff])
 case(None,indices,frame%3,rng.choice(radii),rng.choice(damages),frame%2,'random_arrays',None if frame%3 else rng.randrange(100))
print('PASS explosion randomized ordered arrays',flush=True)
source.body.x=source.body.y=0; source.type=0x67
for x,y in ((0,0),(1,0),(-1,0),(10,0),(100,1),(1024,0),(32767,32767),(-32768,-32768)):
 squared=(x*x+y*y)&0xffffffff
 for radius in sorted(set([1,0xffffffff]+[r for r in (squared-1,squared,squared+1) if 0<r<=0xffffffff])):
  for value in damages:
   for i in range(4):
    e=entities[i]; e.type=0xcd; e.body.flags=0x10 if i in (0,2) else 0
    e.body.x=C.c_int32(x<<16).value; e.body.y=C.c_int32(y<<16).value
   # First player's attenuation applies to subsequent targets even outside range.
   entities[1].body.x=entities[1].body.y=0; entities[3].body.x=entities[3].body.y=0
   case(None,[0,1,2,3],1,radius,value,1,'attenuation_edges')
# Self and empty list.
case(None,[],1,1,4,1,'attenuation_edges')
case(None,[0],1,1,4,1,'attenuation_edges',0)
for level in range(16):
 m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
 data=C.string_at(m.data,m.size); uc.mem_write(BASE,data); put(BASE+0x40,'<I',BASE+m.reject)
 source.sector=level%m.sector_count
 for frame in range(128):
  source.type=0x67 if frame%2 else 0xcd
  source.sector=rng.randrange(m.sector_count)
  source.body.x=rng.randrange(-200,200)*65536; source.body.y=rng.randrange(-200,200)*65536
  for e in entities:
   e.type=rng.choice([0,0x67,0xcd,0xd6,0xd7,0xd8,0xe1])
   e.body.flags=rng.choice([0,0x10,0x20,0x30])
   e.sector=rng.randrange(m.sector_count)
   e.body.x=rng.randrange(-200,200)*65536; e.body.y=rng.randrange(-200,200)*65536
  case(m,list(range(100)),frame%3,0 if frame%2 else 65536,rng.choice(damages),frame%3,'real_maps')
 assert C.string_at(m.data,m.size)==data
 report['maps'].append({'level':level,'cases':128,'sectors':m.sector_count})
 lib.ssa_map_assets_free(C.byref(m))
 print('PASS explosion real map',level,flush=True)
mutation=True
for frame in range(64):
 source.type=0xcd; source.body.x=source.body.y=0
 for i in range(4):
  entities[i].type=0xcd; entities[i].body.flags=0x10 if i==3 else 0
  entities[i].body.x=entities[i].body.y=0
 case(None,[0,1,2,3],1,100,frame,1,'live_mutations')
mutation=False
# Guard validation and callback failures leave state usable, preserving prior effects.
args=list(current_args)
for index,value in ((0,None),(2,None),(3,None),(4,101),(9,None)):
 bad=args.copy(); bad[index]=value
 assert lib.ssa_combat_explode(*bad)==-1 and not state.updating; report['guards']+=1
emptyops=Ops(); bad=args.copy(); bad[9]=C.byref(emptyops)
assert lib.ssa_combat_explode(*bad)==-1; report['guards']+=1
badlist=(EP*4)(); bad=args.copy(); bad[3]=badlist
assert lib.ssa_combat_explode(*bad)==-1; report['guards']+=1
state.updating=1
assert lib.ssa_combat_explode(*args)==-1 and state.updating==1; report['guards']+=1
state.updating=0
bad=args.copy(); bad[6]=0
assert lib.ssa_combat_explode(*bad)==-1 and not state.updating; report['guards']+=1
for fail in (1,2,3):
 fail_at=fail; native=[]
 for e in entities: e.body.flags=0; e.type=0xcd
 assert lib.ssa_combat_explode(*args)==-1 and not state.updating and len(native)==fail
 report['callback_failures']+=1
fail_at=0
native=[]; assert lib.ssa_combat_explode(*args)==0 and len(native)==4
report['rom_comparisons']=sum(report[k] for k in ('type_eligibility','random_arrays','attenuation_edges','real_maps','live_mutations'))
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/native-combat-explosion-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
