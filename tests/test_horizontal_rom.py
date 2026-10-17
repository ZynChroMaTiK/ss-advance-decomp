"""Whole 08004BDC differential oracle with controlled move/slide dependencies.

This validates timestep control flow, not the geometry/slide implementations.
Only 080043D0 and 08004A38 are hooked. Entire entity bytes and ordered dependency
calls (including their incoming body state) are compared after each frame.
"""
from pathlib import Path
import ctypes as C
import hashlib
import itertools
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                "-pedantic", "-shared", "-fPIC", "-fsanitize=undefined",
                "-fno-sanitize-recover=all", "-I", str(ROOT / "include"),
                str(ROOT / "src/horizontal.c"), "-o",
                str(BUILD / "libssa_horizontal.so")], check=True)

class Body(C.Structure):
    _fields_ = [(name, C.c_int32) for name in
                ("x", "y", "z", "velocity_x", "velocity_y", "velocity_z", "height")] + [
        ("flags", C.c_uint16), ("floor", C.c_int16), ("ceiling", C.c_int16),
        ("hover_offset", C.c_int8), ("step_adjust", C.c_int8)]

MOVE = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(Body), C.c_int32, C.c_int32)
CLASSIFY = C.CFUNCTYPE(C.c_int, C.c_void_p)
SLIDE = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(Body))

class Ops(C.Structure):
    _fields_ = [("try_move", MOVE), ("blocked_by_entity", CLASSIFY),
                ("slide", SLIDE), ("context", C.c_void_p)]

lib = C.CDLL(str(BUILD / "libssa_horizontal.so"))
lib.ssa_horizontal_step.argtypes = [C.POINTER(Body), C.c_uint8, C.c_uint8,
                                   C.c_uint8, C.c_uint8, C.POINTER(Ops)]
lib.ssa_horizontal_step.restype = C.c_int
FIELDS = [(0, "i", "x"), (4, "i", "y"), (8, "i", "z"),
          (12, "i", "velocity_x"), (16, "i", "velocity_y"),
          (20, "i", "velocity_z"), (44, "i", "height"),
          (48, "H", "flags"), (0x72, "h", "floor"), (0x70, "h", "ceiling"),
          (0x43, "b", "hover_offset"), (0x53, "b", "step_adjust")]

def unpack(raw):
    return Body(**{name: struct.unpack_from("<" + fmt, raw, offset)[0]
                   for offset, fmt, name in FIELDS})

def pack(body, raw):
    result = bytearray(raw)
    for offset, fmt, name in FIELDS:
        struct.pack_into("<" + fmt, result, offset, getattr(body, name))
    return bytes(result)

def state(body):
    return tuple(getattr(body, name) for _, _, name in FIELDS)

def signed(value):
    return C.c_int32(value).value

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM))
uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000)
uc.mem_map(0x03000000, 0x10000)
ENTITY, GAME, STOP = 0x02000000, 0x02000100, 0x087ff000
uc.mem_write(0x03004010, struct.pack("<I", GAME))
rng = random.Random(0x4BDC)
report = {"rom_sha1": SHA1, "boundary_cases": 0, "random_cases": 0,
          "consecutive_frames": 0, "invalid_api_cases": 0,
          "move_calls": 0, "slide_calls": 0,
          "scope": "whole 08004BDC, hooking only move 080043D0 and slide 08004A38; whole entity bytes and ordered dependency calls; controlled collision responses, not actual geometry"}

class Script:
    def __init__(self, responses, slide_delta):
        self.responses = responses
        self.slide_delta = slide_delta
        self.calls = []
        self.move_count = 0
        self.entity = 0

    def move(self, body, x, y):
        self.calls.append(("move", x, y, state(body)))
        result, self.entity = self.responses[self.move_count % len(self.responses)]
        self.move_count += 1
        if result > 0:
            body.x, body.y = x, y
            # Valid commits can also update sector heights.
            body.floor = -100 + self.move_count % 20
            body.ceiling = 300 + self.move_count % 30
        return result

    def slide(self, body):
        self.calls.append(("slide", state(body)))
        dx, dy, vx, vy = self.slide_delta
        body.x, body.y = signed(body.x + dx), signed(body.y + dy)
        body.velocity_x, body.velocity_y = vx, vy
        return 0

active = None

def rom_hook(machine, address, size, user):
    assert machine.reg_read(UC_ARM_REG_R0) == ENTITY
    raw = bytes(machine.mem_read(ENTITY, 0x74))
    body = unpack(raw)
    if address == 0x080043d0:
        result = active.move(body, signed(machine.reg_read(UC_ARM_REG_R1)),
                             signed(machine.reg_read(UC_ARM_REG_R2)))
        machine.mem_write(0x03004328, struct.pack("<I", 0x02000300 if active.entity else 0))
    else:
        result = active.slide(body)
    machine.mem_write(ENTITY, pack(body, raw))
    machine.reg_write(UC_ARM_REG_R0, result & 0xffffffff)
    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

uc.hook_add(UC_HOOK_CODE, rom_hook, begin=0x080043d0, end=0x080043d0)
uc.hook_add(UC_HOOK_CODE, rom_hook, begin=0x08004a38, end=0x08004a38)

@MOVE
def move_callback(context, body, x, y):
    return active.move(body.contents, x, y)

@CLASSIFY
def classify_callback(context):
    return active.entity

@SLIDE
def slide_callback(context, body):
    return active.slide(body.contents)

ops = Ops(move_callback, classify_callback, slide_callback, None)

def compare(raw, ticks, count, local, responses, delta=(123, -456, 100, -200)):
    global active
    body = unpack(raw)
    uc.mem_write(ENTITY, raw)
    game = bytearray(rng.randbytes(0x80))
    game[7], game[0x10], game[0x68] = ticks, count, local
    uc.mem_write(GAME, bytes(game))
    uc.mem_write(0x03004328, bytes(4))
    active = Script(responses, delta)
    uc.reg_write(UC_ARM_REG_R0, ENTITY)
    uc.reg_write(UC_ARM_REG_SP, 0x03007e00)
    uc.reg_write(UC_ARM_REG_LR, STOP | 1)
    uc.emu_start(0x08004bdd, STOP, count=200000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    expected = bytes(uc.mem_read(ENTITY, 0x74))
    expected_calls = active.calls
    assert bytes(uc.mem_read(GAME, 0x80)) == bytes(game)
    active = Script(responses, delta)
    result = lib.ssa_horizontal_step(C.byref(body), raw[0x54], ticks, count, local, C.byref(ops))
    assert result == 0
    actual = pack(body, raw)
    assert actual == expected, (report, ticks, count, local, responses, state(body), state(unpack(expected)))
    assert active.calls == expected_calls, (report, ticks, responses, active.calls, expected_calls)
    report["move_calls"] += active.move_count
    report["slide_calls"] += sum(call[0] == "slide" for call in active.calls)
    return actual

def make_raw(vx, vy, flags=8, index=0):
    raw = bytearray(rng.randbytes(0x74))
    struct.pack_into("<ii", raw, 12, vx, vy)
    struct.pack_into("<H", raw, 48, flags)
    raw[0x54] = index
    return bytes(raw)

limit = 30 * 65536
edges = (-2147483648, -limit - 1, -limit, -limit + 1, -1, 0, 1,
         limit - 1, limit, limit + 1, 2147483647)
# Solid walls, wall-slide, entity Y-only, entity X-only, full blockage and mixed
# response sequences; velocity mutations must not replace the batch increments.
scripts = ([(1, 0)], [(0, 0)], [(0, 1), (1, 1)],
           [(0, 1), (0, 1), (1, 0)], [(0, 1)],
           [(1, 0), (0, 1), (1, 0), (0, 0)])
for vx, vy, flags, ticks in itertools.product(edges, edges, (0, 8, 16, 24), (0, 1, 4)):
    for responses in scripts:
        compare(make_raw(vx, vy, flags), ticks, 1, 0, responses)
        report["boundary_cases"] += 1
print(f"PASS horizontal boundaries: {report['boundary_cases']:,}", flush=True)

for case in range(6000):
    vx, vy = (signed(rng.getrandbits(32)) for _ in range(2))
    if case % 11 == 0:
        vx = vy = 0
    flags = rng.choice((0, 8, 16, 24, 0xffff))
    index, local = rng.randrange(4), rng.randrange(4)
    responses = [(rng.randrange(2), rng.randrange(2)) for _ in range(rng.randrange(1, 12))]
    compare(make_raw(vx, vy, flags, index), rng.choice((0, 1, 2, 4, 8, 32, 255)),
            rng.choice((0, 1, 2, 4, 255)), local, responses,
            tuple(signed(rng.getrandbits(32)) for _ in range(4)))
    report["random_cases"] += 1
print("PASS horizontal randomized frames: 6,000", flush=True)

for stream in range(16):
    raw = make_raw(3 * 65536, -4 * 65536, 24)
    for frame in range(128):
        # Renew requested velocities periodically, retaining all output state.
        if frame % 7 == 0:
            b = unpack(raw)
            b.velocity_x, b.velocity_y = rng.choice(edges), rng.choice(edges)
            raw = pack(b, raw)
        raw = compare(raw, (1, 2, 4, 8)[frame % 4], 1, 0, scripts[(frame + stream) % len(scripts)])
        report["consecutive_frames"] += 1

# Native-only failure contracts: original code has no callback/API equivalent.
def invalid(body, ticks, options, expected=-1):
    actual = lib.ssa_horizontal_step(C.byref(body) if body is not None else None,
                                     0, ticks, 1, 0, C.byref(options) if options is not None else None)
    assert actual == expected, (actual, expected)
    report["invalid_api_cases"] += 1

invalid(None, 0, None)
invalid(Body(velocity_x=1), 1, None)
invalid(Body(velocity_x=1), 1, Ops())
invalid(Body(velocity_x=1), 1, Ops(move_callback, CLASSIFY(), slide_callback, None))
invalid(Body(), 1, None, 0)
b = Body(velocity_x=2147483647)
invalid(b, 0, None, 0)
assert b.velocity_x == limit
active = Script([(0, 0)], (0, 0, 0, 0))
invalid(Body(velocity_x=1, flags=8), 1, Ops(move_callback, classify_callback, SLIDE(), None))
active = Script([(-1, 0)], (0, 0, 0, 0))
invalid(Body(velocity_x=1), 1, ops)

@SLIDE
def failed_slide(context, body):
    return -1

active = Script([(0, 0)], (0, 0, 0, 0))
invalid(Body(velocity_x=1, flags=8), 1, Ops(move_callback, classify_callback, failed_slide, None))

@CLASSIFY
def failed_classification(context):
    return -1

active = Script([(0, 1), (1, 1)], (100, 200, 300, 400))
b = Body(x=1234, y=5678, velocity_x=1, velocity_y=2, flags=8)
before = state(b)
invalid(b, 1, Ops(move_callback, failed_classification, slide_callback, None))
assert state(b) == before
assert len(active.calls) == 1 and active.calls[0][0] == "move"
for responses in ([(0, 1), (-1, 1)], [(0, 1), (0, 1), (-1, 1)]):
    active = Script(responses, (0, 0, 0, 0))
    invalid(Body(velocity_x=1, flags=8), 1, ops)

report["rom_comparisons"] = sum(report[key] for key in ("boundary_cases", "random_cases", "consecutive_frames"))
report["source_sha256"] = hashlib.sha256((ROOT / "src/horizontal.c").read_bytes()).hexdigest()
report["compiler"] = "GCC C11 -O2 -Wall -Wextra -Werror -pedantic; UBSan, nonrecovering"
(ROOT / "analysis/native-horizontal-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
