"""Original NETRICSA18D70 text rows and19C00 records; DMA, labels and sprintf are controlled test boundaries."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
rom=(r/'orig/SSA.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
sys.path.insert(0,str(r/'scripts'));from inspect_maps import unpack_lz77
font,_=unpack_lz77(rom[0xb7a60:])
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined',
 '-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/netricsa.c'),str(r/'src/map_assets.c'),'-o',str(b/'libssa_netricsa.so')],check=True)
class Weapon(C.Structure):_fields_=[('owned',C.c_int32),('ammo',C.c_int32),('shots',C.c_uint32),('hits',C.c_uint32)]
class Enemy(C.Structure):_fields_=[('spawned',C.c_uint32),('hits',C.c_uint32),('kills',C.c_uint32)]
class Rows(C.Structure):_fields_=[('weapons',Weapon*10),('enemies',Enemy*12)]
class Totals(C.Structure):_fields_=[('score',C.c_uint32),('health',C.c_int32),('armor6',C.c_int32),('armor4',C.c_int32),
 *[(n,C.c_uint32) for n in ('level','elapsed','spawned','kills','hits','weapon_hits','pickups')]]
class Data(C.Structure):_fields_=[('rows',Rows),('totals',Totals),*[(n,C.c_uint) for n in ('level','lives','show_all','random','weapon_entries','enemy_entries','opening_entry')]]
class Net(C.Structure):_fields_=[('rom',C.c_void_p),('rom_size',C.c_size_t),('data',Data),
 ('tiles',C.c_uint8*32768),('frame_tiles',C.c_uint8*3456),('images',(C.c_uint8*4096)*23),('fonts',C.c_uint8*4032),
 ('frame_map',C.c_uint16*600),('palette',C.c_uint16*256),('formatted',(C.c_char*512)*2),('text',C.c_void_p*2),
 *[(n,C.c_uint) for n in ('loaded','language','category','focus','weapon','enemy','has_enemies','image')],
 ('title',C.c_uint*2),('text_id',C.c_uint*2),('counter',C.c_uint),('background_scroll',C.c_uint),
 ('scroll',C.c_int*2),('delta',C.c_int*2),('fade_volume',C.c_int),('fade_step',C.c_int),
 ('closing',C.c_uint),('done',C.c_uint),('commands',C.c_uint*8),('command_count',C.c_uint),('hover',C.c_uint)]
lib=C.CDLL(str(b/'libssa_netricsa.so'));init=lib.ssa_netricsa_init;row=lib.ssa_netricsa_row;step=lib.ssa_netricsa_step
init.argtypes=[C.POINTER(Net),C.c_void_p,C.c_size_t,C.c_uint,C.POINTER(Data)];step.argtypes=[C.POINTER(Net),C.c_uint32]
row.argtypes=[C.POINTER(Net),C.c_void_p,C.c_uint,C.c_uint,C.c_void_p,C.c_size_t]
lib.ssa_netricsa_pointer.argtypes=[C.POINTER(Net),C.c_int,C.c_int,C.c_uint]
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(rom)),(0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x1000),(0x05000000,0x1000),(0x06000000,0x20000)):u.mem_map(at,size)
u.mem_write(0x08000000,rom);u.mem_write(0x06010000,font)
BASE,SETTINGS,GAME,STOP=0x02010000,0x02030000,0x02031000,0x087ff000
def put(at,n):u.mem_write(at,struct.pack('<I',n&0xffffffff))
def get(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def string(at):return bytes(u.mem_read(at,4096)).split(b'\0',1)[0]
def call(pc,args,stack=()):
 sp=0x03007e00
 for i,n in enumerate(stack):put(sp+i*4,n)
 for reg,n in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(reg,n)
 u.reg_write(UC_ARM_REG_SP,sp);u.reg_write(UC_ARM_REG_LR,STOP|1)
 u.emu_start(pc|1,STOP,count=1000000);assert u.reg_read(UC_ARM_REG_PC)==STOP
 return u.reg_read(UC_ARM_REG_R0)
put(0x0300572c,SETTINGS);put(0x03004010,GAME);u.mem_write(GAME+0x10,b'\1')
put(0x03005750+8,0x080b7ea8);put(0x03005750+12,0);put(0x0844a800,0x02000000)
def dma(uc,access,address,size,value,ctx):
 if address==0x040000dc:
  source=get(0x040000d4);dest=get(0x040000d8);count=value&0xffff
  if value==0x85000200:uc.mem_write(dest,bytes(uc.mem_read(source,4))*count)
u.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000df)
labels=[];processed_input=0
def hook(uc,pc,size,ctx):
 regs=[uc.reg_read(x) for x in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
 if pc==0x08015974:labels.append(tuple(regs))
 elif pc==0x08003028:put(0x030042f0,processed_input)
 elif pc in (0x08008eb0,0x08008d5c,0x08008e74,0x08015614,0x08018c84,0x08018d70):pass
 elif pc==0x08018dfc:
  address=regs[1]
  if regs[0]:
   table=get(0x0844cf34+get(SETTINGS)*4);address=get(table+regs[0]*4)
  uc.reg_write(UC_ARM_REG_R0,string(address).count(b'\n')+1)
 elif pc==0x080c56c8:
  fmt=string(regs[1]);at=0;args=regs[2:];sp=uc.reg_read(UC_ARM_REG_SP)
  args += [get(sp+i*4) for i in range(10)]
  values=[];i=0
  while i<len(fmt):
   if fmt[i]!=37:i+=1;continue
   i+=1
   if fmt[i]==37:i+=1;continue
   if fmt[i]==48:i+=2
   spec=fmt[i];i+=1;value=args[at];at+=1
   values.append(string(value) if spec==115 else C.c_int32(value).value)
  out=fmt%tuple(values);uc.mem_write(regs[0],out+b'\0')
 else:raise AssertionError(hex(pc))
 uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
for pc in (0x08015974,0x08008eb0,0x080c56c8):u.hook_add(UC_HOOK_CODE,hook,begin=pc,end=pc)
report={'result':'pass','scope':__doc__,'rom_rows':0,'record_calls':0,'controller_calls':0,'german_undefined_format_cases':0}
for language in range(3):
 d=Data();d.level=0;d.lives=3;d.random=25;d.show_all=1
 for i in range(10):d.rows.weapons[i]=Weapon(1,10*i,100,i*3)
 for i in range(12):d.rows.enemies[i]=Enemy(i+1,2*i,i)
 n=Net();assert init(C.byref(n),rom,len(rom),language,C.byref(d))==1
 u.mem_write(SETTINGS,struct.pack('<H',language));put(SETTINGS+0x10,0)
 table=struct.unpack_from('<I',rom,0x44cf34+language*4)[0]-0x08000000
 for text_id in [*range(84,94),*range(116,128),136,*range(202,237)]:
  address=struct.unpack_from('<I',rom,table+text_id*4)[0];text=rom[address-0x08000000:].split(b'\0',1)[0]
  width=15 if 227<=text_id<=236 or text_id==136 else 18
  for k in range(text.count(b'\n')+1):
   out=C.create_string_buffer(width*64);assert row(C.byref(n),text,k,width,out,len(out))==1
   # Compare only rows in this record, as the original scroll bounds do.
   u.mem_write(0x06009400,bytes(width*64))
   call(0x08018d70,(text_id,0,0x06009400,width),(21,k,0))
   assert out.raw==bytes(u.mem_read(0x06009400,width*64)),(language,text_id,k,text.split(b'\n')[k],len(text.split(b'\n')[k]))
   report['rom_rows']+=1
 # Fresh pistol with no shots exercises the original divide-by-zero path.
 d.rows.weapons[1].shots=0;n.data.rows.weapons[1].shots=0
 u.mem_write(GAME+0x90,bytes(d.rows.weapons));u.mem_write(GAME+0x130,bytes(d.rows.enemies))
 d.totals=Totals(12345,75,25,50,3,7551,78,24,7,93,4);n.data.totals=d.totals
 u.mem_write(GAME+0x2f0,bytes(d.totals))
 for category in (0,1,2,3,4):
  for index in range(12 if category==1 else 10 if category==2 else 12):
   u.mem_write(BASE,bytes(0x14c));put(BASE+4,category);put(BASE+0x134,0x06018000);put(BASE+0x138,0x02020000)
   put(BASE+0x140,d.random);u.mem_write(BASE+0x13c,struct.pack('<hh',index if category==2 else 0,index if category==1 else 0))
   put(0x030042f0,0);put(SETTINGS+0x10,index if category>=3 else 0);labels.clear()
   call(0x08019c00,(BASE,))
   n.weapon=index if category==2 else 0;n.enemy=index if category==1 else 0
   n.data.level=index if category>=3 else 0;n.category=0;n.hover=0
   n.category=4 if category==0 else 0
   assert lib.ssa_netricsa_pointer(C.byref(n),12,73-category*12,0)>=0
   assert n.title[0]==labels[0][0] and n.title[1]==labels[1][0]
   assert n.text_id[0]==get(BASE+0x11c),(language,category,index)
   if category==0:assert C.string_at(n.text[0])==string(BASE+0x18)
   if category==1 and language==1:report['german_undefined_format_cases']+=1
   else:
    expected=string(BASE+0x9c) if get(BASE+0x120)==0 else string(struct.unpack_from('<I',rom,table+get(BASE+0x120)*4)[0])
    assert C.string_at(n.text[1])==expected,(language,category,index,C.string_at(n.text[1]),expected)
   report['record_calls']+=1
 # Full1A2C4 navigation/scroll steps with real19C00 underneath. Pixel services
 # are already compared above; skip them here to compare controller state.
 hooks=[]
 for pc in (0x08003028,0x08008d5c,0x08008e74,0x08015614,0x08018c84,0x08018d70,0x08018dfc):
  hooks.append(u.hook_add(UC_HOOK_CODE,hook,begin=pc,end=pc))
 for category in range(5):
  for focus in range(3):
   for keys in (0,0x400040,0x800080,0x20000,0x100000,0x200000,0x500040,0xa00080,0x300000):
    n.category=4 if category==0 else 0;n.hover=0;n.weapon=n.enemy=0;n.data.level=0
    assert lib.ssa_netricsa_pointer(C.byref(n),12,73-category*12,0)>=0
    n.focus=focus;n.scroll[0]=n.scroll[1]=0;n.delta[0]=n.delta[1]=0
    n.counter=0;n.closing=n.done=0;n.fade_step=n.fade_volume=0
    u.mem_write(BASE,bytes(0x14c));put(BASE+4,category);put(BASE+8,focus)
    put(BASE+0x14,0xffffffff);put(BASE+0x98,0xffffffff)
    put(BASE+0x134,0x06018000);put(BASE+0x138,0x02020000);put(BASE+0x140,d.random)
    put(SETTINGS+0x10,0);u.mem_write(SETTINGS+0x19,b'\0');u.mem_write(SETTINGS+0x2c,b'\0')
    processed_input=keys;labels.clear();call(0x0801a2c4,(BASE,))
    assert step(C.byref(n),keys)==1
    expected=(get(BASE+4),get(BASE+8),get(BASE+0xc),get(BASE+0x10),get(BASE+0x128),get(BASE+0x12c),*struct.unpack('<hh',u.mem_read(BASE+0x13c,4)))
    actual=(n.category,n.focus,*n.scroll,*[v&0xffffffff for v in n.delta],n.weapon,n.enemy)
    assert actual==expected,(language,category,focus,hex(keys),actual,expected)
    report['controller_calls']+=1
 for h in hooks:u.hook_del(h)
names=['src/netricsa.c','include/ssa/netricsa.h','tests/test_netricsa_rom.py']
report['source_sha256']={n:hashlib.sha256((r/n).read_bytes()).hexdigest() for n in names}
(r/'analysis/netricsa-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
