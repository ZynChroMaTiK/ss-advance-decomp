"""Entity predicate, spatial lists and full movement queries vs original ROM."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/"orig/SSA.gba").read_bytes(); SHA1=hashlib.sha1(ROM).hexdigest()
assert SHA1=="4ff48d8c18b70a5184758f1917343ae85a70858e"
BUILD=ROOT/"build/native-tests"; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(["gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-pedantic","-shared","-fPIC",
    "-fsanitize=undefined","-fno-sanitize-recover=all","-I",str(ROOT/"include"),
    *[str(ROOT/"src"/f) for f in ("world.c","geometry.c","map_assets.c","horizontal.c","slide.c","movement.c","physics.c","view.c","player_motion.c")],"-o",str(BUILD/"libssa_world.so")],check=True)
class Body(C.Structure):
    _fields_=[(n,C.c_int32) for n in ("x","y","z","velocity_x","velocity_y","velocity_z","height")]+[
        ("flags",C.c_uint16),("floor",C.c_int16),("ceiling",C.c_int16),("hover_offset",C.c_int8),("step_adjust",C.c_int8)]
class Map(C.Structure):
    _fields_=[("data",C.POINTER(C.c_uint8)),("size",C.c_size_t)]+[(n+"_count",C.c_uint16) for n in
        ("thing","texture","sector","node","leaf","segment","side","line","vertex")]+[(n,C.c_uint32) for n in
        ("things","textures","sectors","nodes","leaves","segments","sides","lines","vertices","reject","blockmap","rom_offset")]
class Collision(C.Structure):
    _fields_=[("map",C.POINTER(Map)),("visited",C.POINTER(C.c_uint8)),("special_lines",C.POINTER(C.c_uint16)),
        ("special_count",C.c_size_t),("line_capacity",C.c_size_t)]+[(n,C.c_int32) for n in ("floor","ceiling","low_floor","ceiling_line","blocking_line")]
class Entity(C.Structure): pass
class World(C.Structure): pass
class Hit(C.Structure):
    _fields_=[("fraction",C.c_int32),("line",C.c_uint16)]
class Slide(C.Structure):
    _fields_=[("map",C.POINTER(Map)),("visited",C.POINTER(C.c_uint8)),("intercepts",C.POINTER(Hit)),("capacity",C.c_size_t),("count",C.c_size_t)]
class Tables(C.Structure):
    _fields_=[("sine",C.c_uint16*16384),("direction",C.c_uint16*32769)]
class Player(C.Structure):
    _fields_=[("eye_offset",C.c_int32),("eye_velocity",C.c_int32),("flags",C.c_uint16),("lowered",C.c_uint16)]
class Jump(C.Structure):
    _fields_=[("launch_velocity",C.c_int32),("was_down",C.c_uint8)]
class Camera(C.Structure):
    _fields_=[("eye_z",C.c_int32),("bob_amplitude",C.c_int32)]
class Frame(C.Structure):
    _fields_=[("previous_bob",C.c_int32),("counter",C.c_uint16),("is_local",C.c_uint8),("footsteps_muted",C.c_uint8)]
class View(C.Structure):
    _fields_=[("distance",C.c_int32),("lift",C.c_int32),("third_person",C.c_uint8),("h_down",C.c_uint8)]
class Pose(C.Structure):
    _fields_=[(n,C.c_int32) for n in ("x","y","z","target_x","target_y","target_z","boom_distance")]+[("third_person",C.c_uint8),("obstructed",C.c_uint8)]
EP=C.POINTER(Entity); WP=C.POINTER(World)
class Movement(C.Structure):
    _fields_=[("keys",C.c_uint32),("weapon_flags",C.c_uint32)]+[(n,C.c_int32) for n in
        ("turn_velocity","forward_velocity","strafe_velocity","impulse","velocity_x","velocity_y")]+[
        (n,C.c_uint16) for n in ("heading","move_heading","flags","modifier_ticks","impulse_heading")]
class Motion(C.Structure):
    _fields_=[("movement",Movement)]+[(n,C.c_int32) for n in ("x","y","z","velocity_z")]+[("facing",C.c_uint8)]
class MotionFrame(C.Structure):
    _fields_=[("players",C.POINTER(EP))]+[(n,C.c_uint8) for n in ("ticks","player_count","local_player","slot")]
class MotionOptions(C.Structure):
    _fields_=[("jump",C.POINTER(Jump))]+[(n,C.c_uint8) for n in ("jump_down","controls_enabled","retain_heading")]
CONTACT=C.CFUNCTYPE(None,C.c_void_p,EP,EP)
EXECUTE=C.CFUNCTYPE(C.c_int,C.c_void_p,EP,EP)
Entity._fields_=[("body",Body),("radius",C.c_int32),("type",C.c_uint16),("player_index",C.c_uint8),
    ("leaf",C.c_int),("sector",C.c_int),("cell",C.c_size_t),("owner",WP)]+[(n,EP) for n in ("block_next","block_previous","sector_next","sector_previous")]
World._fields_=[("collision",Collision)]+[(n,C.POINTER(EP)) for n in ("cells","sectors","special_contacts")]+[
    (n,C.c_size_t) for n in ("entity_count","capacity","special_count")]+[(n,C.c_int32) for n in ("origin_x","origin_y")]+[
    ("width",C.c_uint),("height",C.c_uint),("blocking_entity",EP),("observe_special",CONTACT),("contact_context",C.c_void_p),
    ("execute_special",EXECUTE),("execute_context",C.c_void_p),("query_active",C.c_uint),("executing_special",EP)]
lib=C.CDLL(str(BUILD/"libssa_world.so"))
for n,args in {
    "ssa_map_assets_load":[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],"ssa_map_assets_free":[C.POINTER(Map)],
    "ssa_entity_contact":[EP,EP,C.c_int32,C.c_int32],"ssa_world_init":[WP,C.POINTER(Map),C.c_size_t],
    "ssa_world_free":[WP],"ssa_world_insert":[WP,EP],"ssa_world_remove":[WP,EP],
    "ssa_world_query":[WP,EP,C.c_int32,C.c_int32],"ssa_world_try_move":[WP,EP,C.c_int32,C.c_int32],
    "ssa_math_tables_load":[C.POINTER(Tables),C.c_void_p,C.c_size_t],
    "ssa_slide_init":[C.POINTER(Slide),C.POINTER(Map)],"ssa_slide_free":[C.POINTER(Slide)],
    "ssa_world_horizontal_step":[WP,EP,C.POINTER(Slide),C.POINTER(Tables),C.c_uint8,C.c_uint8,C.c_uint8],
    "ssa_jump_init":[C.POINTER(Jump)],
    "ssa_player_vertical_step":[C.POINTER(Body),C.POINTER(Player),C.POINTER(Jump),C.c_uint8,C.c_int,C.c_int],
    "ssa_camera_step":[C.POINTER(Body),C.POINTER(Player),C.POINTER(Camera),C.POINTER(Frame),C.POINTER(Tables)],
    "ssa_map_collision_init":[C.POINTER(Collision),C.POINTER(Map)],"ssa_map_collision_free":[C.POINTER(Collision)],
    "ssa_view_init":[C.POINTER(View)],"ssa_view_key":[C.POINTER(View),C.c_int,C.c_int,C.c_int,C.c_int],
    "ssa_view_pose":[C.POINTER(View),C.POINTER(Body),C.POINTER(Camera),C.c_uint16,C.POINTER(Tables),C.POINTER(Collision),C.POINTER(Pose)],
    "ssa_player_motion_world_step":[C.POINTER(Motion),EP,C.POINTER(Player),WP,C.POINTER(Slide),C.POINTER(Tables),C.POINTER(MotionFrame),C.POINTER(MotionOptions)],
}.items(): getattr(lib,n).argtypes=args
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
uc.mem_write(0x030003a4,ROM[0x430:0x638])
tables=Tables(); assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
BASE,SCRATCH,GRID,ENTITIES,STOP,GAME=0x02000000,0x02040000,0x02050000,0x02070000,0x087ff000,0x02078000
rng=random.Random(0x3D7C); events=[]; sector_events=[]
remove_special=set()
callback_errors=[]
def checked_execute(fn):
    @EXECUTE
    def checked(*args):
        try: return fn(*args)
        except BaseException as error:
            callback_errors.append(error)
            return -1
    return checked
def addr(i): return ENTITIES+i*0x80
def put(a,fmt,*values): uc.mem_write(a,struct.pack(fmt,*values))
def get(a,fmt): return struct.unpack(fmt,uc.mem_read(a,struct.calcsize(fmt)))[0]
def signed(n): return C.c_int32(n).value
def hook(machine,address,size,user):
    if address==0x0801b99c:
        events.append((machine.reg_read(UC_ARM_REG_R0),machine.reg_read(UC_ARM_REG_R1),machine.reg_read(UC_ARM_REG_R2)))
        other=machine.reg_read(UC_ARM_REG_R1)
        if other in remove_special:
            # Controlled release handler: unlink and zero. This validates the
            # original 09870 traversal AFTER callback mutation, not pickup logic
            # or the original pool allocator/DMA zeroing implementation.
            following=get(other+0x18,"<I"); previous=get(other+0x1c,"<I")
            if following: put(following+0x1c,"<I",previous)
            if previous: put(previous+0x18,"<I",following)
            else:
                bx=((get(other,"<I")-get(0x03004944,"<I"))&0xffffffff)>>23
                by=((get(other+4,"<I")-get(0x03004948,"<I"))&0xffffffff)>>23
                put(GRID+4*(by*get(0x0300494c,"<H")+bx),"<I",following)
            machine.mem_write(other,bytes(0x74))
    else: sector_events.append((address,machine.reg_read(UC_ARM_REG_R0)))
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for a in (0x0801b99c,0x0800d1b4,0x0800d214): uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def biosdiv(machine,address,size,user):
    a=signed(machine.reg_read(UC_ARM_REG_R0)); b=signed(machine.reg_read(UC_ARM_REG_R1))
    q=abs(a)//abs(b) if b else (1 if a<0 else -1)
    if b and (a<0)!=(b<0): q=-q
    machine.reg_write(UC_ARM_REG_R0,q&0xffffffff)
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,biosdiv,begin=0x08002c88,end=0x08002c88)
def run(address,*args):
    for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2),args): uc.reg_write(reg,value&0xffffffff)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(address|1,STOP,count=300000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(address)
    return signed(uc.reg_read(UC_ARM_REG_R0))
def pack(e):
    raw=bytearray(0x74)
    for off,n in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
        struct.pack_into("<i",raw,off,getattr(e.body,n))
    struct.pack_into("<i",raw,0x28,e.radius)
    struct.pack_into("<i",raw,44,e.body.height)
    struct.pack_into("<H",raw,0x30,e.body.flags); struct.pack_into("<H",raw,0x3c,e.type)
    struct.pack_into("<b",raw,0x43,e.body.hover_offset); struct.pack_into("<bB",raw,0x53,e.body.step_adjust,e.player_index)
    struct.pack_into("<hh",raw,0x70,e.body.ceiling,e.body.floor)
    return bytes(raw)
report={"rom_sha1":SHA1,"entity_predicate_cases":0,"world_queries":0,"world_moves":0,"spatial_snapshots":0,"native_guard_checks":0,
    "integrated_horizontal_frames":0,"jump_view_frames":0,"complete_player_motion_frames":0,
    "synchronous_removal_queries":0,"synchronous_guard_checks":0,
    "scope":"whole 03D7C predicate with special-handler recorder; whole 03E5C/043D0 and 04BDC horizontal integration with original grid/BSP/sliding; sector-pool D1B4/D214 hooks and BIOS signed division service only; native sector list order and jump/view integration checked separately"}

flags=(0,1,8,16,32,0x100,0x200,0xffff)
for i in range(14000):
    es=(Entity*2)()
    for e in es:
        if i<10000:
            e.body=Body(*[rng.randrange(-200*65536,200*65536) for _ in range(7)],rng.choice(flags))
            e.radius=rng.choice((0,1,16*65536,32*65536,0x7fffffff,-2147483648))
        else:
            e.body=Body(*[signed(rng.getrandbits(32)) for _ in range(7)],rng.randrange(65536))
            e.radius=signed(rng.getrandbits(32))
        e.type=rng.choice((0,1,0x67,0xe1,0xffff)); e.player_index=rng.choice((0,1,2,255))
    x,y=es[0].body.x,es[0].body.y
    if i%3==0: es[1].body.x=x; es[1].body.y=y
    if i%7==0: es[1].body.x=signed(x+es[0].radius+es[1].radius)
    same=i%17==0
    uc.mem_write(addr(0),pack(es[0])); uc.mem_write(addr(1),pack(es[1]))
    put(0x03004330,"<I",addr(0)); put(0x03004334,"<i",x); put(0x03004438,"<i",y)
    put(0x03004328,"<I",0); events.clear()
    oracle=run(0x08003d7c,addr(0 if same else 1))
    actual=lib.ssa_entity_contact(C.byref(es[0]),C.byref(es[0 if same else 1]),x,y)
    assert oracle==(actual!=0),(i,actual,oracle)
    assert bool(events)==(actual==2)
    if events: assert events==[(addr(0),addr(1),0)]
    assert get(0x03004328,"<I")== (addr(1) if actual==0 else 0)
    assert bytes(uc.mem_read(addr(0),0x74))==pack(es[0])
    assert bytes(uc.mem_read(addr(1),0x74))==pack(es[1])
    report["entity_predicate_cases"]+=1
print("PASS 14,000 entity predicates",flush=True)

for level in range(16):
    m=Map(); w=World(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
    assert lib.ssa_world_init(C.byref(w),C.byref(m),32)
    data=bytearray(C.string_at(m.data,m.size)); original=bytearray(data)
    for off in range(0x1c,0x48,4): struct.pack_into("<I",original,off,BASE+struct.unpack_from("<I",original,off)[0])
    uc.mem_write(BASE,bytes(original)); put(0x03004930,"<I",BASE)
    put(0x03004944,"<iihh",w.origin_x,w.origin_y,w.width,w.height)
    put(0x03004954,"<I",SCRATCH); put(0x03004640,"<I",GRID)
    uc.mem_write(GRID,bytes(w.width*w.height*4))
    vertices=list(struct.iter_unpack("<hh",data[m.vertices:m.vertices+m.vertex_count*4]))
    es=(Entity*24)(); pointers={C.addressof(e):i for i,e in enumerate(es)}
    def idx(p): return pointers[C.addressof(p.contents)] if p else -1
    def romidx(p): return (p-ENTITIES)//0x80 if p else -1
    native_events=[]
    @CONTACT
    def observe(context,current,other): native_events.append((idx(current),idx(other)))
    w.observe_special=observe
    def compare_links():
        for j,e in enumerate(es):
            if not e.owner: continue
            assert idx(e.block_next)==romidx(get(addr(j)+0x18,"<I")),(level,j,"next")
            assert idx(e.block_previous)==romidx(get(addr(j)+0x1c,"<I")),(level,j,"previous")
            assert e.leaf==get(addr(j)+0x6e,"<h") and e.sector==get(addr(j)+0x6a,"<h")
            assert get(addr(j)+0x60,"<I")==BASE+m.sectors+20*e.sector
            assert get(addr(j)+0x64,"<I")==BASE+m.leaves+8*e.leaf
        grid=struct.unpack("<"+"I"*(w.width*w.height),uc.mem_read(GRID,w.width*w.height*4))
        assert [idx(w.cells[k]) for k in range(len(grid))]==[romidx(p) for p in grid]
        seen=[]
        for sector in range(m.sector_count):
            p=w.sectors[sector]; previous=-1
            while p:
                e=p.contents; j=idx(p); assert j not in seen and e.sector==sector and idx(e.sector_previous)==previous
                seen.append(j); previous=j; p=e.sector_next
        assert len(seen)==w.entity_count
        report["spatial_snapshots"]+=1
    # Some entities share cells; some are outside blockmap but have BSP sectors.
    vx,vy=vertices[0]
    for j,e in enumerate(es):
        if j<16: x,y=vx*65536+rng.randrange(-90*65536,90*65536),vy*65536+rng.randrange(-90*65536,90*65536)
        elif j==23: x,y=w.origin_x-256*65536,w.origin_y-256*65536
        else:
            vx2,vy2=rng.choice(vertices); x,y=vx2*65536,vy2*65536
        e.body=Body(x=x,y=y,z=0,height=100*65536,flags=16 if j==0 else rng.choice((0,1,8,16,32,0x100,0x200)),step_adjust=7)
        e.radius=rng.choice((8,16,32))*65536; e.type=0xe1 if j in (3,7,9) else rng.choice((0,1,0x67)); e.player_index=j%4
        uc.mem_write(addr(j),pack(e)); run(0x0800a5d0,addr(j))
        assert lib.ssa_world_insert(C.byref(w),C.byref(e))
        assert e.body.floor==get(addr(j)+0x72,"<h") and e.body.ceiling==get(addr(j)+0x70,"<h") and e.body.step_adjust==0
    compare_links()
    def compare_outputs():
        assert idx(w.blocking_entity)==romidx(get(0x03004328,"<I"))
        assert [idx(w.special_contacts[k]) for k in range(w.special_count)]==[romidx(ev[1]) for ev in events]
        c=w.collision; n=get(0x03004458,"<I")
        assert n<=8,(level,"original line contact overflow")
        assert (c.floor,c.ceiling,c.low_floor)==tuple(get(a,"<i") for a in (0x0300434c,0x0300432c,0x03004444))
        assert list(c.special_lines[:c.special_count])==[(get(0x03004620+4*k,"<I")-BASE-m.lines)//40 for k in range(n)]
    for i in range(300):
        j=rng.randrange(16); e=es[j]
        x=signed(e.body.x+rng.randrange(-30*65536,30*65536)); y=signed(e.body.y+rng.randrange(-30*65536,30*65536))
        events.clear(); native_events.clear(); before=bytes(uc.mem_read(addr(j),0x74))
        oracle=run(0x08003e5c,addr(j),x,y); actual=lib.ssa_world_query(C.byref(w),C.byref(e),x,y)
        assert actual==oracle,(level,i,"query",actual,oracle); compare_outputs()
        assert native_events==[(romidx(a),romidx(b)) for a,b,_ in events]
        assert bytes(uc.mem_read(addr(j),0x74))==before
        report["world_queries"]+=1
        e.body.z=(w.collision.floor+rng.choice((-49,-48,0,1,49,100)))*65536
        e.body.velocity_z=rng.randrange(-65536,65536); e.body.flags=rng.choice((0,4,8,16,0x200))
        put(addr(j)+8,"<i",e.body.z); put(addr(j)+20,"<i",e.body.velocity_z); put(addr(j)+0x30,"<H",e.body.flags)
        events.clear(); native_events.clear(); sector_events.clear(); before=bytearray(uc.mem_read(addr(j),0x74))
        oracle=run(0x080043d0,addr(j),x,y); actual=lib.ssa_world_try_move(C.byref(w),C.byref(e),x,y)
        assert actual==oracle,(level,i,"move",actual,oracle); compare_outputs()
        assert native_events==[(romidx(a),romidx(b)) for a,b,_ in events]
        for off,n in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
            assert getattr(e.body,n)==get(addr(j)+off,"<i"),(level,i,n)
        assert e.body.floor==get(addr(j)+0x72,"<h") and e.body.ceiling==get(addr(j)+0x70,"<h")
        assert e.body.step_adjust==get(addr(j)+0x53,"<b")
        assert sector_events==([(0x0800d214,addr(j)),(0x0800d1b4,addr(j))] if actual else [])
        report["world_moves"]+=1
        if i%25==0: compare_links()
    slide=Slide(); assert lib.ssa_slide_init(C.byref(slide),C.byref(m))
    put(0x03004010,"<I",GAME)
    for i in range(128):
        j=i%16; e=es[j]
        e.body.flags=rng.choice((8,16,24,0x208)); e.body.z=e.body.floor*65536
        e.body.velocity_x=rng.randrange(-40*65536,40*65536); e.body.velocity_y=rng.randrange(-40*65536,40*65536)
        e.body.velocity_z=0
        put(addr(j)+8,"<iiii",e.body.z,e.body.velocity_x,e.body.velocity_y,e.body.velocity_z)
        put(addr(j)+0x30,"<H",e.body.flags)
        ticks=rng.choice((0,1,2,4)); count=rng.choice((1,2)); local=rng.randrange(4)
        put(GAME+7,"<B",ticks); put(GAME+0x10,"<B",count); put(GAME+0x68,"<B",local)
        events.clear(); native_events.clear(); sector_events.clear()
        run(0x08004bdc,addr(j))
        assert lib.ssa_world_horizontal_step(C.byref(w),C.byref(e),C.byref(slide),C.byref(tables),ticks,count,local)==0
        assert native_events==[(romidx(a),romidx(b)) for a,b,_ in events],(level,i,"integrated contact ordering")
        for off,n in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
            assert getattr(e.body,n)==get(addr(j)+off,"<i"),(level,i,"integrated",n,getattr(e.body,n),get(addr(j)+off,"<i"))
        assert e.body.floor==get(addr(j)+0x72,"<h") and e.body.ceiling==get(addr(j)+0x70,"<h")
        if i%16==0: compare_links()
        report["integrated_horizontal_frames"]+=1
    # Entire 064B8: controls, local multiplayer push, sync, real world/sliding,
    # original vertical physics, sync back. Full player bytes are compared.
    player_base=0x02079000
    put(0x03004648,"<I",player_base)
    motion_fields={"keys":(0xa4,"I"),"weapon_flags":(0x30,"I"),"turn_velocity":(0x24,"i"),
        "forward_velocity":(0xb0,"i"),"strafe_velocity":(0xb4,"i"),"impulse":(0x20,"i"),
        "velocity_x":(12,"i"),"velocity_y":(16,"i"),"heading":(0xb8,"H"),
        "move_heading":(0xba,"H"),"flags":(0xbc,"H"),"modifier_ticks":(0xc2,"H"),"impulse_heading":(0xdc,"H")}
    peer_pointers=(EP*4)(*[C.pointer(es[k]) for k in range(4)])
    for i in range(128):
        count=rng.choice((1,2,3,4)); slot=i%count; e=es[slot]
        for k in range(count):
            es[k].player_index=k
            put(addr(k)+0x54,"<B",k)
            put(player_base+k*256+0xac,"<I",addr(k))
        e.body.flags=24; e.body.z=e.body.floor*65536
        put(addr(slot)+0x30,"<H",e.body.flags)
        motion=Motion(Movement(keys=rng.randrange(1024),weapon_flags=rng.choice((0,0x100)),
            heading=rng.randrange(65536),flags=rng.choice((0,4,16)),
            turn_velocity=rng.randrange(-0x1900000,0x1900001),
            forward_velocity=rng.randrange(-0x80000,0x80001),strafe_velocity=rng.randrange(-0x80000,0x80001)),
            e.body.x,e.body.y,e.body.z,rng.randrange(-5*65536,5*65536),0)
        pv=Player(96*65536,0,motion.movement.flags,0)
        raw=bytearray(rng.randbytes(256))
        for name,(off,fmt) in motion_fields.items(): struct.pack_into("<"+fmt,raw,off,getattr(motion.movement,name))
        struct.pack_into("<iii",raw,0,motion.x,motion.y,motion.z)
        struct.pack_into("<i",raw,20,motion.velocity_z)
        struct.pack_into("<I",raw,0xac,addr(slot)); struct.pack_into("<iiH",raw,0xd0,pv.eye_offset,pv.eye_velocity,pv.lowered)
        uc.mem_write(player_base+256*slot,bytes(raw))
        ticks=rng.choice((0,1,2,4)); local=rng.randrange(count)
        mf=MotionFrame(peer_pointers,ticks,count,local,slot)
        put(GAME+7,"<B",ticks); put(GAME+0x10,"<B",count); put(GAME+0x68,"<B",local)
        events.clear(); native_events.clear()
        run(0x080064b8,player_base+256*slot)
        assert lib.ssa_player_motion_world_step(C.byref(motion),C.byref(e),C.byref(pv),C.byref(w),
            C.byref(slide),C.byref(tables),C.byref(mf),None)==0
        for name,(off,fmt) in motion_fields.items(): struct.pack_into("<"+fmt,raw,off,getattr(motion.movement,name))
        struct.pack_into("<iii",raw,0,motion.x,motion.y,motion.z); struct.pack_into("<i",raw,20,motion.velocity_z)
        struct.pack_into("<iiH",raw,0xd0,pv.eye_offset,pv.eye_velocity,pv.lowered)
        actual_rom=bytes(uc.mem_read(player_base+256*slot,256))
        assert bytes(raw)==actual_rom,(level,i,"whole player motion",[(o,a,b) for o,(a,b) in enumerate(zip(raw,actual_rom)) if a!=b])
        assert native_events==[(romidx(a),romidx(b)) for a,b,_ in events]
        assert motion.facing==get(addr(slot)+0x3f,"<B")
        for off,n in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
            assert getattr(e.body,n)==get(addr(slot)+off,"<i"),(level,i,"whole motion body",n)
        assert (e.body.floor,e.body.ceiling,e.body.flags)==(get(addr(slot)+0x72,"<h"),get(addr(slot)+0x70,"<h"),get(addr(slot)+0x30,"<H"))
        if i%16==0: compare_links()
        report["complete_player_motion_frames"]+=1
    # Remove head/middle/tail in arbitrary order; original leaves stale removed
    # entity links, native deliberately clears them to reject double removal.
    order=list(range(len(es))); rng.shuffle(order)
    for j in order:
        run(0x0800a670,addr(j)); assert lib.ssa_world_remove(C.byref(w),C.byref(es[j])); compare_links()
        assert not lib.ssa_world_remove(C.byref(w),C.byref(es[j])); report["native_guard_checks"]+=1
    assert not w.entity_count
    for e in es: assert not e.owner
    assert lib.ssa_world_insert(C.byref(w),C.byref(es[0]))
    assert not lib.ssa_world_insert(C.byref(w),C.byref(es[0])); report["native_guard_checks"]+=1
    # Full native horizontal + jump + original eye + H-view ordering. Separate
    # camera query storage must leave gameplay collision results untouched.
    e=es[0]; e.body.flags=24; e.body.z=e.body.floor*65536
    p=Player(eye_offset=96*65536,flags=4); jump=Jump(); camera=Camera(); frame=Frame(); view=View(); pose=Pose(); camera_query=Collision()
    lib.ssa_jump_init(C.byref(jump)); lib.ssa_view_init(C.byref(view)); assert lib.ssa_map_collision_init(C.byref(camera_query),C.byref(m))
    for tick in range(128):
        e.body.velocity_x=65536; e.body.velocity_y=65536
        assert lib.ssa_world_horizontal_step(C.byref(w),C.byref(e),C.byref(slide),C.byref(tables),1,1,0)==0
        lib.ssa_player_vertical_step(C.byref(e.body),C.byref(p),C.byref(jump),1,tick<64,1)
        frame.counter=tick
        lib.ssa_camera_step(C.byref(e.body),C.byref(p),C.byref(camera),C.byref(frame),C.byref(tables))
        if tick in (0,64): assert lib.ssa_view_key(C.byref(view),ord('H'),1,0,1)
        if tick in (1,65): lib.ssa_view_key(C.byref(view),ord('H'),0,0,1)
        before=bytes(w.collision)
        assert lib.ssa_view_pose(C.byref(view),C.byref(e.body),C.byref(camera),0,C.byref(tables),C.byref(camera_query),C.byref(pose))
        assert bytes(w.collision)==before
        assert pose.target_z==camera.eye_z and e.body.z>=e.body.floor*65536
        if tick>=64: assert not pose.third_person and pose.x==e.body.x and pose.y==e.body.y and pose.z==camera.eye_z
        report["jump_view_frames"]+=1
    lib.ssa_map_collision_free(C.byref(camera_query)); lib.ssa_slide_free(C.byref(slide))
    lib.ssa_world_free(C.byref(w)); assert not es[0].owner
    lib.ssa_map_assets_free(C.byref(m))
    print(f"PASS level {level}: 300 queries, 300 moves, grid/sector lists",flush=True)
# Controlled synchronous release cases keep the real 03E5C -> 09870 -> 03D7C
# traversal. Removing a head/middle/tail must stop only its own cell; subsequent
# cells still execute in X/Y order. Retaining all specials keeps full traversal.
for level in range(16):
    m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
    data=bytearray(C.string_at(m.data,m.size))
    for off in range(0x1c,0x48,4): struct.pack_into("<I",data,off,BASE+struct.unpack_from("<I",data,off)[0])
    uc.mem_write(BASE,bytes(data)); put(0x03004930,"<I",BASE)
    put(0x03004954,"<I",SCRATCH); put(0x03004640,"<I",GRID)
    for case in range(64):
        w=World(); assert lib.ssa_world_init(C.byref(w),C.byref(m),16)
        assert w.width>=2 and w.height>=2
        put(0x03004944,"<iihh",w.origin_x,w.origin_y,w.width,w.height)
        uc.mem_write(GRID,bytes(w.width*w.height*4))
        es=(Entity*13)(); pointers={C.addressof(e):j for j,e in enumerate(es)}
        def idx(p): return pointers[C.addressof(p.contents)] if p else -1
        base_x=min(w.width-2,w.width//2); base_y=min(w.height-2,w.height//2)
        for j in range(12):
            cell=j//3
            x=w.origin_x+(128*(base_x+cell//2)+64)*65536
            y=w.origin_y+(128*(base_y+cell%2)+64)*65536
            e=es[j]; e.body=Body(x=x,y=y,height=32*65536); e.radius=8*65536; e.type=0xe1
            uc.mem_write(addr(j),pack(e)); run(0x0800a5d0,addr(j))
            assert lib.ssa_world_insert(C.byref(w),C.byref(e))
        mover=es[12]; mover.body=Body(x=w.origin_x+128*(base_x+1)*65536,
            y=w.origin_y+128*(base_y+1)*65536,height=100*65536,flags=16)
        mover.radius=160*65536; mover.type=0xc9
        uc.mem_write(addr(12),pack(mover))
        # Four choices per cell: retain, remove head, middle, or tail. Selected
        # cross-cell patterns also remove every encountered special.
        selected=set()
        for cell in range(4):
            choice=(case>>(2*(cell%3)))&3
            if choice: selected.add(cell*3+3-choice)
        if case==63: selected=set(range(12))
        remove_special.clear(); remove_special.update(addr(j) for j in selected)
        observed=[]; executed=[]
        @CONTACT
        def observe_mutation(context,current,other): observed.append(idx(other))
        @checked_execute
        def execute_mutation(context,current,other):
            j=idx(other); executed.append(j)
            if case==0 and len(executed)==1:
                assert lib.ssa_world_query(C.byref(w),current,mover.body.x,mover.body.y)==-1
                assert lib.ssa_world_try_move(C.byref(w),other,other.contents.body.x,other.contents.body.y)==-1
                assert not lib.ssa_world_insert(C.byref(w),C.byref(mover))
                assert not lib.ssa_world_remove(C.byref(w),C.byref(es[(j+1)%12]))
                assert not lib.ssa_world_init(C.byref(w),C.byref(m),16)
                before=w.entity_count; lib.ssa_world_free(C.byref(w)); assert w.entity_count==before
                report["synchronous_guard_checks"]+=6
            if j in selected: assert lib.ssa_world_remove(C.byref(w),other)
            return 0
        w.observe_special=observe_mutation; w.execute_special=execute_mutation
        events.clear()
        oracle=run(0x08003e5c,addr(12),mover.body.x,mover.body.y)
        actual=lib.ssa_world_query(C.byref(w),C.byref(mover),mover.body.x,mover.body.y)
        assert not callback_errors,callback_errors
        expected=[(event[1]-ENTITIES)//0x80 for event in events]
        assert actual==oracle,(level,case,"removal result",actual,oracle)
        assert observed==executed==expected,(level,case,"removal traversal",observed,expected)
        assert [idx(w.special_contacts[k]) for k in range(w.special_count)]==expected
        assert not w.query_active and not w.executing_special
        removed=selected.intersection(expected)
        assert w.entity_count==12-len(removed)
        grid=struct.unpack("<"+"I"*(w.width*w.height),uc.mem_read(GRID,w.width*w.height*4))
        assert [idx(w.cells[k]) for k in range(len(grid))]==[(p-ENTITIES)//0x80 if p else -1 for p in grid]
        for j,e in enumerate(es[:12]):
            if j in removed:
                assert not e.owner and not e.block_next and not e.block_previous
            else:
                assert e.owner
                assert idx(e.block_next)==((get(addr(j)+0x18,"<I")-ENTITIES)//0x80 if get(addr(j)+0x18,"<I") else -1)
                assert idx(e.block_previous)==((get(addr(j)+0x1c,"<I")-ENTITIES)//0x80 if get(addr(j)+0x1c,"<I") else -1)
        # Executor errors preserve earlier removals and release traversal guards.
        if case==0:
            @checked_execute
            def fail_mutation(context,current,other):
                assert lib.ssa_world_remove(C.byref(w),other)
                return -1
            w.execute_special=fail_mutation
            assert lib.ssa_world_query(C.byref(w),C.byref(mover),mover.body.x,mover.body.y)==-1
            assert not callback_errors,callback_errors
            assert w.entity_count==11 and not w.query_active and not w.executing_special
            w.execute_special=EXECUTE()
            assert lib.ssa_world_query(C.byref(w),C.byref(mover),mover.body.x,mover.body.y)>=0
            report["synchronous_guard_checks"]+=3
        lib.ssa_world_free(C.byref(w)); report["synchronous_removal_queries"]+=1
    lib.ssa_map_assets_free(C.byref(m))
remove_special.clear()
report["synchronous_scope"]="1,024 whole original queries with controlled unlink-and-zero special handler; validates head/middle/tail retention/removal and multi-cell callback traversal, not pickup effects or original allocator/DMA"
print("PASS 1,024 synchronous release traversal queries and 144 callback guards",flush=True)
subprocess.run(["gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-pedantic",
    "-fsanitize=undefined","-fno-sanitize-recover=all","-I",str(ROOT/"include"),
    str(ROOT/"tests/test_pickup_world.c"),*[str(ROOT/"src"/f) for f in
    ("world.c","geometry.c","map_assets.c","horizontal.c","slide.c","movement.c","pickup.c","inventory.c")],
    "-o",str(BUILD/"test_pickup_world")],check=True)
subprocess.run([str(BUILD/"test_pickup_world"),str(ROOT/"orig/SSA.gba")],check=True)
report["native_pickup_world_queries"]=64
report["source_sha256"]={f:hashlib.sha256((ROOT/"src"/f).read_bytes()).hexdigest() for f in ("world.c","geometry.c","horizontal.c","slide.c","physics.c","view.c","player_motion.c")}
(ROOT/"analysis/native-world-validation.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))
