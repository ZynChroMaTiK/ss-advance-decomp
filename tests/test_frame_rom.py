"""Whole 0800244C, with all twelve call boundaries controlled identically."""
from pathlib import Path
import ctypes as C, hashlib, json, random, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
class Game(C.Structure):
    _fields_=[('counter',C.c_uint32),('flags',C.c_uint16),('ticks',C.c_uint8),('elapsed',C.c_uint32)]
P16=C.POINTER(C.c_uint16)
class Frame(C.Structure):
    _fields_=[('game',C.POINTER(Game)),('processed_input',C.c_uint32),('input',C.c_uint32),('local_index',C.c_uint16),('player_flags',P16*4),('presentation_latch',C.c_uint16),('updating',C.c_uint)]
CALL=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Frame),C.c_int)
class Ops(C.Structure):_fields_=[('stage',CALL),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/frame.c'),'-o',str(B/'libssa_frame.so')],check=True)
lib=C.CDLL(str(B/'libssa_frame.so'));lib.ssa_frame_step.argtypes=[C.POINTER(Frame),C.POINTER(Ops)]
STAGES=[0x08003028,0x08005898,0x080021e4,0x08004e04,0x0801c13c,0x0801c6dc,0x0801b854,0x08000908,0x0800f85c,0x0800e460,0x0800dbdc,0x0800fce4]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
G,P,L,STOP=0x02000000,0x02001000,0x02002000,0x087ff000
def put(a,f,v):uc.mem_write(a,struct.pack('<'+f,v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
games=(Game*2)();flags=(C.c_uint16*4)();frame=Frame()
def game_index():return (C.addressof(frame.game.contents)-C.addressof(games))//C.sizeof(Game)
def snapshot(native):
    if native:return (game_index(),tuple((g.counter,g.flags,g.ticks,g.elapsed) for g in games),frame.processed_input,frame.input,frame.local_index,tuple(flags),frame.presentation_latch)
    return ((get(0x03004010)-G)//0x100,tuple((get(G+i*0x100),get(G+i*0x100+4,'H'),get(G+i*0x100+7,'B'),get(G+i*0x100+12)) for i in range(2)),get(0x030042f4),get(0x03003f84),get(0x0300464e,'H'),tuple(get(P+i*256+0xbc,'H') for i in range(4)),get(L+0xb02,'H'))
def change(action,native):
    key,value=action
    if key=='game':
        if native:frame.game=C.pointer(games[value])
        else:put(0x03004010,'I',G+value*0x100)
    elif key in ('counter','flags','ticks','elapsed'):
        if native:setattr(frame.game.contents,key,value)
        else:
            offset,fmt={'counter':(0,'I'),'flags':(4,'H'),'ticks':(7,'B'),'elapsed':(12,'I')}[key]
            put(get(0x03004010)+offset,fmt,value)
    elif key.startswith('player'):
        i=int(key[-1])
        if native:flags[i]=value
        else:put(P+i*256+0xbc,'H',value)
    else:
        if native:setattr(frame,key,value)
        else:
            a,fmt={'processed_input':(0x030042f4,'I'),'input':(0x03003f84,'I'),'local_index':(0x0300464e,'H'),'presentation_latch':(L+0xb02,'H')}[key]
            put(a,fmt,value)
oracle=[];actual=[];actions={};fail_at=None
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':{},'callback_reentry_guards':0,'native_guards':0,'callback_failure_cases':0}
def hook(machine,address,size,context):
    if address not in STAGES:return
    oracle.append((address,snapshot(False)))
    for action in actions.get(address,[]):change(action,False)
    if address==fail_at:machine.emu_stop()
    else:machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
@CALL
def callback(context,p,stage):
    actual.append((stage,snapshot(True)))
    before=bytes(frame)
    assert lib.ssa_frame_step(p,C.byref(ops))==-1 and bytes(frame)==before
    report['callback_reentry_guards']+=1
    for action in actions.get(stage,[]):change(action,True)
    return -1 if stage==fail_at else 0
ops=Ops(callback,None)
rng=random.Random(0x244c)
def case(category,state=None,script=None,failure=None):
    global actions,fail_at
    actions=script or {};fail_at=failure
    if state is None:
        state=(rng.randrange(2),[(rng.getrandbits(32),rng.getrandbits(16),rng.randrange(256),rng.getrandbits(32)) for _ in range(2)],rng.getrandbits(32),rng.getrandbits(32),rng.randrange(4),[rng.getrandbits(16) for _ in range(4)],rng.getrandbits(16))
    index,gs,processed,inp,local,pflags,latch=state
    for i,values in enumerate(gs):
        games[i]=Game(*values)
        for off,fmt,value in zip((0,4,7,12),('I','H','B','I'),values):put(G+i*0x100+off,fmt,value)
    frame.game=C.pointer(games[index]);frame.processed_input=processed;frame.input=inp;frame.local_index=local;frame.presentation_latch=latch;frame.updating=0
    for i,v in enumerate(pflags):flags[i]=v;frame.player_flags[i]=C.cast(C.byref(flags,2*i),P16);put(P+i*256+0xbc,'H',v)
    put(0x03004010,'I',G+index*0x100);put(0x03004648,'I',P);put(0x03003f00,'I',L)
    put(0x030042f4,'I',processed);put(0x03003f84,'I',inp);put(0x0300464e,'H',local);put(L+0xb02,'H',latch)
    oracle.clear();actual.clear();uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x0800244d,STOP,count=1000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP or failure is not None
    expected=snapshot(False)
    result=lib.ssa_frame_step(C.byref(frame),C.byref(ops))
    assert result==(-1 if failure else 0),(category,result)
    assert actual==oracle,(category,state,script,actual,oracle)
    assert snapshot(True)==expected,(category,state,script,snapshot(True),expected)
    assert frame.updating==0
    report['rom_comparisons'][category]=report['rom_comparisons'].get(category,0)+1
    if failure:report['callback_failure_cases']+=1

# Every tick byte across time clamp/wrap edges and both independent outer gates.
for tick in range(256):
    for elapsed in (0,0x57e03,0x57e04,0x57e05,0xffffff00,0xffffffff):
        for pause in range(2):
            for hide in (0,8):
                case('tick_time_pause_render_edges',(0,[(0xffffffff,pause,tick,elapsed),(73,0,7,123)],0xaabbccdd,0x10203040,2,[0,0,hide,0],99))
for bit in range(16):
    for stage in STAGES:
        case('callback_bit_mutations',(0,[(99,0,1,99),(15,0,3,15)],1,2,0,[0]*4,47),{stage:[('flags',1<<bit),('player0',1<<bit)]})
# Pointer replacements after poll and control change the correct counter/time owner.
for stage in STAGES:
    for local in range(4):
        case('game_and_display_rebinding',(0,[(0xffffffff,0,1,100),(55,0,255,0xffffffff)],1,2,0,[0,8,0,8],47),{stage:[('game',1),('local_index',local),('processed_input',0x99887766)]})
for _ in range(4096):
    script={}
    for stage in rng.sample(STAGES,rng.randrange(7)):
        script[stage]=[('game',rng.randrange(2)),('flags',rng.getrandbits(16)),('elapsed',rng.getrandbits(32)),('counter',rng.getrandbits(32)),('ticks',rng.randrange(256)),('processed_input',rng.getrandbits(32)),('input',rng.getrandbits(32)),('local_index',rng.randrange(4)),('player'+str(rng.randrange(4)),rng.getrandbits(16)),('presentation_latch',rng.getrandbits(16))]
    case('random_stage_mutations',script=script)
for stage in STAGES:
    case('failure_prefixes',(0,[(99,0,1,99),(15,0,3,15)],1,2,0,[0]*4,47),{stage:[('counter',999),('presentation_latch',71)]},stage)

# Invalid bindings at actual use sites, including repair before that site.
case('repair_before_read',(0,[(1,0,1,2),(3,0,1,4)],1,2,99,[0]*4,47),{0x0800e460:[('local_index',2)]})
for invalid in ('null_frame','null_ops','null_callback','busy','null_game','bad_local','null_player'):
    case('guard_setup',(0,[(1,0,1,2),(3,0,1,4)],1,2,0,[0]*4,47))
    actions={};fail_at=None
    old=bytes(frame)
    fp=C.byref(frame);op=C.byref(ops)
    if invalid=='null_frame':fp=None
    elif invalid=='null_ops':op=None
    elif invalid=='null_callback':empty=Ops();op=C.byref(empty)
    elif invalid=='busy':frame.updating=1
    elif invalid=='null_game':frame.game=C.POINTER(Game)()
    elif invalid=='bad_local':frame.local_index=4
    elif invalid=='null_player':frame.player_flags[0]=P16()
    # A reduced callback handles deliberately invalid projections without snapshot.
    @CALL
    def guard_callback(ctx,p,stage):return 0
    guard_ops=Ops(guard_callback,None)
    if invalid in ('null_game','bad_local','null_player'):op=C.byref(guard_ops)
    assert lib.ssa_frame_step(fp,op)==-1
    if invalid!='busy':assert frame.updating==0
    report['native_guards']+=1
report['rom_comparisons_total']=sum(report['rom_comparisons'].values())
report['source_sha256']=hashlib.sha256((ROOT/'src/frame.c').read_bytes()).hexdigest()
report['scope']='Complete 0244C control flow, all twelve callees controlled identically. Stage internals are not validated here. Counter/input/time projections, pointer replacement, pause/render gates, all tick values, ordered callbacks and failure prefixes. UBSan.'
(ROOT/'analysis/native-frame-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
