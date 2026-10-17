"""Whole1B854 with actual05330/05394; controlled allocation/insertion/particles."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef):nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets):nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world declarations','exec'))
class Marker(C.Structure):_fields_=[('x',C.c_int16),('y',C.c_int16),('z',C.c_int16),('heading',C.c_uint16)]
class Script(C.Structure):_fields_=[('x',C.c_int32),('y',C.c_int32),('z',C.c_int32),('type',C.c_uint16),('flags',C.c_uint16),('trigger',C.c_uint16),('timer',C.c_uint16),('floor',C.c_uint16),('variant',C.c_uint8),('facing',C.c_uint8)]
class Data(C.Structure):_fields_=[('schedule',(C.c_uint16*34)*4),('loaded',C.c_uint)]
class State(C.Structure):_fields_=[('elapsed',C.c_uint16),('sequence',C.c_uint16),('marker',C.c_uint16),('updating',C.c_uint)]
SP=C.POINTER(Script)
OC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Marker),C.POINTER(C.c_int))
AL=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(SP))
AN=C.CFUNCTYPE(C.c_int,C.c_void_p,SP,C.c_uint,C.c_uint)
IN=C.CFUNCTYPE(C.c_int,C.c_void_p,SP)
PA=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_int32,C.c_int32,C.c_int32)
class Ops(C.Structure):_fields_=[('occupied',OC),('allocate',AL),('animation',AN),('insert',IN),('particle',PA),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/pickup_respawn.c'),'-o',str(B/'libssa_pickup_respawn.so')],check=True)
lib=C.CDLL(str(B/'libssa_pickup_respawn.so'))
lib.ssa_pickup_respawn_data_load.argtypes=[C.POINTER(Data),C.c_void_p,C.c_size_t]
run=lib.ssa_pickup_respawn_step;run.argtypes=[C.POINTER(State),C.POINTER(Data),C.c_uint,C.c_uint,C.c_uint,C.POINTER(Marker),C.c_size_t,C.c_int32,C.POINTER(Ops)]
occ=lib.ssa_pickup_respawn_occupied;occ.argtypes=[C.POINTER(Marker),C.POINTER(EP),C.c_size_t]
data=Data();assert lib.ssa_pickup_respawn_data_load(C.byref(data),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0,0x1000)
G,L,E,POOL,MARKERS,STOP=0x02000000,0x02001000,0x02002000,0x02010000,0x03004040,0x087ff000
uc.mem_write(0x03004010,struct.pack('<I',G));uc.mem_write(0x0300572c,struct.pack('<I',L));uc.mem_write(0x03004644,struct.pack('<I',POOL))
rng=random.Random(0x1b854);state=State();context={};logs=[[],[]]
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_respawn':0,'whole_occupancy':0,'reentry_guards':0,'guards':0,'failures':0,'particles':0,'allocated':0,'exhausted':0,'scope':__doc__+' Fullgame/entity/callback snapshots; all65536elapsedvalues; originalnullallocationreadataddress8 suppliedexplicitly asfallbackZ.'}
fields=[('x',0,'i'),('y',4,'i'),('z',8,'i'),('type',60,'H'),('flags',48,'H'),('trigger',104,'H'),('timer',78,'H'),('floor',114,'H'),('variant',62,'B'),('facing',63,'B')]
def rd(d,o,f='I'):return struct.unpack_from('<'+f,d,o)[0]
def wr(d,o,v,f='I'):struct.pack_into('<'+f,d,o,v&({'I':0xffffffff,'H':65535,'B':255}[f]))
def entitybytes():
 raw=bytearray(context['raw'])
 for n,o,f in fields:struct.pack_into('<'+f,raw,o,getattr(context['entity'],n))
 return bytes(raw)
def gamebytes():
 raw=bytearray(context['game']);struct.pack_into('<HHH',raw,126,state.elapsed,state.sequence,state.marker);return bytes(raw)
def args():return [C.byref(state),C.byref(data),context['players'],context['level'],context['ticks'],context['markers'],context['count'],context['fallback'],C.byref(ops)]
def log(kind,*args):
 assert run(*globals()['args']())==-1;report['reentry_guards']+=1
 logs[0].append((kind,args,gamebytes(),entitybytes()))
 return -1 if context.get('fail')==kind else 0
def occupied(ctx,m,out):
 status=log('occupied',C.addressof(m.contents)-C.addressof(context['markers']))
 out[0]=occ(m,context['entities'],context['active']);return status
def allocate(ctx,out):
 status=log('allocate');out[0]=C.pointer(context['entity']) if context['allocated'] else SP();return status
def animation(ctx,e,a,t):
 status=log('animation',a,t)
 if a<=0x8d or a==65535:
  wr(context['raw'],88,a,'H');wr(context['raw'],94,t,'B')
  for o in (92,93,95):wr(context['raw'],o,0,'B')
 return status
def insert(ctx,e):
 status=log('insert');e.contents.floor=context['floor'];return status
def particle(ctx,a,x,y,z):return log('particle',a,x,y,z)
callbacks=(OC(occupied),AL(allocate),AN(animation),IN(insert),PA(particle));ops=Ops(*callbacks,None)
def hook(machine,address,size,unused):
 r=[machine.reg_read(x) for x in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
 if address==0x08005330:kind='occupied';a=(r[0]-MARKERS,)
 elif address==0x08005170:kind='allocate';a=()
 elif address==0x08005394:kind='animation';a=(r[1],r[2])
 elif address==0x0800a5d0:kind='insert';a=()
 else:kind='particle';a=(r[0],*[C.c_int32(v).value for v in r[1:]])
 logs[1].append((kind,a,bytes(machine.mem_read(G,256)),bytes(machine.mem_read(E,116))))
 if kind in ('occupied','animation'):return
 if kind=='allocate':machine.reg_write(UC_ARM_REG_R0,E if context['allocated'] else 0)
 elif kind=='insert':machine.mem_write(E+114,struct.pack('<H',context['floor']))
 machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for a in (0x08005330,0x08005170,0x08005394,0x0800a5d0,0x08014b60):uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def compare(elapsed,level=None,players=None,force=None):
 count=rng.randrange(1,9);markers=(Marker*count)();g=bytearray(rng.randbytes(256));raw=bytearray(rng.randbytes(116))
 for m in markers:m.x=rng.randrange(-32768,32768);m.y=rng.randrange(-32768,32768);m.heading=rng.randrange(65536)
 state.elapsed=elapsed;state.sequence=rng.randrange(16)*2;state.marker=rng.randrange(count)
 level=rng.choice([0,11,12,13,14,15,16,65535]) if level is None else level
 players=rng.choice([0,1,2,4,255]) if players is None else players;ticks=rng.choice([0,1,2,127,255])
 if force:level=12;players=2;ticks=1;state.elapsed=300
 allocated=rng.randrange(2) if not force else True;active=rng.randrange(5);entities=(Entity*active)();ptrs=(EP*active)();slots=rng.sample(range(100),active)
 for i,e in enumerate(entities):
  e.body.x=C.c_int32(rng.getrandbits(32)).value;e.body.y=C.c_int32(rng.getrandbits(32)).value
  if not force and i==0 and rng.randrange(2):
   e.body.x=C.c_int32((markers[state.marker].x<<16)|rng.randrange(65536)).value;e.body.y=C.c_int32((markers[state.marker].y<<16)|rng.randrange(65536)).value
  ptrs[i]=C.pointer(e);uc.mem_write(POOL+slots[i]*116,struct.pack('<ii',e.body.x,e.body.y))
 uc.mem_write(0x03004470,struct.pack('<'+'H'*active,*slots));uc.mem_write(0x03004354,struct.pack('<H',active))
 entity=Script(**{n:rd(raw,o,f) for n,o,f in fields});wr(g,7,ticks,'B');wr(g,16,players,'B');struct.pack_into('<HHH',g,126,state.elapsed,state.sequence,state.marker)
 context.clear();context.update(raw=raw,game=g,entity=entity,markers=markers,count=count,players=players,level=level,ticks=ticks,allocated=allocated,entities=ptrs,active=active,floor=rng.randrange(65536),fallback=C.c_int32(rng.getrandbits(32)).value)
 uc.mem_write(G,bytes(g));uc.mem_write(E,bytes(raw));uc.mem_write(L+16,struct.pack('<H',level));uc.mem_write(MARKERS,bytes(markers));uc.mem_write(0x03004014,struct.pack('<I',count));uc.mem_write(8,struct.pack('<i',context['fallback']))
 logs[0].clear();logs[1].clear();assert run(*args())==0 and not state.updating
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1);uc.emu_start(0x0801b855,STOP,count=100000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert logs[0]==logs[1],(elapsed,level,players,'logs',logs)
 assert gamebytes()==bytes(uc.mem_read(G,256)) and entitybytes()==bytes(uc.mem_read(E,116)),(elapsed,level,players)
 report['whole_respawn']+=1
 if any(x[0]=='particle' for x in logs[0]):
  report['particles']+=1;report['allocated' if allocated else 'exhausted']+=1
for elapsed in range(65536):compare(elapsed,level=12+elapsed%4,players=2)
for _ in range(4096):compare(rng.randrange(65536))
print('PASS whole respawn including elapsed wrap,marker/sequence wrap,occupied/exhausted cases',flush=True)
# Isolated complete05330 with up to100 original active entries.
for case in range(4096):
 count=rng.randrange(101);m=Marker(rng.randrange(-32768,32768),rng.randrange(-32768,32768),0,0)
 entities=(Entity*count)();ptrs=(EP*count)();slots=rng.sample(range(100),count)
 for i,e in enumerate(entities):
  e.body.x=C.c_int32(rng.getrandbits(32)).value;e.body.y=C.c_int32(rng.getrandbits(32)).value
  if i==case%100:e.body.x=C.c_int32((m.x<<16)|rng.randrange(65536)).value;e.body.y=C.c_int32((m.y<<16)|rng.randrange(65536)).value
  ptrs[i]=C.pointer(e);uc.mem_write(POOL+slots[i]*116,struct.pack('<ii',e.body.x,e.body.y))
 uc.mem_write(MARKERS,bytes(m));uc.mem_write(0x03004470,struct.pack('<'+'H'*count,*slots));uc.mem_write(0x03004354,struct.pack('<H',count))
 uc.reg_write(UC_ARM_REG_R0,MARKERS);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1);uc.emu_start(0x08005331,STOP,count=100000)
 assert occ(C.byref(m),ptrs,count)==uc.reg_read(UC_ARM_REG_R0);report['whole_occupancy']+=1
compare(300,force=True)
valid=args()
for i in (0,1,5,8):
 bad=valid.copy();bad[i]=None;assert run(*bad)==-1;report['guards']+=1
for i,value in [(2,256),(3,65536),(4,256),(6,0)]:
 bad=valid.copy();bad[i]=value;assert run(*bad)==-1;report['guards']+=1
for name,value in [('marker',context['count']),('sequence',1),('sequence',32),('updating',1)]:
 old=getattr(state,name);setattr(state,name,value);assert run(*valid)==-1;setattr(state,name,old);report['guards']+=1
for kind in ('occupied','allocate','animation','insert','particle'):
 compare(300,force=True);state.elapsed=300;context['fail']=kind
 assert run(*args())==-1 and not state.updating;report['failures']+=1
for size in (0,0x44d313):
 out=Data.from_buffer_copy(data);saved=bytes(out);assert lib.ssa_pickup_respawn_data_load(C.byref(out),ROM,size)==-1 and bytes(out)==saved;report['guards']+=1
assert occ(None,None,0)==-1 and occ(C.byref(Marker()),None,1)==-1;report['guards']+=2
report['rom_comparisons']=report['whole_respawn']+report['whole_occupancy']
report['source_sha256']=hashlib.sha256((ROOT/'src/pickup_respawn.c').read_bytes()).hexdigest()
(ROOT/'analysis/pickup-respawn-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
