"""Complete84-slot pool and101F4 HUD construction plus scalar HUD handlers.

Only BIOS CpuSet/DMA hardware memory copies are modeled. Native tests use actual
pool allocation, original object initialization/FC04, and strict sanitizer builds.
"""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
helper=B/'sprite_pool_fixture.c'
helper.write_text(r'''
#include "ssa/hud_sprites.h"
#include <string.h>
static SsaSpritePool pool;static SsaSpriteDispatch dispatch;static SsaWeaponObject objects[84];
void start(unsigned seed){unsigned i;memset(objects,(int)seed,sizeof(objects));memset(&dispatch,(int)seed,sizeof(dispatch));memset(&pool,0,sizeof(pool));
 for(i=0;i<84;++i)dispatch.objects[i]=objects+i;
 dispatch.current=objects+7;dispatch.updating=0;(void)ssa_sprite_pool_init(&pool,&dispatch);}
int operation(unsigned op,unsigned a,unsigned b){SsaWeaponObject*o;int rc;
 switch(op){case 0:rc=ssa_sprite_pool_allocate(&pool,&o);return rc==1?(int)(o-objects):rc==0?84:-1;
 case 1:return a<84?ssa_sprite_pool_release(&pool,objects+a):-1;
 case 2:return ssa_sprite_pool_mark(&pool);case 3:return ssa_sprite_pool_remove_types(&pool,(uint16_t)a,(uint16_t)b);
 case 4:return ssa_sprite_pool_reset(&pool);default:return -1;}}
void payload(unsigned i,const void*p){uint16_t slot=objects[i].slot,index=objects[i].active_index;memcpy(objects+i,p,48);objects[i].slot=slot;objects[i].active_index=index;}
void output(void*p){unsigned char*b=p;uint32_t state[7]={dispatch.active_count,pool.free_count,dispatch.counter,dispatch.submitted_count,dispatch.current_index,dispatch.state,(uint32_t)(dispatch.current-objects)};
 memcpy(b,objects,sizeof(objects));memcpy(b+4032,dispatch.active,336);memcpy(b+4368,pool.free_slots,336);memcpy(b+4704,dispatch.submitted,84);memcpy(b+4788,state,sizeof(state));}
int create(unsigned p,unsigned motion,unsigned level){return ssa_hud_sprites_create(&pool,p,motion,level);}
int step(void*o,SsaHudSpriteState*s){return ssa_hud_sprite_step(o,s);}
int transient(unsigned i,unsigned flags){return ssa_hud_transient_step(&pool,objects+i,(uint16_t)flags);}
int guards(void){SsaSpritePool p=pool;SsaSpriteDispatch d=dispatch;SsaWeaponObject*o=NULL;unsigned n=0;
#define BAD(x) do{if((x)!=-1)return -(int)(n+1);++n;}while(0)
 BAD(ssa_sprite_pool_allocate(NULL,&o));BAD(ssa_sprite_pool_allocate(&p,NULL));BAD(ssa_sprite_pool_release(&p,NULL));
 p.updating=1;BAD(ssa_sprite_pool_allocate(&p,&o));p=pool;p.free_count=85;BAD(ssa_sprite_pool_allocate(&p,&o));
 p=pool;p.dispatch=&d;d.objects[0]=NULL;BAD(ssa_sprite_pool_allocate(&p,&o));d=dispatch;d.objects[1]=d.objects[0];BAD(ssa_sprite_pool_allocate(&p,&o));
 d=dispatch;d.active_count=85;BAD(ssa_sprite_pool_allocate(&p,&o));d=dispatch;d.updating=1;BAD(ssa_sprite_pool_reset(&p));
 BAD(ssa_hud_sprites_create(&p,1,0,0));BAD(ssa_hud_sprites_create(&pool,0,0,0));BAD(ssa_hud_sprites_create(&pool,5,0,0));
 BAD(ssa_hud_sprites_create(&pool,1,1,0));BAD(ssa_hud_sprites_create(&pool,1,0,16));
 BAD(ssa_hud_sprite_step(NULL,NULL));BAD(ssa_hud_transient_step(NULL,NULL,0));return (int)n;
}
#ifdef SSA_TEST_MAIN
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char**argv){unsigned i,j;SsaWeaponObject*o;SsaHudSpriteState state={0};FILE*f;unsigned char*rom;
 assert(argc==2);f=fopen(argv[1],"rb");assert(f);rom=malloc(8388608);assert(rom);assert(fread(rom,1,8388608,f)==8388608);fclose(f);
 assert(ssa_hud_glyphs_load(&state,rom,8388608)==1);free(rom);state.current_weapon=3;state.health=100;state.ammo[2]=300;
 for(j=0;j<256;++j){start(j);for(i=0;i<84;++i){assert(ssa_sprite_pool_allocate(&pool,&o)==1);o->type=(uint16_t)(i%19);}
 assert(ssa_sprite_pool_allocate(&pool,&o)==0);assert(ssa_sprite_pool_remove_types(&pool,0,65535)==1);
 assert(ssa_hud_sprites_create(&pool,1,0,j%16)==74);for(i=0;i<74;++i)assert(ssa_hud_sprite_step(dispatch.objects[dispatch.active[i]],&state)==1);
 assert(ssa_sprite_pool_reset(&pool)==1);assert(guards()==16);}
 return 0;}
#endif
''')
sources=['sprite_pool.c','hud_sprites.c','weapon_animation.c']
base=['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-I',str(ROOT/'include'),str(helper),*[str(ROOT/'src'/s) for s in sources]]
subprocess.run(base+['-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-o',str(B/'libssa_sprite_pool.so')],check=True)
lib=C.CDLL(str(B/'libssa_sprite_pool.so'))
for name,args in {'start':[C.c_uint],'operation':[C.c_uint]*3,'payload':[C.c_uint,C.c_void_p],'output':[C.c_void_p],'create':[C.c_uint]*3,'step':[C.c_void_p,C.c_void_p],'transient':[C.c_uint]*2}.items():getattr(lib,name).argtypes=args
class Hud(C.Structure):_fields_=[('flags',C.c_uint16),('weapon',C.c_uint16),('weapon_flags',C.c_uint32),('action',C.c_uint32),('aim',C.c_int32),('value',C.c_uint8),('values',C.c_uint8*4),('skip',C.c_uint8)]
class Hud(C.Structure):
 _fields_=Hud._fields_+[(n,C.c_int16) for n in ('health','armor','boss_health')]+[('boss_present',C.c_uint8),('ammo',C.c_int16*10),('score',C.c_uint32),('time',C.c_uint32),('game_flags',C.c_uint16),('notice_timer',C.c_int16),('rectangle',C.c_uint8*4),('rectangle_tail',C.c_int32*4),('ammo_x',C.c_uint16*3),('ammo_image',C.c_uint16*3),('score_x',C.c_uint16*8),('score_image',C.c_uint16*8),('time_x',C.c_uint16*5),('time_image',C.c_uint16*5),('glyph_widths',C.c_uint8*128),('glyphs_loaded',C.c_uint)]
lib.ssa_hud_glyphs_load.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM);uc.mem_map(0x02000000,0x20000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0x04000000,0x1000)
uc.mem_write(0x030000f0,ROM[0x370:0x380]);OBJ,ACTIVE,FREE,GAME,PLAYER,LEVEL=0x02000000,0x02002000,0x02002400,0x02003000,0x02004000,0x02005000;STOP=0x087ff000
def put(a,f,*v):uc.mem_write(a,struct.pack('<'+f,*v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
def copy(src,dst,ctl):
 unit=4 if ctl&0x4000000 else 2;n=ctl&0x1fffff
 uc.mem_write(dst,bytes(uc.mem_read(src,unit))*n if ctl&0x1000000 else bytes(uc.mem_read(src,unit*n)))
def cp(m,a,n,c):copy(*[m.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)]);m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
def dma(m,access,a,n,v,c):
 if v&0x80000000:copy(get(0x040000d4),get(0x040000d8),v&0x7fffffff)
uc.hook_add(UC_HOOK_CODE,cp,begin=0x080c5320,end=0x080c5320);uc.hook_add(UC_HOOK_MEM_WRITE,dma,begin=0x040000dc,end=0x040000dc)
put(0x0300559c,'I',OBJ);put(0x030055c0,'I',ACTIVE);put(0x030055ac,'I',FREE);put(0x03004010,'I',GAME);put(0x03004648,'I',PLAYER);put(0x0300572c,'I',LEVEL)
def run(at,*args):
 for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(r,v&0xffffffff)
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1);uc.emu_start(at|1,STOP,count=1000000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC));return uc.reg_read(UC_ARM_REG_R0)
rng=random.Random(0xf9b4);out=C.create_string_buffer(4816)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'pool_calls':0,'hud_construction':0,'hud_handlers':0,'transient_handlers':0,'objects_compared':0,'native_guards':0,'scope':__doc__}
def compare(label):
 lib.output(out);b=out.raw
 expected=bytes(uc.mem_read(OBJ,4032))+bytes(uc.mem_read(ACTIVE,336))+bytes(uc.mem_read(FREE,336))+bytes(uc.mem_read(0x03005620,84))
 expected+=struct.pack('<7I',get(0x030055c4),get(0x030055c8),get(0x030055b0),get(0x03005674,'B'),get(0x030055a8),get(0x030055a4),(get(0x030055a0)-OBJ)//48)
 assert b==expected,(label,[(i,b[i],expected[i]) for i in range(len(b)) if b[i]!=expected[i]][:20])
 assert get(0x030055bc)==FREE+4*(84-get(0x030055c8)) and get(0x030055b4)==ACTIVE+4*get(0x030055c4)
 report['objects_compared']+=84
def reset(seed=0x5a,players=1):
 lib.start(seed);uc.mem_write(OBJ,bytes([seed])*4032);uc.mem_write(ACTIVE,bytes([seed])*336);uc.mem_write(FREE,bytes([seed])*336);uc.mem_write(0x03005620,bytes([seed])*84)
 put(GAME+16,'B',players);put(0x030055a0,'III',OBJ+7*48,seed*0x1010101,seed*0x1010101)
 run(0x0800fa78);compare(('reset',seed));report['pool_calls']+=1
def operation(op,a=0,b=0):
 at=(0x0800f9b4,0x0800fa08,0x0800fb5c,0x0800fba0,0x0800fa78)[op]
 expected=run(at,*((OBJ+a*48,) if op==1 else (a,b)));actual=lib.operation(op,a,b)
 if op==0:assert actual==((expected-OBJ)//48 if expected else 84)
 else:assert actual==1
 compare((op,a,b));report['pool_calls']+=1;return actual
def payload(slot):
 r=bytearray(rng.randbytes(48));r[8:12]=uc.mem_read(OBJ+slot*48+8,4);struct.pack_into('<H',r,4,rng.randrange(25))
 lib.payload(slot,bytes(r));uc.mem_write(OBJ+slot*48,bytes(r))
for group in range(32):
 reset(group,1+group%4)
 for i in range(84):payload(operation(0))
 operation(0)
 for i in range(256):
  count=get(0x030055c4);choice=rng.randrange(5)
  if choice==0 and count<84:payload(operation(0))
  elif choice==1 and count:operation(1,get(ACTIVE+4*rng.randrange(count)))
  elif choice==2:operation(2)
  elif choice==3:operation(3,rng.randrange(25),rng.randrange(25))
  else:operation(0) if count==84 else payload(operation(0))
 operation(3,0,65535);operation(4)
 assert lib.guards()==16;report['native_guards']+=16
print('PASS complete sprite pool',report['pool_calls'],flush=True)
for level in range(16):
 for players in range(1,5):
  for motion in range(players):
   need=74 if players==1 else 59+2*players
   for prefix in (0,1,84-need):
    reset((level*11+players)&255,players)
    for i in range(prefix):payload(operation(0))
    put(GAME+104,'B',motion);put(LEVEL+16,'H',level)
    run(0x080101f4);assert lib.create(players,motion,level)==need;compare(('HUD',level,players,motion,prefix));report['hud_construction']+=1
   before=out.raw;assert lib.create(players,motion,level)==-1;lib.output(out);assert out.raw==before;report['native_guards']+=1
print('PASS complete101F4 HUD construction',report['hud_construction'],flush=True)
handlers={4:0x0800f0b8,5:0x0800e700,7:0x0800f1c0,8:0x0800f27c,13:0x0800e9d4,14:0x0800f4a4,15:0x0800f5a4,16:0x0800f6c8,17:0x0800f724,18:0x0800f794}
for typ,at in handlers.items():
 for i in range(4096):
  r=bytearray(rng.randbytes(48));struct.pack_into('<HH',r,4,typ,i%4);struct.pack_into('<H',r,32,(0xd9,0xda,rng.randrange(233))[i%3])
  s=Hud(rng.randrange(65536),rng.choice((0,1,2,3,4,5,6,7,8,9,10,11,65535)),rng.getrandbits(32),rng.choice((0,1,2,8,9,65535)),rng.randrange(-2147483648,2147483648),i&255,(C.c_uint8*4)(*[rng.randrange(256) for _ in range(4)]),i%3)
  local=i%4;put(0x0300464e,'H',local);p=PLAYER+local*256
  s.health=rng.randrange(-32768,32768);s.armor=rng.randrange(-32768,32768);s.boss_health=rng.randrange(-32768,32768);s.boss_present=i%2
  s.game_flags=rng.randrange(65536);s.notice_timer=0;s.rectangle[:]=rng.randbytes(4);s.rectangle_tail[:]=[rng.randrange(-2147483648,2147483648) for _ in range(4)]
  if typ==15:struct.pack_into('<HH',r,6,i%3,struct.unpack_from('<H',r,8)[0]);struct.pack_into('<H',r,32,(0x55,0x54,0x57)[i%3])
  put(p+172,'I',0x02006000);put(0x02006032,'h',s.health);put(p+196,'h',s.armor);put(0x03005710,'I',0x02006100 if s.boss_present else 0);put(0x02006132,'h',s.boss_health)
  put(GAME+4,'H',s.game_flags);put(GAME+118,'h',s.notice_timer);uc.mem_write(GAME+110,bytes(s.rectangle));uc.mem_write(0x03006bcc,bytes(s.rectangle_tail))
  put(p+188,'H',s.flags);put(p+120,'H',s.weapon);put(p+40,'I',s.action);put(p+48,'I',s.weapon_flags);put(p+228,'i',s.aim)
  put(GAME+124,'B',s.value);uc.mem_write(GAME+132,bytes(s.values));put(0x030055b8,'B',s.skip)
  put(0x030055a0,'I',OBJ);uc.mem_write(OBJ,bytes(r));run(at)
  obj=C.create_string_buffer(bytes(r),48);assert lib.step(obj,C.byref(s))==1
  assert obj.raw==bytes(uc.mem_read(OBJ,48)),(typ,i,[(k,obj.raw[k],uc.mem_read(OBJ+k,1)[0]) for k in range(48) if obj.raw[k]!=uc.mem_read(OBJ+k,1)[0]])
  assert s.skip==get(0x030055b8,'B');report['hud_handlers']+=1
  assert s.game_flags==get(GAME+4,'H') and bytes(s.rectangle_tail)==bytes(uc.mem_read(0x03006bcc,16)),(typ,i,'state')
print('PASS scalar HUD controllers',report['hud_handlers'],flush=True)
report['numeric_handlers']=0
for typ,at,n,xaddr,iaddr in ((6,0x0800e7a4,3,0x03006aa8,0x03006ab0),(9,0x0800ecf8,8,0x03006b70,0x03006b80),(12,0x0800f344,5,0x03006bb0,0x03006bc0)):
 s=Hud();assert lib.ssa_hud_glyphs_load(C.byref(s),ROM,len(ROM))==1
 for case in range(1024):
  s.flags=rng.choice((0,4,8,65535));s.weapon=1+case%10;s.ammo[:]=[rng.randrange(1000) for _ in range(10)];s.score=rng.randrange(100000000);s.time=rng.randrange(360000)
  name='ammo' if typ==6 else 'score' if typ==9 else 'time'
  xs=getattr(s,name+'_x');images=getattr(s,name+'_image')
  if case%8==0:
   xs[:]=[rng.randrange(65536) for _ in range(n)];images[:]=[rng.randrange(234) for _ in range(n)]
  uc.mem_write(xaddr,bytes(xs));uc.mem_write(iaddr,bytes(images));put(0x0300464e,'H',0);put(PLAYER+188,'H',s.flags);put(PLAYER+120,'H',s.weapon);uc.mem_write(PLAYER+54,bytes(s.ammo));put(GAME+136,'I',s.score);put(GAME+12,'I',s.time)
  for subtype in range(n):
   r=bytearray(rng.randbytes(48));struct.pack_into('<HH',r,4,typ,subtype);put(0x030055a0,'I',OBJ);uc.mem_write(OBJ,bytes(r));run(at)
   obj=C.create_string_buffer(bytes(r),48);rc=lib.step(obj,C.byref(s));assert rc==1,(typ,case,subtype,rc)
   assert obj.raw==bytes(uc.mem_read(OBJ,48)),(typ,case,subtype,'object',[(k,obj.raw[k],uc.mem_read(OBJ+k,1)[0]) for k in range(48) if obj.raw[k]!=uc.mem_read(OBJ+k,1)[0]])
   assert bytes(xs)==bytes(uc.mem_read(xaddr,n*2)) and bytes(images)==bytes(uc.mem_read(iaddr,n*2)),(typ,case,subtype,'scratch',list(xs),list(images),bytes(uc.mem_read(xaddr,n*2)).hex(),bytes(uc.mem_read(iaddr,n*2)).hex())
   assert bytes(s.ammo)==bytes(uc.mem_read(PLAYER+54,20));report['numeric_handlers']+=1
print('PASS shared numeric HUD controllers',report['numeric_handlers'],flush=True)
for typ,subtype,field,value in ((6,2,'weapon',0),(6,2,'weapon',11),(9,7,'score',100000000),(12,4,'time',360000),(15,3,'health',0),(17,4,'skip',0)):
 r=bytearray(48);struct.pack_into('<HH',r,4,typ,subtype);obj=C.create_string_buffer(bytes(r),48);s=Hud();s.weapon=3;s.glyphs_loaded=1;setattr(s,field,value)
 assert lib.step(obj,C.byref(s))==-1;report['native_guards']+=1
s=Hud();s.notice_timer=1;r=bytearray(rng.randbytes(48));struct.pack_into('<H',r,4,13);obj=C.create_string_buffer(bytes(r),48);before=bytes(s)
assert lib.step(obj,C.byref(s))==0 and obj.raw==bytes(r) and bytes(s)==before;report['native_guards']+=1
for i in range(1024):
 reset(i&255,1+i%4);slots=[operation(0) for _ in range(3)];slot=slots[i%3];payload(slot)
 r=bytearray(uc.mem_read(OBJ+slot*48,48));struct.pack_into('<H',r,4,3);r[38]=i%4;uc.mem_write(OBJ+slot*48,bytes(r));lib.payload(slot,bytes(r))
 local=i%4;flags=rng.randrange(65536);put(PLAYER+local*256+188,'H',flags);put(0x0300464e,'H',local);put(0x030055a0,'I',OBJ+slot*48)
 # Mirror the caller's current pointer in the helper's projection.
 # F100/FA08 do not change this pointer; compare object/pool bytes separately.
 run(0x0800f100);assert lib.transient(slot,flags)==1;lib.output(out)
 assert out.raw[:4788]==bytes(uc.mem_read(OBJ,4032))+bytes(uc.mem_read(ACTIVE,336))+bytes(uc.mem_read(FREE,336))+bytes(uc.mem_read(0x03005620,84));report['transient_handlers']+=1
exe=B/'test_sprite_pool_sanitized'
subprocess.run(base+['-DSSA_TEST_MAIN','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-o',str(exe)],check=True)
subprocess.run(['env','ASAN_OPTIONS=detect_leaks=0',str(exe),str(ROOT/'orig/SSA.gba')],check=True)
report['asan_ubsan_native_cycles']=256
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/sprite_pool.c','include/ssa/sprite_pool.h','src/hud_sprites.c','include/ssa/hud_sprites.h','tests/test_sprite_pool_rom.py']}
(ROOT/'analysis/native-sprite-pool-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
