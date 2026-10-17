"""Original B6B0 list construction, B818/B428 and AC8/C10 differential tests.
Only BIOS division/copy/DMA and the three subsequent drawing-stage calls are boundaries.
Original traversal, math, clipping, wall projection and plane allocation execute.
"""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UC_HOOK_INTR
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
SHA=hashlib.sha1(ROM).hexdigest();assert SHA=='4ff48d8c18b70a5184758f1917343ae85a70858e'
P=C.POINTER
nodes=[n for n in ast.parse((ROOT/'tests/test_map_texture_bindings_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name=='Map']
exec(compile(ast.Module(nodes,type_ignores=[]),'map ctypes','exec'))
class Math(C.Structure):_fields_=[('sine',C.c_uint16*16384),('direction',C.c_uint16*32769)]
class Camera(C.Structure):_fields_=[('x',C.c_int32),('y',C.c_int32),('z',C.c_int32),('heading',C.c_uint16),('sector',C.c_int16),('sine',C.c_int32),('cosine',C.c_int32)]
class Tables(C.Structure):_fields_=[('reciprocal',C.c_uint32*65536),('tangent',C.c_int32*16384),('angle_x',C.c_uint8*32768),('corners',C.c_uint8*44),('loaded',C.c_uint)]
class Plane(C.Structure):_fields_=[('height',C.c_int16),('min_x',C.c_int16),('max_x',C.c_int16),('color',C.c_uint16),('top',C.c_uint8*122),('bottom',C.c_uint8*122)]
class Pool(C.Structure):_fields_=[('records',Plane*80),('count',C.c_uint),('capacity',C.c_uint),('opening_cursor',C.c_size_t)]
class Range(C.Structure):_fields_=[('left',C.c_int16),('right',C.c_int16)]
class Wall(C.Structure):_fields_=[('previous',C.c_uint16),('next',C.c_uint16)]+[(n,C.c_int32) for n in ('depth_left','depth_right','u_left','u_right')]+[('left',C.c_uint8),('right',C.c_uint8)]+[(n,C.c_uint16) for n in ('segment_flags','range_count','range_start','front_sector')]+[('ceiling',P(Plane)),('floor',P(Plane)),('distance',C.c_int32)]
class Projection(C.Structure):_fields_=[(n,C.c_uint16) for n in ('left','right','original_left','original_right','angle_left','angle_right')]
class Visibility(C.Structure):_fields_=[('map',P(Map)),('math',P(Math)),('tables',P(Tables)),('planes',P(Pool)),('camera',Camera)]+[(n,P(C.c_uint16)) for n in ('vertex_angles','vertex_valid','sector_visited','stack')]+[(n,C.c_size_t) for n in ('vertex_words','sector_words','stack_capacity')]+[('walls',Wall*100),('ranges',Range*4096),('solid',Range*122)]+[(n,C.c_uint16) for n in ('head','tail','wall_count','range_count','solid_count','high_water')]+[(n,C.c_int16) for n in ('front_sector','back_sector','front_ceiling','front_floor','back_ceiling','back_floor')]+[('ceiling',P(Plane)),('floor',P(Plane)),('game_counter',C.c_uint8),('updating',C.c_uint)]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
sources=['world_visibility.c','plane_renderer.c','raster_fill.c','enemy_targeting.c','geometry.c','map_assets.c','movement.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/x) for x in sources],'-o',str(BUILD/'libworld_visibility.so')],check=True)
lib=C.CDLL(str(BUILD/'libworld_visibility.so'))
for name,args in {
 'ssa_map_assets_load':[P(Map),C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[P(Map)],
 'ssa_math_tables_load':[P(Math),C.c_void_p,C.c_size_t], 'ssa_visibility_tables_load':[P(Tables),C.c_void_p,C.c_size_t],
 'ssa_plane_pool_init':[P(Pool),C.c_uint], 'ssa_world_visibility_init':[P(Visibility),P(Map),P(Math),P(Tables),P(Pool)],
 'ssa_world_visibility_free':[P(Visibility)],'ssa_world_visibility_build':[P(Visibility),P(Camera),C.c_uint8],
 'ssa_visibility_reset':[P(Visibility),P(Camera),C.c_uint8], 'ssa_visibility_segment':[P(Visibility),C.c_uint,P(Projection)],
 'ssa_visibility_box':[P(Visibility),P(C.c_int16)],'ssa_visibility_clip':[P(Visibility),C.c_int,C.c_int,C.c_int],
 'ssa_map_sector':[P(Map),C.c_int32,C.c_int32]}.items():getattr(lib,name).argtypes=args
math=Math();tables=Tables();assert lib.ssa_math_tables_load(C.byref(math),ROM,len(ROM))==1
assert lib.ssa_visibility_tables_load(C.byref(tables),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x200000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0x04000000,0x1000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
BASE,STACK,ANGLES,VALID,SECTORS,PLANES,WALLS,RANGES,SOLID,GAME,SCRATCH,STOP=(0x02000000,0x02040000,0x02050000,0x02060000,0x02061000,0x02070000,0x02080000,0x02090000,0x020a0000,0x020a1000,0x020a2000,0x087ff000)
def put(a,f,*x):uc.mem_write(a,struct.pack(f,*x))
def get(a,f):return struct.unpack(f,uc.mem_read(a,struct.calcsize(f)))[0]
def ret():uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
stages=[]
def hook(machine,address,size,user):
 if address==0x080c5320:
  src,dst,ctrl=[uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)]
  unit=4 if ctrl&0x4000000 else 2;n=ctrl&0x1fffff
  uc.mem_write(dst,bytes(uc.mem_read(src,unit))*n if ctrl&0x1000000 else bytes(uc.mem_read(src,unit*n)));ret()
 else:stages.append(address);ret()
for addr in (0x080c5320,0x080017cc,0x0800d2d0,0x08001828):uc.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
def dma(machine,access,address,size,value,user):
 if address!=0x040000dc:return
 src=get(0x040000d4,'<I');dst=get(0x040000d8,'<I');unit=4 if value&0x4000000 else 2;n=value&65535
 uc.mem_write(dst,bytes(uc.mem_read(src,unit))*n if value&0x1000000 else bytes(uc.mem_read(src,unit*n)))
uc.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000dc)
def bios(machine,number,user):
 assert number==2 and uc.reg_read(UC_ARM_REG_PC)==0x08002c8a
 a=C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value;b=C.c_int32(uc.reg_read(UC_ARM_REG_R1)).value
 assert b;q=abs(a)//abs(b);q=-q if (a<0)!=(b<0) else q
 uc.reg_write(UC_ARM_REG_R0,q&0xffffffff);uc.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff);uc.reg_write(UC_ARM_REG_R3,abs(q))
uc.hook_add(UC_HOOK_INTR,bios)
def call(addr,*args):
 for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(reg,value&0xffffffff)
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 try:uc.emu_start(addr|1,STOP,count=3000000)
 except Exception as e:raise RuntimeError(hex(uc.reg_read(UC_ARM_REG_PC))) from e
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 return C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
def camera_write(c):put(0x03004020,'<iiiH2xii h',c.x,c.y,c.z,c.heading,c.sine,c.cosine,c.sector)
def plane_ptr(p,pool):return PLANES+(C.addressof(p.contents)-C.addressof(pool.records)) if p else 0
def setup(m,v,pool,data):
 original=bytearray(data)
 for off in range(0x1c,0x48,4):struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
 uc.mem_write(BASE,bytes(original));uc.mem_write(0x03003f00,bytes(0x1800))
 uc.mem_write(ANGLES,bytes(m.vertex_count*4));uc.mem_write(PLANES,bytes(pool.records));uc.mem_write(WALLS,bytes(100*60))
 uc.mem_write(SOLID,bytes(122*4));uc.mem_write(RANGES,bytes(4096*4))
 put(0x03004010,'<I',GAME);put(GAME+16,'<B',2)
 for off,value in ((0,BASE),(0x24,STACK),(0x38,ANGLES),(0x3c,VALID),(0x40,v.vertex_words*2),(0x58,SECTORS),(0x5c,v.sector_words*2),(0x60,PLANES)):
  put(0x03004930+off,'<I',value)
 put(0x03003f44,'<II',RANGES,SOLID);put(0x03003f68,'<I',WALLS)
def reset(v,c):
 assert lib.ssa_visibility_reset(C.byref(v),C.byref(c),0)==1;camera_write(c);call(0x0800b574,BASE)
rng=random.Random(0xb6b0)
report={'rom_sha1':SHA,'whole_builds':0,'segment_calls':0,'box_calls':0,'clip_calls':0,'walls_compared':0,'ranges_compared':0,'live_height_frames':0,'maps':[],'scope':__doc__}
for level in range(16):
 m=Map();pool=Pool();v=Visibility();assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
 assert lib.ssa_plane_pool_init(C.byref(pool),level)==1
 assert lib.ssa_world_visibility_init(C.byref(v),C.byref(m),C.byref(math),C.byref(tables),C.byref(pool))==1
 data=C.string_at(m.data,m.size);vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]));setup(m,v,pool,data)
 total=0;changed=None
 for case in range(128):
  if changed is not None:
   off=m.sectors+changed*20;C.memmove(C.addressof(m.data.contents)+off,data[off:off+4],4);uc.mem_write(BASE+off,data[off:off+4]);changed=None
  if case>=64:
   changed=rng.randrange(m.sector_count);off=m.sectors+changed*20
   floor,ceiling=struct.unpack_from('<hh',data,off)
   values=(floor+8,ceiling-8) if case%3 else (ceiling,ceiling)
   raw=struct.pack('<hh',*[C.c_int16(x).value for x in values])
   C.memmove(C.addressof(m.data.contents)+off,raw,4);uc.mem_write(BASE+off,raw);report['live_height_frames']+=1
  x,y=rng.choice(vertices);x=x*65536+rng.randrange(-48*65536,48*65536);y=y*65536+rng.randrange(-48*65536,48*65536)
  c=Camera(x,y,rng.choice((8,32,48,80,128))*65536,rng.randrange(65536),lib.ssa_map_sector(C.byref(m),x,y),0,0)
  assert c.sector>=0
  # Real-map original construction, including all math/planes/occlusion.
  camera_write(c);put(GAME,'<B',case);put(GAME+16,'<B',1 if case%2 else 2);stages.clear()
  call(0x0800b6b0,BASE)
  result=lib.ssa_world_visibility_build(C.byref(v),C.byref(c),case)
  assert result==1,(level,case,'native error',v.wall_count,v.range_count)
  assert stages==[0x080017cc,0x0800d2d0,0x08001828]
  for field,addr,fmt in (('head',0x03003f60,'<H'),('tail',0x03003f62,'<H'),('wall_count',0x03003f64,'<H'),('range_count',0x03003f3c,'<H'),('high_water',0x03003f78,'<I')):
   assert getattr(v,field)==get(addr,fmt),(level,case,field,getattr(v,field),get(addr,fmt))
  assert v.solid_count==(get(0x03003f50,'<I')-SOLID)//4,(level,case,v.solid_count,(get(0x03003f50,'<I')-SOLID)//4,bytes(v.solid)[:32].hex(),bytes(uc.mem_read(SOLID,32)).hex())
  assert C.string_at(v.solid,v.solid_count*4)==bytes(uc.mem_read(SOLID,v.solid_count*4))
  assert C.string_at(v.ranges,v.range_count*4)==bytes(uc.mem_read(RANGES,v.range_count*4)),(level,case,'ranges')
  assert C.string_at(v.vertex_valid,v.vertex_words*2)==bytes(uc.mem_read(VALID,v.vertex_words*2))
  assert C.string_at(v.sector_visited,v.sector_words*2)==bytes(uc.mem_read(SECTORS,v.sector_words*2))
  for i in range(m.vertex_count):
   if v.vertex_valid[i//16]&(1<<(i&15)):assert v.vertex_angles[i]==get(ANGLES+i*4,'<H')
  assert pool.count==(get(0x03004994,'<I')-PLANES)//252
  assert bytes(pool.records)[:pool.count*252]==bytes(uc.mem_read(PLANES,pool.count*252)),(level,case,'planes')
  for name,addr in (('front_sector',0x0300495c),('back_sector',0x0300495e),('front_ceiling',0x030050ac),('front_floor',0x030050ae),('back_ceiling',0x030050b0),('back_floor',0x030050b2)):
   assert getattr(v,name)==get(addr,'<h'),(level,case,name,getattr(v,name),get(addr,'<h'))
  assert plane_ptr(v.ceiling,pool)==get(0x0300499c,'<I') and plane_ptr(v.floor,pool)==get(0x03004998,'<I')
  linked=set();idx=v.head
  while idx!=65535:assert idx not in linked;linked.add(idx);idx=v.walls[idx].next
  for i in range(v.wall_count):
   w=v.walls[i];addr=WALLS+i*60
   for name,off in (('depth_left',4),('depth_right',8),('u_left',12),('u_right',16)):
    assert getattr(w,name)==get(addr+off,'<i'),(level,case,i,name,getattr(w,name),get(addr+off,'<i'))
   if i not in linked:continue
   for name,off,fmt in (('previous',0,'<H'),('next',2,'<H'),('left',20,'<B'),('right',21,'<B'),('segment_flags',22,'<H'),('range_count',24,'<H'),('range_start',26,'<H'),('front_sector',28,'<H'),('distance',56,'<i')):
    assert getattr(w,name)==get(addr+off,fmt),(level,case,i,name,getattr(w,name),get(addr+off,fmt))
   assert plane_ptr(w.ceiling,pool)==get(addr+32,'<I') and plane_ptr(w.floor,pool)==get(addr+36,'<I')
  report['whole_builds']+=1;report['walls_compared']+=v.wall_count;report['ranges_compared']+=v.range_count;total+=len(linked)
  # Whole isolated B818 on every eighth camera, with a reused angle cache.
  if case%8==0:
   for i in range(m.segment_count):
    out=Projection();C.memset(C.byref(out),0xa7,C.sizeof(out));uc.mem_write(SCRATCH,bytes(out))
    expected=call(0x0800b818,BASE,BASE+m.segments+i*16,SCRATCH)
    actual=lib.ssa_visibility_segment(C.byref(v),i,C.byref(out))
    assert actual==expected and bytes(out)==bytes(uc.mem_read(SCRATCH,12)),(level,case,i,'segment',actual,expected)
    report['segment_calls']+=1
  for j in range(4):
   i=rng.randrange(m.node_count);off=m.nodes+i*32+8+(j%2)*8;b=(C.c_int16*4).from_buffer_copy(data[off:off+8])
   expected=call(0x0800b428,BASE+off);actual=lib.ssa_visibility_box(C.byref(v),b)
   assert actual==expected,(level,case,i,'box',actual,expected);report['box_calls']+=1
 # Independent inclusive-range insertion/portal sequences.
 c=Camera(0,0,0,0,0,0,0)
 for case in range(64):
  reset(v,c)
  for j in range(32):
   a,b=sorted((rng.randrange(120),rng.randrange(120)));solid=rng.randrange(2)
   expected=call(0x08000ac8 if solid else 0x08000c10,a,b);actual=lib.ssa_visibility_clip(C.byref(v),a,b,solid)
   assert actual==expected,(level,case,j,'clip',a,b,actual,expected)
   assert v.range_count==get(0x03003f3c,'<H') and C.string_at(v.ranges,v.range_count*4)==bytes(uc.mem_read(RANGES,v.range_count*4))
   assert v.solid_count==(get(0x03003f50,'<I')-SOLID)//4 and C.string_at(v.solid,v.solid_count*4)==bytes(uc.mem_read(SOLID,v.solid_count*4))
   report['clip_calls']+=1
 report['maps'].append({'level':level,'frames':128,'linked_walls':total})
 print('map',level,'whole builds, wall queue, projections and clip ranges passed',flush=True)
 assert lib.ssa_world_visibility_free(C.byref(v))==1;lib.ssa_map_assets_free(C.byref(m))
binary=BUILD/'test_world_visibility'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie',
 '-I',str(ROOT/'include'),str(ROOT/'tests/test_world_visibility.c'),*[str(ROOT/'src'/x) for x in sources],'-o',str(binary)],check=True)
import os
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
result=subprocess.run([str(binary)],cwd=ROOT,env=env,capture_output=True,text=True,check=True)
report['native_sanitizers']=json.loads(result.stdout)
report['sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
 [ROOT/'include/ssa/world_visibility.h',ROOT/'src/world_visibility.c',ROOT/'tests/test_world_visibility.c',Path(__file__)]}
(ROOT/'analysis/native-world-visibility-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))


