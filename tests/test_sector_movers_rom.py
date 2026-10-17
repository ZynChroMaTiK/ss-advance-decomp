"""Native sector movers against original routines, all maps and wrapped edges."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
BUILD = ROOT/'build/native-tests'
BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
 str(ROOT/'src/sector_movers.c'),str(ROOT/'src/map_assets.c'),'-o',str(BUILD/'libssa_sector_movers.so')],check=True)
class Map(C.Structure):
    _fields_=[('data',C.POINTER(C.c_uint8)),('size',C.c_size_t)]+[(n+'_count',C.c_uint16) for n in
     ('thing','texture','sector','node','leaf','segment','side','line','vertex')]+[(n,C.c_uint32) for n in
     ('things','textures','sectors','nodes','leaves','segments','sides','lines','vertices','reject','blockmap','rom_offset')]
class Movers(C.Structure):
    _fields_=[('map',C.POINTER(Map)),('records',(C.c_uint8*20)*25),('hazards',C.c_uint32*8),
              ('count',C.c_uint),('hazard_count',C.c_uint),('updating',C.c_uint)]
SOUND=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint)
class Ops(C.Structure): _fields_=[('sound',SOUND),('context',C.c_void_p)]
lib=C.CDLL(str(BUILD/'libssa_sector_movers.so'))
lib.ssa_map_assets_load.argtypes=[C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint]
lib.ssa_map_assets_free.argtypes=[C.POINTER(Map)]
init=lib.ssa_sector_movers_init
init.argtypes=[C.POINTER(Movers),C.POINTER(Map),C.c_void_p,C.c_size_t,C.c_uint]
command=lib.ssa_sector_movers_command
command.argtypes=[C.POINTER(Movers),C.c_uint,C.c_uint,C.POINTER(Ops)]
line=lib.ssa_sector_movers_activate_line
line.argtypes=[C.POINTER(Movers),C.c_uint,C.POINTER(Ops)]
step=lib.ssa_sector_movers_step
step.argtypes=[C.POINTER(Movers),C.c_uint,C.POINTER(Ops)]
contains=lib.ssa_sector_movers_hazard_contains
contains.argtypes=[C.POINTER(Movers),C.c_uint32]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(ROM)); uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x40000); uc.mem_map(0x03000000,0x10000)
BASE,GAME,RECORDS,CONFIG,STOP=0x02000000,0x02030000,0x02031000,0x02032000,0x087ff000
uc.mem_write(0x03004010,struct.pack('<I',GAME))
uc.mem_write(0x03004930,struct.pack('<I',BASE))
uc.mem_write(0x0300572c,struct.pack('<I',CONFIG))
calls=[]; native_calls=[]
def hook(machine,address,size,data):
    if address==0x080033d8:
        assert machine.reg_read(UC_ARM_REG_R0)==500
        machine.reg_write(UC_ARM_REG_R0,RECORDS)
    elif address==0x08008d5c: calls.append(machine.reg_read(UC_ARM_REG_R0))
    elif address==0x0801c3b4: pass # Separately recovered switch initialization.
    elif address==0x080c5320:
        assert machine.reg_read(UC_ARM_REG_R2)==250
        machine.mem_write(machine.reg_read(UC_ARM_REG_R1),bytes(machine.mem_read(machine.reg_read(UC_ARM_REG_R0),500)))
    elif address==0x0801bf6c:
        # GBA DMA3 copies 250 halfwords; no hardware exists in the test oracle.
        level=struct.unpack('<H',bytes(machine.mem_read(CONFIG+16,2)))[0]
        source=struct.unpack_from('<I',ROM,0x44dd4c+level*4)[0]
        machine.mem_write(RECORDS,bytes(machine.mem_read(source,500)))
        machine.reg_write(UC_ARM_REG_PC,0x0801bf79); return
    else: return
    machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for address in (0x080033d8,0x08008d5c,0x0801c3b4,0x0801bf6c,0x080c5320):
    uc.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
callback_failure=False
@SOUND
def sound(context,id):
    native_calls.append(id)
    assert step(C.byref(state),1,None)==0 # Callback reentry must fail.
    report['callback_reentry_guards']+=1
    return -1 if callback_failure else 0
ops=Ops(sound,None)
rng=random.Random(0x1c13c)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'comparisons':{},'native_guards':0,'callback_reentry_guards':0,
 'scope':'Whole mover init (allocator, DMA and BIOS CpuSet serviced, final switch init isolated), commands, line activation, update and hazard lookup; full 500-byte records, map data, 8-word hazard list and ordered sound events.'}
def run(address,arg=0):
    calls.clear()
    for reg,value in ((UC_ARM_REG_R0,arg),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):
        uc.reg_write(reg,value)
    uc.emu_start(address|1,STOP,count=1000000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP,hex(address)
def upload():
    raw=bytearray(C.string_at(m.data,m.size))
    struct.pack_into('<I',raw,0x24,BASE+m.sectors)
    uc.mem_write(BASE,bytes(raw))
    uc.mem_write(RECORDS,bytes(state.records))
    uc.mem_write(0x03005770,struct.pack('<I',RECORDS))
    uc.mem_write(0x030057a0,struct.pack('<I',state.count))
    uc.mem_write(0x030057a8,bytes([state.hazard_count]))
    uc.mem_write(0x03005780,bytes(state.hazards))
def check(label):
    assert bytes(state.records)==bytes(uc.mem_read(RECORDS,500)),label
    assert state.count==struct.unpack('<I',bytes(uc.mem_read(0x030057a0,4)))[0]
    assert state.hazard_count==uc.mem_read(0x030057a8,1)[0],label
    assert bytes(state.hazards)==bytes(uc.mem_read(0x03005780,32)),label
    raw=bytearray(uc.mem_read(BASE,m.size))
    raw[0x24:0x28]=C.string_at(m.data,m.size)[0x24:0x28]
    assert bytes(raw)==C.string_at(m.data,m.size),label
    assert calls==native_calls,(label,calls,native_calls)
    report['comparisons'][label]=report['comparisons'].get(label,0)+1
def do_command(index,cmd):
    native_calls.clear()
    run((0x0801c02c,0x0801c050,0x0801c098,0x0801c0b4)[cmd],index)
    assert command(C.byref(state),index,cmd,C.byref(ops))
    check('commands')
def do_step(ticks):
    uc.mem_write(GAME+7,bytes([ticks])); native_calls.clear()
    run(0x0801c13c)
    assert step(C.byref(state),ticks,C.byref(ops))
    check('update')
def do_line(index):
    native_calls.clear(); run(0x0801c0d4,BASE+m.lines+index*40)
    assert line(C.byref(state),index,C.byref(ops)); check('line_activation')
for level in range(16):
    for players in (1,2):
        m=Map(); state=Movers()
        state.hazards[:]=[rng.randrange(0x100000000) for _ in range(8)]
        assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)
        # Initialization preserves unused hazard slots, but count resets.
        upload()
        uc.mem_write(CONFIG+16,struct.pack('<H',level))
        uc.mem_write(GAME+16,bytes([players]))
        native_calls.clear(); run(0x0801bf14)
        assert init(C.byref(state),C.byref(m),ROM,len(ROM),level)
        check('initialization')
        if players==2:
            for frame in range(256):
                if state.count and frame%7==0: do_command(rng.randrange(state.count),rng.randrange(4))
                if frame%11==0: do_line(rng.randrange(m.line_count))
                do_step((0,1,2,3,7,255)[frame%6])
                sector=rng.randrange(m.sector_count)
                run(0x0801c328,sector)
                assert contains(C.byref(state),sector)==uc.reg_read(UC_ARM_REG_R0)
                report['comparisons']['hazard_lookup']=report['comparisons'].get('hazard_lookup',0)+1
        lib.ssa_map_assets_free(C.byref(m))

# Every timestep, both modes/directions, strict endpoint comparisons and signed
# wrapped heights/speed. Duplicate hazards and first eligible line match.
m=Map(); assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),0)
state=Movers(); assert init(C.byref(state),C.byref(m),ROM,len(ROM),0)
state.count=4
for case in range(8192):
    for i in range(4):
        raw=bytearray(rng.randbytes(20))
        flags=rng.randrange(65536)
        struct.pack_into('<H',raw,0,i)
        struct.pack_into('<H',raw,14,flags)
        state.records[i][:]=raw
        for offset in (0,2):
            value=rng.choice((0,1,224,32767,32768,65535,rng.randrange(65536)))
            C.memmove(C.addressof(m.data.contents)+m.sectors+i*20+offset,struct.pack('<H',value),2)
    state.hazard_count=case%9
    state.hazards[:]=[rng.randrange(4) for _ in range(8)]
    upload(); do_step(case%256)
    if case%4==0: do_command(rng.randrange(4),rng.randrange(4))
    if case%8==0:
        C.memmove(C.addressof(m.data.contents)+m.lines+12,struct.pack('<hh',case%4,(case+1)%4),4)
        upload(); do_line(0)

# Exact endpoints, one unit before/after, and the ceiling limit that extends
# above signed16. Completion is strict (> or <), not >= or <=.
state.count=1
for flags in (0,1,8,9,256,257,264,265,0xffff):
    for floor in (0,32700,32767,32768,65535):
        for tick in (0,1,255):
            for offset in (-1,0,1):
                r=bytearray(20)
                struct.pack_into('<HHHH',r,8,100,200,1,flags)
                state.records[0][:]=r
                target=(200 if flags&1 else 100) if flags&256 else (C.c_int16(floor).value+(224 if flags&1 else 0))
                start=(target + offset - (tick if flags&1 else -tick))&65535
                heights=(start,0) if flags&256 else (floor,start)
                C.memmove(C.addressof(m.data.contents)+m.sectors,struct.pack('<HH',*heights),4)
                state.hazard_count=3; state.hazards[:]=[0,1,0,0xeeeeffff,0,0,0,0]
                upload(); do_step(tick)

# Repeated close adds duplicates up to8; subsequent closure removes onlyone.
state.records[0][:]=bytes(20)
state.records[0][12]=1
state.hazard_count=0
state.hazards[:]=[0]*8
C.memmove(C.addressof(m.data.contents)+m.sectors,struct.pack('<HH',0,1),4)
upload()
for _ in range(12): do_command(0,1)
assert state.hazard_count==8
do_step(1); assert state.hazard_count==8
do_step(1); assert state.hazard_count==7

# Native bounds, malformed state, sound failure and mutation/reentry contracts.
for call in (lambda:step(None,1,None),lambda:step(C.byref(state),256,None),
 lambda:command(C.byref(state),25,0,None),lambda:command(C.byref(state),0,4,None),
 lambda:line(C.byref(state),m.line_count,None),lambda:init(None,C.byref(m),ROM,len(ROM),0),
 lambda:init(C.byref(state),C.byref(m),ROM,len(ROM),16),
 lambda:init(C.byref(state),C.byref(m),ROM,4,0),lambda:contains(None,0)):
    before=bytes(state); map_before=C.string_at(m.data,m.size)
    assert call() in (0,-1)
    assert bytes(state)==before and C.string_at(m.data,m.size)==map_before
    report['native_guards']+=1
for name,value in (('count',26),('hazard_count',9),('updating',1)):
    old=getattr(state,name); setattr(state,name,value)
    before=bytes(state)
    assert not step(C.byref(state),1,None) and bytes(state)==before
    setattr(state,name,old); report['native_guards']+=1
for offset,data in ((0x44dd3c,b'\x1a'),(0x44dd4c,struct.pack('<I',0x07ffffff)),
                    (0x44dd4c,struct.pack('<I',0x087fffff))):
    bad=bytearray(ROM); bad[offset:offset+len(data)]=data
    before=bytes(state); map_before=C.string_at(m.data,m.size)
    assert not init(C.byref(state),C.byref(m),bytes(bad),len(bad),0)
    assert bytes(state)==before and C.string_at(m.data,m.size)==map_before
    report['native_guards']+=1
for field,value in (('sectors',m.size),('lines',m.size)):
    old=getattr(m,field); setattr(m,field,value); before=bytes(state)
    assert not line(C.byref(state),0,None) and bytes(state)==before
    setattr(m,field,old); report['native_guards']+=1
state.records[0][0]=255; state.records[0][1]=255
assert not step(C.byref(state),1,None)
state.records[0][0]=0; state.records[0][1]=0
report['native_guards']+=1
callback_failure=True
assert not command(C.byref(state),0,0,C.byref(ops)) and state.updating==0
report['native_guards']+=1
# A failed endpoint sound leaves the wrapped overshoot and cleared bit8. The
# next call must still be permitted; rollback is not claimed by the API.
state.records[0][14]=8
C.memmove(C.addressof(m.data.contents)+m.sectors,struct.pack('<HH',0,0),4)
assert not step(C.byref(state),1,C.byref(ops)) and state.updating==0
assert bytes(state.records[0])[14]==0
assert C.string_at(C.addressof(m.data.contents)+m.sectors+2,2)==b'\xff\xff'
report['native_guards']+=1
callback_failure=False
assert step(C.byref(state),0,None)
lib.ssa_map_assets_free(C.byref(m))
report['total_rom_comparisons']=sum(report['comparisons'].values())
report['source_sha256']=hashlib.sha256((ROOT/'src/sector_movers.c').read_bytes()).hexdigest()
report['passed']=True
(ROOT/'analysis/native-sector-movers-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
