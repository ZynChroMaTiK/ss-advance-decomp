"""Complete original 08001B18 with actual direction/trig; BIOS division only."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD = ROOT / 'build/native-tests'
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                '-I', str(ROOT/'include'), str(ROOT/'src/respawn_select.c'),
                str(ROOT/'src/movement.c'), '-o', str(BUILD/'libssa_respawn_select.so')], check=True)
class Tables(C.Structure):
    _fields_ = [('sine', C.c_uint16*16384), ('direction', C.c_uint16*32769)]
class Marker(C.Structure):
    _fields_ = [('x', C.c_int16), ('y', C.c_int16), ('z', C.c_int16), ('heading', C.c_uint16)]
class Position(C.Structure):
    _fields_ = [('x', C.c_int32), ('y', C.c_int32)]
lib = C.CDLL(str(BUILD/'libssa_respawn_select.so'))
lib.ssa_math_tables_load.argtypes = [C.POINTER(Tables), C.c_void_p, C.c_size_t]
step = lib.ssa_respawn_select
step.argtypes = [C.POINTER(Marker), C.POINTER(Marker), C.c_size_t,
                 C.POINTER(Position), C.c_uint, C.POINTER(Tables)]
step.restype = C.c_int
tables = Tables()
assert lib.ssa_math_tables_load(C.byref(tables), ROM, len(ROM))
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
GAME, PLAYERS, OUTPUT, STOP = 0x02000000, 0x02001000, 0x02002000, 0x087ff000
uc.mem_write(0x03004010, struct.pack('<I', GAME))
uc.mem_write(0x03004648, struct.pack('<I', PLAYERS))
def signed(v): return v if v < 0x80000000 else v - 0x100000000
def division(machine, address, size, data):
    a, b = signed(machine.reg_read(UC_ARM_REG_R0)), signed(machine.reg_read(UC_ARM_REG_R1))
    assert b
    q = (abs(a)//abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
    machine.reg_write(UC_ARM_REG_R0, q & 0xffffffff)
    machine.reg_write(UC_ARM_REG_R1, (a-q*b) & 0xffffffff)
    machine.reg_write(UC_ARM_REG_R3, abs(q))
    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE, division, begin=0x08002c88, end=0x08002c88)
rng = random.Random(0x1b18)
report = {'rom_sha1': hashlib.sha1(ROM).hexdigest(), 'rom_comparisons': 0,
          'native_guards': 0, 'scope': 'Whole 08001B18, original direction and trig; BIOS signed division serviced. Full output, marker, player and game buffers compared.'}
def compare(markers, positions, alias=False):
    m = (Marker*len(markers))(*(Marker(*v) for v in markers))
    p = (Position*len(positions))(*(Position(*v) for v in positions))
    game = bytearray(rng.randbytes(0x80)); game[0x10] = len(positions)
    players = bytearray(rng.randbytes(1024))
    for i, pos in enumerate(positions): struct.pack_into('<ii', players, 256*i, *pos)
    out = Marker.from_buffer_copy(rng.randbytes(8))
    output = 0x03003f90 if alias else OUTPUT
    if alias: out = m[0]
    uc.mem_write(0x03003f80, struct.pack('<I', len(markers)))
    uc.mem_write(GAME, bytes(game)); uc.mem_write(PLAYERS, bytes(players))
    if markers: uc.mem_write(0x03003f90, bytes(m))
    if not alias: uc.mem_write(output, bytes(out))
    for reg, value in ((UC_ARM_REG_R0, output), (UC_ARM_REG_SP, 0x03007e00), (UC_ARM_REG_LR, STOP|1)):
        uc.reg_write(reg, value)
    uc.emu_start(0x08001b19, STOP, count=100000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    result = step(C.byref(out), m, len(m), p, len(p), C.byref(tables))
    assert result == uc.reg_read(UC_ARM_REG_R0), (markers, positions)
    assert bytes(out) == bytes(uc.mem_read(output, 8)), (markers, positions, bytes(out).hex(), bytes(uc.mem_read(output,8)).hex())
    assert bytes(m) == bytes(uc.mem_read(0x03003f90, 8*len(m)))
    assert bytes(p) == b''.join(struct.pack('<ii', *v) for v in positions)
    assert bytes(players) == bytes(uc.mem_read(PLAYERS, 1024))
    assert bytes(game) == bytes(uc.mem_read(GAME, len(game)))
    report['rom_comparisons'] += 1

# No starts/players, zero-coordinate skip, co-location, ties, 10000 cap,
# sub-unit coordinates and ARM signed distance wrap at 32768.
axes = [-32768, -32767, -10000, -1, 0, 1, 9999, 10000, 32767]
for x in axes:
    for y in axes:
        for px in (-32768, 0, 32767):
            for py in (-32768, 0, 32767):
                compare([(x,y,123,0x5678), (-x if x != -32768 else 32767,y,-9,42)], [(px*65536,py*65536)])
for count in range(5):
    for positions in ([(0,0)]*count, [(65536,65536)]*count, [(1,-1)]*count):
        for markers in ([], [(1,1,7,9)], [(5,5,7,9)]*3, [(10001,0,1,1),(10002,0,2,2)]):
            compare(markers, positions)
for _ in range(4096):
    markers = [(rng.randrange(-32768,32768),rng.randrange(-32768,32768),rng.randrange(-32768,32768),rng.randrange(65536)) for _ in range(rng.randrange(9))]
    positions = [(rng.randrange(-2147483648,2147483648),rng.randrange(-2147483648,2147483648)) for _ in range(rng.randrange(5))]
    compare(markers, positions)
    if markers and _ < 128: compare(markers, positions, True)

out = Marker(1,2,3,4); m = (Marker*1)(Marker(7,8,9,10)); p = (Position*1)(Position(100,200))
for args in ((None,m,1,p,1,C.byref(tables)), (C.byref(out),None,1,p,1,C.byref(tables)),
             (C.byref(out),m,1,None,1,C.byref(tables)), (C.byref(out),m,1,p,5,C.byref(tables)),
             (C.byref(out),m,1,p,1,None)):
    assert step(*args) == -1 and bytes(out) == bytes(Marker(1,2,3,4))
    report['native_guards'] += 1
report['source_sha256'] = hashlib.sha256((ROOT/'src/respawn_select.c').read_bytes()).hexdigest()
report['passed'] = True
(ROOT/'analysis/native-respawn-select-validation.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
