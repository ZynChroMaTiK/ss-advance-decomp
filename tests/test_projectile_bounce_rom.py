"""Full4718 including468C and all wall rays on16maps, actual math/geometry."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['projectile_bounce.c','enemy_ray.c','ray_intercepts.c','ray_traversal.c','slide.c','geometry.c','map_assets.c','movement.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(B/'libssa_projectile_bounce.so')],check=True)
nodes=[n for n in ast.parse((ROOT/'tests/test_slide_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name=='Map']
nodes += [n for n in ast.parse((ROOT/'tests/test_enemy_ray_rom.py').read_text()).body if isinstance(n,ast.ClassDef)]
nodes += [n for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name in ('Body','Tables')]
exec(compile(ast.Module(nodes,type_ignores=[]),'ctypes declarations','exec'))
class Bounce(C.Structure):_fields_=[('ray',EnemyRay),('fraction',C.c_int32),('previous_fraction',C.c_int32),('line',C.c_uint),('previous_line',C.c_uint),('body',C.POINTER(Body)),('updating',C.c_uint)]
lib=C.CDLL(str(B/'libssa_projectile_bounce.so'))
for name,args in {
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
 'ssa_map_assets_free':[C.POINTER(Map)],
 'ssa_math_tables_load':[C.POINTER(Tables),C.c_void_p,C.c_size_t],
 'ssa_projectile_bounce_init':[C.POINTER(Bounce),C.POINTER(Map)],
 'ssa_projectile_bounce_free':[C.POINTER(Bounce)],
 'ssa_projectile_bounce':[C.POINTER(Bounce),C.POINTER(Body),C.c_int32,C.POINTER(Tables),C.c_uint8],
}.items():getattr(lib,name).argtypes=args
tables=Tables();assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000);uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]);uc.mem_write(0x030003a4,ROM[0x430:0x638])
BASE,SCRATCH,E,G,STOP=0x02000000,0x02040000,0x02070000,0x02071000,0x087ff000
OPEN=(0x03004910,0x030048f0,0x030054c0,0x030048f4)
rng=random.Random(0x4718)
def put(a,fmt,*v):uc.mem_write(a,struct.pack('<'+fmt,*v))
def get(a,fmt='i'):return struct.unpack('<'+fmt,uc.mem_read(a,struct.calcsize('<'+fmt)))[0]
def si(n):return C.c_int32(n).value
put(0x03004010,'I',G)
def bios(machine,address,size,ctx):
 a,b=[si(machine.reg_read(r)) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1)]
 assert b
 q=abs(a)//abs(b)*(-1 if (a<0)!=(b<0) else 1)
 machine.reg_write(UC_ARM_REG_R0,q&0xffffffff);machine.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff)
 machine.reg_write(UC_ARM_REG_R3,abs(q));machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,bios,begin=0x08002c88,end=0x08002c88)
ray_calls=[0];predicate_calls=[0];maxhits=[0]
def observe(machine,address,size,ctx):
 if address==0x0800a240:ray_calls[0]+=1
 elif address==0x0800468c:predicate_calls[0]+=1
 else:
  count=(get(0x030054dc,'I')-0x03004770)//12
  assert count<=32,('original intercept buffer overflow',count)
  maxhits[0]=max(maxhits[0],count)
for a in (0x0800a240,0x0800468c,0x08009c00):uc.hook_add(UC_HOOK_CODE,observe,begin=a,end=a)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'maps':16,'rom_comparisons':0,'bounced':0,'clear':0,'guards':0,'scope':__doc__+'; BIOS integer division service replaced only; original ray and predicate code executed'}
for level in range(16):
 m=Map();b=Bounce();assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
 assert lib.ssa_projectile_bounce_init(C.byref(b),C.byref(m))==1
 data=bytearray(C.string_at(m.data,m.size));original=bytearray(data)
 for off in range(0x1c,0x48,4):struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
 uc.mem_write(BASE,bytes(original));uc.mem_write(SCRATCH,bytes(m.line_count*4))
 ox,oy,width,height=struct.unpack_from('<hhhh',data,m.blockmap)
 put(0x03004930,'I',BASE);put(0x03004944,'iihh',ox*65536,oy*65536,width,height);put(0x03004954,'I',SCRATCH)
 vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]))
 for i in range(1024):
  line=rng.randrange(m.line_count);l=data[m.lines+40*line:m.lines+40*(line+1)]
  va,vb=struct.unpack_from('<HH',l,0);vx,vy=vertices[va];wx,wy=vertices[vb]
  body=Body();body.x=si((vx+wx)*32768+rng.randrange(-20,21)*65536);body.y=si((vy+wy)*32768+rng.randrange(-20,21)*65536)
  radius=rng.choice([0,1,8,16,32,64])*65536
  ticks=rng.choice([0,1,2,4,8,127,255])
  maxspeed=32 if ticks<8 else 2
  body.velocity_x=rng.randrange(-maxspeed*65536,maxspeed*65536);body.velocity_y=rng.randrange(-maxspeed*65536,maxspeed*65536)
  if i%7==0:body.velocity_x=0
  if i%11==0:body.velocity_y=0
  body.z=rng.choice([-65536,0,1,16*65536,64*65536,128*65536,0x7fffffff,-2147483648])
  body.height=rng.choice([0,1,16*65536,32*65536,128*65536,-65536,0x7fffffff])
  raw=bytearray(rng.randbytes(116))
  for n,o in [('x',0),('y',4),('z',8),('velocity_x',12),('velocity_y',16),('height',44)]:struct.pack_into('<i',raw,o,getattr(body,n))
  struct.pack_into('<i',raw,40,radius);uc.mem_write(E,bytes(raw));put(G+7,'B',ticks)
  for (name,_),addr in zip(Opening._fields_,OPEN):
   v=si(rng.getrandbits(32));setattr(b.ray.opening,name,v);put(addr,'i',v)
  b.fraction=si(rng.getrandbits(32));b.previous_fraction=si(rng.getrandbits(32));b.line=rng.randrange(m.line_count);b.previous_line=rng.randrange(m.line_count)
  put(0x03004460,'i',b.fraction);put(0x03004340,'i',b.previous_fraction)
  put(0x03004614,'I',BASE+m.lines+b.line*40);put(0x0300445c,'I',BASE+m.lines+b.previous_line*40)
  b.ray.traversal.blocking_fraction=si(rng.getrandbits(32));put(0x0300505c,'i',b.ray.traversal.blocking_fraction)
  expectedcalls=ray_calls[0]+3
  uc.reg_write(UC_ARM_REG_R0,E);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
  uc.emu_start(0x08004719,STOP,count=1000000)
  assert uc.reg_read(UC_ARM_REG_PC)==STOP
  expected=uc.reg_read(UC_ARM_REG_R0)
  actual=lib.ssa_projectile_bounce(C.byref(b),C.byref(body),radius,C.byref(tables),ticks)
  assert actual==expected,(level,i,actual,expected)
  struct.pack_into('<ii',raw,12,body.velocity_x,body.velocity_y)
  assert bytes(raw)==bytes(uc.mem_read(E,116)),(level,i,ticks,body.velocity_x,body.velocity_y,get(E+12),get(E+16))
  assert ray_calls[0]==expectedcalls
  assert b.fraction==get(0x03004460) and b.previous_fraction==get(0x03004340)
  assert BASE+m.lines+b.line*40==get(0x03004614,'I') and BASE+m.lines+b.previous_line*40==get(0x0300445c,'I')
  assert [getattr(b.ray.opening,n) for n,_ in Opening._fields_]==[get(a) for a in OPEN]
  assert b.ray.traversal.blocking_fraction==get(0x0300505c)
  assert not b.updating and not b.body
  report['rom_comparisons']+=1;report['bounced' if actual else 'clear']+=1
 for obj in (b,b.ray,b.ray.traversal,b.ray.walls):
  obj.updating=1;assert lib.ssa_projectile_bounce(C.byref(b),C.byref(body),0,C.byref(tables),1)==-1
  assert lib.ssa_projectile_bounce_free(C.byref(b))==-1
  obj.updating=0;report['guards']+=2
 assert lib.ssa_projectile_bounce_init(C.byref(b),C.byref(m))==-1;report['guards']+=1
 assert lib.ssa_projectile_bounce_free(C.byref(b))==1;lib.ssa_map_assets_free(C.byref(m))
 print('PASS map',level,'1024 whole projectile bounce cases',flush=True)
report.update(ray_calls=ray_calls[0],predicate_calls=predicate_calls[0],maximum_intercepts=maxhits[0])
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/projectile-bounce-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
