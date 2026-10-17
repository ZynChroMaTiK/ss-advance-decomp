"""Complete explosion -> actual damage -> actual reward, on all 16 maps."""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world declarations','exec'))
nodes=[]
for n in ast.parse((ROOT/'tests/test_combat_damage_rom.py').read_text()).body:
 if isinstance(n,ast.ClassDef): nodes.append(n)
 elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='EVENT' for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'damage declarations','exec'))
class Explosion(C.Structure): _fields_=[('updating',C.c_uint)]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
shim=BUILD/'combat_combined_shim.c'
shim.write_text('''#include "ssa/combat_explosion.h"
#include "ssa/combat_damage.h"
#include "ssa/combat_rewards.h"
typedef struct Combined {
 SsaCombatDamage *game; SsaCombatDamageTarget *targets; size_t count;
 uint32_t *score; const SsaCombatDamageOps *outer;
} Combined;
static int effect(void *p, const SsaCombatDamageTarget *t, SsaCombatDamageEvent k,
 uint32_t a,uint32_t b) {
 Combined *c=p;
 if(k==SSA_COMBAT_DAMAGE_REWARD) return ssa_combat_reward(t->entity->type,c->score);
 return c->outer->event(c->outer->context,t,k,a,b);
}
static int damage(void *p,SsaEntity *entity,uint32_t amount) {
 Combined *c=p; size_t i; SsaCombatDamageOps ops={effect,c};
 for(i=0;i<c->count;++i) if(c->targets[i].entity==entity)
  return ssa_combat_damage(c->game,&c->targets[i],amount,&ops);
 return -1;
}
int combined(SsaCombatExplosion *state,const SsaMapAssets *map,SsaEntity *origin,
 SsaEntity *const *ordered,size_t count,uint32_t radius,uint32_t amount,int include,
 SsaCombatDamage *game,SsaCombatDamageTarget *targets,size_t target_count,
 uint32_t *score,const SsaCombatDamageOps *outer) {
 Combined c={game,targets,target_count,score,outer}; SsaCombatExplosionOps ops={damage,&c};
 return ssa_combat_explode(state,map,origin,ordered,count,game->player_count,radius,
  amount,include,&ops);
}
''')
sources=['combat_explosion.c','combat_damage.c','combat_rewards.c','enemy_targeting.c','map_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(shim),*[str(ROOT/'src'/f) for f in sources],'-o',str(BUILD/'libssa_combat_combined.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_combat_combined.so'))
lib.combined.argtypes=[C.POINTER(Explosion),C.POINTER(Map),EP,C.POINTER(EP),C.c_size_t,C.c_uint32,C.c_uint32,C.c_int,C.POINTER(Game),C.POINTER(Target),C.c_size_t,C.POINTER(C.c_uint32),C.POINTER(Ops)]
lib.ssa_map_assets_load.argtypes=[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint]
lib.ssa_map_assets_free.argtypes=[C.POINTER(Map)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
BASE,POOL,ORIGIN,SOURCE,OWNER,G,P,SET,STOP=0x02000000,0x02080000,0x02090000,0x02090100,0x02090200,0x020a0000,0x020a1000,0x020a2000,0x087ff000
def put(a,fmt,*v): uc.mem_write(a,struct.pack('<'+fmt,*v))
def pointer_at(array,i,typ): return C.cast(C.byref(array,i*C.sizeof(typ)),C.POINTER(typ))
def snapshot(): return tuple(bytes(uc.mem_read(a,n)) for a,n in ((POOL,100*0x74),(G,0x200),(P,0x400),(SET,0x60),(0x030000e0,4)))
events=[]; requests=[]; rewards=[]
def hook(machine,address,size,ctx):
 if address==0x080037f4:
  requests.append((machine.reg_read(UC_ARM_REG_R0),machine.reg_read(UC_ARM_REG_R1))); return
 if address==0x080125e0:
  rewards.append(machine.reg_read(UC_ARM_REG_R0)); return
 kind=0 if address==0x08005850 else 1
 events.append((kind,machine.reg_read(UC_ARM_REG_R0),machine.reg_read(UC_ARM_REG_R1) if kind==0 else 0,snapshot()))
 machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for address in (0x080037f4,0x080125e0,0x08005850,0x08008d5c): uc.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(0x35a837f4)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':0,'damage_calls':0,'reward_calls':0,'network_events':0,'sound_events':0,'distinct_current_source_cases':0,'zero_radius_cases':0,'maps':[], 'scope':'Whole035A8 -> real037F4 -> real125E0. Only network5850 and sound8D5C mocked; damage/reward entry hooks observe without replacing instructions. Native C composition shim. Full pool/game/player/settings snapshots and ordered network/sound callback snapshots. All16 actual reject tables, UBSan.'}
damages=[0,1,3,10,100,500,32767,32768,65535,65536,0x7fffffff,0x80000000,0xffffffff]
for level in range(16):
 m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
 map_bytes=C.string_at(m.data,m.size); uc.mem_write(BASE,map_bytes); put(BASE+0x40,'I',BASE+m.reject)
 for frame in range(256):
  raw=bytearray(rng.randbytes(100*0x74)); gr=bytearray(rng.randbytes(0x200)); pr=bytearray(rng.randbytes(0x400)); settings=bytearray(rng.randbytes(0x60))
  entities=(Entity*100)(); health=(C.c_int16*100)(); timers=(C.c_uint16*100)(); inventory=(Inventory*4)(); targets=(Target*100)(); ordered=(EP*100)()
  origin,source,owner=Entity(),Entity(),Entity(); game=Game(); state=Explosion(); score=C.c_uint32(rng.getrandbits(32)); shot=C.c_uint32(rng.getrandbits(32)); kills=C.c_uint8(rng.randrange(256))
  for inv in inventory: inv.stat_c4=rng.choice([-32768,-1,0,1,50,100,32767]); inv.stat_c6=rng.choice([-32768,-1,0,1,50,100,32767])
  for i,e in enumerate(entities):
   e.type=rng.choice([0,0x67,0xcc,0xcd,0xce,0xcf,0xd6,0xd7,0xd8,0xd9,0xe1])
   e.body.flags=rng.choice([0,0,0x10,0x20,0x800,0x810,0x811,0x830])
   e.player_index=rng.randrange(4); e.sector=rng.randrange(m.sector_count)
   e.body.x=rng.randrange(-100,101)*65536; e.body.y=rng.randrange(-100,101)*65536
   health[i]=rng.choice([-32768,-1,0,1,10,50,100,32767]); timers[i]=rng.randrange(65536)
   targets[i]=Target(C.pointer(e),pointer_at(health,i,C.c_int16),pointer_at(timers,i,C.c_uint16),C.pointer(inventory[e.player_index]))
  for e in (origin,source,owner):
   e.type=rng.choice([0x67,0xcd,0xd6,0xd7,0xd8,0xe1]); e.body.flags=rng.choice([0,0x10]); e.player_index=rng.randrange(4)
   e.sector=rng.randrange(m.sector_count); e.body.x=rng.randrange(-50,51)*65536; e.body.y=rng.randrange(-50,51)*65536
  game.player_count=(0,1,1,2,4)[frame%5]; game.winner=1 if frame%29==0 else 0
  game.difficulty=(0,1,2,3,255)[(frame//5)%5]; game.god_mode=1 if frame%13==0 else 0
  game.credit_player=(0,1,2,4)[(frame//4)%4]; game.credited_kills=C.pointer(kills)
  game.kill_limit=rng.choice([0,1,2,255]); game.local_player=rng.randrange(4); game.local_flags=8 if frame%7==0 else 0
  game.frame_counter=rng.getrandbits(32); game.hurt_flash=9 if frame%11==0 else 0
  shotindex=frame%11
  if shotindex:game.shot_hit_count=C.pointer(shot)
  for i in range(12): game.enemy_hits[i]=rng.getrandbits(32); game.enemy_kills[i]=rng.getrandbits(32)
  # Original explosion origin and global damage source can differ. Use an active
  # body for some origins to cover identity exclusion and live source flags.
  origin_index=frame%100 if frame%8==0 else None
  originp=C.pointer(origin) if origin_index is None else C.pointer(entities[origin_index])
  origin_addr=ORIGIN if origin_index is None else POOL+origin_index*0x74
  source_mode=(frame//2)%4
  if source_mode==0:game.source=originp; source_addr=origin_addr
  elif source_mode==1 or game.player_count<=1:game.source=C.pointer(source); source_addr=SOURCE
  elif source_mode==2:game.source=C.pointer(owner); source_addr=OWNER
  else:source_addr=0
  if frame%3:game.source_owner=C.pointer(owner)
  count=frame%101; indices=rng.sample(range(100),count)
  for j,i in enumerate(indices):ordered[j]=C.pointer(entities[i])
  radius=0 if frame%4==0 else rng.choice([1,100,10000,40000,0x100000,0x7fffffff,0xffffffff])
  damage=rng.choice(damages); include=frame%3
  def project():
   a,b,c,d=bytearray(raw),bytearray(gr),bytearray(pr),bytearray(settings)
   for i,e in enumerate(entities):
    for off,fmt,val in ((0,'i',e.body.x),(4,'i',e.body.y),(0x30,'H',e.body.flags),(0x32,'h',health[i]),(0x3c,'H',e.type),(0x4e,'H',timers[i]),(0x54,'B',e.player_index),(0x6a,'h',e.sector)):
     struct.pack_into('<'+fmt,a,i*0x74+off,val)
    if origin_addr==POOL+i*0x74 and source_addr==origin_addr:struct.pack_into('<I',a,i*0x74+0x20,OWNER if game.source_owner else 0)
   for off,fmt,val in ((0,'I',game.frame_counter),(0x10,'B',game.player_count),(0x7d,'B',game.winner),(0x88,'I',score.value)):
    struct.pack_into('<'+fmt,b,off,val)
   if game.credit_player:struct.pack_into('<B',b,0x83+game.credit_player,kills.value)
   if shotindex:struct.pack_into('<I',b,0x9c+(shotindex-1)*16,shot.value)
   for i in range(12):struct.pack_into('<II',b,0x134+i*12,game.enemy_hits[i],game.enemy_kills[i])
   struct.pack_into('<H',c,game.local_player*256+0xbc,game.local_flags)
   for i,inv in enumerate(inventory):struct.pack_into('<hh',c,i*256+0xc4,inv.stat_c4,inv.stat_c6)
   for off,value in ((0x1a,game.difficulty),(0x26,game.god_mode),(0x3c,game.kill_limit)):d[off]=value
   return bytes(a),bytes(b),bytes(c),bytes(d),struct.pack('<I',game.hurt_flash)
  for addr,data in zip((POOL,G,P,SET,0x030000e0),project()):uc.mem_write(addr,data)
  for addr,e in ((ORIGIN,origin),(SOURCE,source),(OWNER,owner)):
   raw_source=bytearray(0x74)
   for off,fmt,val in ((0,'i',e.body.x),(4,'i',e.body.y),(0x20,'I',OWNER if game.source_owner and addr==source_addr else 0),(0x30,'H',e.body.flags),(0x3c,'H',e.type),(0x54,'B',e.player_index),(0x6a,'h',e.sector)):
    struct.pack_into('<'+fmt,raw_source,off,val)
   uc.mem_write(addr,bytes(raw_source))
  for addr,value in ((0x03004644,POOL),(0x03004930,BASE),(0x03004010,G),(0x0300572c,SET),(0x03004648,P),(0x0300443c,source_addr),(0x03003f4c,shotindex),(0x03003f40,game.credit_player)):put(addr,'I',value)
  put(0x03004354,'H',count); put(0x0300464e,'H',game.local_player)
  if count:uc.mem_write(0x03004470,struct.pack('<'+'H'*count,*indices))
  events.clear(); requests.clear(); rewards.clear()
  for reg,value in ((UC_ARM_REG_R0,origin_addr),(UC_ARM_REG_R1,radius),(UC_ARM_REG_R2,damage),(UC_ARM_REG_R3,include),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,value)
  uc.emu_start(0x080035a9,STOP,count=300000); assert uc.reg_read(UC_ARM_REG_PC)==STOP
  expected=snapshot(); expected_events=list(events); native_events=[]
  @EVENT
  def event(ctx,target,kind,a,b):
   native_events.append((kind,a,b,project())); return 0
  ops=Ops(event,None)
  assert lib.combined(C.byref(state),C.byref(m),originp,ordered,count,radius,damage,include,C.byref(game),targets,100,C.byref(score),C.byref(ops))==0
  actual=project()
  assert actual==expected,(level,frame,radius,damage,[(j,[(i,x,y) for i,(x,y) in enumerate(zip(a,b)) if x!=y][:15]) for j,(a,b) in enumerate(zip(actual,expected)) if a!=b])
  assert native_events==expected_events,(level,frame,[x[:3] for x in native_events],[x[:3] for x in expected_events])
  assert not state.updating and not game.updating
  report['rom_comparisons']+=1;report['damage_calls']+=len(requests);report['reward_calls']+=len(rewards)
  report['network_events']+=sum(x[0]==0 for x in native_events); report['sound_events']+=sum(x[0]==1 for x in native_events)
  report['distinct_current_source_cases']+=source_addr!=origin_addr; report['zero_radius_cases']+=radius==0
 assert C.string_at(m.data,m.size)==map_bytes
 report['maps'].append({'level':level,'cases':256,'sectors':m.sector_count})
 lib.ssa_map_assets_free(C.byref(m)); print('PASS combined explosion/damage/reward map',level,flush=True)
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
report['shim_sha256']=hashlib.sha256(shim.read_bytes()).hexdigest(); report['status']='passed'
(ROOT/'analysis/native-combat-combined-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
