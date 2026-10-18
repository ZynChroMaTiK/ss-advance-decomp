"""Original021E4 single-player non-network pause control, ordered command/notice requests."""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes();b=r/'build/native-tests'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/pause.c'),str(r/'src/mode_control.c'),'-o',str(b/'pause.so')],check=True)
class State(C.Structure):
 _fields_=[('flags',C.c_uint16),('timer',C.c_uint16),('input_state',C.c_uint16),('selection',C.c_uint8),('events',C.c_uint*8),('event_count',C.c_uint)]
lib=C.CDLL(str(b/'pause.so'));step=lib.ssa_pause_step;step.argtypes=[C.POINTER(State),C.c_uint32]
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in [(0x8000000,len(rom)),(0x2000000,0x10000),(0x3000000,0x10000)]:u.mem_map(at,size)
u.mem_write(0x8000000,rom);GAME=0x2000000;PLAYER=0x2002000;STOP=0x87ff000;events=[]
def put(at,fmt,value):u.mem_write(at,struct.pack('<'+fmt,value))
def get(at,fmt):return struct.unpack('<'+fmt,u.mem_read(at,struct.calcsize(fmt)))[0]
def hook(u,pc,size,ctx):
 if pc==STOP:u.emu_stop();return
 value=u.reg_read(UC_ARM_REG_R0)
 if pc==0x800e914:
  events.append(256+value);put(GAME+0x76,'H',120);put(GAME+4,'H',get(GAME+4,'H')&~0x200)
 else:events.append(value)
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for at in (STOP,0x800e914,0x8008eb0):u.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
player_global=struct.unpack_from('<I',rom,0x224c)[0];game_global=struct.unpack_from('<I',rom,0x2250)[0]
rng=random.Random(0x21e4);n=0
for case in range(8192):
 s=State();s.flags=rng.randrange(65536)&~0x900;s.timer=rng.randrange(65536);s.input_state=0;s.selection=case%4
 input=rng.getrandbits(32) if case>=256 else sum(bit for i,bit in enumerate((0x10000,0x80000,0x400000,0x800000)) if (case//4>>i)&1)
 u.mem_write(GAME,bytes(0x400));u.mem_write(PLAYER,bytes(0x100));put(player_global,'I',PLAYER);put(game_global,'I',GAME)
 put(GAME+0x10,'B',1);put(GAME+4,'H',s.flags);put(GAME+0x72,'B',s.selection);put(GAME+0x76,'H',s.timer)
 put(0x30042f0+0x14,'H',0);put(PLAYER+0xa4,'I',input);events.clear()
 u.reg_write(UC_ARM_REG_SP,0x3007e00);u.reg_write(UC_ARM_REG_LR,STOP|1);u.emu_start(0x80021e5,STOP,count=10000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP
 assert step(C.byref(s),input)==1
 assert (s.flags,s.timer,s.input_state,s.selection)==(get(GAME+4,'H'),get(GAME+0x76,'H'),get(0x3004304,'H'),get(GAME+0x72,'B')),(case,hex(input),s.flags)
 assert list(s.events[:s.event_count])==events,(case,events,list(s.events[:s.event_count]));n+=1
report=dict(result='pass',rom_comparisons=n,scope=__doc__,rom_sha1=hashlib.sha1(rom).hexdigest(),sanitizers='UBSan')
report['source_sha256']={p:hashlib.sha256((r/p).read_bytes()).hexdigest() for p in ['src/pause.c','src/mode_control.c','include/ssa/pause.h','tests/test_pause_rom.py']}
(r/'analysis/pause-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
