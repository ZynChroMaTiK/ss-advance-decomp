"""Whole04E04 scheduler and083E4 reset; controlled handlers, real pool mutations."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R6,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes=[]
for n in ast.parse((ROOT/'tests/test_world_rom.py').read_text()).body:
    if isinstance(n,ast.ClassDef):nodes.append(n)
    elif isinstance(n,ast.Assign) and any(isinstance(t,ast.Attribute) and t.attr=='_fields_' or isinstance(t,ast.Name) and t.id in ('EP','WP','CONTACT','EXECUTE') for t in n.targets):nodes.append(n)
exec(compile(ast.Module(nodes,type_ignores=[]),'world ctypes','exec'))
class Pool(C.Structure):
    _fields_=[('free_slots',C.c_uint16*100),('active_slots',C.c_uint16*100),('positions',C.c_uint16*100),('active_count',C.c_uint16),('occupied',C.c_uint8*100)]
class Dispatch(C.Structure):
    _fields_=[('pool',C.POINTER(Pool)),('entities',C.POINTER(EP)),('current',EP),('frame_counter',C.c_uint32),('walk_count',C.c_uint32),('path_count',C.c_uint32),('active_tail',C.c_uint16),('auxiliary',C.c_uint16*101),('auxiliary_count',C.c_uint),('updating',C.c_uint)]
HANDLER=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Dispatch),C.c_uint32,C.c_uint)
class Ops(C.Structure):_fields_=[('handler',HANDLER),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/entity_dispatch.c'),str(ROOT/'src/entity_pool.c'),'-o',str(B/'libssa_entity_dispatch.so')],check=True)
lib=C.CDLL(str(B/'libssa_entity_dispatch.so'))
lib.ssa_entity_dispatch_step.argtypes=[C.POINTER(Dispatch),C.POINTER(Ops)]
lib.ssa_entity_dispatch_handler.argtypes=[C.c_uint16];lib.ssa_entity_dispatch_handler.restype=C.c_uint32
lib.ssa_entity_pool_allocate.argtypes=[C.POINTER(Pool),C.POINTER(C.c_uint)]
lib.ssa_entity_pool_release.argtypes=[C.POINTER(Pool),C.c_uint]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
E,G,RESUME,STOP=0x02000000,0x02004000,0x0200f000,0x087ff000
ACTIVE,FREE,AUX,CURRENT,FRAME,WALKS,PATHS,COUNT=0x03004470,0x03004540,0x03004370,0x0300443c,0x0300461c,0x03004358,0x030000b0,0x03004354
def put(a,f,*v):uc.mem_write(a,struct.pack('<'+f,*v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
mapping={}
for typ in range(0x64,0xe4):
    branch=struct.unpack_from('<I',ROM,0x4e68+(typ-0x64)*4)[0]
    if branch==0x0800512e:continue
    hi,lo=struct.unpack_from('<HH',ROM,branch-0x08000000+2)
    assert hi&0xf800==0xf000 and lo&0xf800==0xf800
    off=((hi&0x7ff)<<12)|((lo&0x7ff)<<1)
    if off&(1<<22):off-=1<<23
    mapping[typ]=branch+6+off
assert len(mapping)==28
entities=(Entity*100)();pointers=(EP*100)(*[C.pointer(e) for e in entities]);pool=Pool();d=Dispatch()
d.pool=C.pointer(pool);d.entities=pointers
def native_slot(p):
    if not p:return -1
    return (C.addressof(p.contents)-C.addressof(entities))//C.sizeof(Entity)
def oracle_snapshot():
    raw=bytes(uc.mem_read(E,100*0x74))
    return (tuple(struct.unpack_from('<H',raw,i*0x74+0x3c)[0] for i in range(100)),
        tuple(struct.unpack_from('<H',raw,i*0x74+0x3a)[0] for i in range(100)),
        bytes(uc.mem_read(ACTIVE,200)),bytes(uc.mem_read(FREE,200)),get(COUNT,'H'),
        bytes(uc.mem_read(AUX,202)),(get(CURRENT)-E)//0x74,get(FRAME),get(WALKS),get(PATHS))
def native_snapshot():
    return (tuple(e.type for e in entities),tuple(pool.positions),bytes(pool.active_slots),bytes(pool.free_slots),pool.active_count,
        bytes(d.auxiliary),native_slot(d.current),d.frame_counter,d.walk_count,d.path_count)
rng=random.Random(0x04e04)
oracle=[];actual=[];actions=[];pending=None;queue=[];return_pc=0;used=0;output_count=0;failed=False;fail_at=None
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'scope':'Whole 04E04 scheduler and 083E4 reset with controlled handlers and real pool mutations. Complete type range, dynamic allocation/release, current redirection, next-index auxiliary quirk and stale physical tail. No controller behavior is implied. Original pool 05170/051C8 executes in controlled handlers; only BIOS memory clear is replaced.','rom_comparisons':{},'callback_reentry_guards':0,'native_guards':0,'callback_failures':0,'handler_mapping':{hex(k):hex(v) for k,v in mapping.items()}}
def apply_after(action,native):
    kind,arg=action
    if kind=='batch':
        assert native
        for child in arg:apply_after(child,True)
    elif kind=='alloc':
        if native:
            out=C.c_uint();result=lib.ssa_entity_pool_allocate(C.byref(pool),C.byref(out));assert result>=0
            if result:entities[out.value].type=arg
        else:
            ptr=uc.reg_read(UC_ARM_REG_R0)
            if ptr:put(ptr+0x3c,'H',arg)
    elif kind=='release':
        if native:
            assert lib.ssa_entity_pool_release(C.byref(pool),arg)==1
            entities[arg].type=0
    elif kind=='redirect':
        if native:d.current=pointers[arg]
        else:put(CURRENT,'I',E+arg*0x74)
    elif kind=='clear':
        if native:entities[arg].type=0
        else:put(E+arg*0x74+0x3c,'H',0)
    elif kind=='walks':
        if native:d.walk_count=(d.walk_count+arg)&0xffffffff;d.path_count=(d.path_count+3)&0xffffffff
        else:put(WALKS,'I',(get(WALKS)+arg)&0xffffffff);put(PATHS,'I',(get(PATHS)+3)&0xffffffff)
    elif kind=='frame':
        if native:d.frame_counter=(d.frame_counter+arg)&0xffffffff
        else:put(FRAME,'I',(get(FRAME)+arg)&0xffffffff)
def continue_actions():
    global pending
    while queue:
        action=queue.pop(0)
        if action[0] in ('release','alloc'):
            if action[0]=='release':uc.reg_write(UC_ARM_REG_R0,E+action[1]*0x74)
            pending=action;uc.reg_write(UC_ARM_REG_LR,RESUME|1)
            uc.reg_write(UC_ARM_REG_PC,0x080051c9 if action[0]=='release' else 0x08005171)
            return
        apply_after(action,False)
    uc.reg_write(UC_ARM_REG_PC,return_pc)
def hook(machine,address,size,context):
    global pending,return_pc,used,output_count,failed
    if address==0x080c5320:
        target=machine.reg_read(UC_ARM_REG_R1);assert machine.reg_read(UC_ARM_REG_R2)==0x0500001d
        uc.mem_write(target,bytes(0x74));machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR));return
    if address==RESUME:
        apply_after(pending,False);pending=None;continue_actions();return
    if address==0x08005150:
        output_count=(machine.reg_read(UC_ARM_REG_R6)-AUX)//2;return
    slot=(machine.reg_read(UC_ARM_REG_R0)-E)//0x74
    oracle.append((address,slot,oracle_snapshot()))
    if fail_at is not None and used==fail_at:
        used+=1;failed=True;machine.reg_write(UC_ARM_REG_PC,STOP);return
    action=actions[used] if used<len(actions) else ('none',0);used+=1
    return_pc=machine.reg_read(UC_ARM_REG_LR)
    assert not queue
    queue.extend(action[1] if action[0]=='batch' else [action]);continue_actions()
for a in set(mapping.values())|{RESUME,0x080c5320,0x08005150}:uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
@HANDLER
def callback(context,dispatch,entry,slot):
    actual.append((entry,slot,native_snapshot()))
    assert lib.ssa_entity_dispatch_step(dispatch,C.byref(ops))==-1;report['callback_reentry_guards']+=1
    ix=len(actual)-1
    if fail_at is not None and ix==fail_at:return -1
    apply_after(actions[ix] if ix<len(actions) else ('none',0),True)
    return 0
ops=Ops(callback,None)

def compare(label,types,order=None,action=(),tail=None,frame=None,failure=None,stale=None):
    global actions,used,output_count,pending,failed,fail_at
    n=len(types);assert n<=100
    order=list(range(n)) if order is None else list(order)
    assert len(order)==n and len(set(order))==n and all(0<=s<100 for s in order)
    actions=list(action);used=0;pending=None;failed=False;fail_at=failure
    left=[i for i in range(100) if i not in order]
    raw=bytearray(0x74*100)
    for i in range(100):
        entities[i].type=0;pool.positions[i]=0;pool.occupied[i]=0
        pool.free_slots[i]=rng.randrange(0x10000) if i<n else left[i-n]
        pool.active_slots[i]=rng.randrange(0x10000) if stale is None else stale
    for pos,(typ,slot) in enumerate(zip(types,order)):
        entities[slot].type=typ;pool.positions[slot]=pos;pool.occupied[slot]=1;pool.active_slots[pos]=slot
        struct.pack_into('<HHH',raw,slot*0x74+0x38,slot,pos,typ)
    pool.active_count=n
    uc.mem_write(E,bytes(raw));uc.mem_write(ACTIVE,bytes(pool.active_slots));uc.mem_write(FREE,bytes(pool.free_slots))
    put(COUNT,'H',n);put(0x03004348,'H',100-n);put(0x0300433c,'I',FREE+2*n);put(0x03004454,'I',ACTIVE+2*n)
    put(0x03004644,'I',E);put(0x03004010,'I',G);put(G+0x10,'B',4)
    d.frame_counter=rng.getrandbits(32) if frame is None else frame
    d.walk_count=rng.getrandbits(32);d.path_count=rng.getrandbits(32);d.current=pointers[rng.randrange(100)]
    d.active_tail=rng.randrange(0x10000) if tail is None else tail
    d.auxiliary_count=91;d.updating=0
    for i in range(101):d.auxiliary[i]=rng.randrange(0x10000)
    put(FRAME,'I',d.frame_counter);put(WALKS,'I',d.walk_count);put(PATHS,'I',d.path_count)
    put(CURRENT,'I',E+native_slot(d.current)*0x74);put(ACTIVE+200,'H',d.active_tail);uc.mem_write(AUX,bytes(d.auxiliary))
    oracle.clear();actual.clear();output_count=-1
    uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x08004e05,STOP,count=100000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
    if failed:
        # Native error convention terminates the partial list and restores0.
        output_count=(uc.reg_read(UC_ARM_REG_R6)-AUX)//2
        put(AUX+2*output_count,'H',0);put(CURRENT,'I',E)
    expected=oracle_snapshot()
    assert lib.ssa_entity_dispatch_step(C.byref(d),C.byref(ops))==(-1 if failed else 0)
    assert actual==oracle,(label,'events',actual,oracle)
    assert native_snapshot()==expected,(label,'result',native_snapshot(),expected)
    assert d.auxiliary_count==output_count and not d.updating,(label,d.auxiliary_count,output_count)
    if failed:report['callback_failures']+=1
    else:report['rom_comparisons'][label]=report['rom_comparisons'].get(label,0)+1

# Every possible entity type, including the complete 128-entry original table.
for typ in range(65536):
    assert lib.ssa_entity_dispatch_handler(typ)==mapping.get(typ,0)
    compare('all_u16_types',[typ],order=[typ%100])
print('PASS every 16-bit entity type',flush=True)
for n in range(101):
    order=rng.sample(range(100),n)
    for tail in (0,1,99,100,0x7fff,0xffff):
        compare('capacities_and_stale_tail',[0xcd if i%3 else 0 for i in range(n)],order,tail=tail,frame=0xffffffff,stale=0xffff)
for n in range(1,101):
    for position in range(n):
        order=rng.sample(range(100),n)
        compare('swap_release',[0xcd]*n,order,[('none',0)]*position+[('release',order[position])])
for _ in range(1024):
    n=rng.randrange(1,100);order=rng.sample(range(100),n)
    mode=rng.randrange(6)
    if mode==0:action=[('alloc',rng.choice(list(mapping)))]*min(100-n,rng.randrange(1,6))
    elif mode==1:action=[('release',order[-1])]
    elif mode==2:action=[('redirect',order[-1])]
    elif mode==3:action=[('clear',order[0])]
    elif mode==4:action=[('walks',rng.getrandbits(32)),('frame',0xffffffff)]
    else:action=[('release',order[0]),('alloc',0xcd),('alloc',0xe2)]
    compare('dynamic_handlers',[rng.choice(list(mapping)) for i in range(n)],order,action)
# Appended actors execute this frame. Swap release skips the actor moved behind
# the cursor; final stale active slots remain visible in auxiliary entries.
compare('explicit_append',[0xcd],action=[('alloc',0xe2),('alloc',0xe3),('walks',7)])
compare('explicit_skip',[0xcd]*4,action=[('release',0)])
compare('explicit_redirect',[0xcd,0],action=[('redirect',1)])
compare('explicit_full',[0xcd]*100,action=[('alloc',0xcd)],tail=0xffff)
for n in range(1,101):
    order=rng.sample(range(100),n)
    compare('child_then_release',[0xcd]*n,order,[('batch',[('alloc',0xe2),('release',order[0])])])
    compare('release_then_reuse',[0xcd]*n,order,[('batch',[('release',order[0]),('alloc',0xe2)])])
for ix in (0,1,2,3):compare('failure',[0xcd]*4,action=[('walks',5)]*4,failure=ix)

def guard(fn):
    before=bytes(d);assert fn()==-1 and bytes(d)==before;report['native_guards']+=1
guard(lambda:lib.ssa_entity_dispatch_step(None,C.byref(ops)))
guard(lambda:lib.ssa_entity_dispatch_step(C.byref(d),None))
d.updating=1;guard(lambda:lib.ssa_entity_dispatch_step(C.byref(d),C.byref(ops)));d.updating=0
old=pool.active_count;pool.active_count=101;guard(lambda:lib.ssa_entity_dispatch_step(C.byref(d),C.byref(ops)));pool.active_count=old
old=pool.active_slots[0];pool.active_slots[0]=100;guard(lambda:lib.ssa_entity_dispatch_step(C.byref(d),C.byref(ops)));pool.active_slots[0]=old
save=pointers[99];pointers[99]=EP();guard(lambda:lib.ssa_entity_dispatch_step(C.byref(d),C.byref(ops)));pointers[99]=save
report['total_rom_comparisons']=sum(report['rom_comparisons'].values())
report['source_sha256']=hashlib.sha256((ROOT/'src/entity_dispatch.c').read_bytes()).hexdigest()
(ROOT/'analysis/native-entity-dispatch-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
