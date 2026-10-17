"""Whole extended controllers, controlled motion/create/relocation, actual trig."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
for filename in ('test_world_rom.py','test_projectile_controller_rom.py'):
 nodes=[]
 for n in ast.parse((ROOT/'tests'/filename).read_text()).body:
  if isinstance(n,ast.ClassDef):nodes.append(n)
  elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE','MOTION','EVENT','efields','pfields') for t in n.targets):nodes.append(n)
  elif isinstance(n,ast.FunctionDef) and n.name in ('setfield','getfield'):nodes.append(n)
 exec(compile(ast.Module(nodes,type_ignores=[]),filename,'exec'))
class Request(C.Structure):_fields_=[('type',C.c_uint32),('source',EP),('forward',C.c_int32),('side',C.c_int32),('z_offset',C.c_int32),('heading',C.c_uint32),('slope',C.c_int32)]
PP=C.POINTER(Projectile);CREATE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Request),C.POINTER(PP));FINISH=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Controller))
class ExtendedOps(C.Structure):_fields_=[('controller',Ops),('create',CREATE),('finish',FINISH),('relocate',MOTION),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/projectile_extended.c'),str(ROOT/'src/movement.c'),'-o',str(B/'libssa_projectile_extended.so')],check=True)
lib=C.CDLL(str(B/'libssa_projectile_extended.so'));step=lib.ssa_projectile_extended_step
step.argtypes=[C.POINTER(Controller),C.POINTER(Game),C.POINTER(Tables),C.c_uint,C.POINTER(ExtendedOps)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t];math=Tables();assert lib.ssa_math_tables_load(C.byref(math),ROM,len(ROM))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM);uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000);uc.mem_write(0x030000f0,ROM[0x370:0x380])
E,G,STOP=0x02000000,0x02008000,0x087ff000;uc.mem_write(0x03004010,struct.pack('<I',G))
addresses={0x64:0x08010d7c,0x68:0x08011350,0x6a:0x08011090,0xe3:0x08011424}
rng=random.Random(0x11350);current={};logs=[[],[]]
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'controllers':{},'created_children':0,'reentry_guards':0}
def put(a,fmt,*v):uc.mem_write(a,struct.pack('<'+fmt,*v))
def get(a,fmt):return struct.unpack('<'+fmt,uc.mem_read(a,struct.calcsize('<'+fmt)))[0]
def index(p):return next(i for i,p2 in enumerate(current['ps']) if C.addressof(p.contents)==C.addressof(p2))
def snapshot(i):
 out=bytearray(current['raws'][i]);p=current['ps'][i];e=current['es'][i]
 for obj,fields in ((e,efields),(p,pfields)):
  for field,off,fmt in fields:struct.pack_into('<'+fmt,out,off,getfield(obj,field))
 return bytes(out),current['game'].credit,current['game'].hit_weapon
def rsnapshot(i):return bytes(uc.mem_read(E+256*i,116)),get(0x03003f40,'I'),get(0x03003f4c,'I')
def nested():
 assert step(C.byref(current['controller']),C.byref(current['game']),C.byref(math),current['ticks'],C.byref(ops))==-1;report['reentry_guards']+=1
@MOTION
def motion(ctx,pp):
 i=index(pp);nested();logs[0].append(('motion',i,snapshot(i)));p=pp.contents
 p.timer=(p.timer+current['ticks'])&65535;p.entity.contents.body.x=C.c_int32(p.entity.contents.body.x+12345).value
 return current['moved'] if not i else current['child_moved']
@EVENT
def event(ctx,cc,kind,a,b,d):
 i=index(cc.contents.projectile);nested()
 if kind==0 and (a<=0x8d or a==65535):current['raws'][i][92]=current['raws'][i][93]=0
 logs[0].append((kind,i,(a,b,d),snapshot(i)))
 return 0
@CREATE
def create(ctx,req,out):
 nested();r=req.contents;parent=next(i for i,e in enumerate(current['es']) if C.addressof(e)==C.addressof(r.source.contents))
 logs[0].append(('create',parent,r.type,r.forward,r.side,r.z_offset,r.heading))
 current['nc']+=1;i=current['nc']
 if current['full'] and i%3==0:out[0]=PP();return 0
 p=current['ps'][i];p.facing=(r.heading>>8)&255;p.entity.contents.type=r.type;out[0]=C.pointer(p)
 return 1
@FINISH
def finish(ctx,cc):
 i=index(cc.contents.projectile)
 if cc.contents.destroy_requested:current['destroyed'].add(i)
 return 0
@MOTION
def relocate(ctx,pp):
 i=index(pp);nested();logs[0].append(('relocate',i,snapshot(i)));pp.contents.entity.contents.body.z=C.c_int32(pp.contents.entity.contents.body.z+37).value;return 0
ops=ExtendedOps(Ops(motion,event,None),create,finish,relocate,None)
def hook(u,a,size,ctx):
 r=[u.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
 if a==0x08010830:
  parent=(r[1]-E)//256;stack=u.reg_read(UC_ARM_REG_SP);z=get(stack,'i');heading=get(stack+4,'I')
  logs[1].append(('create',parent,r[0],C.c_int32(r[2]).value,C.c_int32(r[3]).value,z,heading));current['rc']+=1;i=current['rc']
  if current['full'] and i%3==0:u.reg_write(UC_ARM_REG_R0,0)
  else:
   put(E+256*i+60,'H',r[0]);put(E+256*i+63,'B',(heading>>8)&255);u.reg_write(UC_ARM_REG_R0,E+256*i)
 elif a==0x08010a20:
  i=(r[0]-E)//256;logs[1].append(('motion',i,rsnapshot(i)));put(r[0]+78,'H',(get(r[0]+78,'H')+current['ticks'])&65535);put(r[0],'i',C.c_int32(get(r[0],'i')+12345).value);u.reg_write(UC_ARM_REG_R0,current['moved'] if not i else current['child_moved'])
 elif a==0x080053d0:
  i=(r[0]-E)//256;logs[1].append(('relocate',i,rsnapshot(i)));put(r[0]+8,'i',C.c_int32(get(r[0]+8,'i')+37).value)
 else:
  kind={0x08005394:0,0x080035a8:1,0x08014b60:2,0x080037e0:3}[a]
  if kind==2:
   i=current['last'];args=(r[0],0,0)
   assert r[1:]==[get(E+256*i+o,'I') for o in (0,4,8)]
  else:
   i=(r[0]-E)//256;current['last']=i;args=tuple(r[1:]) if kind==1 else (r[1],r[2],0) if kind==0 else (0,0,0)
   if kind==0 and (r[1]<=0x8d or r[1]==65535):put(r[0]+88,'H',r[1]);put(r[0]+92,'BBBB',0,0,r[2],0)
  logs[1].append((kind,i,args,rsnapshot(i)))
  if kind==3:current['rdestroyed'].add(i)
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for a in (0x08010830,0x08010a20,0x080053d0,0x08005394,0x080035a8,0x08014b60,0x080037e0):uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for typ,address in addresses.items():
 for case in range(4096):
  raws=[bytearray(rng.randbytes(116)) for _ in range(17)];es=[];ps=[]
  for i,raw in enumerate(raws):
   struct.pack_into('<H',raw,60,typ);raw[62]=(case%256 if case<1024 else case%4) if not i else 0
   struct.pack_into('<H',raw,78,rng.choice([0,1,63,64,65,479,480,481,32767,32768,65535]) if not i else 0)
   if i:struct.pack_into('<H',raw,88,0);raw[95]=0
   e=Entity();p=Projectile()
   for obj,fields in ((e,efields),(p,pfields)):
    for field,off,fmt in fields:setfield(obj,field,struct.unpack_from('<'+fmt,raw,off)[0])
   p.entity=C.pointer(e);es.append(e);ps.append(p);uc.mem_write(E+256*i,bytes(raw))
  ticks=rng.choice([0,1,2,8,63,64,127,255]);g=Game(rng.getrandbits(32),rng.getrandbits(32));c=Controller(C.pointer(ps[0]),0,0)
  current.clear();current.update(raws=raws,es=es,ps=ps,ticks=ticks,game=g,controller=c,moved=case&1,child_moved=(case>>1)&1,full=case%5==0,nc=0,rc=0,last=0,destroyed=set(),rdestroyed=set())
  logs[0].clear();logs[1].clear();put(G+7,'B',ticks);put(0x03003f40,'I',g.credit);put(0x03003f4c,'I',g.hit_weapon)
  result=step(C.byref(c),C.byref(g),C.byref(math),ticks,C.byref(ops));assert result>=0
  for reg,v in ((UC_ARM_REG_R0,E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,v)
  uc.emu_start(address|1,STOP,count=30000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
  assert logs[0]==logs[1],(hex(typ),case,logs)
  if c.destroy_requested:current['destroyed'].add(0)
  assert current['destroyed']==current['rdestroyed']
  assert current['nc']==current['rc']
  for i in range(current['nc']+1):assert snapshot(i)==rsnapshot(i),(hex(typ),case,i,snapshot(i),rsnapshot(i))
  report['created_children']+=current['nc'];report['controllers'][hex(typ)]=report['controllers'].get(hex(typ),0)+1
report['scope']='Whole64/68/6A/E3 dispatch with original trig/fixed multiply; motion10A20,creation10830,relocation053D0,particles,explosions and destruction are controlled explicit boundaries. Full116-byte actors and ordered events compared, including immediatechild6A stepping and68spread.'
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/projectile_extended.c','include/ssa/projectile_extended.h','tests/test_projectile_extended_rom.py']}
(ROOT/'analysis/native-projectile-extended-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
