"""Whole 08012CFC and E1 wrapper 0801B848 differential tests; no hooked calls."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                "-pedantic", "-shared", "-fPIC", "-fsanitize=undefined",
                "-fno-sanitize-recover=all", "-I", str(ROOT / "include"),
                str(ROOT / "src/entity_timer.c"), "-o", str(BUILD / "libssa_entity_timer.so")], check=True)
lib = C.CDLL(str(BUILD / "libssa_entity_timer.so"))
step = lib.ssa_entity_timer_step
step.argtypes = [C.POINTER(C.c_uint16), C.POINTER(C.c_uint16), C.c_uint]
step.restype = C.c_int
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM))
uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000)
uc.mem_map(0x03000000, 0x10000)
ENTITY, GAME, STOP = 0x02000000, 0x02000100, 0x087ff000
uc.mem_write(0x03004010, struct.pack("<I", GAME))
rng = random.Random(0x12cfc)
report = {"rom_sha1": SHA1, "scope": "whole 08012CFC and 0801B848; no hooks; complete entity/game bytes; defined 12CFC return only (wrapper returns void)",
          "rom_comparisons": {}, "native_guards": 0}

def compare(timer, flags, ticks, wrapper=False):
    data = bytearray(rng.randbytes(0x74))
    struct.pack_into("<H", data, 0x30, flags)
    struct.pack_into("<H", data, 0x4e, timer)
    game = bytearray(rng.randbytes(0x80))
    game[7] = ticks
    uc.mem_write(ENTITY, bytes(data))
    uc.mem_write(GAME, bytes(game))
    for reg, value in ((UC_ARM_REG_R0, ENTITY), (UC_ARM_REG_SP, 0x03007e00), (UC_ARM_REG_LR, STOP | 1)):
        uc.reg_write(reg, value)
    uc.emu_start((0x0801b848 if wrapper else 0x08012cfc) | 1, STOP, count=100)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    native_flags, native_timer = C.c_uint16(flags), C.c_uint16(timer)
    actual_result = step(C.byref(native_flags), C.byref(native_timer), ticks)
    struct.pack_into("<H", data, 0x30, native_flags.value)
    struct.pack_into("<H", data, 0x4e, native_timer.value)
    assert bytes(data) == bytes(uc.mem_read(ENTITY, len(data))), (timer, flags, ticks, wrapper)
    assert bytes(game) == bytes(uc.mem_read(GAME, len(game)))
    if not wrapper:
        assert actual_result == uc.reg_read(UC_ARM_REG_R0), (timer, flags, ticks)
    key = "wrapper" if wrapper else "countdown"
    report["rom_comparisons"][key] = report["rom_comparisons"].get(key, 0) + 1

# Every raw timer representation with the flag set and clear; varying timesteps.
for timer in range(65536):
    for enabled in (False, True):
        flags = rng.randrange(65536) & 0xff7f
        if enabled:
            flags |= 0x80
        compare(timer, flags, (timer * 73 + 19) & 255)
# Every possible byte timestep around zero, signed threshold and wrap edges.
for ticks in range(256):
    for timer in (0, 1, 29, 30, 31, ticks, (ticks + 1) & 65535,
                  (ticks - 1) & 65535, 32767, 32768, 32769, 65535):
        for flags in (0, 0x80, 0xff7f, 0xffff):
            compare(timer, flags, ticks)
for _ in range(4096):
    compare(rng.randrange(65536), rng.randrange(65536), rng.randrange(256), wrapper=True)

flags, timer = C.c_uint16(0xffff), C.c_uint16(0xabcd)
for f, t, ticks in ((None, C.byref(timer), 1), (C.byref(flags), None, 1),
                    (None, None, 1), (C.byref(flags), C.byref(flags), 1),
                    (C.byref(flags), C.byref(timer), 256),
                    (C.byref(flags), C.byref(timer), 0xffffffff)):
    assert step(f, t, ticks) == -1
    assert (flags.value, timer.value) == (0xffff, 0xabcd)
    report["native_guards"] += 1
report["total_rom_comparisons"] = sum(report["rom_comparisons"].values())
report["source_sha256"] = hashlib.sha256((ROOT / "src/entity_timer.c").read_bytes()).hexdigest()
report["passed"] = True
(ROOT / "analysis/native-entity-timer-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
