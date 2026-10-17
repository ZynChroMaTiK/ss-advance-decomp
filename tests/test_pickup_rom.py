"""Whole pickup dispatcher, with original inventory helpers executing unchanged.

Only scripts, sound, text, network requests, nearest selection and destruction
are controlled boundaries. Compare full player/entity/game bytes and event order,
including inventory/counter snapshots at every external effect.
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
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / "orig/SSA.gba").read_bytes()
SHA1 = "4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-shared", "-fPIC", "-fsanitize=undefined", "-fno-sanitize-recover=all",
                "-I", str(ROOT / "include"), str(ROOT / "src/pickup.c"),
                str(ROOT / "src/inventory.c"), "-o", str(BUILD / "libssa_pickup.so")], check=True)

class Inventory(C.Structure):
    _fields_ = [("weapons", C.c_uint16), ("ammo", C.c_int16 * 10),
                ("pending_count", C.c_uint16), ("pending_weapons", C.c_uint32 * 10),
                ("health", C.c_int16), ("stat_c4", C.c_int16), ("stat_c6", C.c_int16)]
class Rules(C.Structure):
    _fields_ = [("ammo_capacity", C.c_uint16 * 10)]
class Pickup(C.Structure):
    _fields_ = [("item", C.c_uint16), ("trigger", C.c_uint16), ("handle", C.c_void_p)]
class Player(C.Structure):
    _fields_ = [("inventory", C.POINTER(Inventory)), ("entity_type", C.c_uint16),
                ("index", C.c_uint8), ("body_player_index", C.c_uint8), ("handle", C.c_void_p)]
EVENT = C.CFUNCTYPE(None, C.c_void_p, C.c_int, C.c_uint, C.c_uint)
REMOVE = C.CFUNCTYPE(None, C.c_void_p, C.POINTER(Pickup))
NEAREST = C.CFUNCTYPE(C.c_void_p, C.c_void_p, C.POINTER(Player))
class Ops(C.Structure):
    _fields_ = [("event", EVENT), ("remove", REMOVE), ("nearest", NEAREST), ("context", C.c_void_p)]
class Game(C.Structure):
    _fields_ = [("rules", C.POINTER(Rules)), ("ops", Ops), ("collected", C.c_uint32),
                ("player_count", C.c_uint8), ("local_player", C.c_uint8)]

lib = C.CDLL(str(BUILD / "libssa_pickup.so"))
lib.ssa_pickup_touch.argtypes = [C.POINTER(Game), C.POINTER(Player), C.POINTER(Pickup), C.c_uint32]
lib.ssa_pickup_touch.restype = C.c_int
lib.ssa_inventory_rules_load.argtypes = [C.POINTER(Rules), C.c_void_p, C.c_size_t]
rules = Rules()
assert lib.ssa_inventory_rules_load(C.byref(rules), C.create_string_buffer(ROM), len(ROM)) == 0

def unpack(p, e):
    inv = Inventory()
    inv.weapons = struct.unpack_from("<H", p, 0x34)[0]
    inv.ammo[:] = struct.unpack_from("<10h", p, 0x36)
    inv.pending_count = struct.unpack_from("<H", p, 0x7a)[0]
    inv.pending_weapons[:] = struct.unpack_from("<10I", p, 0x7c)
    inv.health = struct.unpack_from("<h", e, 0x32)[0]
    inv.stat_c4, inv.stat_c6 = struct.unpack_from("<hh", p, 0xc4)
    return inv

def pack(inv, player, entity):
    p, e = bytearray(player), bytearray(entity)
    struct.pack_into("<H10h", p, 0x34, inv.weapons, *inv.ammo)
    struct.pack_into("<H10I", p, 0x7a, inv.pending_count, *inv.pending_weapons)
    struct.pack_into("<hh", p, 0xc4, inv.stat_c4, inv.stat_c6)
    struct.pack_into("<h", e, 0x32, inv.health)
    return bytes(p), bytes(e)

def inv_state(inv):
    return (inv.weapons, tuple(inv.ammo), inv.pending_count, tuple(inv.pending_weapons),
            inv.health, inv.stat_c4, inv.stat_c6)

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
PLAYERS, CURRENT, BODY, PICK, OTHER, GAME, STOP = (0x02000000, 0x02001000, 0x02001100,
                                                 0x02001200, 0x02001300, 0x02002000, 0x087ff000)
uc.mem_write(0x03004648, struct.pack("<I", PLAYERS))
uc.mem_write(0x03004010, struct.pack("<I", GAME))
rng = random.Random(0x1b99c)
report = {"rom_sha1": SHA1, "normal_cases": 0, "replay_cases": 0, "random_cases": 0,
          "native_guards": 0, "effects": 0,
          "scope": "whole0801B99C plus all seven unhooked inventory helpers; hooks only script/sound/text/network/nearest/destruction; full bytes and effect-time inventory/counter snapshots"}
active = None

def snapshot_rom():
    inv = unpack(bytes(uc.mem_read(active['player_address'], 256)), bytes(uc.mem_read(BODY, 0x74)))
    return inv_state(inv), struct.unpack("<I", uc.mem_read(GAME + 0x31c, 4))[0]

def hook(machine, address, size, context):
    a, b, c, d = [machine.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)]
    if address == 0x0801c670:
        active['events'].append((1, a, 0, snapshot_rom()))
        if active['pickup_present']:
            trigger = struct.unpack("<H", machine.mem_read(PICK + 0x68, 2))[0]
            machine.mem_write(PICK + 0x68, struct.pack("<H", trigger ^ 0x8000))
    elif address == 0x08008d5c:
        active['events'].append((2, a, 0, snapshot_rom()))
    elif address == 0x0800e914:
        assert (b,c,d) == (0,48,0)
        active['events'].append((3, a, 0, snapshot_rom()))
    elif address == 0x0800580c:
        active['events'].append((4, a, b, snapshot_rom()))
    elif address == 0x08003758:
        assert a == CURRENT
        active['events'].append((5, 0, 0, snapshot_rom()))
        machine.reg_write(UC_ARM_REG_R0, active['nearest'])
    elif address == 0x080037e0:
        assert a in (PICK, OTHER)
        active['events'].append((6, a, 0, snapshot_rom()))
        machine.mem_write(a, bytes(0x74))
    else:
        raise AssertionError(hex(address))
    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

for address in (0x0801c670, 0x08008d5c, 0x0800e914, 0x0800580c, 0x08003758, 0x080037e0):
    uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

def snapshot_native():
    return inv_state(active['inv']), active['game'].collected

@EVENT
def native_event(context, kind, a, b):
    active['events'].append((kind,a,b,snapshot_native()))
    if kind == 1 and active['pickup_present']:
        active['pick'].trigger ^= 0x8000

@REMOVE
def native_remove(context, pickup):
    ptr = pickup.contents
    active['events'].append((6,ptr.handle,0,snapshot_native()))
    active['removed'].add(ptr.handle)
    ptr.item = ptr.trigger = 0

@NEAREST
def native_nearest(context, player):
    active['events'].append((5,0,0,snapshot_native()))
    return C.addressof(active['pick'] if active['nearest'] == PICK else active['other']) if active['nearest'] else None

ops = Ops(native_event, native_remove, native_nearest, None)

def compare(item, count=1, local=0, body_index=0, entity_type=0xc9, override=0,
            nearest=PICK, nearest_item=None, stats=0, owned=0, trigger=0x123, pickup_present=True):
    global active
    index = rng.randrange(4)
    p = bytearray(rng.randbytes(256)); current = bytearray(rng.randbytes(0x74))
    body = bytearray(rng.randbytes(0x74)); pick = bytearray(rng.randbytes(0x74)); other = bytearray(rng.randbytes(0x74))
    game_raw = bytearray(rng.randbytes(0x400))
    struct.pack_into("<I", p, 0xac, BODY)
    struct.pack_into("<H", p, 0x34, owned)
    struct.pack_into("<H", p, 0x7a, rng.randrange(10))
    struct.pack_into("<hh", p, 0xc4, stats, stats)
    struct.pack_into("<h", body, 0x32, stats)
    struct.pack_into("<H", current, 0x3c, entity_type)
    current[0x54], body[0x54] = index, body_index
    struct.pack_into("<H", pick, 0x58, item)
    struct.pack_into("<H", pick, 0x68, trigger)
    struct.pack_into("<H", other, 0x58, item if nearest_item is None else nearest_item)
    game_raw[0x10] = count
    for addr, data in ((PLAYERS+index*256,p), (CURRENT,current), (BODY,body), (PICK,pick), (OTHER,other), (GAME,game_raw)):
        uc.mem_write(addr, bytes(data))
    uc.mem_write(0x0300464e, bytes([local]))
    active = dict(events=[], player_address=PLAYERS+index*256, nearest=nearest, pickup_present=pickup_present)
    for reg, value in ((UC_ARM_REG_R0,CURRENT), (UC_ARM_REG_R1,PICK if pickup_present else 0),
                       (UC_ARM_REG_R2,override), (UC_ARM_REG_SP,0x03007e00), (UC_ARM_REG_LR,STOP|1)):
        uc.reg_write(reg,value)
    uc.emu_start(0x0801b99d, STOP, count=50000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    expected = [bytes(uc.mem_read(addr,len(data))) for addr,data in
                ((PLAYERS+index*256,p), (BODY,body), (CURRENT,current), (PICK,pick), (OTHER,other), (GAME,game_raw))]
    expected_events = active['events']
    inv = unpack(p,body)
    npick = Pickup(item,trigger,PICK)
    nother = Pickup(struct.unpack_from("<H",other,0x58)[0],struct.unpack_from("<H",other,0x68)[0],OTHER)
    ngame = Game(C.pointer(rules),ops,struct.unpack_from("<I",game_raw,0x31c)[0],count,local)
    player = Player(C.pointer(inv),entity_type,index,body_index,CURRENT)
    active.update(events=[],inv=inv,game=ngame,pick=npick,other=nother,removed=set())
    result = lib.ssa_pickup_touch(C.byref(ngame),C.byref(player),C.byref(npick) if pickup_present else None,override)
    assert result >= 0
    actual = list(pack(inv,p,body)) + [bytes(current)]
    for address,raw,native in ((PICK,pick,npick),(OTHER,other,nother)):
        if address in active['removed']: raw=bytearray(0x74)
        struct.pack_into("<H",raw,0x58,native.item)
        struct.pack_into("<H",raw,0x68,native.trigger)
        actual.append(bytes(raw))
    struct.pack_into("<I",game_raw,0x31c,ngame.collected)
    actual.append(bytes(game_raw))
    assert actual == expected, (item,count,local,body_index,entity_type,override,stats,owned,
                               [(n,[(i,x,y) for i,(x,y) in enumerate(zip(a,b)) if x!=y]) for n,(a,b) in enumerate(zip(actual,expected)) if a!=b])
    assert active['events'] == expected_events, (item,override,active['events'],expected_events)
    report['effects'] += len(expected_events)
    return result

# Every switch item and nearby default values, local/remote and multiplayer gates.
for item,count,body_index,stats in itertools.product(range(66),(0,1,2,255),(0,1),(-1,99,100,199,200)):
    compare(item,count=count,body_index=body_index,stats=stats,owned=rng.choice((0,1023)),trigger=rng.choice((0,0x1ff,0x8123)))
    report['normal_cases'] += 1

# Replay removes nearest first, even when inventory subsequently refuses an item.
for item,count,nearest,match in itertools.product(range(1,66),(1,2),(0,PICK,OTHER),(False,True)):
    compare(item,count=count,override=item,nearest=nearest,nearest_item=item if match else item+1,
            pickup_present=rng.choice((False,True)),stats=200,owned=1023)
    report['replay_cases'] += 1
for _ in range(2000):
    item=rng.randrange(65536)
    compare(item,count=rng.randrange(256),body_index=rng.randrange(4),local=rng.randrange(4),
            entity_type=rng.choice((0,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xffff)),
            stats=rng.randrange(-32768,32768),owned=rng.randrange(65536),trigger=rng.randrange(65536))
    report['random_cases'] += 1

# Native API rejects unsafe integration instead of dereferencing missing state.
inv=Inventory(); p=Player(C.pointer(inv),0xc9,0,0,None); pk=Pickup(32,0,None)
g=Game(C.pointer(rules),ops,0,1,0)
for gg,pp,pkp,override in ((None,C.byref(p),C.byref(pk),0),(C.byref(g),None,C.byref(pk),0),
                          (C.byref(g),C.byref(p),None,0)):
    assert lib.ssa_pickup_touch(gg,pp,pkp,override)==-1
    report['native_guards']+=1
g.ops=Ops(EVENT(),native_remove,native_nearest,None)
assert lib.ssa_pickup_touch(C.byref(g),C.byref(p),C.byref(pk),0)==-1
report['native_guards']+=1
g.ops=Ops(native_event,REMOVE(),native_nearest,None)
assert lib.ssa_pickup_touch(C.byref(g),C.byref(p),C.byref(pk),0)==-1
report['native_guards']+=1
g.ops=Ops(native_event,native_remove,NEAREST(),None)
assert lib.ssa_pickup_touch(C.byref(g),C.byref(p),C.byref(pk),32)==-1
report['native_guards']+=1
report['total_rom_comparisons']=report['normal_cases']+report['replay_cases']+report['random_cases']
report['source_sha256']=hashlib.sha256((ROOT/'src/pickup.c').read_bytes()).hexdigest()
report['compiler']='GCC C11 strict warnings and UndefinedBehaviorSanitizer'
report['result']='pass'
(ROOT/'analysis/native-pickup-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
