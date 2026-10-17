"""Complete BE60 solid and portal walls, real maps/textures and original pixels.

Only BIOS/DMA copies are modeled. Original fixed math, plane allocation/checks
and textured raster execute. The self-modifying ARM instruction requires an
explicit Unicorn translation-cache synchronization after its real STR.
"""
from pathlib import Path
import ast, ctypes as C, hashlib, json, random, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
classes=[n for n in ast.parse((ROOT/'tests/test_map_texture_bindings_rom.py').read_text()).body if isinstance(n,ast.ClassDef)]
exec(compile(ast.Module(classes,type_ignores=[]),'asset structures','exec'))
class Plane(C.Structure):
 _fields_=[('height',C.c_int16),('min_x',C.c_int16),('max_x',C.c_int16),('color',C.c_uint16),('top',C.c_uint8*122),('bottom',C.c_uint8*122)]
class Pool(C.Structure):
 _fields_=[('records',Plane*80),('count',C.c_uint),('capacity',C.c_uint),('opening_cursor',C.c_size_t)]
class Tables(C.Structure):
 _fields_=[('reciprocal',C.c_uint32*65536),('mip_near',C.c_uint8*16),('mip_far',C.c_uint8*8),('closed_top',C.c_uint8*120),('closed_bottom',C.c_uint8*120),('loaded',C.c_uint)]
class Clip(C.Structure):_fields_=[('top',C.c_int16),('bottom',C.c_int16)]
class Range(C.Structure):_fields_=[('left',C.c_int16),('right',C.c_int16)]
class Command(C.Structure):
 _fields_=[(n,C.c_int32) for n in ('depth_left','depth_right','u_left','u_right')]+[('left',C.c_uint8),('right',C.c_uint8),('segment',C.c_uint16),('ranges',C.POINTER(Range)),('range_count',C.c_size_t),('ceiling',C.POINTER(Plane)),('floor',C.POINTER(Plane))]
class Result(C.Structure):
 _fields_=[('sprite_clip_flags',C.c_uint32),('bottom_height',C.c_int16),('top_height',C.c_int16),('top_clip',C.POINTER(C.c_uint8)),('bottom_clip',C.POINTER(C.c_uint8)),('ceiling',C.POINTER(Plane)),('floor',C.POINTER(Plane)),('top_clip_x',C.c_int16),('bottom_clip_x',C.c_int16)]
class Renderer(C.Structure):
 _fields_=[('bindings',C.POINTER(Bindings)),('tables',C.POINTER(Tables)),('planes',C.POINTER(Pool)),('columns',C.POINTER(Clip)),('framebuffer',C.POINTER(C.c_uint8)),('capacity',C.c_size_t),('eye_z',C.c_int32),('updating',C.c_uint),('openings',C.POINTER(C.c_uint8)),('opening_capacity',C.c_size_t),('framebuffer_offset',C.c_size_t)]
B=ROOT/'build/native-tests';B.mkdir(exist_ok=True,parents=True)
sources=['wall_renderer.c','world_raster.c','map_assets.c','map_texture_bindings.c','texture_assets.c','plane_renderer.c','raster_fill.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_wall_renderer.so')],check=True)
lib=C.CDLL(str(B/'libssa_wall_renderer.so'));P=C.POINTER
for name,args in {
 'ssa_texture_assets_load':[P(Assets),C.c_void_p,C.c_size_t], 'ssa_texture_assets_free':[P(Assets)],
 'ssa_map_assets_load':[P(Map),C.c_void_p,C.c_size_t,C.c_uint], 'ssa_map_assets_free':[P(Map)],
 'ssa_map_texture_bindings_init':[P(Bindings),P(Map),P(Assets),C.c_uint], 'ssa_map_texture_bindings_free':[P(Bindings)],
 'ssa_wall_render_tables_load':[P(Tables),C.c_void_p,C.c_size_t],
 'ssa_wall_render_solid':[P(Renderer),P(Command),P(Result)],
 'ssa_wall_render':[P(Renderer),P(Command),P(Result)],
 'ssa_plane_pool_init':[P(Pool),C.c_uint],
}.items():getattr(lib,name).argtypes=args
tables=Tables();assets=Assets()
assert lib.ssa_wall_render_tables_load(C.byref(tables),ROM,len(ROM))==1
assert bytes(tables.reciprocal)==ROM[0x34934:0x74934]
assert lib.ssa_texture_assets_load(C.byref(assets),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0x04000000,0x1000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]);uc.mem_write(0x03000270,ROM[0x2f4:0x338])
BASE,REC,RANGES,FRAME,PLANES,MIPS,GAME,STOP=0x02000000,0x02080000,0x02081000,0x02090000,0x020a0000,0x020b0000,0x020c0000,0x087ff000
OPENINGS=0x020d0000
rng=random.Random(0xbe60);sync=False;resume_sync=False;columns_called=0;copy_calls=0
last_columns=[]
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_be60_solid':0,'whole_be60_portal':0,'maps':16,'pixels_changed':0,'plane_splits':0,'native_guards':0,'solid_segments':0,'portal_segments':0,'original_raster_calls':0,'bios_dma_copies':0,'modified_sector_cases':0,'top_row_predecessor_reads':0,'clip_bytes':0}
def put(at,fmt,*values):uc.mem_write(at,struct.pack('<'+fmt,*values))
def get(at,fmt='I'):return struct.unpack('<'+fmt,uc.mem_read(at,struct.calcsize('<'+fmt)))[0]
def copy(src,dst,control):
 global copy_calls
 width=4 if control&0x4000000 else 2;count=control&0x1fffff
 uc.mem_write(dst,bytes(uc.mem_read(src,width*count)));copy_calls+=1
def bios(machine,address,size,user):
 copy(machine.reg_read(UC_ARM_REG_R0),machine.reg_read(UC_ARM_REG_R1),machine.reg_read(UC_ARM_REG_R2))
 machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
def dma(machine,access,address,size,value,user):
 if address==0x040000dc and value&0x80000000:copy(get(0x040000d4),get(0x040000d8),value&0x7fffffff)
def synchronize(machine,address,size,user):
 global sync,resume_sync,columns_called
 if resume_sync:resume_sync=False;return
 columns_called+=1;last_columns.append(struct.unpack('<6I',machine.mem_read(0x03005080,24)));sync=True;machine.emu_stop()
uc.hook_add(UC_HOOK_CODE,bios,begin=0x080c5320,end=0x080c5320)
uc.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000dc)
uc.hook_add(UC_HOOK_CODE,synchronize,begin=0x0300027c,end=0x0300027c)
def original():
 global sync,resume_sync
 uc.reg_write(UC_ARM_REG_R0,REC);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 pc=0x0800be61;last_columns.clear()
 while True:
  sync=False;uc.emu_start(pc,STOP,count=1000000)
  if not sync:break
  assert uc.reg_read(UC_ARM_REG_PC)==0x0300027c
  uc.ctl_remove_cache(0x03000270,0x030002b4);resume_sync=True;pc=0x0300027c
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
 assert uc.reg_read(UC_ARM_REG_SP)==0x03007e00
def plane_address(p,pool):
 return 0 if not p else PLANES+C.addressof(p.contents)-C.addressof(pool.records)

for level in range(16):
 m=Map();binding=Bindings();assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
 family=0 if level<=5 or level in (12,13) else 1
 assert lib.ssa_map_texture_bindings_init(C.byref(binding),C.byref(m),C.byref(assets),family)==1
 data=bytearray(C.string_at(m.data,m.size));original_map=bytearray(data)
 for off in range(0x1c,0x48,4):struct.pack_into('<I',original_map,off,BASE+struct.unpack_from('<I',original_map,off)[0])
 original_map[m.textures:m.textures+m.texture_count*16]=C.string_at(binding.records,binding.count*16)
 uc.mem_write(BASE,bytes(original_map))
 mip_data=bytearray()
 for i in range(assets.families[family].archives[0].count):
  for mip in range(3):mip_data+=struct.pack('<I',0x08000000+assets.families[family].archives[mip].images[i].rom_offset)
 uc.mem_write(MIPS,bytes(mip_data))
 solid=[];portal=[]
 for seg in range(m.segment_count):
  line_index,direction=struct.unpack_from('<H2xH',data,m.segments+seg*16+4)
  line=m.lines+line_index*40;flags=struct.unpack_from('<H',data,line+26)[0]
  side=struct.unpack_from('<h',data,line+8+direction*2)[0]
  if side<0:continue
  if flags&4:portal.append(seg);continue
  texture=struct.unpack_from('<h',data,m.sides+side*12+6)[0]
  if texture>=0:solid.append(seg)
 report['solid_segments']+=len(solid)
 report['portal_segments']+=len(portal)
 assert solid
 def compare(seg,case,depths=None,range_values=None,plane_mode=None,right_edge=False):
  line_index,direction=struct.unpack_from('<H2xH',data,m.segments+seg*16+4)
  sector=struct.unpack_from('<h',data,m.lines+line_index*40+12+direction*2)[0]
  floor,ceiling=struct.unpack_from('<hh',data,m.sectors+sector*20)
  eye=((floor+ceiling)//2)*65536+(case&0xffff)
  left=case%90;right=min(119,left+1+case%29)
  if range_values is None:range_values=[(left,right)]
  else:left=min(a for a,b in range_values);right=max(b for a,b in range_values)
  if right_edge:right=120
  ranges=(Range*len(range_values))(*[Range(*v) for v in range_values])
  depths=(5000+(case*313)%18000,5000+(case*173)%18000) if depths is None else depths
  pool=Pool();assert lib.ssa_plane_pool_init(C.byref(pool),level)==1
  C.memmove(C.addressof(pool.records),rng.randbytes(C.sizeof(pool.records)),C.sizeof(pool.records))
  pool.count=2
  for i,h in enumerate((ceiling,floor)):
   pool.records[i].height=h;pool.records[i].color=20+i;pool.records[i].min_x=120;pool.records[i].max_x=-1
   pool.records[i].top[1:121]=[255]*120
  mode=case%4 if plane_mode is None else plane_mode
  if mode==3:
   for p in pool.records[:2]:
    p.min_x=left;p.max_x=right;p.top[left+1]=1;p.bottom[left+1]=2
  cp=C.pointer(pool.records[0]) if mode&1 else P(Plane)();fp=C.pointer(pool.records[1]) if mode&2 else P(Plane)()
  command=Command(depths[0],depths[1],C.c_int32(case*0x18431).value,C.c_int32(case*0x18431+0x180000).value,left,right,seg,ranges,len(ranges),cp,fp)
  columns=(Clip*120)(*[Clip(-1,120) for _ in range(120)])
  if case%5==0:
   for x in range(left,min(right,119)+1):columns[x]=Clip(case%13,110-case%17)
  initial=rng.randbytes(14520);fb=(C.c_uint8*14520).from_buffer_copy(initial)
  openings=(C.c_uint8*14400).from_buffer_copy(rng.randbytes(14400))
  renderer=Renderer(C.pointer(binding),C.pointer(tables),C.pointer(pool),columns,fb,14520,eye,0,openings,14400,120);out=Result()
  uc.mem_write(0x03004930,bytes(0x1000));put(0x03004930,'I',BASE);put(0x03004940,'I',MIPS)
  put(0x0300495c,'HH',65535,65535);put(0x03005098,'HHH',65535,65535,65535)
  put(0x030000c0,'I',FRAME);put(0x03004010,'I',GAME);put(GAME+16,'B',1+case%2)
  put(0x03004028,'i',eye);put(0x03003f44,'I',RANGES)
  put(0x03004990,'II',PLANES,PLANES+pool.count*252)
  put(0x03004c70,'I',OPENINGS);uc.mem_write(OPENINGS,bytes(openings))
  uc.mem_write(PLANES,bytes(pool.records));uc.mem_write(FRAME-120,initial);uc.mem_write(RANGES,bytes(ranges));uc.mem_write(0x030050b4,bytes(columns))
  record=bytearray(rng.randbytes(60));struct.pack_into('<4i',record,4,command.depth_left,command.depth_right,command.u_left,command.u_right)
  struct.pack_into('<BBHHH',record,20,left,right,seg,len(ranges),0)
  struct.pack_into('<II',record,32,plane_address(cp,pool),plane_address(fp,pool))
  out.bottom_height,out.top_height=struct.unpack_from('<hh',record,44)
  uc.mem_write(REC,bytes(record));before=bytes(command)
  original();result=lib.ssa_wall_render(C.byref(renderer),C.byref(command),C.byref(out))
  assert result==0,(level,seg,case,depths,result,[(hex(p[0]-FRAME),p[5],hex(p[2]),hex(p[3])) for p in last_columns],bytes(columns).hex())
  assert bytes(command)==before and renderer.updating==0
  expected=bytes(uc.mem_read(FRAME-120,14520))
  assert bytes(fb)==expected,(level,seg,case,depths,[(i,fb[i],b) for i,b in enumerate(expected) if fb[i]!=b][:8])
  assert bytes(columns)==bytes(uc.mem_read(0x030050b4,480)),(level,seg,case,'clip')
  assert bytes(pool.records)==bytes(uc.mem_read(PLANES,C.sizeof(pool.records))),(level,seg,case,'plane')
  assert pool.count==(get(0x03004994)-PLANES)//252
  assert plane_address(out.ceiling,pool)==get(0x0300499c) and plane_address(out.floor,pool)==get(0x03004998)
  def clip_address(pointer,x):
   if not pointer:return 0
   address=C.addressof(pointer.contents)
   if address==C.addressof(tables.closed_top):return 0x0844a9fc
   if address==C.addressof(tables.closed_bottom):return 0x0844aa74
   assert C.addressof(openings)<=address<=C.addressof(openings)+14400
   return OPENINGS+address-C.addressof(openings)-x
  expected_record=bytearray(record);struct.pack_into('<IhhII',expected_record,40,out.sprite_clip_flags,out.bottom_height,out.top_height,clip_address(out.top_clip,out.top_clip_x),clip_address(out.bottom_clip,out.bottom_clip_x))
  assert bytes(uc.mem_read(REC,60))==bytes(expected_record)
  assert pool.opening_cursor==get(0x03004c70)-OPENINGS
  assert bytes(openings)==bytes(uc.mem_read(OPENINGS,14400))
  for dst,tex,fraction,step,opcode,count in last_columns:
   if count==1 and dst<FRAME+120 and (C.c_int32(step).value<=C.c_int32(fraction+step).value):report['top_row_predecessor_reads']+=1
  report['clip_bytes']+=pool.opening_cursor
  report['pixels_changed']+=sum(a!=b for a,b in zip(initial,expected));report['plane_splits']+=pool.count-2;report['whole_be60_portal' if seg in portal else 'whole_be60_solid']+=1
  return renderer,command,out,pool,columns,fb,ranges,openings
 for n,seg in enumerate(solid):compare(seg,n+1)
 for n,seg in enumerate(portal):compare(seg,n+1)
 for case in range(64):compare(solid[case%len(solid)],case+1000,range_values=[(2,7),(18,23),(75,93)])
 for depth in (1,4999,5000,5003,5004,5031,5032,9999,10000,10003,10004,10063,10064,524288):
  compare(solid[depth%len(solid)],depth,depths=(depth,depth),range_values=[(15,35)])
 compare(solid[0],40,range_values=[(0,119)],right_edge=True)
 # Moving/closed sectors, pegging flags, line-state-dependent upper anchoring,
 # live side texture selection, missing layers and equal-height color changes.
 def edit(at,value):
  data[at:at+len(value)]=value;C.memmove(C.addressof(m.data.contents)+at,value,len(value));uc.mem_write(BASE+at,value)
 for case in range(128):
  seg=portal[case%len(portal)]
  li,di=struct.unpack_from('<H2xH',data,m.segments+seg*16+4);line=m.lines+li*40
  fi,bi=struct.unpack_from('<hh',data,line+12);fi,bi=(fi,bi) if di==0 else (bi,fi)
  si=struct.unpack_from('<h',data,line+8+di*2)[0];side=m.sides+si*12
  offsets={m.sectors+fi*20:20,m.sectors+bi*20:20,line:40,side:12}
  saved={off:bytes(data[off:off+n]) for off,n in offsets.items()}
  edit(m.sectors+fi*20,struct.pack('<hh',0,128))
  bf,bc=((-64,64),(64,192),(32,96),(128,128),(-32,0),(0,128),(-64,192),(0,0))[case%8]
  edit(m.sectors+bi*20,struct.pack('<hh',bf,bc))
  edit(m.sectors+bi*20+10,bytes([case%4,(case//4)%4]))
  old_flags=struct.unpack_from('<H',data,line+26)[0]
  edit(line+26,struct.pack('<H',(old_flags&~24)|((case//8)%4)*8|4))
  edit(line+36,struct.pack('<H',(case//32)&1))
  edit(side+2,struct.pack('<h',(-128,-1,0,1,127)[case%5]))
  valid=next(i for i in range(binding.count) if binding.records[i].width and binding.records[i].height)
  edit(side+4,struct.pack('<h',-1 if case%5==0 else valid))
  edit(side+8,struct.pack('<h',-1 if case%7==0 else valid))
  compare(seg,3000+case,depths=(5000+case,5500-case),range_values=[(3,19),(41,48),(96,119)],plane_mode=3)
  report['modified_sector_cases']+=1
  for off,value in saved.items():edit(off,value)
 # Early guards are atomic and release/reentry behavior is explicit.
 keep=compare(solid[0],1)
 renderer,command,out,pool,columns,fb,ranges,openings=keep
 def guard(expected=-1):
  old=(bytes(out),bytes(pool),bytes(columns),bytes(fb),bytes(openings))
  assert lib.ssa_wall_render(C.byref(renderer),C.byref(command),C.byref(out))==expected
  assert (bytes(out),bytes(pool),bytes(columns),bytes(fb),bytes(openings))==old
  report['native_guards']+=1
 for attr,value in (('updating',1),('capacity',14400),('framebuffer_offset',2**64-1)):
  previous=getattr(renderer,attr);setattr(renderer,attr,value);guard();setattr(renderer,attr,previous)
 for attr,value in (('right',121),('left',119),('segment',32767),('range_count',121)):
  previous=getattr(command,attr);setattr(command,attr,value);guard();setattr(command,attr,previous)
 previous=command.segment;command.segment=portal[0]
 old=(bytes(out),bytes(pool),bytes(columns),bytes(fb))
 assert lib.ssa_wall_render_solid(C.byref(renderer),C.byref(command),C.byref(out))==-2
 assert old==(bytes(out),bytes(pool),bytes(columns),bytes(fb));report['native_guards']+=1;command.segment=previous
 print('PASS solid walls level',level,flush=True)
 lib.ssa_map_texture_bindings_free(C.byref(binding));lib.ssa_map_assets_free(C.byref(m))

lib.ssa_texture_assets_free(C.byref(assets))
report['original_raster_calls']=columns_called;report['bios_dma_copies']=copy_calls
report['source_sha256']={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources}
report['header_sha256']=hashlib.sha256((ROOT/'include/ssa/wall_renderer.h').read_bytes()).hexdigest()
report['test_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
report['scope']=__doc__
(ROOT/'analysis/native-wall-renderer-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
