"""Whole10830/109F0; actual trig/fixed math, allocation/insertion boundaries."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef):nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets):nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world declarations','exec'))
i32,u32,u16,u8=C.c_int32,C.c_uint32,C.c_uint16,C.c_uint8
class Projectile(C.Structure):
 _fields_=[('entity',EP),('source',EP),('speed',i32),('health',C.c_int16),('timer',u16),('parameter34',u16),('animation',u16),('state',u8),('facing',u8),('owner',u8),('weapon',u8),('animation_done',u8),('animation_period',u8),('updating',C.c_uint)]
PP=C.POINTER(Projectile)
class Creator(C.Structure):_fields_=[('updating',C.c_uint)]
class Request(C.Structure):_fields_=[('type',u32),('source',EP),('forward',i32),('side',i32),('z_offset',i32),('heading',u32),('slope',i32)]
ALLOC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(PP))
INSERT=C.CFUNCTYPE(C.c_int,C.c_void_p,PP)
class Ops(C.Structure):_fields_=[('allocate',ALLOC),('insert',INSERT),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/projectile_create.c'),str(ROOT/'src/movement.c'),'-o',str(B/'libssa_projectile_create.so')],check=True)
lib=C.CDLL(str(B/'libssa_projectile_create.so'))
basic,aimed=lib.ssa_projectile_create,lib.ssa_projectile_create_aimed
for f in (basic,aimed):f.argtypes=[C.POINTER(Creator),C.POINTER(Request),C.POINTER(Tables),C.POINTER(Ops),C.POINTER(PP)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
tables=Tables();assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
S,E,OTHER,STOP,STACK=0x02000000,0x02001000,0x02002000,0x087ff000,0x03007e00
rng=random.Random(0x10830)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'creation_cases':0,'aimed_cases':0,'allocation_exhaustion':0,'guards':0,'callback_failures':0,'callback_reentry_guards':0,'scope':'whole10830/109F0 actual ROM sincos and fixed multiply; allocation and insertion controlled with full116-byte snapshots; every16bit type and heading, player/nonplayer velocity, ordering and pool exhaustion'}
def rd(d,o,f='I'):return struct.unpack_from('<'+f,d,o)[0]
def wr(d,o,v,f='I'):struct.pack_into('<'+f,d,o,v&({'I':0xffffffff,'H':65535,'B':255}[f]))
bodyfields=[('x',0,'i'),('y',4,'i'),('z',8,'i'),('velocity_x',12,'i'),('velocity_y',16,'i'),('velocity_z',20,'i'),('height',44,'i'),('flags',48,'H'),('hover_offset',67,'b'),('step_adjust',83,'b'),('ceiling',112,'h'),('floor',114,'h')]
def entity(raw):
 e=Entity()
 for n,o,f in bodyfields:setattr(e.body,n,rd(raw,o,f))
 e.radius=rd(raw,40,'i');e.type=rd(raw,60,'H');e.player_index=raw[84]
 return e
creator=Creator();output=PP();current={};logs=[[],[]]
def snapshot(p,raw):
 data=bytearray(raw);e=p.entity.contents
 for n,o,f in bodyfields:
  struct.pack_into('<'+f,data,o,getattr(e.body,n))
 wr(data,40,e.radius);wr(data,60,e.type,'H');wr(data,84,p.owner,'B')
 wr(data,36,p.speed);wr(data,62,p.state,'B');wr(data,63,p.facing,'B');wr(data,78,p.timer,'H')
 wr(data,32,S if bool(p.source) and C.addressof(p.source.contents)==C.addressof(current['source']) else OTHER)
 return bytes(data)
def nested():
 for f in (basic,aimed):
  assert f(C.byref(creator),C.byref(current['request']),C.byref(tables),C.byref(ops),C.byref(output))==-1
  report['callback_reentry_guards']+=1
def allocate(ctx,out):
 nested();logs[0].append(('allocate',))
 if current.get('allocation_error'):return -1
 out[0]=C.pointer(current['projectile']) if current['allocated'] else PP()
 return 0
def insert(ctx,pp):
 nested();p=pp.contents
 assert p.updating==1
 logs[0].append(('insert',snapshot(p,current['raw'])))
 p.entity.contents.body.floor=current['floor'];p.entity.contents.body.ceiling=current['ceiling']
 return -1 if current.get('insert_error') else 0
ac,ic=ALLOC(allocate),INSERT(insert);ops=Ops(ac,ic,None)
def hook(machine,address,size,ctx):
 if address==0x08005170:
  logs[1].append(('allocate',));result=E if current['allocated'] else 0
 else:
  assert machine.reg_read(UC_ARM_REG_R0)==E
  logs[1].append(('insert',bytes(machine.mem_read(E,116))))
  machine.mem_write(E+112,struct.pack('<hh',current['ceiling'],current['floor']));result=0
 machine.reg_write(UC_ARM_REG_R0,result);machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook,begin=0x08005170,end=0x08005170)
uc.hook_add(UC_HOOK_CODE,hook,begin=0x0800a5d0,end=0x0800a5d0)
edges=[-2147483648,-2147483647,-65536,-1,0,1,65536,2147483646,2147483647]
def compare(ty,heading,is_aimed,allocated=True):
 raw=bytearray(rng.randbytes(116));src=bytearray(rng.randbytes(116));wr(raw,32,OTHER)
 wr(src,48,rng.choice([0,0x10,0x104,0x4110,65535]),'H')
 source=entity(src);e=entity(raw);p=Projectile(C.pointer(e),C.pointer(source),rd(raw,36,'i'),rd(raw,50,'h'),rd(raw,78,'H'),rd(raw,52,'H'),0,raw[62],raw[63],raw[84],raw[85],0,0,0)
 dummy=Entity();p.source=C.pointer(dummy)
 r=Request(ty,C.pointer(source),rng.choice(edges) if ty%5==0 else C.c_int32(rng.getrandbits(32)).value,C.c_int32(rng.getrandbits(32)).value,C.c_int32(rng.getrandbits(32)).value,heading,rng.choice(edges) if ty%3==0 else C.c_int32(rng.getrandbits(32)).value)
 current.clear();current.update(raw=raw,source=source,projectile=p,request=r,allocated=allocated,floor=rng.randrange(-32768,32768),ceiling=rng.randrange(-32768,32768))
 logs[0].clear();logs[1].clear();uc.mem_write(S,bytes(src));uc.mem_write(E,bytes(raw))
 f=aimed if is_aimed else basic
 assert f(C.byref(creator),C.byref(r),C.byref(tables),C.byref(ops),C.byref(output))==(1 if allocated else 0)
 assert not creator.updating and not p.updating
 assert bool(output)==allocated
 for reg,value in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR],[ty,S,r.forward,r.side,STACK,STOP|1]):uc.reg_write(reg,value&0xffffffff)
 uc.mem_write(STACK,struct.pack('<III',r.z_offset&0xffffffff,heading,r.slope&0xffffffff))
 uc.emu_start(0x080109f1 if is_aimed else 0x08010831,STOP,count=10000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert uc.reg_read(UC_ARM_REG_R0)==(E if allocated else 0)
 assert logs[0]==logs[1],(ty,heading,is_aimed,logs)
 expected=snapshot(p,raw) if allocated else bytes(raw)
 actual=bytes(uc.mem_read(E,116))
 assert expected==actual,(ty,heading,is_aimed,[(i,x,y) for i,(x,y) in enumerate(zip(expected,actual)) if x!=y])
 assert bytes(src)==bytes(uc.mem_read(S,116))
 report['aimed_cases' if is_aimed else 'creation_cases']+=1
 if not allocated:report['allocation_exhaustion']+=1
for i in range(65536):
 compare(i,(i*40503)&65535,bool(i&1))
 if i and i%16384==0:print('PASS creation types/headings',i,flush=True)
for i in range(4096):compare(rng.choice([0x66,0x67,0x68,0x69,0x6a,0x1006a,0xffffffff]),rng.getrandbits(32),bool(i&1),i%16!=0)
print('PASS full ROM creation and aimed wrappers',flush=True)
# Native invalid inputs, callback failure cleanup and exhausted allocation.
validargs=[C.byref(creator),C.byref(current['request']),C.byref(tables),C.byref(ops),C.byref(output)]
for i in range(5):
 args=validargs.copy();args[i]=None;assert basic(*args)==-1;report['guards']+=1
creator.updating=1;assert basic(*validargs)==-1;creator.updating=0;report['guards']+=1
current['request'].source=EP();assert basic(*validargs)==-1;current['request'].source=C.pointer(current['source']);report['guards']+=1
for name in ('allocate','insert'):
 old=getattr(ops,name);setattr(ops,name,ALLOC() if name=='allocate' else INSERT());assert basic(*validargs)==-1
 setattr(ops,name,ac if name=='allocate' else ic);report['guards']+=1
current['allocated']=True;current['allocation_error']=True
assert basic(*validargs)==-1 and not creator.updating and not output;report['callback_failures']+=1
current['allocation_error']=False;current['insert_error']=True
assert aimed(*validargs)==-1 and not creator.updating and output and not output.contents.updating;report['callback_failures']+=1
current['insert_error']=False
p=current['projectile'];p.updating=1;assert basic(*validargs)==-1 and not creator.updating;p.updating=0;report['guards']+=1
saved=p.entity;p.entity=EP();assert basic(*validargs)==-1 and not creator.updating;p.entity=saved;report['guards']+=1
saved=p.entity;p.entity=current['request'].source;assert basic(*validargs)==-1 and not creator.updating;p.entity=saved;report['guards']+=1
report['total_rom_comparisons']=report['creation_cases']+report['aimed_cases']
report['source_sha256']={f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in ('src/projectile_create.c','include/ssa/projectile_create.h','include/ssa/projectile.h')}
(ROOT/'analysis/projectile-create-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
