"""Whole 08005CCC with controlled stage/script/animation dependencies.

Executes original 05C7C/05CA4 predicates; compares full player/entity/game/view
bytes and ordered boundary inputs. Unicorn is used only as a development oracle.
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
                str(ROOT / "src/player_frame.c"), "-o",
                str(BUILD / "libssa_player_frame.so")], check=True)

class Body(C.Structure):
    _fields_ = [(n, C.c_int32) for n in ("x", "y", "z", "velocity_x", "velocity_y", "velocity_z", "height")] + [
        ("flags", C.c_uint16), ("floor", C.c_int16), ("ceiling", C.c_int16),
        ("hover_offset", C.c_int8), ("step_adjust", C.c_int8)]
class Entity(C.Structure):
    _fields_ = [("body", Body), ("radius", C.c_int32), ("type", C.c_uint16),
                ("player_index", C.c_uint8), ("leaf", C.c_int), ("sector", C.c_int),
                ("cell", C.c_size_t), ("owner", C.c_void_p),
                ("block_next", C.c_void_p), ("block_previous", C.c_void_p),
                ("sector_next", C.c_void_p), ("sector_previous", C.c_void_p)]
class Movement(C.Structure):
    _fields_ = [("keys", C.c_uint32), ("weapon_flags", C.c_uint32)] + [
        (n, C.c_int32) for n in ("turn_velocity", "forward_velocity", "strafe_velocity", "impulse", "velocity_x", "velocity_y")] + [
        (n, C.c_uint16) for n in ("heading", "move_heading", "flags", "modifier_ticks", "impulse_heading")]
class Frame(C.Structure):
    _fields_ = [("entity", C.POINTER(Entity)), ("movement", C.POINTER(Movement)),
                ("x", C.c_int32), ("y", C.c_int32), ("eye_z", C.c_int32),
                ("timer", C.c_uint16), ("animation_state", C.c_uint8)]
class View(C.Structure):
    _fields_ = [("x", C.c_int32), ("y", C.c_int32), ("z", C.c_int32),
                ("heading", C.c_uint16), ("sector", C.c_int16)]
class Game(C.Structure):
    _fields_ = [("view", View)] + [(n, C.c_uint8) for n in
                 ("ticks", "player_count", "protected_player", "local_player")]
STAGE = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int, C.POINTER(Frame), C.POINTER(Game))
SECTOR = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int16)
ANIM = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(Frame), C.c_uint, C.c_uint)
class Ops(C.Structure):
    _fields_ = [("stage", STAGE), ("sector", SECTOR), ("animation", ANIM), ("context", C.c_void_p)]

lib = C.CDLL(str(BUILD / "libssa_player_frame.so"))
lib.ssa_player_frame_step.argtypes = [C.POINTER(Frame), C.POINTER(Game), C.POINTER(Ops)]
for name in ("starts", "stops"):
    getattr(lib, "ssa_player_" + name + "_moving").argtypes = [C.POINTER(Movement)]

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
PLAYER, ENTITY, GAME, VIEW, STOP = 0x02000000, 0x02000400, 0x02000500, 0x03004020, 0x087ff000
uc.mem_write(0x03004648, struct.pack("<I", PLAYER))
uc.mem_write(0x03004010, struct.pack("<I", GAME))
rng = random.Random(0x5ccc)
report = {"rom_sha1": SHA1, "boundary_cases": 0, "random_cases": 0,
          "consecutive_frames": 0, "predicate_cases": 0, "native_guards": 0,
          "scope": "whole 08005CCC and actual 05C7C/05CA4 predicates; death, movement, camera, hazards, sector script and animation dependencies controlled; full player/entity/game/view bytes and ordered calls; not full gameplay"}

def u16(raw, offset): return struct.unpack_from("<H", raw, offset)[0]
def i16(raw, offset): return struct.unpack_from("<h", raw, offset)[0]
def i32(raw, offset): return struct.unpack_from("<i", raw, offset)[0]
def u32(raw, offset): return struct.unpack_from("<I", raw, offset)[0]

def unpack(raw):
    p, e, g, v, local = raw
    ent = Entity(); ent.body.flags = u16(e, 0x30); ent.player_index = e[0x54]; ent.sector = i16(e, 0x6a)
    mov = Movement(); mov.keys = u32(p, 0xa4); mov.flags = u16(p, 0xbc); mov.heading = u16(p, 0xb8)
    frame = Frame(C.pointer(ent), C.pointer(mov), i32(p, 0), i32(p, 4), i32(p, 0x18), u16(e, 0x4e), e[0x3e])
    game = Game(View(i32(v, 0), i32(v, 4), i32(v, 8), u16(v, 12), i16(v, 0x18)), g[7], g[0x10], g[0x68], local)
    return frame, game

def pack(frame, game, raw):
    p, e, g, v = (bytearray(r) for r in raw[:4])
    ent, mov = frame.entity.contents, frame.movement.contents
    struct.pack_into("<ii", p, 0, frame.x, frame.y); struct.pack_into("<i", p, 0x18, frame.eye_z)
    struct.pack_into("<I", p, 0xa4, mov.keys); struct.pack_into("<H", p, 0xb8, mov.heading); struct.pack_into("<H", p, 0xbc, mov.flags)
    struct.pack_into("<H", e, 0x30, ent.body.flags); struct.pack_into("<H", e, 0x4e, frame.timer)
    struct.pack_into("<h", e, 0x6a, ent.sector); e[0x54] = ent.player_index; e[0x3e] = frame.animation_state
    g[7], g[0x10], g[0x68] = game.ticks, game.player_count, game.protected_player
    struct.pack_into("<iiiH", v, 0, game.view.x, game.view.y, game.view.z, game.view.heading)
    struct.pack_into("<h", v, 0x18, game.view.sector)
    return bytes(p), bytes(e), bytes(g), bytes(v), game.local_player

def read_rom():
    return (bytes(uc.mem_read(PLAYER + index * 256, 256)), bytes(uc.mem_read(ENTITY, 0x74)),
            bytes(uc.mem_read(GAME, 0x80)), bytes(uc.mem_read(VIEW, 0x1c)), uc.mem_read(0x0300464e, 1)[0])

def write_rom(raw):
    for address, data in zip((PLAYER + index * 256, ENTITY, GAME, VIEW), raw[:4]):
        uc.mem_write(address, bytes(data))
    uc.mem_write(0x0300464e, bytes((raw[4],)))

class Script:
    def __init__(self, changes, fail=-1):
        self.changes, self.calls, self.fail = changes, [], fail
    def call(self, kind, args, frame, game, raw):
        self.calls.append((kind, args, pack(frame, game, raw)))
        if kind == self.fail:
            return -1
        changes = self.changes[kind] if kind < 5 else {}
        for field, value in changes.items():
            if field in ("timer", "animation_state", "x", "y", "eye_z"):
                setattr(frame, field, value)
            elif field in ("keys", "flags", "heading"):
                setattr(frame.movement.contents, field, value)
            elif field == "body_flags": frame.entity.contents.body.flags = value
            elif field == "sector": frame.entity.contents.sector = value
            elif field == "player_index": frame.entity.contents.player_index = value
            else: setattr(game, field, value)
        return 0

addresses = {0x08006154: 0, 0x080064b8: 1, 0x080062d4: 2, 0x08005680: 3, 0x0801c624: 4, 0x08005394: 5}
def hook(machine, address, size, data):
    raw = read_rom(); f, g = unpack(raw); kind = addresses[address]
    if kind < 4:
        assert uc.reg_read(UC_ARM_REG_R0) == PLAYER + index * 256
        args = ()
    elif kind == 4:
        args = (C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value,)
    else:
        assert uc.reg_read(UC_ARM_REG_R0) == ENTITY
        args = (uc.reg_read(UC_ARM_REG_R1), uc.reg_read(UC_ARM_REG_R2))
    active.call(kind, args, f, g, raw); write_rom(pack(f, g, raw))
    uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
for address in addresses:
    uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

@STAGE
def stage(context, kind, frame, game):
    return active.call(kind, (), frame.contents, game.contents, base)
@SECTOR
def sector(context, number):
    return active.call(4, (number,), native_frame, native_game, base)
@ANIM
def animation(context, frame, number, ticks):
    return active.call(5, (number, ticks), frame.contents, native_game, base)
ops = Ops(stage, sector, animation, None)

def compare(raw, changes):
    global index, active, base, native_frame, native_game
    base = raw; index = raw[1][0x54]
    write_rom(raw)
    active = Script(changes)
    uc.reg_write(UC_ARM_REG_R0, ENTITY); uc.reg_write(UC_ARM_REG_SP, 0x03007e00); uc.reg_write(UC_ARM_REG_LR, STOP | 1)
    uc.emu_start(0x08005ccd, STOP, count=10000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    expected, calls = read_rom(), active.calls
    active = Script(changes); native_frame, native_game = unpack(raw)
    assert lib.ssa_player_frame_step(C.byref(native_frame), C.byref(native_game), C.byref(ops)) == 0
    got = pack(native_frame, native_game, raw)
    assert got == expected, [(n, [(i, a, b) for i, (a, b) in enumerate(zip(got[n], expected[n])) if a != b]) for n in range(4)]
    assert active.calls == calls, (active.calls, calls)
    return got

def fresh():
    p, e, g, v = (bytearray(rng.randbytes(n)) for n in (256, 0x74, 0x80, 0x1c))
    struct.pack_into("<I", p, 0xac, ENTITY)
    e[0x54] = rng.randrange(4)
    return bytes(p), bytes(e), bytes(g), bytes(v), rng.randrange(4)

none = [{} for _ in range(5)]
for count, flags, timer, ticks, state, keys, locked in itertools.product(
        (1, 2), (0, 0x81, 0xffff), (0, 1, 2, 255, 32767, 32768, 65535),
        (0, 1, 255), (0, 1, 2), (0, 0x40), (0, 0x10)):
    raw = list(fresh()); p, e, g = (bytearray(r) for r in raw[:3])
    struct.pack_into("<I", p, 0xa4, keys); struct.pack_into("<H", p, 0xbc, locked)
    struct.pack_into("<H", e, 0x30, flags); struct.pack_into("<H", e, 0x4e, timer); e[0x3e] = state
    g[7], g[0x10], g[0x68] = ticks, count, rng.choice((e[0x54], (e[0x54] + 1) % 4))
    raw[:3] = bytes(p), bytes(e), bytes(g)
    compare(tuple(raw), none); report["boundary_cases"] += 1

def changes():
    result = []
    for _ in range(5):
        values = {"x": rng.randint(-2147483648, 2147483647), "y": rng.randint(-2147483648, 2147483647),
                  "eye_z": rng.randint(-2147483648, 2147483647), "timer": rng.randrange(65536),
                  "animation_state": rng.randrange(4), "keys": rng.getrandbits(32),
                  "flags": rng.randrange(65536), "heading": rng.randrange(65536), "body_flags": rng.randrange(65536),
                  "sector": rng.randint(-32768, 32767), "player_index": rng.randrange(4),
                  "ticks": rng.randrange(256), "local_player": rng.randrange(4), "player_count": rng.randrange(5)}
        result.append({k: v for k, v in values.items() if rng.randrange(4) == 0})
    return result

for _ in range(4000):
    compare(fresh(), changes()); report["random_cases"] += 1
for _ in range(64):
    raw = fresh()
    for frame in range(32):
        raw = compare(raw, changes()); report["consecutive_frames"] += 1

# Isolated original predicates, every input button pattern and both flag states.
for keys, flags in itertools.product(range(1024), (0, 0x10)):
    p = bytearray(256); struct.pack_into("<I", p, 0xa4, keys); struct.pack_into("<H", p, 0xbc, flags)
    m = Movement(); m.keys, m.flags = keys, flags
    for address, name in ((0x08005c7c, "starts"), (0x08005ca4, "stops")):
        uc.mem_write(PLAYER, bytes(p)); uc.reg_write(UC_ARM_REG_R0, PLAYER)
        uc.reg_write(UC_ARM_REG_LR, STOP | 1); uc.emu_start(address | 1, STOP, count=100)
        assert getattr(lib, "ssa_player_" + name + "_moving")(C.byref(m)) == uc.reg_read(UC_ARM_REG_R0)
        report["predicate_cases"] += 1

# Required integration boundaries fail before mutation; failures stop later stages.
base = fresh(); native_frame, native_game = unpack(base)
for args in ((None, C.byref(native_game), C.byref(ops)), (C.byref(native_frame), None, C.byref(ops)),
             (C.byref(native_frame), C.byref(native_game), None)):
    assert lib.ssa_player_frame_step(*args) == -1; report["native_guards"] += 1
for missing in range(3):
    broken = Ops(stage, sector, animation, None)
    setattr(broken, ("stage", "sector", "animation")[missing], (STAGE, SECTOR, ANIM)[missing]())
    before = pack(native_frame, native_game, base)
    assert lib.ssa_player_frame_step(C.byref(native_frame), C.byref(native_game), C.byref(broken)) == -1
    assert pack(native_frame, native_game, base) == before; report["native_guards"] += 1
for failed in range(6):
    native_frame, native_game = unpack(base)
    native_frame.animation_state = 0; native_frame.movement.contents.keys = 0x40; native_frame.movement.contents.flags = 0
    active = Script(none, failed)
    assert lib.ssa_player_frame_step(C.byref(native_frame), C.byref(native_game), C.byref(ops)) == -1
    assert [call[0] for call in active.calls] == list(range(failed + 1)); report["native_guards"] += 1
assert lib.ssa_player_starts_moving(None) == 0 and lib.ssa_player_stops_moving(None) == 0
report["native_guards"] += 2
report["rom_comparisons"] = sum(report[k] for k in ("boundary_cases", "random_cases", "consecutive_frames", "predicate_cases"))
report["source_sha256"] = hashlib.sha256((ROOT / "src/player_frame.c").read_bytes()).hexdigest()
(ROOT / "analysis/native-player-frame-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
