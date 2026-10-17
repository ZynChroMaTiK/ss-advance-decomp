"""Decode all real maps and compare collision setup with original Thumb code."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import struct
import subprocess
import sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R5, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from inspect_maps import unpack_lz77
ROM=(ROOT/"orig/SSA.gba").read_bytes()
SHA1="4ff48d8c18b70a5184758f1917343ae85a70858e"
assert hashlib.sha1(ROM).hexdigest()==SHA1
BUILD=ROOT/"build/native-tests"
BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(["gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-pedantic","-shared","-fPIC",
                "-fsanitize=undefined","-fno-sanitize-recover=all","-I",str(ROOT/"include"),
                str(ROOT/"src/map_assets.c"),"-o",str(BUILD/"libssa_map_assets.so")],check=True)
class Map(C.Structure):
    _fields_=[("data",C.POINTER(C.c_uint8)),("size",C.c_size_t)]+[(n+"_count",C.c_uint16) for n in
        ("thing","texture","sector","node","leaf","segment","side","line","vertex")]+[(n,C.c_uint32) for n in
        ("things","textures","sectors","nodes","leaves","segments","sides","lines","vertices","reject","blockmap","rom_offset")]
lib=C.CDLL(str(BUILD/"libssa_map_assets.so"))
lib.ssa_map_assets_load.argtypes=[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint]
lib.ssa_map_assets_load.restype=C.c_int
lib.ssa_map_assets_free.argtypes=[C.POINTER(Map)]
lib.ssa_lz77_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
lib.ssa_lz77_decode.restype=C.c_int
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x40000); uc.mem_map(0x03000000,0x10000)
BASE=0x02000000
def hook(uc,address,size,user):
    if address==0x08002c88:
        a=C.c_int32(uc.reg_read(UC_ARM_REG_R0)).value
        b=C.c_int32(uc.reg_read(UC_ARM_REG_R1)).value
        value=abs(a)//abs(b)*(-1 if (a<0)!=(b<0) else 1)
        uc.reg_write(UC_ARM_REG_R0,value&0xffffffff)
        uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
report={"rom_sha1":SHA1,"maps":[],"line_records_compared":0,"malformed_cases":0}
for level in range(16):
    m=Map()
    assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level),level
    raw,consumed=unpack_lz77(ROM[m.rom_offset:])
    dest=C.create_string_buffer(len(raw)); count=C.c_size_t()
    assert lib.ssa_lz77_decode(ROM[m.rom_offset:],consumed,dest,len(raw),C.byref(count))
    assert count.value==consumed and dest.raw==raw
    original=bytearray(raw)
    struct.pack_into("<I",original,0x38,BASE+m.lines)
    struct.pack_into("<I",original,0x3c,BASE+m.vertices)
    uc.mem_write(BASE,bytes(original))
    uc.reg_write(UC_ARM_REG_R5,BASE); uc.reg_write(UC_ARM_REG_R0,BASE+m.lines)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00)
    uc.emu_start(0x0800ad4c|1,0x0800adf4,count=1000000)
    assert uc.reg_read(UC_ARM_REG_PC)==0x0800adf4
    actual=C.string_at(m.data,m.size)
    expect=bytes(uc.mem_read(BASE+m.lines,m.line_count*40))
    assert actual[m.lines:m.lines+m.line_count*40]==expect,level
    # The native loader changes only the computed line fields.
    rebuilt=bytearray(raw); rebuilt[m.lines:m.lines+m.line_count*40]=expect
    assert actual==rebuilt
    report["line_records_compared"]+=m.line_count
    report["maps"].append({"index":level,"rom_offset":hex(m.rom_offset),"decoded_size":m.size,
        "compressed_size":consumed,"vertices":m.vertex_count,"lines":m.line_count,"sectors":m.sector_count,
        "nodes":m.node_count,"leaves":m.leaf_count,"decoded_sha256":hashlib.sha256(raw).hexdigest()})
    # Every position around stream boundaries rejects truncation and short output.
    for short in (0,1,3,4,5,consumed//2,consumed-2,consumed-1):
        assert not lib.ssa_lz77_decode(ROM[m.rom_offset:],short,dest,len(raw),None)
        report["malformed_cases"]+=1
    assert not lib.ssa_lz77_decode(ROM[m.rom_offset:],consumed,dest,len(raw)-1,None)
    report["malformed_cases"]+=1
    lib.ssa_map_assets_free(C.byref(m)); assert not m.data and not m.size

def literals(data):
    stream=bytearray([0x10])+len(data).to_bytes(3,"little")
    for i in range(0,len(data),8): stream.append(0); stream.extend(data[i:i+8])
    return bytes(stream)
raw,_=unpack_lz77(ROM[0x47cc5c:])
# Invalid decoded structures are tested through the real public loader.
changes=[(0,b"BAD!"),(0x28,struct.pack("<I",len(raw))),
         (0x3c,struct.pack("<I",0xffffffff)),(0x38,struct.pack("<I",4)),
         (0x44,struct.pack("<I",len(raw)-4)),(0x3c,raw[0x38:0x3c]),
         (4,b"\x00\x00"),(0x18,b"\x00\x00"),(0x1a,b"\x00\x00")]
nodes,leaves,lines,block=struct.unpack_from("<I",raw,0x28)[0],struct.unpack_from("<I",raw,0x2c)[0],struct.unpack_from("<I",raw,0x38)[0],struct.unpack_from("<I",raw,0x44)[0]
changes += [(nodes+24,b"\x00\x00"),(nodes+24,b"\xff\xff"),(leaves+4,b"\xff\xff"),
            (lines,b"\xff\xff"),(lines+12,b"\xff\xff"),(block+4,b"\x00\x00"),
            (block+8,b"\xff\xff")]
for offset,value in changes:
    bad=bytearray(raw); bad[offset:offset+len(value)]=value
    fixture=bytearray(ROM[:0x44aef4+64]); stream=literals(bad)
    struct.pack_into("<I",fixture,0x44aef4,0x08000000+len(fixture)); fixture.extend(stream)
    m=Map(); assert not lib.ssa_map_assets_load(C.byref(m),bytes(fixture),len(fixture),0),(offset,value)
    assert not m.data
    report["malformed_cases"]+=1
for stream in (b"\x11\x00\x00\x00",b"\x10\x03\x00\x00\x80\x00\x00",b"\x10\x03\x00\x00\x40A\x00\x00"):
    assert not lib.ssa_lz77_decode(stream,len(stream),C.create_string_buffer(32),32,None)
    report["malformed_cases"]+=1
assert not lib.ssa_map_assets_load(C.byref(Map()),ROM,len(ROM),16)
report["malformed_cases"]+=1
report["source_sha256"]=hashlib.sha256((ROOT/"src/map_assets.c").read_bytes()).hexdigest()
report["scope"]="16 real level streams decoded independently; original Thumb 0x0800AD4C..0x0800ADF4 line preprocessing, signed division hook only; malformed input checks"
(ROOT/"analysis/native-map-assets-validation.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps({k:v for k,v in report.items() if k!="maps"},indent=2))
