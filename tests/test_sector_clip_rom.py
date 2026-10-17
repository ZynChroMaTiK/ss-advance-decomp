"""Complete D000/CE50/053D0 against original ROM on sixteen maps."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[n for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name in ('Body','Map','Tables')]
exec(compile(ast.Module(nodes,type_ignores=[]),'types','exec'))
class Entity(C.Structure):
 _fields_=[('body',Body),('radius',C.c_int32),('type',C.c_uint16),('player_index',C.c_uint8),('leaf',C.c_int),('sector',C.c_int),('cell',C.c_size_t)]+[(n,C.c_void_p) for n in ('owner','block_next','block_previous','sector_next','sector_previous')]
class Clip(C.Structure):
 _fields_=[(n,C.c_int32) for n in ('x','y','z','end_x','end_y','end_z','radius','distance')]+[(n,C.c_uint16) for n in ('line','leaf','sector')]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['sector_clip.c','geometry.c','map_assets.c','movement.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_sector_clip.so')],check=True)
lib=C.CDLL(str(B/'libssa_sector_clip.so'))
for name,args in {
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
 'ssa_map_assets_free':[C.POINTER(Map)],'ssa_math_tables_load':[C.POINTER(Tables),C.c_void_p,C.c_size_t],
 'ssa_map_leaf':[C.POINTER(Map),C.c_int32,C.c_int32],
 'ssa_sector_clip_find':[C.POINTER(Map),C.POINTER(Tables),C.POINTER(Clip)],
 'ssa_sector_clip':[C.POINTER(Map),C.POINTER(Tables),C.POINTER(Clip)],
 'ssa_sector_relocate':[C.POINTER(Map),C.POINTER(Tables),C.POINTER(Entity),C.POINTER(Clip)],
}.items():getattr(lib,name).argtypes=args
math=Tables();assert lib.ssa_math_tables_load(C.byref(math),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000);uc.mem_map(0x03000000,0x10000);uc.mem_write(0x030000f0,ROM[0x370:0x380])
BASE,E,G,STOP=0x02000000,0x02070000,0x03003f10,0x087ff000
rng=random.Random(0x53d0);links=[]
report={'result':'pass','maps':16,'find_calls':0,'clip_calls':0,'relocate_calls':0,'corrections':0,'link_pairs':0,'guards':0,'live_height_cases':0,'iteration_limit_cases':0}
iterations=0
def put(a,fmt,*v):uc.mem_write(a,struct.pack('<'+fmt,*v))
def get(a,fmt='i'):return struct.unpack('<'+fmt,uc.mem_read(a,struct.calcsize('<'+fmt)))[0]
def hook(u,a,size,ctx):
 global iterations
 if a==0x0800cfc4:
  iterations+=1
  if iterations>1024:u.emu_stop()
  return
 if a==0x08002c88:
  num=C.c_int32(u.reg_read(UC_ARM_REG_R0)).value;den=C.c_int32(u.reg_read(UC_ARM_REG_R1)).value
  assert den
  q=abs(num)//abs(den)*(-1 if (num<0)!=(den<0) else 1)
  u.reg_write(UC_ARM_REG_R0,q&0xffffffff);u.reg_write(UC_ARM_REG_R1,(num-q*den)&0xffffffff);u.reg_write(UC_ARM_REG_R3,abs(q)&0xffffffff)
 else:links.append((a,bytes(u.mem_read(E,116))))
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for a in (0x08002c88,0x0800d214,0x0800d1b4,0x0800cfc4):uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(a,arg=0):
 global iterations
 iterations=0
 links.clear();uc.reg_write(UC_ARM_REG_R0,arg);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 try:uc.emu_start(a|1,STOP,count=20000000)
 except Exception:
  print('oracle fault',hex(a),hex(uc.reg_read(UC_ARM_REG_PC)),[hex(uc.reg_read(r)) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)],flush=True);raise
 if iterations>1024:return None
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,('oracle did not terminate',hex(a),hex(uc.reg_read(UC_ARM_REG_PC)),level,case)
 return C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
for level in range(16):
 m=Map();assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
 data=bytearray(C.string_at(m.data,m.size));original=bytearray(data)
 for off in range(0x1c,0x48,4):struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
 for i in range(m.sector_count):
  off=m.sectors+i*20+12;struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
 uc.mem_write(BASE,bytes(original));put(0x03004930,'I',BASE);put(0x03004954,'I',0x02040000)
 for case in range(256):
  line=rng.randrange(m.line_count);v1,v2=struct.unpack_from('<HH',data,m.lines+40*line)
  ax,ay=struct.unpack_from('<hh',data,m.vertices+4*v1);bx,by=struct.unpack_from('<hh',data,m.vertices+4*v2)
  f=rng.randrange(65537);x=ax*65536+(bx-ax)*f+rng.randrange(-40,41)*65536;y=ay*65536+(by-ay)*f+rng.randrange(-40,41)*65536
  leaf=lib.ssa_map_leaf(C.byref(m),x,y);assert leaf>=0
  sec=struct.unpack_from('<H',data,m.leaves+8*leaf+4)[0]
  if case>=128:
   heights=(rng.choice((-100,0,48,49,100,32700)),rng.choice((99,100,148,149,200,32767)))
   for sector_id in range(m.sector_count):
    raw=struct.pack('<hh',*heights);C.memmove(C.addressof(m.data.contents)+m.sectors+20*sector_id,raw,4);uc.mem_write(BASE+m.sectors+20*sector_id,raw)
   report['live_height_cases']+=1
  state=Clip(x,y,rng.randrange(-100,600)*65536,x,y,rng.randrange(-100,600)*65536,rng.choice((0,1,16,32,33))*65536,123,123,leaf,sec)
  for fn,address,key in (('ssa_sector_clip_find',0x0800d000,'find_calls'),('ssa_sector_clip',0x0800ce50,'clip_calls')):
   native=Clip.from_buffer_copy(bytes(state));uc.mem_write(G,bytes(state)[:38]);expected=run(address,BASE+m.sectors+sec*20)
   actual=getattr(lib,fn)(C.byref(m),C.byref(math),C.byref(native))
   if expected is None:
    assert actual==-1 and bytes(native)==bytes(state),('iteration guard',level,case,actual)
    report['iteration_limit_cases']+=1;continue
   assert actual==expected,(fn,level,case,actual,expected)
   assert bytes(native)[:38]==bytes(uc.mem_read(G,38)),(fn,level,case,list(bytes(native)[:38]),list(uc.mem_read(G,38)))
   report[key]+=1
  e=Entity();e.body.x=x;e.body.y=y;e.body.z=state.z;e.leaf=leaf;e.sector=sec
  e.body.velocity_x=rng.randrange(-40,41)*65536;e.body.velocity_y=rng.randrange(-40,41)*65536;e.body.velocity_z=rng.randrange(-10,11)*65536
  raw=bytearray(rng.randbytes(116));struct.pack_into('<iiiiii',raw,0,x,y,state.z,e.body.velocity_x,e.body.velocity_y,e.body.velocity_z)
  struct.pack_into('<hh',raw,0x6a,sec,0);struct.pack_into('<h',raw,0x6e,leaf);uc.mem_write(E,bytes(raw))
  expected=run(0x080053d0,E);out=Clip();actual=lib.ssa_sector_relocate(C.byref(m),C.byref(math),C.byref(e),C.byref(out))
  if expected is None:
   assert actual==-1 and bytes(out)==bytes(Clip()),('relocate iteration guard',level,case,actual)
   report['iteration_limit_cases']+=1;continue
  assert actual==expected,('relocate return',level,case,actual,expected)
  result=bytes(uc.mem_read(E,116));struct.pack_into('<iii',raw,0,out.end_x,out.end_y,out.end_z)
  struct.pack_into('<h',raw,0x6a,out.sector);struct.pack_into('<h',raw,0x6e,out.leaf)
  assert bytes(raw)==result,('relocate record',level,case)
  assert [a for a,_ in links]==([0x0800d214,0x0800d1b4] if expected else [])
  if expected:
   assert links[0][1][:24]==struct.pack('<iiiiii',x,y,state.z,e.body.velocity_x,e.body.velocity_y,e.body.velocity_z)
   assert links[1][1]==result;report['link_pairs']+=1
  report['relocate_calls']+=1;report['corrections']+=bool(expected)
 # Atomic guards: invalid sector/leaf/list pointer and null math.
 state=Clip();state.sector=m.sector_count;before=bytes(state)
 assert lib.ssa_sector_clip(C.byref(m),C.byref(math),C.byref(state))==-1 and bytes(state)==before;report['guards']+=1
 e=Entity();e.leaf=m.leaf_count;out=Clip.from_buffer_copy(bytes([0xa5])*C.sizeof(Clip));before=bytes(out)
 assert lib.ssa_sector_relocate(C.byref(m),C.byref(math),C.byref(e),C.byref(out))==-1 and bytes(out)==before;report['guards']+=1
 assert lib.ssa_sector_clip(C.byref(m),None,C.byref(state))==-1;report['guards']+=1
 lib.ssa_map_assets_free(C.byref(m))
report['rom_sha1']=hashlib.sha1(ROM).hexdigest();report['scope']=__doc__+' Only BIOS division and sector-list allocator calls are controlled; native game never executes ROM instructions.'
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*[ROOT/'src'/s for s in sources],ROOT/'include/ssa/sector_clip.h',Path(__file__)]}
(ROOT/'analysis/sector-clip-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='source_sha256'}))
