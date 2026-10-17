"""Differential tests: native C against unmodified AENP Thumb instructions.

Unicorn is only a test oracle. It is not linked into or used by src/input.c.
Run with the workspace Python under WSL. Requires GCC and requirements.txt.
"""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1, "Tests require the identified AENP ROM"
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2", "-shared", "-fPIC",
                "-I", str(ROOT / "include"), str(ROOT / "src/input.c"), "-o", str(BUILD / "libssa_input.so")], check=True)

U8P = C.POINTER(C.c_uint8)
class Input(C.Structure):
    _fields_ = [("raw_keys", C.c_uint32), ("keys", C.c_uint32),
                ("record_buffer", U8P), ("playback", U8P), ("record_count", C.c_uint16),
                ("repeat_count", C.c_uint8), ("repeat_keys", C.c_uint8),
                ("reset_state", C.c_uint8), ("flags", C.c_uint8)]

lib = C.CDLL(str(BUILD / "libssa_input.so"))
RESET = C.CFUNCTYPE(None, C.c_void_p)
lib.ssa_input_record.argtypes = [C.POINTER(Input), C.c_uint32]
lib.ssa_input_record.restype = None
lib.ssa_input_playback.argtypes = [C.POINTER(Input)]
lib.ssa_input_playback.restype = C.c_uint32
lib.ssa_input_poll.argtypes = [C.POINTER(Input), C.c_uint16, C.c_int, RESET, C.c_void_p]
lib.ssa_input_poll.restype = None

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM))
uc.mem_write(0x08000000, ROM)
uc.mem_map(0x03000000, 0x10000)
uc.mem_map(0x02000000, 0x40000)
uc.mem_map(0x04000000, 0x1000)
STATE, RECORD, PLAY, GAME, STOP = 0x030042F0, 0x02002000, 0x02004000, 0x02001000, 0x087FF000
CAPACITY = 0xE10
host_record = (C.c_uint8 * CAPACITY)()
host_play = (C.c_uint8 * 4)(9, 0x45, 0, 0xFF)
uc.mem_write(PLAY, bytes(host_play))
uc.mem_write(0x03004010, struct.pack("<I", GAME))
counts = {"original": 0, "native": 0}

def reset_hook(machine, address, size, userdata):
    counts["original"] += 1
    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

uc.hook_add(UC_HOOK_CODE, reset_hook, begin=0x08002F1C, end=0x08002F1C)

@RESET
def reset_native(_):
    counts["native"] += 1

def original(address, arg=0):
    uc.reg_write(UC_ARM_REG_R0, arg)
    uc.reg_write(UC_ARM_REG_SP, 0x03007E00)
    uc.reg_write(UC_ARM_REG_LR, STOP | 1)
    uc.emu_start(address | 1, STOP, count=3000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP, "Original routine failed to return"
    return uc.reg_read(UC_ARM_REG_R0)

def prepare(raw=0, keys=0, index=0, repeat=0, packed=0, flags=0, playback=True):
    C.memset(host_record, 0xA5, CAPACITY)
    uc.mem_write(RECORD, bytes(host_record))
    data = struct.pack("<IIIIIH8sHBBBB", raw, keys, 0, RECORD, PLAY if playback else 0,
                       0, bytes(8), index, repeat, packed, 7, flags)
    uc.mem_write(STATE, data)
    return Input(raw, keys, host_record, host_play if playback else U8P(), index, repeat, packed, 7, flags)

def compare(state):
    data = bytes(uc.mem_read(STATE, 36))
    assert state.raw_keys == struct.unpack_from("<I", data, 0)[0]
    assert state.keys == struct.unpack_from("<I", data, 4)[0]
    assert state.record_count == struct.unpack_from("<H", data, 30)[0]
    assert (state.repeat_count, state.repeat_keys, state.reset_state, state.flags) == tuple(data[32:36])
    pointer = C.cast(state.playback, C.c_void_p).value
    target = 0 if pointer is None else PLAY + pointer - C.addressof(host_play)
    assert struct.unpack_from("<I", data, 16)[0] == target
    assert bytes(host_record) == bytes(uc.mem_read(RECORD, CAPACITY))

report = {"rom_sha1": SHA1, "record_cases": 0, "playback_cases": 0, "poll_cases": 0}
for held in range(1024):
    for index in (0, 1, 0xE0E, 0xE0F, 0xFFFF):
        state = prepare(index=index)
        original(0x08003150, held)
        lib.ssa_input_record(C.byref(state), held)
        compare(state)
        report["record_cases"] += 1
print("PASS record: all 1024 button masks, five count/boundary cases", flush=True)

for packed in range(256):
    for repeat in (0, 1, 255):
        for playback in (False, True):
            state = prepare(packed=packed, repeat=repeat, playback=playback)
            expected = original(0x08003298)
            assert lib.ssa_input_playback(C.byref(state)) == expected
            compare(state)
            report["playback_cases"] += 1
print("PASS playback: all packed values, RLE boundaries, null and live streams", flush=True)

rng = random.Random(0xAE)  # deterministic previous button states
for held in range(1024):
    for flags in (0, 2, 4, 6):
        for paused in (0, 1):
            state = prepare(raw=rng.getrandbits(32), keys=rng.getrandbits(32),
                            index=(0, 0xE0E, 0xE0F)[held % 3], repeat=held % 3,
                            packed=held & 255, flags=flags)
            key_register = held ^ 0x3FF
            uc.mem_write(0x04000130, struct.pack("<H", key_register))
            uc.mem_write(GAME + 4, struct.pack("<H", paused))
            counts.update(original=0, native=0)
            original(0x08003028)
            lib.ssa_input_poll(C.byref(state), key_register, paused, reset_native, None)
            compare(state)
            assert counts["original"] == counts["native"]
            report["poll_cases"] += 1
print("PASS poll: all button masks, record/playback priority, pause, pressed edges and reset chord", flush=True)
report["total_cases"] = sum(report[k] for k in ("record_cases", "playback_cases", "poll_cases"))
report["result"] = "pass"
report["native_source_sha256"] = hashlib.sha256((ROOT / "src/input.c").read_bytes()).hexdigest()
(ROOT / "analysis/native-input-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
