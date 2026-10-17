"""Whole021E4 with both UI callees controlled and compared at every boundary."""
from pathlib import Path
import ctypes as C, hashlib, json, random, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
class Mode(C.Structure):
    _fields_=[('flags',C.POINTER(C.c_uint16)),('input_state',C.POINTER(C.c_uint16)),('menu_timer',C.POINTER(C.c_uint16)),('player_count',C.POINTER(C.c_uint8)),('selection',C.POINTER(C.c_uint8)),('inputs',C.POINTER(C.c_uint32)*4),('updating',C.c_uint)]
CALL=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Mode),C.c_int,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32)
class Ops(C.Structure):_fields_=[('event',CALL),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/mode_control.c'),'-o',str(B/'libssa_mode_control.so')],check=True)
lib=C.CDLL(str(B/'libssa_mode_control.so'));lib.ssa_mode_control_step.argtypes=[C.POINTER(Mode),C.POINTER(Ops)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for a,n in ((0x08000000,len(ROM)),(0x02000000,0x10000),(0x03000000,0x10000)):uc.mem_map(a,n)
uc.mem_write(0x08000000,ROM)
G,P,STOP=0x02000000,0x02001000,0x087ff000
def put(a,f,v):uc.mem_write(a,struct.pack('<'+f,v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
flags,state,timer=C.c_uint16(),C.c_uint16(),C.c_uint16()
count,selection=C.c_uint8(),C.c_uint8()
inputs=(C.c_uint32*4)()
m=Mode(C.pointer(flags),C.pointer(state),C.pointer(timer),C.pointer(count),C.pointer(selection))
for i in range(4):m.inputs[i]=C.cast(C.byref(inputs,4*i),C.POINTER(C.c_uint32))
actual=[];oracle=[];actions={};failure=None
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':{},'callback_reentry_checks':0,'callback_failure_cases':0,'guards':0}
def snapshot(native):
    if native:return flags.value,state.value,timer.value,count.value,selection.value,tuple(inputs)
    return get(G+4,'H'),get(0x03004304,'H'),get(G+0x76,'H'),get(G+0x10,'B'),get(G+0x72,'B'),tuple(get(P+i*256+0xa4) for i in range(4))
def change(action,native):
    field,value=action
    if field.startswith('input') and field!='input_state':
        i=int(field[-1])
        if native:inputs[i]=value
        else:put(P+i*256+0xa4,'I',value)
    elif native:{'flags':flags,'input_state':state,'timer':timer,'count':count,'selection':selection}[field].value=value
    else:
        a,f={'flags':(G+4,'H'),'input_state':(0x03004304,'H'),'timer':(G+0x76,'H'),'count':(G+0x10,'B'),'selection':(G+0x72,'B')}[field]
        put(a,f,value)
def hook(machine,address,size,context):
    if address not in (0x08008eb0,0x0800e914):return
    args=tuple(machine.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3))
    if address==0x08008eb0:args=(args[0],0,0,0)
    index=len(oracle);oracle.append((address,args,snapshot(False)))
    for action in actions.get(index,[]):change(action,False)
    if index==failure:machine.emu_stop()
    else:machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
@CALL
def callback(context,p,event,a,b,c,d):
    index=len(actual);actual.append((event,(a,b,c,d),snapshot(True)))
    before=bytes(m)
    assert lib.ssa_mode_control_step(p,C.byref(ops))==-1 and bytes(m)==before
    report['callback_reentry_checks']+=1
    for action in actions.get(index,[]):change(action,True)
    return -1 if index==failure else 0
ops=Ops(callback,None)
def case(category,initial,script=None,fail=None):
    global actions,failure
    actions=script or {};failure=fail
    fv,sv,tv,cv,mv,iv=initial
    flags.value=fv;state.value=sv;timer.value=tv;count.value=cv;selection.value=mv;m.updating=0
    for i,v in enumerate(iv):inputs[i]=v;put(P+i*256+0xa4,'I',v)
    for a,f,v in ((G+4,'H',fv),(0x03004304,'H',sv),(G+0x76,'H',tv),(G+0x10,'B',cv),(G+0x72,'B',mv),(0x03004010,'I',G),(0x03004648,'I',P)):put(a,f,v)
    actual.clear();oracle.clear();uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x080021e5,STOP,count=10000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP or fail is not None
    expected=snapshot(False)
    result=lib.ssa_mode_control_step(C.byref(m),C.byref(ops))
    assert result==(-1 if fail is not None else 0),(category,result)
    assert actual==oracle,(category,initial,script,actual,oracle)
    assert snapshot(True)==expected,(category,initial,script,snapshot(True),expected)
    assert m.updating==0
    report['rom_comparisons'][category]=report['rom_comparisons'].get(category,0)+1
    if fail is not None:report['callback_failure_cases']+=1

# Exhaust every relevant flag/selection/button combination. Low held bits never
# act as pressed bits here. Includes reserved menu selections and all four slots.
bits=(0x10000,0x80000,0x400000,0x800000)
for f in range(16):
    fv=(f&1)|((f&2)<<7)|((f&4)<<8)|((f&8)<<8)|0x40
    for mv in (0,1,2,3,4,255):
        for buttons in range(16):
            word=sum(b for i,b in enumerate(bits) if buttons&(1<<i))|0xffff
            for cv in range(5):case('flags_buttons_selection_count',(fv,3,0xffff,cv,mv,[word]*4))
rng=random.Random(0x21e4)
for _ in range(5000):case('random_reserved_bits',(rng.getrandbits(16),rng.getrandbits(16),rng.getrandbits(16),rng.randrange(5),rng.randrange(256),[rng.getrandbits(32) for _ in range(4)]))
# Scalar mutation at each external call must influence subsequent original reads.
base=(1,3,17,4,2,[0xc90000]*4)
for index in range(16):
    for field,values in (('flags',(0,1,0x100,0x801,0xffff)),('selection',(0,1,2,3,255)),('count',(0,1,2,4)),('input0',(0,0xc90000)),('input3',(0,0xc90000)),('input_state',(0,5,65535)),('timer',(0,65535))):
        for value in values:case('live_callback_mutations',base,{index:[(field,value)]})
# Fail only at boundaries that are actually reached; compare prefix state.
for initial in ((0,3,17,4,0,[0x80000]*4),base,(0x101,3,17,4,3,[0x90000]*4)):
    case('failure_baseline',initial)
    for index in range(len(oracle)):case('callback_failure_prefix',initial,fail=index)
# Invalid bindings and nested update leave bytes unchanged.
def guard(pointer,op):
    before=bytes(m);saved=snapshot(True)
    assert lib.ssa_mode_control_step(pointer,op)==-1
    assert bytes(m)==before and snapshot(True)==saved
    report['guards']+=1
guard(None,C.byref(ops));guard(C.byref(m),None)
empty=Ops();guard(C.byref(m),C.byref(empty))
for field in ('flags','input_state','menu_timer','player_count','selection'):
    saved=getattr(m,field);address=C.cast(saved,C.c_void_p).value
    setattr(m,field,type(saved)());guard(C.byref(m),C.byref(ops))
    setattr(m,field,C.cast(address,type(saved)))
m.updating=1;guard(C.byref(m),C.byref(ops));m.updating=0
count.value=5;guard(C.byref(m),C.byref(ops));count.value=1
address=C.cast(m.inputs[0],C.c_void_p).value;m.inputs[0]=C.POINTER(C.c_uint32)()
guard(C.byref(m),C.byref(ops));m.inputs[0]=C.cast(address,C.POINTER(C.c_uint32))
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('src/mode_control.c','include/ssa/mode_control.h','tests/test_mode_control_rom.py')}
(ROOT/'analysis/mode-control-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
