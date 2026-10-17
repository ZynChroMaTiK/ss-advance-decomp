"""Whole14B60,14C58,9D4C with original setters and controlled allocation/audio."""
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
i32,u32,u16,u8=C.c_int32,C.c_uint32,C.c_uint16,C.c_uint8
for n in ast.parse((ROOT/'tests/test_projectile_create_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef) and n.name in ('Projectile','Creator'):exec(compile(ast.Module([n],type_ignores=[]),'projectile declarations','exec'))
PP=C.POINTER(Projectile)
class Rules(C.Structure):_fields_=[('type',u16*16),('animation',u16*16),('period',u16*16),('loaded',C.c_uint)]
ALLOC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(PP));ONE=C.CFUNCTYPE(C.c_int,C.c_void_p,PP);SOUND=C.CFUNCTYPE(C.c_int,C.c_void_p,PP,C.c_uint)
class Ops(C.Structure):_fields_=[('allocate',ALLOC),('animation',ONE),('insert',ONE),('sound',SOUND),('context',C.c_void_p)]
class Impact(C.Structure):_fields_=[('random_state',u32),('updating',C.c_uint)]
PART=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,i32,i32,i32)
class ImpactOps(C.Structure):_fields_=[('particle',PART),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(exist_ok=True,parents=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/particle_create.c'),'-o',str(B/'libssa_particle.so')],check=True)
lib=C.CDLL(str(B/'libssa_particle.so'));load=lib.ssa_particle_rules_load;load.argtypes=[C.POINTER(Rules),C.c_void_p,C.c_size_t]
create=lib.ssa_particle_create;create.argtypes=[C.POINTER(Creator),C.POINTER(Rules),C.c_uint,i32,i32,i32,C.POINTER(Ops),C.POINTER(PP)]
step=lib.ssa_particle_step;step.argtypes=[PP]
impact=lib.ssa_weapon_impact;impact.argtypes=[C.POINTER(Impact),EP,C.c_uint,u16,C.c_int,i32,i32,i32,C.POINTER(ImpactOps)]
rules=Rules();assert load(C.byref(rules),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
E,S,G,PLAYERS,STACK,STOP=0x02000000,0x02001000,0x02002000,0x02003000,0x03007e00,0x087ff000
uc.mem_write(0x03004760,struct.pack('<I',S));uc.mem_write(0x03004010,struct.pack('<I',G));uc.mem_write(0x03004648,struct.pack('<I',PLAYERS))
rng=random.Random(0x14b60);creator=Creator();output=PP();current={};logs=[[],[]]
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'particle_creation':0,'particle_controller':0,'impact_selection':0,'guards':0,'callback_failures':0,'reentry_guards':0}
def snapshot(p):
 d=bytearray(current['raw']);e=p.entity.contents
 for off,v in ((0,e.body.x),(4,e.body.y),(8,e.body.z)):struct.pack_into('<i',d,off,v)
 struct.pack_into('<H',d,48,e.body.flags);struct.pack_into('<H',d,60,e.type);d[62]=p.state
 struct.pack_into('<H',d,88,p.animation);d[94]=p.animation_period;d[95]=p.animation_done
 if current.get('animation'):d[92]=d[93]=0
 return bytes(d)
def nested():
 assert create(C.byref(creator),C.byref(rules),0,0,0,0,C.byref(ops),C.byref(output))==-1
 report['reentry_guards']+=1
def allocate(ctx,out):
 nested();logs[0].append(('allocate',));out[0]=C.pointer(current['p']) if current['allocated'] else PP()
 return -1 if current.get('fail')=='allocate' else 0
def animation(ctx,p):
 nested();current['animation']=True;logs[0].append(('animation',snapshot(p.contents)))
 return -1 if current.get('fail')=='animation' else 0
def insert(ctx,p):
 nested();logs[0].append(('insert',snapshot(p.contents)));return -1 if current.get('fail')=='insert' else 0
def sound(ctx,p,id):
 nested();logs[0].append(('sound',id,snapshot(p.contents)));return -1 if current.get('fail')=='sound' else 0
callbacks=[ALLOC(allocate),ONE(animation),ONE(insert),SOUND(sound)];ops=Ops(*callbacks,None)
def particle(ctx,id,x,y,z):
 logs[0].append(('particle',id,x,y,z));assert impact(C.byref(current['impact']),C.byref(current['source']),0,1,1,x,y,z,C.byref(iops))==-1
 report['reentry_guards']+=1
 return -1 if current.get('fail') else 0
pc=PART(particle);iops=ImpactOps(pc,None)
def hook(u,a,size,ctx):
 if a==0x08005170:logs[1].append(('allocate',));u.reg_write(UC_ARM_REG_R0,E if current['allocated'] else 0)
 elif a==0x080053c4:logs[1].append(('animation',bytes(u.mem_read(E,116))));return
 elif a==0x0800a5d0:logs[1].append(('insert',bytes(u.mem_read(E,116))))
 elif a==0x08008d5c:logs[1].append(('sound',u.reg_read(UC_ARM_REG_R0),bytes(u.mem_read(E,116))))
 elif a==0x080037e0:logs[1].append(('destroy',u.reg_read(UC_ARM_REG_R0)))
 elif a==0x08014b60:
  if not current.get('impactmode'):return
  values=[u.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
  logs[1].append(('particle',values[0],*[C.c_int32(v).value for v in values[1:]]))
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for address in (0x08005170,0x080053c4,0x0800a5d0,0x08008d5c,0x080037e0,0x08014b60):uc.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def run(address,args):
 for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(reg,v&0xffffffff)
 uc.reg_write(UC_ARM_REG_SP,STACK);uc.reg_write(UC_ARM_REG_LR,STOP|1);uc.emu_start(address|1,STOP,count=10000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
for i in range(4096):
 raw=bytearray(rng.randbytes(116));e=Entity();e.body.flags=struct.unpack_from('<H',raw,48)[0];e.type=struct.unpack_from('<H',raw,60)[0]
 for off,n in ((0,'x'),(4,'y'),(8,'z')):setattr(e.body,n,struct.unpack_from('<i',raw,off)[0])
 p=Projectile();p.entity=C.pointer(e);p.state=raw[62];p.animation=struct.unpack_from('<H',raw,88)[0];p.animation_period=raw[94];p.animation_done=raw[95]
 current.clear();current.update(raw=raw,p=p,allocated=i%19!=0);logs[0].clear();logs[1].clear();xyz=[C.c_int32(rng.getrandbits(32)).value for _ in range(3)]
 result=create(C.byref(creator),C.byref(rules),i%16,*xyz,C.byref(ops),C.byref(output));assert result==int(current['allocated'])
 uc.mem_write(E,bytes(raw));run(0x08014b60,[i%16,*xyz]);assert logs[0]==logs[1],(i,logs)
 assert snapshot(p)==bytes(uc.mem_read(E,116)),i
 assert uc.reg_read(UC_ARM_REG_R0)==(E if current['allocated'] else 0);report['particle_creation']+=1
for done in range(256):
 p.entity.contents.type=0x96;p.animation_done=done;uc.mem_write(E+95,bytes([done]));logs[1].clear();run(0x08014c58,[E])
 assert step(C.byref(p))==bool(done);assert logs[1]==([('destroy',E)] if done else []);report['particle_controller']+=1
for i in range(8192):
 source=Entity();source.body.flags=rng.choice([0,16,0x4110,65535]);source.player_index=rng.randrange(4)
 state=Impact(rng.getrandbits(32),0);idx=rng.randrange(4);weapon=rng.randrange(1,11);target=i&1
 xyz=[C.c_int32(rng.getrandbits(32)).value for _ in range(3)];current.clear();current.update(impact=state,source=source,impactmode=True)
 uc.mem_write(S+48,struct.pack('<H',source.body.flags));uc.mem_write(S+84,bytes([source.player_index]));uc.mem_write(G+104,bytes([idx]));uc.mem_write(PLAYERS+256*source.player_index+120,struct.pack('<H',weapon));uc.mem_write(0x030041bc,struct.pack('<I',state.random_state))
 logs[0].clear();logs[1].clear();assert impact(C.byref(state),C.byref(source),idx,weapon,target,*xyz,C.byref(iops))==0
 run(0x08009d4c,[*xyz,target]);assert logs[0]==logs[1],(i,logs);assert state.random_state==struct.unpack('<I',uc.mem_read(0x030041bc,4))[0];report['impact_selection']+=1
# Invalid pointers, out-of-table IDs, reentry and each failed callback retain prefix.
for id in (16,255,0xffffffff):assert create(C.byref(creator),C.byref(rules),id,0,0,0,C.byref(ops),C.byref(output))==-1;report['guards']+=1
assert load(C.byref(rules),ROM,len(ROM))==-1;report['guards']+=1
for size in (0,0x44c13b):
 r=Rules();assert load(C.byref(r),ROM,size)==-1 and not r.loaded;report['guards']+=1
for failure in ('allocate','animation','insert','sound'):
 e=Entity();p=Projectile();p.entity=C.pointer(e);current.clear();current.update(raw=bytes(116),p=p,allocated=True,fail=failure)
 assert create(C.byref(creator),C.byref(rules),0,1,2,3,C.byref(ops),C.byref(output))==-1
 assert not creator.updating and not p.updating and not (e.body.flags&32);report['callback_failures']+=1
files=['include/ssa/particle_create.h','src/particle_create.c','tests/test_particle_create_rom.py']
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files}
(ROOT/'analysis/native-particle-create-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

