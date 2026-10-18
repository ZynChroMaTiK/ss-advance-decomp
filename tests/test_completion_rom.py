"""Original02564 single-player tally loop, preserving real E914 notice writes.

World capture, sprite animation/draw, VBlank, input and audio are controlled
boundaries; post-tally screen teardown/commit is outside this comparison.
"""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R7,UC_ARM_REG_R9,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes();raw=C.create_string_buffer(rom)
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/completion.c'),
 '-o',str(b/'libssa_completion.so')],check=True)
class Completion(C.Structure):
 _fields_=[*[(n,C.c_uint32) for n in ('counter','score','pending','elapsed','par_seconds')],('ammo',C.c_int16*8),
 ('flags',C.c_uint16),('notice_timer',C.c_uint16),('level',C.c_uint16),('difficulty',C.c_uint8),('bonus_y',C.c_uint8),
 *[(n,C.c_uint32) for n in ('stage','hold','delay','flash')],('blend',C.c_uint16),('blend_table',C.c_uint16*30),
 *[(n,C.c_uint) for n in ('loaded','running','updating')],('source_score',C.c_uint32),('source_counter',C.c_uint32),('recorded',C.c_uint)]
CALL=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Completion),C.c_int,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32)
class Ops(C.Structure):_fields_=[('event',CALL),('context',C.c_void_p)]
lib=C.CDLL(str(b/'libssa_completion.so'));init=lib.ssa_completion_init;step=lib.ssa_completion_step
init.argtypes=[C.POINTER(Completion),C.c_void_p,C.c_size_t,C.c_uint,C.c_uint,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint16,C.POINTER(C.c_int16)]
step.argtypes=[C.POINTER(Completion),C.c_uint32,C.c_uint32,C.c_uint,C.POINTER(Ops)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for address,size in ((0x08000000,len(rom)),(0x02000000,0x10000),(0x03000000,0x10000),(0x04000000,0x1000)):
 uc.mem_map(address,size)
uc.mem_write(0x08000000,rom)
GAME,PLAYER,SETTINGS,STOP=0x02000000,0x02001000,0x02002000,0x087ff000
report={'result':'pass','rom_sha1':hashlib.sha1(rom).hexdigest(),'scope':__doc__,'sanitizers':'UBSan',
 'sequence_frames':0,'sequences':0,'directed_frames':0,'callback_reentry_guards':0,'guards':0,'callback_failures':0}
oracle=[];actual=[];skip_entry=False;current_input=0;phase='setup';stack=0;fail_event=None
def put(at,fmt,value):uc.mem_write(at,struct.pack('<'+fmt,value))
def get(at,fmt='I'):return struct.unpack('<'+fmt,uc.mem_read(at,struct.calcsize('<'+fmt)))[0]
def state(native):
 if native:return tuple(getattr(native,n) for n in ('counter','score','pending','flags','notice_timer','bonus_y','stage','hold','delay','flash','running'))
 return (get(GAME),get(GAME+0x88),get(GAME+0x8c),get(GAME+4,'H'),get(GAME+0x76,'H'),get(GAME+0x75,'B'),
 uc.reg_read(UC_ARM_REG_R9),uc.reg_read(UC_ARM_REG_R7),get(stack+8),get(stack),get(stack+4))
def hook(u,pc,size,ctx):
 global skip_entry,stack
 if pc==0x08002626:
  stack=u.reg_read(UC_ARM_REG_SP)
  if skip_entry:skip_entry=False
  else:u.emu_stop()
  return
 if pc==0x08002a2e:u.emu_stop();return
 if pc==0x0800e914:
  oracle.append((0,tuple(u.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)),state(None)))
  return # Execute the original notice setter, controlling only its formatter.
 if pc==0x080c56c8:
  dest=u.reg_read(UC_ARM_REG_R0);at=u.reg_read(UC_ARM_REG_R1);text=bytearray()
  while (byte:=u.mem_read(at,1))!=b'\0':text+=byte;at+=1
  assert b'%' not in text
  u.mem_write(dest,bytes(text)+b'\0');u.reg_write(UC_ARM_REG_R0,len(text))
 elif pc==0x08003028:put(0x030042f4,'I',current_input)
 elif pc==0x080c532c:put(0x030000e0,'I',2) # Finish initial snapshot fade; no tally timing is skipped.
 elif pc in (0x08008eb0,0x08008d5c):
  if stack:oracle.append((1 if pc==0x08008eb0 else 2,(u.reg_read(UC_ARM_REG_R0),0,0,0),state(None)))
 elif pc not in (0x0800fb5c,0x0800f85c,0x0800fce4,0x0800fba0,0x0801063c,0x0800e460,0x0800dbdc):
  raise AssertionError(hex(pc))
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for at in (0x08002626,0x08002a2e,0x0800e914,0x080c56c8,0x08003028,0x080c532c,
 0x08008eb0,0x08008d5c,0x0800fb5c,0x0800f85c,0x0800fce4,0x0800fba0,0x0801063c,0x0800e460,0x0800dbdc):
 uc.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
@CALL
def event(ctx,p,kind,a,bv,c,d):
 actual.append((kind,(a,bv,c,d),state(p.contents)))
 before=bytes(p.contents)
 assert step(p,0,0,0,C.byref(ops))==-1 and bytes(p.contents)==before
 report['callback_reentry_guards']+=1
 return -1 if fail_event==kind else 0
ops=Ops(event,None)
def setup(level,difficulty,elapsed,ammo,counter=0,score=0,flags=0):
 global skip_entry,current_input
 uc.mem_write(0x02000000,bytes(0x10000));uc.mem_write(0x03000000,bytes(0x10000))
 put(0x03004010,'I',GAME);put(0x03004648,'I',PLAYER);put(0x0300572c,'I',SETTINGS)
 put(0x030042f0+0x14,'H',3);put(GAME+0x10,'B',1);put(GAME,'I',counter)
 put(GAME+4,'H',flags);put(GAME+12,'I',elapsed);put(GAME+0x88,'I',score)
 put(SETTINGS+0x10,'H',level);put(SETTINGS+0x1a,'B',difficulty)
 uc.mem_write(PLAYER+0x28+0x12,struct.pack('<8h',*ammo))
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 skip_entry=False;current_input=0;uc.emu_start(0x08002565,STOP,count=20000)
 assert uc.reg_read(UC_ARM_REG_PC)==0x08002626
 s=Completion();assert init(C.byref(s),raw,len(rom),level,difficulty,counter,score,elapsed,flags,(C.c_int16*8)(*ammo))==1
 assert state(s)==state(None),(state(s),state(None))
 oracle.clear();actual.clear();return s
def compare(s,held,pressed,voice):
 global skip_entry,current_input
 oracle.clear();actual.clear();current_input=(held&1)|((pressed&1)<<16)
 put(0x03006994,'I',0xffffffff if voice else 0)
 skip_entry=True;uc.emu_start(0x08002627,STOP,count=20000)
 result=step(C.byref(s),held,pressed,voice,C.byref(ops))
 assert result==int(s.running) and result>=0
 assert oracle==actual,('events',oracle,actual)
 assert state(s)==state(None),('state',state(s),state(None))
 assert s.blend==get(0x04000052,'H')
 assert tuple(s.ammo)==struct.unpack('<8h',uc.mem_read(PLAYER+0x28+0x12,16))
 assert s.elapsed==get(GAME+12)
 return result
rng=random.Random(0x2564)
# Every campaign map/difficulty, real sequential state execution and all original
# par values. Inputs cover normal/accelerated tally, held wait skipping, late
# audio completion and an early pressed dismissal distinct from held fire.
for level in range(12):
 for difficulty in range(3):
  par=struct.unpack_from('<I',rom,0x44a610+4*level)[0]
  elapsed=60*(par-(level%4+1))+(level*7)%60
  if level%4==3:elapsed=60*(par+1)
  ammo=[0 if level%3==0 else 1+(i+level)%5 for i in range(8)]
  s=setup(level,difficulty,elapsed,ammo,0xfffffff0,0xfffffff0,0x200)
  for frame in range(5000):
   held=int(frame%4==0);pressed=int(s.stage==6 and frame%7==0)
   if not compare(s,held,pressed,int(frame>200)):
    report['sequence_frames']+=frame+1;report['sequences']+=1;break
  else:raise AssertionError(('sequence timeout',level,difficulty,state(s)))
  before=bytes(s);assert step(C.byref(s),0,0,1,C.byref(ops))==0 and bytes(s)==before
# One-frame comparisons from explicit projected loop state cover arithmetic
# edge cases unreachable with ordinary map limits without huge test durations.
edges=[0,1,99,100,101,999,1000,1001,0x7fffffff,0x80000000,0xfffffffe,0xffffffff]
for case in range(4096):
 level=case%12;difficulty=(case//12)%3
 s=setup(level,difficulty,rng.randrange(2**32),[rng.randrange(-32768,32768) for _ in range(8)],rng.randrange(2**32),rng.randrange(2**32),rng.randrange(65536))
 s.stage=case%7;s.hold=(0,1,2,59,60,61)[case%6];s.delay=edges[(case//6)%len(edges)]
 s.pending=edges[(case//72)%len(edges)];s.flash=case%30;s.notice_timer=case%65536;s.bonus_y=case%256
 uc.reg_write(UC_ARM_REG_R9,s.stage);uc.reg_write(UC_ARM_REG_R7,s.hold)
 put(stack+8,'I',s.delay);put(stack,'I',s.flash);put(GAME+0x8c,'I',s.pending)
 put(GAME+0x76,'H',s.notice_timer);put(GAME+0x75,'B',s.bonus_y)
 compare(s,case&1,(case>>1)&1,(case>>2)&1);report['directed_frames']+=1
# API failures preserve the entire state before update; callback errors retain
# original prefix effects and clear the update guard.
s=setup(0,1,0,[10]*8)
for bad in ('loaded','updating','stage','flash'):
 saved=getattr(s,bad);setattr(s,bad,{'loaded':0,'updating':1,'stage':7,'flash':30}[bad]);before=bytes(s)
 assert step(C.byref(s),0,0,0,C.byref(ops))==-1 and bytes(s)==before
 setattr(s,bad,saved);report['guards']+=1
for args in ((None,0,0,0,C.byref(ops)),(C.byref(s),0,0,2,C.byref(ops)),(C.byref(s),0,0,0,None)):
 before=bytes(s);assert step(*args)==-1 and bytes(s)==before;report['guards']+=1
for missing in ('out','rom','size','level','difficulty','ammo'):
 args=[C.byref(s),raw,len(rom),0,1,0,0,0,0,(C.c_int16*8)(*([0]*8))]
 index,value={'out':(0,None),'rom':(1,None),'size':(2,len(rom)-1),'level':(3,12),'difficulty':(4,3),'ammo':(9,None)}[missing]
 args[index]=value;before=bytes(s)
 assert init(*args)==-1 and bytes(s)==before;report['guards']+=1
for kind,stage in ((0,0),(1,1),(2,6)):
 s=setup(0,1,0,[10]*8);s.stage=stage;s.hold=60 if stage==6 else 0;s.counter=7;s.pending=2000
 fail_event=kind;assert step(C.byref(s),0,0,0,C.byref(ops))==-1 and not s.updating
 assert s.counter==8;report['callback_failures']+=1
fail_event=None
inputs=[Path(__file__),r/'src/completion.c',r/'include/ssa/completion.h']
report['source_sha256']={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(r/'analysis/completion-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
