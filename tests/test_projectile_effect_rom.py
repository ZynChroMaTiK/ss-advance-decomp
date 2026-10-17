"""Complete113E4 comparisons; world unlink/insert and release are controlled."""
from pathlib import Path
import ctypes as C, hashlib, json, random, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
exec((ROOT/'tests/test_projectile_motion_rom.py').read_text().split('BUILD=')[0])
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
 str(ROOT/'src/projectile_effect.c'),'-o',str(BUILD/'libssa_projectile_effect.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_projectile_effect.so'))
lib.ssa_projectile_effect_follow.argtypes=[PP,C.POINTER(Ops)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000);uc.mem_map(0x03000000,0x10000)
E,T,STOP=0x02070000,0x02070100,0x087ff000
oracle=[];actual=[];failure='';reentry=0
def snapshot():return struct.unpack('<iii',uc.mem_read(E,12))
def hook(machine,address,size,ctx):
 assert machine.reg_read(UC_ARM_REG_R0)==E
 oracle.append(({0x0800a65c:'unlink',0x0800a5d0:'insert',0x080037e0:'destroy'}[address],snapshot()))
 machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for address in (0x0800a65c,0x0800a5d0,0x080037e0):uc.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
e,target=Entity(),Entity();p=Projectile();p.entity=C.pointer(e);p.source=C.pointer(target);e.type=0xe2
def emit(kind):
 global reentry
 actual.append((kind,(e.body.x,e.body.y,e.body.z)))
 assert lib.ssa_projectile_effect_follow(C.byref(p),C.byref(ops))==-1;reentry+=1
 return -1 if kind==failure else 0
@SIMPLE
def unlink(ctx,pp):return emit('unlink')
@SIMPLE
def insert(ctx,pp):return emit('insert')
ops=Ops();ops.unlink=unlink;ops.insert=insert
rng=random.Random(0x113e4)
for i in range(8192):
 raw=bytearray(rng.randbytes(0x74));traw=bytearray(rng.randbytes(0x74))
 struct.pack_into('<I',raw,0x20,T);struct.pack_into('<H',raw,0x3c,0xe2)
 for entity,data in ((e,raw),(target,traw)):
  for name,offset,fmt in [('x',0,'i'),('y',4,'i'),('z',8,'i'),('velocity_x',12,'i'),
   ('velocity_y',16,'i'),('velocity_z',20,'i'),('height',44,'i'),('flags',48,'H')]:
   setattr(entity.body,name,struct.unpack_from('<'+fmt,data,offset)[0])
 uc.mem_write(E,bytes(raw));uc.mem_write(T,bytes(traw));oracle=[];actual=[]
 for reg,value in ((UC_ARM_REG_R0,E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,value)
 uc.emu_start(0x080113e5,STOP,count=1000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
 result=lib.ssa_projectile_effect_follow(C.byref(p),C.byref(ops))
 assert result==(0 if target.body.flags&0x20 else 1)
 if result==0:actual.append(('destroy',(e.body.x,e.body.y,e.body.z)))
 assert actual==oracle,(i,actual,oracle)
 struct.pack_into('<iii',raw,0,e.body.x,e.body.y,e.body.z)
 assert raw==uc.mem_read(E,0x74)
 assert not p.updating
guards=0
for badp,bado in ((None,C.byref(ops)),(C.byref(p),None)):
 assert lib.ssa_projectile_effect_follow(badp,bado)==-1;guards+=1
for attr,value in [('updating',1),('source',EP()),('source',p.entity)]:
 old=getattr(p,attr)
 # ctypes pointer field reads alias storage; preserve the original target pointer explicitly.
 setattr(p,attr,value);assert lib.ssa_projectile_effect_follow(C.byref(p),C.byref(ops))==-1
 setattr(p,attr,C.pointer(target) if attr=='source' else old);guards+=1
e.type=0x69;assert lib.ssa_projectile_effect_follow(C.byref(p),C.byref(ops))==-1;guards+=1;e.type=0xe2
target.body.flags=0
for failure in ('unlink','insert'):
 e.body.x=e.body.y=e.body.z=0;target.body.x=123;target.body.y=456;target.body.z=789;target.body.height=100
 actual=[];assert lib.ssa_projectile_effect_follow(C.byref(p),C.byref(ops))==-1
 assert (e.body.x==0) if failure=='unlink' else (e.body.x==123 and e.body.z==789+100+0x300000)
 assert not p.updating
report=dict(comparisons=8192,guards=guards,reentry=reentry,callback_failures=2,
 rom_sha1=hashlib.sha1(ROM).hexdigest(),source_sha256=hashlib.sha256((ROOT/'src/projectile_effect.c').read_bytes()).hexdigest(),
 scope='Whole113E4; original unlink/insert/release boundaries controlled. Native destruction deferred; full entity bytes and ordered callback positions compared. UBSan.')
(ROOT/'analysis/projectile-effect-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
