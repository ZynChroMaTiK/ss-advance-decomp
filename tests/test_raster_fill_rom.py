"""Complete original ARM row/column fills and sky copy, including overlaps."""
from pathlib import Path
import ctypes as C, hashlib, json, random, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/raster_fill.c'),'-o',str(B/'libssa_raster_fill.so')],check=True)
lib=C.CDLL(str(B/'libssa_raster_fill.so'))
for name in ('ssa_raster_fill_row','ssa_raster_fill_column'):getattr(lib,name).argtypes=[C.c_void_p,C.c_size_t,C.c_size_t,C.c_uint32,C.c_uint8]
lib.ssa_raster_copy_sky.argtypes=[C.c_void_p,C.c_size_t,C.c_size_t,C.c_void_p,C.c_size_t,C.c_size_t,C.c_uint32]
uc=Uc(UC_ARCH_ARM,UC_MODE_ARM)
uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000)
BASE,STOP=0x02000000,0x087ff000
size=32768;buf=(C.c_uint8*size)();rng=random.Random(0x3348)
initial=bytes(rng.randrange(256) for _ in range(size))
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'row_comparisons':0,'column_comparisons':0,'sky_comparisons':0,'overlap_comparisons':0,'guards':0}
def reset():C.memmove(buf,initial,size);uc.mem_write(BASE,initial)
def run(address):
    uc.reg_write(UC_ARM_REG_LR,STOP);uc.emu_start(address,STOP,count=200000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    assert bytes(buf)==bytes(uc.mem_read(BASE,size))
for stride,address,name in ((1,0x08000348,'row'),(120,0x08000338,'column')):
    function=getattr(lib,'ssa_raster_fill_'+name)
    for index in range(256):
        for count in (1,2,3,7,16,79,80,81,160,256):
            offset=rng.randrange(size-(count-1)*stride)
            reset();uc.reg_write(UC_ARM_REG_R1,BASE+offset);uc.reg_write(UC_ARM_REG_R2,count);uc.reg_write(UC_ARM_REG_R3,index)
            assert function(buf,size,offset,count,index)==1
            run(address);report[name+'_comparisons']+=1
    for offset in (0,1,size-1):
        count=(size-offset-1)//stride+1
        reset();uc.reg_write(UC_ARM_REG_R1,BASE+offset);uc.reg_write(UC_ARM_REG_R2,count);uc.reg_write(UC_ARM_REG_R3,0)
        assert function(buf,size,offset,count,0)==1
        run(address);report[name+'_comparisons']+=1
for count in (1,2,3,7,80,120,256,1024,16384):
    for source,target in ((0,0),(100,0),(0,100),(100,101),(101,100),(120,240),(240,120)):
        reset();uc.reg_write(UC_ARM_REG_R1,BASE+target);uc.reg_write(UC_ARM_REG_R2,BASE+source);uc.reg_write(UC_ARM_REG_R3,count)
        assert lib.ssa_raster_copy_sky(buf,size,target,buf,size,source,count)==1
        run(0x08000358);report['sky_comparisons']+=1
        if max(source,target)<min(source+count,target+count):report['overlap_comparisons']+=1
for _ in range(1000):
    count=rng.randrange(1,size+1);source=rng.randrange(size-count+1);target=rng.randrange(size-count+1)
    reset();uc.reg_write(UC_ARM_REG_R1,BASE+target);uc.reg_write(UC_ARM_REG_R2,BASE+source);uc.reg_write(UC_ARM_REG_R3,count)
    assert lib.ssa_raster_copy_sky(buf,size,target,buf,size,source,count)==1
    run(0x08000358);report['sky_comparisons']+=1
    if max(source,target)<min(source+count,target+count):report['overlap_comparisons']+=1
def guard(function,args):
    saved=bytes(buf);assert function(*args)==-1 and bytes(buf)==saved;report['guards']+=1
maximum=C.c_size_t(-1).value
for name in ('ssa_raster_fill_row','ssa_raster_fill_column'):
    fn=getattr(lib,name)
    for args in ((None,size,0,1,0),(buf,0,0,1,0),(buf,size,size,1,0),(buf,size,maximum,1,0),(buf,size,0,0,0),(buf,size,0,0xffffffff,0),(buf,size,size-1,2,0)):guard(fn,args)
for args in ((None,size,0,buf,size,0,1),(buf,size,0,None,size,0,1),(buf,size,0,buf,size,0,0),(buf,size,size,buf,size,0,1),(buf,size,0,buf,size,size,1),(buf,size,maximum,buf,size,0,1),(buf,size,0,buf,size,maximum,1),(buf,size,0,buf,size,0,0xffffffff)):guard(lib.ssa_raster_copy_sky,args)
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('src/raster_fill.c','include/ssa/raster_fill.h','tests/test_raster_fill_rom.py')}
(ROOT/'analysis/raster-fill-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
