"""Complete B6B0 world+actor render chain versus native composition.

Uses genuine01984 map-object positions and actual animation/resources; no AI
activation is invented. All original rendering calls execute, including D2D0,
D338,D490,D884,1238,01828,09238/09258 and ARM pixels. BIOS/DMA are modeled.
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

fixture=B/'actor_scene_fixture.c'
fixture.write_text(r"""
#include "ssa/actor_projection.h"
#include "ssa/world_scene.h"
#include "ssa/spawn.h"
#include "ssa/actor_animation.h"
#include "ssa/actor_scene.h"
#include <string.h>
static SsaActorAssets assets;static SsaActorScene scene;static SsaWorld world;
static SsaSpawnDefinitions definitions;static SsaSpawnPlan plan;static SsaActorSource sources[100];
static SsaActorAnimations animations;static SsaActorAnimation states[100];
static void w16(unsigned char*p,unsigned n){p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);}
static void w32(unsigned char*p,uint32_t n){w16(p,n);w16(p+2,n>>16);}
int actors_init(SsaWorldScene*s,const void*rom,size_t n,unsigned level){unsigned i;
 if(!assets.loaded && ssa_actor_assets_load(&assets,rom,n)!=1)return -1;
 if(!animations.loaded && ssa_actor_animations_load(&animations,rom,n)!=1)return -1;
 if(ssa_spawn_definitions_load(&definitions,rom,n)!=1 || ssa_world_init(&world,s->visibility.map,100)!=1 ||
    ssa_spawn_plan_load(&plan,s->visibility.map,&definitions,level,1,100)!=1 || ssa_spawn_plan_insert(&plan,&world)!=1)return -1;
 memset(states,0,sizeof(states));
 for(i=0;i<plan.object_count;++i){const SsaSpawnObject*o=plan.objects+i;sources[i]=(SsaActorSource){&o->entity,o->animation,0,o->entity.body.flags,o->facing,o->entity.player_index};
 if(ssa_actor_animation_set(states+i,o->animation,o->animation_ticks)!=1)return -1;}
 if(ssa_actor_scene_init(&scene,s,&world,&assets,s->visibility.math)!=1)return -1;
 return (int)plan.object_count;
}
int actors_free(void){if(ssa_actor_scene_free(&scene)!=1)return -1;ssa_world_free(&world);ssa_spawn_plan_free(&plan);return 1;}
void actors_export(unsigned char*raw,int16_t*heads,int16_t*next){unsigned i,s;
 for(i=0;i<plan.object_count;++i){const SsaSpawnObject*o=plan.objects+i;const SsaBody*b=&o->entity.body;unsigned char*r=raw+116*i;memset(r,0,116);
 w32(r,(uint32_t)b->x);w32(r+4,(uint32_t)b->y);w32(r+8,(uint32_t)b->z);w32(r+44,(uint32_t)b->height);w16(r+48,sources[i].flags);
 r[63]=o->facing;r[83]=(unsigned char)b->step_adjust;r[84]=o->entity.player_index;memcpy(r+88,states+i,8);w16(r+114,(unsigned)b->floor);
 next[i]=-1;if(o->entity.sector_next)for(s=0;s<plan.object_count;++s)if(&plan.objects[s].entity==o->entity.sector_next)next[i]=(int16_t)s;}
 for(s=0;s<world.collision.map->sector_count;++s){heads[s]=-1;for(i=0;i<plan.object_count;++i)if(&plan.objects[i].entity==world.sectors[s])heads[s]=(int16_t)i;}
}
int actors_draw(SsaWorldScene*s,const SsaRenderCamera*c,unsigned flags){
 if(s!=scene.geometry)return -1;
 return ssa_actor_scene_draw(&scene,c,flags,sources,plan.object_count);
}
SsaActorQueue* actors_queue(void){return &scene.queue;}
int actors_tick(unsigned ticks){unsigned i;for(i=0;i<plan.object_count;++i){
 if(ssa_actor_animation_step(states+i,&animations,plan.objects[i].entity.body.flags,ticks)!=1)return -1;
 sources[i].animation=states[i].animation;sources[i].frame=states[i].frame;}return 1;}
""")
sources+=['actor_projection.c','actor_assets.c','actor_queue.c','actor_raster.c','world.c','physics.c','slide.c','horizontal.c','spawn.c','actor_animation.c','actor_scene.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(fixture),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_actor_scene_rom.so')],check=True)
lib=C.CDLL(str(B/'libssa_actor_scene_rom.so'))
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
classes('test_actor_queue_rom.py',('ActorProjection','Actor','WallClip','Queue'))
lib.actors_init.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint]
lib.actors_export.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p]
lib.actors_draw.argtypes=[C.c_void_p,C.c_void_p,C.c_uint]
lib.actors_queue.restype=P(Queue)
lib.actors_tick.argtypes=[C.c_uint]
math=Math();vt=VisTables();wt=WallTables();sky=Sky();assets=Assets()
for name,value in [('ssa_math_tables_load',math),('ssa_visibility_tables_load',vt),('ssa_wall_render_tables_load',wt),('ssa_sky_assets_load',sky),('ssa_texture_assets_load',assets)]:assert getattr(lib,name)(C.byref(value),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x200000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0x04000000,0x1000)
for dst,start,end in ((0x03000110,0x638,0x798),(0x030000f0,0x370,0x380),(0x03000270,0x2f4,0x338),(0x030005ac,0x348,0x358),(0x030005bc,0x358,0x36c)):uc.mem_write(dst,ROM[start:end])
BASE,STACK,ANGLES,VALID,SECTORS,PLANES,WALLS,RANGES,SOLID,GAME,MIPS,PLAYER,LEVEL=(0x02000000,0x02040000,0x02050000,0x02060000,0x02061000,0x02070000,0x02080000,0x02090000,0x020a0000,0x020a1000,0x020b0000,0x020c0000,0x020c1000)
ENTITIES,HEADS,NODES,PROJECTIONS,ACTORS,CLIPS,ORDER,ANIMFRAMES=range(0x020d0000,0x02150000,0x10000)
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
for at in (0x080c5320,):uc.hook_add(UC_HOOK_CODE,boundary,begin=at,end=at)
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
 assert stages==[]
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_world_actor_frames':0,'animated_frames':0,'actor_commands':0,'map_objects':0,'maps':16,'pixels_compared':0,'wall_commands':0,'portal_commands':0,'planes':0,'opening_bytes':0,'frames':[],'guards':0,'scope':__doc__}
for level in range(16):
 m=Map();binding=Bindings();s=Scene();start=Start()
 assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
 family=0 if level<=5 or level in (12,13) else 1
 assert lib.ssa_map_texture_bindings_init(C.byref(binding),C.byref(m),C.byref(assets),family)==1
 assert lib.ssa_world_scene_init(C.byref(s),C.byref(binding),C.byref(math),C.byref(vt),C.byref(wt),C.byref(sky),level)==1
 nactors=lib.actors_init(C.byref(s),ROM,len(ROM),level);assert 0<=nactors<=100;report['map_objects']+=nactors
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
 rawactors=C.create_string_buffer(11600);heads=(C.c_int16*m.sector_count)();nexts=(C.c_int16*100)()
 lib.actors_export(rawactors,heads,nexts)
 uc.mem_write(ENTITIES,rawactors.raw);uc.mem_write(HEADS,bytes(heads))
 for i in range(nactors):put(NODES+6*i,'HHH',65535,nexts[i]&65535,i)
 put(0x03004644,'I',ENTITIES);put(0x03004984,'I',HEADS)
 put(0x03004978,'I',6);put(0x03004980,'I',NODES)
 put(0x03004754,'I',PROJECTIONS);put(0x03003f6c,'III',ACTORS,ORDER,CLIPS)
 uc.mem_write(PROJECTIONS,bytes(32*36));uc.mem_write(ACTORS,bytes(32*12));uc.mem_write(CLIPS,bytes(7684*2));uc.mem_write(ORDER,bytes(64))
 strides=[]
 for i in range(142):
  table,anim,dirs,flags=struct.unpack_from('<IIHH',ROM,0x449d2c+i*12)
  count=struct.unpack_from('<I',ROM,table-0x08000000)[0] if table else 0
  strides.append((count//dirs//flags.bit_count())&255 if count else 0)
 uc.mem_write(ANIMFRAMES,bytes(strides));put(0x03003f38,'I',ANIMFRAMES)
 put(0x03004354,'H',nactors)
 for i in range(nactors):put(0x03004470+i*2,'H',i)
 for frame in range(16):
  # Repeat the eight bounded original scratch views while animation advances.
  # Original openings have720 bytes before their own cursor; arbitrary views
  # can overflow that original layout, unlike the native bounded24k buffer.
  view=frame%8
  heading=(start.heading+(view%4)*16384+(view//4)*1024)&65535
  c=Camera(start.x,start.y,start.z+48*65536+(view//4)*4*65536,heading,lib.ssa_map_sector(C.byref(m),start.x,start.y),0,0)
  sine=C.c_int32();cosine=C.c_int32();lib.ssa_sincos(C.byref(math),(-heading)&65535,C.byref(sine),C.byref(cosine));c.sine=sine.value;c.cosine=cosine.value
  if frame==4:
   sector=c.sector;off=m.sectors+sector*20;floor,ceiling=struct.unpack_from('<hh',data,off)
   raw=struct.pack('<hh',floor,ceiling-4);C.memmove(C.addressof(m.data.contents)+off,raw,4);uc.mem_write(BASE+off,raw)
  put(0x03004020,'iiiH2xii h',c.x,c.y,c.z,c.heading,c.sine,c.cosine,c.sector)
  put(GAME,'B',frame);put(GAME+16,'B',1+frame%2);put(PLAYER+0xb8,'H',heading)
  if frame>=8:
   ticks=1+(frame-8)*3;put(GAME+7,'B',ticks)
   uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
   uc.emu_start(0x08000909,STOP,count=100000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
   assert lib.actors_tick(ticks)==1
   lib.actors_export(rawactors,heads,nexts)
   assert rawactors.raw[:116*nactors]==bytes(uc.mem_read(ENTITIES,116*nactors)),(level,frame,'animation state')
   report['animated_frames']+=1
  assert lib.ssa_world_scene_clear(C.byref(s),0)==1;uc.mem_write(FRAME,bytes(14400))
  original();result=lib.ssa_world_scene_draw(C.byref(s),C.byref(c),frame,heading)
  assert result==1,(level,frame,'native scene error',s.visibility.wall_count,s.planes.count,s.planes.opening_cursor)
  rc=lib.actors_draw(C.byref(s),C.byref(c),frame);assert rc==1,(level,frame,'native actors',rc)
  q=lib.actors_queue().contents
  assert q.projection_count==get(0x03004750,'H') and q.actor_count==get(0x03003f66,'H'),(level,frame,'counts',q.projection_count,get(0x03004750,'H'))
  assert bytes(q.projections)==bytes(uc.mem_read(PROJECTIONS,32*36)),(level,frame,'projections')
  assert bytes(q.actors)==bytes(uc.mem_read(ACTORS,32*12)),(level,frame,'actor records')
  assert bytes(q.clip_words)==bytes(uc.mem_read(CLIPS,7684*2)),(level,frame,'clip words')
  assert q.clip_cursor==get(0x0300503c),(level,frame,'clip cursor')
  assert bytes(q.draw_order)==bytes(uc.mem_read(ORDER,64)),(level,frame,'draw order')
  report['actor_commands']+=q.draw_count
  expected=bytes(uc.mem_read(FRAME-120,14520))
  assert bytes(s.framebuffer)==expected,(level,frame,'pixels',[(i,s.framebuffer[i],b) for i,b in enumerate(expected) if s.framebuffer[i]!=b][:12])
  assert bytes(s.columns)==bytes(uc.mem_read(0x030050b4,480)),(level,frame,'columns')
  assert s.planes.count==(get(0x03004994)-PLANES)//252
  assert bytes(s.planes.records)==bytes(uc.mem_read(PLANES,80*252)),(level,frame,'plane bytes')
  assert s.planes.opening_cursor==get(0x03004c70)-OPENINGS,(level,frame,'opening cursor',s.planes.opening_cursor,hex(get(0x03004c70)),s.planes.count)
  assert bytes(s.openings[:s.planes.opening_cursor])==bytes(uc.mem_read(OPENINGS,s.planes.opening_cursor))
  assert bytes(s.plane_renderer.row_starts)==bytes(uc.mem_read(0x03004c74,480))
  assert (s.plane_renderer.sky_offset,s.plane_renderer.height_distance,s.plane_renderer.color)==(get(0x0300506c),get(0x03005034),get(0x03005038,'B'))
  assert s.visibility.head==get(0x03003f60,'H') and s.visibility.wall_count==get(0x03003f64,'H')
  linked=[];idx=s.visibility.head
  seen=set()
  while idx!=65535:
   assert idx not in seen;seen.add(idx)
   if idx&0x8000:
    idx=q.actors[idx&32767].next;continue
   assert idx not in linked;linked.append(idx);w=s.visibility.walls[idx]
   assert w.segment_flags==get(WALLS+idx*60+22,'H')
   li=struct.unpack_from('<H',data,m.segments+(w.segment_flags&32767)*16+4)[0]
   report['portal_commands']+=bool(struct.unpack_from('<H',data,m.lines+li*40+26)[0]&4)
   idx=w.next
  assert s.updating==0 and s.wall_renderer.updating==0 and s.plane_renderer.updating==0
  report['wall_commands']+=len(linked);report['planes']+=s.planes.count;report['opening_bytes']+=s.planes.opening_cursor
  report['whole_world_actor_frames']+=1;report['pixels_compared']+=14400
  report['frames'].append({'level':level,'frame':frame,'heading':heading,'walls':len(linked),'planes':s.planes.count,'sha256':hashlib.sha256(expected[120:]).hexdigest()})
  (B/f'actor-scene-rom-level-{level:02d}-frame-{frame}.indices').write_bytes(bytes(s.framebuffer[120:]))
 print('PASS complete original world and actor pixels level',level,flush=True)
 snapshot=bytes(s.framebuffer);s.updating=1
 assert lib.ssa_world_scene_draw(C.byref(s),C.byref(c),0,0)==-1 and bytes(s.framebuffer)==snapshot
 assert lib.ssa_world_scene_clear(C.byref(s),0)==-1 and lib.ssa_world_scene_free(C.byref(s))==-1
 report['guards']+=3;s.updating=0
 assert lib.actors_free()==1
 assert lib.ssa_world_scene_free(C.byref(s))==1;lib.ssa_map_texture_bindings_free(C.byref(binding));lib.ssa_map_assets_free(C.byref(m))
lib.ssa_texture_assets_free(C.byref(assets))
report['original_arm_columns']=raster_calls
report['source_sha256']={name:hashlib.sha256((ROOT/'src'/name).read_bytes()).hexdigest() for name in sources}
report['test_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
(ROOT/'analysis/native-actor-scene-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='frames'},indent=2),flush=True)

