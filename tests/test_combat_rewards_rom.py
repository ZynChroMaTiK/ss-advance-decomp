"""Complete 080125E0 with all 16-bit types and score wrap boundaries."""
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
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD = ROOT / 'build/native-tests'
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
    '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
    '-I', str(ROOT / 'include'), str(ROOT / 'src/combat_rewards.c'),
    '-o', str(BUILD / 'libssa_combat_rewards.so')], check=True)
lib = C.CDLL(str(BUILD / 'libssa_combat_rewards.so'))
reward = lib.ssa_combat_reward
reward.argtypes = [C.c_uint16, C.POINTER(C.c_uint32)]
reward.restype = C.c_int
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
E, G, STOP = 0x02000000, 0x02001000, 0x087ff000
uc.mem_write(0x03004010, struct.pack('<I', G))
rng = random.Random(0x125e0)
comparisons = 0

def compare(typ, initial):
    global comparisons
    entity = bytearray(rng.randbytes(0x74))
    game = bytearray(rng.randbytes(0xc0))
    struct.pack_into('<H', entity, 0x3c, typ)
    struct.pack_into('<I', game, 0x88, initial)
    uc.mem_write(E, bytes(entity)); uc.mem_write(G, bytes(game))
    for register, value in ((UC_ARM_REG_R0,E),(UC_ARM_REG_SP,0x03007e00),
                            (UC_ARM_REG_LR,STOP|1)):
        uc.reg_write(register, value)
    uc.emu_start(0x080125e1, STOP, count=80)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    score = C.c_uint32(initial)
    assert reward(typ, C.byref(score)) == 0
    struct.pack_into('<I', game, 0x88, score.value)
    assert bytes(uc.mem_read(G, len(game))) == bytes(game), (typ, initial)
    assert bytes(uc.mem_read(E, len(entity))) == bytes(entity), (typ, initial)
    comparisons += 1

for typ in range(65536): compare(typ, rng.getrandbits(32))
for typ in range(0xc0, 0xe2):
    for initial in [0,1,1999,2000,0x7fffffff,0x80000000,0xfffff82f,
                    0xfffff830,0xfffff831,0xfffffffe,0xffffffff]:
        compare(typ, initial)
assert reward(0xcd, None) == -1
report = {'status': 'passed', 'rom_sha1': hashlib.sha1(ROM).hexdigest(),
    'scope': 'Complete 080125E0 with every 16-bit type; full entity and GAME memory compared; no hooked dependencies',
    'total_rom_comparisons': comparisons, 'native_guards': 1,
    'source_sha256': hashlib.sha256((ROOT / 'src/combat_rewards.c').read_bytes()).hexdigest()}
(ROOT / 'analysis/native-combat-rewards-validation.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
