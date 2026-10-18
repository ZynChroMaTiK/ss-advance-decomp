"""Whole original1965C calls, including real19458 timing and193D8 motion.

Input polling, image transfer, text layout/line count and tile rendering are
controlled presentation boundaries. This does not establish outer197B0
screen/fade/dispatch parity or a playable campaign ending.
"""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/campaign_story.c'),'-o',str(b/'libssa_campaign_story.so')],check=True)
names=('counter','wait','index','stage','x','y','vx','vy','display_image','image','scroll','rate','display_text','text','speed')
class Story(C.Structure):
 _fields_=[*[(n,C.c_uint32) for n in names],('positions',(C.c_int16*2)*4),('fade_target',C.c_uint16),
  ('mode',C.c_uint8),('music',C.c_uint8),('records',(C.c_uint32*5)*8),('loaded',C.c_uint),('updating',C.c_uint)]
CALL=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Story),C.c_int,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32)
class Ops(C.Structure):_fields_=[('event',CALL),('context',C.c_void_p)]
lib=C.CDLL(str(b/'libssa_campaign_story.so'));init=lib.ssa_campaign_story_init;step=lib.ssa_campaign_story_step
init.argtypes=[C.POINTER(Story),C.c_void_p,C.c_size_t,C.c_uint,C.c_uint];step.argtypes=[C.POINTER(Story),C.c_uint32,C.POINTER(Ops)]
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(rom)),(0x02000000,0x10000),(0x03000000,0x10000)):u.mem_map(at,size)
u.mem_write(0x08000000,rom)
BASE,SETTINGS,SPRITES,TILES,IMAGES,STOP=0x02000000,0x02000100,0x02001000,0x02002000,0x02003000,0x087ff000
report={'result':'pass','rom_sha1':hashlib.sha1(rom).hexdigest(),'scope':__doc__,'sanitizers':'UBSan',
 'whole_calls':0,'sequences':0,'sequence_frames':0,'directed_calls':0,'events':0,'row_events':0,'guards':0,'callback_reentry_checks':0,'callback_failures':0}
oracle=[];actual=[];current_input=0;line_count=48;fail_kind=None;failed_prefix=None
def put(at,fmt,value):u.mem_write(at,struct.pack('<'+fmt,value))
def get(at,fmt='I'):return struct.unpack('<'+fmt,u.mem_read(at,struct.calcsize('<'+fmt)))[0]
def state(s=None):
 if s is not None:return (*[getattr(s,n) for n in names],tuple(tuple(p) for p in s.positions),s.fade_target,s.mode)
 return (*struct.unpack('<15I',u.mem_read(BASE,60)),tuple(struct.unpack('<hh',u.mem_read(SPRITES+i*20,4)) for i in range(4)),get(BASE+0x4a,'H'),get(SETTINGS+0x19,'B'))
def emit(kind,args):oracle.append((kind,args,state()))
def hook(uc,pc,size,ctx):
 regs=[uc.reg_read(x) for x in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
 if pc==0x08003028:put(0x030042f0,'I',current_input)
 elif pc==0x08008eb0:emit(0,(regs[0],0,0,0))
 elif pc==0x08008e74:emit(1,(regs[0],0,0,0))
 elif pc==0x08018ee4:emit(2,(((regs[0]-IMAGES)&0xffffffff)//16384,0,0,0));assert regs[1]==TILES
 elif pc==0x08018c84:emit(3,(regs[0],0,0,0));assert regs[1:]==[0,0x06009400,12]
 elif pc==0x08018dfc:emit(4,(regs[0],0,0,0));assert regs[1]==0;uc.reg_write(UC_ARM_REG_R0,line_count)
 elif pc==0x080c556c:
  a=C.c_int32(regs[0]).value;bv=C.c_int32(regs[1]).value;q=abs(a)//abs(bv)*(-1 if (a<0)!=(bv<0) else 1)
  uc.reg_write(UC_ARM_REG_R0,(a-q*bv)&0xffffffff)
 elif pc==0x08018d70:
  offset=C.c_int32((regs[2]-0x06009400)&0xffffffff).value
  emit(5,(regs[0],get(uc.reg_read(UC_ARM_REG_SP)+4)&65535,(offset//768)&0xffffffff,regs[3]))
  assert regs[1]==0 and get(uc.reg_read(UC_ARM_REG_SP))==21 and get(uc.reg_read(UC_ARM_REG_SP)+8)==0
 else:raise AssertionError(hex(pc))
 uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
for at in (0x08003028,0x08008eb0,0x08008e74,0x08018ee4,0x08018c84,0x08018dfc,0x080c556c,0x08018d70):u.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
@CALL
def event(ctx,p,kind,a,bv,c,d):
 global failed_prefix
 s=p.contents;actual.append((kind,(a,bv,c,d),state(s)))
 before=bytes(s)
 assert step(p,0,C.byref(ops))==-1 and bytes(s)==before
 report['callback_reentry_checks']+=1
 if kind==fail_kind:failed_prefix=state(s);return -1
 return line_count if kind==4 else 1
ops=Ops(event,None)
def original(s,keys):
 global current_input,oracle
 current_input=keys;oracle=[]
 u.mem_write(BASE,struct.pack('<15I',*[getattr(s,n) for n in names])+bytes(20))
 for i,p in enumerate(s.positions):u.mem_write(SPRITES+20*i,struct.pack('<hh',*p))
 put(BASE+0x3c,'I',SPRITES);put(BASE+0x40,'I',TILES);put(BASE+0x44,'I',IMAGES);put(BASE+0x4a,'H',s.fade_target)
 put(SETTINGS+0x19,'B',s.mode);put(SETTINGS+0x2c,'B',s.music);put(0x0300572c,'I',SETTINGS)
 u.reg_write(UC_ARM_REG_SP,0x03007e00);u.reg_write(UC_ARM_REG_R0,BASE);u.reg_write(UC_ARM_REG_LR,STOP|1)
 u.emu_start(0x0801965d,STOP,count=10000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP
 return u.reg_read(UC_ARM_REG_R0),state(),list(oracle)
def compare(s,keys):
 global actual
 expected=original(s,keys);actual=[]
 result=step(C.byref(s),keys,C.byref(ops))
 assert (result,state(s),actual)==expected,{'index':s.index,'stage':s.stage,'keys':hex(keys),'native':(result,state(s),actual),'rom':expected}
 report['whole_calls']+=1;report['events']+=len(actual);report['row_events']+=sum(k==5 for k,_,_ in actual)
def new(level=0,music=1):
 s=Story();assert init(C.byref(s),rom,len(rom),level,music)==1;return s
for level in (0,6,12):
 for music in (0,1):
  for policy in range(4):
   s=new(level,music);line_count=48+level
   for frame in range(15000):
    keys=0 if policy==0 else (0x80 if frame%120<80 else 0x40) if policy==1 else 0x800000 if policy==2 else (0x10000 if frame==135 else 0)
    compare(s,keys);report['sequence_frames']+=1
    if s.mode:break
   else:raise AssertionError(('did not finish',level,policy,state(s)))
   assert s.mode==1 and (policy==3 or s.stage==6)
   s.mode=2
   for _ in range(64):compare(s,0x10000|0x80)
   report['sequences']+=1
rng=random.Random(0x1965c)
for _ in range(8192):
 s=new(rng.choice((0,6,12)),rng.randrange(2));s.index=rng.choice((0,1,2,4,6));s.stage=rng.randrange(7)
 for n in names:
  if n not in ('index','stage'):setattr(s,n,rng.getrandbits(32))
 s.display_image=rng.randrange(5);s.image=rng.randrange(5);s.display_text=rng.randrange(197,202);s.text=rng.randrange(197,202)
 s.mode=rng.randrange(3);s.fade_target=rng.randrange(65536);line_count=rng.randrange(200)
 compare(s,rng.choice((0,0x40,0x80,0x800000,0x800040,0x800080,0x10000,0x100c0)))
 report['directed_calls']+=1
for index in (0,1,2,4,6):
 for stage in (1,5):
  for value in (0x00700000,0x00680000,0x0067ffff,0x00100000,0x000fffff,0xff800000,0xff7fffff,0x00f00000,0x009fffff,0x00a00000):
   s=new();s.index=index;s.stage=stage;s.x=s.y=value;compare(s,0);report['directed_calls']+=1
for rows in (0,1,20,21,22,48,200):
 for scroll in (0,0x7ffff,0x80000,0x7fffffff,0x80000000,0xffff0000):
  s=new();s.stage=3;s.speed=0x4000;s.scroll=scroll;line_count=rows;compare(s,0x80);report['directed_calls']+=1
for kind in range(6):
 s=new();s.stage=3;s.speed=0x4000;s.scroll=0x00060000;s.display_text=s.text=197
 if kind<2:keys=0x10000
 elif kind==2:keys=0
 elif kind==3:s.display_image=s.image=0;s.display_text=0;keys=0
 else:s.display_image=s.image=0;keys=0
 line_count=48;actual=[];fail_kind=kind;failed_prefix=None
 assert step(C.byref(s),keys,C.byref(ops))==-1 and not s.updating and failed_prefix==state(s),(kind,state(s),actual)
 report['callback_failures']+=1
fail_kind=None
for level,music,size in ((1,1,len(rom)),(5,1,len(rom)),(11,1,len(rom)),(13,1,len(rom)),(0,2,len(rom)),(0,1,len(rom)-1)):
 s=new();before=bytes(s);assert init(C.byref(s),rom,size,level,music)==-1 and bytes(s)==before;report['guards']+=1
for field,value in (('loaded',0),('updating',1),('index',8),('stage',7),('mode',3),('music',2)):
 s=new();setattr(s,field,value);before=bytes(s);assert step(C.byref(s),0,C.byref(ops))==-1 and bytes(s)==before;report['guards']+=1
for field,offset,value in (('entry',0,3),('exit',4,3),('image',12,5),('text',16,252),('delay',8,0),('sentinel',3*20+8,1)):
 bad=bytearray(rom);struct.pack_into('<I',bad,0x44cfc4+offset,value);s=new();before=bytes(s)
 assert init(C.byref(s),bytes(bad),len(bad),0,1)==-1 and bytes(s)==before,field;report['guards']+=1
s=new();before=bytes(s);assert step(C.byref(s),0,None)==-1 and bytes(s)==before;report['guards']+=1
inputs=['src/campaign_story.c','include/ssa/campaign_story.h','tests/test_campaign_story_rom.py']
report['source_sha256']={p:hashlib.sha256((r/p).read_bytes()).hexdigest() for p in inputs}
(r/'analysis/campaign-story-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
