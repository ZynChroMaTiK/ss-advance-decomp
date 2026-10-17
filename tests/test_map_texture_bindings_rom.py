"""Whole B0C4/B0FC comparisons; real C57F4 executes, no instruction hooks."""
from pathlib import Path
import ctypes as C
import hashlib,json,os,random,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from inspect_maps import unpack_lz77
ROM=(ROOT/'orig/SSA.gba').read_bytes();SHA1=hashlib.sha1(ROM).hexdigest()
assert SHA1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
sources=[ROOT/'src'/s for s in ('map_assets.c','texture_assets.c','map_texture_bindings.c')]
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*map(str,sources),
 '-o',str(BUILD/'libmap_texture_bindings.so')],check=True)
lib=C.CDLL(str(BUILD/'libmap_texture_bindings.so'))
class Map(C.Structure):
 _fields_=[('data',C.POINTER(C.c_uint8)),('size',C.c_size_t)]+[(n+'_count',C.c_uint16) for n in
 ('thing','texture','sector','node','leaf','segment','side','line','vertex')]+[(n,C.c_uint32) for n in
 ('things','textures','sectors','nodes','leaves','segments','sides','lines','vertices','reject','blockmap','rom_offset')]
class Image(C.Structure):
 _fields_=[('name',C.c_uint8*8),('width',C.c_uint16),('height',C.c_uint16),('rom_offset',C.c_uint32),('pixels',C.c_void_p)]
class Archive(C.Structure):
 _fields_=[('rom_offset',C.c_uint32),('count',C.c_uint16),('images',C.POINTER(Image))]
class Family(C.Structure):
 _fields_=[('archives',Archive*3),('mip_indices',C.POINTER(C.c_uint16*3))]
class Assets(C.Structure):
 _fields_=[('families',Family*2),('loaded',C.c_uint)]
class Record(C.Structure):
 _fields_=[('name',C.c_uint8*8),('width',C.c_uint16),('height',C.c_uint16),('texture_index',C.c_int32)]
class Bindings(C.Structure):
 _fields_=[('map',C.POINTER(Map)),('assets',C.POINTER(Assets)),('records',C.POINTER(Record)),('count',C.c_uint16),('family',C.c_uint8)]
P=C.POINTER
load=lib.ssa_texture_assets_load;load.argtypes=[P(Assets),C.c_void_p,C.c_size_t]
free=lib.ssa_texture_assets_free;free.argtypes=[P(Assets)]
mapload=lib.ssa_map_assets_load;mapload.argtypes=[P(Map),C.c_void_p,C.c_size_t,C.c_uint]
mapfree=lib.ssa_map_assets_free;mapfree.argtypes=[P(Map)]
bind=lib.ssa_map_texture_bind_records;bind.argtypes=[P(Record),C.c_size_t,P(Archive)]
init=lib.ssa_map_texture_bindings_init;init.argtypes=[P(Bindings),P(Map),P(Assets),C.c_uint]
release=lib.ssa_map_texture_bindings_free;release.argtypes=[P(Bindings)]
record=lib.ssa_map_texture_record;record.argtypes=[P(Bindings),C.c_uint];record.restype=P(Record)
image=lib.ssa_map_texture_image;image.argtypes=[P(Bindings),C.c_uint,C.c_uint];image.restype=P(Image)
side=lib.ssa_map_side_texture;side.argtypes=[P(Bindings),C.c_uint,C.c_uint,P(P(Record))]
assetimage=lib.ssa_texture_assets_image;assetimage.argtypes=[P(Assets),C.c_uint,C.c_uint,C.c_uint];assetimage.restype=P(Image)
family_for=lib.ssa_texture_assets_family;family_for.argtypes=[C.c_uint]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x200000);uc.mem_map(0x03000000,0x10000)
MAP,AR,RECORDS,TEXTURES,STOP=0x02000000,0x02000100,0x02001000,0x02020000,0x087ff000
uc.mem_write(0x03004934,struct.pack('<I',AR))
rng=random.Random(0xb0c4);report={'rom_sha1':SHA1,'whole_rom_calls':0,'records_compared':0,'map_cases':[],
 'live_side_lookups':0,'image_lookups':0,'live_changes':0,'native_guards':0,
 'scope':'Whole original B0C4 including B0FC and real C57F4, no instruction hooks. Both archive families/all16 maps; bounded live wall-side lookup. No rasterizer or sector texture invention.'}
def oracle(data,archive):
 header=bytearray(0x48);struct.pack_into('<H',header,8,len(data)//16);struct.pack_into('<I',header,0x20,RECORDS)
 uc.mem_write(MAP,bytes(header));uc.mem_write(RECORDS,data or b'\0'*16)
 header=bytearray(0x48);struct.pack_into('<H',header,8,archive.count);struct.pack_into('<I',header,0x20,TEXTURES)
 uc.mem_write(AR,bytes(header));src=bytearray()
 for i in range(archive.count):
  im=archive.images[i];src+=bytes(im.name)+struct.pack('<HHI',im.width,im.height,im.rom_offset+0x08000000)
 uc.mem_write(TEXTURES,bytes(src) or b'\0'*16)
 for reg,value in ((UC_ARM_REG_R0,MAP),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):uc.reg_write(reg,value)
 uc.emu_start(0x0800b0c5,STOP,count=12000000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
 report['whole_rom_calls']+=1;report['records_compared']+=len(data)//16
 return bytes(uc.mem_read(RECORDS,len(data))) if data else b''
def compare(data,archive):
 arr=(Record*(len(data)//16)).from_buffer_copy(data)
 expected=oracle(data,archive);assert bind(arr,len(arr),C.byref(archive))==1
 assert bytes(arr)==expected,(data.hex(),bytes(arr).hex(),expected.hex())
 return expected
assets=Assets();assert load(C.byref(assets),ROM,len(ROM))==1
for level in range(16):
 m=Map();assert mapload(C.byref(m),ROM,len(ROM),level)==1
 before=C.string_at(m.data,m.size);raw=before[m.textures:m.textures+m.texture_count*16]
 for family in range(2):
  archive=assets.families[family].archives[0];expected=compare(raw,archive);b=Bindings()
  assert init(C.byref(b),C.byref(m),C.byref(assets),family)==1
  assert C.string_at(b.records,b.count*16)==expected and C.string_at(m.data,m.size)==before
  missing=sum(1 for i in range(b.count) if not b.records[i].width)
  report['map_cases'].append({'level':level,'family':family,'selected':family==family_for(level),'records':b.count,'missing_names':missing})
  for i in range(b.count):
   assert bytes(record(C.byref(b),i).contents)==expected[i*16:i*16+16]
   for mip in range(3):
    actual=image(C.byref(b),i,mip);wanted=assetimage(C.byref(assets),family,b.records[i].texture_index,mip)
    assert C.addressof(actual.contents)==C.addressof(wanted.contents);report['image_lookups']+=1
  for i in range(m.side_count):
   for layer in range(3):
    index=struct.unpack_from('<h',before,m.sides+12*i+4+2*layer)[0];out=P(Record)();result=side(C.byref(b),i,layer,C.byref(out))
    assert result==(0 if index==-1 else 1)
    if index==-1:assert not out
    else:
     assert C.addressof(out.contents)==C.addressof(b.records[index])
     if family==family_for(level):assert out.contents.width and out.contents.height
    report['live_side_lookups']+=1
  # Mutate only the caller-owned decoded side, as switches do. A lookup must
  # observe the new index and must not cache the original texture selection.
  for new in (0,m.texture_count-1,65535,32768,m.texture_count):
   pos=m.sides+6;saved=bytes(m.data[pos:pos+2]);m.data[pos]=new&255;m.data[pos+1]=new>>8
   out=C.cast(C.c_void_p(1),P(Record));result=side(C.byref(b),0,1,C.byref(out))
   if new==65535:assert result==0 and not out
   elif new>=32768 or new>=m.texture_count:assert result==-1 and C.cast(out,C.c_void_p).value==1;report['native_guards']+=1
   else:assert result==1 and C.addressof(out.contents)==C.addressof(b.records[new])
   m.data[pos],m.data[pos+1]=saved;report['live_changes']+=1
  snapshot=bytes(b);assert init(C.byref(b),C.byref(m),C.byref(assets),family)==-1 and bytes(b)==snapshot;report['native_guards']+=1
  assert not record(C.byref(b),m.texture_count) and not image(C.byref(b),0,3);report['native_guards']+=2
  old=b.records[0].texture_index;b.records[0].texture_index=-32768
  assert not image(C.byref(b),0,0);b.records[0].texture_index=old;report['native_guards']+=1
  out=P(Record)();assert side(C.byref(b),m.side_count,0,C.byref(out))==-1
  assert side(C.byref(b),0,3,C.byref(out))==-1;report['native_guards']+=2
  release(C.byref(b));assert not b.map and not b.records
 # Section bounds and overflow are rejected without publishing partial state.
 for attr,value in (('textures',m.size),('textures',0xffffffff),('texture_count',65535),('data',P(C.c_uint8)())):
  old=bytes(m);setattr(m,attr,value);b=Bindings();snapshot=bytes(b)
  assert init(C.byref(b),C.byref(m),C.byref(assets),family_for(level))==-1 and bytes(b)==snapshot
  C.memmove(C.byref(m),old,len(old));report['native_guards']+=1
 mapfree(C.byref(m))
print('All16 maps/both families complete binder and live side references passed',flush=True)
# Synthetic descriptors preserve bytes beyond NUL, first-match ties, original
# zero/non-power-of-two dimensions, and high-byte comparison behavior.
for case in range(1024):
 n=case%17;images=(Image*n)();a=Archive(0,n,images)
 names=[rng.randbytes(8) for _ in range(n)]
 if n:names[0]=b'Ab\0'+rng.randbytes(5)
 if n>1:names[1]=b'aB\0'+rng.randbytes(5)
 for i,name in enumerate(names):
  images[i].name[:]=name;images[i].width=rng.randrange(65536);images[i].height=rng.randrange(65536);images[i].rom_offset=0x123400+i*256
 data=bytearray()
 for i in range(case%19):
  name=names[rng.randrange(n)] if n and i%3 else rng.randbytes(8)
  if n and i%5==0:name=b'AB\0'+rng.randbytes(5)
  data+=name+struct.pack('<HHI',rng.randrange(65536),rng.randrange(65536),rng.randrange(2**32))
 compare(bytes(data),a)
# Signed16 archive indices are deliberately retained by the recovered binder;
# renderer-facing resolution rejects negative indices instead of dereferencing.
for target in (32767,32768,65534):
 images=(Image*(target+1))();a=Archive(0,target+1,images)
 for i in range(target+1):images[i].name[:]=b'NO_MATCH'
 images[target].name[:]=b'FOUND123';images[target].width=123;images[target].height=456
 expected=compare(b'FOUND123'+struct.pack('<HHI',9,10,0xffffffff),a)
 assert struct.unpack_from('<i',expected,12)[0]==C.c_int16(target).value
empty=Archive();assert bind(None,0,C.byref(empty))==1
rec=Record();snapshot=bytes(rec)
for args in ((None,1,C.byref(empty)),(C.byref(rec),65536,C.byref(empty)),(C.byref(rec),1,None)):
 assert bind(*args)==-1 and bytes(rec)==snapshot;report['native_guards']+=1
bad=Archive(0,1,None);assert bind(C.byref(rec),1,C.byref(bad))==-1 and bytes(rec)==snapshot;report['native_guards']+=1
free(C.byref(assets));report['source_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined',
 '-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_map_texture_bindings.c'),
 *map(str,sources),str(ROOT/'src/switches.c'),'-o',str(BUILD/'test_map_texture_bindings')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
result=subprocess.run([str(BUILD/'test_map_texture_bindings'),str(ROOT/'orig/SSA.gba')],capture_output=True,text=True,env=env)
if result.returncode:print(result.stdout);print(result.stderr);result.check_returncode()
report['sanitizer_integration']=json.loads(result.stdout)
(ROOT/'analysis/native-map-texture-bindings-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('map_cases','source_sha256')},indent=2))
