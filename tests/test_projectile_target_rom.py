"""Whole1097C including original10830,93E8,970C and fixed ARM helpers."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
base=(ROOT/'tests/test_projectile_create_rom.py').read_text().split('for i in range(65536):')[0]
base=base.replace("str(ROOT/'src/projectile_create.c'),str(ROOT/'src/movement.c')","*map(str,sorted((ROOT/'src').glob('*.c')))")
base=base.replace('libssa_projectile_create.so','libssa_projectile_target.so')
exec(compile(base,str(ROOT/'tests/test_projectile_create_rom.py'),'exec'))
target_create=lib.ssa_projectile_create_target
target_create.argtypes=[C.POINTER(Creator),C.POINTER(Request),EP,C.POINTER(Tables),C.POINTER(Ops),C.POINTER(PP)]
uc.mem_write(0x030003a4,ROM[0x430:0x638])
def bios(machine,address,size,ctx):
 a,b=[C.c_int32(machine.reg_read(r)).value for r in (UC_ARM_REG_R0,UC_ARM_REG_R1)]
 assert b
 q=abs(a)//abs(b)*(-1 if (a<0)!=(b<0) else 1)
 machine.reg_write(UC_ARM_REG_R0,q&0xffffffff);machine.reg_write(UC_ARM_REG_R1,(a-q*b)&0xffffffff)
 machine.reg_write(UC_ARM_REG_R3,abs(q));machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,bios,begin=0x08002c88,end=0x08002c88)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole1097C':0,'retained_slot_cases':0,'pool_exhaustion_guard':0,'guards':0,'callback_reentry_guards':0}
for i in range(8194):
 raw=bytearray(rng.randbytes(116));src=bytearray(rng.randbytes(116));traw=bytearray(rng.randbytes(116));wr(raw,32,OTHER)
 has_target=i%8!=0;wr(src,32,OTHER if has_target else 0);wr(src,48,rng.choice([0,0x10,0x104,0x4110,65535]),'H')
 if i>=8192:
  has_target=True;traw=bytearray(116);wr(src,32,E if i==8193 else OTHER)
 source=entity(src);e=entity(raw);target=entity(traw)
 if i<8192 and i%16==0:target.body.x=source.body.x;target.body.y=source.body.y;wr(traw,0,target.body.x);wr(traw,4,target.body.y)
 p=Projectile(C.pointer(e),C.pointer(target),rd(raw,36,'i'),rd(raw,50,'h'),rd(raw,78,'H'),rd(raw,52,'H'),0,raw[62],raw[63],raw[84],raw[85],0,0,0)
 r=Request(rng.choice([0x66,0x67,0x6a]),C.pointer(source),rng.choice(edges),rng.choice(edges),rng.choice(edges),rng.getrandbits(32),0)
 current.clear();current.update(raw=raw,source=source,projectile=p,request=r,allocated=True,floor=rng.randrange(-32768,32768),ceiling=rng.randrange(-32768,32768))
 logs[0].clear();logs[1].clear();uc.mem_write(S,bytes(src));uc.mem_write(E,bytes(raw));uc.mem_write(OTHER,bytes(traw))
 # Cleared retained slot, then that slot reused for the projectile itself.
 target_pointer=C.pointer(e) if i==8193 else C.pointer(target) if has_target else EP()
 assert target_create(C.byref(creator),C.byref(r),target_pointer,C.byref(tables),C.byref(ops),C.byref(output))==1
 for reg,value in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR],[r.type,S,r.forward,r.side,STACK,STOP|1]):uc.reg_write(reg,value&0xffffffff)
 uc.mem_write(STACK,struct.pack('<II',r.z_offset&0xffffffff,r.heading));uc.emu_start(0x0801097d,STOP,count=10000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert logs[0]==logs[1],(i,logs)
 expected=snapshot(p,raw);actual=bytes(uc.mem_read(E,116));assert expected==actual,(i,[(j,x,y) for j,(x,y) in enumerate(zip(expected,actual)) if x!=y])
 report['whole1097C']+=1
 if i>=8192:report['retained_slot_cases']+=1
current['allocated']=False
assert target_create(C.byref(creator),C.byref(r),C.pointer(target),C.byref(tables),C.byref(ops),C.byref(output))==0 and not output
report['pool_exhaustion_guard']+=1
report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/projectile_target.c','src/projectile_create.c','include/ssa/projectile_create.h','tests/test_projectile_target_rom.py']}
(ROOT/'analysis/native-projectile-target-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
