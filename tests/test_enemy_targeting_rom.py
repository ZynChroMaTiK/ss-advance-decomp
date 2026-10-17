"""Whole original nearest routines and reject bit, without controlled callees."""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'ctypes world','exec'))
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['enemy_targeting.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(BUILD/'libssa_enemy_targeting.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_enemy_targeting.so'))
for name,args in {
 'ssa_enemy_nearest':[EP,C.POINTER(EP),C.c_size_t,C.POINTER(EP)],
 'ssa_pickup_nearest':[EP,C.POINTER(EP),C.c_size_t,C.POINTER(EP)],
 'ssa_sector_reject':[C.POINTER(Map),C.c_int,C.c_int],
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
 'ssa_map_assets_free':[C.POINTER(Map)]
}.items(): getattr(lib,name).argtypes=args
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
BASE,POOL,EXTERNAL,STOP=0x02000000,0x02080000,0x02090000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def run(addr,a,b=0,c=0):
 for r,v in ((UC_ARM_REG_R0,a),(UC_ARM_REG_R1,b),(UC_ARM_REG_R2,c),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)): uc.reg_write(r,v&0xffffffff)
 uc.emu_start(addr|1,STOP,count=20000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 return uc.reg_read(UC_ARM_REG_R0)
rng=random.Random(0x36c4)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'nearest_enemy':0,'nearest_pickup':0,'reject_real_map_pairs':0,'reject_synthetic':0,'guards':0,'maps':[],'scope':'Whole036C4/03758/B18C unhooked ROM instructions; stable ordered pool snapshots; allsectorpairs on16maps; UBSan native helpers; visibility bit does not prove lineofsight.'}
entities=(Entity*100)(); external=Entity(); ordered=(EP*100)(); output=EP()
put(0x03004644,'<I',POOL)
addresses={C.addressof(entities[i]):POOL+i*0x74 for i in range(100)}
def nearest_case(count,indices,source_index):
 raw=bytearray(rng.randbytes(100*0x74))
 for i,e in enumerate(entities):
  struct.pack_into('<ii',raw,i*0x74,e.body.x,e.body.y)
  struct.pack_into('<H',raw,i*0x74+0x30,e.body.flags)
  struct.pack_into('<H',raw,i*0x74+0x3c,e.type)
 uc.mem_write(POOL,bytes(raw))
 for j,i in enumerate(indices): ordered[j]=C.pointer(entities[i])
 active=struct.pack('<'+'H'*len(indices),*indices)
 if active: uc.mem_write(0x03004470,active)
 put(0x03004354,'<H',count)
 if source_index is None:
  source=C.pointer(external); source_addr=EXTERNAL
  put(EXTERNAL,'<ii',external.body.x,external.body.y)
 else: source=C.pointer(entities[source_index]); source_addr=POOL+source_index*0x74
 native_before=bytes(entities)
 for addr,name,key in ((0x080036c4,'ssa_enemy_nearest','nearest_enemy'),(0x08003758,'ssa_pickup_nearest','nearest_pickup')):
  expected=run(addr,source_addr)
  assert getattr(lib,name)(source,ordered,count,C.byref(output))==0
  actual=addresses[C.addressof(output.contents)] if output else 0
  assert actual==expected,(key,count,source_index,hex(actual),hex(expected))
  assert bytes(raw)==bytes(uc.mem_read(POOL,len(raw))) and native_before==bytes(entities)
  assert (not active or active==bytes(uc.mem_read(0x03004470,len(active))))
  report[key]+=1
# Exhaustive type and corpse-flag eligibility, self exclusion and empty pools.
for typ in range(65536):
 entities[0].type=typ; entities[0].body.flags=0x20 if typ&1 else 0
 entities[0].body.x=entities[0].body.y=0
 # Avoid full-array case preparation for exhaustive scalar eligibility.
 put(POOL,'<ii',0,0); put(POOL+0x30,'<H',entities[0].body.flags); put(POOL+0x3c,'<H',typ)
 put(EXTERNAL,'<ii',0,0); put(0x03004470,'<H',0); put(0x03004354,'<H',1)
 ordered[0]=C.pointer(entities[0])
 for addr,name,key in ((0x080036c4,'ssa_enemy_nearest','nearest_enemy'),(0x08003758,'ssa_pickup_nearest','nearest_pickup')):
  expected=run(addr,EXTERNAL)
  assert getattr(lib,name)(C.byref(external),ordered,1,C.byref(output))==0
  assert (POOL if output else 0)==expected
  report[key]+=1
print('PASS all65536 type values for both selectors',flush=True)
axes=[-2147483648,-2147483647,-65537,-65536,-1,0,1,65535,65536,2147483647]
for frame in range(4096):
 count=frame%101; indices=rng.sample(range(100),count)
 for i,e in enumerate(entities):
  e.type=rng.choice([0xcc,0xcd,0xcf,0xd6,0xd7,0xd8,0xe1,0xffff])
  e.body.flags=rng.randrange(65536)
  e.body.x=rng.choice(axes) if frame<512 else rng.randrange(-2**31,2**31)
  e.body.y=rng.choice(axes) if frame<512 else rng.randrange(-2**31,2**31)
 external.body.x=rng.randrange(-2**31,2**31); external.body.y=rng.randrange(-2**31,2**31)
 nearest_case(count,indices,None if frame%2 else rng.randrange(100))
# Deliberate earliest ties, negative signed squaredsum and source identity.
for typ in (0xcd,0xd6,0xe1):
 for flags in (0,0x20,0xffff):
  for x,y in ((0,0),(65536,0),(-2147483648,-2147483648),(-1,-1)):
   external.body.x=external.body.y=0
   for e in entities:
    e.type=typ; e.body.flags=flags; e.body.x=x; e.body.y=y
   for order in ([0,1],[1,0],[1,0,99],[]): nearest_case(len(order),order,None)
   nearest_case(1,[0],0)
print('PASS ordered arrays, ties, self exclusion and signed overflow distances',flush=True)
for level in range(16):
 m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
 data=C.string_at(m.data,m.size); uc.mem_write(BASE,data)
 put(BASE+0x40,'<I',BASE+m.reject)
 reject_before=data[m.reject:m.reject+(m.sector_count*m.sector_count+7)//8]
 nonzero=0
 for a in range(m.sector_count):
  for b in range(m.sector_count):
   expected=run(0x0800b18c,BASE,a,b)
   assert lib.ssa_sector_reject(C.byref(m),a,b)==expected,(level,a,b,expected)
   report['reject_real_map_pairs']+=1; nonzero+=bool(expected)
 assert C.string_at(m.data,m.size)==data
 assert bytes(uc.mem_read(BASE+m.reject,len(reject_before)))==reject_before
 report['maps'].append({'level':level,'sectors':m.sector_count,'pairs':m.sector_count**2,'rejected':nonzero})
 lib.ssa_map_assets_free(C.byref(m))
 print('PASS reject map',level,flush=True)
# Distinct asymmetric bytes explicitly exercise every bit and row stride.
buf=(C.c_uint8*128)(*[rng.randrange(256) for _ in range(128)])
m=Map(); m.data=buf; m.size=128; m.sector_count=31; m.reject=3
uc.mem_write(BASE,bytes(128)); put(BASE+0xa,'<H',31); put(BASE+0x40,'<I',BASE+256+3)
uc.mem_write(BASE+256,bytes(buf))
for a in range(31):
 for b in range(31):
  assert lib.ssa_sector_reject(C.byref(m),a,b)==run(0x0800b18c,BASE,a,b)
  report['reject_synthetic']+=1
# Invalid arguments leave output unchanged; empty array is valid.
for name in ('ssa_enemy_nearest','ssa_pickup_nearest'):
 fn=getattr(lib,name); output=C.pointer(entities[1]); sentinel=C.addressof(output.contents)
 bad=(EP*1)()
 for args in ((None,ordered,1,C.byref(output)),(C.byref(external),None,1,C.byref(output)),(C.byref(external),ordered,101,C.byref(output)),(C.byref(external),ordered,1,None),(C.byref(external),bad,1,C.byref(output))):
  assert fn(*args)==-1 and C.addressof(output.contents)==sentinel
  report['guards']+=1
 assert fn(C.byref(external),None,0,C.byref(output))==0 and not output
for a,b in ((-1,0),(0,-1),(31,0),(0,31),(32768,0),(0,65536)):
 assert lib.ssa_sector_reject(C.byref(m),a,b)==-1; report['guards']+=1
assert lib.ssa_sector_reject(None,0,0)==-1; report['guards']+=1
for field,value in (('size',123),('reject',129),('sector_count',0),('data',None)):
 old=getattr(m,field)
 # ctypes pointer field retrieval aliases its own storage: save address explicitly.
 if field=='data': old=C.cast(C.addressof(buf),C.POINTER(C.c_uint8))
 setattr(m,field,value)
 assert lib.ssa_sector_reject(C.byref(m),0,0)==-1; report['guards']+=1
 setattr(m,field,old)
report['rom_comparisons']=sum(report[k] for k in ('nearest_enemy','nearest_pickup','reject_real_map_pairs','reject_synthetic'))
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/native-enemy-targeting-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
