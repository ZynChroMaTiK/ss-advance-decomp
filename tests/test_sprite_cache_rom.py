"""Whole DBF4/DBDC/E0A8/E2EC and E358 transfers; BIOS/DMA only modeled."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess,os
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
for filename,names in [('test_sprite_assets.py',('Part','Image','Assets')),('test_weapon_animation_rom.py',('Object',)),('test_sprite_dispatch_rom.py',('Dispatch',))]:
    if 'Dispatch' in names:OP=C.POINTER(Object)
    nodes=[n for n in ast.parse((ROOT/'tests'/filename).read_text()).body if isinstance(n,ast.ClassDef) and n.name in names]
    exec(compile(ast.Module(nodes,type_ignores=[]),filename+' declarations','exec'))
u8,u16,u32=C.c_uint8,C.c_uint16,C.c_uint32
class Entry(C.Structure):_fields_=[('resident',u8),('pending',u8),('tile',u16)]
class Block(C.Structure):_fields_=[('start',u16),('count',u16)]
class Transfer(C.Structure):_fields_=[('image',u16),('destination',u16),('count',u16),('reserved',u16)]
class Cache(C.Structure):
    _fields_=[('entries',Entry*311),('free_blocks',Block*52),('used_blocks',Block*52),
      ('used_images',u16*52),('requests',u16*50),('transfers',Transfer*10),('tiles',u8*32768),
      ('free_count',u16),('used_count',u16),('image_count',u16),('read_cursor',u16),('write_cursor',u16),
      ('allocation_failures',u16),('transfer_count',u16),('budget',u32),('loaded',C.c_uint),('updating',C.c_uint)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['sprite_cache.c','sprite_assets.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(B/'libssa_sprite_cache.so')],check=True)
lib=C.CDLL(str(B/'libssa_sprite_cache.so'))
lib.ssa_sprite_assets_load.argtypes=[C.POINTER(Assets),C.c_void_p,C.c_size_t]
lib.ssa_sprite_assets_free.argtypes=[C.POINTER(Assets)]
for name in ('init','free'):getattr(lib,'ssa_sprite_cache_'+name).argtypes=[C.POINTER(Cache)]
lib.ssa_sprite_cache_reset.argtypes=[C.POINTER(Cache),C.POINTER(Dispatch)]
for name in ('allocate','release'):getattr(lib,'ssa_sprite_cache_'+name).argtypes=[C.POINTER(Cache),u16]
lib.ssa_sprite_visibility.argtypes=[C.POINTER(Dispatch),C.POINTER(Assets),C.c_uint]
lib.ssa_sprite_cache_prepare.argtypes=[C.POINTER(Cache),C.POINTER(Dispatch),C.POINTER(Assets),C.c_uint]
lib.ssa_sprite_cache_flush.argtypes=[C.POINTER(Cache),C.POINTER(Assets)]
a=Assets();assert lib.ssa_sprite_assets_load(C.byref(a),ROM,len(ROM))==1
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for p,n in ((0x08000000,len(ROM)),(0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x10000),(0x06000000,0x20000)):u.mem_map(p,n)
u.mem_write(0x08000000,ROM)
OBJECTS,ACTIVE,ENTRIES,FREE,USED,IMAGES,RING,GAME=0x02000000,0x02002000,0x02003000,0x02005000,0x02005400,0x02005800,0x02005c00,0x02006000
STOP=0x087ff000;DMA=0x040000d4;rng=random.Random(0xdbdc)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_visibility':0,'whole_preparation':0,'whole_allocator':0,
 'whole_release':0,'whole_transfer':0,'whole_reset':0,'uploaded_bytes':0,'guards':0,'scope':__doc__}
def put(p,f,*v):u.mem_write(p,struct.pack('<'+f,*v))
def get(p,f='I'):return struct.unpack('<'+f,u.mem_read(p,struct.calcsize('<'+f)))[0]
def copy(src,dst,count):
    raw=bytes(u.mem_read(src,count))
    if 0x06010000<=dst and dst+count<=0x06020000:
        # Physical VRAM repeats64K+32K+32K: upper32K aliases OBJ32K.
        for at in range(0,count,64):u.mem_write(0x06010000+((dst-0x06010000+at)&32767),raw[at:at+64])
    else:u.mem_write(dst,raw)
def code(m,p,size,ctx):
    if p==0x080c5320:
        control=m.reg_read(UC_ARM_REG_R2);count=(control&0x1fffff)*(4 if control&(1<<26) else 2)
        copy(m.reg_read(UC_ARM_REG_R0),m.reg_read(UC_ARM_REG_R1),count)
        m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
def hardware(m,access,p,size,value,ctx):
    if p==DMA+8 and value&0x80000000:copy(get(DMA),get(DMA+4),(value&65535)*(4 if value&(1<<26) else 2))
u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,hardware)
def call(p,args=()):
    for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(reg,v)
    u.reg_write(UC_ARM_REG_SP,0x03007f00);u.reg_write(UC_ARM_REG_LR,STOP|1)
    u.emu_start(p|1,STOP,count=300000);assert u.reg_read(UC_ARM_REG_PC)==STOP,hex(p)
    return u.reg_read(UC_ARM_REG_R0)
def bind(c,objects=None,d=None,local=0):
    u.mem_write(0x03000000,bytes(0x8000))
    for p,target in ((0x03005574,FREE),(0x03005570,USED),(0x03005578,IMAGES),(0x03005568,RING),
      (0x03005580,ENTRIES),(0x0300559c,OBJECTS),(0x030055c0,ACTIVE),(0x03004010,GAME)):put(p,'I',target)
    put(0x03005560,'I',RING+c.write_cursor*2);put(0x03005584,'I',RING+c.read_cursor*2)
    for p,v in ((0x0300556c,c.free_count),(0x03005564,c.used_count),(0x0300557c,c.image_count),(0x0300555c,c.allocation_failures)):put(p,'H',v)
    put(0x03005588,'I',c.budget);put(0x0300464e,'H',local)
    u.mem_write(FREE,bytes(c.free_blocks));u.mem_write(USED,bytes(c.used_blocks));u.mem_write(IMAGES,bytes(c.used_images))
    u.mem_write(RING,bytes(c.requests));u.mem_write(ENTRIES,bytes(8000));u.mem_write(ENTRIES,bytes(c.entries))
    # Original initial previous-frameFFFF retention probes beyond its2000-entry
    # cache. This oracle explicitly seeds that unrelated memory to0; native
    # sentinel handling has no such unowned memory access.
    u.mem_write(ENTRIES+0x3fff*4,bytes(4))
    u.mem_write(0x06010000,bytes(c.tiles));u.mem_write(0x030054e0,bytes(124))
    if objects is not None:u.mem_write(OBJECTS,bytes(objects));u.mem_write(ACTIVE,bytes(d.active));put(0x030055c4,'I',d.active_count)
def equal(c):
    for name,p in (('free_blocks',FREE),('used_blocks',USED),('used_images',IMAGES),('requests',RING),('entries',ENTRIES)):
        assert bytes(getattr(c,name))==bytes(u.mem_read(p,C.sizeof(getattr(c,name)))),(name,report)
    for name,p in (('free_count',0x0300556c),('used_count',0x03005564),('image_count',0x0300557c),('allocation_failures',0x0300555c)):
        assert getattr(c,name)==get(p,'H'),(name,getattr(c,name),get(p,'H'))
    assert c.read_cursor==(get(0x03005584)-RING)//2 and c.write_cursor==(get(0x03005560)-RING)//2
    assert c.budget==get(0x03005588),(c.budget,get(0x03005588))
    for i,t in enumerate(c.transfers[:c.transfer_count]):
        im=a.images[t.image];payload=0x08000000+im.rom_offset+28+im.part_count*8
        assert (get(0x030054e0+i*12),get(0x030054e4+i*12),get(0x030054e8+i*12))==(payload,t.destination,t.count)
    assert get(0x030054e0+c.transfer_count*12)==0

# Whole allocator/release include exact removal and stale spare record copies.
for trial in range(128):
    c=Cache();assert lib.ssa_sprite_cache_init(C.byref(c))==1
    for n in range(128):
        bind(c)
        if c.used_count and rng.randrange(3)==0:
            tile=c.used_blocks[rng.randrange(c.used_count)].start
            call(0x0800e2ec,(tile,));assert lib.ssa_sprite_cache_release(C.byref(c),tile)==1;report['whole_release']+=1
        else:
            count=rng.choice((0,1,2,3,8,16,32,64,138,512,513,65535))
            expected=call(0x0800e0a8,(count,))&65535
            result=lib.ssa_sprite_cache_allocate(C.byref(c),count)
            assert result==(expected if expected!=65535 else -1),(count,result,expected)
            report['whole_allocator']+=1
        equal(c)

# Exact directed fragmentation case: E1CC retains the last adjacent pair.
c=Cache();assert lib.ssa_sprite_cache_init(C.byref(c))==1
for i in range(50):
    bind(c);expected=call(0x0800e0a8,(10,))&65535
    assert lib.ssa_sprite_cache_allocate(C.byref(c),10)==expected;equal(c);report['whole_allocator']+=1
for i in [*range(0,50,2),*range(1,50,2)]:
    bind(c);call(0x0800e2ec,(160+i*10,));assert lib.ssa_sprite_cache_release(C.byref(c),160+i*10)==1
    equal(c);report['whole_release']+=1
assert (c.free_count,c.free_blocks[0].start,c.free_blocks[0].count,c.free_blocks[1].start,c.free_blocks[1].count)==(2,160,500,660,12)

objects=(Object*84)();d=Dispatch()
for i in range(84):d.objects[i]=C.pointer(objects[i]);d.active[i]=i
for f in range(311):
    for flip in (0,0x4000,0x8000,0xc000):
        for boundary in (-32768,-241,-1,0,160,240,241,32767):
            objects[0].type=1;objects[0].owner=0;objects[0].animation=0;objects[0].frame=f|flip
            objects[0].screen_x=boundary;objects[0].screen_y=boundary;objects[0].flags=0xa5a5a5a5
            d.active_count=1;bind(Cache(),objects,d);call(0x0800dbf4)
            assert lib.ssa_sprite_visibility(C.byref(d),C.byref(a),0)==1
            assert bytes(objects)==bytes(u.mem_read(OBJECTS,C.sizeof(objects))), (f,flip,boundary,hex(objects[0].flags),hex(get(OBJECTS+28)),tuple(a.images[f].bounds))
            report['whole_visibility']+=1

# Consecutive native cache frames: genuine resources, owner/visibility/flip,
# weapon tile0 uploads, request ring wrap, budget misses, allocation/aging.
for series in range(16):
    c=Cache();assert lib.ssa_sprite_cache_init(C.byref(c))==1
    C.memset(objects,0,C.sizeof(objects));d.active_count=20
    for frame in range(128):
        local=series%4
        for i in range(20):
            o=objects[i];o.type=2 if i==0 else 1;o.owner=local+1 if i==0 else rng.randrange(6)
            o.animation=0;o.frame=((frame//rng.choice((1,2,4,8))+series*31+i*7)%311)|rng.choice((0,0x4000,0x8000,0xc000))
            o.screen_x=rng.randrange(-80,300);o.screen_y=rng.randrange(-80,220)
            if frame==0:o.previous_frame=65535
        bind(c,objects,d,local);put(GAME+16,'B',1+series%2)
        call(0x0800dbdc)
        rc=lib.ssa_sprite_cache_prepare(C.byref(c),C.byref(d),C.byref(a),local)
        assert rc==1,(series,frame,rc)
        assert bytes(objects)==bytes(u.mem_read(OBJECTS,C.sizeof(objects))),(series,frame,'objects')
        equal(c);report['whole_preparation']+=1
        report['uploaded_bytes']+=sum(min(t.count,160)*64 for t in c.transfers[:c.transfer_count])
        call(0x0800e358);assert lib.ssa_sprite_cache_flush(C.byref(c),C.byref(a))==1
        assert bytes(c.tiles)==bytes(u.mem_read(0x06010000,32768)),(series,frame,'OBJ pixels')
        report['whole_transfer']+=1
    assert lib.ssa_sprite_cache_free(C.byref(c))==1

for frame in range(64):
    c=Cache();assert lib.ssa_sprite_cache_init(C.byref(c))==1
    assert lib.ssa_sprite_cache_allocate(C.byref(c),3)==160
    c.image_count=1;c.used_images[0]=4;c.entries[4]=Entry(3,1,160)
    c.requests[20]=4;c.read_cursor=4;c.write_cursor=21;c.budget=51;c.allocation_failures=4
    for i in range(20):objects[i].previous_frame=65535-i
    bind(c,objects,d);call(0x0800daf0)
    assert lib.ssa_sprite_cache_reset(C.byref(c),C.byref(d))==1
    equal(c);assert bytes(objects)==bytes(u.mem_read(OBJECTS,C.sizeof(objects)))
    report['whole_reset']+=1

# Native entry guards and malformed resource bounds preserve output.
c=Cache();assert lib.ssa_sprite_cache_init(C.byref(c))==1
before=bytes(c);assert lib.ssa_sprite_cache_init(C.byref(c))==-1 and bytes(c)==before;report['guards']+=1
d.active_count=1;objects[0].type=1;objects[0].owner=0;objects[0].animation=0;objects[0].frame=311
old=bytes(objects);assert lib.ssa_sprite_visibility(C.byref(d),C.byref(a),0)==-1 and bytes(objects)==old;report['guards']+=1
for field,value in [('updating',1),('free_count',51),('used_count',51),('image_count',51),('read_cursor',50),('write_cursor',50),('transfer_count',11)]:
    old=getattr(c,field);setattr(c,field,value);before=bytes(c)
    assert lib.ssa_sprite_cache_prepare(C.byref(c),C.byref(d),C.byref(a),0)==-1 and bytes(c)==before
    setattr(c,field,old);report['guards']+=1
lib.ssa_sprite_assets_free(C.byref(a))
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined',
 '-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),*[str(ROOT/'src'/s) for s in sources],
 str(ROOT/'tests/test_sprite_cache.c'),'-o',str(B/'test_sprite_cache')],check=True)
sanitized=subprocess.run([str(B/'test_sprite_cache'),str(ROOT/'orig/SSA.gba')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True)
assert sanitized.returncode==0,(sanitized.stdout,sanitized.stderr)
report['sanitizers']=json.loads(sanitized.stdout)
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'src'/s for s in sources]+[ROOT/'include/ssa/sprite_cache.h',ROOT/'tests/test_sprite_cache.c',Path(__file__)]}
(ROOT/'analysis/native-sprite-cache-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
