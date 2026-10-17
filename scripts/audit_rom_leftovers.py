"""Read-only ROM investigation: dormant language slots, boot secrets and art usage.

Unicorn is used only as an isolated original-routine test oracle. Display/audio
calls are observed or skipped; this does not emulate a playable game.
"""
from pathlib import Path
import hashlib
import json
import struct
import zipfile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
from export_graphics_archive import png

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/rom-leftovers'
OUT.mkdir(parents=True, exist_ok=True)
ROM = (ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
u16 = lambda data, at: struct.unpack_from('<H', data, at)[0]
u32 = lambda data, at: struct.unpack_from('<I', data, at)[0]
SETTINGS, STOP = 0x02001000, 0x087ff000
report = {'rom_sha1': hashlib.sha1(ROM).hexdigest(), 'scope': __doc__, 'boot_cases': [], 'language_cases': [], 'menu_cases': []}


def run(address, inputs, selection=0, unlocked=0, god=0):
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    for base, size in [(0x02000000,0x40000),(0x03000000,0x10000),(0x04000000,0x1000),(0x05000000,0x1000),(0x06000000,0x20000),(0x08000000,len(ROM))]:
        uc.mem_map(base,size)
    uc.mem_write(0x08000000,ROM)
    uc.mem_write(0x0300572c,struct.pack('<I',SETTINGS))
    uc.mem_write(SETTINGS+6,struct.pack('<H',selection))
    uc.mem_write(SETTINGS+0x28,bytes([unlocked]))
    uc.mem_write(SETTINGS+0x26,bytes([god]))
    uc.reg_write(UC_ARM_REG_SP,0x03007f00)
    uc.reg_write(UC_ARM_REG_LR,STOP|1)
    polls = iter(inputs); log = {'images': [], 'text_ids': []}
    def hook(u, pc, size, _):
        skip = False
        if pc == 0x08003028:
            u.mem_write(0x030042f0,struct.pack('<I',next(polls))); skip = True
        elif pc == 0x080c5328:
            log['images'].append(hex(u.reg_read(UC_ARM_REG_R0))); skip = True
        elif pc == 0x080168e4:
            log['text_ids'].append(u.reg_read(UC_ARM_REG_R0))
            u.reg_write(UC_ARM_REG_R0,u.reg_read(UC_ARM_REG_R2)+12); skip = True
        elif pc in (0x080162e0,0x08015848,0x08015868,0x08015708,0x08008eb0):
            skip = True
        if skip: u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE,hook)
    uc.emu_start(address|1,STOP,count=20000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    state = bytes(uc.mem_read(SETTINGS,0x40))
    log.update(selection=u16(state,6),language=u16(state,0),menu=u16(state,4),unlock=state[0x28],god=state[0x26])
    return log


# Run complete original boot routine; only presentation and input boundaries hooked.
for inputs, expected in [([0xfc00]*4,0),([0xfe00,0xfd00,0xfc00,0xfc02],3),
                         ([0xfe00,0xfc00,0xfc00,0xfc02],0),
                         ([0xfe01,0xfd00,0xfc00,0xfc02],0),
                         ([0xfe00,0xfd00,0xfc00,0xfc03],0),
                         ([0xfc00,0xfc00,0xfda9,0xfc00],0),
                         ([0xfc00,0xfc00,0xfd59,0xfc00],0)]:
    result=run(0x08016328,inputs)
    assert result['unlock']==expected
    if inputs[2] == 0xfda9: assert result['images'][-1]=='0x80b7fa4'
    if inputs[2] == 0xfd59: assert result['images'][-1]=='0x80bd4dc'
    report['boot_cases'].append(dict(raw_inputs=[hex(i) for i in inputs],result=result))

# Language menu draws only English/German/French; attempts to go below/above clamp.
for selection, keys, expected in [(0,0x400000,0),(2,0x800000,2),(0,0x800000,1),(1,0x800000,2)]:
    result=run(0x08017bbc,[keys],selection)
    assert result['selection']==expected and result['text_ids']==[242,237,238,239]
    report['language_cases'].append(dict(selection=selection,keys=hex(keys),result=result))
for selected in range(3):
    result=run(0x08017bbc,[0x10000],selected)
    assert result['language']==selected
    report['language_cases'].append(dict(confirm=selected,result=result))
tables=[u32(ROM,0x44cf34+4*i) for i in range(5)]
assert tables==[0x0844c364,0x0844c754,0x0844cb44,0,0]
report['language_tables']=[hex(p) for p in tables]
report['extra_language_labels']={}
for index in (240,241):
    p=u32(ROM,0x44c364+index*4)-0x08000000
    report['extra_language_labels'][str(index)]={'address':hex(p+0x08000000),'text':ROM[p:ROM.index(0,p)].decode('ascii')}

# Hidden main-menu entry gates and actual cheat handler behavior.
for unlocked in (0,3):
    result=run(0x08016a34,[0],unlocked=unlocked)
    assert (12 in result['text_ids']) == bool(unlocked)
    report['menu_cases'].append(dict(handler='main',unlocked=unlocked,result=result))
for god in (0,1):
    result=run(0x0801820c,[0x10000],selection=1,god=god)
    assert result['god']==1-god
    report['menu_cases'].append(dict(handler='cheats',initial_god=god,result=result))
result=run(0x0801820c,[0x10000],selection=0)
assert result['menu']==7
report['menu_cases'].append(dict(handler='cheats_level_select',result=result))

# Export the two secret splash images and four ordinary logos, as originally drawn.
palette=list(struct.unpack_from('<256H',ROM,0xa8168))
for offset in (0xa5df4,0xa4df4,0xa45c4,0xa3ef8,0xb7fa4,0xbd4dc):
    assert ROM[offset]==0x30
    length=u32(ROM,offset)>>8; pixels=bytearray(); p=offset+4
    while len(pixels)<length:
        control=ROM[p]; p+=1
        if control&128:
            n=(control&127)+3; pixels.extend(bytes([ROM[p]])*n); p+=1
        else:
            n=control+1; pixels.extend(ROM[p:p+n]); p+=n
    assert len(pixels)==length==240*160
    (OUT/f'splash-{offset+0x08000000:08X}.png').write_bytes(png(240,160,pixels,palette,False))

# Inventory texture names against every original map texture table.
def map_data(level):
    p=u32(ROM,0x44aef4+4*level)-0x08000000
    assert ROM[p]==0x10
    length=u32(ROM,p)>>8; p+=4; data=bytearray()
    while len(data)<length:
        flags=ROM[p]; p+=1
        for bit in range(7,-1,-1):
            if len(data)==length: break
            if flags&(1<<bit):
                pair=(ROM[p]<<8)|ROM[p+1];p+=2
                n=(pair>>12)+3; distance=(pair&4095)+1
                for _ in range(n):data.append(data[-distance])
            else: data.append(ROM[p]);p+=1
    assert data[:4]==b'WOD0'
    return data

names=[set(),set()]
report['map_texture_names']=[]
for level in range(16):
    data=map_data(level); count=u16(data,8); table=u32(data,0x20)
    level_names=[bytes(data[table+i*16:table+i*16+8]).split(b'\0')[0].decode('ascii').upper() for i in range(count)]
    family=0 if level<=5 or level in (12,13) else 1
    names[family].update(level_names)
    report['map_texture_names'].append(dict(level=level,names=level_names))
with zipfile.ZipFile(ROOT/'build/exports/ssa-graphics.zip') as archive:
    graphics=json.loads(archive.read('manifest.json'))
    report['textures_absent_from_all_map_texture_tables']=[]
    for texture in graphics['textures']:
        if texture['mip'] or texture['name'].upper() in names[texture['family']]:continue
        # Door/switch replacement names are potential runtime uses, not proof of disuse.
        report['textures_absent_from_all_map_texture_tables'].append(texture)
        preview=OUT/'unused-textures'/f"family-{texture['family']}-{Path(texture['png']).name}"
        preview.parent.mkdir(exist_ok=True)
        preview.write_bytes(archive.read(texture['png']))
report['texture_usage_caveat']='Absence from map texture tables does not rule out runtime replacement, menus or other consumers.'
report['diagnostic']={'handler':'0x0801B464','frame':'0x0801B208','state_table_entry':'0x0844A660','state_index':8,'status':'Present in original state table; no normal entry path established. Displays remote player buttons and link errors.'}
report['build_stamp']={'format':'0x080CC390','time':'15:31:24','date':'Mar 15 2004','draw_handler':'0x08017CD0','gate':'settings+0x28'}
report['classification']={'spanish_italian':'Unimplemented language options: labels survive, menu omits them and translation pointers are null.',
    'cheat_menu':'Hidden but reachable via original splash-input sequence; not proven scrapped.',
    'secret_splashes':'Hidden but reachable; not scrapped.',
    'link_diagnostic':'Development diagnostic retained; normal reachability unproven.'}
assert hashlib.sha1((ROOT/'orig/SSA.gba').read_bytes()).hexdigest()==report['rom_sha1']
(OUT/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'result':'PASS','boot_cases':len(report['boot_cases']),'language_cases':len(report['language_cases']),'menu_cases':len(report['menu_cases']),
                  'texture_candidates':[(t['family'],t['name']) for t in report['textures_absent_from_all_map_texture_tables']], 'evidence':str(OUT/'evidence.json')},indent=2))
