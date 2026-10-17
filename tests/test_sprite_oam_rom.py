"""Whole FCE4, E648, FC3C/FC58 and actual math; no controlled callees."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess,os
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
def classes(path,names):
 nodes=[n for n in ast.parse((ROOT/path).read_text()).body if isinstance(n,ast.ClassDef) and n.name in names]
 exec(compile(ast.Module(nodes,type_ignores=[]),path,'exec'),globals())
classes('tests/test_sprite_assets.py',['Part','Image','Assets'])
classes('tests/test_weapon_animation_rom.py',['Object']);OP=C.POINTER(Object)
classes('tests/test_sprite_dispatch_rom.py',['Dispatch'])
class Math(C.Structure):_fields_=[('sine',C.c_uint16*16384),('direction',C.c_uint16*32769)]
class Cache(C.Structure):_fields_=[('resident',C.c_uint8),('pending',C.c_uint8),('tile',C.c_uint16)]
class Entry(C.Structure):_fields_=[('a0',C.c_uint16),('a1',C.c_uint16),('a2',C.c_uint16),('affine',C.c_uint16)]
class Oam(C.Structure):
 _fields_=[('entries',Entry*128),('heads',C.c_uint32*17),('buckets',C.c_uint8*16),('count',C.c_uint16),('affine_count',C.c_uint16),('local_index',C.c_uint16),('mosaic',C.c_uint16),('mosaic_register',C.c_uint16),('counter',C.c_uint32),('flags',C.c_uint8),('transfer_flag',C.c_uint8),('loaded',C.c_uint),('updating',C.c_uint)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['sprite_oam.c','sprite_assets.c','movement.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'libssa_sprite_oam.so')],check=True)
lib=C.CDLL(str(B/'libssa_sprite_oam.so'))
lib.ssa_sprite_assets_load.argtypes=[C.POINTER(Assets),C.c_void_p,C.c_size_t]
lib.ssa_sprite_assets_free.argtypes=[C.POINTER(Assets)]
lib.ssa_math_tables_load.argtypes=[C.POINTER(Math),C.c_void_p,C.c_size_t]
lib.ssa_sprite_oam_init.argtypes=[C.POINTER(Oam),C.c_void_p,C.c_size_t]
lib.ssa_sprite_oam_build.argtypes=[C.POINTER(Oam),C.POINTER(Dispatch),C.POINTER(Assets),C.POINTER(Cache),C.POINTER(Math)]
assets=Assets();math=Math();o=Oam();assert lib.ssa_sprite_assets_load(C.byref(assets),ROM,len(ROM))==1
assert lib.ssa_math_tables_load(C.byref(math),ROM,len(ROM))==1
assert lib.ssa_sprite_oam_init(C.byref(o),ROM,len(ROM))==1
objects=(Object*84)();dispatch=Dispatch();cache=(Cache*311)()
for i in range(84):dispatch.objects[i]=C.pointer(objects[i])
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
for p,n in [(0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x10000)]:uc.mem_map(p,n)
uc.mem_write(0x030000f0,ROM[0x370:0x380])
E,A,CACHE,OUT,STOP=0x02000000,0x02002000,0x02003000,0x02004000,0x087ff000
def put(p,f,*v):uc.mem_write(p,struct.pack('<'+f,*v))
def get(p,f='I'):return struct.unpack('<'+f,uc.mem_read(p,struct.calcsize('<'+f)))[0]
counts=[]
def done(u,pc,size,ctx):
 sp=u.reg_read(UC_ARM_REG_SP);counts[:]=[get(sp+0x2c),get(sp+0x24)]
uc.hook_add(UC_HOOK_CODE,done,begin=0x0801019c,end=0x0801019c)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_rom_frames':0,'oam_entries':0,'affine_objects':0,'guards':0,'images':311}
rng=random.Random(0xfce4)
def norm(v):
 if not v:return 0
 if v&0x80000000:return 0x80000000|(((v&0x7fffffff)-E)//48+1)
 assert 0x030055d0<=v<=0x03005610,hex(v)
 return (v-0x030055d0)//4+1
def compare():
 original=bytes(objects);uc.mem_write(E,original);uc.mem_write(A,bytes(dispatch.active));uc.mem_write(CACHE,bytes(cache))
 for p,v in [(0x03005594,OUT),(0x0300559c,E),(0x030055c0,A),(0x030055c4,dispatch.active_count),(0x03005580,CACHE),(0x030055b0,o.counter)]:put(p,'I',v)
 put(0x0300464e,'H',o.local_index);put(0x03005590,'H',o.mosaic);put(0x03005614,'B',o.flags);put(0x03005618,'B',o.transfer_flag)
 uc.mem_write(OUT,b'\xa5'*1024)
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(0x0800fce5,STOP,count=2000000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert lib.ssa_sprite_oam_build(C.byref(o),C.byref(dispatch),C.byref(assets),cache,C.byref(math))==1
 expected=bytes(uc.mem_read(OUT,1024))
 assert bytes(o.entries)==expected,(report['whole_rom_frames'],next((i for i in range(1024) if bytes(o.entries)[i]!=expected[i]),None),[(objects[dispatch.active[j]].scale_x,objects[dispatch.active[j]].scale_y,objects[dispatch.active[j]].unknown2c) for j in range(dispatch.active_count)])
 assert [o.count,o.affine_count]==counts
 assert (o.mosaic_register,o.flags,o.transfer_flag)==(get(0x0400004c,'H'),get(0x03005614,'B'),get(0x03005618,'B'))
 assert list(o.heads)==[norm(get(0x030055d0+4*i)) for i in range(17)]
 raw=bytearray(uc.mem_read(E,84*48))
 for slot in dispatch.active[:dispatch.active_count]:struct.pack_into('<I',raw,slot*48,norm(struct.unpack_from('<I',raw,slot*48)[0]))
 assert bytes(objects)==raw,(report['whole_rom_frames'],'objects')
 report['whole_rom_frames']+=1;report['oam_entries']+=o.count;report['affine_objects']+=o.affine_count
def setup(image=0):
 C.memset(objects,0,C.sizeof(objects));C.memset(cache,0,C.sizeof(cache));dispatch.active_count=1;dispatch.active[0]=0
 p=objects[0];im=assets.images[image];p.previous_frame=image;p.frame=310;p.animation=0;p.flags=5;p.x=-im.min_x*256;p.y=-im.min_y*256
 p.scale_x=4096;p.scale_y=4096;o.counter=0;o.local_index=0;o.mosaic=0;o.flags=0xa5;o.transfer_flag=7
 for c in cache:c.resident=3;c.pending=29;c.tile=160
 return p
for image in range(311):
 for flip in [0,0x4000,0x8000,0xc000]:
  p=setup(image);p.previous_frame|=flip;compare()
 for sx,sy,rot in [(2048,3072,0),(8192,4096,0),(4096,4096,12345),(-4096,4096,0),(0,0,65535),(32768,32767,49152),(1,1,16384),(8192,8192,32768)]:
  p=setup(image);p.scale_x=sx&65535;p.scale_y=sy&65535;p.unknown2c=rot;compare()
for case in range(2048):
 setup();dispatch.active_count=rng.randrange(85);slots=rng.sample(range(84),dispatch.active_count)
 for j,slot in enumerate(slots):
  dispatch.active[j]=slot;p=objects[slot]
  p.previous_frame=rng.randrange(311)|rng.choice([0,0x4000,0x8000,0xc000]);p.frame=rng.randrange(65536);p.animation=rng.choice([0,232,65535]);p.flags=rng.choice([0,1,4,5,7,0x04000005])
  p.x=rng.randrange(-400*256,400*256);p.y=rng.randrange(-100*256,220*256);p.layer=rng.randrange(16);p.owner=rng.randrange(6)
  p.scale_x=rng.choice([0,1,2048,4096,8192,32767,32768,65535]);p.scale_y=rng.choice([0,4096,8192,32768]);p.unknown2c=rng.randrange(65536) if case&1 else 0
  p.screen_x=1234;p.screen_y=-2345
 for c in cache:c.resident=rng.choice([0,1,3]);c.tile=rng.randrange(512)
 o.counter=rng.getrandbits(32);o.local_index=rng.randrange(4);o.mosaic=rng.randrange(65536);compare()
# OAM and matrix saturation, reverse equal-layer ties, first part vertically
# culled but later parts visible (matrix slot/count divergence is original).
for case in range(256):
 setup();dispatch.active_count=84
 for i in range(84):
  dispatch.active[i]=i;p=objects[i];p.previous_frame=(11 if case&1 else 0)|0xc000;p.animation=0;p.flags=5;p.x=30*256;p.y=(case%160-64)*256
  p.scale_x=8192;p.scale_y=8192;p.unknown2c=case*257;p.layer=case%16
 compare()
def guard():
 before=(bytes(o),bytes(objects),bytes(dispatch))
 assert lib.ssa_sprite_oam_build(C.byref(o),C.byref(dispatch),C.byref(assets),cache,C.byref(math))==-1
 assert before==(bytes(o),bytes(objects),bytes(dispatch));report['guards']+=1
setup();dispatch.active_count=85;guard();setup();dispatch.active_count=2;dispatch.active[1]=0;guard()
setup();dispatch.active[0]=84;guard();setup();objects[0].layer=16;guard();setup();objects[0].previous_frame=311;guard()
setup();o.updating=1;guard();o.updating=0;dispatch.updating=1;guard();dispatch.updating=0
setup();o.buckets[0]=16;guard();o.buckets[0]=15
setup();dispatch.objects[0]=OP();guard();dispatch.objects[0]=C.pointer(objects[0])
before=bytes(o);assert lib.ssa_sprite_oam_init(C.byref(o),ROM,0x44bbfb)==-1 and bytes(o)==before;report['guards']+=1
lib.ssa_sprite_assets_free(C.byref(assets))
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],str(ROOT/'tests/test_sprite_oam.c'),'-o',str(B/'test_sprite_oam')],check=True)
sanitized=subprocess.run([str(B/'test_sprite_oam'),str(ROOT/'orig/SSA.gba')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True,check=True)
report.update(json.loads(sanitized.stdout))
report['source_sha256']={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources}
report['fixture_sha256']={s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in ['include/ssa/sprite_oam.h','tests/test_sprite_oam_rom.py','tests/test_sprite_oam.c']}
(ROOT/'analysis/native-sprite-oam-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
