"""Whole 05394 and 00908 ROM differential proof; Unicorn is test-only."""
from pathlib import Path
import ctypes as C
import hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
SHA1=hashlib.sha1(ROM).hexdigest();assert SHA1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
 str(ROOT/'src/actor_animation.c'),'-o',str(BUILD/'libactor_animation.so')],check=True)
lib=C.CDLL(str(BUILD/'libactor_animation.so'))
class State(C.Structure):
 _fields_=[('animation',C.c_uint16),('frame',C.c_uint16),('index',C.c_uint8),('elapsed',C.c_uint8),('period',C.c_uint8),('done',C.c_uint8)]
class Data(C.Structure):
 _fields_=[('words',C.c_uint16*772),('offsets',C.c_uint16*142),('loaded',C.c_uint)]
class Pool(C.Structure):
 _fields_=[('free',C.c_uint16*100),('active',C.c_uint16*100),('positions',C.c_uint16*100),('count',C.c_uint16),('occupied',C.c_uint8*100)]
load=lib.ssa_actor_animations_load;load.argtypes=[C.POINTER(Data),C.c_void_p,C.c_size_t]
set_anim=lib.ssa_actor_animation_set;set_anim.argtypes=[C.POINTER(State),C.c_uint16,C.c_uint16]
step=lib.ssa_actor_animation_step;step.argtypes=[C.POINTER(State),C.POINTER(Data),C.c_uint16,C.c_uint]
pool_step=lib.ssa_actor_animation_pool_step
pool_step.argtypes=[C.POINTER(State),C.POINTER(C.c_uint16),C.POINTER(Pool),C.POINTER(Data),C.c_uint]
data=Data();assert load(C.byref(data),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
ENTITY,GAME,STOP=0x02000000,0x02004000,0x087ff000
uc.mem_write(0x03004010,struct.pack('<I',GAME));uc.mem_write(0x03004644,struct.pack('<I',ENTITY))
uc.mem_write(0x03004354,struct.pack('<H',1));uc.mem_write(0x03004470,struct.pack('<H',0))
rng=random.Random(0x9085394)
report={'rom_sha1':SHA1,'rom_comparisons':{},'native_guards':0,'animations':142,'sequence_words':772,
 'scope':'Whole05394 and00908, no instruction hooks; complete116-byte entities, actual ROM sequence data, all100 active slots and explicit marker data mutations.'}
def count(key,n=1):report['rom_comparisons'][key]=report['rom_comparisons'].get(key,0)+n
def run(addr,r0=0,r1=0,r2=0):
 for reg,value in ((UC_ARM_REG_R0,r0),(UC_ARM_REG_R1,r1),(UC_ARM_REG_R2,r2),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,value)
 uc.emu_start(addr|1,STOP,count=1000000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
def entity(state,flags=0):
 b=bytearray(rng.randbytes(116));b[0x58:0x60]=bytes(state);struct.pack_into('<H',b,0x30,flags);return b
def compare_set(anim,period):
 s=State.from_buffer_copy(rng.randbytes(8));b=entity(s);uc.mem_write(ENTITY,bytes(b));run(0x08005394,ENTITY,anim,period)
 assert set_anim(C.byref(s),anim,period)==int(anim<=0x8d or anim==0xffff)
 b[0x58:0x60]=bytes(s);assert bytes(b)==bytes(uc.mem_read(ENTITY,116));count('setter')
for a in range(65536):compare_set(a,rng.randrange(65536))
for p in range(65536):compare_set(p%142,p)
print('05394 all IDs and periods passed',flush=True)
def compare_step(s,flags,ticks,key='tick',d=data):
 b=entity(s,flags);uc.mem_write(ENTITY,bytes(b));uc.mem_write(GAME+7,bytes([ticks]));run(0x08000908)
 before=bytes(s);assert step(C.byref(s),C.byref(d),flags,ticks)==1
 b[0x58:0x60]=bytes(s)
 assert bytes(b)==bytes(uc.mem_read(ENTITY,116)),(before.hex(),flags,ticks,bytes(s).hex(),bytes(uc.mem_read(ENTITY+0x58,8)).hex())
 count(key)
lengths=[]
for anim in range(142):
 start=data.offsets[anim];n=0
 while data.words[start+n]<0xfffe:n+=1
 lengths.append(max(1,n))
 for ticks in range(256):
  s=State(anim,rng.randrange(65536),rng.randrange(max(1,n)),rng.randrange(256),(ticks*73+anim)&255,rng.randrange(256))
  compare_step(s,rng.randrange(65536)&~0x800,ticks)
print('00908 all142 sequences and256 timesteps passed',flush=True)
for anim in range(142):
 for period in (0,1,3,5,255):
  s=State();assert set_anim(C.byref(s),anim,period)==1
  for frame in range(64):compare_step(s,0,(1,2,3,7,255)[frame%5],'consecutive')
for i in range(2048):
 s=State.from_buffer_copy(rng.randbytes(8));compare_step(s,rng.randrange(65536)|0x800,rng.randrange(256),'flag800_skip')
# Marker processing skips the marker rather than displaying it. Exhaust all byte
# ticks, elapsed and periods around wrap with data-only patches to the ROM oracle.
saved=bytes(uc.mem_read(0x08449728,16));savedptr=bytes(uc.mem_read(0x08449d30,4));modified=Data.from_buffer_copy(bytes(data))
uc.mem_write(0x08449d30,struct.pack('<I',0x08449728));modified.offsets[0]=0
for marker in range(0xfffa,0xfffe):
 seq=(10,marker,20,30,0xffff)
 uc.mem_write(0x08449728,struct.pack('<5H',*seq))
 for i,v in enumerate(seq):modified.words[i]=v
 for ticks in range(256):
  for period in (1,3,255):compare_step(State(0,33,0,(ticks*13)&255,period,0),0,ticks,'markers',modified)
uc.mem_write(0x08449728,saved);uc.mem_write(0x08449d30,savedptr)
# Whole active pool, shuffled and sparse, with byte-for-byte preservation of
# inactive entities and index bookkeeping. Every active slot is native-ticked.
for case in range(256):
 pool=Pool();states=(State*100)();flags=(C.c_uint16*100)();order=list(range(100));rng.shuffle(order)
 pool.count=case%101;raw=bytearray()
 for slot in range(100):
  anim=rng.randrange(142);states[slot]=State(anim,rng.randrange(65536),rng.randrange(lengths[anim]),rng.randrange(256),rng.randrange(256),rng.randrange(256))
  flags[slot]=rng.randrange(65536);raw+=entity(states[slot],flags[slot])
 for i,slot in enumerate(order[:pool.count]):pool.active[i]=slot;pool.positions[slot]=i;pool.occupied[slot]=1
 ticks=rng.randrange(256);uc.mem_write(ENTITY,bytes(raw));uc.mem_write(GAME+7,bytes([ticks]))
 uc.mem_write(0x03004354,struct.pack('<H',pool.count));uc.mem_write(0x03004470,bytes(pool.active));run(0x08000908)
 assert pool_step(states,flags,C.byref(pool),C.byref(data),ticks)==1
 for slot in range(100):raw[slot*116+0x58:slot*116+0x60]=bytes(states[slot])
 assert bytes(raw)==bytes(uc.mem_read(ENTITY,len(raw)))
 assert bytes(pool.active)==bytes(uc.mem_read(0x03004470,200))
 count('whole_pool_frames');count('whole_pool_entities',pool.count)
def guard(value):assert value==-1;report['native_guards']+=1
guard(load(None,ROM,len(ROM)));guard(load(C.byref(Data()),None,len(ROM)));guard(load(C.byref(Data()),ROM,100))
bad=bytearray(ROM);struct.pack_into('<I',bad,0x449d30,0x08449729);guard(load(C.byref(Data()),bytes(bad),len(bad)))
guard(set_anim(None,0,0));s=State(65535,3,2,1,1,4);before=bytes(s)
guard(step(C.byref(s),C.byref(data),0,1));assert bytes(s)==before
guard(step(None,C.byref(data),0,1));guard(step(C.byref(s),None,0,1));guard(step(C.byref(s),C.byref(data),0,256))
bad_data=Data.from_buffer_copy(bytes(data));bad_data.offsets[0]=772;s.animation=0;before=bytes(s)
guard(step(C.byref(s),C.byref(bad_data),0,1));assert bytes(s)==before
pool=Pool();pool.count=2;pool.active[0]=0;pool.active[1]=1;pool.occupied[0]=pool.occupied[1]=1;pool.positions[1]=1
states=(State*100)();flags=(C.c_uint16*100)();states[0]=State(4,77,0,0,1,0);states[1].animation=65535;before=bytes(states)
guard(pool_step(states,flags,C.byref(pool),C.byref(data),1));assert bytes(states)==before
states[1].animation=4;pool.positions[1]=0;guard(pool_step(states,flags,C.byref(pool),C.byref(data),1))
pool.count=101;guard(pool_step(states,flags,C.byref(pool),C.byref(data),1))
report['total_rom_comparisons']=sum(v for k,v in report['rom_comparisons'].items() if k!='whole_pool_entities')
report['source_sha256']=hashlib.sha256((ROOT/'src/actor_animation.c').read_bytes()).hexdigest()
(ROOT/'analysis/actor-animation-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
