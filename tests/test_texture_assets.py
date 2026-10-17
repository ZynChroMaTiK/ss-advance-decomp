"""WOD0 data recovery versus complete original AFF4 and AF18 execution."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,os,random,zlib
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(exist_ok=True,parents=True)
class Image(C.Structure):
    _fields_=[('name',C.c_char*8),('width',C.c_uint16),('height',C.c_uint16),('rom_offset',C.c_uint32),('pixels',C.POINTER(C.c_uint8))]
class Archive(C.Structure):
    _fields_=[('rom_offset',C.c_uint32),('count',C.c_uint16),('images',C.POINTER(Image))]
class Family(C.Structure):
    _fields_=[('archives',Archive*3),('mip_indices',C.POINTER(C.c_uint16*3))]
class Assets(C.Structure):
    _fields_=[('families',Family*2),('loaded',C.c_uint)]
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/texture_assets.c'),'-o',str(B/'libssa_texture_assets.so')],check=True)
lib=C.CDLL(str(B/'libssa_texture_assets.so'))
lib.ssa_texture_assets_load.argtypes=[C.POINTER(Assets),C.c_void_p,C.c_size_t]
lib.ssa_texture_assets_free.argtypes=[C.POINTER(Assets)]
lib.ssa_texture_assets_image.argtypes=[C.POINTER(Assets),C.c_uint,C.c_uint,C.c_uint];lib.ssa_texture_assets_image.restype=C.POINTER(Image)
lib.ssa_texture_archive_find.argtypes=[C.POINTER(Archive),C.c_char_p]
lib.ssa_texture_image_indices.argtypes=[C.POINTER(Image),C.c_void_p,C.c_size_t,C.c_size_t]
def load(data):
    a=Assets();buf=C.create_string_buffer(bytes(data));assert lib.ssa_texture_assets_load(C.byref(a),buf,len(data))==1;return a
a=load(ROM)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'archives':6,'images':0,'pixel_bytes':0,'rom_archive_loads':0,'rom_mip_joins':0,'rom_name_comparisons':0,'rom_column_copy_blocks':0,'malformed_cases':0,'guards':0,'images_sha256':[]}
def u16(data,at):return struct.unpack_from('<H',data,at)[0]
def u32(data,at):return struct.unpack_from('<I',data,at)[0]
for f in range(2):
    for m,ar in enumerate(a.families[f].archives):
        off=u32(ROM,0x44aef4+4*(16+3*f+m))-0x8000000;records=u32(ROM,off+32)
        assert ar.rom_offset==off and ar.count==u16(ROM,off+8)
        for i,im in enumerate(ar.images[:ar.count]):
            name,w,h,p=struct.unpack_from('<8sHHI',ROM,off+records+16*i)
            assert C.string_at(C.addressof(im),8)==name and (im.width,im.height,im.rom_offset)==(w,h,off+p)
            payload=C.string_at(im.pixels,w*h);assert payload==ROM[off+p:off+p+w*h]
            found=lib.ssa_texture_archive_find(C.byref(ar),name);assert found==i
            selected=lib.ssa_texture_assets_image(C.byref(a),f,i,m).contents
            assert selected.rom_offset==im.rom_offset
            report['images']+=1;report['pixel_bytes']+=w*h
            report['images_sha256'].append({'family':f,'mip':m,'index':i,'name':name.rstrip(b'\0').decode('ascii'),'width':w,'height':h,'sha256':hashlib.sha256(payload).hexdigest()})

uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
for address,size in ((0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x10000)):uc.mem_map(address,size)
STOP=0x087ff000;heap=0x02010000;alloc=[]
def put(at,fmt,*v):uc.mem_write(at,struct.pack('<'+fmt,*v))
def get(at,fmt='I'):return struct.unpack('<'+fmt,uc.mem_read(at,struct.calcsize('<'+fmt)))[0]
def copy(src,dst,control):
    width=4 if control&0x4000000 else 2;count=control&0x1fffff
    uc.mem_write(dst,bytes(uc.mem_read(src,width*count)))
def hook(machine,address,size,ctx):
    global heap
    if address==0x080033d8:
        n=machine.reg_read(UC_ARM_REG_R0);alloc.append((heap,n));machine.reg_write(UC_ARM_REG_R0,heap);heap+=(n+3)&~3
    elif address==0x080c5320:copy(machine.reg_read(UC_ARM_REG_R0),machine.reg_read(UC_ARM_REG_R1),machine.reg_read(UC_ARM_REG_R2))
    else:return
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
def dma(machine,access,address,size,value,ctx):
    if address==0x040000dc and value&0x80000000:copy(get(0x040000d4),get(0x040000d8),value&0x7fffffff)
uc.hook_add(UC_HOOK_CODE,hook);uc.hook_add(UC_HOOK_MEM_WRITE,dma)
def call(address,args=(),count=10000000):
    for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2),args):uc.reg_write(reg,v)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(address|1,STOP,count=count);assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
    return uc.reg_read(UC_ARM_REG_R0)
def chain(data,assets,f,players):
    global heap
    uc.mem_write(0x08000000,bytes(data));heap=0x02010000;alloc.clear();put(0x03004010,'I',0x02000000);put(0x02000010,'B',players)
    for m,ar in enumerate(assets.families[f].archives):
        addr=call(0x0800aff4,(16+f*3+m,));assert addr
        put(0x03004934+4*m,'I',addr)
        raw=bytearray(data[ar.rom_offset:ar.rom_offset+72]);struct.pack_into('<I',raw,32,get(addr+32))
        assert bytes(uc.mem_read(addr,72))==raw
        for i,im in enumerate(ar.images[:ar.count]):
            expected=C.string_at(C.addressof(im),8)+struct.pack('<HHI',im.width,im.height,0x08000000+im.rom_offset)
            assert bytes(uc.mem_read(get(addr+32)+i*16,16))==expected
        report['rom_archive_loads']+=1
    call(0x0800af18)
    triples=get(0x03004940)
    assert alloc[-1][1]==assets.families[f].archives[0].count*12
    for i in range(assets.families[f].archives[0].count):
        for m in range(3):
            im=lib.ssa_texture_assets_image(C.byref(assets),f,i,m).contents
            assert get(triples+i*12+m*4)==0x08000000+im.rom_offset
    report['rom_mip_joins']+=1
for f in range(2):
    for players in (1,2):chain(ROM,a,f,players)

# Bounded real renderer block, NOT a claim of complete BE60 recovery: execute
# C9DA..CA4A with selected mip, column, base height and actual mip pointers.
# Both CpuSet and DMA paths copy exactly one contiguous vertical column.
for f in range(2):
    for m,ar in enumerate(a.families[f].archives):
        for i,im in enumerate(ar.images[:ar.count]):
            stride=im.width+3;out=C.create_string_buffer(b'\xa5'*(stride*im.height))
            assert lib.ssa_texture_image_indices(C.byref(im),out,stride,len(out))==1
            for x in range(im.width):
                for players in (1,2):
                    put(0x02000010,'B',players);put(0x03007e68,'I',m)
                    put(0x03005080+0x418+4*m,'I',0x08000000+im.rom_offset)
                    uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_R8,x)
                    uc.reg_write(UC_ARM_REG_R9,a.families[f].archives[0].images[i].height)
                    uc.reg_write(UC_ARM_REG_R12,0x03005080)
                    uc.mem_write(0x03005318,b'\xa5'*130)
                    uc.emu_start(0x0800c9db,0x0800ca4a,count=1000)
                    assert uc.reg_read(UC_ARM_REG_PC)==0x0800ca4a
                    column=bytes(uc.mem_read(0x03005318,im.height))
                    assert column==bytes(out.raw[y*stride+x] for y in range(im.height))
                    assert bytes(uc.mem_read(0x03005318+im.height,2))==b'\xa5'*2
                    report['rom_column_copy_blocks']+=1
            for y in range(im.height):assert out.raw[y*stride+im.width:(y+1)*stride]==b'\xa5'*3
            before=out.raw
            for bad_stride,capacity in ((im.width-1,len(out)),(im.width,im.width*im.height-1),(0,0),(2**64-1,2**64-1)):
                assert lib.ssa_texture_image_indices(C.byref(im),out,bad_stride,capacity)==-1 and before==out.raw
                report['guards']+=1

# Rearranged, mixed-case, duplicate and missing lower mip records. Actual string
# comparator and its full scan execute, including AF18's missing-name strncpy.
rng=random.Random(0xaff4)
for case in range(24):
    data=bytearray(ROM);f=case%2
    for m in (1,2):
        ar=a.families[f].archives[m];start=ar.rom_offset+u32(data,ar.rom_offset+32)
        rows=[bytearray(data[start+i*16:start+(i+1)*16]) for i in range(ar.count)]
        rng.shuffle(rows)
        for i,row in enumerate(rows):
            for k in range(8):
                if 65<=row[k]<=90 and (i+k+case)%2:row[k]+=32
            if i%17==case%17:row[:8]=b'MISSING!'
        rows[1]=rows[0][:] # first match must win
        data[start:start+ar.count*16]=b''.join(rows)
    modified=load(data);chain(data,modified,f,1+case%2);lib.ssa_texture_assets_free(C.byref(modified))

# Exhaustive one-byte name relation proves ASCII fold and all high-byte cases,
# plus embedded NUL and fixed-length names. No comparator stub is installed.
uc.mem_write(0x08000000,ROM);im=(Image*1)();ar=Archive(0,1,im)
for x in range(256):
    left=bytes([x])+b'abcdeFG';C.memmove(C.addressof(im),left,8);uc.mem_write(0x02001000,left)
    for y in range(256):
        right=bytes([y])+b'abcdeFG';uc.mem_write(0x02001010,right)
        equal=call(0x080c57f4,(0x02001000,0x02001010,8),1000)==0
        assert (lib.ssa_texture_archive_find(C.byref(ar),right)==0)==equal
        report['rom_name_comparisons']+=1
for left,right in ((b'AB\0xxxxx',b'ab\0YYYYY'),(b'ABCDEFGH',b'abcdefgh'),(b'ABCDEFGH',b'abcdefg!')):
    C.memmove(C.addressof(im),left,8);uc.mem_write(0x02001000,left);uc.mem_write(0x02001010,right)
    assert (lib.ssa_texture_archive_find(C.byref(ar),right)==0)==(call(0x080c57f4,(0x02001000,0x02001010,8))==0)
    report['rom_name_comparisons']+=1

def malformed(data,size=None):
    out=Assets();out.loaded=0;out.families[0].archives[0].rom_offset=0xdeadbeef;before=bytes(out)
    buf=C.create_string_buffer(bytes(data));assert lib.ssa_texture_assets_load(C.byref(out),buf,len(data) if size is None else size)==-1
    assert bytes(out)==before;report['malformed_cases']+=1
for resource in range(16,22):
    off=u32(ROM,0x44aef4+resource*4)-0x08000000;table=off+u32(ROM,off+32)
    for at,fmt,v in ((0x44aef4+resource*4,'I',0),(0x44aef4+resource*4,'I',0xffffffff),(off,'I',0),(off+8,'H',0),(off+8,'H',1025),(off+32,'I',0xffffffff),(off+32,'I',0)):
        data=bytearray(ROM);struct.pack_into('<'+fmt,data,at,v);malformed(data)
    for i in range(u16(ROM,off+8)):
        for delta,fmt,v in ((8,'H',0),(10,'H',1025),(12,'I',0),(12,'I',0xffffffff)):
            data=bytearray(ROM);struct.pack_into('<'+fmt,data,table+16*i+delta,v);malformed(data)
    malformed(ROM,table+u16(ROM,off+8)*16-1)
for n in (0,1,0x44aef4,0x44aef4+87):malformed(ROM,n)
# Deliberately overlapping repeated payloads may not defeat the aggregate cap.
data=bytearray(ROM);off=a.families[0].archives[0].rom_offset;table=off+u32(data,off+32)
struct.pack_into('<H',data,off+8,1024)
data[table:table+16384]=struct.pack('<8sHHI',b'BIG     ',1024,512,72)*1024
malformed(data)
for f,i,m in ((2,0,0),(0,101,0),(1,121,0),(0,0,3),(0xffffffff,0,0)):
    assert not lib.ssa_texture_assets_image(C.byref(a),f,i,m);report['guards']+=1
buf=C.create_string_buffer(ROM);before=bytes(a)
assert lib.ssa_texture_assets_load(C.byref(a),buf,len(ROM))==-1 and bytes(a)==before;report['guards']+=1
for level in range(16):assert lib.ssa_texture_assets_family(level)==(0 if level<=5 or level in (12,13) else 1)
assert lib.ssa_texture_assets_family(16)==-1;report['guards']+=1
# Ignored local preview; source artwork remains in the user's ROM. No derived
# assets enter source directories. Preview uses the verified level palette.
def png_chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
for f in range(2):
    im=a.families[f].archives[0].images[0];raw=C.create_string_buffer(im.width*im.height)
    assert lib.ssa_texture_image_indices(C.byref(im),raw,im.width,len(raw))==1
    palette=struct.unpack_from('<256H',ROM,0x454a5c+512*f)
    def rgb(index):
        color=palette[index] if index else 0
        return bytes((((color>>shift)&31)<<3)|(((color>>shift)&31)>>2) for shift in (0,5,10))
    rows=b''.join(b'\0'+b''.join(rgb(v) for v in raw.raw[y*im.width:(y+1)*im.width]) for y in range(im.height))
    png=b'\x89PNG\r\n\x1a\n'+png_chunk(b'IHDR',struct.pack('>IIBBBBB',im.width,im.height,8,2,0,0,0))+png_chunk(b'IDAT',zlib.compress(rows))+png_chunk(b'IEND',b'')
    (B/f'texture-family{f}-000.png').write_bytes(png)
lib.ssa_texture_assets_free(C.byref(a));assert bytes(a)==bytes(Assets())
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'src/texture_assets.c'),str(ROOT/'tests/test_texture_assets.c'),'-o',str(B/'test_texture_assets')],check=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0');result=subprocess.run([str(B/'test_texture_assets'),str(ROOT/'orig/SSA.gba')],check=True,text=True,capture_output=True,env=env)
report.update(json.loads(result.stdout));report['source_sha256']=hashlib.sha256((ROOT/'src/texture_assets.c').read_bytes()).hexdigest()
(ROOT/'analysis/native-texture-assets-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='images_sha256'},indent=2))
