"""Original switch initialization/activation; original string comparator runs intact."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD = ROOT/'build/native-tests'; BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
 str(ROOT/'src/switches.c'),str(ROOT/'src/map_assets.c'),'-o',str(BUILD/'libssa_switches.so')],check=True)
class Map(C.Structure):
 _fields_=[('data',C.POINTER(C.c_uint8)),('size',C.c_size_t)]+[(n+'_count',C.c_uint16) for n in
 ('thing','texture','sector','node','leaf','segment','side','line','vertex')]+[(n,C.c_uint32) for n in
 ('things','textures','sectors','nodes','leaves','segments','sides','lines','vertices','reject','blockmap','rom_offset')]
class Entry(C.Structure):
 _fields_=[('line',C.c_uint16),('off_texture',C.c_uint8),('on_texture',C.c_uint8)]+[(n,C.c_uint16) for n in ('flags','type','trigger','reserved')]
class Switches(C.Structure):
 _fields_=[('map',C.POINTER(Map)),('entries',C.POINTER(Entry)),('count',C.c_size_t),('updating',C.c_uint)]
Callback=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint)
class Ops(C.Structure): _fields_=[('enqueue',Callback),('sound',Callback),('context',C.c_void_p)]
lib=C.CDLL(str(BUILD/'libssa_switches.so'))
for name,args in {
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[C.POINTER(Map)],
 'ssa_switches_init':[C.POINTER(Switches),C.POINTER(Map),C.c_void_p,C.c_size_t],
 'ssa_switches_free':[C.POINTER(Switches)],'ssa_switch_activate':[C.POINTER(Switches),C.c_uint,C.POINTER(Ops)]
}.items(): getattr(lib,name).argtypes=args
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
BASE,POOL,STOP=0x02000000,0x02080000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
def ret(): uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
allocations=[]
def allocate(machine,address,size,user):
 n=machine.reg_read(UC_ARM_REG_R0); allocations.append(n)
 machine.mem_write(POOL,bytes(n+16)); machine.reg_write(UC_ARM_REG_R0,POOL); ret()
uc.hook_add(UC_HOOK_CODE,allocate,begin=0x080033d8,end=0x080033d8)
events=[]; mutation=None
def callback(machine,address,size,user):
 events.append((address,machine.reg_read(UC_ARM_REG_R0),bytes(machine.mem_read(POOL,12*active_count)),bytes(machine.mem_read(BASE,active_size))))
 if mutation and address==0x0801c670:
  offset,data=mutation; machine.mem_write(POOL+offset,data)
 ret()
for a in (0x0801c670,0x08008d5c): uc.hook_add(UC_HOOK_CODE,callback,begin=a,end=a)
def run(address,arg=0):
 for r,v in ((UC_ARM_REG_R0,arg),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)): uc.reg_write(r,v)
 uc.emu_start(address|1,STOP,count=2000000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
 return uc.reg_read(UC_ARM_REG_R0)
def raw_map(m): return C.string_at(m.data,m.size)
def oracle_map(m):
 raw=bytearray(raw_map(m))
 for off in range(0x1c,0x48,4): struct.pack_into('<I',raw,off,BASE+struct.unpack_from('<I',raw,off)[0])
 struct.pack_into('<H',raw,8,m.texture_count); struct.pack_into('<H',raw,0x14,m.line_count)
 uc.mem_write(BASE,bytes(raw)); put(0x03004930,'<I',BASE)
 return bytes(raw)
names=[ROM[struct.unpack_from('<I',ROM,0x44dd8c+4*i)[0]-0x08000000:][:8] for i in range(4)]
rng=random.Random(0x1c3b4)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'initializer_comparisons':0,'lookup_comparisons':0,
 'activation_comparisons':0,'native_guards':0,'callback_failures':0,'real_maps':16,
 'scope':'Whole 1C3B4/1C374/1C518; original 0C57F4 comparator and ROM character classification execute. Allocation boundary supplies zero-filled storage. Activation enqueue/sound are controlled, with ordered state snapshots and callback mutation. No script or audio implementation.'}
def compare_init(m):
 s=Switches(); before=oracle_map(m); original=raw_map(m); allocations.clear()
 run(0x0801c3b4)
 assert lib.ssa_switches_init(C.byref(s),C.byref(m),ROM,len(ROM))==0
 assert s.count==get(0x030057a4,'<I')
 assert allocations==[12*s.count]
 assert C.string_at(s.entries,12*s.count)==bytes(uc.mem_read(POOL,12*s.count))
 assert before==bytes(uc.mem_read(BASE,m.size)) and original==raw_map(m)
 report['initializer_comparisons']+=1
 for i,name in enumerate(names):
  actual=run(0x0801c374,i)
  expected=next((BASE+m.textures+j*16 for j in range(m.texture_count)
                 if original[m.textures+j*16:m.textures+j*16+8].split(b'\0')[0].lower()==name.split(b'\0')[0].lower()),0)
  assert actual==expected,(i,actual,expected)
  report['lookup_comparisons']+=1
 return s

active_count=active_size=0
def compare_activate(s,m,line,mutate=False):
 global active_count,active_size,mutation
 active_count=s.count; active_size=m.size
 before=oracle_map(m); put(0x030057a4,'<I',s.count); put(0x030057ac,'<I',POOL)
 uc.mem_write(POOL,C.string_at(s.entries,s.count*12)); events.clear()
 ix=next(i for i in range(s.count) if s.entries[i].line==line)
 mutation=(ix*12+4,struct.pack('<H',0x9a52)) if mutate else None
 run(0x0801c518,BASE+m.lines+40*line)
 native_events=[]
 def native(address,arg):
  snap=bytearray(raw_map(m))
  for off in range(0x1c,0x48,4): struct.pack_into('<I',snap,off,BASE+struct.unpack_from('<I',snap,off)[0])
  struct.pack_into('<H',snap,8,m.texture_count); struct.pack_into('<H',snap,0x14,m.line_count)
  native_events.append((address,arg,C.string_at(s.entries,12*s.count),bytes(snap)))
  if mutation and address==0x0801c670: C.memmove(C.addressof(s.entries.contents)+mutation[0],mutation[1],len(mutation[1]))
  return 0
 en=Callback(lambda ctx,arg:native(0x0801c670,arg)); snd=Callback(lambda ctx,arg:native(0x08008d5c,arg)); ops=Ops(en,snd,None)
 assert lib.ssa_switch_activate(C.byref(s),line,C.byref(ops))==0
 assert native_events==events,(line,'callback state/order')
 assert C.string_at(s.entries,12*s.count)==bytes(uc.mem_read(POOL,12*s.count))
 native_raw=bytearray(before)
 native_raw[m.sides:m.sides+12*m.side_count]=raw_map(m)[m.sides:m.sides+12*m.side_count]
 assert bytes(native_raw)==bytes(uc.mem_read(BASE,m.size))
 report['activation_comparisons']+=1

for level in range(16):
 m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
 s=compare_init(m)
 for i in range(s.count):
  compare_activate(s,m,s.entries[i].line)
  compare_activate(s,m,s.entries[i].line)
 assert lib.ssa_switches_free(C.byref(s))==0
 lib.ssa_map_assets_free(C.byref(m))

def synthetic(count=12,textures=300):
 m=Map(); m.textures=0x60; m.texture_count=textures; m.sides=m.textures+16*textures
 m.side_count=count; m.lines=m.sides+12*count; m.line_count=count; m.size=m.lines+40*count
 buf=(C.c_uint8*m.size)(); m.data=C.cast(buf,C.POINTER(C.c_uint8))
 def write(off,fmt,*v): C.memmove(C.addressof(buf)+off,struct.pack(fmt,*v),struct.calcsize(fmt))
 for off,value in ((0x20,m.textures),(0x34,m.sides),(0x38,m.lines)): write(off,'<I',value)
 for i in range(textures): C.memmove(C.addressof(buf)+m.textures+16*i,b'UNKNOWN\0',8)
 slots=rng.sample(range(textures),4)
 for i,k in enumerate(slots):
  name=bytes(c+32 if 65<=c<=90 and rng.randrange(2) else c for c in names[i])
  C.memmove(C.addressof(buf)+m.textures+16*k,name,8)
 for i in range(count):
  write(m.lines+40*i+8,'<h',i)
  write(m.lines+40*i+36,'<HH',rng.choice([0,2,2,2,4]),rng.randrange(65536))
  write(m.sides+12*i+6,'<h',rng.choice(slots+[next(j for j in range(textures) if j not in slots)]))
 return m,buf,slots

for case in range(512):
 m,buf,slots=synthetic(rng.randrange(1,20))
 s=compare_init(m)
 for repeat in range(8):
  if not s.count: break
  for i in range(s.count):
   e=s.entries[i]; e.flags=rng.randrange(65536); e.on_texture=rng.randrange(256); e.off_texture=rng.randrange(256)
   e.trigger=rng.randrange(65536); e.type=rng.randrange(65536); e.reserved=rng.randrange(65536)
  compare_activate(s,m,s.entries[rng.randrange(s.count)].line,repeat%2==0)
 assert lib.ssa_switches_free(C.byref(s))==0

# Empty levels and duplicate registration select the earliest record.
m,buf,slots=synthetic(0)
s=compare_init(m); assert not s.count
assert lib.ssa_switches_free(C.byref(s))==0
m,buf,slots=synthetic(4)
for i in range(4): C.memmove(C.addressof(buf)+m.lines+40*i+36,struct.pack('<H',2),2)
s=compare_init(m)
for i in range(1,s.count): s.entries[i].line=s.entries[0].line
for flags in (0,1,0xfffe,0xffff):
 s.entries[0].flags=flags
 compare_activate(s,m,s.entries[0].line,True)
assert lib.ssa_switches_free(C.byref(s))==0

# Malformed inputs and callback failure contracts have no original counterpart.
m,buf,slots=synthetic(); s=Switches()
def guard_init(out,mp,rom,size):
 before=bytes(s); raw=raw_map(m)
 assert lib.ssa_switches_init(out,mp,rom,size)==-1
 assert before==bytes(s) and raw==raw_map(m)
 report['native_guards']+=1
for args in ((None,C.byref(m),ROM,len(ROM)),(C.byref(s),None,ROM,len(ROM)),
 (C.byref(s),C.byref(m),None,len(ROM)),(C.byref(s),C.byref(m),ROM,0x44dd9b)):
 guard_init(*args)
for address in (0x07ffffff,0x08000000+len(ROM)-7,0xffffffff):
 corrupt=bytearray(ROM); struct.pack_into('<I',corrupt,0x44dd8c,address)
 guard_init(C.byref(s),C.byref(m),bytes(corrupt),len(corrupt))
for field,value in (('lines',m.size),('sides',m.size),('textures',m.size)):
 old=getattr(m,field); saved=bytes(s); setattr(m,field,value)
 assert lib.ssa_switches_init(C.byref(s),C.byref(m),ROM,len(ROM))==-1 and bytes(s)==saved
 setattr(m,field,old); report['native_guards']+=1
empty_map=Map()
guard_init(C.byref(s),C.byref(empty_map),ROM,len(ROM))
# Force a known switch on line zero and then corrupt each required relation.
def store(off,fmt,v): C.memmove(C.addressof(buf)+off,struct.pack(fmt,v),struct.calcsize(fmt))
store(m.lines+36,'<H',2); store(m.sides+6,'<H',slots[0])
for offset,fmt,value in ((m.lines+8,'<H',0xffff),(m.sides+6,'<H',0x8000),(m.sides+6,'<H',300)):
 old=C.string_at(C.addressof(buf)+offset,struct.calcsize(fmt)); store(offset,fmt,value)
 guard_init(C.byref(s),C.byref(m),ROM,len(ROM)); C.memmove(C.addressof(buf)+offset,old,len(old))
old=C.string_at(C.addressof(buf)+m.textures+16*slots[1],8)
C.memmove(C.addressof(buf)+m.textures+16*slots[1],b'ABSENT\0\0',8)
guard_init(C.byref(s),C.byref(m),ROM,len(ROM)); C.memmove(C.addressof(buf)+m.textures+16*slots[1],old,8)
assert lib.ssa_switches_init(C.byref(s),C.byref(m),ROM,len(ROM))==0
guard_init(C.byref(s),C.byref(m),ROM,len(ROM))
noop=Callback(lambda ctx,arg:0); ops=Ops(noop,noop,None); null=Callback()
for sp,line,op in ((None,0,C.byref(ops)),(C.byref(s),m.line_count,C.byref(ops)),(C.byref(s),0,None),
 (C.byref(s),0,C.byref(Ops(null,noop,None))),(C.byref(s),0,C.byref(Ops(noop,null,None)))):
 assert lib.ssa_switch_activate(sp,line,op)==-1; report['native_guards']+=1
line=s.entries[0].line
s.updating=1
assert lib.ssa_switch_activate(C.byref(s),line,C.byref(ops))==-1 and lib.ssa_switches_free(C.byref(s))==-1
report['native_guards']+=2; s.updating=0
# Ensure missing record is rejected even when valid side exists.
saved=[s.entries[i].line for i in range(s.count)]
for i in range(s.count): s.entries[i].line=65535
assert lib.ssa_switch_activate(C.byref(s),0,C.byref(ops))==-1; report['native_guards']+=1
for i,v in enumerate(saved): s.entries[i].line=v
for failing in ('enqueue','sound'):
 for repeat in range(64):
  calls=[]; nested=[]; s.entries[0].flags=0; s.entries[0].on_texture=repeat
  def en(ctx,arg):
   calls.append('enqueue'); nested.extend((lib.ssa_switch_activate(C.byref(s),line,C.byref(ops)),lib.ssa_switches_free(C.byref(s))))
   return -1 if failing=='enqueue' else 7
  def snd(ctx,arg): calls.append('sound'); return -1
  en_cb,snd_cb=Callback(en),Callback(snd); failing_ops=Ops(en_cb,snd_cb,None)
  assert lib.ssa_switch_activate(C.byref(s),line,C.byref(failing_ops))==-1
  assert calls==(['enqueue'] if failing=='enqueue' else ['enqueue','sound']) and nested==[-1,-1]
  assert not s.updating and s.entries[0].flags&1
  side_index=struct.unpack_from('<H',raw_map(m),m.lines+40*line+8)[0]
  assert struct.unpack_from('<H',raw_map(m),m.sides+12*side_index+6)[0]==repeat
  report['callback_failures']+=1
assert lib.ssa_switches_free(C.byref(s))==0 and lib.ssa_switches_free(C.byref(s))==0
assert lib.ssa_switches_free(None)==-1; report['native_guards']+=1
report['rom_comparisons']=sum(report[k] for k in ('initializer_comparisons','lookup_comparisons','activation_comparisons'))
report['source_sha256']=hashlib.sha256((ROOT/'src/switches.c').read_bytes()).hexdigest(); report['passed']=True
(ROOT/'analysis/native-switches-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
