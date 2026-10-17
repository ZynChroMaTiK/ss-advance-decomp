"""Whole 10A20 with original trig/multiply; collision/effect boundaries controlled."""
from pathlib import Path
import ast, ctypes as C, hashlib, json, random, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'ctypes world','exec'))
class Projectile(C.Structure):
 _fields_=[('entity',EP),('source',EP),('speed',C.c_int32),('health',C.c_int16)]+[(n,C.c_uint16) for n in ('timer','parameter34','animation')]+[(n,C.c_uint8) for n in ('state','facing','owner','weapon','animation_done','animation_period')]+[('updating',C.c_uint)]
PP=C.POINTER(Projectile)
class Game(C.Structure):
 _fields_=[('players',EP*4),('headings',C.c_uint16*4),('credit',C.c_uint32),('hit_weapon',C.c_uint32),('ticks',C.c_uint8),('player_count',C.c_uint8)]
MOVE=C.CFUNCTYPE(C.c_int,C.c_void_p,PP,C.c_int32,C.c_int32)
SIMPLE=C.CFUNCTYPE(C.c_int,C.c_void_p,PP)
BLOCK=C.CFUNCTYPE(C.c_int,C.c_void_p,PP,C.POINTER(EP))
HEALTH=C.CFUNCTYPE(C.c_int,C.c_void_p,EP,C.POINTER(C.c_int16))
DAMAGE=C.CFUNCTYPE(C.c_int,C.c_void_p,PP,EP,C.c_uint32)
FREEZE=C.CFUNCTYPE(C.c_int,C.c_void_p,PP,EP)
class Ops(C.Structure):
 _fields_=[('move',MOVE),('vertical',SIMPLE),('blocker',BLOCK),('health',HEALTH),('damage',DAMAGE),('freeze',FREEZE),('unlink',SIMPLE),('insert',SIMPLE),('bounce',SIMPLE),('context',C.c_void_p)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/projectile_motion.c'),str(ROOT/'src/movement.c'),'-o',str(BUILD/'libssa_projectile_motion.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_projectile_motion.so'))
lib.ssa_projectile_motion.argtypes=[PP,C.POINTER(Game),C.POINTER(Tables),C.POINTER(Ops)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
E,T,A,PLAYERS,OWNER,G,STOP=0x02070000,0x02070100,0x02070200,0x02071000,0x02072000,0x02074000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack(fmt,*v))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
put(0x03004010,'<I',G); put(0x03004648,'<I',PLAYERS)
for i in range(4): put(PLAYERS+256*i+0xac,'<I',OWNER+i*256)
def run():
 for r,v in ((UC_ARM_REG_R0,E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)): uc.reg_write(r,v)
 uc.emu_start(0x08010a21,STOP,count=50000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 return C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
def signed(v): return C.c_int32(v).value
def hook(machine,address,size,ctx):
 a,b,c=[machine.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)]
 value=0
 if address==0x080043d0:
  oracle.append(('move',signed(b),signed(c)))
  if moved: put(E,'<ii',signed(b),signed(c))
  value=moved
 elif address==0x08004ce0:
  oracle.append(('vertical',)); put(E+8,'<i',signed(get(E+8,'<I')+dz)); value=vertical
 elif address==0x0800400c: oracle.append(('blocker',)); value=T if blocked else 0
 elif address==0x080037f4:
  oracle.append(('damage',b,get(0x03003f40,'<I'),get(0x03003f4c,'<I')))
  put(T+0x32,'<H',(get(T+0x32,'<H')-b)&65535)
 elif address==0x08005170: oracle.append(('freeze',)); value=A if allocate else 0
 elif address==0x08005394: put(A+0x58,'<H',b); put(A+0x5e,'<B',c)
 elif address==0x0800a5d0:
  if a==E: oracle.append(('insert',))
  else: assert a==A
 elif address==0x0800a65c: oracle.append(('unlink',))
 elif address==0x08004718:
  oracle.append(('bounce',)); put(E+12,'<i',signed(-get(E+12,'<i'))); value=bounced
 machine.reg_write(UC_ARM_REG_R0,value&0xffffffff); machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for address in (0x080043d0,0x08004ce0,0x0800400c,0x080037f4,0x08005170,0x08005394,0x0800a5d0,0x0800a65c,0x08004718): uc.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'comparisons':0,'reentry':0,'guards':0,'callback_failures':0,'branches':{},'scope':'whole 10A20; original trig and fixed multiply; move/vertical/blocker/damage/allocation/animation/insertion/bounce controlled; no runtime emulation'}
e,target=Entity(),Entity(); owners=(Entity*4)(); p=Projectile(); p.entity=C.pointer(e); g=Game(); g.player_count=4
for i in range(4): g.players[i]=C.pointer(owners[i])
fail=''
def enter(kind):
 assert lib.ssa_projectile_motion(C.byref(p),C.byref(g),C.byref(tables),C.byref(ops))==-1
 report['reentry']+=1
 return -1 if kind==fail else 0
@MOVE
def move(ctx,pp,x,y):
 actual.append(('move',x,y))
 if enter('move')<0:return -1
 if moved:e.body.x=x;e.body.y=y
 return moved
@SIMPLE
def vert(ctx,pp):
 actual.append(('vertical',))
 if enter('vertical')<0:return -1
 e.body.z=signed(e.body.z+dz);return vertical
@BLOCK
def blocker(ctx,pp,out):
 actual.append(('blocker',))
 if enter('blocker')<0:return -1
 out[0]=C.pointer(target) if blocked else EP();return 0
@HEALTH
def health(ctx,entity,out):
 if enter('health')<0:return -1
 out[0]=target_health.value;return 0
@DAMAGE
def damage(ctx,pp,entity,amount):
 actual.append(('damage',amount,g.credit,g.hit_weapon))
 if enter('damage')<0:return -1
 target_health.value-=amount;return 0
@FREEZE
def freeze(ctx,pp,entity):
 global target_parameter
 actual.append(('freeze',))
 if enter('freeze')<0:return -1
 target_parameter=30
 if allocate:
  struct.pack_into('<H',native_alloc,0x3c,0xe2); native_alloc[0x3e]=0
  struct.pack_into('<I',native_alloc,0x20,T)
  struct.pack_into('<iiiiii',native_alloc,0,target.body.x,target.body.y,signed(target.body.z+target.body.height+0x100000),0,0,0)
  struct.pack_into('<H',native_alloc,0x30,struct.unpack_from('<H',native_alloc,0x30)[0]|0x20)
  struct.pack_into('<H',native_alloc,0x58,0x5b); native_alloc[0x5e]=5
 return 0
@SIMPLE
def unlink(ctx,pp):actual.append(('unlink',));return enter('unlink')
@SIMPLE
def insert(ctx,pp):actual.append(('insert',));return enter('insert')
@SIMPLE
def bounce(ctx,pp):
 actual.append(('bounce',))
 if enter('bounce')<0:return -1
 e.body.velocity_x=signed(-e.body.velocity_x);return bounced
ops=Ops(move,vert,blocker,health,damage,freeze,unlink,insert,bounce,None)
fields=[('x',0,'i'),('y',4,'i'),('z',8,'i'),('velocity_x',12,'i'),('velocity_y',16,'i'),('velocity_z',20,'i'),('height',44,'i'),('flags',48,'H')]
pfields=[('speed',0x24,'i'),('health',0x32,'h'),('parameter34',0x34,'H'),('state',0x3e,'B'),('facing',0x3f,'B'),('timer',0x4e,'H'),('owner',0x54,'B'),('weapon',0x55,'B'),('animation',0x58,'H'),('animation_done',0x5f,'B'),('animation_period',0x5e,'B')]
def load(raw,body):
 for n,o,f in fields:setattr(body,n,struct.unpack_from('<'+f,raw,o)[0])
def project():
 result=bytearray(raw)
 for n,o,f in fields:struct.pack_into('<'+f,result,o,getattr(e.body,n))
 for n,o,f in pfields:struct.pack_into('<'+f,result,o,getattr(p,n))
 return bytes(result)
rng=random.Random(0x10a20)
for i in range(16384):
 raw=bytearray(rng.randbytes(0x74)); traw=bytearray(rng.randbytes(0x74)); araw=bytearray(rng.randbytes(0x74))
 typ=(0x65,0x67,0x68,0x69,0x6a,0x6b,0x100)[i%7]
 struct.pack_into('<H',raw,0x3c,typ); raw[0x54]=i%5
 raw[0x30]|=4; raw[0x31]=(raw[0x31]&~0x40)|(0x40 if (i//5)%2 else 0)
 timer=(-32768,-2,-1,0,1,24,25,26,479,480,481,32767)[(i//7)%12]
 if i%3:struct.pack_into('<h',raw,0x4e,timer)
 if i%4:struct.pack_into('<i',raw,0x24,(0,0x100000,-0x100000,0x7fffffff)[(i//12)%4])
 ttyp=(0xcc,0xcd,0xd6,0xd7,0xd8,0xe1,0x68)[(i//3)%7];struct.pack_into('<H',traw,0x3c,ttyp)
 if i%2:traw[0x31]&=~4
 load(raw,e.body);load(traw,target.body);e.type=typ;target.type=ttyp
 for n,o,f in pfields:setattr(p,n,struct.unpack_from('<'+f,raw,o)[0])
 target_health=C.c_int16(struct.unpack_from('<h',traw,0x32)[0]);target_parameter=struct.unpack_from('<H',traw,0x34)[0]
 for j in range(4):
  coords=[rng.randrange(-2**31,2**31) for _ in range(3)]; heading=rng.randrange(65536)
  owners[j].body.x,owners[j].body.y,owners[j].body.z=coords;g.headings[j]=heading
  put(OWNER+j*256,'<iii',*coords);put(PLAYERS+j*256+0xb8,'<H',heading)
 g.ticks=(0,1,2,7,255)[(i//11)%5];g.credit=rng.randrange(2**32);g.hit_weapon=rng.randrange(2**32)
 put(G+7,'<B',g.ticks);put(0x03003f40,'<I',g.credit);put(0x03003f4c,'<I',g.hit_weapon)
 moved=(i//2)%2; vertical=(i//4)%2;blocked=(i//8)%2;allocate=(i//16)%2;bounced=(i//32)%2;dz=rng.randrange(-2**31,2**31)
 uc.mem_write(E,bytes(raw));uc.mem_write(T,bytes(traw));uc.mem_write(A,bytes(araw));native_alloc=bytearray(araw)
 oracle=[];actual=[];expected=run()
 result=lib.ssa_projectile_motion(C.byref(p),C.byref(g),C.byref(tables),C.byref(ops))
 assert result==expected,(i,typ,result,expected)
 assert project()==bytes(uc.mem_read(E,0x74)),(i,typ,[(j,a,b) for j,(a,b) in enumerate(zip(project(),uc.mem_read(E,0x74))) if a!=b])
 struct.pack_into('<HhH',traw,0x30,target.body.flags,target_health.value,target_parameter)
 assert bytes(traw)==bytes(uc.mem_read(T,0x74)),(i,'target')
 assert bytes(native_alloc)==bytes(uc.mem_read(A,0x74)),(i,'allocation')
 assert actual==oracle,(i,actual,oracle)
 assert (g.credit,g.hit_weapon)==(get(0x03003f40,'<I'),get(0x03003f4c,'<I'))
 assert not p.updating
 report['comparisons']+=1
 for event in actual:report['branches'][event[0]]=report['branches'].get(event[0],0)+1
print('PASS 16,384 whole projectile-motion ROM comparisons',flush=True)
# Explicit callback failure and guard paths; preserve lock release and68credit cleanup.
g.ticks=1;p.owner=1;p.timer=0;e.body.flags=0;p.speed=1;e.type=0x67;target.type=0xcd
for fail in ('move','vertical','blocker','health','damage','freeze','unlink','insert','bounce'):
 p.timer=0;p.health=100;moved=1 if fail=='vertical' else 0;vertical=0;blocked=0 if fail in ('unlink','insert','bounce') else 1
 e.type=0x69 if fail=='freeze' else (0x68 if fail=='damage' else 0x67);target.body.flags=0
 actual=[]
 assert lib.ssa_projectile_motion(C.byref(p),C.byref(g),C.byref(tables),C.byref(ops))==-1,fail
 assert not p.updating
 if fail=='damage':assert g.credit==0 and g.hit_weapon==0
 report['callback_failures']+=1
fail=''
for name in ('move','vertical','blocker','health','damage','freeze','unlink','insert','bounce'):
 old=getattr(ops,name);setattr(ops,name,type(old)())
 assert lib.ssa_projectile_motion(C.byref(p),C.byref(g),C.byref(tables),C.byref(ops))==-1
 setattr(ops,name,old);report['guards']+=1
p.updating=1;assert lib.ssa_projectile_motion(C.byref(p),C.byref(g),C.byref(tables),C.byref(ops))==-1;p.updating=0;report['guards']+=1
p.owner=5;assert lib.ssa_projectile_motion(C.byref(p),C.byref(g),C.byref(tables),C.byref(ops))==-1;p.owner=1;report['guards']+=1
report['source_sha256']=hashlib.sha256((ROOT/'src/projectile_motion.c').read_bytes()).hexdigest()
(ROOT/'analysis/projectile-motion-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
