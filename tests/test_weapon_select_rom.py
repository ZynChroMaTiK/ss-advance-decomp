"""Whole original weapon request/acquisition/cycling routines, no ROM hooks."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                "-pedantic", "-shared", "-fPIC", "-fsanitize=undefined",
                "-fno-sanitize-recover=all", "-I", str(ROOT / "include"),
                *(str(ROOT / ("src/" + name + ".c")) for name in ("inventory", "weapon_select", "pickup")),
                "-o", str(BUILD / "libssa_weapon_select.so")], check=True)

class Inventory(C.Structure):
    _fields_ = [("weapons", C.c_uint16), ("ammo", C.c_int16 * 10),
                ("pending_count", C.c_uint16), ("pending_weapons", C.c_uint32 * 10),
                ("health", C.c_int16), ("stat_c4", C.c_int16), ("stat_c6", C.c_int16)]
class Rules(C.Structure):
    _fields_ = [("ammo_capacity", C.c_uint16 * 10)]
class Control(C.Structure):
    _fields_ = [("current", C.c_uint32), ("flags", C.c_uint32),
                ("requested", C.c_uint16), ("keys", C.c_uint16)]
lib = C.CDLL(str(BUILD / "libssa_weapon_select.so"))
I, W, R = C.POINTER(Inventory), C.POINTER(Control), C.POINTER(Rules)
lib.ssa_inventory_rules_load.argtypes = [R, C.c_void_p, C.c_size_t]
lib.ssa_weapon_take_request.argtypes = [I, W]
lib.ssa_weapon_cycle.argtypes = [I, W, C.c_int]
lib.ssa_weapon_update.argtypes = [I, W, R, C.c_uint]
rules = Rules()
assert lib.ssa_inventory_rules_load(C.byref(rules), C.create_string_buffer(ROM), len(ROM)) == 0
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
PLAYER, GAME, STOP = 0x02000000, 0x02000200, 0x087ff000
uc.mem_write(0x03004010, struct.pack("<I", GAME))
rng = random.Random(0x11604)
report = {"rom_sha1": SHA1, "rom_comparisons": {}, "native_guards": 0,
          "pickup_integration_cases": 0,
          "scope": "whole11588,11604,12038,12050 with actual1FFC/114D4/114B8 dependencies; no hooks; full player bytes; cycling ownership mask0x3FF and at least one eligible weapon"}

def unpack(raw):
    inv = Inventory()
    inv.weapons = struct.unpack_from("<H", raw, 0x34)[0]
    inv.ammo[:] = struct.unpack_from("<10h", raw, 0x36)
    inv.pending_count = struct.unpack_from("<H", raw, 0x7a)[0]
    inv.pending_weapons[:] = struct.unpack_from("<10I", raw, 0x7c)
    inv.stat_c4, inv.stat_c6 = struct.unpack_from("<hh", raw, 0xc4)
    ctl = Control(struct.unpack_from("<I", raw, 0x28)[0], struct.unpack_from("<I", raw, 0x30)[0],
                  struct.unpack_from("<H", raw, 0x78)[0], struct.unpack_from("<H", raw, 0xbc)[0])
    return inv, ctl

def pack(inv, ctl, raw):
    result = bytearray(raw)
    struct.pack_into("<H10h", result, 0x34, inv.weapons, *inv.ammo)
    struct.pack_into("<H10I", result, 0x7a, inv.pending_count, *inv.pending_weapons)
    struct.pack_into("<hh", result, 0xc4, inv.stat_c4, inv.stat_c6)
    struct.pack_into("<I", result, 0x28, ctl.current)
    struct.pack_into("<I", result, 0x30, ctl.flags)
    struct.pack_into("<H", result, 0x78, ctl.requested)
    struct.pack_into("<H", result, 0xbc, ctl.keys)
    return bytes(result)

def fresh():
    raw = bytearray(rng.randbytes(256))
    struct.pack_into("<I", raw, 0x28, rng.randrange(1, 11))
    struct.pack_into("<I", raw, 0x30, rng.getrandbits(32) & ~0x2c00)
    struct.pack_into("<H", raw, 0x34, rng.randrange(1, 1024))
    struct.pack_into("<10h", raw, 0x36, *(rng.choice((0, -32768, -1, 1, 2, 100, 32767)) for _ in range(10)))
    # Original cycling must terminate. Force an owned slot with nonzero ammo.
    owned = struct.unpack_from("<H", raw, 0x34)[0]
    slot = next(i for i in range(10) if owned & (1 << i))
    struct.pack_into("<h", raw, 0x36 + slot*2, rng.choice((-1, 1)))
    struct.pack_into("<H10I", raw, 0x7a, rng.randrange(11), *(rng.randrange(1, 11) for _ in range(10)))
    return raw

def compare(name, raw, count=1):
    raw = bytes(raw)
    inv, ctl = unpack(raw)
    uc.mem_write(PLAYER, raw)
    game = bytearray(rng.randbytes(0x80)); game[0x10] = count
    uc.mem_write(GAME, bytes(game))
    uc.reg_write(UC_ARM_REG_R0, PLAYER)
    uc.reg_write(UC_ARM_REG_SP, 0x03007e00)
    uc.reg_write(UC_ARM_REG_LR, STOP | 1)
    address = {"take": 0x08011588, "update": 0x08011604,
               "next": 0x08012038, "previous": 0x08012050}[name]
    uc.emu_start(address | 1, STOP, count=20000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP, (name, raw.hex())
    expected = bytes(uc.mem_read(PLAYER, 256))
    if name == "take":
        actual = lib.ssa_weapon_take_request(C.byref(inv), C.byref(ctl))
        assert actual == uc.reg_read(UC_ARM_REG_R0)
    elif name == "update":
        assert lib.ssa_weapon_update(C.byref(inv), C.byref(ctl), C.byref(rules), count) == 0
    else:
        assert lib.ssa_weapon_cycle(C.byref(inv), C.byref(ctl), 1 if name == "next" else -1) >= 1
    got = pack(inv, ctl, raw)
    assert got == expected, (name, [(i, a, b) for i, (a, b) in enumerate(zip(got, expected)) if a != b])
    assert bytes(uc.mem_read(GAME, 0x80)) == bytes(game)
    report["rom_comparisons"][name] = report["rom_comparisons"].get(name, 0) + 1
    return got

# Priority, blocked flags, queue boundaries, zero/negative ammo and key edges.
for pending in (0, 1, 9, 10):
    for flags in (0, 0x400, 0x800, 0x2000, 0x2c00, 0x100, 0xffffffff):
        for keys in (0, 0x40, 0x80, 0xc0, 0xffff):
            for ammo in (-32768, -1, 0, 1, 32767):
                raw = fresh()
                struct.pack_into("<I", raw, 0x28, 1)
                struct.pack_into("<I", raw, 0x30, flags)
                struct.pack_into("<H", raw, 0x34, 3)
                struct.pack_into("<hh", raw, 0x36, ammo, 1)
                struct.pack_into("<H", raw, 0x7a, pending)
                struct.pack_into("<H", raw, 0xbc, keys)
                compare("take", raw)
                compare("update", raw)

# Numeric ownership threshold and new/duplicate grants with every mask and ID.
for mask in range(1024):
    for weapon in range(1, 11):
        raw = fresh()
        struct.pack_into("<H", raw, 0x34, mask)
        struct.pack_into("<HI", raw, 0x7a, 1, weapon)
        compare("update", raw, rng.choice((0, 1, 2, 255)))

for _ in range(4000):
    raw = fresh()
    for name in ("take", "update", "next", "previous"):
        compare(name, raw, rng.randrange(256))

# Consecutive acquisition frames preserve LIFO order, ownership and ammo.
for _ in range(128):
    raw = fresh()
    struct.pack_into("<H", raw, 0x7a, 10)
    for frame in range(12):
        raw = bytearray(raw)
        struct.pack_into("<I", raw, 0x30, struct.unpack_from("<I", raw, 0x30)[0] & ~0x2c00)
        raw = compare("update", raw)

def guard(call, inv, ctl):
    before = bytes(inv), bytes(ctl)
    assert call() == -1
    assert (bytes(inv), bytes(ctl)) == before
    report["native_guards"] += 1

inv, ctl = Inventory(), Control(1, 0, 1, 0)
for count in (11, 65535):
    inv.pending_count = count
    guard(lambda: lib.ssa_weapon_take_request(C.byref(inv), C.byref(ctl)), inv, ctl)
    guard(lambda: lib.ssa_weapon_update(C.byref(inv), C.byref(ctl), C.byref(rules), 1), inv, ctl)
inv.pending_count = 1
for weapon in (0, 11, 254, 255, 0xffffffff):
    inv.pending_weapons[0] = weapon
    guard(lambda: lib.ssa_weapon_take_request(C.byref(inv), C.byref(ctl)), inv, ctl)
inv.pending_count = 0
for current in (0, 11, 0xffffffff):
    ctl.current = current
    guard(lambda: lib.ssa_weapon_take_request(C.byref(inv), C.byref(ctl)), inv, ctl)
ctl.current = 1
for mask in (0, 1, 0x3ff, 0x400, 0xffff):
    inv.weapons = mask
    for direction in (-1, 1):
        guard(lambda: lib.ssa_weapon_cycle(C.byref(inv), C.byref(ctl), direction), inv, ctl)
inv.weapons = 1; inv.ammo[0] = 10
for direction in (-2, 0, 2):
    guard(lambda: lib.ssa_weapon_cycle(C.byref(inv), C.byref(ctl), direction), inv, ctl)
for args in ((None, C.byref(ctl), C.byref(rules), 1),
             (C.byref(inv), None, C.byref(rules), 1),
             (C.byref(inv), C.byref(ctl), None, 1),
             (C.byref(inv), C.byref(ctl), C.byref(rules), 256)):
    guard(lambda: lib.ssa_weapon_update(*args), inv, ctl)
for fn in (lib.ssa_weapon_take_request,):
    guard(lambda: fn(None, C.byref(ctl)), inv, ctl)
    guard(lambda: fn(C.byref(inv), None), inv, ctl)
for i, w in ((None, C.byref(ctl)), (C.byref(inv), None)):
    guard(lambda: lib.ssa_weapon_cycle(i, w, 1), inv, ctl)
# Failure after a key would be consumed must roll back the entire update.
inv.weapons = 0; ctl.keys = 0xc0
guard(lambda: lib.ssa_weapon_update(C.byref(inv), C.byref(ctl), C.byref(rules), 1), inv, ctl)

class Pickup(C.Structure):
    _fields_ = [("item", C.c_uint16), ("trigger", C.c_uint16), ("handle", C.c_void_p)]
class Player(C.Structure):
    _fields_ = [("inventory", I), ("entity_type", C.c_uint16),
                ("index", C.c_uint8), ("body_player_index", C.c_uint8), ("handle", C.c_void_p)]
EVENT = C.CFUNCTYPE(None, C.c_void_p, C.c_int, C.c_uint, C.c_uint)
REMOVE = C.CFUNCTYPE(None, C.c_void_p, C.POINTER(Pickup))
NEAREST = C.CFUNCTYPE(C.c_void_p, C.c_void_p, C.POINTER(Player))
class Ops(C.Structure):
    _fields_ = [("event", EVENT), ("remove", REMOVE), ("nearest", NEAREST), ("context", C.c_void_p)]
class Game(C.Structure):
    _fields_ = [("rules", R), ("ops", Ops), ("collected", C.c_uint32),
                ("player_count", C.c_uint8), ("local_player", C.c_uint8)]
effects = []
@EVENT
def event(ctx, kind, a, b):
    effects.append((kind, a, b))
@REMOVE
def remove(ctx, pickup):
    effects.append(("remove", pickup.contents.item))
ops = Ops(event, remove, NEAREST(), None)
lib.ssa_pickup_touch.argtypes = [C.POINTER(Game), C.POINTER(Player), C.POINTER(Pickup), C.c_uint32]
for item, weapon in ((0x20, 2), (0x21, 3), (0x22, 4), (0x23, 6), (0x25, 5),
                     (0x26, 10), (0x27, 8), (0x28, 9), (0x29, 7)):
    for duplicate in (False, True):
        inv = Inventory(weapons=1); inv.ammo[0] = 999
        ctl = Control(1, 0, 1, 0)
        player = Player(C.pointer(inv), 0xc9, 0, 0, None)
        game = Game(C.pointer(rules), ops, 0, 1, 0)
        pickup = Pickup(item, 0, None)
        effects.clear()
        for _ in range(2 if duplicate else 1):
            assert lib.ssa_pickup_touch(C.byref(game), C.byref(player), C.byref(pickup), 0) == 1
        assert inv.weapons == 1 and inv.ammo[weapon-1] == 0
        assert inv.pending_count == (2 if duplicate else 1)
        assert game.collected == inv.pending_count
        assert sum(e[0] == "remove" for e in effects) == inv.pending_count
        for _ in range(2 if duplicate else 1):
            ctl.flags &= ~0x2c00
            assert lib.ssa_weapon_update(C.byref(inv), C.byref(ctl), C.byref(rules), 1) == 0
        assert inv.weapons == (1 | (1 << (weapon-1)))
        assert inv.ammo[weapon-1] == (rules.ammo_capacity[weapon-1] >> 1) * (2 if duplicate else 1)
        assert inv.pending_count == 0 and ctl.requested == weapon and ctl.current == 1
        before = bytes(inv)
        assert lib.ssa_pickup_touch(C.byref(game), C.byref(player), C.byref(pickup), 0) == 0
        assert bytes(inv) == before
        report["pickup_integration_cases"] += 1

report["total_rom_comparisons"] = sum(report["rom_comparisons"].values())
report["source_sha256"] = hashlib.sha256((ROOT / "src/weapon_select.c").read_bytes()).hexdigest()
report["compiler"] = "GCC C11 -O2 strict warnings, UndefinedBehaviorSanitizer"
report["result"] = "pass"
(ROOT / "analysis/native-weapon-selection-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
