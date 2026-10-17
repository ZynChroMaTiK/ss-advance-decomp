"""Original pool init, allocate and release vs host indices; BIOS fill is hooked."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
SHA1 = '4ff48d8c18b70a5184758f1917343ae85a70858e'
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / 'build/native-tests'
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                '-I', str(ROOT / 'include'), str(ROOT / 'src/entity_pool.c'),
                '-o', str(BUILD / 'libssa_entity_pool.so')], check=True)

class Pool(C.Structure):
    _fields_ = [('free_slots', C.c_uint16 * 100), ('active_slots', C.c_uint16 * 100),
                ('positions', C.c_uint16 * 100), ('active_count', C.c_uint16),
                ('occupied', C.c_uint8 * 100)]

lib = C.CDLL(str(BUILD / 'libssa_entity_pool.so'))
lib.ssa_entity_pool_init.argtypes = [C.POINTER(Pool)]
lib.ssa_entity_pool_allocate.argtypes = [C.POINTER(Pool), C.POINTER(C.c_uint)]
lib.ssa_entity_pool_release.argtypes = [C.POINTER(Pool), C.c_uint]
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
PAYLOAD, GAME, STOP = 0x02000000, 0x02004000, 0x087ff000
FREE, ACTIVE = 0x03004540, 0x03004470
rng = random.Random(0x5170)
report = {'rom_sha1': SHA1, 'rom_comparisons': {'init': 0, 'allocate': 0, 'release': 0},
          'native_guards': 0, 'scope': 'Whole 05238, 05170, 051C8 and original 052B0 clear; heap allocation and BIOS CpuSet fill controlled. All 100 payload records, complete free/active arrays, slot positions and counts compared after every call. Native caller clears payloads after pool release. Multiplayer CpuSet branch exercised, DMA hardware branch excluded.'}
events = []
def put(address, fmt, *values): uc.mem_write(address, struct.pack(fmt, *values))
def get(address, fmt): return struct.unpack(fmt, uc.mem_read(address, struct.calcsize(fmt)))[0]
def external(machine, address, size, user):
    if address == 0x080033d8:
        assert machine.reg_read(UC_ARM_REG_R0) == 0x2d50
        events.append(('heap', 0x2d50))
        machine.reg_write(UC_ARM_REG_R0, PAYLOAD)
    else:
        source = machine.reg_read(UC_ARM_REG_R0)
        target = machine.reg_read(UC_ARM_REG_R1)
        control = machine.reg_read(UC_ARM_REG_R2)
        assert control == 0x0500001d
        assert get(source, '<I') == 0
        assert PAYLOAD <= target < PAYLOAD + 100 * 0x74 and (target - PAYLOAD) % 0x74 == 0
        events.append(('clear', (target - PAYLOAD) // 0x74))
        machine.mem_write(target, bytes(0x74))
    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
for address in (0x080033d8, 0x080c5320):
    uc.hook_add(UC_HOOK_CODE, external, begin=address, end=address)

pool = Pool()
payload = bytearray(100 * 0x74)
def run(address, argument=0):
    events.clear()
    for reg, value in ((UC_ARM_REG_R0, argument), (UC_ARM_REG_SP, 0x03007e00), (UC_ARM_REG_LR, STOP | 1)):
        uc.reg_write(reg, value)
    uc.emu_start(address | 1, STOP, count=10000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    return uc.reg_read(UC_ARM_REG_R0)

def compare():
    count = pool.active_count
    assert get(0x03004348, '<H') == 100 - count
    assert get(0x03004354, '<H') == count
    assert get(0x0300433c, '<I') == FREE + count * 2
    assert get(0x03004454, '<I') == ACTIVE + count * 2
    assert bytes(pool.free_slots) == bytes(uc.mem_read(FREE, 200))
    assert bytes(pool.active_slots) == bytes(uc.mem_read(ACTIVE, 200))
    assert bytes(payload) == bytes(uc.mem_read(PAYLOAD, len(payload)))
    active = set(pool.active_slots[:count])
    for slot in range(100):
        assert pool.occupied[slot] == (slot in active)
        assert pool.positions[slot] == get(PAYLOAD + slot * 0x74 + 0x3a, '<H')

def initialize():
    uc.mem_write(PAYLOAD, rng.randbytes(len(payload)))
    uc.mem_write(ACTIVE, bytes(200))  # Original init leaves stale active bytes unchanged.
    put(0x03004010, '<I', GAME); put(GAME + 0x10, '<B', 4)
    put(0x0300461c, '<I', 0xffffffff)
    run(0x08005238)
    assert events == [('heap', 0x2d50)] + [('clear', i) for i in range(100)]
    assert get(0x0300461c, '<I') == 0
    assert lib.ssa_entity_pool_init(C.byref(pool)) == 1
    payload[:] = bytes(len(payload))
    compare(); report['rom_comparisons']['init'] += 1

def allocate(randomize=True):
    slot = C.c_uint(0xffffffff)
    result = lib.ssa_entity_pool_allocate(C.byref(pool), C.byref(slot))
    oracle = run(0x08005170)
    assert events == []
    if not oracle:
        assert result == 0 and slot.value == 0xffffffff
    else:
        assert result == 1 and slot.value == (oracle - PAYLOAD) // 0x74
        offset = slot.value * 0x74
        struct.pack_into('<HH', payload, offset + 0x38, slot.value, pool.positions[slot.value])
    compare(); report['rom_comparisons']['allocate'] += 1
    # Populate caller payload only after proving allocation preserves prior bytes.
    if oracle and randomize:
        offset = slot.value * 0x74
        payload[offset:offset + 0x74] = rng.randbytes(0x74)
        struct.pack_into('<HH', payload, offset + 0x38, slot.value, pool.positions[slot.value])
        uc.mem_write(oracle, bytes(payload[offset:offset + 0x74]))

def release(slot):
    assert lib.ssa_entity_pool_release(C.byref(pool), slot) == 1
    run(0x080051c8, PAYLOAD + slot * 0x74)
    assert events == [('clear', slot)]
    # Caller contract: clear after release, mirror moved entity's active position.
    payload[slot * 0x74:(slot + 1) * 0x74] = bytes(0x74)
    for active in pool.active_slots[:pool.active_count]:
        struct.pack_into('<H', payload, active * 0x74 + 0x3a, pool.positions[active])
    compare(); report['rom_comparisons']['release'] += 1

# All initial capacities and each release position, with stack reuse after every
# removal. This checks both last-entry removal and swap into every other slot.
for count in range(101):
    initialize()
    for _ in range(count): allocate()
    for position in range(count):
        old_slot = pool.active_slots[position]
        release(old_slot)
        allocate()
        assert pool.active_slots[pool.active_count - 1] == old_slot
    if count == 100: allocate()
    while pool.active_count: release(pool.active_slots[0])

# Long runs repeatedly cross empty/full boundaries and create arbitrary active
# permutations. Exhaustion never changes metadata or output.
for seed in range(8):
    initialize()
    for frame in range(2048):
        if not pool.active_count or rng.randrange(100) < (30 if seed & 1 else 75):
            allocate()
        else:
            release(pool.active_slots[rng.randrange(pool.active_count)])

def guard(action, subject=None):
    before = bytes(pool)
    if subject is not None: before = bytes(subject)
    assert action() == -1
    assert bytes(pool if subject is None else subject) == before
    report['native_guards'] += 1

initialize()
out = C.c_uint(123456)
guard(lambda: lib.ssa_entity_pool_init(None))
guard(lambda: lib.ssa_entity_pool_allocate(None, C.byref(out)))
guard(lambda: lib.ssa_entity_pool_allocate(C.byref(pool), None))
guard(lambda: lib.ssa_entity_pool_release(None, 0))
for slot in (0, 99, 100, 0xffffffff):
    guard(lambda slot=slot: lib.ssa_entity_pool_release(C.byref(pool), slot))
allocate(); release(0)
guard(lambda: lib.ssa_entity_pool_release(C.byref(pool), 0))
for kind in range(8):
    bad = Pool.from_buffer_copy(pool)
    if kind == 0: bad.active_count = 101
    if kind == 1: bad.free_slots[0] = 100
    if kind == 2: bad.free_slots[1] = bad.free_slots[0]
    if kind == 3: bad.occupied[0] = 1
    if kind == 4: bad.positions[0] = 1
    if kind >= 5:
        assert lib.ssa_entity_pool_allocate(C.byref(bad), C.byref(out)) == 1
        if kind == 5: bad.active_slots[0] = 100
        if kind == 6: bad.occupied[0] = 2
        if kind == 7: bad.positions[0] = 1
    guard(lambda: lib.ssa_entity_pool_allocate(C.byref(bad), C.byref(out)), bad)
    guard(lambda: lib.ssa_entity_pool_release(C.byref(bad), 0), bad)
report['total_rom_comparisons'] = sum(report['rom_comparisons'].values())
report['source_sha256'] = hashlib.sha256((ROOT / 'src/entity_pool.c').read_bytes()).hexdigest()
report['passed'] = True
(ROOT / 'analysis/native-entity-pool-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
