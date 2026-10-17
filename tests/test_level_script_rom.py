"""Original VM/queue/sector trigger/reset/release versus native data interpreter."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
SHA1 = '4ff48d8c18b70a5184758f1917343ae85a70858e'
assert hashlib.sha1(ROM).hexdigest() == SHA1
BUILD = ROOT / 'build/native-tests'
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                '-I', str(ROOT/'include'), str(ROOT/'src/level_script.c'),
                '-o', str(BUILD/'libssa_level_script.so')], check=True)
lib = C.CDLL(str(BUILD/'libssa_level_script.so'))
class Data(C.Structure):
    _fields_ = [('rom', C.POINTER(C.c_uint8)), ('size', C.c_size_t),
                ('table', C.c_uint32*16), ('count', C.c_uint16*16)]
class VM(C.Structure):
    _fields_ = [('pc', C.c_uint32*5), ('state', (C.c_uint32*5)*5), ('count', C.c_uint32),
                ('alive_before', C.c_uint8), ('alive', C.c_uint8*256), ('visited', C.c_uint8*256),
                ('game_state', C.c_uint16), ('exit_toggle', C.c_uint16), ('updating', C.c_uint)]
class Entity(C.Structure):
    _fields_ = [('x',C.c_int32),('y',C.c_int32),('z',C.c_int32),
                ('type',C.c_uint16),('flags',C.c_uint16),('trigger',C.c_uint16),
                ('timer',C.c_uint16),('floor',C.c_uint16),('variant',C.c_uint8),('facing',C.c_uint8)]
ALLOC = C.CFUNCTYPE(C.c_void_p,C.c_void_p)
ANIM = C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Entity),C.c_uint,C.c_uint)
INSERT = C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Entity))
PARTICLE = C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_int32,C.c_int32,C.c_int32)
MOVER = C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_uint32)
class Ops(C.Structure):
    _fields_ = [('allocate',ALLOC),('animation',ANIM),('insert',INSERT),
                ('particle',PARTICLE),('mover',MOVER),('context',C.c_void_p)]
load=lib.ssa_level_script_data_load; load.argtypes=[C.POINTER(Data),C.POINTER(C.c_uint8),C.c_size_t]
reset=lib.ssa_level_script_reset;reset.argtypes=[C.POINTER(VM)]
queue=lib.ssa_level_script_queue;queue.argtypes=[C.POINTER(VM),C.POINTER(Data),C.c_uint,C.c_uint]
enter=lib.ssa_level_script_enter;enter.argtypes=[C.POINTER(VM),C.POINTER(Data),C.c_uint,C.c_uint,C.c_uint16]
release=lib.ssa_level_script_release;release.argtypes=[C.POINTER(VM),C.c_uint16]
step=lib.ssa_level_script_step;step.argtypes=[C.POINTER(VM),C.POINTER(Data),C.c_uint,C.c_int32,C.POINTER(Ops)]
rombuf=(C.c_uint8*len(ROM)).from_buffer_copy(ROM)
data=Data();assert load(C.byref(data),rombuf,len(ROM))==1
assert list(data.count)==[5,12,10,11,10,13,10,15,9,3,7,11,0,0,0,0]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000)
GAME,ALIVE,VISITED,ENTITY,LEVEL,MAP,SECTORS=0x02000000,0x02000100,0x02000200,0x02001000,0x02002000,0x02002100,0x02003000
STOP=0x087ff000; SYN=0x087f0000; SPAWNS=0x087f1000
uc.mem_write(0x03004010,struct.pack('<I',GAME))
uc.mem_write(0x030057b0,struct.pack('<I',ALIVE));uc.mem_write(0x030057b8,struct.pack('<I',VISITED))
uc.mem_write(0x0300572c,struct.pack('<I',LEVEL));uc.mem_write(0x03004930,struct.pack('<I',MAP))
uc.mem_write(MAP+0x24,struct.pack('<I',SECTORS))
rng=random.Random(0x1c6dc)
report={'rom_sha1':SHA1,'scope':'whole VM 1C6DC, queue 1C670, enter 1C624, reset 1C5A8, release 1C6B0; allocation/animation/insertion/particles/movers hooked and compared in order; script data read natively',
        'rom_comparisons':{},'native_guards':0,'real_scripts':0,'real_script_frames':0,
        'allocation_failure_z':'explicit incoming r10 supplied as fallback_z; not an original defined value'}
fields=[('x',0,'i'),('y',4,'i'),('z',8,'i'),('type',0x3c,'H'),('flags',0x30,'H'),
        ('trigger',0x68,'H'),('timer',0x4e,'H'),('floor',0x72,'H'),('variant',0x3e,'B'),('facing',0x3f,'B')]
def entity_from(raw):
    return Entity(**{name:struct.unpack_from('<'+fmt,raw,offset)[0] for name,offset,fmt in fields})
def entity_tuple(e):return tuple(getattr(e,n) for n,_,_ in fields)
def oracle_entity(addr):return entity_tuple(entity_from(uc.mem_read(addr,0x74)))
def signed(v):return C.c_int32(v).value
native_log=[];oracle_log=[];entities=[];raw_entities=[];allocation_index=0;oracle_index=0;floor_value=0;failure_pattern=0
@ALLOC
def allocate(_):
    global allocation_index
    index=allocation_index;allocation_index+=1
    native_log.append(('allocate',))
    if failure_pattern & (1<<index):return None
    e=entity_from(raw_entities[index]);entities.append(e)
    return C.addressof(e)
@ANIM
def animation(_,e,a,b):native_log.append(('animation',entity_tuple(e.contents),a,b));return 0
@INSERT
def insert(_,e):
    native_log.append(('insert',entity_tuple(e.contents)))
    e.contents.floor=floor_value
    return 0
@PARTICLE
def particle(_,a,x,y,z):native_log.append(('particle',a,x,y,z));return 0
@MOVER
def mover(_,a,b):native_log.append(('mover',a,b));return 0
ops=Ops(allocate,animation,insert,particle,mover,None)
alloc_mem=[]
def hook(u,address,size,_):
    global oracle_index
    args=[u.reg_read(UC_ARM_REG_R0+i) for i in range(4)]
    if address==0x08005170:
        index=oracle_index;oracle_index+=1;oracle_log.append(('allocate',))
        addr=0 if failure_pattern&(1<<index) else ENTITY+index*0x80
        if addr:u.mem_write(addr,raw_entities[index]);alloc_mem.append((addr,index))
        u.reg_write(UC_ARM_REG_R0,addr)
    elif address==0x08005394:oracle_log.append(('animation',oracle_entity(args[0]),args[1],args[2]))
    elif address==0x0800a5d0:
        oracle_log.append(('insert',oracle_entity(args[0])));u.mem_write(args[0]+0x72,struct.pack('<H',floor_value))
    elif address==0x08014b60:oracle_log.append(('particle',args[0],*[signed(x) for x in args[1:]]))
    elif address in (0x0801c02c,0x0801c050,0x0801c0b4,0x0801c098):
        oracle_log.append(('mover',{0x0801c02c:4,0x0801c050:5,0x0801c0b4:6,0x0801c098:7}[address],args[0]))
    elif address==0x080033d8:
        oracle_log.append(('malloc',args[0]));u.reg_write(UC_ARM_REG_R0,ALIVE if len(oracle_log)==1 else VISITED)
    else:return
    u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
for addr in (0x08005170,0x08005394,0x0800a5d0,0x08014b60,0x0801c02c,0x0801c050,0x0801c0b4,0x0801c098,0x080033d8):
    uc.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
def run(addr,arg=0,fallback=0):
    for reg,val in ((UC_ARM_REG_R0,arg),(UC_ARM_REG_R10,fallback&0xffffffff),
                    (UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):
        uc.reg_write(reg,val)
    uc.emu_start(addr|1,STOP,count=20000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
def write_vm(v,ticks=0):
    uc.mem_write(0x030057b4,struct.pack('<I',v.count));uc.mem_write(0x030057c0,bytes(v.pc))
    uc.mem_write(0x030057e0,bytes(v.state));uc.mem_write(ALIVE-1,bytes([v.alive_before])+bytes(v.alive));uc.mem_write(VISITED,bytes(v.visited))
    uc.mem_write(0x03004304,struct.pack('<H',v.game_state));uc.mem_write(0x03004180,struct.pack('<H',v.exit_toggle))
    uc.mem_write(GAME+7,bytes([ticks]))
def check_vm(v):
    assert v.count==struct.unpack('<I',uc.mem_read(0x030057b4,4))[0]
    assert bytes(v.pc)==bytes(uc.mem_read(0x030057c0,20)),(list(v.pc),bytes(uc.mem_read(0x030057c0,20)).hex())
    assert bytes(v.state)==bytes(uc.mem_read(0x030057e0,100)),(bytes(v.state).hex(),bytes(uc.mem_read(0x030057e0,100)).hex())
    assert bytes([v.alive_before])+bytes(v.alive)==bytes(uc.mem_read(ALIVE-1,257))
    assert bytes(v.visited)==bytes(uc.mem_read(VISITED,256))
    assert v.game_state==struct.unpack('<H',uc.mem_read(0x03004304,2))[0]
    assert v.exit_toggle==struct.unpack('<H',uc.mem_read(0x03004180,2))[0]
    assert v.updating==0
def random_vm():
    v=VM.from_buffer_copy(rng.randbytes(C.sizeof(VM)));v.count=0;v.updating=0
    return v
def count(name):report['rom_comparisons'][name]=report['rom_comparisons'].get(name,0)+1
def patch(address,blob):
    C.memmove(C.addressof(rombuf)+address-0x08000000,blob,len(blob));uc.mem_write(address,blob)
def frame(v,ticks,fallback=0,fail=0):
    global allocation_index,oracle_index,floor_value,failure_pattern
    native_log.clear();oracle_log.clear();entities.clear();alloc_mem.clear();raw_entities.clear()
    allocation_index=oracle_index=0;failure_pattern=fail;floor_value=rng.randrange(65536)
    raw_entities.extend(rng.randbytes(0x74) for _ in range(5))
    write_vm(v,ticks);run(0x0801c6dc,fallback=fallback)
    assert step(C.byref(v),C.byref(data),ticks,fallback,C.byref(ops))==0, (list(v.pc), [list(s) for s in v.state], native_log, oracle_log)
    check_vm(v);assert native_log==oracle_log,(native_log,oracle_log)
    for e,(addr,index) in zip(entities,alloc_mem):
        b=bytearray(raw_entities[index])
        for name,off,fmt in fields:struct.pack_into('<'+fmt,b,off,getattr(e,name))
        assert b==bytes(uc.mem_read(addr,0x74))

# Reset: original zeroes only 3 of each scratch record's 5 words.
for _ in range(128):
    v=random_vm();write_vm(v);oracle_log.clear();run(0x0801c5a8)
    assert reset(C.byref(v))==1;check_vm(v)
    assert oracle_log==[('malloc',256),('malloc',256)];count('reset')
# All real per-level script IDs, at every free queue position.
for level in range(16):
    uc.mem_write(LEVEL+16,struct.pack('<H',level))
    for ident in range(1,data.count[level]+1):
        for oldcount in range(5):
            v=random_vm();v.count=oldcount;write_vm(v);run(0x0801c670,ident)
            assert queue(C.byref(v),C.byref(data),level,ident)==1;check_vm(v);count('queue')
# First visit always latches, even with no script; duplicate visits do nothing.
for _ in range(4096):
    v=random_vm();level=rng.randrange(12);sector=rng.randrange(256);ident=rng.randrange(1,data.count[level]+1)
    v.visited[sector]=rng.choice([0,0,1,255]);v.count=rng.randrange(5)
    flags=rng.randrange(65536)&0xfe00|ident|(rng.randrange(2)<<8)
    uc.mem_write(LEVEL+16,struct.pack('<H',level));uc.mem_write(SECTORS+20*sector+16,struct.pack('<H',flags))
    write_vm(v);run(0x0801c624,sector)
    assert enter(C.byref(v),C.byref(data),level,sector,flags)==1;check_vm(v);count('sector_enter')
for tag in range(512):
    for _ in range(4):
        v=random_vm();write_vm(v);uc.mem_write(ENTITY+0x68,struct.pack('<H',tag));run(0x0801c6b0,ENTITY)
        assert release(C.byref(v),tag)==1;check_vm(v);count('release')
# Synthetic records cover all switch cases, unknown opcodes, wrapped timers,
# pending spawn sequences, allocator exhaustion and mixed-slot compaction.
for case in range(8192):
    v=random_vm();v.count=rng.randrange(6)
    for i in range(5):
        op=rng.choice([0,1,1,1,2,3,4,5,6,7,8,8,9,10,11,0xffffffff]);pc=SYN+i*80
        arg=rng.randrange(1,257) if op==2 else rng.randrange(1,512) if op==1 else rng.getrandbits(32)
        delay=rng.choice([0,1,2,30,60,255,0x7fffffff,0x80000000,0xffffffff])
        sp=SPAWNS+i*32
        patch(pc,struct.pack('<5I',op,sp,arg,rng.getrandbits(32),delay)+struct.pack('<5I',10,0,0,0,0))
        x,y=(0,0) if rng.randrange(5)==0 else (rng.randrange(65536),rng.randrange(65536))
        patch(sp,struct.pack('<4H',x,y,rng.randrange(64),rng.randrange(65536))+bytes(8))
        v.pc[i]=pc
        if op in (1,8) and rng.randrange(2):v.state[i][0]=op;v.state[i][1]=sp
        elif v.state[i][0] in (1,8):v.state[i][0]=0
    frame(v,rng.randrange(256),signed(rng.getrandbits(32)),rng.randrange(32));count('vm_mixed')

# Execute every original script to its end or level-exit opcode. Enemy deaths
# are an external event: release live-group counters between frames here.
for level in range(12):
    for ident in range(1,data.count[level]+1):
        v=VM();assert queue(C.byref(v),C.byref(data),level,ident)
        for n in range(4096):
            frame(v,255);report['real_script_frames']+=1;count('vm_real_data')
            if not v.count or v.game_state==3:break
            for i in range(256):v.alive[i]=0
        else:raise AssertionError(('script did not finish',level,ident,list(v.pc)))
        report['real_scripts']+=1

# Native bounded-input and reentry contracts; mutation-free entry checks.
v=VM();before=bytes(v)
def guard(value):
    assert value;report['native_guards']+=1
for level,ident in ((16,1),(0,0),(0,6),(12,1),(0,65537)):
    guard(queue(C.byref(v),C.byref(data),level,ident)==0);assert bytes(v)==before
v.count=5;guard(queue(C.byref(v),C.byref(data),0,1)==0)
v.count=0;guard(enter(C.byref(v),C.byref(data),0,256,0)==0)
guard(enter(C.byref(v),C.byref(data),0,1,0x100)==0);assert v.visited[1]==0
guard(release(C.byref(v),0x201)==0)
for func,args in ((reset,()),(queue,(C.byref(data),0,1)),(enter,(C.byref(data),0,1,0)),(release,(1,))):
    guard(func(None,*args)==0)
    v.updating=1;guard(func(C.byref(v),*args)==0);v.updating=0
for ticks in (256,0xffffffff):guard(step(C.byref(v),C.byref(data),ticks,0,C.byref(ops))==-1)
v.updating=1;guard(step(C.byref(v),C.byref(data),1,0,C.byref(ops))==-1);v.updating=0
guard(step(None,C.byref(data),1,0,C.byref(ops))==-1)
guard(step(C.byref(v),None,1,0,C.byref(ops))==-1)
guard(step(C.byref(v),C.byref(data),1,0,None)==-1)
v.count=6;guard(step(C.byref(v),C.byref(data),1,0,C.byref(ops))==-1)
v.count=1
for pc in (0,0x7ffffff,0x08800000,0x087ffff0,SYN+1):
    v.pc[0]=pc;guard(step(C.byref(v),C.byref(data),1,0,C.byref(ops))==-1);assert v.updating==0
guard(load(None,rombuf,len(ROM))==0);guard(load(C.byref(Data()),None,len(ROM))==0)
guard(load(C.byref(Data()),rombuf,0x45284b)==0)
# Invalid table/program/list references and incomplete callback tables are
# bounded failures; no host pointer is made from the script address.
for address,value in ((0x0845280c,0),(0x0845280c,0x0845263d),
                      (data.table[0],0x087ffff0),(data.table[0],SYN+1)):
    old=bytes(uc.mem_read(address,4));patch(address,struct.pack('<I',value))
    guard(load(C.byref(Data()),rombuf,len(ROM))==0);patch(address,old)
for op,arg,ptr in ((1,1,0),(1,1,0x087ffffc),(1,1,SPAWNS+1),(2,0,0),(2,257,0)):
    v=VM();v.count=1;v.pc[0]=SYN
    patch(SYN,struct.pack('<5I',op,ptr,arg,0,0))
    guard(step(C.byref(v),C.byref(data),1,0,C.byref(ops))==-1);assert v.updating==0
for name,typ in (('allocate',ALLOC),('animation',ANIM),('insert',INSERT),('particle',PARTICLE),('mover',MOVER)):
    broken=Ops(allocate,animation,insert,particle,mover,None);setattr(broken,name,typ())
    v=VM();guard(step(C.byref(v),C.byref(data),1,0,C.byref(broken))==-1)
@MOVER
def failed_mover(_,op,arg):return -1
v=VM();v.count=1;v.pc[0]=SYN;patch(SYN,struct.pack('<5I',4,0,1,0,0))
broken=Ops(allocate,animation,insert,particle,failed_mover,None)
guard(step(C.byref(v),C.byref(data),1,0,C.byref(broken))==-1)
assert v.pc[0]==SYN and v.updating==0
@MOVER
def reentrant_mover(_,op,arg):
    assert step(C.byref(v),C.byref(data),1,0,C.byref(ops))==-1
    assert queue(C.byref(v),C.byref(data),0,1)==0
    assert reset(C.byref(v))==0
    assert enter(C.byref(v),C.byref(data),0,0,0)==0
    assert release(C.byref(v),1)==0
    return 0
broken.mover=reentrant_mover
guard(step(C.byref(v),C.byref(data),1,0,C.byref(broken))==0)
assert v.pc[0]==SYN+20 and v.updating==0
report['total_rom_comparisons']=sum(report['rom_comparisons'].values())
report['source_sha256']=hashlib.sha256((ROOT/'src/level_script.c').read_bytes()).hexdigest()
report['passed']=True
(ROOT/'analysis/native-level-script-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
