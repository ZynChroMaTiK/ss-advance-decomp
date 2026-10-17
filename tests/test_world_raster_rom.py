"""Execute the complete original relocated ARM column, including code patching.

No ROM functions are hooked. This proves the column primitive, not BE60 wall
projection or a complete rendered scene. Unicorn is test-only.
"""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import (UC_ARM_REG_R1, UC_ARM_REG_R3, UC_ARM_REG_R4,
    UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD = ROOT / 'build/native-tests'
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
    '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
    '-I', str(ROOT / 'include'), str(ROOT / 'src/world_raster.c'),
    str(ROOT / 'src/texture_assets.c'),
    '-o', str(BUILD / 'libssa_world_raster.so')], check=True)
lib = C.CDLL(str(BUILD / 'libssa_world_raster.so'))
run = lib.ssa_world_raster_column
run.argtypes = [C.POINTER(C.c_uint8), C.c_size_t, C.c_size_t,
    C.POINTER(C.c_uint8), C.c_size_t, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint]
run.restype = C.c_int
class Image(C.Structure):
    _fields_ = [('name', C.c_char * 8), ('width', C.c_uint16), ('height', C.c_uint16),
               ('rom_offset', C.c_uint32), ('pixels', C.POINTER(C.c_uint8))]
class Archive(C.Structure):
    _fields_ = [('rom_offset', C.c_uint32), ('count', C.c_uint16), ('images', C.POINTER(Image))]
class Family(C.Structure):
    _fields_ = [('archives', Archive * 3), ('mip_indices', C.POINTER(C.c_uint16 * 3))]
class Assets(C.Structure):
    _fields_ = [('families', Family * 2), ('loaded', C.c_uint)]
lib.ssa_texture_assets_load.argtypes = [C.POINTER(Assets), C.c_void_p, C.c_size_t]
lib.ssa_texture_assets_free.argtypes = [C.POINTER(Assets)]
uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
uc.mem_map(0x02000000, 0x40000)
uc.mem_map(0x03000000, 0x10000)
CODE, ARGS, FRAME, TEX, STOP = 0x03000270, 0x03005080, 0x02001000, 0x02010000, 0x03007f00
SP = 0x03007e00
SIZE = 120 * 122
rng = random.Random(0x2f4)
report = {'rom_sha1': hashlib.sha1(ROM).hexdigest(), 'routine': '080002F4 / 03000270',
    'whole_arm_comparisons': 0, 'overlap_cases': 0, 'one_pixel_cases': 0,
    'final_pixel_repeated': 0, 'final_pixel_kept': 0, 'guards': 0,
    'real_texture_cases': 0, 'real_texture_records': 0}


def compare(count, shift, fraction, step, texture=None, destination=None,
            overlap=None, native_texture=None):
    source = rng.randbytes(128) if texture is None else texture
    initial = rng.randbytes(SIZE)
    destination = 120 + rng.randrange(120) if destination is None else destination
    native = (C.c_uint8 * SIZE).from_buffer_copy(initial)
    native_source = (C.c_uint8 * len(source)).from_buffer_copy(source)
    if native_texture is not None:
        native_source = native_texture
    source_address = TEX
    if overlap is not None:
        native_source = C.cast(C.byref(native, overlap), C.POINTER(C.c_uint8))
        source_address = FRAME + overlap
        source = initial[overlap:overlap + 128]
        report['overlap_cases'] += 1
    opcode = 0xe7d21023 | ((shift & 31) << 7)
    args = struct.pack('<6I', FRAME + destination, source_address,
                       fraction, step, opcode, count)
    uc.mem_write(CODE, ROM[0x2f4:0x338])
    uc.mem_write(ARGS, args)
    uc.mem_write(FRAME, initial)
    if overlap is None:
        uc.mem_write(TEX, source)
    preserved = [rng.getrandbits(32) for _ in range(3)]
    for register, value in zip((UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6), preserved):
        uc.reg_write(register, value)
    uc.reg_write(UC_ARM_REG_R1, ARGS)
    uc.reg_write(UC_ARM_REG_SP, SP)
    uc.reg_write(UC_ARM_REG_LR, STOP)
    uc.emu_start(CODE, STOP, count=3)
    # ARM7 refetches the patched instruction after its two pipeline NOPs.
    # Unicorn caches a whole translation block, so invalidate that block after
    # the original STR (which really ran) and continue at the original next PC.
    assert uc.reg_read(UC_ARM_REG_PC) == CODE + 12
    uc.ctl_remove_cache(CODE, CODE + 0x44)
    uc.emu_start(CODE + 12, STOP, count=2000)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    assert uc.reg_read(UC_ARM_REG_SP) == SP
    assert [uc.reg_read(r) for r in (UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6)] == preserved
    assert uc.reg_read(UC_ARM_REG_R3) == (fraction + step * count) & 0xffffffff
    assert uc.mem_read(CODE + 0x14, 4) == struct.pack('<I', opcode)
    assert uc.mem_read(ARGS, 24) == args
    assert run(native, SIZE, destination, native_source, len(source),
               fraction, step, count, shift) == 0
    assert bytes(native) == bytes(uc.mem_read(FRAME, SIZE)), (count, shift, fraction, step, overlap, [(i, native[i], b, initial[i]) for i, b in enumerate(uc.mem_read(FRAME, SIZE)) if native[i] != b][:8])
    if overlap is None:
        assert C.string_at(native_source, len(source)) == source == bytes(uc.mem_read(TEX, len(source)))
    report['whole_arm_comparisons'] += 1
    report['one_pixel_cases'] += count == 1
    repeat = (step ^ 0x80000000) <= (((fraction + step * count) & 0xffffffff) ^ 0x80000000)
    report['final_pixel_repeated' if repeat else 'final_pixel_kept'] += 1


# Every caller-supported height and legal column length; signed comparison and
# accumulator wrap endpoints. Fractions lie across all quadrants of the word.
shifts = (25, 26, 27, 28, 29, 30, 32)
edges = (0, 1, 0x00ffffff, 0x01000000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff)
steps = (0, 1, 0x00abcdef, 0x20000000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff)
for shift in shifts:
    for count in range(1, 121):
        for fraction in edges:
            for step in steps:
                compare(count, shift, fraction, step)
print('PASS all column lengths and original sample opcodes', flush=True)

# Exact sample-index boundaries and arbitrary wrapped phase increments.
for shift in shifts:
    for index in range(1 if shift == 32 else 1 << (32 - shift)):
        for delta in (-1, 0, 1):
            compare(rng.randrange(1, 121), shift, ((index << shift) + delta) & 0xffffffff,
                    rng.getrandbits(32))
for _ in range(4096):
    compare(rng.randrange(1, 121), rng.choice(shifts), rng.getrandbits(32), rng.getrandbits(32))

# Actual read-after-write behavior for overlapping source/framebuffer memory.
# This deliberately includes repeated writes to texture[0] and later reads.
for count in (1, 2, 3, 64, 119, 120):
    for shift in shifts:
        for step in (0, 1, 0x01000000, 0x80000000, 0xffffffff):
            for offset in (0, 1, 120, 121, 240):
                compare(count, shift, rng.getrandbits(32), step,
                        destination=120, overlap=offset)


def guard(frame_size=SIZE, destination=120, source_size=128, fraction=0,
          step=0, count=2, shift=25, null_frame=False, null_texture=False):
    frame = (C.c_uint8 * SIZE).from_buffer_copy(rng.randbytes(SIZE))
    source = (C.c_uint8 * 128).from_buffer_copy(rng.randbytes(128))
    before, old_source = bytes(frame), bytes(source)
    assert run(None if null_frame else frame, frame_size, destination,
               None if null_texture else source, source_size,
               fraction, step, count, shift) == -1
    assert bytes(frame) == before and bytes(source) == old_source
    report['guards'] += 1


guard(null_frame=True)
guard(null_texture=True)
guard(frame_size=0)
guard(destination=SIZE)
guard(destination=C.c_size_t(-1).value)
guard(count=0)
guard(count=121)
guard(count=0xffffffff)
guard(destination=SIZE-120, count=2)
guard(source_size=0)
guard(source_size=1, fraction=1 << 25)
guard(source_size=1, fraction=0, step=1 << 25)  # later sample failure is atomic
for invalid_shift in (*range(25), 31, 33, 255, 0xffffffff):
    guard(shift=invalid_shift)
for destination in (0, 1, 119):
    guard(destination=destination, count=1, fraction=0, step=0)
# count1 at the first row is safe when the original final-copy branch is skipped.
for shift in shifts:
    compare(1, shift, 0x80000000, 0, destination=0)
# Last legal pixel and unaligned positions are included in complete ARM checks.
for shift in shifts:
    compare(1, shift, 0, 0, destination=SIZE-1)
    compare(120, shift, 0xffffffff, 1, destination=239)

# Bridge the native archive loader directly into the native ARM-derived sampler.
# Every real mip image contributes first/middle/last vertical columns, sampled
# using several wrapped phases and steps. Original code reads those exact bytes.
assets = Assets()
rom_buffer = C.create_string_buffer(ROM)
assert lib.ssa_texture_assets_load(C.byref(assets), rom_buffer, len(ROM)) == 1
del rom_buffer
for family in assets.families:
    for archive in family.archives:
        for im in archive.images[:archive.count]:
            shift = {4: 30, 8: 29, 16: 28, 32: 27, 64: 26, 128: 25}.get(im.height, 32)
            for x in sorted({0, im.width // 2, im.width - 1}):
                pointer = C.cast(C.addressof(im.pixels.contents) + x * im.height,
                                 C.POINTER(C.c_uint8))
                column = C.string_at(pointer, im.height)
                assert column == ROM[im.rom_offset + x * im.height:
                                     im.rom_offset + (x + 1) * im.height]
                for fraction, step, count in ((0, 0x01000000, 120),
                        (0x7fffffff, 0xffffffff, 63), (0xffffffff, 0x10000001, 2),
                        (0x80000000, 0, 1)):
                    compare(count, shift, fraction, step, texture=column,
                            native_texture=pointer)
                    report['real_texture_cases'] += 1
            report['real_texture_records'] += 1
lib.ssa_texture_assets_free(C.byref(assets))
assert report['real_texture_records'] == 666

report['source_sha256'] = hashlib.sha256((ROOT/'src/world_raster.c').read_bytes()).hexdigest()
report['header_sha256'] = hashlib.sha256((ROOT/'include/ssa/world_raster.h').read_bytes()).hexdigest()
report['texture_assets_source_sha256'] = hashlib.sha256((ROOT/'src/texture_assets.c').read_bytes()).hexdigest()
report['test_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
report['validation'] = ('Complete original relocated ARM, actual self-modifying LDRB with '
    'Unicorn cache synchronization after its STR, no hooks; native GCC C11 with UBSan; '
    'native texture_assets loader supplies all 666 actual mip images')
(ROOT/'analysis/native-world-raster-validation.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2), flush=True)
