"""Complete original plane pool/span generation and native indexed pixels."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,random,os,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from inspect_maps import unpack_lz77
ROM=(ROOT/'orig/SSA.gba').read_bytes();SHA=hashlib.sha1(ROM).hexdigest()
assert SHA=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
class Plane(C.Structure):
    _fields_=[('height',C.c_int16),('min_x',C.c_int16),('max_x',C.c_int16),('color',C.c_uint16),('top',C.c_uint8*122),('bottom',C.c_uint8*122)]
assert C.sizeof(Plane)==252
class Pool(C.Structure):
    _fields_=[('records',Plane*80),('count',C.c_uint),('capacity',C.c_uint),('opening_cursor',C.c_size_t)]
class Sky(C.Structure):
    _fields_=[('pixels',(C.c_uint8*(768*120))*2),('loaded',C.c_uint)]
class Renderer(C.Structure):
    _fields_=[('row_starts',C.c_int32*120),('sky_offset',C.c_uint32),('height_distance',C.c_uint32),('color',C.c_uint8),('updating',C.c_uint)]
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/plane_renderer.c'),str(ROOT/'src/raster_fill.c'),'-o',str(B/'libssa_plane_renderer.so')],check=True)
lib=C.CDLL(str(B/'libssa_plane_renderer.so'));P=C.POINTER
lib.ssa_plane_pool_init.argtypes=[P(Pool),C.c_uint];lib.ssa_plane_pool_reset.argtypes=[P(Pool)]
lib.ssa_plane_find.argtypes=[P(Pool),C.c_int32,C.c_uint];lib.ssa_plane_find.restype=P(Plane)
lib.ssa_plane_check.argtypes=[P(Pool),P(Plane),C.c_int32,C.c_int32];lib.ssa_plane_check.restype=P(Plane)
lib.ssa_sky_assets_load.argtypes=[P(Sky),C.c_void_p,C.c_size_t]
common=[P(Renderer),P(Sky),C.c_uint,C.c_void_p,C.c_size_t]
lib.ssa_plane_span.argtypes=common+[C.c_int32,C.c_int32,C.c_int32]
lib.ssa_plane_edges.argtypes=common+[C.c_int32,C.c_uint,C.c_uint,C.c_uint,C.c_uint]
lib.ssa_plane_render.argtypes=[P(Renderer),P(Pool),P(Sky),C.c_uint,C.c_uint16,C.c_int16,C.c_void_p,C.c_size_t]
sky=Sky();rom=C.create_string_buffer(ROM);assert lib.ssa_sky_assets_load(C.byref(sky),rom,len(ROM))==1;del rom
assert bytes(sky.pixels[0])==ROM[0x46285c:0x46285c+92160]
assert bytes(sky.pixels[1])==ROM[0x46dc5c:0x46dc5c+92160]
assert struct.unpack_from('<120I',ROM,0x44aaec)==tuple(i*768 for i in range(120))
assert struct.unpack_from('<120I',ROM,0x44accc)==tuple(i*120 for i in range(120))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000)
# Real ARM functions copied to original IWRAM locations; no raster callbacks.
uc.mem_write(0x030005ac,ROM[0x348:0x358]);uc.mem_write(0x030005bc,ROM[0x358:0x36c])
WORLD=0x03004930;PLANES=0x02000000;PLAYER=0x02008000;LEVEL=0x02009000;FB=0x03000648;STOP=0x087ff000
rng=random.Random(0xab10);fb=(C.c_uint8*14400)();pool=Pool();r=Renderer();alloc=[]
report={'rom_sha1':SHA,'pool_init':0,'pool_reset':0,'pool_find':0,'pool_check':0,'spans':0,'edges':0,'complete_plane_frames':0,'real_map_sector_cases':0,'real_map_levels':16,'guards':0,'failure_prefixes':0,'sky_bytes':184320}
def put(at,fmt,*values):uc.mem_write(at,struct.pack('<'+fmt,*values))
def get(at,fmt='I'):return struct.unpack('<'+fmt,uc.mem_read(at,struct.calcsize('<'+fmt)))[0]
def hook(machine,address,size,ctx):
    if address==0x080033d8:
        alloc.append(machine.reg_read(UC_ARM_REG_R0));machine.reg_write(UC_ARM_REG_R0,PLANES)
        machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
def call(address,args=(),limit=1000000):
    for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(reg,v&0xffffffff)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00)
    for i,v in enumerate(args[4:]):put(0x03007e00+4*i,'I',v&0xffffffff)
    uc.reg_write(UC_ARM_REG_LR,STOP|1);uc.emu_start(address|1,STOP,count=limit)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
    return uc.reg_read(UC_ARM_REG_R0)
def install_pool():
    uc.mem_write(PLANES,bytes(pool.records));put(WORLD+0x60,'II',PLANES,PLANES+pool.count*252)
def compare_pool():
    assert get(WORLD+0x64)==PLANES+pool.count*252
    assert bytes(uc.mem_read(PLANES,80*252))==bytes(pool.records)
def index(p):return (C.addressof(p.contents)-C.addressof(pool.records))//252
for level in range(16):
    put(0x0300572c,'I',LEVEL);put(LEVEL+16,'H',level);alloc.clear();call(0x0800a844)
    assert lib.ssa_plane_pool_init(C.byref(pool),level)==1
    assert alloc==[pool.capacity*252] and get(WORLD+0x60)==PLANES;report['pool_init']+=1
for case in range(256):
    assert lib.ssa_plane_pool_init(C.byref(pool),case%16)==1
    initial=rng.randbytes(80*252);C.memmove(pool.records,initial,len(initial));install_pool()
    # Reset retains all payload bytes and rewinds only both original cursors.
    pool.count=5;pool.opening_cursor=17;put(WORLD+0x64,'I',PLANES+5*252);put(WORLD+0x340,'I',WORLD+0x70+17)
    assert lib.ssa_plane_pool_reset(C.byref(pool))==1;call(0x0800a870);compare_pool()
    assert get(WORLD+0x340)==WORLD+0x70 and pool.opening_cursor==0;report['pool_reset']+=1
    keys=[]
    for j in range(24):
        h,c=(rng.randrange(-100000,100000),rng.randrange(65536)) if not keys or j%3 else rng.choice(keys)
        keys.append((h,c));p=lib.ssa_plane_find(C.byref(pool),h,c);answer=call(0x0800a888,(h,c))
        assert answer==PLANES+index(p)*252;compare_pool();report['pool_find']+=1
    # Native handles remain stable through splits; randomized stale payload
    # verifies only originaltop[1..120] is initialized in newly allocatedslots.
    for j in range(16):
        i=rng.randrange(pool.count);p=C.pointer(pool.records[i]);left=rng.randrange(120);right=rng.randrange(left,120)
        for x in range(120):pool.records[i].top[x+1]=255 if rng.randrange(3) else rng.randrange(120)
        install_pool();p=lib.ssa_plane_check(C.byref(pool),p,left,right);answer=call(0x0800a900,(PLANES+i*252,left,right))
        assert p and answer==PLANES+index(p)*252;compare_pool();report['pool_check']+=1

def setup(level,color,offset=0):
    initial=rng.randbytes(14400);C.memmove(fb,initial,len(initial));uc.mem_write(FB,initial)
    r.color=color;r.sky_offset=offset
    for y in range(120):r.row_starts[y]=rng.randrange(120)
    uc.mem_write(WORLD+0x344,bytes(r.row_starts));put(WORLD+0x708,'B',color);put(0x0300506c,'I',offset)
    put(0x0300572c,'I',LEVEL);put(LEVEL+16,'H',level)
def compare_pixels():assert bytes(fb)==bytes(uc.mem_read(FB,14400))
for case in range(8192):
    level=case%16;row=case%120;left=rng.randrange(120);right=rng.randrange(left,120)
    color=0 if case%3 else rng.randrange(1,256);offset=(rng.randrange(65536)*5)>>9
    setup(level,color,offset)
    assert lib.ssa_plane_span(C.byref(r),C.byref(sky),level,fb,14400,row,left,right)==1
    call(0x0800a98c,(row,left,right));compare_pixels();report['spans']+=1
for case in range(4096):
    level=case%16;color=0 if case%2 else rng.randrange(1,256);x=rng.randrange(1,121)
    def interval():
        t=rng.randrange(120);return (255,rng.randrange(120)) if rng.randrange(5)==0 else (t,rng.randrange(t,120))
    t1,b1=interval();t2,b2=interval();setup(level,color,rng.randrange(640))
    for y in range(120):r.row_starts[y]=rng.randrange(x)
    uc.mem_write(WORLD+0x344,bytes(r.row_starts))
    assert lib.ssa_plane_edges(C.byref(r),C.byref(sky),level,fb,14400,x,t1,b1,t2,b2)==1
    call(0x0800aa4c,(x,t1,b1,t2,b2));compare_pixels()
    assert bytes(r.row_starts)==bytes(uc.mem_read(WORLD+0x344,480));report['edges']+=1

def whole(level,heading,eye):
    install_pool();setup(level,rng.randrange(256),rng.randrange(640))
    r.height_distance=rng.randrange(65536);put(WORLD+0x704,'I',r.height_distance)
    put(0x03004648,'I',PLAYER);put(0x0300464e,'H',2);put(PLAYER+512+0xb8,'H',heading);put(0x0300402a,'h',eye)
    assert lib.ssa_plane_render(C.byref(r),C.byref(pool),C.byref(sky),level,heading,eye,fb,14400)==1
    call(0x0800ab10);compare_pixels();compare_pool()
    assert bytes(r.row_starts)==bytes(uc.mem_read(WORLD+0x344,480))
    assert (r.sky_offset,r.height_distance,r.color)==(get(0x0300506c),get(WORLD+0x704),get(WORLD+0x708,'B'))
    report['complete_plane_frames']+=1
def populate(height,color):
    p=lib.ssa_plane_find(C.byref(pool),height,color);assert p
    left=rng.randrange(100);right=rng.randrange(left,120)
    p=lib.ssa_plane_check(C.byref(pool),p,left,right);assert p
    for x in range(left,right+1):
        if rng.randrange(6):
            top=rng.randrange(120);p.contents.top[x+1]=top;p.contents.bottom[x+1]=rng.randrange(top,120)
for case in range(1024):
    level=case%16;assert lib.ssa_plane_pool_init(C.byref(pool),level)==1
    for j in range(case%6):populate(rng.randrange(-32768,32768),0 if j%2 else rng.randrange(1,256))
    whole(level,rng.randrange(65536),rng.randrange(-32768,32768))
# Real sector heights and floor/ceiling selectors from everymap, with explicit
# controlled coverage. This is not a claim of native BSP visibility here.
for level in range(16):
    offset=struct.unpack_from('<I',ROM,0x44aef4+level*4)[0]-0x08000000
    data,_=unpack_lz77(ROM[offset:]);count=struct.unpack_from('<H',data,10)[0];start=struct.unpack_from('<I',data,0x24)[0]
    for case in range(8):
        lib.ssa_plane_pool_init(C.byref(pool),level)
        sector=(case*count)//8;floor,ceiling=struct.unpack_from('<hh',data,start+sector*20)
        populate(floor,data[start+sector*20+10]);populate(ceiling,data[start+sector*20+11])
        whole(level,(case*8191)&65535,(floor+ceiling)//2);report['real_map_sector_cases']+=1

def guard(fn,args,objects):
    before=[bytes(x) for x in objects];assert fn(*args)==-1
    assert [bytes(x) for x in objects]==before;report['guards']+=1
setup(0,1);lib.ssa_plane_pool_init(C.byref(pool),0)
for row,left,right in ((-1,0,2),(0,-1,2),(0,0,120),(0,2,1)):
    guard(lib.ssa_plane_span,(C.byref(r),C.byref(sky),0,fb,14400,row,left,right),(r,fb))
for row in (120,255,0x7fffffff):
    before=bytes(fb);assert lib.ssa_plane_span(C.byref(r),None,99,fb,0,row,-1,-2)==1
    call(0x0800a98c,(row,-1,-2));assert bytes(fb)==before;compare_pixels();report['spans']+=1
for values in ((-1,0,0,0,0),(121,0,0,0,0),(1,256,0,0,0),(1,0,120,0,0)):
    guard(lib.ssa_plane_edges,(C.byref(r),C.byref(sky),0,fb,14400,*values),(r,fb))
r.updating=1
guard(lib.ssa_plane_span,(C.byref(r),C.byref(sky),0,fb,14400,0,0,1),(r,fb))
guard(lib.ssa_plane_render,(C.byref(r),C.byref(pool),C.byref(sky),0,0,0,fb,14400),(r,pool,fb));r.updating=0
pool.count=pool.capacity
before=bytes(pool);assert not lib.ssa_plane_find(C.byref(pool),12345,77) and bytes(pool)==before;report['guards']+=1
lib.ssa_plane_pool_init(C.byref(pool),0);p=lib.ssa_plane_find(C.byref(pool),0,1)
for left,right in ((-1,0),(0,120),(2,1)):
    before=bytes(pool);assert not lib.ssa_plane_check(C.byref(pool),p,left,right) and bytes(pool)==before;report['guards']+=1
for lo,hi in ((-1,-2),(121,-1),(120,-2),(120,120)):
    p.contents.min_x=lo;p.contents.max_x=hi;before=bytes(pool)
    assert not lib.ssa_plane_check(C.byref(pool),p,0,1) and bytes(pool)==before;report['guards']+=1
lib.ssa_plane_pool_init(C.byref(pool),0);pool.capacity=1;p=lib.ssa_plane_find(C.byref(pool),0,1)
assert lib.ssa_plane_check(C.byref(pool),p,0,3);p.contents.top[1]=0
before=bytes(pool);assert not lib.ssa_plane_check(C.byref(pool),p,0,2) and bytes(pool)==before;report['guards']+=1
# Native validation failure after an earlier completed row retains that prefix.
setup(0,42);r.row_starts[0]=0;r.row_starts[1]=-1;before=bytes(fb)
assert lib.ssa_plane_edges(C.byref(r),C.byref(sky),0,fb,14400,2,0,1,2,2)==-1
assert bytes(fb[:2])==bytes([42,42]) and bytes(fb[2:])==before[2:];report['failure_prefixes']+=1
empty=Sky();rom=C.create_string_buffer(ROM)
for n in (0,1,0x46dc5c,0x46dc5c+92159):guard(lib.ssa_sky_assets_load,(C.byref(empty),rom,n),(empty,))
guard(lib.ssa_sky_assets_load,(C.byref(sky),rom,len(ROM)),(sky,))
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'src/plane_renderer.c'),str(ROOT/'src/raster_fill.c'),str(ROOT/'tests/test_plane_renderer.c'),'-o',str(B/'test_plane_renderer')],check=True)
result=subprocess.run([str(B/'test_plane_renderer'),str(ROOT/'orig/SSA.gba')],check=True,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));report.update(json.loads(result.stdout))
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('src/plane_renderer.c','include/ssa/plane_renderer.h','tests/test_plane_renderer_rom.py')}
(ROOT/'analysis/native-plane-renderer-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
