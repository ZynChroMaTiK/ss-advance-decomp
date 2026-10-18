"""Whole original1231C capture/statistics and123C4 restore, including originalFC04 and untouched record bytes."""
from pathlib import Path
import ast, ctypes as C, hashlib, json, random, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
tree=ast.parse((ROOT/'tests/test_weapon_animation_rom.py').read_text())
exec(compile(ast.Module([n for n in tree.body if isinstance(n,ast.ClassDef) and n.name in ('Object','Control')],type_ignores=[]),'animation projections','exec'))
class Inventory(C.Structure):
 _fields_=[('weapons',C.c_uint16),('ammo',C.c_int16*10),('pending_count',C.c_uint16),('pending_weapons',C.c_uint32*10),('health',C.c_int16),('stat_c4',C.c_int16),('stat_c6',C.c_int16)]
class Progress(C.Structure):
 _fields_=[('current',C.c_uint32),('weapons',C.c_uint16),('ammo',C.c_int16*10),('requested',C.c_uint16)]
class Stats(C.Structure):
 _fields_=[('owned',C.c_int32),('ammo',C.c_int32),('shots',C.c_uint32),('hits',C.c_uint32)]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),str(ROOT/'src/weapon_progress.c'),str(ROOT/'src/weapon_animation.c'),'-o',str(BUILD/'libssa_weapon_progress.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_weapon_progress.so'))
restore=lib.ssa_weapon_progress_restore
restore.argtypes=[C.POINTER(Inventory),C.POINTER(Control),C.POINTER(Object),C.POINTER(Progress)]
restore.restype=C.c_int
capture=lib.ssa_weapon_progress_capture
capture.argtypes=[C.POINTER(Progress),C.POINTER(Stats),C.POINTER(Inventory),C.POINTER(Control),C.c_uint8]
capture.restype=C.c_int
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
TARGET,OBJECT,STOP=0x02000000,0x02001000,0x087ff000
rng=random.Random(0x123c4)
edge_current=(0,1,10,233,234,65535,65536,65537,0x100e9,0xffffffff)
report={'result':'pass','rom_sha1':hashlib.sha1(ROM).hexdigest(),'rom_comparisons':0,'native_guards':0,'scope':__doc__,'sanitizers':'UBSan'}
report['capture_comparisons']=0
report['capture_restore_comparisons']=0
capture_settings=set()
for case in range(8192):
 raw=bytearray(rng.randbytes(256));obj=Object.from_buffer_copy(rng.randbytes(C.sizeof(Object)))
 inv=Inventory.from_buffer_copy(rng.randbytes(C.sizeof(Inventory)))
 control=Control(struct.unpack_from('<I',raw)[0],struct.unpack_from('<I',raw,8)[0],struct.unpack_from('<H',raw,0x50)[0],rng.randrange(65536))
 current=1+case%10 if case<4096 else rng.randrange(2**32)
 if 4096<=case<4096+len(edge_current):current=edge_current[case-4096]
 progress=Progress(current,rng.randrange(65536),(C.c_int16*10)(*[rng.randrange(-32768,32768) for _ in range(10)]))
 expected=bytearray(raw);saved=bytearray(rng.randbytes(0x60))
 struct.pack_into('<I',raw,4,OBJECT);struct.pack_into('<I',expected,4,OBJECT)
 struct.pack_into('<I',saved,0,progress.current);struct.pack_into('<H',saved,12,progress.weapons)
 saved[14:34]=bytes(progress.ammo)
 uc.mem_write(TARGET,bytes(raw));uc.mem_write(OBJECT,bytes(obj));uc.mem_write(0x03005680,bytes(saved))
 uc.reg_write(UC_ARM_REG_R0,TARGET);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(0x080123c5,STOP,count=1000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
 before=bytearray(bytes(inv));old_flags=control.flags;old_keys=control.keys
 assert restore(C.byref(inv),C.byref(control),C.byref(obj),C.byref(progress))==1
 struct.pack_into('<I',expected,0,control.current);struct.pack_into('<H',expected,12,inv.weapons)
 expected[14:34]=bytes(inv.ammo);struct.pack_into('<H',expected,0x50,control.requested)
 assert expected==bytes(uc.mem_read(TARGET,256)),case
 assert bytes(obj)==bytes(uc.mem_read(OBJECT,C.sizeof(Object))),case
 struct.pack_into('<H',before,Inventory.weapons.offset,progress.weapons)
 before[Inventory.ammo.offset:Inventory.ammo.offset+20]=bytes(progress.ammo)
 assert bytes(inv)==before and control.flags==old_flags and control.keys==old_keys
 report['rom_comparisons']+=1
for missing in range(4):
 inv=Inventory();control=Control();obj=Object();progress=Progress()
 pointers=[C.byref(inv),C.byref(control),C.byref(obj),C.byref(progress)]
 before=(bytes(inv),bytes(control),bytes(obj),bytes(progress));pointers[missing]=None
 assert restore(*pointers)==-1 and before==(bytes(inv),bytes(control),bytes(obj),bytes(progress))
 report['native_guards']+=1
for case in range(4096):
 raw=bytearray(rng.randbytes(256));game=bytearray(rng.randbytes(0x200));saved=bytearray(rng.randbytes(0x60));settings=bytearray(rng.randbytes(64))
 if case==0:
  struct.pack_into('<H',raw,12,0xffff)
  struct.pack_into('<10h',raw,14,-32768,-1,0,1,32767,999,-999,12345,-12345,42)
 if case<512:struct.pack_into('<I',raw,0,case%10+1)
 settings[0x1b]=0 if case%2==0 else (case//2)%255+1
 capture_settings.add(settings[0x1b])
 inv=Inventory.from_buffer_copy(rng.randbytes(C.sizeof(Inventory)))
 inv.weapons=struct.unpack_from('<H',raw,12)[0]
 for i in range(10):inv.ammo[i]=struct.unpack_from('<h',raw,14+2*i)[0]
 control=Control(struct.unpack_from('<I',raw,0)[0],struct.unpack_from('<I',raw,8)[0],struct.unpack_from('<H',raw,0x50)[0],rng.randrange(65536))
 progress=Progress.from_buffer_copy(rng.randbytes(C.sizeof(Progress)))
 stats=(Stats*10).from_buffer_copy(game[0x90:0x130])
 old_inv,old_control=bytes(inv),bytes(control)
 uc.mem_write(TARGET,bytes(raw));uc.mem_write(0x03005680,bytes(saved));uc.mem_write(0x02002000,bytes(game));uc.mem_write(0x02003000,bytes(settings))
 uc.mem_write(0x03004010,struct.pack('<I',0x02002000));uc.mem_write(0x0300572c,struct.pack('<I',0x02003000))
 uc.reg_write(UC_ARM_REG_R0,TARGET);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(0x0801231d,STOP,count=2000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert capture(C.byref(progress),stats,C.byref(inv),C.byref(control),settings[0x1b])==1
 struct.pack_into('<I',saved,0,progress.current);struct.pack_into('<H',saved,12,progress.weapons)
 saved[14:34]=bytes(progress.ammo);struct.pack_into('<H',saved,0x50,progress.requested)
 game[0x90:0x130]=bytes(stats)
 assert saved==bytes(uc.mem_read(0x03005680,len(saved))),case
 assert game==bytes(uc.mem_read(0x02002000,len(game))),case
 assert raw==bytes(uc.mem_read(TARGET,len(raw))) and (bytes(inv),bytes(control))==(old_inv,old_control)
 assert settings==bytes(uc.mem_read(0x02003000,len(settings)))
 report['capture_comparisons']+=1
 if case<512:
  target=bytearray(rng.randbytes(256));obj=Object.from_buffer_copy(rng.randbytes(C.sizeof(Object)))
  next_inv=Inventory.from_buffer_copy(rng.randbytes(C.sizeof(Inventory)));next_control=Control()
  # Compose the actual capture output with original/native restoration.
  struct.pack_into('<I',target,4,OBJECT);uc.mem_write(0x03005680,bytes(saved));uc.mem_write(TARGET,bytes(target));uc.mem_write(OBJECT,bytes(obj))
  uc.reg_write(UC_ARM_REG_R0,TARGET);uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
  uc.emu_start(0x080123c5,STOP,count=1000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
  assert restore(C.byref(next_inv),C.byref(next_control),C.byref(obj),C.byref(progress))==1
  struct.pack_into('<I',target,0,next_control.current);struct.pack_into('<H',target,12,next_inv.weapons)
  target[14:34]=bytes(next_inv.ammo);struct.pack_into('<H',target,0x50,next_control.requested)
  assert target==bytes(uc.mem_read(TARGET,256)) and bytes(obj)==bytes(uc.mem_read(OBJECT,C.sizeof(Object)))
  report['capture_restore_comparisons']+=1
for missing in range(4):
 progress=Progress();stats=(Stats*10)();inv=Inventory();control=Control()
 pointers=[C.byref(progress),stats,C.byref(inv),C.byref(control)]
 before=bytes(progress),bytes(stats),bytes(inv),bytes(control);pointers[missing]=None
 assert capture(*pointers,1)==-1 and before==(bytes(progress),bytes(stats),bytes(inv),bytes(control))
 report['native_guards']+=1
assert capture_settings==set(range(256))
report['capture_setting_values']=len(capture_settings)
inputs=[Path(__file__),ROOT/'tests/test_weapon_animation_rom.py',
 ROOT/'src/weapon_progress.c',ROOT/'src/weapon_animation.c',
 *[ROOT/'include/ssa'/name for name in ('weapon_progress.h','weapon_animation.h','weapon_select.h','inventory.h')]]
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(ROOT/'analysis/weapon-progress-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
