"""Whole-routine ROM differential tests plus native jump behavior scenarios."""
from pathlib import Path
import ctypes as C
import hashlib
import itertools
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic", "-shared", "-fPIC",
                "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I", str(ROOT / "include"),
                str(ROOT / "src/physics.c"), str(ROOT / "src/movement.c"), "-o", str(BUILD / "libssa_physics.so")], check=True)
class Body(C.Structure):
    _fields_ = [(n,C.c_int32) for n in ("x","y","z","velocity_x","velocity_y","velocity_z","height")] + [
        ("flags",C.c_uint16),("floor",C.c_int16),("ceiling",C.c_int16),("hover_offset",C.c_int8),("step_adjust",C.c_int8)]
class Player(C.Structure):
    _fields_ = [("eye_offset",C.c_int32),("eye_velocity",C.c_int32),("flags",C.c_uint16),("lowered",C.c_uint16)]
class Camera(C.Structure):
    _fields_ = [("eye_z",C.c_int32),("bob_amplitude",C.c_int32)]
class Frame(C.Structure):
    _fields_ = [("previous_bob",C.c_int32),("counter",C.c_uint16),("is_local",C.c_uint8),("footsteps_muted",C.c_uint8)]
class Tables(C.Structure):
    _fields_ = [("sine",C.c_uint16*16384),("direction",C.c_uint16*32769)]
class Jump(C.Structure):
    _fields_ = [("launch_velocity",C.c_int32),("was_down",C.c_uint8)]
lib = C.CDLL(str(BUILD / "libssa_physics.so"))
lib.ssa_vertical_step.argtypes = [C.POINTER(Body),C.POINTER(Player),C.c_uint8]
lib.ssa_vertical_step.restype = C.c_int
lib.ssa_jump_init.argtypes = [C.POINTER(Jump)]
lib.ssa_jump_init.restype = None
lib.ssa_player_vertical_step.argtypes = [C.POINTER(Body),C.POINTER(Player),C.POINTER(Jump),C.c_uint8,C.c_int,C.c_int]
lib.ssa_player_vertical_step.restype = C.c_int
lib.ssa_camera_step.argtypes = [C.POINTER(Body),C.POINTER(Player),C.POINTER(Camera),C.POINTER(Frame),C.POINTER(Tables)]
lib.ssa_camera_step.restype = C.c_int
lib.ssa_math_tables_load.argtypes = [C.POINTER(Tables),C.c_void_p,C.c_size_t]
lib.ssa_math_tables_load.restype = C.c_int
tables = Tables()
assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM))
uc.mem_write(0x08000000,ROM)
uc.mem_map(0x03000000,0x10000)
uc.mem_map(0x02000000,0x40000)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
GAME, ENTITY, PLAYERS, STOP = 0x02001000,0x02002000,0x02003000,0x087ff000
uc.mem_write(0x03004010,struct.pack("<I",GAME))
uc.mem_write(0x03004648,struct.pack("<I",PLAYERS))
rng = random.Random(0x4CE0)
report = {"rom_sha1":SHA1,"vertical_cases":0,"camera_cases":0,"jump_scenarios":0,
          "scope":"whole 0x08004CE0 routine without mocked calls; 0x080062D4 camera with audio event hook only; native jump tested separately"}

def signed(v): return C.c_int32(v).value
def check(b,p,ticks):
    index = rng.randrange(4)
    entity = bytearray(rng.randbytes(0x74))
    players = bytearray(rng.randbytes(0x400))
    for offset,name in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
        struct.pack_into("<i",entity,offset,getattr(b,name))
    struct.pack_into("<H",entity,0x30,b.flags)
    struct.pack_into("<b",entity,0x43,b.hover_offset)
    entity[0x54] = index
    struct.pack_into("<hh",entity,0x70,b.ceiling,b.floor)
    struct.pack_into("<H",players,index*256+0xbc,p.flags)
    struct.pack_into("<ii",players,index*256+0xd0,p.eye_offset,p.eye_velocity)
    uc.mem_write(ENTITY,bytes(entity))
    uc.mem_write(PLAYERS,bytes(players))
    uc.mem_write(GAME+7,bytes([ticks]))
    before = (bytes(b).hex(),bytes(p).hex(),ticks)
    uc.reg_write(UC_ARM_REG_R0,ENTITY)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00)
    uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x08004ce1,STOP,count=2000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    result = lib.ssa_vertical_step(C.byref(b),C.byref(p),ticks)
    assert result == uc.reg_read(UC_ARM_REG_R0),before
    # Compare complete RAM objects, including untouched bytes and other players.
    for offset,name in ((0,"x"),(4,"y"),(8,"z"),(12,"velocity_x"),(16,"velocity_y"),(20,"velocity_z"),(44,"height")):
        struct.pack_into("<i",entity,offset,getattr(b,name))
    struct.pack_into("<H",entity,0x30,b.flags)
    struct.pack_into("<b",entity,0x43,b.hover_offset)
    struct.pack_into("<hh",entity,0x70,b.ceiling,b.floor)
    struct.pack_into("<H",players,index*256+0xbc,p.flags)
    struct.pack_into("<ii",players,index*256+0xd0,p.eye_offset,p.eye_velocity)
    assert bytes(entity) == bytes(uc.mem_read(ENTITY,len(entity))),before
    assert bytes(players) == bytes(uc.mem_read(PLAYERS,len(players))),before
    report["vertical_cases"] += 1

for floor,flags,ticks,offset,velocity,hover in itertools.product(
        (-100,0,100),(0,0x10,0x100,0x110,0xffff),(0,1,2,8,255),
        (-65537,-1,0,1,65535,65536,100*65536),
        (-6*65536,-1,0,1,5*65536,-2147483648,2147483647),(-128,-1,0,1,127)):
    b = Body(z=floor*65536+offset,velocity_z=velocity,height=100*65536,
             flags=flags,floor=floor,ceiling=floor+rng.choice((99,100,101,256)),hover_offset=hover)
    p = Player(rng.choice((-2147483648,0,0x180000,0x600000,2147483647)),rng.randrange(-65536,65536),rng.randrange(65536))
    check(b,p,ticks)
for _ in range(4096):
    b = Body(*[signed(rng.getrandbits(32)) for _ in range(7)],rng.randrange(65536),
             rng.randrange(-32768,32768),rng.randrange(-32768,32768),rng.randrange(-128,128))
    p = Player(signed(rng.getrandbits(32)),signed(rng.getrandbits(32)),rng.randrange(65536))
    check(b,p,rng.randrange(256))
print(f"PASS original vertical physics: {report['vertical_cases']} cases",flush=True)

SETTINGS = 0x02004000
uc.mem_write(0x0300572c,struct.pack("<I",SETTINGS))
events = []
def audio_event(machine,address,size,data):
    events.append(machine.reg_read(UC_ARM_REG_R0))
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,audio_event,begin=0x08008d5c,end=0x08008d5c)
def camera_check(b,p,c,f):
    entity = bytearray(rng.randbytes(0x74))
    player = bytearray(rng.randbytes(0x100))
    struct.pack_into("<iii",entity,8,b.z,b.velocity_x,b.velocity_y)
    struct.pack_into("<b",entity,0x53,b.step_adjust)
    entity[0x54] = 1
    struct.pack_into("<hh",entity,0x70,b.ceiling,b.floor)
    struct.pack_into("<I",player,0xac,ENTITY)
    struct.pack_into("<H",player,0xbc,p.flags)
    struct.pack_into("<iiH",player,0xd0,p.eye_offset,p.eye_velocity,p.lowered)
    struct.pack_into("<i",player,0x18,c.eye_z)
    struct.pack_into("<i",player,0xe0,c.bob_amplitude)
    uc.mem_write(ENTITY,bytes(entity))
    uc.mem_write(PLAYERS,bytes(player))
    uc.mem_write(GAME+8,struct.pack("<H",f.counter))
    uc.mem_write(GAME+0x68,bytes([1 if f.is_local else 0]))
    uc.mem_write(SETTINGS+0x2c,bytes([f.footsteps_muted]))
    uc.mem_write(0x03006aa4,struct.pack("<i",f.previous_bob))
    before = tuple(bytes(s).hex() for s in (b,p,c,f))
    events.clear()
    uc.reg_write(UC_ARM_REG_R0,PLAYERS)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00)
    uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x080062d5,STOP,count=2000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    event = lib.ssa_camera_step(C.byref(b),C.byref(p),C.byref(c),C.byref(f),C.byref(tables))
    assert events == ([event] if event else []),before
    struct.pack_into("<H",player,0xbc,p.flags)
    struct.pack_into("<iiH",player,0xd0,p.eye_offset,p.eye_velocity,p.lowered)
    struct.pack_into("<i",player,0x18,c.eye_z)
    struct.pack_into("<i",player,0xe0,c.bob_amplitude)
    assert bytes(uc.mem_read(ENTITY,len(entity))) == bytes(entity),before
    assert bytes(uc.mem_read(PLAYERS,len(player))) == bytes(player),before
    assert f.previous_bob == struct.unpack("<i",uc.mem_read(0x03006aa4,4))[0],before
    report["camera_cases"] += 1

for flags,local,muted,lowered,counter,offset,velocity in itertools.product(
        (0,4,0x10,0x14,0xffff),(0,1),(0,1),(0,1),(0,8,18,35,65535),
        (0x17ffff,0x180000,0x180001,0x600000,0x600001),(-65536,-16384,0,1,65536)):
    b = Body(z=rng.choice((-65536,0,1,50*65536)),velocity_x=3*65536,velocity_y=-2*65536,
             height=100*65536,flags=0x10,floor=0,ceiling=rng.choice((24,100,256)),step_adjust=rng.choice((-5,0,5)))
    p = Player(offset,velocity,flags,lowered)
    c = Camera(12345,65432)
    f = Frame(rng.choice((-65536,-1,0,1,65536)),counter,local,muted)
    camera_check(b,p,c,f)
for _ in range(4096):
    b = Body(z=signed(rng.getrandbits(32)),velocity_x=signed(rng.getrandbits(32)),velocity_y=signed(rng.getrandbits(32)),
             floor=rng.randrange(-32768,32768),ceiling=rng.randrange(-32768,32768),step_adjust=rng.randrange(-128,128))
    p = Player(signed(rng.getrandbits(32)),signed(rng.getrandbits(32)),rng.randrange(65536),rng.choice((0,1,65535)))
    c = Camera(signed(rng.getrandbits(32)),signed(rng.getrandbits(32)))
    f = Frame(signed(rng.getrandbits(32)),rng.randrange(65536),rng.randrange(2),rng.randrange(2))
    camera_check(b,p,c,f)
print(f"PASS original camera: {report['camera_cases']} cases",flush=True)

UNIT = 65536
def fresh(floor=0,ceiling=256):
    b = Body(x=123,y=-456,z=floor*UNIT,velocity_x=3*UNIT,velocity_y=-2*UNIT,
             velocity_z=-UNIT//2,height=100*UNIT,flags=0x10,floor=floor,ceiling=ceiling)
    p = Player(0x600000,0,0x20)
    j = Jump()
    lib.ssa_jump_init(C.byref(j))
    return b,p,j
def step(state,down,ticks=1,enabled=1):
    b,p,j = state
    return lib.ssa_player_vertical_step(C.byref(b),C.byref(p),C.byref(j),ticks,down,enabled)
def passed(): report["jump_scenarios"] += 1

state = fresh()
b,p,j = state
assert step(state,1) == 1 and b.z == 5*UNIT and not p.flags & 0x20
peak = b.z
for _ in range(127):
    assert step(state,1) == 0
    peak = max(peak,b.z)
    assert b.z >= 0 and b.z+b.height <= 256*UNIT
assert peak == 52*UNIT+UNIT//2 and b.z == 0 and p.flags & 0x20
assert (b.x,b.y,b.velocity_x,b.velocity_y) == (123,-456,3*UNIT,-2*UNIT)
assert step(state,0) == 0 and step(state,1) == 1
passed() # full arc, no hold-repeat, release/repress, horizontal preservation

state = fresh()
assert step(state,1) == 1
step(state,0)
assert step(state,1) == 0
for _ in range(128): assert step(state,1) == 0
assert state[0].z == 0
passed() # no air jump or queued auto-jump on landing

state = fresh(ceiling=102)
assert step(state,1) == 1
assert state[0].z == 2*UNIT and state[0].velocity_z == 0
for _ in range(32): step(state,1)
assert state[0].z == 0
passed() # low-ceiling contact and landing

state = fresh(ceiling=100)
assert step(state,1) == 0 and state[0].z == 0
passed() # no headroom

for condition in ("locked","disabled","nonplayer","hover","no_gravity","airborne","rising"):
    state = fresh()
    b,p,j = state
    if condition == "locked": p.flags |= 0x10
    if condition == "nonplayer": b.flags = 0
    if condition == "hover": b.hover_offset = 5
    if condition == "no_gravity": b.flags |= 0x100
    if condition == "airborne": b.z = UNIT
    if condition == "rising": b.velocity_z = UNIT
    assert step(state,1,enabled=condition != "disabled") == 0
    assert j.was_down == 1
    passed()

state = fresh()
before = tuple(bytes(x) for x in state)
assert step(state,1,ticks=0) == 0 and tuple(bytes(x) for x in state) == before
assert step(state,1) == 1
passed() # paused frame does not consume input or alter physics

state = fresh()
assert step(state,1,enabled=0) == 0
assert step(state,1,enabled=1) == 0
assert step(state,0) == 0 and step(state,1) == 1
passed() # disabled control edge is consumed

for speed in (0,-1,2*UNIT,2147483647):
    state = fresh(ceiling=1000)
    state[2].launch_velocity = speed
    assert step(state,1) == (speed > 0)
    assert state[0].z == (min(speed,30*UNIT) if speed > 0 else 0)
    passed()

for floor in (-100,0,100):
    for ticks in (1,2,8,32,128,255):
        a,b = fresh(floor,floor+256),fresh(floor,floor+256)
        assert step(a,1,ticks=ticks) == 1
        for i in range(ticks): assert step(b,1) == (i == 0)
        assert tuple(bytes(x) for x in a) == tuple(bytes(x) for x in b)
        passed()

state = fresh()
b,p,j = state
b.velocity_x = b.velocity_y = 0
c,frame = Camera(),Frame(is_local=1)
for tick in range(128):
    step(state,1)
    frame.counter = tick
    event = lib.ssa_camera_step(C.byref(b),C.byref(p),C.byref(c),C.byref(frame),C.byref(tables))
    assert c.eye_z == b.z + 96*UNIT and event == 0
assert b.z == 0 and c.eye_z == 96*UNIT
passed() # camera tracks body throughout jump and landing
print(f"PASS native jump: {report['jump_scenarios']} scenarios",flush=True)
report["source_sha256"] = hashlib.sha256((ROOT / "src/physics.c").read_bytes()).hexdigest()
report["movement_source_sha256"] = hashlib.sha256((ROOT / "src/movement.c").read_bytes()).hexdigest()
(ROOT / "analysis/native-physics-validation.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))
