"""Original07FDC pixels and normal08420 automap geometry; ARM execution is test-only."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(r/'scripts'));from inspect_maps import unpack_lz77
rom=(r/'orig/SSA.gba').read_bytes();rng=random.Random(0x8420)
inputs=[r/'src/automap.c',r/'include/ssa/automap.h',r/'src/map_assets.c',r/'src/movement.c',Path(__file__)]
subprocess.run(['gcc','-std=c11','-O1','-g','-shared','-fPIC','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),*map(str,(inputs[0],inputs[2],inputs[3])),'-o',str(b/'libautomap.so')],check=True)
lib=C.CDLL(str(b/'libautomap.so'))
nodes=[n for n in ast.parse((r/'tests/test_map_assets.py').read_text()).body if isinstance(n,ast.ClassDef)]
exec(compile(ast.Module(nodes,type_ignores=[]),'map declarations','exec'))
class Math(C.Structure):_fields_=[('sine',C.c_uint16*16384),('direction',C.c_uint16*32769)]
class Point(C.Structure):_fields_=[('x',C.c_int32),('y',C.c_int32)]
class View(C.Structure):
 _fields_=[(n,C.c_int32) for n in ('x','y','player_x','player_y')]+[('zoom',C.c_uint32),('heading',C.c_uint16),('facing',C.c_uint8),('scanline',C.c_uint8),('entities',C.POINTER(Point)),('entity_count',C.c_size_t),('contacts',C.POINTER(C.c_uint16)),('contact_count',C.c_size_t)]
CALL=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int32,C.c_int32,C.c_int32,C.c_int32,C.c_uint)
lib.ssa_automap_line.argtypes=[C.c_void_p,C.c_size_t]+[C.c_int32]*4+[C.c_uint]
lib.ssa_automap_scene.argtypes=[C.POINTER(Map),C.POINTER(Math),C.POINTER(View),CALL,C.c_void_p]
lib.ssa_automap_draw.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Map),C.POINTER(Math),C.POINTER(View)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Math),C.c_void_p,C.c_size_t]
lib.ssa_map_assets_load.argtypes=[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint]
lib.ssa_map_assets_free.argtypes=[C.POINTER(Map)]
math=Math();assert lib.ssa_math_tables_load(C.byref(math),rom,len(rom))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(rom)),(0x02000000,0x100000),(0x03000000,0x10000),(0x04000000,0x1000),(0x06000000,0x18000)):
 uc.mem_map(at,size)
uc.mem_write(0x08000000,rom)
STOP=0x087ff000
def put(at,fmt,*values):uc.mem_write(at,struct.pack('<'+fmt,*values))
def signed(n):return n if n<0x80000000 else n-0x100000000
def call(at,*args):
 uc.reg_write(UC_ARM_REG_SP,0x03007d00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args[:4]):uc.reg_write(reg,value&0xffffffff)
 for i,value in enumerate(args[4:]):put(0x03007d00+i*4,'I',value&0xffffffff)
 uc.emu_start(at|1,STOP,count=200000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
pixels=(C.c_uint8*38400)();line_cases=0
edges=[(-10,80,250,80),(120,-20,120,180),(20,20,20,20),(21,20,21,20),(-50,250,150,-40)]
for trial in range(3000):
 coords=edges[trial] if trial<len(edges) else tuple(rng.randrange(-500,700) for _ in range(4))
 initial=bytes(rng.randrange(16) for _ in range(38400)) if trial%100==0 else bytes(38400)
 color=rng.randrange(256);uc.mem_write(0x06000000,initial);C.memmove(pixels,initial,38400)
 call(0x08007fdc,*coords,color)
 assert lib.ssa_automap_line(pixels,38400,*coords,color)==1,coords
 expected=bytes(uc.mem_read(0x06000000,38400));actual=bytes(pixels)
 assert actual==expected,('line',trial,coords,next((i,a,e) for i,(a,e) in enumerate(zip(actual,expected)) if a!=e))
 line_cases+=1
oracle=[];native=[];contacts=[];query_args=[];capture_line=True
def hook(u,at,size,ctx):
 if at==0x08008870:
  if capture_line:u.emu_stop()
  return
 elif at==0x08007fdc:
  if not capture_line:return
  oracle.append(tuple(signed(u.reg_read(reg)) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3))+(struct.unpack('<I',u.mem_read(u.reg_read(UC_ARM_REG_SP),4))[0],))
 elif at==0x0800036c:
  u.reg_write(UC_ARM_REG_R0,((signed(u.reg_read(UC_ARM_REG_R1))*signed(u.reg_read(UC_ARM_REG_R2)))>>16)&0xffffffff)
 elif at==0x0800429c:
  query_args.append((u.reg_read(UC_ARM_REG_R1),u.reg_read(UC_ARM_REG_R2)))
  put(0x03004458,'I',len(contacts))
  for i,index in enumerate(contacts):put(0x03004620+4*i,'I',0x02010000+index*40)
 else:raise AssertionError(hex(at))
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for at in (0x08007fdc,0x0800036c,0x0800429c,0x08008870):uc.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
uc.ctl_remove_cache(0x08000000,0x08800000)
def dma(u,access,at,size,value,ctx):
 if value&0x80000000:
  source,dest=struct.unpack('<II',u.mem_read(0x040000d4,8));unit=4 if value&0x04000000 else 2
  count=value&0x1ffff
  u.mem_write(dest,bytes(u.mem_read(source,unit))*count if value&0x01000000 else bytes(u.mem_read(source,unit*count)))
uc.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000dc)
@CALL
def emit(ctx,x,y,ex,ey,color):native.append((x,y,ex,ey,color));return 1
scene_cases=segments=full_frames=0
for level in range(16):
 m=Map();assert lib.ssa_map_assets_load(C.byref(m),rom,len(rom),level)
 data=C.string_at(m.data,m.size)
 uc.mem_write(0x02010000,data[m.lines:m.lines+m.line_count*40])
 uc.mem_write(0x02040000,data[m.vertices:m.vertices+m.vertex_count*4])
 uc.mem_write(0x02050000,data[m.sectors:m.sectors+m.sector_count*20])
 put(0x03004930,'I',0x02000000);put(0x02000000+0x14,'H',m.line_count)
 for off,address in ((0x38,0x02010000),(0x3c,0x02040000),(0x24,0x02050000)):put(0x02000000+off,'I',address)
 put(0x03004648,'I',0x02060000);put(0x0300464e,'H',0);put(0x02060000+0xac,'I',0x02061000)
 put(0x030000b0,'I',0);put(0x03004354,'H',5);put(0x03004644,'I',0x02070000)
 for i in range(5):put(0x03004470+2*i,'H',i)
 for trial in range(24):
  x,y=struct.unpack_from('<hh',data,m.things)
  x=x*65536+rng.randrange(-1000000,1000000);y=y*65536+rng.randrange(-1000000,1000000)
  view=View();view.x=x;view.y=y;view.player_x=x+32768;view.player_y=y-65536
  view.heading=rng.randrange(65536);view.facing=rng.randrange(256);view.zoom=(4096,8192,16384,32768,65536)[trial%5]
  points=(Point*5)(*[Point(x+rng.randrange(-1000,1000)*65536,y+rng.randrange(-1000,1000)*65536) for _ in range(5)])
  view.entities=points;view.entity_count=5
  contacts=[rng.randrange(m.line_count) for _ in range(3)];indices=(C.c_uint16*3)(*contacts);view.contacts=indices;view.contact_count=3
  put(0x03004020,'ii',x,y);put(0x0300402c,'H',view.heading)
  put(0x02060000,'ii',view.player_x,view.player_y);put(0x02061000,'ii',view.player_x,view.player_y);put(0x02061000+0x3f,'B',view.facing)
  for i,p in enumerate(points):put(0x02070000+0x74*i,'ii',p.x,p.y)
  oracle.clear();native.clear();query_args.clear()
  # Stop at the normal geometry boundary, before optional debug rays/scanline DMA.
  uc.reg_write(UC_ARM_REG_SP,0x03007d00);uc.reg_write(UC_ARM_REG_LR,STOP|1);uc.reg_write(UC_ARM_REG_R0,view.zoom)
  try:uc.emu_start(0x08008421,0x08008870,count=200000)
  except Exception as e:raise AssertionError(('scene execution',level,trial,hex(uc.reg_read(UC_ARM_REG_PC)),hex(uc.reg_read(UC_ARM_REG_CPSR)))) from e
  assert uc.reg_read(UC_ARM_REG_PC)==0x08008870
  assert lib.ssa_automap_scene(C.byref(m),C.byref(math),C.byref(view),emit,None)==1
  assert native==oracle,('scene',level,trial,len(native),len(oracle),next(((i,a,e) for i,(a,e) in enumerate(zip(native,oracle)) if a!=e),('tail',native[-3:],oracle[-3:])))
  assert len(query_args)==1
  scene_cases+=1;segments+=len(native)
 capture_line=False
 put(0x03004010,'I',0x02080000);put(0x02080000+7,'B',1);put(0x02080000+0x10,'B',1)
 scan=level*17;put(0x03000000,'B',scan);view.scanline=(scan+1)&255
 uc.mem_write(0x06000000,bytes(38400));call(0x08008420,view.zoom)
 assert lib.ssa_automap_draw(pixels,38400,C.byref(m),C.byref(math),C.byref(view))==1
 actual=bytes(pixels);expected=bytes(uc.mem_read(0x06000000,38400))
 assert actual==expected,('full automap',level,[(i,a,e) for i,(a,e) in enumerate(zip(actual,expected)) if a!=e][:8])
 assert uc.mem_read(0x03000000,1)[0]==view.scanline
 capture_line=True;full_frames+=1
 lib.ssa_map_assets_free(C.byref(m))
report={'result':'pass','line_rom_cases':line_cases,'line_rom_pixels':line_cases*38400,'scene_rom_cases':scene_cases,'original_line_submissions':segments,'complete_rom_frames':full_frames,'complete_rom_pixels':full_frames*38400,'maps':16,'scope':__doc__,
 'sanitizers':'UBSan','source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
(r/'analysis/automap-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='source_sha256'}))
