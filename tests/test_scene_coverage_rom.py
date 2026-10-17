"""Complete00A84/00E3C secondarycoverage; all live/spare bytes compared."""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-DSSA_SCENE_COVERAGE_ONLY','-I',str(ROOT/'include'),str(ROOT/'src/world_scene.c'),'-o',str(B/'libssa_scene_coverage.so')],check=True)
class Range(C.Structure):_fields_=[('left',C.c_int16),('right',C.c_int16)]
class Coverage(C.Structure):_fields_=[('ranges',Range*124),('count',C.c_uint)]
lib=C.CDLL(str(B/'libssa_scene_coverage.so'))
lib.ssa_scene_coverage_reset.argtypes=[C.POINTER(Coverage)]
lib.ssa_scene_coverage_merge.argtypes=[C.POINTER(Coverage),C.c_uint,C.c_uint]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for a,n in ((0x08000000,len(ROM)),(0x02000000,0x10000),(0x03000000,0x10000)):uc.mem_map(a,n)
uc.mem_write(0x08000000,ROM)
BASE,STOP=0x02000000,0x087ff000
uc.mem_write(0x03003f48,struct.pack('<I',BASE))
c=Coverage();rng=random.Random(0xe3c)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_reset_calls':0,'whole_merge_calls':0,'guards':0,'max_stored_ranges':0}
def run(address):
    uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1);uc.emu_start(address|1,STOP,count=10000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    n=(struct.unpack('<I',uc.mem_read(0x03003f50,4))[0]-BASE)//4
    assert c.count==n and C.string_at(C.addressof(c),496)==bytes(uc.mem_read(BASE,496)),(c.count,n)
    report['max_stored_ranges']=max(report['max_stored_ranges'],n)
def reset():
    raw=rng.randbytes(496);C.memmove(C.addressof(c),raw,496);c.count=77;uc.mem_write(BASE,raw)
    assert lib.ssa_scene_coverage_reset(C.byref(c))==1;run(0x08000a84);report['whole_reset_calls']+=1
def merge(left,right):
    uc.reg_write(UC_ARM_REG_R0,left);uc.reg_write(UC_ARM_REG_R1,right)
    assert lib.ssa_scene_coverage_merge(C.byref(c),left,right)==1
    run(0x08000e3c);report['whole_merge_calls']+=1
for left in range(120):
    for right in range(left,121):reset();merge(left,right)
for _ in range(512):
    reset()
    for i in range(64):
        a,b=sorted((rng.randrange(121),rng.randrange(121)))
        if a==120:a=119
        merge(a,b)
reset()
for x in range(0,120,2):merge(x,x)
for x in range(119,-1,-2):merge(x,x)
def guard(pointer,left,right):
    saved=bytes(c);assert lib.ssa_scene_coverage_merge(pointer,left,right)==-1 and bytes(c)==saved;report['guards']+=1
for args in ((None,0,1),(C.byref(c),120,120),(C.byref(c),0,121),(C.byref(c),3,2),(C.byref(c),0xffffffff,1)):guard(*args)
for count in (0,1,124,0xffffffff):c.count=count;guard(C.byref(c),0,1)
assert lib.ssa_scene_coverage_reset(None)==-1;report['guards']+=1
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('src/world_scene.c','include/ssa/world_scene.h','tests/test_scene_coverage_rom.py')}
(ROOT/'analysis/scene-coverage-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
