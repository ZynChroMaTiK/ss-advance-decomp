"""Native static-world pixels versus B6B0 ->017CC ->BE60 ->AB10.

The original D2D0/01828 actor passes are explicit excluded boundaries. BIOS
division/copy and DMA are modeled hardware. All world traversal, projection,
plane generation and ARM raster instructions execute from the user's ROM.
"""
from pathlib import Path
import ast,ctypes as C,hashlib,json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UC_HOOK_INTR
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes();P=C.POINTER
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
def classes(file,names=None):
 nodes=[n for n in ast.parse((ROOT/'tests'/file).read_text()).body if isinstance(n,ast.ClassDef) and (names is None or n.name in names)]
 exec(compile(ast.Module(nodes,type_ignores=[]),file,'exec'),globals())
classes('test_map_texture_bindings_rom.py')
classes('test_world_visibility_rom.py');VisTables=Tables
classes('test_wall_renderer_rom.py',('Tables','Clip','Command','Result','Renderer'));WallTables=Tables;WallRenderer=Renderer
classes('test_plane_renderer_rom.py',('Sky','Renderer'));PlaneRenderer=Renderer
class Coverage(C.Structure):_fields_=[('ranges',Range*124),('count',C.c_uint)]
class Scene(C.Structure):
 _fields_=[('visibility',Visibility),('planes',Pool),('plane_renderer',PlaneRenderer),('wall_renderer',WallRenderer),('columns',Clip*120),('results',Result*100),('draw_ranges',Range*4096),('coverage',Coverage),('sky',P(Sky)),('framebuffer',C.c_uint8*14520),('openings',C.c_uint8*24000),('level',C.c_uint),('updating',C.c_uint),('loaded',C.c_uint)]
class Start(C.Structure):_fields_=[('x',C.c_int32),('y',C.c_int32),('z',C.c_int32),('heading',C.c_uint16),('floor',C.c_int16),('ceiling',C.c_int16)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['world_scene.c','world_visibility.c','wall_renderer.c','world_raster.c','plane_renderer.c','raster_fill.c','enemy_targeting.c','geometry.c','map_assets.c','movement.c','texture_assets.c','map_texture_bindings.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_world_scene_rom.so')],check=True)
lib=C.CDLL(str(B/'libssa_world_scene_rom.so'))
for name,args in {
 'ssa_math_tables_load':[C.c_void_p,C.c_void_p,C.c_size_t], 'ssa_visibility_tables_load':[C.c_void_p,C.c_void_p,C.c_size_t],
 'ssa_wall_render_tables_load':[C.c_void_p,C.c_void_p,C.c_size_t], 'ssa_sky_assets_load':[C.c_void_p,C.c_void_p,C.c_size_t],
 'ssa_texture_assets_load':[C.c_void_p,C.c_void_p,C.c_size_t], 'ssa_texture_assets_free':[C.c_void_p],
 'ssa_map_assets_load':[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[C.c_void_p],
 'ssa_map_texture_bindings_init':[C.c_void_p,C.c_void_p,C.c_void_p,C.c_uint], 'ssa_map_texture_bindings_free':[C.c_void_p],
 'ssa_world_scene_init':[C.c_void_p]*6+[C.c_uint], 'ssa_world_scene_free':[C.c_void_p],
 'ssa_world_scene_clear':[C.c_void_p,C.c_uint8], 'ssa_world_scene_draw':[C.c_void_p,C.c_void_p,C.c_uint8,C.c_uint16],
 'ssa_map_player_start':[C.c_void_p,C.c_void_p], 'ssa_map_sector':[C.c_void_p,C.c_int32,C.c_int32],
 'ssa_sincos':[C.c_void_p,C.c_uint16,C.c_void_p,C.c_void_p],
}.items():getattr(lib,name).argtypes=args
math=Math();vt=VisTables();wt=WallTables();sky=Sky();assets=Assets()
for name,value in [('ssa_math_tables_load',math),('ssa_visibility_tables_load',vt),('ssa_wall_render_tables_load',wt),('ssa_sky_assets_load',sky),('ssa_texture_assets_load',assets)]:assert getattr(lib,name)(C.byref(value),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x200000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0x04000000,0x1000)
for dst,start,end in ((0x030000f0,0x370,0x380),(0x03000270,0x2f4,0x338),(0x030005ac,0x348,0x358),(0x030005bc,0x358,0x36c)):uc.mem_write(dst,ROM[start:end])
BASE,STACK,ANGLES,VALID,SECTORS,PLANES,WALLS,RANGES,SOLID,GAME,MIPS,PLAYER,LEVEL=(0x02000000,0x02040000,0x02050000,0x02060000,0x02061000,0x02070000,0x02080000,0x02090000,0x020a0000,0x020a1000,0x020b0000,0x020c0000,0x020c1000)
FRAME,OPENINGS,STOP=0x03000648,0x030049a0,0x087ff000
sync=False;resume_sync=False;stages=[];raster_calls=0
def put(a,f,*v):uc.mem_write(a,struct.pack('<'+f,*v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
def copy(src,dst,control):
 unit=4 if control&0x4000000 else 2;count=control&0x1fffff
 uc.mem_write(dst,bytes(uc.mem_read(src,unit))*count if control&0x1000000 else bytes(uc.mem_read(src,unit*count)))
def boundary(machine,address,size,user):
 if address==0x080c5320:copy(*[machine.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)])
 else:stages.append(address)
 machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for at in (0x080c5320,0x0800d2d0,0x08001828):uc.hook_add(UC_HOOK_CODE,boundary,begin=at,end=at)
def dma(machine,access,address,size,value,user):
 if value&0x80000000:copy(get(0x040000d4),get(0x040000d8),value&0x7fffffff)
uc.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000dc)
def bios(machine,number,user):
 assert number==2 and machine.reg_read(UC_ARM_REG_PC)==0x08002c8a
 a=C.c_int32(machine.reg_read(UC_ARM_REG_R0)).value;b=C.c_int32(machine.reg_read(UC_ARM_REG_R1)).value;assert b
 q=abs(a)//abs(b);q=-q if (a<0)!=(b<0) else q
 machine.reg_write(UC_ARM_REG_R0,q&0xffffffff);machine.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff);machine.reg_write(UC_ARM_REG_R3,abs(q))
uc.hook_add(UC_HOOK_INTR,bios)
def synchronize(machine,address,size,user):
 global sync,resume_sync,raster_calls
 if resume_sync:resume_sync=False;return
 raster_calls+=1;sync=True;machine.emu_stop()
uc.hook_add(UC_HOOK_CODE,synchronize,begin=0x0300027c,end=0x0300027c)
def original():
 global sync,resume_sync
 uc.reg_write(UC_ARM_REG_R0,BASE);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 pc=0x0800b6b1;stages.clear()
 while True:
  sync=False;uc.emu_start(pc,STOP,count=5000000)
  if not sync:break
  assert uc.reg_read(UC_ARM_REG_PC)==0x0300027c
  uc.ctl_remove_cache(0x03000270,0x030002b4);resume_sync=True;pc=0x0300027c
 assert uc.reg_read(UC_ARM_REG_PC)==STOP and uc.reg_read(UC_ARM_REG_SP)==0x03007e00
 assert stages==[0x0800d2d0,0x08001828]
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_static_world_frames':0,'maps':16,'pixels_compared':0,'wall_commands':0,'portal_commands':0,'planes':0,'opening_bytes':0,'frames':[],'guards':0,'scope':__doc__}
for level in range(16):
 m=Map();binding=Bindings();s=Scene();start=Start()
 assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
 family=0 if level<=5 or level in (12,13) else 1
 assert lib.ssa_map_texture_bindings_init(C.byref(binding),C.byref(m),C.byref(assets),family)==1
 assert lib.ssa_world_scene_init(C.byref(s),C.byref(binding),C.byref(math),C.byref(vt),C.byref(wt),C.byref(sky),level)==1
 assert s.loaded==1 and s.wall_renderer.framebuffer_offset==120
 assert lib.ssa_map_player_start(C.byref(m),C.byref(start))==1
 data=C.string_at(m.data,m.size);mapped=bytearray(data)
 for off in range(0x1c,0x48,4):struct.pack_into('<I',mapped,off,BASE+struct.unpack_from('<I',mapped,off)[0])
 mapped[m.textures:m.textures+m.texture_count*16]=C.string_at(binding.records,binding.count*16)
 uc.mem_write(BASE,bytes(mapped));uc.mem_write(0x03003f00,bytes(0x1800))
 for addr,n in ((ANGLES,m.vertex_count*4),(VALID,s.visibility.vertex_words*2),(SECTORS,s.visibility.sector_words*2),(PLANES,80*252),(WALLS,100*60),(RANGES,4096*4),(SOLID,124*4)):uc.mem_write(addr,bytes(n))
 uc.mem_write(FRAME-120,bytes(s.framebuffer))
 for off,value in ((0,BASE),(0x10,MIPS),(0x24,STACK),(0x38,ANGLES),(0x3c,VALID),(0x40,s.visibility.vertex_words*2),(0x58,SECTORS),(0x5c,s.visibility.sector_words*2),(0x60,PLANES)):put(0x03004930+off,'I',value)
 put(0x03003f44,'II',RANGES,SOLID);put(0x03003f68,'I',WALLS);put(0x03004010,'I',GAME)
 put(0x030000c0,'I',FRAME);put(0x03004648,'I',PLAYER);put(0x0300464e,'H',0);put(0x0300572c,'I',LEVEL);put(LEVEL+16,'H',level)
 triples=bytearray()
 for i in range(assets.families[family].archives[0].count):
  for mip in range(3):triples+=struct.pack('<I',0x08000000+assets.families[family].archives[mip].images[i].rom_offset)
 uc.mem_write(MIPS,bytes(triples))
 for frame in range(8):
  heading=(start.heading+(frame%4)*16384+(frame//4)*1024)&65535
  c=Camera(start.x,start.y,start.z+48*65536+(frame//4)*4*65536,heading,lib.ssa_map_sector(C.byref(m),start.x,start.y),0,0)
  sine=C.c_int32();cosine=C.c_int32();lib.ssa_sincos(C.byref(math),(-heading)&65535,C.byref(sine),C.byref(cosine));c.sine=sine.value;c.cosine=cosine.value
  if frame==4:
   sector=c.sector;off=m.sectors+sector*20;floor,ceiling=struct.unpack_from('<hh',data,off)
   raw=struct.pack('<hh',floor,ceiling-4);C.memmove(C.addressof(m.data.contents)+off,raw,4);uc.mem_write(BASE+off,raw)
  put(0x03004020,'iiiH2xii h',c.x,c.y,c.z,c.heading,c.sine,c.cosine,c.sector)
  put(GAME,'B',frame);put(GAME+16,'B',1+frame%2);put(PLAYER+0xb8,'H',heading)
  assert lib.ssa_world_scene_clear(C.byref(s),0)==1;uc.mem_write(FRAME,bytes(14400))
  original();result=lib.ssa_world_scene_draw(C.byref(s),C.byref(c),frame,heading)
  assert result==1,(level,frame,'native scene error',s.visibility.wall_count,s.planes.count,s.planes.opening_cursor)
  expected=bytes(uc.mem_read(FRAME-120,14520))
  assert bytes(s.framebuffer)==expected,(level,frame,'pixels',[(i,s.framebuffer[i],b) for i,b in enumerate(expected) if s.framebuffer[i]!=b][:12])
  assert bytes(s.columns)==bytes(uc.mem_read(0x030050b4,480)),(level,frame,'columns')
  assert s.planes.count==(get(0x03004994)-PLANES)//252
  assert bytes(s.planes.records)==bytes(uc.mem_read(PLANES,80*252)),(level,frame,'plane bytes')
  assert s.planes.opening_cursor==get(0x03004c70)-OPENINGS
  assert bytes(s.openings[:s.planes.opening_cursor])==bytes(uc.mem_read(OPENINGS,s.planes.opening_cursor))
  assert bytes(s.plane_renderer.row_starts)==bytes(uc.mem_read(0x03004c74,480))
  assert (s.plane_renderer.sky_offset,s.plane_renderer.height_distance,s.plane_renderer.color)==(get(0x0300506c),get(0x03005034),get(0x03005038,'B'))
  assert s.visibility.head==get(0x03003f60,'H') and s.visibility.wall_count==get(0x03003f64,'H')
  linked=[];idx=s.visibility.head
  while not(idx&0x8000):
   assert idx not in linked;linked.append(idx);w=s.visibility.walls[idx]
   assert w.segment_flags==get(WALLS+idx*60+22,'H')
   li=struct.unpack_from('<H',data,m.segments+(w.segment_flags&32767)*16+4)[0]
   report['portal_commands']+=bool(struct.unpack_from('<H',data,m.lines+li*40+26)[0]&4)
   idx=w.next
  assert s.updating==0 and s.wall_renderer.updating==0 and s.plane_renderer.updating==0
  report['wall_commands']+=len(linked);report['planes']+=s.planes.count;report['opening_bytes']+=s.planes.opening_cursor
  report['whole_static_world_frames']+=1;report['pixels_compared']+=14400
  report['frames'].append({'level':level,'frame':frame,'heading':heading,'walls':len(linked),'planes':s.planes.count,'sha256':hashlib.sha256(expected[120:]).hexdigest()})
  (B/f'world-scene-rom-level-{level:02d}-frame-{frame}.indices').write_bytes(bytes(s.framebuffer[120:]))
 print('PASS original static world pixels level',level,flush=True)
 snapshot=bytes(s.framebuffer);s.updating=1
 assert lib.ssa_world_scene_draw(C.byref(s),C.byref(c),0,0)==-1 and bytes(s.framebuffer)==snapshot
 assert lib.ssa_world_scene_clear(C.byref(s),0)==-1 and lib.ssa_world_scene_free(C.byref(s))==-1
 report['guards']+=3;s.updating=0
 assert lib.ssa_world_scene_free(C.byref(s))==1;lib.ssa_map_texture_bindings_free(C.byref(binding));lib.ssa_map_assets_free(C.byref(m))
lib.ssa_texture_assets_free(C.byref(assets))
report['original_arm_columns']=raster_calls
report['source_sha256']={name:hashlib.sha256((ROOT/'src'/name).read_bytes()).hexdigest() for name in sources}
report['test_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
(ROOT/'analysis/native-world-scene-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='frames'},indent=2),flush=True)
