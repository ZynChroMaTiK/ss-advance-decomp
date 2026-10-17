"""Wave instruments: original ROM note/DAC registers and host sample scaling.

Original ARM instructions are executed only in this isolated test oracle.
Hardware DAC truncation and pulse/wave periods follow the documented register
semantics, cross-checked against mGBA's original PSG implementation.
"""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R4,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'tests/audio_wave_oracle.c'),
 '-o',str(b/'libssa_audio_wave.so')],check=True)
lib=C.CDLL(str(b/'libssa_audio_wave.so'));data=(C.c_uint8*len(rom)).from_buffer_copy(rom)
pitch=lib.ssa_test_psg_register;pitch.argtypes=[C.POINTER(C.c_uint8),C.c_uint,C.c_uint,C.c_uint];pitch.restype=C.c_uint
dac=lib.ssa_test_wave_dac;dac.argtypes=[C.POINTER(C.c_uint8),C.c_uint,C.c_uint];dac.restype=C.c_uint
note=lib.ssa_test_wave_note;note.argtypes=[C.POINTER(C.c_uint8),C.c_uint,C.c_uint,C.c_uint,C.c_uint,C.POINTER(C.c_double)];note.restype=C.c_int
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(rom));uc.mem_write(0x08000000,rom)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000);uc.mem_map(0x04000000,0x1000)
stop=0x087ff000;info=0x02000000;channels=0x02000400;wave_channel=channels+128
uc.mem_write(0x03007ff0,struct.pack('<I',info));block=bytearray(0x50)
struct.pack_into('<I',block,0x1c,channels);uc.mem_write(info,bytes(block));uc.mem_write(0x04000089,b'\x40')
active_end=stop
def stop_slice(u,pc,size,unused):
 if pc==active_end:u.emu_stop()
for endpoint in (0x080c4d4e,0x080c4dae):uc.hook_add(UC_HOOK_CODE,stop_slice,begin=endpoint,end=endpoint)
def run(address,end=stop):
 global active_end
 active_end=end
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,stop|1)
 try:uc.emu_start(address|1,end,count=4000)
 except Exception as error:
  raise RuntimeError((hex(address),hex(uc.reg_read(UC_ARM_REG_PC)),hex(uc.reg_read(UC_ARM_REG_R4)),uc.mem_read(0x04000089,1).hex())) from error
 assert uc.reg_read(UC_ARM_REG_PC)==end
def reference_pitch(kind,key,fraction,rounded):
 uc.reg_write(UC_ARM_REG_R0,kind);uc.reg_write(UC_ARM_REG_R1,key);uc.reg_write(UC_ARM_REG_R2,fraction);run(0x080c4880)
 result=uc.reg_read(UC_ARM_REG_R0)
 if rounded:
  ch=bytearray(0x40);ch[1]=kind|8;struct.pack_into('<I',ch,0x20,result);uc.mem_write(wave_channel,bytes(ch))
  uc.reg_write(UC_ARM_REG_R4,wave_channel)
  run(0x080c4d1c,0x080c4d4e);result=struct.unpack('<I',uc.mem_read(wave_channel+0x20,4))[0]
 return result
pitch_cases=0
for kind in (1,2,3):
 for key in range(128):
  for fraction in (0,1,64,128,255):
   for rounded in (0,1):
    expected=reference_pitch(kind,key,fraction,rounded)
    assert pitch(data,key,fraction,rounded)==expected,(kind,key,fraction,rounded)
    pitch_cases+=1
def hardware_dac(reg,nibble):
 if reg&0x80:return (nibble*3)>>2
 return {0:0,0x20:nibble,0x40:nibble>>1,0x60:nibble>>2}[reg&0x60]
# Execute the original register lookup/store, covering every level and nibble.
dac_cases=0
for level in range(16):
 ch=bytearray(0x40);ch[9]=level;uc.mem_write(wave_channel,bytes(ch));uc.reg_write(UC_ARM_REG_R4,wave_channel)
 # C4DA2 stores the DAC byte using [sp+C], without synthesizing a waveform.
 uc.mem_write(0x03007e0c,struct.pack('<I',0x04000073));run(0x080c4da2,0x080c4dae)
 reg=uc.mem_read(0x04000073,1)[0]
 for nibble in range(16):
  assert dac(data,level,nibble)==hardware_dac(reg,nibble),(level,nibble,reg)
  dac_cases+=1
# All wave instruments in the music bank; aliases intentionally share data.
instruments=[n for n in range(128) if rom[0x3e8fdc+n*12] in (3,11)]
assert instruments==[1,5,7,18,24,32]
note_cases=0;waveforms=set();bass=[]
for instrument in instruments:
 tone=rom[0x3e8fdc+instrument*12:0x3e8fdc+(instrument+1)*12]
 wave=struct.unpack_from('<I',tone,4)[0];waveforms.add(wave)
 for volume in (32,64,105,127):
  for velocity in (1,16,32,44,64,80,108,127):
   for key in (28,36,40,60,84):
    out=(C.c_double*39)();assert note(data,instrument,key,velocity,volume,out)==1
    # Independent track/note volume, then actual CgbSound note-on register writes.
    vr=(volume*2*128//256)*velocity*128//16384
    vl=(volume*2*127//256)*velocity*127//16384
    ch=bytearray(0x40);ch[0]=128;ch[1]=tone[0];ch[2]=vr;ch[3]=vl
    ch[4:8]=tone[8:12];ch[0x1c]=0x44;ch[0x1f]=8
    struct.pack_into('<I',ch,0x24,wave);reg=reference_pitch(3,key,0,1);struct.pack_into('<I',ch,0x20,reg)
    uc.mem_write(channels,bytes(256));uc.mem_write(wave_channel,bytes(ch));run(0x080c49e0)
    actual=bytes(uc.mem_read(wave_channel,0x40));volume_reg=uc.mem_read(0x04000073,1)[0]
    assert list(out[:4])==[actual[0xa],actual[9],vl,vr],(instrument,key,volume,velocity,list(out[:4]),actual.hex())
    expected_hz=65536/(2048-reg)
    assert abs(out[4]-expected_hz)<1e-10
    raw=rom[wave-0x08000000:wave-0x08000000+16]
    levels=[hardware_dac(volume_reg,(raw[i//2]&15) if i&1 else raw[i//2]>>4) for i in range(32)]
    assert list(out[7:])==levels
    index=int(expected_hz/24000*32)
    sample=(levels[index]-hardware_dac(volume_reg,15)/2)*4*128
    # Original stereo routing can mute a side; all these centered notes route both.
    assert out[5]==out[6]==sample,(instrument,key,volume,velocity,list(out[:7]),sample)
    if instrument==1 and key==40 and volume==105 and velocity in (44,108):
     bass.append({'velocity':velocity,'volume_goal':actual[0xa],'dac_register':volume_reg,'frequency_hz':expected_hz,
      'peak_to_peak':(max(levels)-min(levels))*4*128})
    note_cases+=1
report={'result':'pass','pitch_register_rom_comparisons':pitch_cases,'dac_level_checks':dac_cases,
 'wave_note_rom_comparisons':note_cases,'wave_instruments':instruments,'unique_waveforms':len(waveforms),
 'first_level_bass':bass,'test_only_arm_execution':True,'physical_listening_tested':False,
 'hardware_reference':'https://github.com/mgba-emu/mgba/blob/master/src/gb/audio.c',
 'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
 (r/'src/audio.c',r/'include/ssa/audio.h',r/'tests/audio_wave_oracle.c',Path(__file__))}}
(r/'analysis/audio-wave-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
