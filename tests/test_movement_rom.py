"""Native movement/math vs AENP instructions; Unicorn is test-only.

Movement stops at 0x08006A3E, before entity/collision calls. Player count is one:
this validates controls and vectors, not multiplayer push or collision/position.
Only the BIOS integer-division service is replaced; trig and fixed multiply run
their original code, including the multiply routine copied into IWRAM.
"""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic", "-shared", "-fPIC",
                "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I", str(ROOT / "include"),
                str(ROOT / "src/movement.c"), "-o", str(BUILD / "libssa_movement.so")], check=True)
class Tables(C.Structure):
    _fields_ = [("sine", C.c_uint16 * 16384), ("direction", C.c_uint16 * 32769)]
class Movement(C.Structure):
    _fields_ = [("keys", C.c_uint32), ("weapon_flags", C.c_uint32),
                *[(n, C.c_int32) for n in ("turn_velocity", "forward_velocity", "strafe_velocity", "impulse", "velocity_x", "velocity_y")],
                *[(n, C.c_uint16) for n in ("heading", "move_heading", "flags", "modifier_ticks", "impulse_heading")]]
lib = C.CDLL(str(BUILD / "libssa_movement.so"))
lib.ssa_math_tables_load.argtypes = [C.POINTER(Tables), C.c_void_p, C.c_size_t]
lib.ssa_math_tables_load.restype = C.c_int
lib.ssa_sincos.argtypes = [C.POINTER(Tables), C.c_uint16, C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
lib.ssa_sincos.restype = None
lib.ssa_direction.argtypes = [C.POINTER(Tables), C.c_int16, C.c_int16]
lib.ssa_direction.restype = C.c_uint16
lib.ssa_movement_controls.argtypes = [C.POINTER(Movement), C.c_uint8]
lib.ssa_movement_controls.restype = None
lib.ssa_movement_vector.argtypes = [C.POINTER(Movement), C.POINTER(Tables)]
lib.ssa_movement_vector.restype = None
tables = Tables()
assert not lib.ssa_math_tables_load(C.byref(tables), ROM, 0x34931)
assert lib.ssa_math_tables_load(C.byref(tables), ROM, len(ROM))
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM))
uc.mem_write(0x08000000, ROM)
uc.mem_map(0x03000000, 0x10000)
uc.mem_map(0x02000000, 0x40000)
uc.mem_write(0x030000F0, ROM[0x370:0x380])
GAME, PLAYER, ENTITY, OUTPUT, STOP = 0x02001000, 0x02002000, 0x02003000, 0x02004000, 0x087FF000
uc.mem_write(0x03004010, struct.pack("<I", GAME))
uc.mem_write(GAME+0x10, b"\1")
def signed(v):
    return v if v < 0x80000000 else v - 0x100000000
def bios_div(machine, address, size, data):
    a, b = signed(machine.reg_read(UC_ARM_REG_R0)), signed(machine.reg_read(UC_ARM_REG_R1))
    assert b != 0, "Unexpected division by zero"
    q = (abs(a) // abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
    machine.reg_write(UC_ARM_REG_R0, q & 0xffffffff)
    machine.reg_write(UC_ARM_REG_R1, (a-q*b) & 0xffffffff)
    machine.reg_write(UC_ARM_REG_R3, abs(q) & 0xffffffff)
    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE, bios_div, begin=0x08002C88, end=0x08002C88)
def run(start, a=0, b=0, c=0, end=STOP):
    for reg, val in ((UC_ARM_REG_R0,a), (UC_ARM_REG_R1,b), (UC_ARM_REG_R2,c),
                     (UC_ARM_REG_SP,0x03007E00), (UC_ARM_REG_LR, STOP|1)):
        uc.reg_write(reg, val & 0xffffffff)
    uc.emu_start(start|1, end, count=8000)
    assert uc.reg_read(UC_ARM_REG_PC) == end, hex(uc.reg_read(UC_ARM_REG_PC))
    return uc.reg_read(UC_ARM_REG_R0)
report = {"rom_sha1": SHA1, "sincos_cases": 0, "direction_cases": 0, "movement_cases": 0,
          "scope": "single-player controls and vectors through 0x08006A3E, before collision/entity sync; BIOS division stub only"}
s, c = C.c_int32(), C.c_int32()
for angle in range(65536):
    run(0x080094E8, angle, OUTPUT, OUTPUT+4)
    lib.ssa_sincos(C.byref(tables), angle, C.byref(s), C.byref(c))
    assert (s.value, c.value) == struct.unpack("<ii", uc.mem_read(OUTPUT, 8)), angle
    report["sincos_cases"] += 1
print("PASS sin/cos: all 65,536 headings", flush=True)
rng = random.Random(0xAE09)
axes = [-32768, -32767, -256, -1, 0, 1, 256, 32766, 32767]
for x, y in [(x,y) for x in axes for y in axes] + [(rng.randrange(-32768,32768),rng.randrange(-32768,32768)) for _ in range(4096)]:
    expected = run(0x080093E8, x, y, 0)
    actual = lib.ssa_direction(C.byref(tables), x, y)
    assert actual == expected, (x,y,actual,expected)
    report["direction_cases"] += 1
print("PASS direction: boundary axes and 4,096 seeded vectors", flush=True)
offsets = {"turn_velocity":0x24, "forward_velocity":0xb0, "strafe_velocity":0xb4,
           "impulse":0x20, "velocity_x":0xc, "velocity_y":0x10,
           "heading":0xb8, "move_heading":0xba, "flags":0xbc, "modifier_ticks":0xc2, "impulse_heading":0xdc}
def check(state, ticks):
    data = bytearray(256)
    struct.pack_into("<I", data, 0xac, ENTITY)
    struct.pack_into("<I", data, 0xa4, state.keys)
    struct.pack_into("<I", data, 0x30, state.weapon_flags)
    for name, offset in offsets.items():
        struct.pack_into("<i" if offset in (0x24,0xb0,0xb4,0x20,0xc,0x10) else "<H", data, offset, getattr(state,name))
    uc.mem_write(PLAYER, bytes(data))
    uc.mem_write(GAME+7, bytes([ticks]))
    before = bytes(state)
    run(0x080064B8, PLAYER, end=0x08006A3E)
    lib.ssa_movement_controls(C.byref(state), ticks)
    lib.ssa_movement_vector(C.byref(state), C.byref(tables))
    actual_rom = bytes(uc.mem_read(PLAYER, 256))
    for name, offset in offsets.items():
        expected = struct.unpack_from("<i" if offset in (0x24,0xb0,0xb4,0x20,0xc,0x10) else "<H", actual_rom, offset)[0]
        assert getattr(state,name) == expected, {"field":name,"got":getattr(state,name),"expected":expected,"ticks":ticks,"before":before.hex()}
    report["movement_cases"] += 1
for keys in range(1024):
    for flags in (0,1,2,3,4,0x10,0x14,0xffff):
        for ticks in (0,1,3,255):
            st = Movement(keys | rng.randrange(1024)<<16, rng.choice((0,0x100)),
                rng.choice((-0x1900000,-0x640001,-0x640000,-1,0,1,0x640000,0x640001,0x1900000)),
                rng.choice((-0x80000,-1,0,1,0x80000)), rng.choice((-0x80000,-1,0,1,0x80000)),
                rng.choice((-1,0,1,0x10000,0x80000)), 0,0,
                rng.randrange(65536),0,flags,rng.choice((0,1,8,9,15,16,65535)),rng.randrange(65536))
            check(st,ticks)
print("PASS controls/vector: 32,768 button/flag/timestep combinations", flush=True)
# Stateful sequences detect interactions that isolated random states can miss.
for sequence in range(16):
    st = Movement(heading=rng.randrange(65536))
    old_keys = 0
    for frame in range(256):
        keys = rng.randrange(1024) if frame % 16 == 0 else old_keys
        st.keys = keys | (keys & ~old_keys)<<16
        old_keys = keys
        check(st,1)
print("PASS movement: 4,096 consecutive frames in 16 sequences", flush=True)
report["source_sha256"] = hashlib.sha256((ROOT / "src/movement.c").read_bytes()).hexdigest()
report["total_cases"] = sum(v for k,v in report.items() if k.endswith("_cases"))
(ROOT / "analysis/native-movement-validation.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))
