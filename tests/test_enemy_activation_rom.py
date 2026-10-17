"""Whole CD..D8 state-zero initializers with real timer/counter/boss helpers.

Animation setter executes original instructions. Only sound, music and notice
are intercepted; native effects are checked at the same ordered boundaries.
"""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR,
                              UC_ARM_REG_PC)

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
SHA1 = hashlib.sha1(ROM).hexdigest()
assert SHA1 == '4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes = []
for n in ast.parse((ROOT / 'tests/test_spawn_rom.py').read_text()).body:
    if isinstance(n, ast.ClassDef): nodes.append(n)
    elif isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Attribute) and t.attr == '_fields_' or
            isinstance(t, ast.Name) and t.id == 'EP' for t in n.targets):
        nodes.append(n)
exec(compile(ast.Module(nodes, type_ignores=[]), 'entity declarations', 'exec'))
class View(C.Structure):
    _fields_ = [('entity', EP), ('health', C.c_int16), ('timer', C.c_uint16),
                ('saved_facing', C.c_uint16), ('state', C.c_uint8),
                ('facing', C.c_uint8), ('boss_ticks', C.c_uint8), ('updating', C.c_uint)]
class Game(C.Structure):
    _fields_ = [('spawned', C.c_uint32 * 12), ('boss', EP), ('boss_phase', C.c_uint8)]
EVENT = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(View), C.POINTER(Game),
                    C.c_int, C.c_int32, C.c_int32, C.c_int32, C.c_int32)
class Ops(C.Structure):
    _fields_ = [('event', EVENT), ('context', C.c_void_p)]
BUILD = ROOT / 'build/native-tests'; BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                '-I', str(ROOT / 'include'), str(ROOT / 'src/enemy_activation.c'),
                str(ROOT / 'src/entity_timer.c'), '-o', str(BUILD / 'libssa_enemy_activation.so')], check=True)
lib = C.CDLL(str(BUILD / 'libssa_enemy_activation.so'))
step = lib.ssa_enemy_activation_step
step.argtypes = [C.POINTER(View), C.POINTER(Game), C.POINTER(Ops), C.c_uint]
step.restype = C.c_int
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
E, G, OTHER, BOSS, STOP = 0x02000000, 0x02001000, 0x02002000, 0x03005710, 0x087ff000
uc.mem_write(0x03004010, struct.pack('<I', G))
addresses = [0x08013940, 0x08013584, 0x08013008, 0x0801315c, 0x08013b08, 0x08013318,
             0x08013f48, 0x08014158, 0x08013d78, 0x08013748, 0x0801485c, 0x080143b0]
regs = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
hooks = {0x08008d5c: 0, 0x08005394: 1, 0x08008eb0: 2, 0x0800e914: 3}
rng = random.Random(0xcd00d8)
report = {'rom_sha1': SHA1, 'scope': '12 whole state-zero initializers CD..D8; original 12CFC,125BC,14328,147D4,05394 execute; sound/music/notice controlled; full entity/game/boss-global bytes and ordered callback snapshots',
          'rom_comparisons': {}, 'native_guards': 0, 'callback_reentry_guards': 0,
          'callback_failure_cases': 0}
oracle_events = []
def snapshot(): return tuple(bytes(uc.mem_read(a, n)) for a, n in ((E, 0x74), (G, 0x200), (BOSS, 8)))
def hook(machine, address, size, user):
    kind = hooks[address]
    raw = [machine.reg_read(r) for r in regs]
    args = (raw[1], raw[2], 0, 0) if kind == 1 else tuple(raw) if kind == 3 else (raw[0], 0, 0, 0)
    oracle_events.append((kind, args, snapshot()))
    if kind != 1: machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
for addr in hooks: uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr)

def setfield(obj, path, value):
    parts = path.split('.')
    for part in parts[:-1]: obj = getattr(obj, part)
    setattr(obj, parts[-1], value)
def getfield(obj, path):
    for part in path.split('.'): obj = getattr(obj, part)
    return obj
entity_fields = [(n, off, fmt) for n, off, fmt in [
    ('body.x', 0, 'i'), ('body.y', 4, 'i'), ('body.z', 8, 'i'),
    ('body.velocity_x', 12, 'i'), ('body.velocity_y', 16, 'i'), ('body.velocity_z', 20, 'i'),
    ('radius', 0x28, 'i'), ('body.height', 0x2c, 'i'), ('body.flags', 0x30, 'H'),
    ('type', 0x3c, 'H'), ('body.hover_offset', 0x43, 'b'), ('body.step_adjust', 0x53, 'b'),
    ('player_index', 0x54, 'B'), ('sector', 0x6a, 'h'), ('leaf', 0x6e, 'h'),
    ('body.ceiling', 0x70, 'h'), ('body.floor', 0x72, 'h')]]
view_fields = [('health', 0x32, 'h'), ('state', 0x3e, 'B'), ('facing', 0x3f, 'B'),
               ('saved_facing', 0x48, 'H'), ('timer', 0x4e, 'H'), ('boss_ticks', 0x51, 'B')]

def compare(index, timer, flags, ticks, label, facing=None, counter=None):
    raw = bytearray(rng.randbytes(0x74)); game_raw = bytearray(rng.randbytes(0x200)); boss_raw = bytearray(rng.randbytes(8))
    struct.pack_into('<H', raw, 0x30, flags); struct.pack_into('<H', raw, 0x3c, 0xcd + index)
    struct.pack_into('<H', raw, 0x4e, timer); raw[0x3e] = 0
    if facing is not None: raw[0x3f] = facing
    game_raw[7] = ticks
    if counter is not None: struct.pack_into('<I', game_raw, 0x130 + index * 12, counter)
    struct.pack_into('<I', boss_raw, 0, OTHER)
    e, other, v, game = Entity(), Entity(), View(), Game()
    for path, off, fmt in entity_fields: setfield(e, path, struct.unpack_from('<' + fmt, raw, off)[0])
    for path, off, fmt in view_fields: setfield(v, path, struct.unpack_from('<' + fmt, raw, off)[0])
    v.entity = C.pointer(e); game.boss = C.pointer(other); game.boss_phase = boss_raw[4]
    for i in range(12): game.spawned[i] = struct.unpack_from('<I', game_raw, 0x130 + 12 * i)[0]
    uc.mem_write(E, bytes(raw)); uc.mem_write(G, bytes(game_raw)); uc.mem_write(BOSS, bytes(boss_raw))
    oracle_events.clear()
    for r, value in ((UC_ARM_REG_R0, E), (UC_ARM_REG_SP, 0x03007e00), (UC_ARM_REG_LR, STOP | 1)): uc.reg_write(r, value)
    uc.emu_start(addresses[index] | 1, STOP, count=1000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    expected_end = snapshot(); expected_events = list(oracle_events)
    native_events = []; reentry_results = []
    def project():
        er, gr, br = bytearray(raw), bytearray(game_raw), bytearray(boss_raw)
        for path, off, fmt in entity_fields: struct.pack_into('<' + fmt, er, off, getfield(e, path))
        for path, off, fmt in view_fields: struct.pack_into('<' + fmt, er, off, getfield(v, path))
        for i in range(12): struct.pack_into('<I', gr, 0x130 + 12 * i, game.spawned[i])
        struct.pack_into('<I', br, 0, E if C.addressof(game.boss.contents) == C.addressof(e) else OTHER)
        br[4] = game.boss_phase
        return bytes(er), bytes(gr), bytes(br)
    @EVENT
    def callback(ctx, vp, gp, kind, a, b, c, d):
        native_events.append((kind, (a, b, c, d), project()))
        reentry_results.append(step(vp, gp, C.byref(ops), ticks))
        if kind == 1:
            struct.pack_into('<H', raw, 0x58, a); raw[0x5e] = b
            raw[0x5c] = raw[0x5d] = raw[0x5f] = 0
        return 0
    ops = Ops(callback, None)
    result = step(C.byref(v), C.byref(game), C.byref(ops), ticks)
    assert result == int(v.state == 1) and not v.updating
    assert project() == expected_end, (index, timer, flags, ticks, [(i, a, b) for i, (a,b) in enumerate(zip(project()[0], expected_end[0])) if a != b])
    assert native_events == expected_events, (index, timer, flags, ticks, native_events, expected_events)
    assert all(x == -1 for x in reentry_results)
    report['callback_reentry_guards'] += len(reentry_results)
    report['rom_comparisons'][label] = report['rom_comparisons'].get(label, 0) + 1

for index in range(12):
    for ticks in (0, 1, 2, 29, 30, 31, 127, 255):
        for timer in (0, 1, 29, 30, 31, 255, 256, 32767, 32768, 32769, 65535,
                      ticks, (ticks - 1) & 65535, (ticks + 1) & 65535):
            for flags in (0, 2, 0x80, 0x1000, 0xff7f, 0xffff):
                compare(index, timer, flags, ticks, 'boundary', counter=0xffffffff)
for _ in range(6000):
    compare(rng.randrange(12), rng.randrange(65536), rng.randrange(65536), rng.randrange(256), 'random')
for index in (10, 11):
    for facing in range(256): compare(index, 0, 0x1000, 1, 'boss_facing', facing=facing)

# Native argument/lifetime guards must precede countdown/counter mutation.
e, v, game = Entity(), View(), Game(); e.type = 0xcd; v.entity = C.pointer(e)
@EVENT
def ok(*args): return 0
ops = Ops(ok, None)
def guarded(vp, gp, op, ticks):
    before = bytes(e), bytes(v), bytes(game)
    assert step(vp, gp, op, ticks) == -1
    assert (bytes(e), bytes(v), bytes(game)) == before
    report['native_guards'] += 1
for vp, gp, op, ticks in ((None, C.byref(game), None, 1), (C.byref(v), None, None, 1),
                         (C.byref(v), C.byref(game), None, 256), (C.byref(v), C.byref(game), None, 0xffffffff)):
    guarded(vp, gp, op, ticks)
v.entity = EP(); guarded(C.byref(v), C.byref(game), None, 1); v.entity = C.pointer(e)
for state in (1, 2, 255):
    v.state = state; guarded(C.byref(v), C.byref(game), None, 1)
v.state = 0; v.updating = 1; guarded(C.byref(v), C.byref(game), None, 1); v.updating = 0
for entity_type in (0, 0xcc, 0xd9, 0xffff):
    e.type = entity_type; guarded(C.byref(v), C.byref(game), None, 1)
for entity_type in (0xce, 0xd7, 0xd8):
    e.type = entity_type
    guarded(C.byref(v), C.byref(game), None, 1)
    empty_ops = Ops(); guarded(C.byref(v), C.byref(game), C.byref(empty_ops), 1)
e.type = 0xcd; world = World(); world.query_active = 1; e.owner = C.pointer(world)
guarded(C.byref(v), C.byref(game), None, 1); e.owner = C.POINTER(World)()
# Every ordinary type with no effect permits absent ops.
for index in (0, 2, 3, 4, 5, 6, 7, 8, 9):
    e.type = 0xcd + index; v.state = 0; v.timer = 0; e.body.flags = 0
    assert step(C.byref(v), C.byref(game), None, 1) == 1
    report['native_guards'] += 1
for index, kinds in ((1, (0,)), (10, (1, 2, 3)), (11, (1, 2, 3))):
    for fail in kinds:
        e = Entity(); e.type = 0xcd + index; e.body.flags = 0x1000
        v = View(); v.entity = C.pointer(e); game = Game(); calls = []
        @EVENT
        def failing(ctx, vp, gp, kind, a, b, c, d):
            calls.append(kind)
            return -1 if kind == fail else 0
        ops = Ops(failing, None)
        assert step(C.byref(v), C.byref(game), C.byref(ops), 1) == -1
        assert calls == list(kinds[:kinds.index(fail) + 1]) and v.state == 1 and not v.updating
        assert game.spawned[index] == 1
        if index >= 10: assert bool(e.body.flags & 0x1000) == (fail == 1)
        report['callback_failure_cases'] += 1
report['total_rom_comparisons'] = sum(report['rom_comparisons'].values())
report['source_sha256'] = hashlib.sha256((ROOT / 'src/enemy_activation.c').read_bytes()).hexdigest()
report['passed'] = True
(ROOT / 'analysis/native-enemy-activation-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
