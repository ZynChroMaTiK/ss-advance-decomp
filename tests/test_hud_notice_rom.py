"""Whole E914 (literal formatter boundary) and E9D4 -> FC04 -> E410 ROM comparisons."""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes();raw=C.create_string_buffer(rom)
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
sources=[r/'src'/f'{s}.c' for s in ('hud_notice','hud_sprites','weapon_animation','sprite_pool','sprite_dispatch')]
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'tests/test_hud_notice_rom.c'),
 *map(str,sources),'-o',str(b/'libssa_hud_notice.so')],check=True)
lib=C.CDLL(str(b/'libssa_hud_notice.so'));lib.start.argtypes=[C.c_void_p,C.c_size_t]
lib.set.argtypes=[*[C.c_void_p]*4,*[C.c_uint]*5];lib.step.argtypes=[*[C.c_void_p]*5,C.c_uint,C.c_uint]
lib.output.argtypes=[C.c_uint];lib.output.restype=C.c_void_p
assert lib.start(raw,len(rom))==1
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(rom)),(0x02000000,0x10000),(0x03000000,0x10000)):u.mem_map(at,size)
u.mem_write(0x08000000,rom)
GAME,OBJ,PLAYERS,SETTINGS,STOP=0x02000000,0x02001000,0x02002000,0x02003000,0x087ff000
X,Y,IMAGES=0x03006ab8,0x03006ae8,0x03006b18
report=dict(result='pass',rom_sha1=hashlib.sha1(rom).hexdigest(),scope=__doc__,sanitizers='UBSan',
 setter_cases=0,whole_controller_calls=0,sequences=0,sequence_frames=0,directed_cases=0,guards=0)
rng=random.Random(0xe9d4)
def put(at,fmt,*v):u.mem_write(at,struct.pack('<'+fmt,*v))
def read(at,size):return bytes(u.mem_read(at,size))
def hook(m,at,size,ctx):
 dest=m.reg_read(UC_ARM_REG_R0);p=m.reg_read(UC_ARM_REG_R1);text=bytearray()
 while (c:=read(p,1))!=b'\0':text+=c;p+=1
 assert b'%' not in text
 m.mem_write(dest,bytes(text)+b'\0');m.reg_write(UC_ARM_REG_R0,len(text));m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,hook,begin=0x080c56c8,end=0x080c56c8)
def call(at,args=()):
 u.reg_write(UC_ARM_REG_SP,0x03007e00);u.reg_write(UC_ARM_REG_LR,STOP|1)
 for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(reg,v)
 u.emu_start(at|1,STOP,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP,hex(u.reg_read(UC_ARM_REG_PC))
def initialize():
 u.mem_write(GAME,bytes(0x1000));u.mem_write(0x03000000,bytes(0x8000))
 for at,v in ((0x03004010,GAME),(0x030055a0,OBJ),(0x03004648,PLAYERS),(0x0300572c,SETTINGS)):put(at,'I',v)
 put(0x0300464e,'H',2);put(PLAYERS+188,'H',8);put(PLAYERS+2*256+188,'H',0)
def state():return (read(GAME,0x520),read(X,42),read(Y,42),read(IMAGES,84))
def compare(label,obj=False):
 for kind,(at,size) in enumerate(((GAME,0x520),(X,42),(Y,42),(IMAGES,84),(OBJ,48)) if obj else ((GAME,0x520),(X,42),(Y,42),(IMAGES,84))):
  expected=read(at,size);actual=C.string_at(lib.output(kind),size)
  assert actual==expected,(label,kind,[(i,a,e) for i,(a,e) in enumerate(zip(actual,expected)) if a!=e][:20])
def text(lang,index):
 t=struct.unpack_from('<I',rom,0x44cf34+4*lang)[0]-0x8000000
 p=struct.unpack_from('<I',rom,t+4*index)[0]-0x8000000
 return rom[p:rom.index(0,p)]
def setter(lang,index,value=0,y=48,font=0):
 before=state();assert lib.set(*before,lang,index,value,y,font)==1
 put(SETTINGS,'H',lang);call(0x0800e914,(index,value,y,font));compare(('set',lang,index));report['setter_cases']+=1
def controller(obj,ticks,local=0):
 before=state();assert lib.step(*before,obj,ticks,local)==1
 put(GAME+7,'B',ticks);put(PLAYERS+2*256+188,'H',local);u.mem_write(OBJ,obj)
 # GAME+7 is an explicit input, not a field changed by the controller.
 # Import it before the native call as well.
 call(0x0800e9d4);compare(('step',report['whole_controller_calls']),True)
 report['whole_controller_calls']+=1;return read(OBJ,48)
def object_for(index):
 o=bytearray(48);struct.pack_into('<HH',o,4,13,index);struct.pack_into('<HH',o,24,0x3fff,0x3fff)
 struct.pack_into('<IHH',o,28,1,0x5c,0);o[36]=5;struct.pack_into('<HH',o,40,2048,2048);o[46]=15;return bytes(o)
for lang in range(3):
 for index in range(252):
  s=text(lang,index)
  if b'%' in s or len(s)>=512 or sum(c not in (32,10) for c in s)>42:continue
  initialize();put(GAME+4,'H',rng.randrange(65536));u.mem_write(GAME+0x320,bytes([0xa5])*512)
  setter(lang,index,0x12345678,0x134,0x100+(index&1))
# Original readiness compares current image with the previous cache image. The
# transfer boundary is explicitly controlled here; FC04 and E410 execute whole.
for lang in range(3):
 for index in (0x8d,0x91,0x72,0xb5,0xb6,0xb7,0xb9,0xc4):
  initialize();setter(lang,index,font=lang&1);objects=[object_for(i) for i in range(42)]
  for frame in range(132):
   ticks=1;put(GAME+7,'B',ticks)
   for i,o in enumerate(objects):
    objects[i]=controller(o,ticks,8 if frame in (10,11) else 0)
   if frame>=2:
    for i,o in enumerate(objects):
     p=bytearray(o);p[24:26]=p[26:28];objects[i]=bytes(p)
   report['sequence_frames']+=1
  report['sequences']+=1
for case in range(2048):
 initialize();setter(case%3,0xb7 if case&1 else 0x91,y=rng.randrange(256),font=(0,1,2,7,31,32,255)[case%7])
 put(GAME+4,'H',rng.randrange(65536));put(GAME+0x76,'H',(0,1,2,120,32767,32768,65535)[case%7])
 put(GAME+0x74,'B',read(GAME+0x6a,1)[0] if case&1 else rng.randrange(256))
 ticks=(0,1,2,16,255)[case%5];put(GAME+7,'B',ticks)
 controller(object_for(case%42),ticks,8 if case%11==0 else 0);report['directed_cases']+=1
# Native safety boundaries reject unsupported formatted/oversized text, invalid
# subtype and ticks without changing the supplied field/image projections.
initialize();setter(0,0x91);before=state()
for args in ((3,0x91,0,48,0),(0,252,0,48,0),(0,98,0,48,0),(0,127,0,48,0)):
 assert lib.set(*before,*args)==-1
 assert C.string_at(lib.output(0),0x520)==before[0];report['guards']+=1
for obj,ticks in ((object_for(42),1),(object_for(0),256)):
 assert lib.step(*before,obj,ticks,0)==-1
 assert C.string_at(lib.output(0),0x520)==before[0];report['guards']+=1
inputs=sources+[r/'include/ssa/hud_notice.h',r/'include/ssa/weapon_animation.h',r/'include/ssa/hud_sprites.h',
 r/'tests/test_hud_notice_rom.c',Path(__file__)]
report['source_sha256']={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(r/'analysis/hud-notice-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='source_sha256'}))
