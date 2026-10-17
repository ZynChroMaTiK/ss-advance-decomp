"""Whole 12D5C/12D9C/12E1C routines; controlled group/release boundaries."""
from pathlib import Path
import ast
import ctypes as C
import hashlib
import json
import random
import struct
import subprocess
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

ROOT = Path(__file__).resolve().parents[1]
ROM = (ROOT / 'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'
nodes = []
for n in ast.parse((ROOT / 'tests/test_spawn_rom.py').read_text()).body:
    if isinstance(n, ast.ClassDef): nodes.append(n)
    elif isinstance(n, ast.Assign) and any(
        isinstance(t, ast.Attribute) and t.attr == '_fields_' or
        isinstance(t, ast.Name) and t.id == 'EP' for t in n.targets): nodes.append(n)
exec(compile(ast.Module(nodes, type_ignores=[]), 'entity declarations', 'exec'))
class View(C.Structure):
    _fields_ = [('entity', EP), ('health', C.c_int16), ('timer', C.c_uint16),
                ('saved_facing', C.c_uint16), ('state', C.c_uint8),
                ('facing', C.c_uint8), ('boss_ticks', C.c_uint8), ('updating', C.c_uint)]
EVENT = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(View), C.c_int)
class Ops(C.Structure):
    _fields_ = [('event', EVENT), ('context', C.c_void_p)]
BUILD = ROOT / 'build/native-tests'; BUILD.mkdir(parents=True, exist_ok=True)
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
    '-shared', '-fPIC', '-fsanitize=undefined', '-fno-sanitize-recover=all',
    '-I', str(ROOT / 'include'), str(ROOT / 'src/enemy_lifecycle.c'),
    '-o', str(BUILD / 'libssa_enemy_lifecycle.so')], check=True)
lib = C.CDLL(str(BUILD / 'libssa_enemy_lifecycle.so'))
death = lib.ssa_enemy_death_begin
death.argtypes = [C.POINTER(View), C.POINTER(Ops)]
corpse = lib.ssa_enemy_corpse_step
corpse.argtypes = [C.POINTER(View), C.c_uint, C.POINTER(Ops), C.c_uint]
drain = lib.ssa_enemy_drain_step
drain.argtypes = [C.POINTER(View), C.c_uint16, C.POINTER(C.c_uint16), C.c_uint]
for f in (death, corpse, drain): f.restype = C.c_int
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x08000000, len(ROM)); uc.mem_write(0x08000000, ROM)
uc.mem_map(0x02000000, 0x10000); uc.mem_map(0x03000000, 0x10000)
E, G, STOP = 0x02000000, 0x02001000, 0x087ff000
uc.mem_write(0x03004010, struct.pack('<I', G))
oracle_events = []
def hook(machine, address, size, user):
    assert machine.reg_read(UC_ARM_REG_R0) == E
    oracle_events.append((0 if address == 0x0801c6b0 else 1, bytes(machine.mem_read(E, 0x74))))
    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
for addr in (0x0801c6b0, 0x080037e0):
    uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr)
fields = [('body.x',0,'i'), ('body.y',4,'i'), ('body.z',8,'i'),
    ('body.velocity_x',12,'i'), ('body.velocity_y',16,'i'), ('body.velocity_z',20,'i'),
    ('radius',0x28,'i'), ('body.height',0x2c,'i'), ('body.flags',0x30,'H'),
    ('type',0x3c,'H'), ('body.hover_offset',0x43,'b'), ('body.step_adjust',0x53,'b'),
    ('player_index',0x54,'B'), ('sector',0x6a,'h'), ('leaf',0x6e,'h'),
    ('body.ceiling',0x70,'h'), ('body.floor',0x72,'h')]
vfields = [('health',0x32,'h'), ('state',0x3e,'B'), ('facing',0x3f,'B'),
    ('saved_facing',0x48,'H'), ('timer',0x4e,'H'), ('boss_ticks',0x51,'B')]
def setfield(obj,path,value):
    parts = path.split('.')
    for p in parts[:-1]: obj = getattr(obj,p)
    setattr(obj,parts[-1],value)
def getfield(obj,path):
    for p in path.split('.'): obj = getattr(obj,p)
    return obj
rng = random.Random(0x12d5c)
report = {'rom_sha1': hashlib.sha1(ROM).hexdigest(),
    'scope': 'Whole 12D5C death entry,12D9C corpse update,12E1C periodic drain; group release 1C6B0 and unlink/destroy 037E0 controlled callbacks; complete entity bytes and ordered callback snapshots',
    'rom_comparisons': {'death':0,'corpse':0,'drain':0},
    'native_guards':0, 'callback_reentry_guards':0, 'callback_failure_cases':0}
def compare(kind, ticks=0, overrides=()):
    raw = bytearray(rng.randbytes(0x74))
    for off,fmt,value in overrides: struct.pack_into('<'+fmt,raw,off,value)
    e,v = Entity(),View()
    for path,off,fmt in fields: setfield(e,path,struct.unpack_from('<'+fmt,raw,off)[0])
    for path,off,fmt in vfields: setfield(v,path,struct.unpack_from('<'+fmt,raw,off)[0])
    v.entity = C.pointer(e)
    countdown = C.c_uint16(struct.unpack_from('<H',raw,0x36)[0])
    period = struct.unpack_from('<H',raw,0x34)[0]
    def project():
        out = bytearray(raw)
        for obj,fs in ((e,fields),(v,vfields)):
            for path,off,fmt in fs: struct.pack_into('<'+fmt,out,off,getfield(obj,path))
        struct.pack_into('<H',out,0x36,countdown.value)
        return bytes(out)
    uc.mem_write(E,bytes(raw)); uc.mem_write(G+7,bytes([ticks])); oracle_events.clear()
    for r,value in ((UC_ARM_REG_R0,E),(UC_ARM_REG_SP,0x03007e00),(UC_ARM_REG_LR,STOP|1)):
        uc.reg_write(r,value)
    uc.emu_start({'death':0x08012d5d,'corpse':0x08012d9d,'drain':0x08012e1d}[kind],STOP,count=500)
    assert uc.reg_read(UC_ARM_REG_PC) == STOP
    expected = bytes(uc.mem_read(E,0x74)); events = list(oracle_events); native_events=[]; reentry=[]
    @EVENT
    def callback(ctx,vp,ev):
        native_events.append((ev,project()))
        reentry.extend([death(vp,C.byref(ops)), corpse(vp,0,C.byref(ops),0),
                        drain(vp,1,C.byref(countdown),0)])
        return 0
    ops = Ops(callback,None)
    if kind == 'death': result = death(C.byref(v),C.byref(ops))
    elif kind == 'corpse': result = corpse(C.byref(v),raw[0x5f],C.byref(ops),ticks)
    else: result = drain(C.byref(v),period,C.byref(countdown),ticks)
    assert result == int(kind == 'corpse' and bool(events)) and not v.updating
    assert project() == expected, (kind,ticks,overrides,[(i,a,b) for i,(a,b) in enumerate(zip(project(),expected)) if a!=b])
    assert native_events == events, (kind,native_events,events)
    assert all(x == -1 for x in reentry)
    report['rom_comparisons'][kind] += 1
    report['callback_reentry_guards'] += len(reentry)

for typ in list(range(0xc0,0xe2)) + [0,1,0xffff]:
    for flags in [0,1,2,3,0x40,0x80,0xffff]:
        compare('death',overrides=[(0x3c,'H',typ),(0x30,'H',flags)])
for timer in [0,1,118,119,120,121,255,256,360,720,0x7fff,0x8000,0xfffe,0xffff]:
    for ticks in [0,1,2,119,120,127,255]:
        for done in [0,1,255]:
            for z,floor in [(0,0),(-1,0),(0,-32768),(0x7fffffff,32767),(-0x80000000,-32768),(65536,-1)]:
                compare('corpse',ticks,[(0x4e,'H',timer),(0x5f,'B',done),(8,'i',z),(0x72,'h',floor)])
for health in [-32768,-1,0,1,2,32767]:
    for period in [0,1,0x7fff,0x8000,0xffff]:
        for timer in [0,1,255,0x7fff,0x8000,0xffff]:
            for ticks in [0,1,2,255]:
                compare('drain',ticks,[(0x32,'h',health),(0x34,'H',period),(0x36,'H',timer)])
for kind in ('death','corpse','drain'):
    for _ in range(4096): compare(kind,rng.randrange(256))

e,v = Entity(),View(); v.entity = C.pointer(e); countdown=C.c_uint16(7)
@EVENT
def ok(ctx,vp,ev): return 0
@EVENT
def fail(ctx,vp,ev): return -1
ops=Ops(ok,None); badops=Ops(); failureops=Ops(fail,None)
checks = [lambda: death(None,C.byref(ops)), lambda: corpse(None,0,C.byref(ops),0),
          lambda: drain(None,1,C.byref(countdown),0), lambda: death(C.byref(v),None),
          lambda: corpse(C.byref(v),0,None,0), lambda: death(C.byref(v),C.byref(badops)),
          lambda: corpse(C.byref(v),0,C.byref(badops),0),
          lambda: corpse(C.byref(v),256,C.byref(ops),0),
          lambda: corpse(C.byref(v),0,C.byref(ops),256),
          lambda: drain(C.byref(v),1,None,0), lambda: drain(C.byref(v),1,C.byref(countdown),256)]
for check in checks:
    assert check() == -1; report['native_guards'] += 1
for mode in ('nullentity','updating','query'):
    e=Entity(); v=View(); v.entity=C.pointer(e); world=World()
    if mode == 'nullentity': v.entity=EP()
    elif mode == 'updating': v.updating=1
    else: e.owner=C.pointer(world); world.query_active=1
    for result in (death(C.byref(v),C.byref(ops)),corpse(C.byref(v),0,C.byref(ops),0),
                   drain(C.byref(v),1,C.byref(countdown),0)):
        assert result == -1; report['native_guards'] += 1
e=Entity(); v=View(); v.entity=C.pointer(e); v.health=99; v.timer=17; e.body.flags=0xffff
assert death(C.byref(v),C.byref(failureops)) == -1
assert (v.health,v.timer,e.body.flags,v.updating) == (0,17,0xfffd,0)
report['callback_failure_cases'] += 1
v.timer=0; e.body.z=65536; e.body.floor=0
assert corpse(C.byref(v),1,C.byref(failureops),1) == -1
assert (v.timer,e.body.z,e.body.flags,v.updating) == (0,0,0xffbc,0)
report['callback_failure_cases'] += 1
report['source_sha256'] = hashlib.sha256((ROOT/'src/enemy_lifecycle.c').read_bytes()).hexdigest()
report['total_rom_comparisons'] = sum(report['rom_comparisons'].values())
report['status'] = 'passed'
(ROOT/'analysis/native-enemy-lifecycle-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
