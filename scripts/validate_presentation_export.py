"""Validate the new presentation decoder against native C and original ROM code."""
from pathlib import Path
import ctypes
import hashlib
import json
import struct
import zipfile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
from export_presentation_assets import decompress, untile

ROOT=Path(__file__).resolve().parents[1]
rom=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
archive=ROOT/'build/exports/ssa-graphics.new.zip'
report={'rom_sha1':hashlib.sha1(rom).hexdigest(),'lz77_native_comparisons':0,'story_layout_rom_comparisons':0,
        'menu_map_rom_comparisons':0,'preserved_old_payloads':0}
lib=ctypes.CDLL(str(ROOT/'build/native-tests/libssa_map_assets.so'))
lib.ssa_lz77_decode.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
lib.ssa_lz77_decode.restype=ctypes.c_int
with zipfile.ZipFile(archive) as z:
    manifest=json.loads(z.read('manifest.json'))
    offsets=[p['rom_offset'] for p in manifest['presentation'] if p['compression']=='lz77']
    offsets += [m['rom_offset'] for m in manifest['maps']]
    for offset in offsets:
        decoded,consumed=decompress(rom,offset)
        src=ctypes.create_string_buffer(rom[offset:offset+consumed])
        dst=ctypes.create_string_buffer(len(decoded)); count=ctypes.c_size_t()
        assert lib.ssa_lz77_decode(src,consumed,dst,len(decoded),ctypes.byref(count))==1
        assert count.value==consumed and dst.raw==decoded
        report['lz77_native_comparisons']+=1
    with zipfile.ZipFile(ROOT/'build/exports/ssa-graphics.zip') as old:
        before=json.loads(old.read('manifest.json'))
        for name,meta in before['files'].items():
            if name in ('README.txt','export_graphics_archive.py'): continue
            assert manifest['files'][name]==meta, name
            report['preserved_old_payloads']+=1

STOP=0x087ff000
def oracle(address,src=None):
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
    for base,size in [(0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x1000),
                      (0x05000000,0x1000),(0x06000000,0x20000),(0x08000000,len(rom))]:
        uc.mem_map(base,size)
    uc.mem_write(0x08000000,rom)
    uc.mem_write(0x0300572c,struct.pack('<I',0x02030000))
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    if src is not None:
        uc.mem_write(0x02000000,src)
        uc.reg_write(UC_ARM_REG_R0,0x02000000);uc.reg_write(UC_ARM_REG_R1,0x02010000)
    def hook(u,pc,size,_):
        skip=False
        if pc==0x08015888:
            p=u.reg_read(UC_ARM_REG_R0)-0x08000000
            data,_=decompress(rom,p)
            u.mem_write(u.reg_read(UC_ARM_REG_R1),data);skip=True
        elif pc==0x08003444:
            u.reg_write(UC_ARM_REG_R0,0x02000000);skip=True
        elif pc in (0x08015708,0x08015718,0x080033fc,0x0800341c):
            skip=True
        if skip:u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE,hook)
    uc.emu_start(address|1,STOP,count=300000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    return uc

for i in range(8):
    off=struct.unpack_from('<I',rom,0x44cfa4+i*4)[0]-0x08000000
    decoded,_=decompress(rom,off)
    uc=oracle(0x08018ee4,decoded)
    result=bytes(uc.mem_read(0x02010000,16384))
    pieces=[untile(result[j*4096:(j+1)*4096],64,64) for j in range(4)]
    # Rebuild screen pixels from the ORIGINAL routine's four OBJ outputs.
    rows=b''.join(pieces[(y//64)*2][(y%64)*64:(y%64+1)*64]+
                  pieces[(y//64)*2+1][(y%64)*64:(y%64+1)*64] for y in range(128))
    assert rows==untile(decoded,128,128)
    report['story_layout_rom_comparisons']+=1

entries=[((8-band)*8+j)&511 for band in range(8) for j in range(512)]
for address,base,block_rows in [(0x080164e8,0x0600e000,2),(0x080191b4,0x0600f000,1)]:
    uc=oracle(address)
    expected=[entries[(by*32+y)*64+bx*32+x] for by in range(block_rows) for bx in range(2) for y in range(32) for x in range(32)]
    assert bytes(uc.mem_read(base,len(expected)*2))==struct.pack('<'+'H'*len(expected),*expected)
    report['menu_map_rom_comparisons']+=1
report['status']='PASS'
report['decoder_sha256']=hashlib.sha256((ROOT/'scripts/export_presentation_assets.py').read_bytes()).hexdigest()
(ROOT/'build/exports/presentation-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
