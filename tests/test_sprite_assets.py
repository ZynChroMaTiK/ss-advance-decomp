"""Real sprite data, original cache transfer chain, palette setup and guards."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,os
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
class Part(C.Structure):
    _fields_=[('attribute0',C.c_uint16),('attribute1',C.c_uint16),('tile',C.c_uint16),('x',C.c_int16),('y',C.c_int16),('width',C.c_uint8),('height',C.c_uint8)]
class Image(C.Structure):
    _fields_=[('rom_offset',C.c_uint32),('part_count',C.c_uint16),('tile_count',C.c_uint16),('bounds',C.c_int8*4),('min_x',C.c_int16),('min_y',C.c_int16),('width',C.c_uint16),('height',C.c_uint16),('parts',C.POINTER(Part)),('tiles',C.POINTER(C.c_uint8))]
class Assets(C.Structure):
    _fields_=[('images',Image*311),('palette',C.c_uint16*256),('background_palette',(C.c_uint16*256)*2),('loaded',C.c_uint)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/sprite_assets.c'),'-o',str(B/'libssa_sprite_assets.so')],check=True)
lib=C.CDLL(str(B/'libssa_sprite_assets.so'))
lib.ssa_sprite_assets_load.argtypes=[C.POINTER(Assets),C.c_void_p,C.c_size_t]
lib.ssa_sprite_assets_free.argtypes=[C.POINTER(Assets)]
lib.ssa_sprite_image_indices.argtypes=[C.POINTER(Image),C.c_void_p,C.c_size_t,C.c_size_t]
lib.ssa_sprite_palette_rgba.argtypes=[C.POINTER(C.c_uint16),C.c_void_p]
lib.ssa_sprite_palette_family.argtypes=[C.c_uint]
assets=Assets();rom=C.create_string_buffer(ROM)
assert lib.ssa_sprite_assets_load(C.byref(assets),rom,len(ROM))==1
del rom # Asset ownership must not depend on the supplied ROM buffer.
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'images':311,'parts':0,'tiles':0,'tile_bytes':0,'rom_transfer_chains':0,'rom_oam_descriptors':0,'rom_palette_setups':0,'malformed_cases':0,'native_guards':0,'image_sha256':[]}
def signed9(v):return (v&511)-512 if v&256 else v&511
rasters=[]
for index,im in enumerate(assets.images):
    offset=struct.unpack_from('<I',ROM,0x44b710+index*4)[0]-0x8000000
    assert im.rom_offset==offset and ROM[offset:offset+4]==b'AGB!'
    parts,tiles=struct.unpack_from('<HH',ROM,offset+4)
    assert (im.part_count,im.tile_count)==(parts,tiles)
    assert tuple(im.bounds)==struct.unpack_from('<4b',ROM,offset+8)
    raw=ROM[offset+28+parts*8:offset+28+parts*8+tiles*64]
    assert C.string_at(im.tiles,tiles*64)==raw
    independent=[]
    for j,p in enumerate(im.parts[:parts]):
        a,b,t,w,h=struct.unpack_from('<HHHBB',ROM,offset+28+j*8)
        assert (p.attribute0,p.attribute1,p.tile,p.width,p.height,p.x,p.y)==(a,b,t,w,h,signed9(b),signed9(a))
        independent.append((signed9(b),signed9(a),t,w,h))
    left=min(p[0] for p in independent);top=min(p[1] for p in independent)
    right=max(p[0]+p[3] for p in independent);bottom=max(p[1]+p[4] for p in independent)
    assert (im.min_x,im.min_y,im.width,im.height)==(left,top,right-left,bottom-top)
    # Independent tile expansion: visit tile blocks, reverse part priority,
    # overwrite nontransparent source pixels rather than fill empty destination.
    expected=bytearray(im.width*im.height)
    for x,y,t,w,h in reversed(independent):
        for ty in range(h//8):
            for tx in range(w//8):
                tile=raw[64*(t+ty*(w//8)+tx):64*(t+ty*(w//8)+tx+1)]
                for py in range(8):
                    for px in range(8):
                        if tile[py*8+px]:expected[(y-top+ty*8+py)*im.width+x-left+tx*8+px]=tile[py*8+px]
    stride=im.width+7;out=C.create_string_buffer(b'\xa5'*(stride*im.height))
    assert lib.ssa_sprite_image_indices(C.byref(im),out,stride,stride*im.height)==1
    for y in range(im.height):
        assert out.raw[y*stride:y*stride+im.width]==expected[y*im.width:(y+1)*im.width]
        assert out.raw[y*stride+im.width:(y+1)*stride]==b'\xa5'*7
    before=out.raw
    for bad_stride,capacity in ((im.width-1,len(out)),(im.width,im.width*im.height-1),(0,0)):
        assert lib.ssa_sprite_image_indices(C.byref(im),out,bad_stride,capacity)==-1 and out.raw==before
        report['native_guards']+=1
    rasters.append(bytes(expected));report['image_sha256'].append(hashlib.sha256(expected).hexdigest())
    report['parts']+=parts;report['tiles']+=tiles;report['tile_bytes']+=len(raw)
for i in range(256):
    assert assets.palette[i]==(struct.unpack_from('<H',ROM,0x79effc+2*i)[0] if i else 0)
    for f in range(2):assert assets.background_palette[f][i]==(struct.unpack_from('<H',ROM,0x454a5c+512*f+2*i)[0] if i else 0)
rgba=(C.c_uint8*1024)();assert lib.ssa_sprite_palette_rgba(assets.palette,rgba)==1
for i in range(256):
    for c in range(3):
        n=(assets.palette[i]>>(5*c))&31
        bits=f'{n:05b}';assert rgba[4*i+c]==int(bits+bits[:3],2)
    assert rgba[4*i+3]==(255 if i else 0)

uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
for address,size in ((0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x10000),(0x05000000,0x1000),(0x06000000,0x20000)):uc.mem_map(address,size)
G,CACHE,ACTIVE,USED,STOP=0x02000000,0x02001000,0x02004000,0x02004100,0x087ff000
def put(a,f,*v):uc.mem_write(a,struct.pack('<'+f,*v))
def get(a,f='I'):return struct.unpack('<'+f,uc.mem_read(a,struct.calcsize('<'+f)))[0]
allocations=[];copies=[];mode='transfer'
def copy(source,dest,control,dma=False):
    width=4 if control&(1<<26) else 2
    count=control&(0xffff if dma else 0x1fffff)
    fixed=(control>>23)&3==2 if dma else bool(control&(1<<24))
    data=bytes(uc.mem_read(source,width if fixed else count*width))
    if fixed:data*=count
    uc.mem_write(dest,data);copies.append((source,dest,len(data)))
def hardware(machine,access,address,size,value,ctx):
    if address==0x040000dc and value&0x80000000:copy(get(0x040000d4),get(0x040000d8),value,True)
def hook(machine,address,size,ctx):
    if address==0x0800e0a8:
        allocations.append(machine.reg_read(UC_ARM_REG_R0));machine.reg_write(UC_ARM_REG_R0,160)
    elif address==0x080c5320:copy(machine.reg_read(UC_ARM_REG_R0),machine.reg_read(UC_ARM_REG_R1),machine.reg_read(UC_ARM_REG_R2))
    elif mode=='palette' and address in (0x08007c94,0x08006fcc,0x08007e24,0x080c532c,0x080074f0,0x08006e30):pass
    elif mode=='oam' and address in (0x0800e648,0x0800fc3c,0x0800fc58):pass
    else:return
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook);uc.hook_add(UC_HOOK_MEM_WRITE,hardware)
def call(address,args=()):
    for reg,arg in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(reg,arg)
    uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(address|1,STOP,count=30000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
for index,im in enumerate(assets.images):
    for players in (1,2):
        uc.mem_write(0x03000000,bytes(0x8000));uc.mem_write(CACHE,bytes(8000));put(0x03004010,'I',G);put(G+16,'B',players)
        put(0x03005568,'I',ACTIVE);put(0x03005560,'I',ACTIVE+2);put(0x03005584,'I',ACTIVE)
        put(0x03005580,'I',CACHE);put(0x03005578,'I',USED);put(ACTIVE,'H',index)
        allocations.clear();copies.clear();call(0x0800df80)
        payload=0x08000000+im.rom_offset+28+im.part_count*8
        assert allocations==[im.tile_count]
        assert (get(0x030054e0),get(0x030054e4),get(0x030054e8))==(payload,160,im.tile_count)
        assert get(0x03005588)==10000-im.tile_count*64
        assert get(CACHE+index*4,'B')==3 and get(CACHE+index*4+2,'H')==160
        assert get(0x0300557c,'H')==1 and get(USED,'H')==index
        call(0x0800e358)
        assert copies==[(payload,0x06012800,im.tile_count*64)]
        assert bytes(uc.mem_read(0x06012800,im.tile_count*64))==C.string_at(im.tiles,im.tile_count*64)
        assert get(0x030054e0)==0
        report['rom_transfer_chains']+=1
mode='oam'
for index,im in enumerate(assets.images):
    # Execute full FCE4 with one prepared non-affine, unflipped sprite. Only
    # linked-list preparation/clearing helpers are controlled. Every actual OAM
    # descriptor is produced by the ROM using the recovered image metadata.
    uc.mem_write(0x03000000,bytes(0x8000));uc.mem_write(G,bytes(48));uc.mem_write(CACHE,bytes(8000))
    output=0x02005000;uc.mem_write(output,b'\xa5'*1024)
    put(0x03005594,'I',output);put(0x030055d0,'I',G|0x80000000);put(0x03005580,'I',CACHE)
    put(G+4,'H',1);put(G+12,'ii',-im.min_x*256,-im.min_y*256)
    put(G+24,'HHIHH',index,index,5,0,0);put(G+40,'HH',4096,4096)
    put(CACHE+index*4,'BBH',3,0,160)
    call(0x0800fce4)
    expected=bytearray()
    for p in im.parts[:im.part_count]:
        a0=(p.attribute0&0xfe00)|((p.y-im.min_y)&255)|0x2000
        a1=(p.attribute1&0xfe00)|((p.x-im.min_x)&511)
        a2=((160+p.tile)*2)|0xc00
        expected.extend(struct.pack('<HHH',a0,a1,a2)+b'\xa5\xa5')
    assert bytes(uc.mem_read(output,len(expected)))==expected
    assert bytes(uc.mem_read(output+len(expected),1024-len(expected)))==b'\xa5'*(1024-len(expected))
    report['rom_oam_descriptors']+=im.part_count
mode='palette'
for level in range(16):
    family=lib.ssa_sprite_palette_family(level);assert family==(0 if level<=5 or level in (12,13) else 1)
    for players in (1,2):
        uc.mem_write(0x03000000,bytes(0x8000));put(0x03004010,'I',G);put(G+16,'B',players)
        copies.clear();call(0x08007014,(0x08454a5c+family*512,256,0x0879effc,256))
        assert bytes(uc.mem_read(0x05000000,512))==bytes(assets.background_palette[family])
        assert bytes(uc.mem_read(0x05000200,512))==bytes(assets.palette)
        report['rom_palette_setups']+=1
for level in (16,255,0xffffffff):assert lib.ssa_sprite_palette_family(level)==-1;report['native_guards']+=1

# Corrupt each image independently; failure never changes destination ownership.
mutated=C.create_string_buffer(ROM)
def reject(offset,data,size=len(ROM)):
    old=C.string_at(C.addressof(mutated)+offset,len(data));C.memmove(C.addressof(mutated)+offset,data,len(data))
    target=Assets();target.palette[7]=123;before=bytes(target)
    assert lib.ssa_sprite_assets_load(C.byref(target),mutated,size)==-1 and bytes(target)==before
    C.memmove(C.addressof(mutated)+offset,old,len(old));report['malformed_cases']+=1
for i,im in enumerate(assets.images):
    reject(0x44b710+i*4,struct.pack('<I',0x07000000))
    reject(im.rom_offset,b'AGB*')
    reject(im.rom_offset+4,b'\x00\x00')
    reject(im.rom_offset+6,b'\xff\xff')
    reject(im.rom_offset+28+4,b'\xff\xff')
    reject(im.rom_offset+28+6,b'\x00')
    reject(im.rom_offset+28,b'\x00\xc0')
for size in (0,1,0x44b710,0x44bbeb,0x79effc,0x79f1fb):reject(0,b'',size)
for args in ((None,mutated,len(ROM)),(C.byref(Assets()),None,len(ROM)),(C.byref(assets),mutated,len(ROM))):
    assert lib.ssa_sprite_assets_load(*args)==-1;report['native_guards']+=1
# Preserve real image pixels locally for visual inspection; no ROM-derived asset
# is copied into source directories or published.
for index in (0,12,150,310):
    im=assets.images[index];pix=rasters[index]
    rgb=bytearray()
    for value in pix:rgb.extend(rgba[value*4:value*4+3] if value else b'\x36\x3e\x48')
    (B/f'sprite-{index:03}.ppm').write_bytes(f'P6\n{im.width} {im.height}\n255\n'.encode()+rgb)
lib.ssa_sprite_assets_free(C.byref(assets));assert bytes(assets)==bytes(C.sizeof(Assets))
lib.ssa_sprite_assets_free(C.byref(assets));lib.ssa_sprite_assets_free(None)
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie','-I',str(ROOT/'include'),str(ROOT/'src/sprite_assets.c'),str(ROOT/'tests/test_sprite_assets.c'),'-o',str(B/'test_sprite_assets')],check=True)
environment=os.environ.copy();environment['ASAN_OPTIONS']='detect_leaks=0'
result=subprocess.run([str(B/'test_sprite_assets'),str(ROOT/'orig/SSA.gba')],env=environment,check=True,text=True,capture_output=True)
report['sanitizers']=json.loads(result.stdout)
report['source_sha256']=hashlib.sha256((ROOT/'src/sprite_assets.c').read_bytes()).hexdigest()
report['scope']='All311real sprite resources, complete originalDF80/E320/E358 transfer chains (allocator/BIOS/DMA controlled), fullFCE4 generating1261actualOAMdescriptors with one prepared non-affine sprite (listpreparation helpers controlled), original07014 palette writes (setup/hardwarewait callbacks controlled), exactpart/tiledata, independenttile rasterization, malformedownership/bounds checks. No world renderer or runtime OAM/scaling implementation.'
(ROOT/'analysis/native-sprite-assets-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='image_sha256'},indent=2))
