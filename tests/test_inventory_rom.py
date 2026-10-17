"""Compare native inventory helpers against whole original Thumb routines.

No ROM dependencies are hooked. Compare complete player/entity byte arrays,
including unchanged bytes, and actual defined return values. Unicorn is oracle
only; the native library reads a data table from the supplied ROM.
"""
from pathlib import Path
import ctypes as C
import hashlib
import itertools
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
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
                str(ROOT / "src/inventory.c"), "-o",
                str(BUILD / "libssa_inventory.so")], check=True)

class Inventory(C.Structure):
    _fields_ = [("weapons", C.c_uint16), ("ammo", C.c_int16 * 10),
                ("pending_count", C.c_uint16), ("pending_weapons", C.c_uint32 * 10),
                ("health", C.c_int16), ("stat_c4", C.c_int16), ("stat_c6", C.c_int16)]

class Rules(C.Structure):
    _fields_ = [("ammo_capacity", C.c_uint16 * 10)]

lib = C.CDLL(str(BUILD / "libssa_inventory.so"))
I, R = C.POINTER(Inventory), C.POINTER(Rules)
signatures = {
    "rules_load": ([R, C.c_void_p, C.c_size_t], C.c_int),
    "has_weapon": ([I, C.c_uint], C.c_int),
    "queue_weapon": ([I, C.c_uint], C.c_int),
    "add_ammo": ([I, R, C.c_uint, C.c_int, C.c_uint], C.c_int),
    "fill_ammo": ([I, R, C.c_uint], C.c_int),
    "add_health": ([I, C.c_int], None),
    "add_stat_c4": ([I, C.c_int], None),
    "add_stat_c6": ([I, C.c_int], None),
}
for name, (args, result) in signatures.items():
    fn = getattr(lib, "ssa_inventory_" + name)
    fn.argtypes, fn.restype = args, result
rules = Rules()
rom_buffer = C.create_string_buffer(ROM)
assert lib.ssa_inventory_rules_load(C.byref(rules), rom_buffer, len(ROM)) == 0
expected_capacities = [struct.unpack_from("<H", ROM, 0x44bc8c + i * 12 + 6)[0] for i in range(10)]
assert list(rules.ammo_capacity) == expected_capacities

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM))
uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000)
uc.mem_map(0x03000000, 0x10000)
PLAYER, ENTITY, GAME, STOP = 0x02000000, 0x02000100, 0x02000200, 0x087ff000
uc.mem_write(0x03004010, struct.pack("<I", GAME))
rng = random.Random(0x11558)
report = {"rom_sha1": SHA1, "ammo_capacities": expected_capacities,
          "scope": "whole 080114B8,08011558,080114D4,08011534,0800549C,080054D8,08005510; no hooked calls; complete player/entity bytes and defined returns; queue count bounded below 10", 
          "rom_comparisons": {}, "native_guard_cases": 0}

def unpack(player, entity):
    inv = Inventory()
    inv.weapons = struct.unpack_from("<H", player, 0x34)[0]
    inv.ammo[:] = struct.unpack_from("<10h", player, 0x36)
    inv.pending_count = struct.unpack_from("<H", player, 0x7a)[0]
    inv.pending_weapons[:] = struct.unpack_from("<10I", player, 0x7c)
    inv.health = struct.unpack_from("<h", entity, 0x32)[0]
    inv.stat_c4, inv.stat_c6 = struct.unpack_from("<hh", player, 0xc4)
    return inv

def pack(inv, player, entity):
    p, e = bytearray(player), bytearray(entity)
    struct.pack_into("<H10h", p, 0x34, inv.weapons, *inv.ammo)
    struct.pack_into("<H10I", p, 0x7a, inv.pending_count, *inv.pending_weapons)
    struct.pack_into("<hh", p, 0xc4, inv.stat_c4, inv.stat_c6)
    struct.pack_into("<h", e, 0x32, inv.health)
    return bytes(p), bytes(e)

def fresh():
    p, e = bytearray(rng.randbytes(256)), bytearray(rng.randbytes(0x74))
    struct.pack_into("<I", p, 0xac, ENTITY)
    struct.pack_into("<H", p, 0x7a, rng.randrange(10))
    return p, e

ADDRESSES = {"has_weapon": 0x080114b8, "queue_weapon": 0x08011558,
             "add_ammo": 0x080114d4, "fill_ammo": 0x08011534,
             "add_health": 0x0800549c, "add_stat_c4": 0x080054d8,
             "add_stat_c6": 0x08005510}

def compare(name, p, e, arg1=0, arg2=0, count=1):
    p, e = bytes(p), bytes(e)
    inv = unpack(p, e)
    uc.mem_write(PLAYER, p)
    uc.mem_write(ENTITY, e)
    game = bytearray(rng.randbytes(0x80))
    game[0x10] = count
    uc.mem_write(GAME, bytes(game))
    for reg, val in ((UC_ARM_REG_R0, PLAYER), (UC_ARM_REG_R1, arg1),
                     (UC_ARM_REG_R2, arg2), (UC_ARM_REG_SP, 0x03007e00),
                     (UC_ARM_REG_LR, STOP | 1)):
        uc.reg_write(reg, val & 0xffffffff)
    uc.emu_start(ADDRESSES[name] | 1, STOP, count=10000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    expected = bytes(uc.mem_read(PLAYER, 256)), bytes(uc.mem_read(ENTITY, 0x74))
    result = uc.reg_read(UC_ARM_REG_R0)
    fn = getattr(lib, "ssa_inventory_" + name)
    if name == "add_ammo":
        actual_result = fn(C.byref(inv), C.byref(rules), arg1, arg2, count)
    elif name == "fill_ammo":
        actual_result = fn(C.byref(inv), C.byref(rules), count)
        assert actual_result == 0
    else:
        actual_result = fn(C.byref(inv), arg1)
    actual = pack(inv, p, e)
    assert actual == expected, (name, arg1, arg2, count, [(i, a, b) for i, (a, b) in enumerate(zip(actual[0], expected[0])) if a != b], actual[1].hex(), expected[1].hex())
    if name in ("has_weapon", "queue_weapon", "add_ammo"):
        assert actual_result == result, (name, actual_result, result)
    assert bytes(uc.mem_read(GAME, 0x80)) == bytes(game)
    report["rom_comparisons"][name] = report["rom_comparisons"].get(name, 0) + 1
    return actual

# Every ownership mask in the valid ten-bit domain, and every valid weapon.
for mask, weapon in itertools.product(range(1024), range(1, 11)):
    p, e = fresh()
    struct.pack_into("<H", p, 0x34, mask)
    compare("has_weapon", p, e, weapon)

for _ in range(2000):
    p, e = fresh()
    compare("queue_weapon", p, e, rng.randrange(1, 11))

amounts = (-2147483648, -65537, -32768, -1, 0, 1, 5, 10, 25, 50,
           99, 100, 101, 2000, 32767, 32768, 65535, 65536, 65636, 2147483647)
for weapon, owned, count, amount in itertools.product(range(1, 11), (False, True), (0, 1, 2, 255), amounts):
    for ammo in (-32768, -1, 0, max(0, expected_capacities[weapon-1]-1),
                 min(32767, expected_capacities[weapon-1]), 32767):
        if ammo > 32767:
            continue
        p, e = fresh()
        struct.pack_into("<H", p, 0x34, (1 << (weapon-1)) if owned else 0)
        struct.pack_into("<h", p, 0x36 + (weapon-1)*2, ammo)
        compare("add_ammo", p, e, weapon, amount, count)
for _ in range(2000):
    p, e = fresh()
    compare("add_ammo", p, e, rng.randrange(1, 11), rng.randint(-2147483648, 2147483647), rng.randrange(256))
for _ in range(1000):
    p, e = fresh()
    compare("fill_ammo", p, e, count=rng.choice((0, 1, 2, 255)))

for name, offset in (("add_health", 0x32), ("add_stat_c4", 0xc4), ("add_stat_c6", 0xc6)):
    for value, amount in itertools.product((-32768, -32767, -101, -1, 0, 1, 49, 50, 99, 100, 101, 199, 200, 32766, 32767), amounts):
        p, e = fresh()
        struct.pack_into("<h", e if name == "add_health" else p, offset, value)
        compare(name, p, e, amount)
    for _ in range(2000):
        p, e = fresh()
        compare(name, p, e, rng.randint(-2147483648, 2147483647))

# Native guard behavior has no corresponding safe original behavior.
def guard(result, expected):
    assert result == expected
    report["native_guard_cases"] += 1

inv = Inventory()
before = bytes(rules)
for size in (0, 1, 0x44bcff):
    guard(lib.ssa_inventory_rules_load(C.byref(rules), rom_buffer, size), -1)
    assert bytes(rules) == before
guard(lib.ssa_inventory_rules_load(None, rom_buffer, len(ROM)), -1)
guard(lib.ssa_inventory_rules_load(C.byref(rules), None, len(ROM)), -1)
for weapon in (0, 11, 255, 0xffffffff):
    guard(lib.ssa_inventory_has_weapon(C.byref(inv), weapon), -1)
    guard(lib.ssa_inventory_queue_weapon(C.byref(inv), weapon), -1)
    guard(lib.ssa_inventory_add_ammo(C.byref(inv), C.byref(rules), weapon, 10, 1), -1)
for name in ("has_weapon", "queue_weapon"):
    guard(getattr(lib, "ssa_inventory_" + name)(None, 1), -1)
guard(lib.ssa_inventory_add_ammo(None, C.byref(rules), 1, 1, 1), -1)
guard(lib.ssa_inventory_add_ammo(C.byref(inv), None, 1, 1, 1), -1)
guard(lib.ssa_inventory_add_ammo(C.byref(inv), C.byref(rules), 1, 1, 256), -1)
guard(lib.ssa_inventory_fill_ammo(None, C.byref(rules), 1), -1)
guard(lib.ssa_inventory_fill_ammo(C.byref(inv), None, 1), -1)
guard(lib.ssa_inventory_fill_ammo(C.byref(inv), C.byref(rules), 256), -1)
for count in (10, 11, 65535):
    inv.pending_count = count
    before = bytes(inv)
    guard(lib.ssa_inventory_queue_weapon(C.byref(inv), 1), -1)
    assert bytes(inv) == before
for name in ("add_health", "add_stat_c4", "add_stat_c6"):
    guard(getattr(lib, "ssa_inventory_" + name)(None, 100), None)

report["total_rom_comparisons"] = sum(report["rom_comparisons"].values())
report["source_sha256"] = hashlib.sha256((ROOT / "src/inventory.c").read_bytes()).hexdigest()
report["compiler"] = "GCC C11 -O2 -Wall -Wextra -Werror -pedantic, UndefinedBehaviorSanitizer"
report["result"] = "pass"
(ROOT / "analysis/native-inventory-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
