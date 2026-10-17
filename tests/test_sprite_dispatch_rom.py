"""Whole F85C/101C4/101D0, controlled handlers and post-submit observation."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
node=next(n for n in ast.parse((ROOT/'tests/test_weapon_animation_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name=='Object')
exec(compile(ast.Module([node],type_ignores=[]),'object projection','exec'))
OP=C.POINTER(Object)
class Dispatch(C.Structure):
    _fields_=[('objects',OP*84),('active',C.c_uint32*84),('active_count',C.c_uint32),('counter',C.c_uint32),('state',C.c_uint32),('current_index',C.c_uint32),('current',OP),('submitted_count',C.c_uint8),('submitted',C.c_uint8*84),('updating',C.c_uint)]
HANDLER=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Dispatch),C.c_uint32,C.c_uint)
SUBMIT=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Dispatch),OP)
class Ops(C.Structure):_fields_=[('handler',HANDLER),('submit',SUBMIT),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/sprite_dispatch.c'),'-o',str(B/'libssa_sprite_dispatch.so')],check=True)
lib=C.CDLL(str(B/'libssa_sprite_dispatch.so'))
lib.ssa_sprite_dispatch_step.argtypes=[C.POINTER(Dispatch),C.POINTER(Ops)]
lib.ssa_sprite_dispatch_handler.argtypes=[C.c_uint16];lib.ssa_sprite_dispatch_handler.restype=C.c_uint32
mapping={}
for typ in range(1,19):
    branch=struct.unpack_from('<I',ROM,0xf8bc+4*(typ-1))[0]
    if branch==0x0800f968:continue
    hi,lo=struct.unpack_from('<HH',ROM,branch-0x08000000)
    off=((hi&0x7ff)<<12)|((lo&0x7ff)<<1)
    if off&(1<<22):off-=1<<23
    mapping[typ]=branch+4+off
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
E,A,STOP=0x02000000,0x02002000,0x087ff000
def put(a,f,*v):uc.mem_write(a,struct.pack('<'+f,*v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
objects=(Object*84)();d=Dispatch()
for i in range(84):d.objects[i]=C.pointer(objects[i])
def slot(p):return (C.addressof(p.contents)-C.addressof(objects))//48
def snapshot(native):
    if native:return (bytes(objects),bytes(d.active),d.active_count,d.counter,d.state,d.current_index,slot(d.current),d.submitted_count,bytes(d.submitted))
    return (bytes(uc.mem_read(E,84*48)),bytes(uc.mem_read(A,84*4)),get(0x030055c4),get(0x030055b0),get(0x030055a4),get(0x030055a8),(get(0x030055a0)-E)//48,get(0x03005674,'B'),bytes(uc.mem_read(0x03005620,84)))
def action(kind,arg,native):
    if kind=='release':
        n=d.active_count if native else get(0x030055c4)
        current=slot(d.current) if native else (get(0x030055a0)-E)//48
        indices=list(d.active) if native else list(struct.unpack('<84I',uc.mem_read(A,336)))
        pos=indices[:n].index(current);indices[pos]=indices[n-1]
        if native:d.active_count=n-1;d.active[pos]=indices[pos];objects[current].type=0
        else:put(0x030055c4,'I',n-1);put(A+4*pos,'I',indices[pos]);put(E+48*current+4,'H',0)
    elif kind=='append':
        n=d.active_count if native else get(0x030055c4)
        if n>=84:return
        indices=list(d.active)[:n] if native else list(struct.unpack('<'+str(n)+'I',uc.mem_read(A,4*n)))
        new=next(i for i in range(84) if i not in indices)
        if native:d.active[n]=new;d.active_count=n+1;objects[new].type=arg
        else:put(A+4*n,'I',new);put(0x030055c4,'I',n+1);put(E+new*48+4,'H',arg)
    elif kind=='clear':
        current=slot(d.current) if native else (get(0x030055a0)-E)//48
        if native:objects[current].type=0
        else:put(E+48*current+4,'H',0)
    elif kind=='redirect':
        if native:d.current=d.objects[arg]
        else:put(0x030055a0,'I',E+arg*48)
    elif kind=='state':
        if native:d.state=arg;d.current_index=arg;d.counter=(d.counter+arg)&0xffffffff
        else:put(0x030055a4,'I',arg);put(0x030055a8,'I',arg);put(0x030055b0,'I',(get(0x030055b0)+arg)&0xffffffff)
    elif kind=='submit':
        if native:d.submitted_count=(d.submitted_count+1)&255
        else:put(0x03005674,'B',(get(0x03005674,'B')+1)&255)
oracle=[];actual=[];script={};fail_at=None;calls=0
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':{},'callback_reentry_guards':0,'native_guards':0,'callback_failures':0,'handler_mapping':{hex(k):hex(v) for k,v in mapping.items()}}
def hook(machine,address,size,ctx):
    global calls
    #101D0 executes its real stores; observation is immediately before its pop.
    if address not in mapping.values() and address!=0x080101e6:return
    post=address==0x080101e6
    if post:address=0x080101d0
    current=(get(0x030055a0)-E)//48
    oracle.append((address,current,snapshot(False)))
    for kind,arg in script.get(calls,[]):action(kind,arg,False)
    fail=calls==fail_at;calls+=1
    if fail:machine.emu_stop()
    elif not post:machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
def native_call(address,current):
    global calls
    actual.append((address,current,snapshot(True)))
    before=bytes(d)
    assert lib.ssa_sprite_dispatch_step(C.byref(d),C.byref(ops))==-1 and before==bytes(d)
    report['callback_reentry_guards']+=1
    for kind,arg in script.get(calls,[]):action(kind,arg,True)
    fail=calls==fail_at;calls+=1
    return -1 if fail else 0
@HANDLER
def handler(ctx,p,address,current):return native_call(address,current)
@SUBMIT
def submit(ctx,p,obj):return native_call(0x080101d0,slot(obj))
ops=Ops(handler,submit,None);rng=random.Random(0xf85c)
def case(category,types,actions=None,failure=None,order=None):
    global script,fail_at,calls
    script=actions or {};fail_at=failure
    raw=bytearray(rng.randbytes(84*48))
    for i in range(84):struct.pack_into('<H',raw,i*48+4,types[i] if i<len(types) else 0)
    C.memmove(objects,bytes(raw),len(raw));uc.mem_write(E,bytes(raw))
    active=list(range(84)) if order is None else list(order)
    d.active[:]=active;put(A,'84I',*active);d.active_count=len(types)
    d.counter=0xffffffff;d.state=0x76543210;d.current_index=0x12345678;d.current=d.objects[83];d.submitted_count=0xfe;d.updating=0
    tail=rng.randbytes(84);d.submitted[:]=tail;uc.mem_write(0x03005620,tail)
    put(0x0300559c,'I',E);put(0x030055a0,'I',E+83*48);put(0x030055a4,'I',d.state);put(0x030055a8,'I',d.current_index)
    put(0x030055b0,'I',d.counter);put(0x030055c0,'II',A,len(types));put(0x03005674,'B',d.submitted_count)
    calls=0;oracle.clear();actual.clear();uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x0800f85d,STOP,count=30000)
    if failure is not None:put(0x030055a4,'I',3);put(0x030055a0,'I',E)
    else:assert uc.reg_read(UC_ARM_REG_PC)==STOP
    expected=snapshot(False);calls=0
    result=lib.ssa_sprite_dispatch_step(C.byref(d),C.byref(ops))
    assert result==(-1 if failure is not None else 0),(category,result)
    assert actual==oracle,(category,types,actions,actual,oracle)
    assert snapshot(True)==expected,(category,types,actions,snapshot(True),expected)
    assert not d.updating
    report['rom_comparisons'][category]=report['rom_comparisons'].get(category,0)+1
    if failure is not None:report['callback_failures']+=1
for typ in range(65536):
    assert lib.ssa_sprite_dispatch_handler(typ)==mapping.get(typ,0)
    case('all_types',[typ])
for count in range(85):
    case('capacities',[2]*count)
    case('dead_cursor_repeats',[0]*count)
for count in range(1,85):
    for pos in range(count):case('swap_release',[2]*count,{pos*2:[('release',0)]})
for stage in range(8):
    case('append_release_redirect',[2]*4,{stage:[('append',2)]})
    case('append_release_redirect',[2]*4,{stage:[('clear',0)]})
    case('append_release_redirect',[2]*4,{stage:[('redirect',3),('state',11)]})
case('submit_clear_keeps_cursor',[2]*4,{1:[('clear',0)]})
for _ in range(1024):
    count=rng.randrange(1,85)
    # Only choose actions valid even after prior releases; each frame one mutation.
    pos=rng.randrange(count);kind=rng.choice(('append','release','clear','redirect','state'))
    arg=2 if kind=='append' else rng.randrange(count) if kind=='redirect' else rng.getrandbits(32) if kind=='state' else 0
    case('random_mutation', [rng.choice([2,3,4,0x1234]) for _ in range(count)],{pos*2:[(kind,arg)]})
for at in range(8):case('failure_prefixes',[2]*4,{at:[('state',99)]},at)
# Entry guards leave all state unchanged.
for name in ('null','busy','ops','handler','count','object'):
    case('guard_setup',[]);fp=C.byref(d);op=C.byref(ops);saved=None
    if name=='null':fp=None
    elif name=='busy':d.updating=1
    elif name=='ops':op=None
    elif name in ('handler','submit'):
        bad=Ops(handler,submit,None);setattr(bad,name,HANDLER() if name=='handler' else SUBMIT());op=C.byref(bad)
    elif name=='count':d.active_count=85
    elif name=='object':saved=d.objects[83];d.objects[83]=OP()
    before=bytes(d);assert lib.ssa_sprite_dispatch_step(fp,op)==-1 and bytes(d)==before
    if name=='object':d.objects[83]=C.pointer(objects[83])
    report['native_guards']+=1
for name in ('bad_slot','callback_count','callback_current','queue_overflow'):
    case('guard_setup',[])
    d.active_count=1;d.active[0]=0;objects[0].type=2
    @HANDLER
    def invalid_handler(ctx,p,address,current):
        if name=='callback_count':d.active_count=85
        elif name=='callback_current':d.current=OP()
        elif name=='queue_overflow':d.submitted_count=84
        return 0
    guarded=Ops(invalid_handler,SUBMIT(),None)
    if name=='bad_slot':d.active[0]=84
    assert lib.ssa_sprite_dispatch_step(C.byref(d),C.byref(guarded))==-1
    assert d.state==3 and slot(d.current)==0 and d.updating==0
    report['native_guards']+=1
case('guard_setup',[])
d.active_count=84
for i in range(84):d.active[i]=i;objects[i].type=1;objects[i].active_index=0x100+i
observer_free=Ops(handler,SUBMIT(),None)
assert lib.ssa_sprite_dispatch_step(C.byref(d),C.byref(observer_free))==0
assert d.submitted_count==84 and bytes(d.submitted)==bytes(range(84))
report['optional_observer_full_queue']=1
report['rom_comparisons_total']=sum(report['rom_comparisons'].values())
report['source_sha256']=hashlib.sha256((ROOT/'src/sprite_dispatch.c').read_bytes()).hexdigest()
report['scope']='Whole F85C plus actual101C4 reset and101D0 queue append, controlled handlers and post-submit observers, all65536types, allcapacity/releasepositions, append/deletion/currentredirection, alive-dependentcursor, state/counter changes. Full84queuebytes/lowbyte active_index truncation. Nativecallbackfailurecleanup and guards. UBSan.'
(ROOT/'analysis/native-sprite-dispatch-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
