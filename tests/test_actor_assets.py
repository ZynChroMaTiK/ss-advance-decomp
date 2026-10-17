"""Owned actor RLE assets and complete09238/09258/ARM0638 pixel differential."""
from pathlib import Path
import ctypes as C,struct,hashlib,json,subprocess,random
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
class Image(C.Structure):
 _fields_=[('rom_offset',C.c_uint32),('origin_x',C.c_int16),('origin_y',C.c_int16),('width',C.c_uint16),('height',C.c_uint16),('pixels',C.POINTER(C.c_uint8))]
class Resource(C.Structure):
 _fields_=[('table_offset',C.c_uint32),('animation_offset',C.c_uint32),('count',C.c_uint16),('directions',C.c_uint16),('mip_flags',C.c_uint16),('frames',C.c_uint8),('images',C.POINTER(Image))]
class Assets(C.Structure):
 _fields_=[('resources',Resource*142),('reciprocal',C.c_uint32*65536),('closed_top',C.c_uint8*121),('closed_bottom',C.c_uint8*121),('loaded',C.c_uint)]
class Projection(C.Structure):
 _fields_=[('reserved',C.c_uint32),('x',C.c_int16),('y',C.c_int16),('width',C.c_int16),('height',C.c_int16),('frame_flags',C.c_uint32),('scale',C.c_int32),('world_x',C.c_int16),('world_y',C.c_int16),('bottom_z',C.c_int16),('top_z',C.c_int16),('animation',C.c_uint16),('clip_offset',C.c_uint16),('left',C.c_int16),('clipped_width',C.c_int16)]
assert C.sizeof(Projection)==36
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/actor_assets.c'),str(ROOT/'src/actor_raster.c'),'-o',str(B/'libssa_actor_assets.so')],check=True)
lib=C.CDLL(str(B/'libssa_actor_assets.so'))
lib.ssa_actor_assets_load.argtypes=[C.POINTER(Assets),C.c_void_p,C.c_size_t]
lib.ssa_actor_assets_free.argtypes=[C.POINTER(Assets)]
lib.ssa_actor_assets_image.argtypes=[C.POINTER(Assets),C.c_uint,C.c_uint32];lib.ssa_actor_assets_image.restype=C.POINTER(Image)
lib.ssa_actor_image_indices.argtypes=[C.POINTER(Image),C.c_void_p,C.c_size_t,C.c_size_t]
lib.ssa_actor_raster_draw.argtypes=[C.POINTER(Assets),C.POINTER(Projection),C.POINTER(C.c_int16),C.c_size_t,C.c_void_p,C.c_size_t]
a=Assets();raw=C.create_string_buffer(ROM);assert lib.ssa_actor_assets_load(C.byref(a),raw,len(ROM))==1
del raw
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'resources':142,'nonempty_resources':0,'image_references':0,'unique_images':0,'decoded_reference_bytes':0,'columns':0,'whole_rom_draws':0,'metadata_rom_cases':0,'malformed_cases':0,'guards':0}
images=[];unique={};raw_hash=hashlib.sha256()
for g,r in enumerate(a.resources):
 table,anim,directions,flags=struct.unpack_from('<IIHH',ROM,0x449d2c+12*g)
 assert (r.animation_offset,r.directions,r.mip_flags)==(anim-0x8000000,directions,flags)
 if not table:assert not r.count and not r.images;continue
 count=struct.unpack_from('<I',ROM,table-0x8000000)[0]
 assert (r.table_offset,r.count,r.frames)==(table-0x8000000,count,(count//directions//flags.bit_count())&255)
 report['nonempty_resources']+=1
 for i,im in enumerate(r.images[:count],1):
  q=struct.unpack_from('<I',ROM,table-0x8000000+4*i)[0]-0x8000000
  assert im.rom_offset==q
  assert (im.origin_x,im.origin_y,im.width,im.height)==struct.unpack_from('<hhHH',ROM,q+24)
  expanded=[]
  for x in range(im.width):
   pos=q+32+2*im.width+struct.unpack_from('<H',ROM,q+32+2*x)[0];column=[]
   while len(column)<im.height:
    packet=ROM[pos];pos+=1;n=packet&127
    if packet&128:column.extend([0]*n)
    else:column.extend(ROM[pos:pos+n]);pos+=n
   assert len(column)==im.height;expanded+=column
  assert C.string_at(im.pixels,im.width*im.height)==bytes(expanded)
  out=C.create_string_buffer(b'\xa5'*((im.width+3)*im.height))
  assert lib.ssa_actor_image_indices(C.byref(im),out,im.width+3,len(out))==1
  for y in range(im.height):
   assert out.raw[y*(im.width+3):y*(im.width+3)+im.width]==bytes(expanded[x*im.height+y] for x in range(im.width))
   assert out.raw[y*(im.width+3)+im.width:(y+1)*(im.width+3)]==b'\xa5'*3
  report['image_references']+=1;report['decoded_reference_bytes']+=len(expanded);report['columns']+=im.width
  images.append((g,i,im));unique.setdefault(q,(g,i,im));raw_hash.update(bytes(expanded))
report['unique_images']=len(unique);report['decoded_sha256']=raw_hash.hexdigest()
assert bytes(a.closed_top)==ROM[0x44a9fc:0x44a9fc+121] and bytes(a.closed_bottom)==ROM[0x44aa74:0x44aa74+121]
assert bytes(a.reciprocal)==ROM[0x34934:0x34934+65536*4]

uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000)
uc.mem_write(0x03000110,ROM[0x638:0x798])
FB,PROJ,CLIPS,STOP=0x02000000,0x02008000,0x02009000,0x087ff000
def put(p,f,*v):uc.mem_write(p,struct.pack('<'+f,*v))
def hook(u,pc,size,data):
 if pc==0x080033d8:u.reg_write(UC_ARM_REG_R0,0x0200a000);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
allocation_hook=uc.hook_add(UC_HOOK_CODE,hook)
def run(addr,r0,r1=0):
 uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.reg_write(UC_ARM_REG_R0,r0);uc.reg_write(UC_ARM_REG_R1,r1)
 uc.emu_start(addr|1,STOP,count=4000000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(uc.reg_read(UC_ARM_REG_PC))
uc.mem_write(0x0200a000,b'\0'*142);run(0x08000890,0)
assert bytes(uc.mem_read(0x0200a000,142))==bytes(r.frames for r in a.resources);report['metadata_rom_cases']+=1
uc.hook_del(allocation_hook)
put(0x030000c0,'I',FB);put(0x03004754,'I',PROJ)
rng=random.Random(0x9238)
def check(g,i,im,scale,mirror,x,y,variant):
 dx=(a.reciprocal[4096] if scale==65536 else a.reciprocal[scale] if scale<65536 else 0)>>8
 ys=6144 if scale==65536 else scale+scale//2
 dy=(a.reciprocal[ys] if ys<65536 else 0)>>8
 w=min(200,im.width*4096//dx) if dx else 120
 h=min(200,im.height*4096//dy) if dy else 120
 left=max(0,x);width=min(120,x+w)-left
 if width<=0 or y+h<=0:return
 p=Projection(x=x,y=y,width=w,height=h,frame_flags=i|(0x80000000 if mirror else 0),scale=scale,animation=g,left=left,clipped_width=width)
 clips=(C.c_int16*(width*2))()
 for col in range(width):
  clips[2*col]=rng.randrange(-10,65) if variant else 0
  clips[2*col+1]=rng.randrange(55,130) if variant else 120
 before=bytes((j*13+variant*21)&255 for j in range(14400));out=C.create_string_buffer(before)
 result=lib.ssa_actor_raster_draw(C.byref(a),C.byref(p),clips,len(clips),out,14400)
 assert result==1,(g,i,scale,mirror,x,y,w,h,result)
 uc.mem_write(FB,before);uc.mem_write(PROJ,bytes(p));uc.mem_write(CLIPS,bytes(clips))
 run(0x08009238,0,CLIPS)
 expected=bytes(uc.mem_read(FB,14400))
 assert out.raw[:14400]==expected,(g,i,scale,mirror,x,y,next((j for j in range(14400) if out.raw[j]!=expected[j]),None))
 report['whole_rom_draws']+=1
for g,i,im in images:check(g,i,im,4096,False,7,3,0)
for g,i,im in unique.values():
 for scale,mirror,x,y in ((2048,True,-2,-3),(8192,False,-7,-11),(65536,True,20,8)):
  check(g,i,im,scale,mirror,x,y,1)
for n in range(1024):
 g,i,im=rng.choice(images);check(g,i,im,rng.choice([1024,3072,4096,5461,32768,65535,65537,100000]),bool(n&1),rng.randrange(-15,60),rng.randrange(-15,50),n&1)
for g,i,im in list(unique.values())[:32]:check(g,i|0x40000000,im,4096,True,0,0,1)

# Every real header rejects a truncated header without replacing existing owned state.
original=C.string_at(C.byref(a),C.sizeof(a));source=C.create_string_buffer(ROM)
for q in unique:
 assert lib.ssa_actor_assets_load(C.byref(a),source,q+31)==-1
 assert C.string_at(C.byref(a),C.sizeof(a))==original;report['malformed_cases']+=1
# Patch the FIRST referenced image to force all packet and descriptor guards early.
first=a.resources[4].images[0];q=first.rom_offset;base=q+32+first.width*2
packet=base+struct.unpack_from('<H',ROM,q+32)[0]
mutations=[(0x449d2c+4*12,struct.pack('<I',0x07000000)),(a.resources[4].table_offset,struct.pack('<I',3001)),(0x449d2c+4*12+8,b'\0\0'),(0x449d2c+4*12+10,b'\0\0'),(q,b'bad!'),(q+28,b'\0\0'),(q+30,struct.pack('<H',1025)),(packet,b'\0'),(packet,b'\x80'),(packet,b'\xff')]
for offset,data in mutations:
 changed=bytearray(ROM);changed[offset:offset+len(data)]=data;buf=C.create_string_buffer(bytes(changed))
 assert lib.ssa_actor_assets_load(C.byref(a),buf,len(changed))==-1,(offset,data)
 assert C.string_at(C.byref(a),C.sizeof(a))==original;report['malformed_cases']+=1
p=Projection(x=0,y=0,width=10,height=10,frame_flags=1,scale=4096,animation=4,left=0,clipped_width=10)
clips=(C.c_int16*20)(*([0,120]*10));out=C.create_string_buffer(b'\x55'*14400)
for field,value in [('left',-1),('left',121),('clipped_width',121),('height',-1),('animation',142),('frame_flags',3001),('x',-32768),('scale',0)]:
 bad=Projection.from_buffer_copy(p);setattr(bad,field,value)
 assert lib.ssa_actor_raster_draw(C.byref(a),C.byref(bad),clips,20,out,14400)==-1,(field,value)
 assert out.raw[:14400]==b'\x55'*14400;report['guards']+=1
for capacity,clipcount in [(14399,20),(14400,19)]:
 assert lib.ssa_actor_raster_draw(C.byref(a),C.byref(p),clips,clipcount,out,capacity)==-1
 assert out.raw[:14400]==b'\x55'*14400;report['guards']+=1
# A final bad source column must not leave earlier valid columns drawn.
bad=Projection.from_buffer_copy(p);bad.clipped_width=first.width+2;bad.width=bad.clipped_width
wideclips=(C.c_int16*(2*bad.clipped_width))(*([0,120]*bad.clipped_width))
assert lib.ssa_actor_raster_draw(C.byref(a),C.byref(bad),wideclips,len(wideclips),out,14400)==-1
assert out.raw[:14400]==b'\x55'*14400;report['guards']+=1
# Previews contain only decoded original artwork; never synthesized pixels.
import zlib
def png_rgba(path,width,height,pixels):
 def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
 rows=b''.join(b'\0'+pixels[y*width*4:(y+1)*width*4] for y in range(height))
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
palette=struct.unpack_from('<256H',ROM,0x454a5c)
for resource,index in [(4,1),(101,1),(101,3),(102,1),(106,1)]:
 im=a.resources[resource].images[index-1];pixels=C.string_at(im.pixels,im.width*im.height);rgba=[]
 for y in range(im.height):
  for x in range(im.width):
   v=pixels[x*im.height+y];color=palette[v]
   rgba.append(tuple((((color>>(5*k))&31)*255//31) for k in range(3))+(255 if v else 0,))
 enlarged=b''.join(bytes(rgba[(y//4)*im.width+x//4]) for y in range(im.height*4) for x in range(im.width*4))
 png_rgba(B/f'actor-{resource:03d}-{index:03d}.png',im.width*4,im.height*4,enlarged)
lib.ssa_actor_assets_free(C.byref(a));lib.ssa_actor_assets_free(C.byref(a));assert not a.loaded
import os
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'src/actor_assets.c'),str(ROOT/'src/actor_raster.c'),str(ROOT/'tests/test_actor_assets.c'),'-o',str(B/'test_actor_assets')],check=True)
sanitized=subprocess.run([str(B/'test_actor_assets'),str(ROOT/'orig/SSA.gba')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True,check=True)
report.update(json.loads(sanitized.stdout))
report['source_sha256']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ('src/actor_assets.c','src/actor_raster.c','include/ssa/actor_assets.h','include/ssa/actor_raster.h','tests/test_actor_assets.py','tests/test_actor_assets.c')}
(ROOT/'analysis/native-actor-assets-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
