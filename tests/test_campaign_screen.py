"""Original story fonts/text rows plus native chapter/ending presentation."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,sys,os
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(r/'scripts'))
from inspect_maps import unpack_lz77
rom=(r/'orig/SSA.gba').read_bytes();font,_=unpack_lz77(rom[0xb7a60:])
sources=[r/'src'/name for name in ('campaign_story.c','campaign_screen.c','map_assets.c')]
base=['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic','-fno-sanitize-recover=all','-I',str(r/'include')]
subprocess.run([*base,'-shared','-fPIC','-fsanitize=undefined',*map(str,sources),'-o',str(b/'libssa_campaign_screen.so')],check=True)
lib=C.CDLL(str(b/'libssa_campaign_screen.so'))
init=lib.ssa_campaign_screen_init;init.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint,C.c_uint,C.c_uint]
row=lib.ssa_campaign_screen_row;row.argtypes=[C.c_void_p,C.c_uint,C.c_uint,C.c_void_p]
lines=lib.ssa_campaign_screen_lines;lines.argtypes=[C.c_void_p,C.c_uint]
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(rom)),(0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x1000),(0x06000000,0x20000)):u.mem_map(at,size)
u.mem_write(0x08000000,rom);u.mem_write(0x06010000,font)
SETTINGS,GAME,STOP=0x02030000,0x02031000,0x087ff000
def put(at,value):u.mem_write(at,struct.pack('<I',value))
def get(at):return struct.unpack('<I',u.mem_read(at,4))[0]
put(0x0300572c,SETTINGS);put(0x03004010,GAME);u.mem_write(GAME+0x10,b'\1')
put(0x03005750+8,0x080b7ea8);u.mem_write(0x03005750+12,b'\0\0')
put(0x0844a800,0x02000000)
def dma(uc,access,address,size,value,ctx):
 if address==0x040000dc:
  source=get(0x040000d4);dest=get(0x040000d8);assert value==0x85000200
  uc.mem_write(dest,bytes(uc.mem_read(source,4))*512)
u.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000df)
def call(pc,args,stack=()):
 sp=0x03007e00
 for i,x in enumerate(stack):put(sp+4*i,x)
 for reg,x in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(reg,x)
 u.reg_write(UC_ARM_REG_SP,sp);u.reg_write(UC_ARM_REG_LR,STOP|1)
 u.emu_start(pc|1,STOP,count=1000000);assert u.reg_read(UC_ARM_REG_PC)==STOP
 return u.reg_read(UC_ARM_REG_R0)
comparisons=0;total_rows=0
for language in range(3):
 s=C.create_string_buffer(256000);assert init(s,rom,len(rom),0,language,1)==1
 u.mem_write(SETTINGS,struct.pack('<H',language))
 for text_id in range(197,202):
  count=lines(s,text_id);assert count==call(0x08018dfc,(text_id,0)) and count>0
  call(0x08018c84,(text_id,0,0x06009400,12),(21,0))
  for n in range(21):
   out=C.create_string_buffer(768);assert row(s,text_id,n,out)==1
   assert out.raw==bytes(u.mem_read(0x06009400+n*768,768)),(language,text_id,n,'initial')
   comparisons+=1
  for n in range(count):
   out=C.create_string_buffer(768);assert row(s,text_id,n,out)==1
   call(0x08018d70,(text_id,0,0x06009400,12),(21,n,0))
   assert out.raw==bytes(u.mem_read(0x06009400,768)),(language,text_id,n,'stream')
   comparisons+=1;total_rows+=1
subprocess.run([*base,'-fsanitize=address,undefined','-no-pie',str(r/'tests/test_campaign_screen.c'),*map(str,sources),'-o',str(b/'test_campaign_screen')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_campaign_screen'),str(r/'orig/SSA.gba'),str(b/'story')],capture_output=True,text=True,env=env,check=True)
native=json.loads(p.stdout)
paths=['src/campaign_story.c','src/campaign_screen.c','src/map_assets.c','include/ssa/campaign_story.h','include/ssa/campaign_screen.h','tests/test_campaign_screen.c','tests/test_campaign_screen.py']
report={'result':'pass','scope':__doc__,'rom_row_comparisons':comparisons,'streamed_rows':total_rows,'languages':3,'native':native,
 'sanitizers':'UBSan text ROM comparisons; ASan/UBSan/leaks=1 native screen sequences',
 'source_sha256':{name:hashlib.sha256((r/name).read_bytes()).hexdigest() for name in paths}}
(r/'analysis/campaign-screen-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
