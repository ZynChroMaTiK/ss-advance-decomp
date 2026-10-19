"""Original register/VRAM evidence plus independent native display pixels."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,math,os,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
def classes(path,names):
    nodes=[n for n in ast.parse((ROOT/path).read_text()).body if isinstance(n,ast.ClassDef) and n.name in names]
    exec(compile(ast.Module(nodes,type_ignores=[]),path,'exec'),globals())
classes('tests/test_sprite_assets.py',['Part','Image','Assets'])
classes('tests/test_weapon_animation_rom.py',['Object']);OP=C.POINTER(Object)
classes('tests/test_sprite_dispatch_rom.py',['Dispatch'])
classes('tests/test_sprite_oam_rom.py',['Math','Cache','Entry','Oam'])
class Registers(C.Structure):
    _fields_=[('dispcnt',C.c_uint16),('bg2cnt',C.c_uint16),('pa',C.c_int16),('pb',C.c_int16),
              ('pc',C.c_int16),('pd',C.c_int16),('x',C.c_int32),('y',C.c_int32),('bldcnt',C.c_uint16),('mosaic',C.c_uint16)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['display.c','sprite_assets.c','sprite_oam.c','movement.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_display.so')],check=True)
lib=C.CDLL(str(B/'libssa_display.so'))
lib.ssa_display_registers_default.argtypes=[C.POINTER(Registers)]
lib.ssa_display_compose.argtypes=[C.POINTER(Registers),C.c_void_p,C.c_size_t,C.POINTER(C.c_uint16),C.POINTER(C.c_uint16),C.POINTER(Oam),C.c_void_p,C.c_size_t,C.POINTER(C.c_uint16),C.c_size_t]
lib.ssa_sprite_assets_load.argtypes=[C.POINTER(Assets),C.c_void_p,C.c_size_t]
lib.ssa_sprite_oam_init.argtypes=[C.POINTER(Oam),C.c_void_p,C.c_size_t]
lib.ssa_sprite_oam_build.argtypes=[C.POINTER(Oam),C.POINTER(Dispatch),C.POINTER(Assets),C.POINTER(Cache),C.POINTER(Math)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Math),C.c_void_p,C.c_size_t]
assets=Assets();tables=Math();o=Oam();r=Registers()
assert lib.ssa_sprite_assets_load(C.byref(assets),ROM,len(ROM))==1
assert lib.ssa_math_tables_load(C.byref(tables),ROM,len(ROM))==1
assert lib.ssa_sprite_oam_init(C.byref(o),ROM,len(ROM))==1
assert lib.ssa_display_registers_default(C.byref(r))==1
world=bytes((x*7+y*13)%256 for y in range(120) for x in range(120))
world_buffer=C.create_string_buffer(world);output=(C.c_uint16*38400)()
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_register_frames':0,'rom_defaults':0,'rom_tilemaps':0,'rom_transfers':0,'independent_pixel_frames':0,'real_images':0,'affine_frames':0}
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
for address,size in ((0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x10000),(0x05000000,0x1000),(0x06000000,0x20000)):uc.mem_map(address,size)
G,P,E,L,BUFFER,STOP=0x02000000,0x02001000,0x02002000,0x02003000,0x02010000,0x087ff000
def put(a,f,*v):uc.mem_write(a,struct.pack('<'+f,*v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
def copy(source,dest,control,dma=False):
    width=4 if control&(1<<26) else 2;count=control&(0xffff if dma else 0x1fffff)
    fixed=((control>>23)&3)==2 if dma else bool(control&(1<<24))
    data=bytes(uc.mem_read(source,width if fixed else count*width));uc.mem_write(dest,data*count if fixed else data)
def hardware(machine,access,address,size,value,ctx):
    if address==0x040000dc and value&0x80000000:copy(get(0x040000d4),get(0x040000d8),value,True)
def hook(machine,address,size,ctx):
    if address==0x080c5320:
        copy(machine.reg_read(UC_ARM_REG_R0),machine.reg_read(UC_ARM_REG_R1),machine.reg_read(UC_ARM_REG_R2))
        machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook);uc.hook_add(UC_HOOK_MEM_WRITE,hardware)
def call(address,args=(),arm=False):
    for reg,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(reg,value)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(address if arm else address|1,STOP,count=300000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
for players in (1,2):
    for level in range(16):
        family=0 if level<6 or level in (12,13) else 1
        for page in (0,1):
            uc.mem_write(0x03000000,bytes(0x8000));put(0x03004010,'I',G);put(G+16,'B',players)
            put(0x03004648,'I',P);put(P+0xac,'I',E);put(E+0x32,'h',100)
            put(0x0300572c,'I',L);put(L+0x10,'H',level);put(0x030000c0,'IB',BUFFER,page)
            call(0x080074f0);report['rom_defaults']+=1
            assert struct.unpack('<6i',uc.mem_read(0x030000c8,24))==(128,0,0,192,0,0)
            call(0x08006e30);report['rom_tilemaps']+=1
            uc.mem_write(BUFFER,world);call(0x08000264,(0,0x030000c0),True);report['rom_transfers']+=1
            bank=0x8000 if page==0 else 0x4000;vram=bytes(uc.mem_read(0x06000000,0x10000))
            for ty in range(15):
                for tx in range(15):
                    tile=vram[ty*32+tx];assert tile==tx*15+ty
                    for row in range(8):assert vram[bank+tile*64+row*8:bank+tile*64+row*8+8]==world[(ty*8+row)*120+tx*8:(ty*8+row)*120+tx*8+8]
            for flags in (0,1,0x10,0x20):
                put(G+4,'H',flags);call(0x0800750c);report['rom_register_frames']+=1
                assert get(0x04000000,'H')==0x1441
                r.bg2cnt=get(0x0400000c,'H');assert r.bg2cnt==(0x6089 if page==0 else 0x6085)
                assert struct.unpack('<4h',uc.mem_read(0x04000020,8))==(r.pa,r.pb,r.pc,r.pd)
                assert bytes(uc.mem_read(0x04000028,8))==bytes(8)
            # Independent hardware path reads original-generated tilemap/VRAM,
            # rather than use the native flattened framebuffer's address formula.
            expected=[]
            for sy in range(160):
                for sx in range(240):
                    bx=(r.pa*sx+r.pb*sy+r.x)//256%256;by=(r.pc*sx+r.pd*sy+r.y)//256%256
                    tile=vram[(by//8)*32+bx//8]
                    index=vram[bank+tile*64+(by%8)*8+bx%8]
                    expected.append(assets.background_palette[family][index]&32767)
            assert lib.ssa_display_compose(C.byref(r),world_buffer,14400,assets.background_palette[family],assets.palette,None,None,0,output,38400)==1
            assert list(output)==expected;report['independent_pixel_frames']+=1
lib.ssa_display_registers_default(C.byref(r))
objects=(Object*84)();dispatch=Dispatch();cache=(Cache*311)()
for i in range(84):dispatch.objects[i]=C.pointer(objects[i])
dispatch.active_count=1;dispatch.active[0]=0
base=[assets.background_palette[0][world[(y*3//4)*120+x//2]]&32767 for y in range(160) for x in range(240)]
shapes=[[(8,8),(16,16),(32,32),(64,64)],[(16,8),(32,8),(32,16),(64,32)],[(8,16),(8,32),(16,32),(32,64)]]
def reference(entries,tiles):
    # Independent full-rectangle painter, with floating-point affine samples.
    # Reduce the OBJ layer separately before comparing it with the world BG.
    pixels={}
    for p in entries:
        if p.a0&0x300==0x200:continue
        width,height=shapes[p.a0>>14][p.a1>>14];affine=bool(p.a0&0x100)
        bw=width*(2 if affine and p.a0&0x200 else 1);bh=height*(2 if affine and p.a0&0x200 else 1)
        matrix=[C.c_int16(entries[4*((p.a1>>9)&31)+n].affine).value/256 for n in range(4)]
        for ly in range(bh):
            oy=((p.a0&255)+ly)%256
            if oy>=160:continue
            for lx in range(bw):
                ox=((p.a1&511)+lx)%512
                if ox>=240:continue
                if affine:
                    a,b,c,d=matrix;sx=math.floor(a*(lx-bw/2)+b*(ly-bh/2)+width/2);sy=math.floor(c*(lx-bw/2)+d*(ly-bh/2)+height/2)
                    if not(0<=sx<width and 0<=sy<height):continue
                else:sx=width-1-lx if p.a1&0x1000 else lx;sy=height-1-ly if p.a1&0x2000 else ly
                tile=(sy//8)*(width//8)+sx//8;index=tiles[((p.a2&1023)*32+tile*64+(sy%8)*8+sx%8)%32768]
                at=oy*240+ox;priority=(p.a2>>10)&3
                prior=pixels.get(at)
                if prior and prior[1]<=priority:continue
                if index:pixels[at]=(index,priority)
                elif prior:pixels[at]=(prior[0],priority)
    expected=base.copy()
    for offset,(index,priority) in pixels.items():
        y,x=divmod(offset,240)
        if priority<=1 or world[(y*3//4)*120+x//2]==0:expected[offset]=assets.palette[index]&32767
    return expected
for index,im in enumerate(assets.images):
    p=objects[0];p.type=1;p.flags=5;p.animation=0;p.previous_frame=index;p.frame=index;p.layer=15
    p.scale_x=p.scale_y=4096;p.unknown2c=0;p.x=120*256;p.y=80*256
    cache[index].resident=1;cache[index].tile=160
    assert lib.ssa_sprite_oam_build(C.byref(o),C.byref(dispatch),C.byref(assets),cache,C.byref(tables))==1
    tiles=bytearray(32768);offset=im.rom_offset+28+8*im.part_count
    raw=ROM[offset:offset+64*im.tile_count]
    for j,value in enumerate(raw):tiles[(160*64+j)%32768]=value
    tile_buffer=C.create_string_buffer(bytes(tiles))
    assert lib.ssa_display_compose(C.byref(r),world_buffer,14400,assets.background_palette[0],assets.palette,C.byref(o),tile_buffer,32768,output,38400)==1,index
    assert list(output)==reference(o.entries,tiles),index
    report['real_images']+=1;report['independent_pixel_frames']+=1
# Matrices include flips, right-angle rotations, fractional scaling and shear;
# coordinates exercise both hardware wrap periods and clipping edges.
rng=random.Random(0x750c);tiles=bytes(rng.randrange(256) for _ in range(32768));tile_buffer=C.create_string_buffer(tiles)
for case in range(256):
    for e in o.entries:e.a0=0x200;e.a1=e.a2=e.affine=0
    p=o.entries[0];shape=case%3;size=(case//3)%4;affine=case%2==0
    p.a0=(shape<<14)|0x2000|([0,120,150,250][case%4])|(0x100 if affine else 0)|(0x200 if affine and case%3==0 else 0)
    p.a1=(size<<14)|([0,208,238,500][case%4])|(0 if affine else (case%4)*0x1000)
    p.a2=([0,319,1022,1023][case%4])|((case%4)<<10)
    matrices=[(256,0,0,256),(-256,0,0,256),(0,256,-256,0),(128,0,0,192),(256,128,-128,256),(0,0,0,0),(-32768,32767,32767,-32768)]
    for e,v in zip(o.entries,matrices[case%len(matrices)]):e.affine=v&65535
    assert lib.ssa_display_compose(C.byref(r),world_buffer,14400,assets.background_palette[0],assets.palette,C.byref(o),tile_buffer,32768,output,38400)==1
    assert list(output)==reference(o.entries,tiles),case
    report['independent_pixel_frames']+=1;report['affine_frames']+=affine
# Full overlap: transparent holes expose later OAM, but BG can still cover it.
for priority in range(4):
    for e in o.entries:e.a0=0x200;e.a1=e.a2=0
    for j in range(4):o.entries[j].a0=0x2000|60;o.entries[j].a1=100;o.entries[j].a2=j*2|(priority<<10)
    assert lib.ssa_display_compose(C.byref(r),world_buffer,14400,assets.background_palette[0],assets.palette,C.byref(o),tile_buffer,32768,output,38400)==1
    assert list(output)==reference(o.entries,tiles);report['independent_pixel_frames']+=1
# Every pair of priorities, with opaque/transparent samples in each order.
# Explicit color assertions catch ordinary sorting or ignoring transparent OBJ
# samples: a later transparent priority0 promotes the earlier priority3 color,
# preventing an intervening-priority opaque sprite from replacing it.
report['mixed_priority_frames']=0
for first in range(4):
    for second in range(4):
        for mask in range(4):
            for e in o.entries:e.a0=0x200;e.a1=e.a2=0
            data=bytearray(32768)
            for j,priority in enumerate((first,second)):
                o.entries[j].a0=0x2000|60;o.entries[j].a1=100;o.entries[j].a2=j*2|(priority<<10)
                if mask&(1<<j):data[j*64:(j+1)*64]=bytes([j+1])*64
            buf=C.create_string_buffer(bytes(data))
            assert lib.ssa_display_compose(C.byref(r),world_buffer,14400,assets.background_palette[0],assets.palette,C.byref(o),buf,32768,output,38400)==1
            assert list(output)==reference(o.entries,data),(first,second,mask)
            report['independent_pixel_frames']+=1;report['mixed_priority_frames']+=1
for transparent in (False,True):
    for e in o.entries:e.a0=0x200;e.a1=e.a2=0
    data=bytearray(32768)
    for j,priority in enumerate((3,0,1)):
        o.entries[j].a0=0x2000|60;o.entries[j].a1=100;o.entries[j].a2=j*2|(priority<<10)
        data[j*64:(j+1)*64]=bytes([0 if j==1 and transparent else j+1])*64
    buf=C.create_string_buffer(bytes(data))
    assert lib.ssa_display_compose(C.byref(r),world_buffer,14400,assets.background_palette[0],assets.palette,C.byref(o),buf,32768,output,38400)==1
    assert output[60*240+100]==assets.palette[1 if transparent else 2]&32767
    assert list(output)==reference(o.entries,data)
    report['independent_pixel_frames']+=1;report['mixed_priority_frames']+=1
# Mixed affine/normal OAM, wrapping tile addresses, flips and transparent holes.
for case in range(192):
    for e in o.entries:e.a0=0x200;e.a1=e.a2=e.affine=0
    data=bytes(rng.choice((0,0,0,1,2,139,143,255)) for _ in range(32768))
    for j in range(8):
        affine=(case+j)%3==0
        o.entries[j].a0=0x2000|(0x100 if affine else 0)|((56+rng.randrange(16))&255)
        o.entries[j].a1=(96+rng.randrange(16))|(0 if affine else rng.randrange(4)*0x1000)
        o.entries[j].a2=rng.randrange(1024)|(rng.randrange(4)<<10)
    for e,v in zip(o.entries,(128,64,-64,256)):e.affine=v&65535
    buf=C.create_string_buffer(data)
    assert lib.ssa_display_compose(C.byref(r),world_buffer,14400,assets.background_palette[0],assets.palette,C.byref(o),buf,32768,output,38400)==1
    assert list(output)==reference(o.entries,data),case
    report['independent_pixel_frames']+=1;report['mixed_priority_frames']+=1
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_display.c'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'test_display')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
run=subprocess.run([str(B/'test_display'),str(ROOT/'orig/SSA.gba')],capture_output=True,text=True,env=env)
if run.returncode:print(run.stderr,end='');run.check_returncode()
report['sanitizers']=json.loads(run.stdout)
report['scope']='Normal mode1 BG2 hardware presentation, complete original074F0/06E30/0750C and ARM264 transfer (BIOS/DMA copies controlled), actual311nativeFCE4 sprite images, independent VRAM/map and mixed-priority pixel references including transparent priority promotion. Blend/window/mosaic/budget overflow explicitly rejected. Pixel priority checks are a hardware-model reference, not ROM CPU or physical hardware comparisons. No game CPU execution in native library.'
report['source_sha256']={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources}
report['header_sha256']={s:hashlib.sha256((ROOT/'include/ssa'/s).read_bytes()).hexdigest() for s in ('display.h','sprite_oam.h','sprite_assets.h')}
report['hardware_references']=['https://raw.githubusercontent.com/mgba-emu/gbatek/master/gba.md','https://github.com/mgba-emu/mgba/blob/master/src/gba/renderers/software-obj.c','https://github.com/mgba-emu/mgba/blob/master/src/gba/memory.c']
report['test_sha256']={s:hashlib.sha256((ROOT/'tests'/s).read_bytes()).hexdigest() for s in ('test_display.py','test_display.c')}
(ROOT/'analysis/native-display-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
