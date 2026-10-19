"""Inspect retained HUD assets/controllers; original CPU execution is test-only."""
from pathlib import Path
import hashlib,json,struct
from collections import Counter
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC

ROOT=Path(__file__).resolve().parents[1]
ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for address,size in ((0x08000000,len(ROM)),(0x02000000,0x40000),(0x03000000,0x10000)):
    u.mem_map(address,size)
u.mem_write(0x08000000,ROM)
GAME,PLAYER,BODY,OBJECT,LEVEL,POOL,STOP=0x02000000,0x02001000,0x02002000,0x02003000,0x02004000,0x02005000,0x087ff000
allocating=False
allocated=[]
def put(address,fmt,*values):u.mem_write(address,struct.pack('<'+fmt,*values))
def get(address,fmt='I'):return struct.unpack('<'+fmt,u.mem_read(address,struct.calcsize('<'+fmt)))[0]
def allocation(machine,address,size,ctx):
    if not allocating:return
    assert len(allocated)<84
    obj=POOL+48*len(allocated);allocated.append(obj)
    u.mem_write(obj,bytes(48));put(obj+0x1c,'I',1);put(obj+0x20,'HH',65535,65535)
    put(obj+0x28,'HH',4096,4096)
    machine.reg_write(UC_ARM_REG_R0,obj);machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,allocation,begin=0x0800f9b4,end=0x0800f9b4)
def call(address):
    u.reg_write(UC_ARM_REG_SP,0x03007e00);u.reg_write(UC_ARM_REG_LR,STOP|1)
    u.emu_start(address|1,STOP,count=500000)
    assert u.reg_read(UC_ARM_REG_PC)==STOP,hex(u.reg_read(UC_ARM_REG_PC))
put(0x03004010,'I',GAME);put(0x03004648,'I',PLAYER)
put(0x0300464e,'H',0);put(0x030055a0,'I',OBJECT);put(0x0300572c,'I',LEVEL)
put(PLAYER+0xac,'I',BODY)
health_cases=score_cases=0
for health in (0,1,7,9,10,57,99,100,175,255,999):
    for digit in range(3):
        for mapped in (False,True):
            u.mem_write(OBJECT,bytes(48));put(OBJECT+4,'HH',5,digit);put(OBJECT+0x1c,'I',1);put(OBJECT+0x20,'H',0xab)
            put(PLAYER+0xbc,'H',8 if mapped else 0);put(BODY+0x32,'h',health)
            call(0x0800e700)
            assert bool(get(OBJECT+0x1c)&1)==(not mapped)
            assert get(OBJECT+0x20,'H')==(0xab if mapped else 0x24+(health//(100,10,1)[digit])%10)
            health_cases+=1
for value in (0,1,9,10,12345678,99999999):
    for digit in range(8):
        for mapped in (False,True):
            u.mem_write(OBJECT,bytes(48));put(OBJECT+4,'HH',11,digit);put(OBJECT+0x1c,'I',1);put(OBJECT+0x20,'H',0xab)
            put(PLAYER+0xbc,'H',8 if mapped else 0);put(GAME+0x34c,'I',value)
            call(0x0800efb4)
            assert bool(get(OBJECT+0x1c)&1)==(not mapped)
            assert get(OBJECT+0x20,'H')==(0xab if mapped else 0x2e+(value//10**digit)%10),(value,digit,mapped,get(OBJECT+0x20,'H'))
            score_cases+=1
icon_cases=0
retail_weapon_images=[285,286,280,281,282,283,292,291,290,284]
retail_ammo_images=[243,273,277,274,275,278,294,271,276,272]
for weapon in range(1,11):
    for mapped in (False,True):
        for address,kind,base,expected in ((0x0800f1c0,7,0x42,retail_weapon_images),
                                         (0x0800f27c,8,0xdf,retail_ammo_images)):
            u.mem_write(OBJECT,bytes(48));put(OBJECT+4,'H',kind);put(OBJECT+0x1c,'I',1)
            put(OBJECT+0x20,'H',0xab);put(PLAYER+0xbc,'H',8 if mapped else 0);put(PLAYER+0x78,'H',weapon)
            call(address)
            assert bool(get(OBJECT+0x1c)&1)==(not mapped)
            animation=get(OBJECT+0x20,'H')
            assert animation==(0xab if mapped else base+weapon-1),(weapon,hex(address),animation)
            if not mapped:
                at=struct.unpack_from('<I',ROM,0x44b36c+4*animation)[0]-0x08000000
                assert struct.unpack_from('<H',ROM,at)[0]==expected[weapon-1]
            icon_cases+=1
ammo_limits=[struct.unpack_from('<H',ROM,0x44bc8c+12*i+6)[0] for i in range(10)]
assert ammo_limits==[999,999,100,300,300,25,10,3,10,10]
constructor_cases=0;types=set();animations=set();counts={}
allocating=True
for level in range(16):
    for players in range(1,5):
        for motion in range(players):
            allocated.clear();put(GAME+0x10,'B',players);put(GAME+0x68,'B',motion);put(LEVEL+0x10,'H',level)
            call(0x080101f4)
            assert len(allocated)==(74 if players==1 else 59+2*players)
            histogram=Counter(get(obj+4,'H') for obj in allocated)
            assert 5 not in histogram and 11 not in histogram
            types.update(histogram);animations.update(get(obj+0x20,'H') for obj in allocated)
            counts[str(players)]=dict(sorted(histogram.items()));constructor_cases+=1
assert not (animations & set(range(0x17,0x24)))
refs={}
for target in (0xe700,0xefb4):
    found=[]
    for at in range(0,0xc6000-2,2):
        a,b=struct.unpack_from('<HH',ROM,at)
        if a&0xf800!=0xf000 or b&0xf800!=0xf800:continue
        delta=((a&2047)<<12)|((b&2047)<<1)
        if delta&0x400000:delta-=0x800000
        if at+4+delta==target:found.append(f'0x{0x08000000+at:08X}')
    refs[f'0x{0x08000000+target:08X}']=found
asset_mapping=[];referenced_frames=set()
for animation in range(233):
    off=struct.unpack_from('<I',ROM,0x44b36c+animation*4)[0]-0x08000000
    words=[]
    while True:
        value=struct.unpack_from('<H',ROM,off)[0];words.append(value);off+=2
        if value>=0xfffe:break
    referenced_frames.update(value&0x3fff for value in words if value<0xfffa)
    if 0x17<=animation<0x24:asset_mapping.append({'animation':f'0x{animation:02X}','words':words})
unused_green_icons=[image for image in range(67,76) if image not in referenced_frames]
assert unused_green_icons==list(range(67,76))
report=dict(result='pass',rom_sha1=hashlib.sha1(ROM).hexdigest(),
    health_digit_original_cases=health_cases,fixed_digit_original_cases=score_cases,
    weapon_ammo_icon_original_cases=icon_cases,retail_weapon_images=retail_weapon_images,
    retail_ammo_images=retail_ammo_images,ammo_limits=ammo_limits,
    original_constructor_cases=constructor_cases,constructor_types=sorted(types),
    constructor_counts_by_players=counts,controller_direct_callers=refs,legacy_animation_mapping=asset_mapping,
    green_weapon_icons_without_animation_frames=unused_green_icons,
    scope='Complete original E700/EFB4 and F1C0/F27C handlers, including original FC04/arithmetic and retail weapon/ammo animation images/capacities; original101F4 constructor with only F9B4 allocation controlled.16maps,1..4players,all motion indices. Static Thumb direct-call scan through080C6000; indirect/computed reachability is not proved. GAME+34C meaning is unresolved. Old full layout/gauges are not recovered. No game runtime or ROM modifications.')
report['source_sha256']={'scripts/audit_legacy_hud.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
(ROOT/'analysis/legacy-hud-audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
