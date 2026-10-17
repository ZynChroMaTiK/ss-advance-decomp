"""Original05898 local/bypass return paths and exact multiplayer entry gate.

The network path is deliberately not executed or claimed as recovered. Native
NETWORK_REQUIRED is compared with reaching the original first transport call.
"""
from pathlib import Path
import ctypes as C
import hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
SHA1=hashlib.sha1(ROM).hexdigest()
assert SHA1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all',
 '-I',str(ROOT/'include'),str(ROOT/'src/input_dispatch.c'),str(ROOT/'src/input.c'),
 '-o',str(BUILD/'libssa_input_dispatch.so')],check=True)
class Player(C.Structure):
 _fields_=[('flags',C.POINTER(C.c_uint16)),('keys',C.POINTER(C.c_uint32))]
class Dispatch(C.Structure):
 _fields_=[('players',Player*4),('game_flags',C.c_uint16),('player_count',C.c_uint8),
           ('motion_index',C.c_uint8),('exit_flag',C.c_uint8)]
class Input(C.Structure):
 _fields_=[('raw_keys',C.c_uint32),('keys',C.c_uint32),
           ('record_buffer',C.POINTER(C.c_uint8)),('playback',C.POINTER(C.c_uint8)),
           ('record_count',C.c_uint16),('repeat_count',C.c_uint8),('repeat_keys',C.c_uint8),
           ('reset_state',C.c_uint8),('flags',C.c_uint8)]
lib=C.CDLL(str(BUILD/'libssa_input_dispatch.so'))
run=lib.ssa_input_dispatch_local;run.argtypes=[C.POINTER(Dispatch),C.c_uint32]
RESET=C.CFUNCTYPE(None,C.c_void_p)
lib.ssa_input_poll.argtypes=[C.POINTER(Input),C.c_uint16,C.c_int,RESET,C.c_void_p]

uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0x04000000,0x1000)
GAME,PLAYERS,PLAYBACK,STATE,PACKED,STOP=0x02001000,0x02002000,0x02003000,0x030042f0,0x03003f84,0x087ff000
uc.mem_write(0x03004010,struct.pack('<I',GAME));uc.mem_write(0x03004648,struct.pack('<I',PLAYERS))
rng=random.Random(0x5898);network_entry=False;reset_counts=[0,0]
report={'rom_sha1':SHA1,'local_routine_comparisons':0,'network_entry_checks':0,
        'poll_dispatch_chains':0,'guards':0,'preserved_nonselected_slots':0}
def network_hook(machine,address,size,unused):
 global network_entry
 network_entry=True;machine.reg_write(UC_ARM_REG_PC,STOP)
def reset_hook(machine,address,size,unused):
 reset_counts[0]+=1;machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,network_hook,begin=0x08014d44,end=0x08014d44)
uc.hook_add(UC_HOOK_CODE,reset_hook,begin=0x08002f1c,end=0x08002f1c)
@RESET
def native_reset(unused):reset_counts[1]+=1
def original(address):
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(address|1,STOP,count=1000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
def word(data,at,value,fmt='I'):struct.pack_into('<'+fmt,data,at,value)

def compare(game_flags=0,players=1,motion=0,exit_flag=0,flag_values=None,packed=None):
 global network_entry
 flags=[C.c_uint16(rng.getrandbits(16)) for _ in range(4)]
 keys=[C.c_uint32(rng.getrandbits(32)) for _ in range(4)]
 if flag_values is not None:
  for i in range(4):flags[i].value=flag_values[i]
 d=Dispatch((Player*4)(*[Player(C.pointer(flags[i]),C.pointer(keys[i])) for i in range(4)]),
            game_flags,players,motion,exit_flag)
 packed=rng.getrandbits(32) if packed is None else packed
 game=bytearray(rng.randbytes(256));records=bytearray(rng.randbytes(1024));state=rng.randbytes(36)
 word(game,4,game_flags,'H');word(game,16,players,'B');word(game,104,motion,'B');word(game,125,exit_flag,'B')
 for i in range(4):word(records,i*256+164,keys[i].value);word(records,i*256+188,flags[i].value,'H')
 uc.mem_write(GAME,bytes(game));uc.mem_write(PLAYERS,bytes(records));uc.mem_write(STATE,state)
 uc.mem_write(PACKED,struct.pack('<I',packed));uc.mem_write(0x0300464e,struct.pack('<H',rng.randrange(65536)))
 before=bytes(d);old_keys=[k.value for k in keys];network_entry=False
 result=run(C.byref(d),packed);original(0x08005898)
 assert result==(-2 if network_entry else 0),(game_flags,players,exit_flag,result,network_entry)
 assert bytes(d)==before and bytes(uc.mem_read(GAME,256))==bytes(game)
 assert bytes(uc.mem_read(STATE,36))==state and struct.unpack('<I',uc.mem_read(PACKED,4))[0]==packed
 for i in range(4):
  word(records,i*256+164,keys[i].value)
  assert flags[i].value==struct.unpack_from('<H',records,i*256+188)[0]
  if network_entry or not(flags[i].value&4) or i!=motion:
   assert keys[i].value==old_keys[i];report['preserved_nonselected_slots']+=1
 assert bytes(uc.mem_read(PLAYERS,1024))==bytes(records),(game_flags,players,motion,exit_flag)
 report['network_entry_checks' if network_entry else 'local_routine_comparisons']+=1

# Complete 16-bit GAME flags, player flags and both packed input halves.
for value in range(65536):
 compare(game_flags=value,players=(0,1,2,4,255)[value%5],motion=value%5)
 compare(game_flags=1 if value&1 else 0,players=1,motion=value%4,
         flag_values=[value,value^0xffff,value^4,value^0xfffb],packed=(value<<16)|(value^0xa5a5))
print('PASS every GAME flag, player flag and packed-key halfword',flush=True)
# Every count/exit byte pair, with flags=0 and flags=100, includes branch priority.
for players in range(256):
 for exit_flag in range(256):
  compare(game_flags=0x100 if (players+exit_flag)&1 else 0,players=players,
          exit_flag=exit_flag,motion=(players+exit_flag)&3)
# Motion index is a byte, not clamped to the count or display-local index.
for motion in range(256):
 for mask in range(16):
  compare(game_flags=0x100,players=255,motion=motion,
          flag_values=[4 if mask&(1<<i) else 0 for i in range(4)])
print('PASS all player-count/exit and motion-index/enabled-slot combinations',flush=True)

# Actual03028 -> processed +4 word -> original05898. Playback differs from raw
# input, proving that distribution must consume processed keys. No frame stage
# other than this documented0244C copy is synthesized here.
host_play=(C.c_uint8*4)(9,0x45,0,0xff);uc.mem_write(PLAYBACK,bytes(host_play))
for held in range(1024):
 for playback,paused in ((0,0),(4,0),(4,1)):
  previous=rng.getrandbits(32);raw=rng.getrandbits(32)
  state=Input(raw,previous,None,host_play,0,2,held&255,0,playback)
  raw_state=struct.pack('<IIIIIH8sHBBBB',raw,previous,0,0,PLAYBACK,held%16,bytes(8),0,2,held&255,0,playback)
  uc.mem_write(STATE,raw_state);game=bytearray(256);word(game,4,paused,'H');game[16]=1
  motion=held%4;game[104]=motion;uc.mem_write(GAME,bytes(game))
  records=bytearray(rng.randbytes(1024));flags=[C.c_uint16(4) for _ in range(4)]
  keys=[C.c_uint32(struct.unpack_from('<I',records,i*256+164)[0]) for i in range(4)]
  for i in range(4):word(records,i*256+188,4,'H')
  uc.mem_write(PLAYERS,bytes(records));uc.mem_write(0x04000130,struct.pack('<H',held^0x3ff))
  reset_counts[:]=[0,0];original(0x08003028)
  lib.ssa_input_poll(C.byref(state),held^0x3ff,paused,native_reset,None)
  assert reset_counts[0]==reset_counts[1]
  expected=struct.unpack('<II',uc.mem_read(STATE,8));assert expected==(state.raw_keys,state.keys)
  uc.mem_write(PACKED,struct.pack('<I',expected[1]));network_entry=False;original(0x08005898);assert not network_entry
  d=Dispatch((Player*4)(*[Player(C.pointer(flags[i]),C.pointer(keys[i])) for i in range(4)]),paused,1,motion,0)
  assert run(C.byref(d),state.keys)==0
  for i in range(4):word(records,i*256+164,keys[i].value)
  assert bytes(uc.mem_read(PLAYERS,1024))==bytes(records)
  assert keys[motion].value==state.keys
  report['poll_dispatch_chains']+=1
print('PASS 3072 actual poll/distribute chains including playback and pause',flush=True)

# Missing projections must not cause partial writes. Unsupported networking is
# reported without dereferencing local projections or altering any keys.
assert run(None,123)==-1;report['guards']+=1
flags=[C.c_uint16(4) for _ in range(4)];keys=[C.c_uint32(i+1) for i in range(4)]
for slot in range(4):
 for name in ('flags','keys'):
  d=Dispatch((Player*4)(*[Player(C.pointer(flags[i]),C.pointer(keys[i])) for i in range(4)]),0,1,0,0)
  setattr(d.players[slot],name,None)
  assert run(C.byref(d),123)==-1 and [x.value for x in keys]==[1,2,3,4]
  report['guards']+=1
assert run(C.byref(Dispatch(player_count=2)),123)==-2;report['guards']+=1
report.update(result='pass',
 rom_comparisons=report['local_routine_comparisons']+report['poll_dispatch_chains'],
 scope='Complete original05898 local/bypass return paths only; network entry checks stop at014D44 and are counted separately. Network transport, packet collection, position correction, received damage/pickups and packet sending remain unsupported. Native source executes no ROM instructions.',
 source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in ('input_dispatch.c','input.c')})
(ROOT/'analysis/input-dispatch-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
