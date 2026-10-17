"""Whole1238 mixed depth/clip insertion and01828 final native actor drawing."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,os,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes();SHA=hashlib.sha1(ROM).hexdigest()
assert SHA=='4ff48d8c18b70a5184758f1917343ae85a70858e';P=C.POINTER
nodes=[n for n in ast.parse((ROOT/'tests/test_map_texture_bindings_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name=='Map']
exec(compile(ast.Module(nodes,type_ignores=[]),'map types','exec'))
nodes=[n for n in ast.parse((ROOT/'tests/test_world_visibility_rom.py').read_text()).body if isinstance(n,ast.ClassDef)]
exec(compile(ast.Module(nodes,type_ignores=[]),'visibility types','exec'))
class ActorProjection(C.Structure):
 _fields_=[('reserved',C.c_uint32)]+[(n,C.c_int16) for n in ('x','y','width','height')]+[('frame_flags',C.c_uint32),('scale',C.c_int32)]+[(n,C.c_int16) for n in ('world_x','world_y','bottom_z','top_z')]+[('animation',C.c_uint16),('clip_offset',C.c_uint16),('left',C.c_int16),('clipped_width',C.c_int16)]
class Actor(C.Structure):_fields_=[('previous',C.c_uint16),('next',C.c_uint16),('depth',C.c_int32),('projection',C.c_uint16),('clip_offset',C.c_uint16)]
class WallClip(C.Structure):_fields_=[('flags',C.c_uint32),('bottom_height',C.c_int16),('top_height',C.c_int16),('top',P(C.c_uint8)),('bottom',P(C.c_uint8)),('top_size',C.c_size_t),('bottom_size',C.c_size_t),('top_x',C.c_int16),('bottom_x',C.c_int16)]
class Queue(C.Structure):_fields_=[('visibility',P(Visibility)),('wall_clips',WallClip*100),('projections',ActorProjection*32),('actors',Actor*32),('clip_words',C.c_int16*7684),('draw_order',C.c_uint16*32),('projection_count',C.c_uint16),('actor_count',C.c_uint16),('draw_count',C.c_uint16),('clip_cursor',C.c_uint32),('updating',C.c_uint)]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
sources=['actor_queue.c','geometry.c','map_assets.c','movement.c']
sources+=['actor_raster.c','actor_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/x) for x in sources],'-o',str(BUILD/'libactor_queue.so')],check=True)
lib=C.CDLL(str(BUILD/'libactor_queue.so'))
for name,args in {'ssa_map_assets_load':[P(Map),C.c_void_p,C.c_size_t,C.c_uint],'ssa_map_assets_free':[P(Map)],'ssa_actor_queue_begin':[P(Queue),P(Visibility)],'ssa_actor_queue_allocate':[P(Queue)],'ssa_actor_queue_insert':[P(Queue),C.c_uint],'ssa_actor_queue_order':[P(Queue)]}.items():getattr(lib,name).argtypes=args
nodes=[n for n in ast.parse((ROOT/'tests/test_actor_assets.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name in ('Image','Resource','Assets')]
exec(compile(ast.Module(nodes,type_ignores=[]),'actor assets types','exec'))
lib.ssa_actor_assets_load.argtypes=[P(Assets),C.c_void_p,C.c_size_t];lib.ssa_actor_assets_free.argtypes=[P(Assets)]
lib.ssa_actor_queue_draw.argtypes=[P(Queue),P(Assets),C.c_void_p,C.c_size_t]
lib.ssa_actor_raster_draw.argtypes=[P(Assets),P(ActorProjection),P(C.c_int16),C.c_size_t,C.c_void_p,C.c_size_t]
assets=Assets();assert lib.ssa_actor_assets_load(C.byref(assets),ROM,len(ROM))==1
images=[(resource,image+1,r.images[image]) for resource,r in enumerate(assets.resources) for image in range(r.count)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x200000);uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x03000110,ROM[0x638:0x798])
BASE,WALLS,ACTORS,PROJECTIONS,CLIPS,ORDER,BYTES,STOP=0x02000000,0x02040000,0x02042000,0x02043000,0x02044000,0x02049000,0x02050000,0x087ff000
def put(a,f,*v):uc.mem_write(a,struct.pack(f,*v))
def get(a,f):return struct.unpack(f,uc.mem_read(a,struct.calcsize(f)))[0]
def call(address,*args):
 for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(r,v&0xffffffff)
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(address|1,STOP,count=4000000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
 return C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
draw_calls=[]
actual_pixels=False
def draw_hook(machine,address,size,user):
 draw_calls.append((uc.reg_read(UC_ARM_REG_R0),(uc.reg_read(UC_ARM_REG_R1)-CLIPS)//2))
 if not actual_pixels:uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
draw_handle=uc.hook_add(UC_HOOK_CODE,draw_hook,begin=0x08009238,end=0x08009238)
rng=random.Random(0x1238)
report={'rom_sha1':SHA,'whole_allocations':0,'whole_insertions':0,'whole_order_passes':0,'whole_pixel_passes':0,'actual_resource_draws':0,'clip_words_compared':0,'linked_records_compared':0,'guards':0,'scope':__doc__}
for level in range(16):
 m=Map();assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
 data=C.string_at(m.data,m.size);original=bytearray(data)
 for off in range(0x1c,0x48,4):struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
 uc.mem_write(BASE,bytes(original));put(0x03004930,'<I',BASE)
 for case in range(64):
  v=Visibility();q=Queue();v.map=C.pointer(m);count=case%17;v.wall_count=count;v.head=0 if count else 65535;v.tail=count-1 if count else 65535
  put(0x03003f60,'<HHHHIIII',v.head,v.tail,count,0,WALLS,ACTORS,ORDER,CLIPS)
  put(0x03004754,'<I',PROJECTIONS);put(0x03004930+0x70c,'<I',0);put(0x03004930+0x714,'<I',CLIPS)
  uc.mem_write(ACTORS,bytes(32*12));uc.mem_write(PROJECTIONS,bytes(32*36));uc.mem_write(CLIPS,bytes(7684*2));uc.mem_write(ORDER,bytes(64))
  buffers=[]
  for i in range(count):
   w=v.walls[i];w.previous=i-1 if i else 65535;w.next=i+1 if i+1<count else 65535
   w.left,w.right=sorted((rng.randrange(120),rng.randrange(121)));w.depth_left=rng.randrange(1,65536);w.depth_right=rng.randrange(1,65536);w.segment_flags=rng.randrange(m.segment_count)
   clip=q.wall_clips[i];clip.flags=rng.randrange(4);clip.bottom_height=rng.choice((-32768,-32,0,32,32767));clip.top_height=rng.choice((-32768,0,64,128,32767))
   if i%5==0:clip.flags=3;clip.bottom_height=32767;clip.top_height=-32768
   for k,name in enumerate(('top','bottom')):
    buf=(C.c_uint8*121)(*[rng.choice((254,255,0,8,32,64,119,120)) for _ in range(121)]);buffers.append(buf)
    setattr(clip,name,buf);setattr(clip,name+'_size',121);setattr(clip,name+'_x',0);uc.mem_write(BYTES+i*256+k*128,bytes(buf))
   raw=bytearray(60);struct.pack_into('<HHii',raw,0,w.previous,w.next,w.depth_left,w.depth_right)
   struct.pack_into('<BBH',raw,20,w.left,w.right,w.segment_flags);struct.pack_into('<IhhII',raw,40,clip.flags,clip.bottom_height,clip.top_height,BYTES+i*256,BYTES+i*256+128);uc.mem_write(WALLS+i*60,bytes(raw))
  assert lib.ssa_actor_queue_begin(C.byref(q),C.byref(v))==1
  for actor in range(32):
   expected=call(0x08000f98);actual=lib.ssa_actor_queue_allocate(C.byref(q));assert actual==expected==actor
   report['whole_allocations']+=1
   p=q.projections[actor];p.left=rng.randrange(120);p.clipped_width=rng.randrange(1,121-p.left)
   if actor%8==0:p.left=0;p.clipped_width=120
   vertex=rng.randrange(m.vertex_count);p.world_x,p.world_y=struct.unpack_from('<hh',data,m.vertices+vertex*4)
   p.bottom_z=rng.choice((-64,0,32,128,32767));p.top_z=rng.choice((-32768,32,64,256));q.projection_count+=1
   rec=q.actors[actor];rec.depth=rng.choice((1,32768,65535,rng.randrange(65536)));rec.projection=actor
   uc.mem_write(PROJECTIONS+actor*36,bytes(p));uc.mem_write(ACTORS+actor*12,bytes(rec));call(0x08001238,actor)
   result=lib.ssa_actor_queue_insert(C.byref(q),actor)
   assert result==1,(level,case,actor,'insert',result)
   assert (v.head,v.tail)==struct.unpack('<HH',uc.mem_read(0x03003f60,4))
   assert q.clip_cursor==get(0x03004930+0x70c,'<I')
   assert bytes(q.actors)[:q.actor_count*12]==bytes(uc.mem_read(ACTORS,q.actor_count*12)),(level,case,actor,'actors')
   assert bytes(q.projections)[:q.projection_count*36]==bytes(uc.mem_read(PROJECTIONS,q.projection_count*36))
   n=(q.clip_cursor+4)*2
   assert bytes(q.clip_words)[:n]==bytes(uc.mem_read(CLIPS,n)),(level,case,actor,'clips',bytes(q.clip_words)[q.actors[actor].clip_offset*2:n].hex(),bytes(uc.mem_read(CLIPS+q.actors[actor].clip_offset*2,n-q.actors[actor].clip_offset*2)).hex())
   for i in range(count):assert (v.walls[i].previous,v.walls[i].next)==struct.unpack('<HH',uc.mem_read(WALLS+i*60,4))
   report['whole_insertions']+=1;report['clip_words_compared']+=n//2;report['linked_records_compared']+=count+q.actor_count
  assert lib.ssa_actor_queue_allocate(C.byref(q))==-1 and call(0x08000f98)==65535;report['guards']+=1
  report['whole_allocations']+=1
  draw_calls.clear();call(0x08001828)
  assert lib.ssa_actor_queue_order(C.byref(q))==1
  expected=[(q.actors[q.draw_order[i]].projection,q.actors[q.draw_order[i]].clip_offset) for i in range(q.draw_count-1,-1,-1)]
  assert draw_calls==expected and bytes(q.draw_order)[:q.draw_count*2]==bytes(uc.mem_read(ORDER,q.draw_count*2))
  report['whole_order_passes']+=1
  if case%4==0:
   # Controlled valid projection scalars; all pixels are the original RLE
   # resources. Existing1238 clips retain their original overlapping storage.
   for i in range(q.projection_count):
    p=q.projections[i];resource,image,im=rng.choice(images)
    p.x=p.left;p.y=rng.randrange(-8,65);p.width=p.clipped_width=min(p.clipped_width,im.width)
    p.height=min(100,im.height*4096//(assets.reciprocal[6144]>>8))
    if p.y+p.height<=0:p.y=0
    p.scale=4096;p.animation=resource;p.frame_flags=image|(0x80000000 if i%2 else 0)
   uc.mem_write(PROJECTIONS,bytes(q.projections));before=rng.randbytes(14400);out=C.create_string_buffer(before)
   fb=0x02060000;uc.mem_write(fb,before);put(0x030000c0,'<I',fb)
   actual_pixels=True;call(0x08001828);actual_pixels=False
   result=lib.ssa_actor_queue_draw(C.byref(q),C.byref(assets),out,14400)
   if result!=1:
    for idx in reversed(q.draw_order[:q.draw_count]):
     rec=q.actors[idx];p=q.projections[rec.projection];clips=C.cast(C.byref(q.clip_words,rec.clip_offset*2),P(C.c_int16))
     if lib.ssa_actor_raster_draw(C.byref(assets),C.byref(p),clips,7684-rec.clip_offset,out,14400)<0:
      raise AssertionError((level,case,idx,'bad draw projection',[(name,getattr(p,name)) for name,_ in p._fields_]))
   assert result==1,(level,case,'actual draw',result)
   assert out.raw[:14400]==bytes(uc.mem_read(fb,14400)),(level,case,'actual pixels')
   report['whole_pixel_passes']+=1;report['actual_resource_draws']+=q.draw_count
 print('map',level,'native1238 mixed queue and reverse01828 passed',flush=True)
 lib.ssa_map_assets_free(C.byref(m))
lib.ssa_actor_assets_free(C.byref(assets))
binary=BUILD/'test_actor_queue'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie',
 '-I',str(ROOT/'include'),str(ROOT/'tests/test_actor_queue.c'),*[str(ROOT/'src'/x) for x in sources],'-o',str(binary)],check=True)
result=subprocess.run([str(binary)],cwd=ROOT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),capture_output=True,text=True,check=True)
report['native_sanitizers']=json.loads(result.stdout)
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
 [ROOT/'include/ssa/actor_queue.h',ROOT/'src/actor_queue.c',ROOT/'tests/test_actor_queue.c',Path(__file__)]}
(ROOT/'analysis/native-actor-queue-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

