"""06154 + 05FEC original execution versus native host-state projections.

Original copies/zero fills are serviced as platform operations. Respawn itself
and player/body synchronization execute original instructions; named external
world, spawn-selection, animation, effect, audio and notice calls are controlled.
"""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import itertools
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
SHA1 = hashlib.sha1(ROM).hexdigest()
assert SHA1 == '4ff48d8c18b70a5184758f1917343ae85a70858e'
# Import declarations, never another suite's execution or compiler.
nodes = []
for filename in ('test_spawn_rom.py', 'test_player_spawn_rom.py'):
    for n in ast.parse((ROOT / 'tests' / filename).read_text()).body:
        if isinstance(n, ast.ClassDef): nodes.append(n)
        elif isinstance(n, ast.Assign) and any(
                isinstance(t, ast.Attribute) and t.attr == '_fields_' or
                isinstance(t, ast.Name) and t.id == 'EP' for t in n.targets):
            nodes.append(n)
exec(compile(ast.Module(nodes, type_ignores=[]), 'spawn declarations', 'exec'))
class Frame(C.Structure):
    _fields_ = [('entity', EP), ('movement', C.POINTER(Movement)),
                ('x', C.c_int32), ('y', C.c_int32), ('eye_z', C.c_int32),
                ('timer', C.c_uint16), ('animation_state', C.c_uint8)]
class Camera(C.Structure):
    _fields_ = [('eye_z', C.c_int32), ('bob_amplitude', C.c_int32)]
class Weapon(C.Structure):
    _fields_ = [('current', C.c_uint32), ('flags', C.c_uint32),
                ('requested', C.c_uint16), ('keys', C.c_uint16)]
class WeaponView(C.Structure):
    _fields_ = [('x', C.c_int32), ('y', C.c_int32), ('flags', C.c_uint16),
                ('kind', C.c_uint8), ('player_number', C.c_uint8)]
class Lifecycle(C.Structure):
    _fields_ = [('player', C.POINTER(Player)), ('frame', C.POINTER(Frame)),
                ('camera', C.POINTER(Camera)), ('weapon', C.POINTER(Weapon)),
                ('weapon_view', C.POINTER(WeaponView)), ('death_ticks', C.c_uint16),
                ('updating', C.c_uint)]
class Game(C.Structure):
    _fields_ = [('message_ticks', C.c_uint16), ('input_state', C.c_uint16)] + [
        (n, C.c_uint8) for n in ('ticks', 'player_count', 'local_player', 'continue_flag', 'exit_flag')]
EVENT = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(Lifecycle), C.POINTER(Game),
                    C.c_int, C.c_int32, C.c_int32, C.c_int32, C.c_int32)
SELECT = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(Lifecycle), C.POINTER(Game), C.POINTER(Marker))
class Ops(C.Structure):
    _fields_ = [('event', EVENT), ('select_spawn', SELECT), ('context', C.c_void_p)]
BUILD = ROOT / 'build/native-tests'; BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                '-I', str(ROOT / 'include'), str(ROOT / 'src/player_lifecycle.c'),
                '-o', str(BUILD / 'libssa_player_lifecycle.so')], check=True)
lib = C.CDLL(str(BUILD / 'libssa_player_lifecycle.so'))
for name in ('ssa_player_death_step', 'ssa_player_respawn'):
    getattr(lib, name).argtypes = [C.POINTER(Lifecycle), C.POINTER(Game), C.POINTER(Ops)]

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000); uc.mem_map(0x04000000, 0x1000)
P, E, G, W, INPUT, LOCAL, STOP = 0x02000000, 0x02000400, 0x02000500, 0x02000600, 0x030042f0, 0x0300464e, 0x087ff000
ADDRESSES, SIZES = (P, E, G, W, INPUT, LOCAL), (256, 0x74, 0x80, 0x40, 0x28, 1)
uc.mem_write(0x03004010, struct.pack('<I', G))
rng = random.Random(0x61545fec)
# Every already-recovered field that the routine clears, changes or preserves.
fields = {}
def field(path, record, offset, fmt): fields[path] = (record, offset, fmt)
for n, off in [('x', 0), ('y', 4), ('z', 8), ('velocity_z', 20)]:
    field('player.motion.' + n, 0, off, 'i')
for n, off, fmt in [('keys', 0xa4, 'I'), ('weapon_flags', 0x30, 'I'), ('turn_velocity', 0x24, 'i'),
                    ('forward_velocity', 0xb0, 'i'), ('strafe_velocity', 0xb4, 'i'), ('impulse', 0x20, 'i'),
                    ('velocity_x', 12, 'i'), ('velocity_y', 16, 'i'), ('heading', 0xb8, 'H'),
                    ('move_heading', 0xba, 'H'), ('flags', 0xbc, 'H'), ('modifier_ticks', 0xc2, 'H'), ('impulse_heading', 0xdc, 'H')]:
    field('player.motion.movement.' + n, 0, off, fmt)
for n, off, fmt in [('eye_offset', 0xd0, 'i'), ('eye_velocity', 0xd4, 'i'), ('lowered', 0xd8, 'H')]:
    field('player.vertical.' + n, 0, off, fmt)
for n, off, fmt in [('x', 0, 'i'), ('y', 4, 'i'), ('z', 8, 'i'), ('velocity_x', 12, 'i'),
                    ('velocity_y', 16, 'i'), ('velocity_z', 20, 'i'), ('height', 44, 'i'), ('flags', 0x30, 'H'),
                    ('floor', 0x72, 'h'), ('ceiling', 0x70, 'h'), ('hover_offset', 0x43, 'b'), ('step_adjust', 0x53, 'b')]:
    field('player.entity.body.' + n, 1, off, fmt)
for n, off, fmt in [('radius', 0x28, 'i'), ('type', 0x3c, 'H'), ('player_index', 0x54, 'B'), ('leaf', 0x6e, 'h'), ('sector', 0x6a, 'h')]:
    field('player.entity.' + n, 1, off, fmt)
field('player.motion.facing', 1, 0x3f, 'B')
field('player.animation', 1, 0x58, 'H'); field('player.animation_ticks', 1, 0x5e, 'B')
for n, off, fmt in [('weapons', 0x34, 'H'), ('pending_count', 0x7a, 'H'), ('stat_c4', 0xc4, 'h'), ('stat_c6', 0xc6, 'h')]:
    field('player.inventory.' + n, 0, off, fmt)
field('player.inventory.health', 1, 0x32, 'h')
for i in range(10):
    field('player.inventory.ammo.' + str(i), 0, 0x36 + i*2, 'h')
    field('player.inventory.pending_weapons.' + str(i), 0, 0x7c + i*4, 'I')
for path, record, off, fmt in [('frame.timer', 1, 0x4e, 'H'), ('frame.animation_state', 1, 0x3e, 'B'),
    ('camera.eye_z', 0, 0x18, 'i'), ('camera.bob_amplitude', 0, 0xe0, 'i'), ('weapon.current', 0, 0x28, 'I'),
    ('weapon.flags', 0, 0x30, 'I'), ('weapon.requested', 0, 0x78, 'H'), ('weapon_view.x', 3, 0xc, 'i'),
    ('weapon_view.y', 3, 0x10, 'i'), ('weapon_view.flags', 3, 4, 'H'), ('weapon_view.kind', 3, 0x2e, 'B'),
    ('weapon_view.player_number', 3, 0x2f, 'B'), ('death_ticks', 0, 0xda, 'H'),
    ('game.message_ticks', 2, 0x76, 'H'), ('game.input_state', 4, 0x14, 'H'), ('game.ticks', 2, 7, 'B'),
    ('game.player_count', 2, 0x10, 'B'), ('game.local_player', 5, 0, 'B'), ('game.continue_flag', 2, 0x7c, 'B'), ('game.exit_flag', 2, 0x7d, 'B')]:
    field(path, record, off, fmt)

def read(): return [bytes(uc.mem_read(a, n)) for a, n in zip(ADDRESSES, SIZES)]
def write(raw):
    for a, r in zip(ADDRESSES, raw): uc.mem_write(a, bytes(r))
def project(raw): return {n: struct.unpack_from('<'+fmt, raw[r], off)[0] for n, (r, off, fmt) in fields.items()}
def patch(raw, changes):
    for n, v in changes.items():
        r, off, fmt = fields[n]; struct.pack_into('<'+fmt, raw[r], off, v)
def target(s, game, path):
    parts = path.split('.'); obj = game if parts[0] == 'game' else s
    if parts[0] == 'game': parts = parts[1:]
    for part in parts[:-1]:
        obj = obj[int(part)] if part.isdigit() else getattr(obj, part)
        if isinstance(obj, C._Pointer): obj = obj.contents
    return obj, parts[-1]
def get_native(s, game):
    out = {}
    for path in fields:
        obj, n = target(s, game, path); out[path] = obj[int(n)] if n.isdigit() else getattr(obj, n)
    return out
def set_native(s, game, changes):
    for path, v in changes.items():
        obj, n = target(s, game, path)
        if n.isdigit(): obj[int(n)] = v
        else: setattr(obj, n, v)
def native(raw):
    p, f, c, w, v, g = Player(), Frame(), Camera(), Weapon(), WeaponView(), Game()
    f.entity = C.pointer(p.entity); f.movement = C.pointer(p.motion.movement)
    s = Lifecycle(C.pointer(p), C.pointer(f), C.pointer(c), C.pointer(w), C.pointer(v), 0, 0)
    set_native(s, g, project(raw)); p.active = 1
    p.entity_health = p.inventory.health; p.vertical.flags = p.motion.movement.flags
    return s, g

events, errors, mutation, marker = [], [], {}, None
failing = -1
def effects(kind, args):
    result = dict(mutation.get(kind, {}))
    if kind == 0:
        result.setdefault('player.animation', args[0]); result.setdefault('player.animation_ticks', args[1])
    return result
def ret(): uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
def reg(r): return uc.reg_read(r)
HOOKS = {0x08005394: 0, 0x0800e914: 1, 0x08008d5c: 2, 0x08008eb0: 3,
         0x0800a65c: 4, 0x08014b60: 5, 0x0800a5d0: 6, 0x0800fc04: 7, 0x08001b18: 8}
def hook(machine, address, size, user):
    kind = HOOKS[address]
    r = [reg(q) for q in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)]
    args = (0, 0, 0, 0)
    if kind in (0, 7):
        assert r[0] == (E if kind == 0 else W); args = (r[1], r[2], 0, 0)
    elif kind in (1, 5): args = tuple(C.c_int32(v).value for v in r)
    elif kind in (2, 3): args = (r[0], 0, 0, 0)
    elif kind in (4, 6): assert r[0] == E
    raw = read(); events.append((kind, args, project(raw)))
    changes = effects(kind, args); raw = [bytearray(x) for x in raw]; patch(raw, changes); write(raw)
    if kind == 8: uc.mem_write(r[0], struct.pack('<hhhH', *marker))
    ret()
for a in HOOKS: uc.hook_add(UC_HOOK_CODE, hook, begin=a, end=a)
def copy(machine, address, size, user):
    if address == 0x080c562c:
        uc.mem_write(reg(UC_ARM_REG_R0), bytes(uc.mem_read(reg(UC_ARM_REG_R1), reg(UC_ARM_REG_R2))))
    else:
        assert reg(UC_ARM_REG_R2) == 0x05000040
        uc.mem_write(reg(UC_ARM_REG_R1), bytes(uc.mem_read(reg(UC_ARM_REG_R0), 4))*64)
    ret()
for a in (0x080c562c, 0x080c5320): uc.hook_add(UC_HOOK_CODE, copy, begin=a, end=a)
def dma(machine, access, address, size, value, user):
    if address == 0x040000dc:
        assert value == 0x85000040
        src, dst = struct.unpack('<II', uc.mem_read(0x040000d4, 8))
        uc.mem_write(dst, bytes(uc.mem_read(src, 4))*64)
uc.hook_add(UC_HOOK_MEM_WRITE, dma, begin=0x040000dc, end=0x040000df)
@EVENT
def event(context, sp, gp, kind, a, b, c, d):
    try:
        s, g = sp.contents, gp.contents
        args = (a, b, c, d); events.append((kind, args, get_native(s, g)))
        if kind == failing: return -1
        set_native(s, g, effects(kind, args)); return 0
    except BaseException as error: errors.append(error); return -1
@SELECT
def select(context, sp, gp, out):
    try:
        s, g = sp.contents, gp.contents
        events.append((8, (0, 0, 0, 0), get_native(s, g)))
        if failing == 8: return -1
        set_native(s, g, effects(8, (0, 0, 0, 0)))
        out[0] = Marker(*marker); return 0
    except BaseException as error: errors.append(error); return -1
ops = Ops(event, select, None)
report = {'rom_sha1': SHA1, 'boundary_death_calls': 0, 'random_death_calls': 0,
          'respawn_calls': 0, 'sequence_frames': 0, 'native_guards': 0,
          'scope': 'Whole original 06154 and 05FEC, including nested respawn and actual 05E04 synchronization; projected known native player/body/weapon/game fields and ordered boundary snapshots. Spawn selection, spatial unlink/insert, animation, notices, sound, effects and mode transition controlled. memcpy and BIOS/DMA fill serviced. Unknown player fields are not represented by native state.'}

def state():
    raw = [bytearray(rng.randbytes(n)) for n in SIZES]
    count = rng.randrange(1, 5)
    patch(raw, {'game.player_count': count, 'game.local_player': rng.randrange(count),
                'player.entity.player_index': rng.randrange(count), 'game.continue_flag': rng.randrange(3),
                'game.exit_flag': rng.randrange(2), 'game.ticks': rng.choice((0, 1, 2, 4, 8, 255))})
    struct.pack_into('<I', raw[0], 0xac, E); struct.pack_into('<I', raw[0], 0x2c, W)
    return raw
def compare(raw, routine, category, changes=None):
    global mutation, marker
    mutation = changes or {}
    marker = (rng.randrange(-32768, 32768), rng.randrange(-32768, 32768), rng.randrange(-32768, 32768), rng.randrange(65536))
    write(raw); events.clear(); errors.clear()
    uc.reg_write(UC_ARM_REG_R0, P); uc.reg_write(UC_ARM_REG_SP, 0x03007e00); uc.reg_write(UC_ARM_REG_LR, STOP|1)
    uc.emu_start((0x08006154 if routine == 'death' else 0x08005fec)|1, STOP, count=20000)
    assert reg(UC_ARM_REG_PC) == STOP
    expected, expected_events = project(read()), list(events)
    s, g = native(raw); events.clear()
    result = getattr(lib, 'ssa_player_death_step' if routine == 'death' else 'ssa_player_respawn')(C.byref(s), C.byref(g), C.byref(ops))
    assert not errors, errors
    assert result == 0 and s.updating == 0
    actual = get_native(s, g)
    assert actual == expected, (category, report[category], {k: (actual[k], expected[k]) for k in actual if actual[k] != expected[k]})
    assert events == expected_events, (category, report[category], 'events', [
        (a[:2], b[:2], {k: (a[2][k], b[2][k]) for k in a[2] if a[2][k] != b[2][k]})
        for a, b in zip(events, expected_events) if a != b], len(events), len(expected_events))
    assert s.player.contents.entity_health == s.player.contents.inventory.health
    assert s.player.contents.vertical.flags == s.player.contents.motion.movement.flags
    report[category] += 1
    return read()

# Exact-zero death trigger, 4-unit eye threshold, timer301..399/590 boundaries,
# wraparound, local/remote players and both single-player continue modes.
for count, health, locked, eye in itertools.product((1, 2, 4), (-32768, -1, 0, 1, 32767), (0, 0x10), (0x3ffff, 0x40000, 0x40001)):
    for ticks in (0, 1, 255):
        raw = state(); patch(raw, {'game.player_count': count, 'game.local_player': 0,
            'player.entity.player_index': count-1, 'player.inventory.health': health,
            'player.motion.movement.flags': locked, 'player.vertical.eye_offset': eye,
            'death_ticks': rng.choice((0, 300, 301, 399, 400, 590, 591, 65535)), 'game.ticks': ticks})
        compare(raw, 'death', 'boundary_death_calls')
for timer, ticks, count, continuation, leaving in itertools.product(
        (0, 45, 46, 299, 300, 301, 398, 399, 400, 499, 500, 589, 590, 591, 65534, 65535),
        (0, 1, 2, 255), (1, 2, 4), (0, 1, 2), (0, 1)):
    raw = state(); patch(raw, {'game.player_count': count, 'game.local_player': 0,
        'player.entity.player_index': 0, 'player.motion.movement.flags': 0x10,
        'death_ticks': timer, 'game.ticks': ticks, 'game.continue_flag': continuation, 'game.exit_flag': leaving})
    compare(raw, 'death', 'boundary_death_calls')
print('PASS death boundary and wrapping cases', flush=True)
for i in range(6000):
    raw = state()
    patch(raw, {'player.inventory.health': rng.choice((0, -1, 100)),
        'player.motion.movement.flags': rng.randrange(65536), 'death_ticks': rng.choice((rng.randrange(65536), rng.randrange(280, 610)))})
    changes = {}
    if i % 3 == 0:
        changes = {0: {'player.motion.movement.flags': rng.choice((0, 0x10, 0xffff)), 'game.ticks': rng.randrange(256)},
                   1: {'player.inventory.health': rng.randrange(-32768, 32768)},
                   4: {'player.entity.body.floor': rng.randrange(-32768, 32768)},
                   6: {'player.entity.body.floor': rng.randrange(-32768, 32768), 'player.entity.sector': rng.randrange(-32768, 32768)}}
    compare(raw, 'death', 'random_death_calls', changes)
print('PASS 6,000 randomized death calls including controlled callback mutations', flush=True)
for i in range(1024):
    changes = {4: {'player.entity.body.floor': rng.randrange(-32768, 32768)},
               8: {'player.inventory.health': rng.randrange(-32768, 32768)},
               6: {'player.entity.body.floor': rng.randrange(-32768, 32768),
                   'player.inventory.stat_c4': rng.randrange(-32768, 32768),
                   'player.inventory.weapons': rng.randrange(65536)},
               7: {'player.motion.movement.flags': rng.randrange(65536)}} if i % 2 else {}
    compare(state(), 'respawn', 'respawn_calls', changes)
print('PASS 1,024 direct respawns and weapon preservation', flush=True)
for sequence in range(8):
    raw = state(); count = 1 + sequence % 4
    patch(raw, {'game.player_count': count, 'game.local_player': 0, 'player.entity.player_index': 0,
                'player.inventory.health': 0, 'player.motion.movement.flags': 0, 'game.exit_flag': 0, 'game.ticks': 2})
    for frame in range(256):
        raw = [bytearray(r) for r in compare(raw, 'death', 'sequence_frames')]

# Native invalid arguments fail before mutation, and each external failure
# stops the call, retaining earlier changes, while clearing the reentry latch.
s, g = native(state()); before = get_native(s, g)
for args in ((None, C.byref(g), C.byref(ops)), (C.byref(s), None, C.byref(ops)), (C.byref(s), C.byref(g), None)):
    for name in ('ssa_player_death_step', 'ssa_player_respawn'):
        assert getattr(lib, name)(*args) == -1; report['native_guards'] += 1
for n, value in (('player_count', 0), ('player_count', 5), ('local_player', 4)):
    saved = getattr(g, n); setattr(g, n, value)
    snapshot = get_native(s, g)
    assert lib.ssa_player_death_step(C.byref(s), C.byref(g), C.byref(ops)) == -1
    assert get_native(s, g) == snapshot; setattr(g, n, saved); report['native_guards'] += 1
s.updating = 1
assert lib.ssa_player_death_step(C.byref(s), C.byref(g), C.byref(ops)) == -1
assert s.updating == 1; s.updating = 0; report['native_guards'] += 1
s.weapon.contents.flags ^= 1
assert lib.ssa_player_respawn(C.byref(s), C.byref(g), C.byref(ops)) == -1; report['native_guards'] += 1
s.weapon.contents.flags ^= 1
assert get_native(s, g) == before
for kind in range(9):
    raw = state(); patch(raw, {'game.player_count': 1, 'game.local_player': 0, 'player.entity.player_index': 0,
                               'player.inventory.health': 0, 'player.motion.movement.flags': 0, 'game.continue_flag': 0})
    routine = 'ssa_player_death_step' if kind < 4 else 'ssa_player_respawn'
    if kind == 3: patch(raw, {'player.motion.movement.flags': 0x10, 'death_ticks': 591})
    s, g = native(raw); mutation = {}; marker = (1, 2, 3, 4); failing = kind; events.clear()
    assert getattr(lib, routine)(C.byref(s), C.byref(g), C.byref(ops)) == -1, kind
    assert not errors and s.updating == 0 and events[-1][0] == kind
    report['native_guards'] += 1
failing = -1
report['rom_comparisons'] = sum(report[k] for k in ('boundary_death_calls', 'random_death_calls', 'respawn_calls', 'sequence_frames'))
report['projected_fields'] = len(fields)
report['source_sha256'] = hashlib.sha256((ROOT / 'src/player_lifecycle.c').read_bytes()).hexdigest()
(ROOT / 'analysis/native-player-lifecycle-validation.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
