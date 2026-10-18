"""Whole original1E48/1CF4/1D9C statistics routines; no controlled callees."""
from pathlib import Path
import ctypes as C, hashlib, json, random, struct, subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
class Weapon(C.Structure):
 _fields_=[('owned',C.c_int32),('ammo',C.c_int32),('shots',C.c_uint32),('hits',C.c_uint32)]
class Enemy(C.Structure):
 _fields_=[('spawned',C.c_uint32),('hits',C.c_uint32),('kills',C.c_uint32)]
class Rows(C.Structure):
 _fields_=[('weapons',Weapon*10),('enemies',Enemy*12)]
class Level(C.Structure):
 _fields_=[('rows',Rows),('elapsed',C.c_uint32),('score',C.c_uint32),('pickups',C.c_uint32),
 ('health',C.c_int16),('armor6',C.c_int16),('armor4',C.c_int16),('level',C.c_uint8)]
class Totals(C.Structure):
 _fields_=[('score',C.c_uint32),('health',C.c_int32),('armor6',C.c_int32),('armor4',C.c_int32),
 *[(n,C.c_uint32) for n in ('level','elapsed','spawned','kills','hits','weapon_hits','pickups')]]
assert C.sizeof(Rows)==304 and C.sizeof(Totals)==44 and C.sizeof(Level)==324
BUILD=ROOT/'build/native-tests';BUILD.mkdir(exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
 str(ROOT/'src/statistics.c'),'-o',str(BUILD/'libssa_statistics.so')],check=True)
lib=C.CDLL(str(BUILD/'libssa_statistics.so'))
complete=lib.ssa_statistics_complete;capture=lib.ssa_statistics_checkpoint_capture;restore=lib.ssa_statistics_checkpoint_restore
complete.argtypes=[C.POINTER(Totals),C.POINTER(Level)]
capture.argtypes=[C.POINTER(Rows),C.POINTER(Level)]
restore.argtypes=[C.POINTER(Level),C.POINTER(Rows)]
for f in (complete,capture,restore):f.restype=C.c_int
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x10000);uc.mem_map(0x03000000,0x10000)
GAME,PLAYER,BODY,STOP=0x02000000,0x02001000,0x02002000,0x087ff000
rng=random.Random(0x1e48)
report={'result':'pass','rom_sha1':hashlib.sha1(ROM).hexdigest(),'scope':__doc__,
 'sanitizers':'UBSan','complete_comparisons':0,'checkpoint_capture_comparisons':0,
 'checkpoint_restore_comparisons':0,'composed_sequences':0,'guards':0}
def run(address,game,player,body):
 uc.mem_write(GAME,bytes(game));uc.mem_write(PLAYER,bytes(player));uc.mem_write(BODY,bytes(body))
 uc.mem_write(0x03004010,struct.pack('<I',GAME));uc.mem_write(0x03004648,struct.pack('<I',PLAYER))
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(address|1,STOP,count=4000);assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert bytes(player)==bytes(uc.mem_read(PLAYER,len(player)))
 assert bytes(body)==bytes(uc.mem_read(BODY,len(body)))
 assert uc.mem_read(0x03004010,4)==struct.pack('<I',GAME)
 assert uc.mem_read(0x03004648,4)==struct.pack('<I',PLAYER)
 return bytes(uc.mem_read(GAME,len(game)))
def project(game,player,body):
 level=Level.from_buffer_copy(rng.randbytes(C.sizeof(Level)))
 level.rows=Rows.from_buffer_copy(game[0x90:0x1c0])
 level.elapsed=struct.unpack_from('<I',game,12)[0];level.score=struct.unpack_from('<I',game,0x88)[0]
 level.pickups=struct.unpack_from('<I',game,0x31c)[0];level.level=game[0x7c]
 level.health=struct.unpack_from('<h',body,0x32)[0]
 level.armor6=struct.unpack_from('<h',player,0xc6)[0];level.armor4=struct.unpack_from('<h',player,0xc4)[0]
 return level
for case in range(8192):
 game=bytearray(rng.randbytes(0x400));player=bytearray(rng.randbytes(0x100));body=bytearray(rng.randbytes(0x80))
 struct.pack_into('<I',player,0xac,BODY)
 # Distinct overflow boundaries, signed halfwords, all byte levels, sparse rows.
 game[0x7c]=case%256
 if case<8:
  game[0x90:0x31c]=bytes([0xff if case&1 else 0])*(0x31c-0x90)
  for offset in (0xc,0x88,0x31c):struct.pack_into('<I',game,offset,(0,1,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,123,999)[case])
  for target,offset in ((body,0x32),(player,0xc4),(player,0xc6)):
   struct.pack_into('<h',target,offset,(-32768,-1,0,1,100,200,32766,32767)[case])
 level=project(game,player,body);before=bytes(level)
 totals=Totals.from_buffer_copy(game[0x2f0:0x31c]);expected=bytearray(game)
 assert complete(C.byref(totals),C.byref(level))==1 and bytes(level)==before
 expected[0x2f0:0x31c]=bytes(totals)
 assert expected==run(0x08001e48,game,player,body),('complete',case)
 report['complete_comparisons']+=1
 saved=Rows.from_buffer_copy(game[0x1c0:0x2f0]);expected=bytearray(game)
 assert capture(C.byref(saved),C.byref(level))==1 and bytes(level)==before
 expected[0x1c0:0x2f0]=bytes(saved)
 assert expected==run(0x08001cf4,game,player,body),('capture',case)
 report['checkpoint_capture_comparisons']+=1
 saved=Rows.from_buffer_copy(game[0x1c0:0x2f0]);saved_before=bytes(saved);expected=bytearray(game)
 assert restore(C.byref(level),C.byref(saved))==1 and bytes(saved)==saved_before
 expected[0x90:0x1c0]=bytes(level.rows);struct.pack_into('<I',expected,0x31c,level.pickups)
 assert expected==run(0x08001d9c,game,player,body),('restore',case)
 # Restore must not change source time/score/vitality or tail padding.
 keep=bytearray(before);keep[:304]=bytes(saved);struct.pack_into('<I',keep,Level.pickups.offset,0)
 assert bytes(level)==keep
 report['checkpoint_restore_comparisons']+=1
 if case<512:
  level=project(game,player,body);saved=Rows();assert capture(C.byref(saved),C.byref(level))==1
  actual=run(0x08001cf4,game,player,body)
  changed=bytearray(actual);changed[0x90:0x1c0]=rng.randbytes(304);struct.pack_into('<I',changed,0x31c,0xffffffff)
  level=project(changed,player,body);assert restore(C.byref(level),C.byref(saved))==1
  actual=run(0x08001d9c,changed,player,body)
  expected=bytearray(actual);assert bytes(level.rows)==actual[0x90:0x1c0] and level.pickups==0
  totals=Totals.from_buffer_copy(actual[0x2f0:0x31c])
  # A second call adds again in the original: no hidden once-only latch.
  for repeat in range(2):
   assert complete(C.byref(totals),C.byref(level))==1
   expected[0x2f0:0x31c]=bytes(totals)
   actual=run(0x08001e48,actual,player,body);assert expected==actual
  report['composed_sequences']+=1
for func,args in ((complete,[Totals(),Level()]),(capture,[Rows(),Level()]),(restore,[Level(),Rows()])):
 for missing in range(2):
  before=[bytes(a) for a in args];ptrs=[C.byref(a) for a in args];ptrs[missing]=None
  assert func(*ptrs)==-1 and before==[bytes(a) for a in args];report['guards']+=1
inputs=[Path(__file__),ROOT/'src/statistics.c',ROOT/'include/ssa/statistics.h',ROOT/'include/ssa/weapon_progress.h']
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(ROOT/'analysis/statistics-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
