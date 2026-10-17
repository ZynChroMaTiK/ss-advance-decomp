"""Differential channel arbitration and volume tests against the owned ROM."""
from pathlib import Path
import ctypes as C
import hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'tests/audio_channels_oracle.c'),
 '-o',str(b/'libssa_audio_channels.so')],check=True)
lib=C.CDLL(str(b/'libssa_audio_channels.so'));data=(C.c_uint8*len(rom)).from_buffer_copy(rom)
select=lib.ssa_test_audio_channel;select.argtypes=[C.POINTER(C.c_uint8),C.POINTER(C.c_uint)];select.restype=C.c_int
volume=lib.ssa_test_audio_volume;volume.argtypes=[C.POINTER(C.c_uint8),C.POINTER(C.c_int),C.POINTER(C.c_uint)]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(rom));uc.mem_write(0x08000000,rom)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000)
stop=0x087ff000;info=0x02000000;psg=info+0x400;tone=info+0x600;sp=0x03007e00
tracks=[struct.unpack_from('<I',rom,0x3e99a0+p*12+4)[0] for p in range(4)]
def owner(p,t):return tracks[p]+t*0x50
mode=struct.unpack_from('<I',rom,0xc3e90)[0]
uc.mem_write(info,bytes(0x400));uc.reg_write(UC_ARM_REG_R3,mode);uc.reg_write(UC_ARM_REG_R5,info)
uc.emu_start(0x080c43e7,0x080c4414,count=200)
assert bytes(uc.mem_read(info+6,2))==bytes((3,13)) and mode==0x0092d300
uc.hook_add(UC_HOOK_CODE,lambda u,pc,size,data:u.emu_stop(),begin=0x080c3c34,end=0x080c3c34)
uc.hook_add(UC_HOOK_CODE,lambda u,pc,size,data:u.emu_stop(),begin=0x080c3cda,end=0x080c3cda)
rng=random.Random(0x535341);arbitration=0
for case in range(12000):
 kind=case%5;p=rng.randrange(4);t=rng.randrange(16);priority=rng.choice((0,1,64,128,254,255))
 states=[];ram=bytearray(0x700);ram[6]=3;struct.pack_into('<I',ram,0x1c,psg)
 for i in range(7):
  active=rng.randrange(2);stage=rng.randrange(6);op=rng.randrange(4);ot=rng.randrange(16)
  pr=rng.choice((priority,max(priority-1,0),min(priority+1,255)))
  states.extend((active,stage,op,ot,pr));off=0x50+i*0x40 if i<3 else 0x400+(i-3)*0x40
  ram[off]=(0x44 if stage==5 else 0x40 if stage==3 else 4 if stage==4 else 1) if active else 0
  ram[off+0x13]=pr;struct.pack_into('<I',ram,off+0x2c,owner(op,ot))
 ram[0x600]=kind;uc.mem_write(info,bytes(ram));uc.mem_write(sp,struct.pack('<6I',0,info,0,0,priority,0))
 uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_R5,owner(p,t));uc.reg_write(UC_ARM_REG_R9,tone)
 uc.emu_start(0x080c3ba3,stop,count=200)
 if uc.reg_read(UC_ARM_REG_PC)==0x080c3cda:expected=-1
 else:
  assert uc.reg_read(UC_ARM_REG_PC)==0x080c3c34
  address=uc.reg_read(UC_ARM_REG_R4);expected=(address-info-0x50)//0x40 if address<psg else 3+(address-psg)//0x40
 actual=select(data,(C.c_uint*39)(kind,p,t,priority,*states))
 assert actual==expected,(case,kind,p,t,priority,states,actual,expected)
 arbitration+=1
levels=0;player=0x02002000;track=0x02002100;channel=0x02002200
for case in range(6000):
 vol=rng.randrange(128);pan=rng.randrange(128);velocity=rng.randrange(128);instrument=rng.randrange(-64,64)*2;env=rng.randrange(256)
 tr=bytearray(0x50);tr[0]=1;tr[0x12]=vol;tr[0x13]=64;tr[0x14]=(pan-64)&255
 ch=bytearray(0x40);ch[0x12]=velocity;ch[0x14]=instrument&255
 uc.mem_write(track,bytes(tr));uc.mem_write(channel,bytes(ch))
 uc.reg_write(UC_ARM_REG_R0,player);uc.reg_write(UC_ARM_REG_R1,track);uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_LR,stop|1)
 uc.emu_start(0x080c47cd,stop,count=200);assert uc.reg_read(UC_ARM_REG_PC)==stop
 uc.reg_write(UC_ARM_REG_R4,channel);uc.reg_write(UC_ARM_REG_R5,track);uc.reg_write(UC_ARM_REG_LR,stop|1)
 uc.emu_start(0x080c3ac5,stop,count=100);assert uc.reg_read(UC_ARM_REG_PC)==stop
 note=bytes(uc.mem_read(channel,0x40));l,rr=note[2:4]
 uc.reg_write(UC_ARM_REG_R0,channel);uc.reg_write(UC_ARM_REG_LR,stop|1)
 uc.emu_start(0x080c4979,stop,count=100);goal=uc.mem_read(channel,0x40)[0xa]
 ram=bytearray(0x50);ram[7]=13;uc.mem_write(info,bytes(ram));uc.mem_write(sp+0x18,struct.pack('<I',info))
 uc.reg_write(UC_ARM_REG_R4,channel);uc.reg_write(UC_ARM_REG_R5,env)
 uc.emu_start(0x080c3357,0x080c3370,count=40);mixed=bytes(uc.mem_read(channel,0x40))
 out=(C.c_uint*5)();volume(data,(C.c_int*5)(vol,pan,velocity,instrument,env),out)
 assert list(out)==[rr,l,goal,mixed[0xb],mixed[0xa]],(case,list(out),l,rr,goal,mixed.hex())
 levels+=1
report={'result':'pass','original_reference_arbitration_rom_comparisons':arbitration,'volume_rom_comparisons':levels,
 'pc_uses_original_allocator':False,
 'rom_pcm_channels':3,'rom_psg_channels':4,'rom_sound_mode':mode,'rom_pcm_master':13,'test_only_arm_execution':True,
 'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
 (r/'src/audio.c',r/'include/ssa/audio.h',r/'tests/audio_channels_oracle.c',Path(__file__))}}
(r/'analysis/audio-channels-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
