"""Native geometry against full original routines on all sixteen real maps."""
from pathlib import Path
import ctypes as C
import hashlib
import itertools
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/"orig/SSA.gba").read_bytes()
SHA1="4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest()==SHA1
BUILD=ROOT/"build/native-tests"; BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(["gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-pedantic","-shared","-fPIC",
    "-fsanitize=undefined","-fno-sanitize-recover=all","-I",str(ROOT/"include"),
    *[str(ROOT/"src"/f) for f in ("geometry.c","map_assets.c","physics.c","movement.c")],
    "-o",str(BUILD/"libssa_geometry.so")],check=True)
class Map(C.Structure):
    _fields_=[("data",C.POINTER(C.c_uint8)),("size",C.c_size_t)]+[(n+"_count",C.c_uint16) for n in
        ("thing","texture","sector","node","leaf","segment","side","line","vertex")]+[(n,C.c_uint32) for n in
        ("things","textures","sectors","nodes","leaves","segments","sides","lines","vertices","reject","blockmap","rom_offset")]
class Collision(C.Structure):
    _fields_=[("map",C.POINTER(Map)),("visited",C.POINTER(C.c_uint8)),("special_lines",C.POINTER(C.c_uint16)),
        ("special_count",C.c_size_t),("line_capacity",C.c_size_t)]+[(n,C.c_int32) for n in ("floor","ceiling","low_floor","ceiling_line","blocking_line")]
class Start(C.Structure):
    _fields_=[("x",C.c_int32),("y",C.c_int32),("z",C.c_int32),("heading",C.c_uint16),("floor",C.c_int16),("ceiling",C.c_int16)]
class Bounds(C.Structure):
    _fields_=[(n,C.c_int32) for n in ("top","bottom","left","right")]
class Body(C.Structure):
    _fields_=[(n,C.c_int32) for n in ("x","y","z","velocity_x","velocity_y","velocity_z","height")]+[
        ("flags",C.c_uint16),("floor",C.c_int16),("ceiling",C.c_int16),("hover_offset",C.c_int8),("step_adjust",C.c_int8)]
class Player(C.Structure):
    _fields_=[("eye_offset",C.c_int32),("eye_velocity",C.c_int32),("flags",C.c_uint16),("lowered",C.c_uint16)]
class Jump(C.Structure):
    _fields_=[("launch_velocity",C.c_int32),("was_down",C.c_uint8)]
lib=C.CDLL(str(BUILD/"libssa_geometry.so"))
for name,args in {
    "ssa_map_assets_load":[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint],
    "ssa_map_assets_free":[C.POINTER(Map)],
    "ssa_map_collision_init":[C.POINTER(Collision),C.POINTER(Map)],
    "ssa_map_collision_free":[C.POINTER(Collision)],
    "ssa_map_leaf":[C.POINTER(Map),C.c_int32,C.c_int32],
    "ssa_map_sector":[C.POINTER(Map),C.c_int32,C.c_int32],
    "ssa_map_player_start":[C.POINTER(Map),C.POINTER(Start)],
    "ssa_bsp_side":[C.c_int32]*6,
    "ssa_line_side":[C.c_int32]*2+[C.c_int16]*4,
    "ssa_box_line_side":[C.POINTER(Bounds)]+[C.c_int16]*4+[C.c_uint16],
    "ssa_map_query":[C.POINTER(Collision)]+[C.c_int32]*5,
    "ssa_map_try_move":[C.POINTER(Collision),C.POINTER(Body)]+[C.c_int32]*3,
    "ssa_jump_init":[C.POINTER(Jump)],
    "ssa_player_vertical_step":[C.POINTER(Body),C.POINTER(Player),C.POINTER(Jump),C.c_uint8,C.c_int,C.c_int]
}.items(): getattr(lib,name).argtypes=args
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000); uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
BASE,SCRATCH,GRID,ENTITY,PARAM,STOP=0x02000000,0x02040000,0x02050000,0x02070000,0x02071000,0x087ff000
REGS=(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)
rng=random.Random(0x3E5C)
report={"rom_sha1":SHA1,"bsp_side_cases":0,"line_side_cases":0,"box_side_cases":0,"leaf_cases":0,
    "sector_cases":0,"map_query_cases":0,"move_cases":0,"real_map_jump_scenarios":0,"spawn_jump_scenarios":0,"spawn_cases":0,"rejected_geometry_cases":0,
    "scope":"whole original point/line/box/BSP and static map query routines; empty entity grid; move routine hooks only spatial unlink/link; native jump integration separately"}
def signed(n): return C.c_int32(n).value
def put(addr,fmt,*values): uc.mem_write(addr,struct.pack(fmt,*values))
def get(addr,fmt): return struct.unpack(fmt,uc.mem_read(addr,struct.calcsize(fmt)))[0]
def run(addr,*args):
    for reg,arg in zip(REGS,args): uc.reg_write(reg,arg&0xffffffff)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(addr|1,STOP,count=200000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(addr)
    return signed(uc.reg_read(UC_ARM_REG_R0))

values=(-2147483648,-65537,-65536,-1,0,1,65535,65536,2147483647)
for i in range(6000):
    x,y=(rng.choice(values) if i<2000 else signed(rng.getrandbits(32)) for _ in range(2))
    ox,oy,dx,dy=(rng.randrange(-32768,32768) for _ in range(4))
    if i%4==0: dx=0
    if i%4==1: dy=0
    put(PARAM,"<iiiiii",x,y,ox,oy,dx,dy)
    expect=run(0x0800b1b0,PARAM,PARAM+8,PARAM+16)
    assert lib.ssa_bsp_side(x,y,ox,oy,dx,dy)==expect,(x,y,ox,oy,dx,dy)
    report["bsp_side_cases"]+=1
    put(0x03004338,"<h",ox); put(0x03004324,"<h",oy)
    put(PARAM+4,"<hh",dx,dy)
    expect=run(0x08003b0c,x,y,PARAM)
    assert lib.ssa_line_side(x,y,ox,oy,dx,dy)==expect,(x,y,ox,oy,dx,dy)
    report["line_side_cases"]+=1
    b=Bounds(*[signed(rng.getrandbits(32)) for _ in range(4)])
    slope=i%6
    put(PARAM+24,"<H",slope); uc.mem_write(0x03004360,bytes(b))
    expect=run(0x08003b98,PARAM)
    assert lib.ssa_box_line_side(C.byref(b),ox,oy,dx,dy,slope)==expect
    report["box_side_cases"]+=1
print("PASS point and box classification: 18,000 cases",flush=True)

links=[]
def link_hook(machine,address,size,user):
    links.append(address)
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,link_hook,begin=0x0800a65c,end=0x0800a65c)
uc.hook_add(UC_HOOK_CODE,link_hook,begin=0x0800a5d0,end=0x0800a5d0)

for level in range(16):
    m=Map(); c=Collision()
    assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
    assert lib.ssa_map_collision_init(C.byref(c),C.byref(m))
    data=bytearray(C.string_at(m.data,m.size))
    original=bytearray(data)
    for off in range(0x1c,0x48,4):
        struct.pack_into("<I",original,off,BASE+struct.unpack_from("<I",original,off)[0])
    uc.mem_write(BASE,bytes(original))
    ox,oy,width,height=struct.unpack_from("<hhhh",data,m.blockmap)
    put(0x03004930,"<I",BASE); put(0x03004944,"<iihh",ox*65536,oy*65536,width,height)
    put(0x03004954,"<I",SCRATCH); put(0x03004640,"<I",GRID)
    uc.mem_write(GRID,bytes(width*height*4))
    vertices=list(struct.iter_unpack("<hh",data[m.vertices:m.vertices+m.vertex_count*4]))
    points=[(vx*65536,vy*65536) for vx,vy in vertices]
    points += [(rng.randrange(ox-128,ox+width*128+128)*65536+rng.randrange(65536),
                rng.randrange(oy-128,oy+height*128+128)*65536+rng.randrange(65536)) for _ in range(500)]
    for x,y in points:
        leaf=run(0x0800b370,BASE,x,y)
        assert lib.ssa_map_leaf(C.byref(m),x,y)==leaf,(level,x,y,leaf)
        report["leaf_cases"]+=1
        sector=run(0x0800b2c8,x,y)
        assert lib.ssa_map_sector(C.byref(m),x,y)==sector
        report["sector_cases"]+=1

    def query_outputs():
        n=get(0x03004458,"<I")
        assert n<=8,("original special-line array overflow",level,n)
        special=[(get(0x03004620+4*i,"<I")-BASE-m.lines)//40 for i in range(n)]
        ceiling_line=get(0x03004344,"<I")
        return (get(0x0300434c,"<i"),get(0x0300432c,"<i"),get(0x03004444,"<i"),
                (ceiling_line-BASE-m.lines)//40 if ceiling_line else -1,special)
    def native_outputs():
        return (c.floor,c.ceiling,c.low_floor,c.ceiling_line,list(c.special_lines[:c.special_count]))
    for case in range(400):
        vx,vy=rng.choice(vertices)
        x=vx*65536+rng.randrange(-64*65536,64*65536)
        y=vy*65536+rng.randrange(-64*65536,64*65536)
        nx=x+rng.randrange(-30*65536,30*65536); ny=y+rng.randrange(-30*65536,30*65536)
        radius=rng.choice((0,1,8,16,24,32))*65536
        raw=bytearray(rng.randbytes(0x74)); struct.pack_into("<ii",raw,0,x,y)
        struct.pack_into("<i",raw,0x28,radius)
        uc.mem_write(ENTITY,bytes(raw))
        expect=run(0x08003e5c,ENTITY,nx,ny)
        expected=query_outputs()
        actual=lib.ssa_map_query(C.byref(c),x,y,nx,ny,radius)
        assert actual==expect and native_outputs()==expected,(level,case,actual,expect,native_outputs(),expected)
        assert bytes(uc.mem_read(ENTITY,0x74))==bytes(raw)
        report["map_query_cases"]+=1

        # Use query heights to exercise exact clearance/step boundaries.
        z=(c.floor+rng.choice((-49,-48,-1,0,1,48,49,100)))*65536+rng.choice((0,1,65535))
        b=Body(x=x,y=y,z=z,velocity_z=rng.randrange(-600000,600000),
               height=rng.choice((1,48,64,100,128,200))*65536,flags=rng.choice((0,4,8,16,0xffff)),floor=-123,ceiling=234)
        for off,name in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
            struct.pack_into("<i",raw,off,getattr(b,name))
        struct.pack_into("<H",raw,0x30,b.flags); struct.pack_into("<hh",raw,0x70,b.ceiling,b.floor)
        uc.mem_write(ENTITY,bytes(raw)); links.clear()
        expect=run(0x080043d0,ENTITY,nx,ny); expected=query_outputs()
        actual=lib.ssa_map_try_move(C.byref(c),C.byref(b),radius,nx,ny)
        assert actual==expect and native_outputs()==expected,(level,case,"move",actual,expect)
        for off,name in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
            struct.pack_into("<i",raw,off,getattr(b,name))
        struct.pack_into("<hh",raw,0x70,b.ceiling,b.floor)
        assert bytes(uc.mem_read(ENTITY,0x74))==bytes(raw),(level,case,"entity bytes")
        assert links==([0x0800a65c,0x0800a5d0] if actual else []),(level,case,"links")
        report["move_cases"]+=1

    # Find a real open position and run the new jump using real map heights.
    for x,y in points:
        if lib.ssa_map_query(C.byref(c),x,y,x,y,16*65536)!=1 or c.ceiling-c.floor<180: continue
        b=Body(x=x,y=y,z=c.floor*65536,height=100*65536,flags=16,floor=c.floor,ceiling=c.ceiling)
        p=Player(eye_offset=0x600000,flags=4); j=Jump(); lib.ssa_jump_init(C.byref(j))
        base=b.z; apex=base; launched=0
        for frame in range(128):
            assert lib.ssa_map_try_move(C.byref(c),C.byref(b),16*65536,b.x,b.y)==1
            launched+=lib.ssa_player_vertical_step(C.byref(b),C.byref(p),C.byref(j),1,1,1)
            apex=max(apex,b.z)
            assert b.z>=base and b.z+b.height<=b.ceiling*65536
        assert launched==1 and apex>base and b.z==base and b.velocity_z<=0 and p.flags&0x20,(level,"jump",launched,apex,base,b.z,b.velocity_z)
        report["real_map_jump_scenarios"]+=1
        break
    else: raise AssertionError((level,"no open real-map jump position found"))

    # Exact player-start record used by 0x08005F14; original radius 32, height 100.
    start=Start(); assert lib.ssa_map_player_start(C.byref(m),C.byref(start))
    thing=run(0x0800b14c,BASE,1,0)
    assert thing
    sx,sy,heading=struct.unpack("<hhH",uc.mem_read(thing,6))
    assert (start.x,start.y,start.heading)==(sx*65536,sy*65536,heading)
    assert run(0x0800b14c,BASE,1,thing)==0 # exactly one player start per map
    sector=run(0x0800b2c8,start.x,start.y)
    floor,ceiling=struct.unpack_from("<hh",data,m.sectors+20*sector)
    assert (start.z,start.floor,start.ceiling)==(floor*65536,floor,ceiling)
    report["spawn_cases"]+=1
    b=Body(x=start.x,y=start.y,z=start.z,height=100*65536,flags=0x1a,floor=floor,ceiling=ceiling)
    p=Player(eye_offset=0x600000,flags=4); j=Jump(); lib.ssa_jump_init(C.byref(j))
    launch=0; apex=b.z; move_successes=0; base=b.z
    for frame in range(128):
        # Walking during the arc exercises changing wall/height queries.
        old_x=b.x; old_y=b.y
        result=lib.ssa_map_try_move(C.byref(c),C.byref(b),32*65536,b.x+65536,b.y)
        assert result in (0,1)
        if result==0: assert (b.x,b.y)==(old_x,old_y)
        else: move_successes+=1
        launch+=lib.ssa_player_vertical_step(C.byref(b),C.byref(p),C.byref(j),1,1,1)
        apex=max(apex,b.z)
        assert b.z>=b.floor*65536 and b.z+b.height<=b.ceiling*65536
    assert launch==1 and apex>base and b.z==b.floor*65536 and p.flags&0x20,(level,"spawn walk/jump")
    assert move_successes>0,(level,"spawn blocked all movement")
    report["spawn_jump_scenarios"]+=1

    # Native guard behavior is deliberately stricter than the original.
    node=m.nodes+(m.node_count-1)*32
    save=bytes(m.data[node:node+32])
    for children in ((m.node_count-1,m.node_count-1),(0x7ffe,0x7ffe),(0xffff,0xffff)):
        for j,v in enumerate(struct.pack("<HH",*children)): m.data[node+24+j]=v
        assert lib.ssa_map_leaf(C.byref(m),0,0)==-1
        report["rejected_geometry_cases"]+=1
    for j,v in enumerate(save): m.data[node+j]=v
    assert lib.ssa_map_query(C.byref(c),0,0,0,0,-1)==-1
    report["rejected_geometry_cases"]+=1
    assert lib.ssa_map_try_move(C.byref(c),None,0,0,0)==-1
    m.line_count+=1
    assert lib.ssa_map_query(C.byref(c),0,0,0,0,0)==-1
    m.line_count-=1
    block_words=bytes(m.data[0x1a:0x1c]); m.data[0x1a]=4; m.data[0x1b]=0
    assert lib.ssa_map_query(C.byref(c),0,0,0,0,0)==-1
    for j,v in enumerate(block_words): m.data[0x1a+j]=v
    report["rejected_geometry_cases"]+=3
    lib.ssa_map_collision_free(C.byref(c)); lib.ssa_map_assets_free(C.byref(m))
    print(f"PASS level {level}: BSP/sector lookup, 400 map queries, 400 moves, real-map jump",flush=True)

report["source_sha256"]={n:hashlib.sha256((ROOT/"src"/n).read_bytes()).hexdigest() for n in ("geometry.c","map_assets.c","physics.c")}
(ROOT/"analysis/native-geometry-validation.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))
