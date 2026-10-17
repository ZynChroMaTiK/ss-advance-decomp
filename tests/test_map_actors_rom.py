"""Map01984 construction + whole00908 animation; AI intentionally excluded."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
# Reuse only established ABI declarations and the complete-spawn oracle helper;
# no previous test suite is imported/executed.
nodes=[]
for node in ast.parse((ROOT/'tests/test_spawn_rom.py').read_text()).body:
    if isinstance(node,ast.ClassDef):nodes.append(node)
    elif isinstance(node,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id=='EP' for t in node.targets):nodes.append(node)
    elif isinstance(node,ast.FunctionDef) and node.name in ('put','get','compare','sector_hook'):nodes.append(node)
exec(compile(ast.Module(nodes,type_ignores=[]),'spawn oracle declarations','exec'))
class State(C.Structure):_fields_=[('animation',C.c_uint16),('frame',C.c_uint16),('index',C.c_uint8),('elapsed',C.c_uint8),('period',C.c_uint8),('done',C.c_uint8)]
class Data(C.Structure):_fields_=[('words',C.c_uint16*772),('offsets',C.c_uint16*142),('loaded',C.c_uint)]
class Pool(C.Structure):_fields_=[('free',C.c_uint16*100),('active',C.c_uint16*100),('positions',C.c_uint16*100),('count',C.c_uint16),('occupied',C.c_uint8*100)]
class Actors(C.Structure):_fields_=[('plan',Plan),('pool',Pool),('animations',State*100),('world',C.POINTER(World)),('data',C.POINTER(Data)),('loaded',C.c_uint),('updating',C.c_uint)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['map_actors.c','actor_animation.c','entity_pool.c','spawn.c','world.c','geometry.c','map_assets.c','horizontal.c','slide.c','movement.c','physics.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_map_actors.so')],check=True)
lib=C.CDLL(str(B/'libssa_map_actors.so'))
for name,args in {
 'ssa_map_assets_load':[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],'ssa_map_assets_free':[C.POINTER(Map)],
 'ssa_spawn_definitions_load':[C.POINTER(Definitions),C.c_void_p,C.c_size_t],
 'ssa_spawn_plan_load':[C.POINTER(Plan),C.POINTER(Map),C.POINTER(Definitions),C.c_uint,C.c_uint,C.c_size_t],
 'ssa_spawn_plan_insert':[C.POINTER(Plan),C.POINTER(World)],'ssa_spawn_plan_free':[C.POINTER(Plan)],
 'ssa_world_init':[C.POINTER(World),C.POINTER(Map),C.c_size_t],'ssa_world_free':[C.POINTER(World)],
 'ssa_actor_animations_load':[C.POINTER(Data),C.c_void_p,C.c_size_t],
 'ssa_map_actors_init':[C.POINTER(Actors),C.POINTER(World),C.POINTER(Definitions),C.POINTER(Data),C.c_uint],
 'ssa_map_actors_step':[C.POINTER(Actors),C.c_uint],'ssa_map_actors_free':[C.POINTER(Actors)],
 'ssa_map_actor_object':[C.POINTER(Actors),C.c_uint],'ssa_map_actor_animation':[C.POINTER(Actors),C.c_uint]}.items():getattr(lib,name).argtypes=args
lib.ssa_map_actor_object.restype=C.POINTER(Object);lib.ssa_map_actor_animation.restype=C.POINTER(State)
defs=Definitions();data=Data();assert lib.ssa_spawn_definitions_load(C.byref(defs),ROM,len(ROM))
assert lib.ssa_actor_animations_load(C.byref(data),ROM,len(ROM))==1
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for a,n in ((0x08000000,len(ROM)),(0x02000000,0x100000),(0x03000000,0x10000)):uc.mem_map(a,n)
uc.mem_write(0x08000000,ROM)
BASE,GRID,POOL,GAME,SETTINGS,STOP=0x02000000,0x02050000,0x02060000,0x02070000,0x02071000,0x087ff000
uc.hook_add(UC_HOOK_CODE,sector_hook,begin=0x0800d1b4,end=0x0800d1b4)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_spawn_runs':0,'entity_comparisons':0,'marker_comparisons':0,'grid_snapshots':0,'native_guards':0,'whole_animation_frames':0,'animated_records':0,'maps':[]}
rng=random.Random(0x1984908)
def guard_step(a,ticks):
    before=bytes(a);assert lib.ssa_map_actors_step(C.byref(a),ticks)==-1 and bytes(a)==before;report['native_guards']+=1
for level in range(16):
    m=Map();w=World();a=Actors()
    assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
    assert lib.ssa_world_init(C.byref(w),C.byref(m),100)
    assert lib.ssa_map_actors_init(C.byref(a),C.byref(w),C.byref(defs),C.byref(data),level)==1
    compare(m,level,1,100) # Full original01984, real pool/scanner/setter/BSP/grid.
    n=a.plan.object_count;assert n==get(0x03004354,'<H')==a.pool.count and n<100
    before=bytes(a)
    assert lib.ssa_map_actors_init(C.byref(a),C.byref(w),C.byref(defs),C.byref(data),level)==-1 and bytes(a)==before;report['native_guards']+=1
    for i in range(n):
        o=a.plan.objects[i];raw=bytes(uc.mem_read(POOL+i*116,116))
        assert (o.entity.body.x,o.entity.body.y,o.entity.body.z)==struct.unpack_from('<iii',raw)
        assert (o.entity.type,o.variant,o.facing)==struct.unpack_from('<HBB',raw,0x3c)
        assert o.trigger==struct.unpack_from('<H',raw,0x68)[0]
        assert bytes(a.animations[i])==raw[0x58:0x60]
        assert not any((o.entity.radius,o.entity.body.height,o.entity.body.flags))
        assert C.addressof(lib.ssa_map_actor_object(C.byref(a),i).contents)==C.addressof(o)
        assert C.addressof(lib.ssa_map_actor_animation(C.byref(a),i).contents)==C.addressof(a.animations[i])
    assert not lib.ssa_map_actor_object(C.byref(a),n) and not lib.ssa_map_actor_animation(C.byref(a),n)
    for frame in range(128):
        ticks=(0,1,2,3,7,255)[frame%6]
        raw=bytearray(uc.mem_read(POOL,n*116))
        for i in range(n):
            flags=0x800 if (i+frame)%13==0 else 0
            a.plan.objects[i].entity.body.flags=flags;struct.pack_into('<H',raw,i*116+0x30,flags)
        uc.mem_write(POOL,bytes(raw));uc.mem_write(GAME+7,bytes([ticks]))
        uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
        uc.emu_start(0x08000909,STOP,count=1000000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
        assert lib.ssa_map_actors_step(C.byref(a),ticks)==1
        for i in range(n):raw[i*116+0x58:i*116+0x60]=bytes(a.animations[i])
        assert bytes(raw)==bytes(uc.mem_read(POOL,n*116))
        assert not any(a.plan.objects[i].entity.radius or a.plan.objects[i].entity.body.height for i in range(n))
        report['whole_animation_frames']+=1;report['animated_records']+=n
    guard_step(a,256);a.updating=1;guard_step(a,1);a.updating=0
    w.query_active=1;guard_step(a,1);assert lib.ssa_map_actors_free(C.byref(a))==-1;report['native_guards']+=1;w.query_active=0
    if n:
        saved=a.animations[0].animation;a.animations[0].animation=65535;a.plan.objects[0].entity.body.flags=0;guard_step(a,1);a.animations[0].animation=saved
    report['maps'].append({'level':level,'objects':n,'unused_slots_before_player':100-n})
    assert lib.ssa_map_actors_free(C.byref(a))==1 and w.entity_count==0
    lib.ssa_world_free(C.byref(w));lib.ssa_map_assets_free(C.byref(m))
import os
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],str(ROOT/'src/player_spawn.c'),str(ROOT/'tests/test_map_actors.c'),'-o',str(B/'test_map_actors')],check=True)
sanitized=subprocess.run([str(B/'test_map_actors'),str(ROOT/'orig/SSA.gba')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True,check=True)
report.update(json.loads(sanitized.stdout))
report['source_sha256']={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources+['player_spawn.c','actor_assets.c','actor_raster.c']}
report['ownership_test_sha256']=hashlib.sha256((ROOT/'tests/test_map_actors.c').read_bytes()).hexdigest()
report['scope']='Original01984 construction with real pool/setter/BSP/grid and whole00908 animation on map objects. Only original sector-list allocation is modeled. Native ownership test retains a separately spawned player after map-actor teardown on all16 maps. No enemy AI activation, scripts, pickups or completed gameplay claim.'
(ROOT/'analysis/native-map-actors-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
