"""Whole FC04, 116BC and E460 differential tests using actual ROM animation data."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-shared", "-fPIC", "-fsanitize=undefined", "-fno-sanitize-recover=all",
                "-I", str(ROOT / "include"), str(ROOT / "src/weapon_animation.c"),
                "-o", str(BUILD / "libssa_weapon_animation.so")], check=True)
lib = C.CDLL(str(BUILD / "libssa_weapon_animation.so"))

class Object(C.Structure):
    _fields_ = [("link", C.c_uint32), ("type", C.c_uint16), ("subtype", C.c_uint16),
                ("slot", C.c_uint16), ("active_index", C.c_uint16),
                ("x", C.c_int32), ("y", C.c_int32), ("screen_x", C.c_int16), ("screen_y", C.c_int16),
                ("previous_frame", C.c_uint16), ("frame", C.c_uint16), ("flags", C.c_uint32),
                ("animation", C.c_uint16), ("frame_index", C.c_uint16),
                ("period", C.c_uint8), ("elapsed", C.c_uint8), ("done", C.c_uint8), ("unknown27", C.c_uint8),
                ("scale_x", C.c_uint16), ("scale_y", C.c_uint16), ("unknown2c", C.c_uint16),
                ("layer", C.c_uint8), ("owner", C.c_uint8)]

class Animations(C.Structure):
    _fields_ = [("words", C.c_uint16 * 528), ("offsets", C.c_uint16 * 233),
                ("lengths", C.c_uint16 * 233), ("loaded", C.c_uint)]

class Control(C.Structure):
    _fields_ = [("current", C.c_uint32), ("flags", C.c_uint32),
                ("requested", C.c_uint16), ("keys", C.c_uint16)]

assert C.sizeof(Object) == 48
load = lib.ssa_weapon_animations_load
load.argtypes = [C.POINTER(Animations), C.c_void_p, C.c_size_t]
reset = lib.ssa_weapon_object_init
reset.argtypes = [C.POINTER(Object)]
set_animation = lib.ssa_weapon_animation_set
set_animation.argtypes = [C.POINTER(Object), C.c_uint16, C.c_uint16]
idle = lib.ssa_weapon_animation_idle
idle.argtypes = [C.POINTER(Object), C.POINTER(Control)]
step = lib.ssa_weapon_animation_step
step.argtypes = [C.POINTER(Object), C.POINTER(Animations), C.c_uint16, C.c_uint]
for fn in (load, reset, set_animation, idle, step): fn.restype = C.c_int
animations = Animations()
rom_buffer = C.create_string_buffer(ROM)
assert load(C.byref(animations), rom_buffer, len(ROM)) == 1
del rom_buffer  # Copied animation data has independent ownership.

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM))
uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000)
uc.mem_map(0x03000000, 0x10000)
OBJECT, CONTROL, GAME, ACTIVE, STOP = 0x02000000, 0x02002000, 0x02003000, 0x02004000, 0x087ff000
uc.mem_write(0x03004010, struct.pack("<I", GAME))
uc.mem_write(0x0300559c, struct.pack("<I", OBJECT))
uc.mem_write(0x030055c0, struct.pack("<II", ACTIVE, 1))
uc.mem_write(ACTIVE, struct.pack("<I", 0))
rng = random.Random(0xe460fc04)
report = {"rom_sha1": SHA1,
          "scope": "Whole FC04,116BC,E460; no instruction hooks. Complete 48-byte objects and control bytes. Actual 233 ROM animation sequences plus explicit marker-data mutations.",
          "rom_comparisons": {}, "native_guards": 0,
          "animations": 233, "largest_sequence_words": max(animations.lengths),
          "native_initializer_checks": 0}

def run(address, r0=0, r1=0, r2=0):
    for register, value in ((UC_ARM_REG_R0, r0), (UC_ARM_REG_R1, r1), (UC_ARM_REG_R2, r2),
                            (UC_ARM_REG_SP, 0x03007e00), (UC_ARM_REG_LR, STOP | 1)):
        uc.reg_write(register, value)
    uc.emu_start(address | 1, STOP, count=200000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP, hex(address)

def count(key): report["rom_comparisons"][key] = report["rom_comparisons"].get(key, 0) + 1

def compare_set(animation, period):
    obj = Object.from_buffer_copy(rng.randbytes(48))
    uc.mem_write(OBJECT, bytes(obj))
    run(0x0800fc04, OBJECT, animation, period)
    result = set_animation(C.byref(obj), animation, period)
    assert result == int(animation <= 0xe8 or animation == 0xffff)
    assert bytes(obj) == bytes(uc.mem_read(OBJECT, 48)), (animation, period)
    count("set")

for animation in range(65536): compare_set(animation, rng.randrange(65536))
for period in range(65536): compare_set(period % 233, period)
print("FC04 exhaustive IDs/periods passed", flush=True)

for i in range(8192):
    obj = Object.from_buffer_copy(rng.randbytes(48))
    current = i if i < 256 else rng.randrange(1 << 32)
    control = Control(current, rng.randrange(1 << 32), rng.randrange(65536), rng.randrange(65536))
    original = bytearray(rng.randbytes(0x20))
    struct.pack_into("<I", original, 0, control.current)
    struct.pack_into("<I", original, 8, control.flags)
    uc.mem_write(OBJECT, bytes(obj))
    uc.mem_write(CONTROL, bytes(original))
    run(0x080116bc, OBJECT, CONTROL)
    assert idle(C.byref(obj), C.byref(control)) >= 0
    struct.pack_into("<I", original, 8, control.flags)
    assert bytes(obj) == bytes(uc.mem_read(OBJECT, 48)), current
    assert bytes(original) == bytes(uc.mem_read(CONTROL, 0x20)), current
    count("idle")

def make_object(animation, frame_index=None):
    obj = Object.from_buffer_copy(rng.randbytes(48))
    obj.type = rng.choice((0, 1, 2, 4, 18, 65535))
    obj.animation = animation
    obj.frame_index = (rng.randrange(animations.lengths[animation] - 1) if frame_index is None else frame_index) if animation != 65535 else rng.randrange(65536)
    return obj

def compare_step(obj, flags, ticks, key="tick", data=animations):
    game = bytearray(rng.randbytes(0x80))
    struct.pack_into("<H", game, 4, flags)
    game[7] = ticks
    uc.mem_write(OBJECT, bytes(obj))
    uc.mem_write(GAME, bytes(game))
    run(0x0800e460)
    before = bytes(obj)
    assert step(C.byref(obj), C.byref(data), flags, ticks) == 1
    assert bytes(obj) == bytes(uc.mem_read(OBJECT, 48)), (before.hex(), flags, ticks, bytes(obj).hex(), bytes(uc.mem_read(OBJECT, 48)).hex())
    assert bytes(game) == bytes(uc.mem_read(GAME, len(game)))
    count(key)

# Every original sequence and timestep with randomized valid starting frames,
# elapsed bytes, periods, flags and preexisting completion state.
for animation in range(233):
    for ticks in range(256):
        obj = make_object(animation)
        obj.period = (ticks * 73 + animation) & 255
        compare_step(obj, rng.randrange(65536), ticks)
print("All233 real sequences/every timestep passed", flush=True)

# Consecutive stateful playback: observe completion at its actual tick, then
# restart only through the recovered setter, including byte wrap at long ticks.
for animation in range(233):
    for period in (0, 1, 3, 5, 255):
        obj = make_object(animation, 0)
        assert set_animation(C.byref(obj), animation, period) == 1
        for frame in range(64):
            compare_step(obj, 0, (1, 2, 3, 7, 255)[frame % 5], "consecutive")

for flags in (0, 1, 65535):
    for ticks in range(256):
        obj = make_object(65535)
        compare_step(obj, flags, ticks, "disabled")

# The special marker bytes are legal original branches. Exercise all four even
# if a sequence is absent in the currently loaded game's animation table.
modified = Animations.from_buffer_copy(bytes(animations))
marker_address = 0x0844af4c
saved_words = bytes(uc.mem_read(marker_address, 16))
saved_pointer = bytes(uc.mem_read(0x0844b36c, 4))
uc.mem_write(0x0844b36c, struct.pack("<I", marker_address))
for marker in range(0xfffa, 0xfffe):
    sequence = (10, marker, 20, 30, 0xffff)
    uc.mem_write(marker_address, struct.pack("<5H", *sequence))
    modified.offsets[0], modified.lengths[0] = 0, 5
    for index, value in enumerate(sequence): modified.words[index] = value
    for ticks in range(256):
        obj = make_object(0, 0)
        obj.flags = 0
        obj.period = 3
        obj.elapsed = 0
        compare_step(obj, 0, ticks, "markers", modified)
uc.mem_write(marker_address, saved_words)
uc.mem_write(0x0844b36c, saved_pointer)

# Original active-index iteration with84 stable slots in shuffled active order.
objects = [make_object(i % 233, 0) for i in range(84)]
for obj in objects: obj.period, obj.elapsed = 3, 0
order = list(range(84)); rng.shuffle(order)
uc.mem_write(ACTIVE, struct.pack("<84I", *order))
uc.mem_write(0x030055c4, struct.pack("<I", 84))
for frame in range(256):
    flags, ticks = frame & 1, frame & 255
    game = bytearray(0x80)
    struct.pack_into("<H", game, 4, flags); game[7] = ticks
    uc.mem_write(GAME, bytes(game))
    uc.mem_write(OBJECT, b"".join(bytes(obj) for obj in objects))
    run(0x0800e460)
    for index in order: assert step(C.byref(objects[index]), C.byref(animations), flags, ticks) == 1
    assert b"".join(bytes(obj) for obj in objects) == bytes(uc.mem_read(OBJECT, 84 * 48))
    count("active_pool_frames")

obj = Object()
assert reset(C.byref(obj)) == 1
assert obj.animation == obj.previous_frame == 65535 and obj.flags == obj.layer == 1
assert obj.scale_x == obj.scale_y == 4096
report["native_initializer_checks"] += 1
assert reset(None) == -1 and set_animation(None, 0, 1) == -1
report["native_guards"] += 2
control = Control(1, 0xffffffff, 2, 3)
before_control, before_object = bytes(control), bytes(obj)
for o, c in ((None, C.byref(control)), (C.byref(obj), None), (None, None)):
    assert idle(o, c) == -1
    assert bytes(control) == before_control and bytes(obj) == before_object
    report["native_guards"] += 1

for mode in range(10):
    obj = make_object(0, 0)
    bad = Animations.from_buffer_copy(bytes(animations))
    ticks = 1
    if mode == 0: bad.loaded = 0
    if mode == 1: bad.loaded = 2
    if mode == 2: ticks = 256
    if mode == 3: obj.animation = 233
    if mode == 4: obj.frame_index = 65535
    if mode == 5: bad.offsets[0] = 65535
    if mode == 6: bad.lengths[0] = 0
    if mode == 7: bad.lengths[0] = 65535
    before = bytes(obj)
    assert step(None if mode == 8 else C.byref(obj), None if mode == 9 else C.byref(bad), 0, ticks) == -1
    assert bytes(obj) == before
    report["native_guards"] += 1

for mode in range(8):
    output = Animations.from_buffer_copy(bytes(animations))
    bad_rom = bytearray(ROM)
    if mode == 0: struct.pack_into("<I", bad_rom, 0x44b36c, 0)
    if mode == 1: struct.pack_into("<I", bad_rom, 0x44b36c, 0x0844af4d)
    if mode == 2: struct.pack_into("<I", bad_rom, 0x44b36c, 0x0844b36c)
    if mode == 3: bad_rom[0x44af4c:0x44b36c] = bytes(1056)
    if mode == 4:
        struct.pack_into("<I", bad_rom, 0x44b36c, 0x0844af4c)
        struct.pack_into("<H", bad_rom, 0x44af4c, 65535)
    buf = C.create_string_buffer(bytes(bad_rom))
    before = bytes(output)
    assert load(None if mode == 5 else C.byref(output), None if mode == 6 else buf, 100 if mode == 7 else len(ROM)) == -1
    assert bytes(output) == before
    report["native_guards"] += 1

report["total_rom_comparisons"] = sum(report["rom_comparisons"].values())
report["source_sha256"] = hashlib.sha256((ROOT / "src/weapon_animation.c").read_bytes()).hexdigest()
report["passed"] = True
(ROOT / "analysis/native-weapon-animation-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
