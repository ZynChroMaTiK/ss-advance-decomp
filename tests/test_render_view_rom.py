"""Whole02B88 with real94E8 and explicit remaining display boundaries."""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess,os
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
class Tables(C.Structure):_fields_=[('sine',C.c_uint16*16384),('direction',C.c_uint16*32769)]
class Camera(C.Structure):_fields_=[('x',C.c_int32),('y',C.c_int32),('z',C.c_int32),('heading',C.c_uint16),('sector',C.c_int16),('sine',C.c_int32),('cosine',C.c_int32)]
class Player(C.Structure):_fields_=[('flags',C.c_uint16),('keys',C.c_uint32),('zoom',C.c_uint32)]
class View(C.Structure):_fields_=[('players',Player*4),('camera',Camera),('math',C.POINTER(Tables)),('local_index',C.c_uint16),('overlay',C.c_uint16),('ticks',C.c_uint8),('updating',C.c_uint)]
CALL=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(View),C.c_int,C.c_uint32)
class Ops(C.Structure):_fields_=[('event',CALL),('context',C.c_void_p)]
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/render_view.c'),str(ROOT/'src/movement.c'),str(ROOT/'src/geometry.c'),str(ROOT/'src/physics.c'),str(ROOT/'src/map_assets.c'),'-o',str(B/'libssa_render_view.so')],check=True)
lib=C.CDLL(str(B/'libssa_render_view.so'));lib.ssa_render_view_step.argtypes=[C.POINTER(View),C.POINTER(Ops)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Tables),C.c_void_p,C.c_size_t]
tables=Tables();assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))
v=View();v.math=C.pointer(tables)
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for a,n in ((0x08000000,len(ROM)),(0x02000000,0x10000),(0x03000000,0x10000)):uc.mem_map(a,n)
uc.mem_write(0x08000000,ROM)
G,P,CAM,STOP=0x02000000,0x02001000,0x03004020,0x087ff000
def put(a,f,n):uc.mem_write(a,struct.pack('<'+f,n))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
def snapshot(native):
    if native:return tuple((p.flags,p.keys,p.zoom) for p in v.players),tuple(getattr(v.camera,f) for f in ('x','y','z','heading','sector','sine','cosine')),v.local_index,v.overlay,v.ticks
    return tuple((get(P+i*256+0xbc,'H'),get(P+i*256+0xa4),get(P+i*256+0xa8)) for i in range(4)),(get(CAM,'i'),get(CAM+4,'i'),get(CAM+8,'i'),get(CAM+12,'H'),get(CAM+24,'h'),get(CAM+16,'i'),get(CAM+20,'i')),get(0x0300464e,'H'),get(0x03004180,'H'),get(G+7,'B')
actual=[];oracle=[];actions={};failure=None
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_rom_comparisons':{},'callback_reentry_checks':0,'callback_failures':0,'guards':0}
def change(action,native):
    key,value=action
    if key.startswith('player'):
        _,index,field=key.split('_');i=int(index)
        if native:setattr(v.players[i],field,value)
        else:
            off,fmt={'flags':(0xbc,'H'),'keys':(0xa4,'I'),'zoom':(0xa8,'I')}[field];put(P+256*i+off,fmt,value)
    elif key.startswith('camera_'):
        field=key[7:]
        if native:setattr(v.camera,field,value)
        else:
            off,fmt={'x':(0,'i'),'y':(4,'i'),'z':(8,'i'),'heading':(12,'H'),'sector':(24,'h'),'sine':(16,'i'),'cosine':(20,'i')}[field];put(CAM+off,fmt,value)
    elif native:setattr(v,key,value)
    else:
        a,f={'local_index':(0x0300464e,'H'),'overlay':(0x03004180,'H'),'ticks':(G+7,'B')}[key];put(a,f,value)
def hook(machine,address,size,context):
    if address not in (0x08007c94,0x08008420,0x0800b6b0,0x08002ca0):return
    argument=machine.reg_read(UC_ARM_REG_R0) if address in (0x08007c94,0x08008420) else 0
    index=len(oracle);oracle.append((address,argument,snapshot(False)))
    for action in actions.get(index,[]):change(action,False)
    if index==failure:machine.emu_stop()
    else:machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
@CALL
def callback(context,p,event,argument):
    index=len(actual);actual.append((event,argument,snapshot(True)))
    saved=bytes(v);assert lib.ssa_render_view_step(p,C.byref(ops))==-1 and bytes(v)==saved
    report['callback_reentry_checks']+=1
    for action in actions.get(index,[]):change(action,True)
    return -1 if index==failure else 0
ops=Ops(callback,None);rng=random.Random(0x2b88)
def case(category,players=None,camera=None,local=0,overlay=0,ticks=1,script=None,fail=None):
    global actions,failure
    actions=script or {};failure=fail
    players=players or [(0,0,4096)]*4
    camera=camera or (rng.randrange(-0x80000000,0x80000000),rng.randrange(-0x80000000,0x80000000),rng.randrange(-0x80000000,0x80000000),rng.randrange(65536),rng.randrange(-32768,32768),rng.randrange(-0x80000000,0x80000000),rng.randrange(-0x80000000,0x80000000))
    for i,row in enumerate(players):
        v.players[i]=Player(*row)
        for off,f,n in zip((0xbc,0xa4,0xa8),('H','I','I'),row):put(P+256*i+off,f,n)
    v.camera=Camera(*camera)
    for off,f,n in zip((0,4,8,12,24,16,20),('i','i','i','H','h','i','i'),camera):put(CAM+off,f,n)
    v.local_index=local;v.overlay=overlay;v.ticks=ticks;v.updating=0
    for a,f,n in ((0x03004010,'I',G),(0x03004648,'I',P),(0x0300464e,'H',local),(0x03004180,'H',overlay),(G+7,'B',ticks),(0x03004930,'I',0x02002000)):put(a,f,n)
    actual.clear();oracle.clear();uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x08002b89,STOP,count=2000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP or fail is not None
    expected=snapshot(False);result=lib.ssa_render_view_step(C.byref(v),C.byref(ops))
    assert result==(-1 if fail is not None else 0)
    assert actual==oracle,(category,players,camera,script,actual,oracle)
    assert snapshot(True)==expected and not v.updating,(category,snapshot(True),expected)
    report['whole_rom_comparisons'][category]=report['whole_rom_comparisons'].get(category,0)+1
    if fail is not None:report['callback_failures']+=1
for heading in range(65536):case('all_negative_yaw',camera=(0,0,0,heading,0,123,456),overlay=heading&1)
for tick in range(256):
    for zoom in (0,4095,4096,65535,65536,0xfffffffe,0xffffffff,0x80000000):
        for key in range(4):
            for active in (0,4):
                case('zoom_wrap_clamp_and_gates',[(8|active,key,zoom),(12,3,zoom),(4,key,zoom),(0xffff,key,zoom)],local=tick&3,overlay=1,ticks=tick)
for _ in range(2048):case('random_reserved_flags',[(rng.getrandbits(16),rng.getrandbits(32),rng.getrandbits(32)) for _ in range(4)],local=rng.randrange(4),overlay=rng.randrange(65536),ticks=rng.randrange(256))
for stage in range(3):
    for field,values in (('local_index',(0,1,2,3)),('ticks',(0,1,255)),('overlay',(0,65535)),('camera_heading',(0,0xffff,0x4000)),('player_0_keys',(0,3)),('player_0_flags',(0,8,12)),('player_1_zoom',(0,0xffffffff))):
        for value in values:case('live_callbacks',[(0,0,1),(12,3,65536),(0,0,2),(0,0,3)],overlay=1,script={stage:[(field,value)]})
for players in ([(0,0,4096)]*4,[(12,3,65536)]*4):
    case('failure_baseline',players,overlay=1)
    for index in range(len(oracle)):case('failure_prefix',players,overlay=1,fail=index)
def guard(pointer,op):
    before=bytes(v);assert lib.ssa_render_view_step(pointer,op)==-1 and bytes(v)==before;report['guards']+=1
guard(None,C.byref(ops));guard(C.byref(v),None);empty=Ops();guard(C.byref(v),C.byref(empty))
v.updating=1;guard(C.byref(v),C.byref(ops));v.updating=0
# These invalid use-site guards retain the original CLEAR/zoom prefix.
v.local_index=4;actual.clear();actions={};failure=None
assert lib.ssa_render_view_step(C.byref(v),C.byref(ops))==-1 and len(actual)==1 and not v.updating;report['guards']+=1
v.local_index=0;v.players[0].flags=0;v.math=C.POINTER(Tables)();actual.clear()
assert lib.ssa_render_view_step(C.byref(v),C.byref(ops))==-1 and len(actual)==1 and not v.updating;report['guards']+=1
v.math=C.pointer(tables)
# Existing native PC/game fixture supplies real movement, collision, jumping and
# H-view. Rendering calls observe camera inputs; this does not claim scene pixels.
unused_renderers={'plane_renderer.c','wall_renderer.c','world_visibility.c','world_scene.c'}
sources=sorted(p for p in (ROOT/'src').glob('*.c') if p.name not in unused_renderers)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_render_view.c'),*map(str,sources),'-o',str(B/'test_render_view')],check=True)
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
integration=subprocess.run([str(B/'test_render_view'),str(ROOT/'orig/SSA.gba')],capture_output=True,text=True,env=env,timeout=180)
if integration.returncode:
    print(integration.stdout,end='');print(integration.stderr,end='');integration.check_returncode()
report['sanitizer_integration']=json.loads(integration.stdout)
report['integration_scope']='Native PC/Game frames on16realmaps, jump/H/mouse yaw ->posecamera ->negativeyawsincos. CLEAR fills actual120x120buffer; WORLD event observes input only, no scene rendering claim.'
report['integration_source_sha256']={s.name:hashlib.sha256(s.read_bytes()).hexdigest() for s in sources}
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('src/render_view.c','include/ssa/render_view.h','src/movement.c','src/geometry.c','tests/test_render_view_rom.py')}
(ROOT/'analysis/native-render-view-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
