"""Whole D490/D884 and D2D0/D338 using actual actor metadata/math.

Isolated projection cases control only the1238 queue-insertion boundary;
collection cases execute native/original1238 with a genuine empty wall list.
BIOS divide is the only modeled hardware service. Unicorn is test-only.
"""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_INTR
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
helper=B/'actor_projection_fixture.c'
helper.write_text(r'''
#include "ssa/actor_projection.h"
#include "ssa/spawn.h"
#include <string.h>
static SsaActorAssets assets;static SsaMathTables math;static SsaMapAssets map;
static SsaWorldVisibility vis;static SsaActorQueue queue;static SsaRenderCamera camera;
static SsaActorProjector projector;static SsaEntity entities[100];static SsaActorSource sources[100];
static SsaWorld world;static uint16_t visited[4096];static unsigned calls,controlled;
static SsaSpawnDefinitions definitions;
static unsigned u16(const unsigned char*p){return p[0]|(unsigned)p[1]<<8;}
static int s16(const unsigned char*p){unsigned u=u16(p);return u<32768?(int)u:(int)u-65536;}
static int32_t s32(const unsigned char*p){uint32_t u=(uint32_t)u16(p)|(uint32_t)u16(p+2)<<16;return u<=2147483647?(int32_t)u:-1-(int32_t)~u;}
int __real_ssa_actor_queue_insert(SsaActorQueue*,unsigned);
int __wrap_ssa_actor_queue_insert(SsaActorQueue*q,unsigned i){++calls;return controlled?1:__real_ssa_actor_queue_insert(q,i);}
int setup(const void*rom,size_t n){return ssa_math_tables_load(&math,rom,n)==1 && ssa_actor_assets_load(&assets,rom,n)==1 && ssa_spawn_definitions_load(&definitions,rom,n)==1;}
int level(const void*rom,size_t n,unsigned l){ssa_world_free(&world);ssa_map_assets_free(&map);if(ssa_map_assets_load(&map,rom,n,l)!=1)return -1;return ssa_world_init(&world,&map,100);}
void reset(const unsigned char*c,unsigned flags,unsigned pc,unsigned ac,unsigned seed,int control){
 memset(&queue,(int)seed,sizeof(queue));memset(&vis,0,sizeof(vis));
 vis.map=&map;vis.head=vis.tail=65535;vis.sector_visited=visited;vis.sector_words=((size_t)map.sector_count+15)/16;
 queue.visibility=&vis;queue.projection_count=(uint16_t)pc;queue.actor_count=(uint16_t)ac;queue.clip_cursor=0;queue.updating=0;
 camera.x=s32(c);camera.y=s32(c+4);camera.z=s32(c+8);camera.sine=s32(c+16);camera.cosine=s32(c+20);
 projector=(SsaActorProjector){&assets,&math,&camera,&queue,flags,0};calls=0;controlled=(unsigned)control;
}
void entity(unsigned i,const unsigned char*r){SsaEntity*e=&entities[i];memset(e,0,sizeof(*e));
 e->body.x=s32(r);e->body.y=s32(r+4);e->body.z=s32(r+8);e->body.height=s32(r+44);
 e->body.flags=(uint16_t)u16(r+48);e->body.step_adjust=(int8_t)r[83];e->body.floor=(int16_t)s16(r+114);
 sources[i]=(SsaActorSource){e,(uint16_t)u16(r+88),(uint16_t)u16(r+90),(uint16_t)u16(r+48),r[63],r[84]};
}
int one(const unsigned char*r,int fixed){entity(0,r);return ssa_actor_project(&projector,sources,fixed);}
void output(unsigned char*out){memcpy(out,queue.projections,1152);memcpy(out+1152,queue.actors,384);memcpy(out+1536,queue.clip_words,sizeof(queue.clip_words));
 uint32_t v[7]={queue.projection_count,queue.actor_count,calls,queue.clip_cursor,vis.head,vis.tail,projector.updating};memcpy(out+1536+sizeof(queue.clip_words),v,sizeof(v));}
int populate(const unsigned char*raw,unsigned n,const unsigned char*bits){unsigned i;ssa_world_free(&world);if(ssa_world_init(&world,&map,100)!=1)return -1;
 memcpy(visited,bits,vis.sector_words*2);for(i=0;i<n;++i){entity(i,raw+116*i);if(ssa_world_insert(&world,&entities[i])!=1)return -1;}return 1;}
int sector_order(int16_t*out){unsigned s,i;for(s=0;s<map.sector_count;++s){const SsaEntity*e=world.sectors[s];out[s*101]=-1;
 for(i=0;e;e=e->sector_next,++i){out[s*101+i]=(int16_t)(e-entities);out[s*101+i+1]=-1;}}return map.sector_count;}
int collect(unsigned n){return ssa_actor_collect(&projector,&world,sources,n);}
int start(int32_t*out){SsaPlayerStart s;if(ssa_map_player_start(&map,&s)!=1)return -1;out[0]=s.x;out[1]=s.y;out[2]=s.z;out[3]=s.heading;return 1;}
void trig(unsigned h,int32_t*out){ssa_sincos(&math,(uint16_t)h,out,out+1);}
static void w16(unsigned char*p,unsigned n){p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);}
static void w32(unsigned char*p,uint32_t n){w16(p,n);w16(p+2,n>>16);}
int map_objects(unsigned char*out,unsigned l){SsaSpawnPlan plan={0};unsigned i;
 if(ssa_spawn_plan_load(&plan,&map,&definitions,l,1,100)!=1)return -1;
 for(i=0;i<plan.object_count;++i){const SsaSpawnObject*o=plan.objects+i;unsigned char*r=out+i*116;memset(r,0,116);
 w32(r,(uint32_t)o->entity.body.x);w32(r+4,(uint32_t)o->entity.body.y);w32(r+8,(uint32_t)o->entity.body.z);
 w32(r+44,(uint32_t)o->entity.body.height);w16(r+48,o->entity.body.flags);r[63]=o->facing;
 r[83]=(unsigned char)o->entity.body.step_adjust;r[84]=o->entity.player_index;w16(r+88,o->animation);w16(r+114,(unsigned)o->entity.body.floor);}
 i=(unsigned)plan.object_count;ssa_spawn_plan_free(&plan);return (int)i;}
int guards(void){SsaActorProjector p=projector;SsaActorQueue q=queue;SsaActorSource s=sources[0];unsigned n=0;
#define BAD(expression) do{if((expression)!=-1)return -(int)(n+1);++n;}while(0)
 BAD(ssa_actor_project(NULL,&s,0));BAD(ssa_actor_project(&p,NULL,0));s.entity=NULL;BAD(ssa_actor_project(&p,&s,0));s=sources[0];
 p.updating=1;BAD(ssa_actor_project(&p,&s,0));p=projector;p.assets=NULL;BAD(ssa_actor_project(&p,&s,0));
 p=projector;p.math=NULL;BAD(ssa_actor_project(&p,&s,0));p=projector;p.camera=NULL;BAD(ssa_actor_project(&p,&s,0));
 p=projector;p.queue=NULL;BAD(ssa_actor_project(&p,&s,0));p=projector;p.queue=&q;q.updating=1;BAD(ssa_actor_project(&p,&s,0));
 q=queue;q.projection_count=33;BAD(ssa_actor_project(&p,&s,0));q=queue;q.actor_count=33;BAD(ssa_actor_project(&p,&s,0));
 q=queue;q.clip_cursor=SSA_ACTOR_CLIP_WORDS+1;BAD(ssa_actor_project(&p,&s,0));q=queue;q.visibility=NULL;BAD(ssa_actor_project(&p,&s,0));
 q=queue;q.projection_count=0;s.animation=142;s.frame=s.flags=0;BAD(ssa_actor_project(&p,&s,1));
 BAD(ssa_actor_collect(&projector,NULL,sources,1));BAD(ssa_actor_collect(&projector,&world,NULL,1));
 {SsaActorSource two[2]={sources[0],sources[0]};BAD(ssa_actor_collect(&projector,&world,two,2));}
 world.query_active=1;BAD(ssa_actor_collect(&projector,&world,sources,1));world.query_active=0;
 return (int)n;
}
''')
sources=['actor_projection.c','actor_assets.c','actor_queue.c','actor_raster.c','movement.c','map_assets.c','world.c','geometry.c','physics.c','slide.c','horizontal.c','spawn.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-Wl,--wrap=ssa_actor_queue_insert','-I',str(ROOT/'include'),str(helper),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_actor_projection.so')],check=True)
lib=C.CDLL(str(B/'libssa_actor_projection.so'))
for name,args in {'setup':[C.c_void_p,C.c_size_t],'level':[C.c_void_p,C.c_size_t,C.c_uint],'reset':[C.c_void_p]+[C.c_uint]*5,'one':[C.c_void_p,C.c_int],'output':[C.c_void_p],'populate':[C.c_void_p,C.c_uint,C.c_void_p],'sector_order':[C.c_void_p],'collect':[C.c_uint],'start':[C.c_void_p],'trig':[C.c_uint,C.c_void_p],'map_objects':[C.c_void_p,C.c_uint]}.items():getattr(lib,name).argtypes=args
assert lib.setup(ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM);uc.mem_map(0x02000000,0x200000);uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
ENTITY,PROJ,ACTORS,CLIPS,FRAMES,GAME,HEADS,NODES,VIS,MAP=range(0x02000000,0x020a0000,0x10000)
STOP=0x087ff000;controlled=True;calls=[]
def put(a,f,*v):uc.mem_write(a,struct.pack('<'+f,*v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
def insertion(m,a,n,c):
 calls.append(m.reg_read(UC_ARM_REG_R0))
 if controlled:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,insertion,begin=0x08001238,end=0x08001238)
def bios(m,n,c):
 a,b=[C.c_int32(m.reg_read(r)).value for r in (UC_ARM_REG_R0,UC_ARM_REG_R1)];assert b
 q=abs(a)//abs(b)*(-1 if (a<0)!=(b<0) else 1)
 m.reg_write(UC_ARM_REG_R0,q&0xffffffff);m.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff);m.reg_write(UC_ARM_REG_R3,abs(q))
uc.hook_add(UC_HOOK_INTR,bios)
resources=[];stride=[]
for i in range(142):
 table,anim,dirs,flags=struct.unpack_from('<IIHH',ROM,0x449d2c+12*i)
 count=struct.unpack_from('<I',ROM,table-0x08000000)[0] if table else 0
 frames=count//dirs//flags.bit_count() if count else 0
 resources.append((count,dirs,flags,frames));stride.append(frames&255)
uc.mem_write(FRAMES,bytes(stride));put(0x03003f38,'I',FRAMES);put(0x03004010,'I',GAME)
put(0x03004754,'I',PROJ);put(0x03003f6c,'I',ACTORS);put(0x03003f70,'I',CLIPS);put(0x03003f74,'I',CLIPS)
put(0x03005040,'I',CLIPS);put(0x03005044,'I',CLIPS)
put(0x03004644,'I',ENTITY);put(0x03004984,'I',HEADS);put(0x03004988,'I',VIS)
put(0x03004974+4,'I',6);put(0x03004974+12,'I',NODES)
rng=random.Random(0xd490);out=C.create_string_buffer(1536+(32*120*2+4)*2+28)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'billboard':0,'fixed_screen':0,'collection':0,'actual_map_object_collection':0,'actual_map_objects':0,'emitted':0,'native_guards':0,'maps':16,'scope':__doc__}
def run(at,r0=ENTITY):
 for r,v in ((UC_ARM_REG_R0,r0),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(r,v)
 uc.emu_start(at|1,STOP,count=2000000);assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
def camera(x=0,y=0,z=48*65536,heading=0):
 trig=(C.c_int32*2)();lib.trig((-heading)&65535,trig)
 return struct.pack('<iiiHhii',x,y,z,heading,0,*trig)
def reset(c,flags=0,pc=0,ac=0,seed=0x5a,control=True):
 global controlled
 controlled=control;calls.clear();lib.reset(c,flags,pc,ac,seed,control)
 uc.mem_write(0x03004020,c);put(GAME,'I',flags);put(0x03004750,'H',pc);put(0x03003f60,'HHHH',65535,65535,0,ac)
 uc.mem_write(PROJ,bytes([seed])*1152);uc.mem_write(ACTORS,bytes([seed])*384);uc.mem_write(CLIPS,bytes([seed])*(32*120*2+4)*2);put(0x0300503c,'I',0)
def compare(label,initial=0):
 lib.output(out);data=out.raw;state=struct.unpack_from('<7I',data,1536+(32*120*2+4)*2)
 assert state[:3]==(get(0x03004750,'H'),get(0x03003f66,'H'),len(calls)),(label,state,calls)
 expected=bytes(uc.mem_read(PROJ,1152));assert data[:1152]==expected,(label,'projection',[(i,data[i],expected[i]) for i in range(1152) if data[i]!=expected[i]][:20])
 expected=bytes(uc.mem_read(ACTORS,384));assert data[1152:1536]==expected,(label,'actors',data[1152:1164].hex(),expected[:12].hex())
 if not controlled:
  assert state[3:6]==(get(0x0300503c),get(0x03003f60,'H'),get(0x03003f62,'H')),(label,state)
  assert data[1536:1536+(32*120*2+4)*2]==bytes(uc.mem_read(CLIPS,(32*120*2+4)*2)),(label,'clips')
 assert state[6]==0;report['emitted']+=state[0]-initial
def raw(animation,frame=0,x=0,y=128*65536,z=0,facing=0,flags=0,owner=0):
 r=bytearray(rng.randbytes(116));struct.pack_into('<iii',r,0,x,y,z);struct.pack_into('<iH',r,44,64*65536,flags)
 r[63]=facing&255;r[83]=0;r[84]=owner;struct.pack_into('<HH',r,88,animation,frame);struct.pack_into('<h',r,114,0);return r
assert lib.level(ROM,len(ROM),0)==1
for anim,(count,dirs,flags,frames) in enumerate(resources):
 if not count:continue
 for n in range(128):
  fixed=n%8==0;depth=(0,1,2,4,8,16,32,48,64,96,128,192,256,512,1024,8192)[n%16]
  r=raw(anim,n%max(frames,1),x=rng.randrange(-128,129)*65536,y=depth*65536,z=rng.randrange(-32,33)*65536,facing=(n*19)&255,flags=(0,16,64,128)[n%4],owner=n%4)
  r[83]=(0,1,127,255)[n%4];struct.pack_into('<h',r,114,rng.randrange(-32,33))
  pc=(0,1,31,32)[n%4] if n>=120 else 0;ac=(0,31,32)[n%3] if n>=120 else 0
  reset(camera(heading=(0,8192,16384,32768)[n%4]),n%2,pc,ac)
  uc.mem_write(ENTITY,bytes(r));run(0x0800d884 if fixed else 0x0800d490)
  rc=lib.one(bytes(r),fixed)
  assert rc==1,(anim,n,rc)
  assert bytes(uc.mem_read(ENTITY,116))==bytes(r);compare((anim,n,fixed),pc)
  report['fixed_screen' if fixed else 'billboard']+=1
print('PASS actor individual projections',report,flush=True)
for level in range(16):
 assert lib.level(ROM,len(ROM),level)==1
 start=(C.c_int32*4)();assert lib.start(start)==1
 map_raw=C.create_string_buffer(11600);map_n=lib.map_objects(map_raw,level);assert 0<=map_n<=100
 report['actual_map_objects']+=map_n
 for view in range(16):
  reset(camera(start[0],start[1],start[2]+48*65536,(start[3]+view*8192)&65535),control=False)
  # Genuine native sector insertion, actual start position, explicit resource inputs.
  rs=[raw(4+(i*7+level)%138,x=start[0],y=start[1],z=start[2],facing=i*7,flags=16 if i%7==0 else (0x2000 if i%11==0 else 0),owner=i%4) for i in range(40)]
  # Flag10 secondary resource must also be an actual resource.
  for r in rs:
   if struct.unpack_from('<H',r,48)[0]&16:struct.pack_into('<H',r,88,4)
  if view>=8:rs=[bytearray(map_raw.raw[i*116:(i+1)*116]) for i in range(map_n)]
  packed=b''.join(bytes(r) for r in rs);bits=bytes([255])*8192
  assert lib.populate(packed,len(rs),bits)==1
  order=(C.c_int16*(4096*101))();sectors=lib.sector_order(order);assert sectors<=4096
  uc.mem_write(ENTITY,packed);uc.mem_write(VIS,bits);put(MAP+10,'H',sectors);uc.mem_write(HEADS,bytes([255])*sectors*2)
  for sector in range(sectors):
   ids=[]
   for j in range(100):
    index=order[sector*101+j]
    if index<0:break
    ids.append(index)
   if ids:put(HEADS+2*sector,'H',ids[0])
   for j,index in enumerate(ids):put(NODES+index*6,'HHH',65535,ids[j+1] if j+1<len(ids) else 65535,index)
  run(0x0800d2d0,MAP);assert lib.collect(len(rs))==1,(level,view)
  assert bytes(uc.mem_read(ENTITY,len(packed)))==packed;compare(('collection',level,view));report['collection']+=1
  if view>=8:report['actual_map_object_collection']+=1
  g=lib.guards();assert g==18,g;report['native_guards']+=g
 print('PASS collection level',level,flush=True)
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/actor_projection.c','include/ssa/actor_projection.h','src/actor_queue.c','src/actor_assets.c','tests/test_actor_projection_rom.py']}
(ROOT/'analysis/native-actor-projection-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))


