"""Complete1240C with original12050/11FFC/FC04; only F9B4 allocation controlled."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1];ROM=(ROOT/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(ROM).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
u8,u16,u32,i16,i32=C.c_uint8,C.c_uint16,C.c_uint32,C.c_int16,C.c_int32
nodes=[n for n in ast.parse((ROOT/'tests/test_weapon_controller_rom.py').read_text()).body if isinstance(n,ast.ClassDef) and n.name in ('Inventory','Control','Object')]
exec(compile(ast.Module(nodes,type_ignores=[]),'weapon projections','exec'))
OP=C.POINTER(Object)
class Rules(C.Structure):_fields_=[('capacity',u16*10)]
class Loadouts(C.Structure):_fields_=[('ammo',(i16*9)*36),('loaded',C.c_uint)]
class Player(C.Structure):_fields_=[('inventory',C.POINTER(Inventory)),('control',C.POINTER(Control)),('object',OP),('cooldowns',C.POINTER(u16))]
class State(C.Structure):_fields_=[('updating',C.c_uint)]
ALLOC=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.POINTER(OP))
class Ops(C.Structure):_fields_=[('allocate',ALLOC),('context',C.c_void_p)]
B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=['weapon_loadout.c','weapon_select.c','weapon_animation.c','inventory.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),*[str(ROOT/'src'/f) for f in sources],'-o',str(B/'libssa_weapon_loadout.so')],check=True)
lib=C.CDLL(str(B/'libssa_weapon_loadout.so'))
lib.ssa_weapon_loadouts_load.argtypes=[C.POINTER(Loadouts),C.c_void_p,C.c_size_t]
lib.ssa_inventory_rules_load.argtypes=[C.POINTER(Rules),C.c_void_p,C.c_size_t]
run=lib.ssa_weapon_loadout_initialize;run.argtypes=[C.POINTER(State),C.POINTER(Player),C.c_uint,C.POINTER(Loadouts),C.POINTER(Rules),C.c_uint,u8,C.POINTER(Ops)]
base=Loadouts();assert lib.ssa_weapon_loadouts_load(C.byref(base),ROM,len(ROM))==1
baserules=Rules();assert lib.ssa_inventory_rules_load(C.byref(baserules),ROM,len(ROM))==0
for row in range(36):assert list(base.ammo[row])==list(struct.unpack_from('<9h',ROM,0x44bcf0+(row+1)*20))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(ROM));uc.mem_write(0x08000000,ROM)
uc.mem_map(0x02000000,0x100000);uc.mem_map(0x03000000,0x10000)
P,O,G,SETTINGS,STOP=0x02000000,0x02004000,0x02008000,0x02009000,0x087ff000
uc.mem_write(0x03004648,struct.pack('<I',P));uc.mem_write(0x03004010,struct.pack('<I',G));uc.mem_write(0x0300572c,struct.pack('<I',SETTINGS))
rng=random.Random(0x1240c);state=State();context={};logs=[[],[]]
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_rom_comparisons':0,'native_guards':0,'callback_reentry_guards':0,'native_failure_paths':0,'allocation_calls':0,'scope':__doc__+'; all36table rows plus setting0,player counts0..4,active masks,godmode,signedtable/capacity fuzz; full256byteplayer and48bytesprite snapshots'}
def rd(d,o,f='I'):return struct.unpack_from('<'+f,d,o)[0]
def wr(d,o,v,f='I'):struct.pack_into('<'+f,d,o,v&({'I':0xffffffff,'H':65535,'B':255}[f]))
def playerbytes(i):
 raw=bytearray(context['raw'][i]);p=context['players'][i];inv=p.inventory.contents;c=p.control.contents
 wr(raw,40,c.current);wr(raw,48,c.flags);wr(raw,52,inv.weapons,'H');wr(raw,120,c.requested,'H');wr(raw,122,inv.pending_count,'H');wr(raw,188,c.keys,'H')
 if p.object:wr(raw,44,O+i*48)
 for j in range(10):wr(raw,54+j*2,inv.ammo[j],'H');wr(raw,74+j*2,p.cooldowns[j],'H')
 return bytes(raw)
def snapshot():return tuple(playerbytes(i) for i in range(4))+tuple(bytes(o) for o in context['objects'])
def args():return [C.byref(state),context['players'],context['count'],C.byref(context['table']),C.byref(context['rules']),context['setting'],context['god'],C.byref(ops)]
def allocate(ctx,index,out):
 assert run(*args())==-1;report['callback_reentry_guards']+=1
 logs[0].append((index,snapshot()))
 if context.get('fail_index')==index:return -1
 if context.get('null_index')==index:return 0
 out[0]=C.pointer(context['objects'][index]);return 0
ac=ALLOC(allocate);ops=Ops(ac,None)
def hook(machine,address,size,unused):
 index=context['active'][len(logs[1])]
 logs[1].append((index,tuple(bytes(machine.mem_read(P+i*256,256)) for i in range(4))+tuple(bytes(machine.mem_read(O+i*48,48)) for i in range(4))))
 machine.reg_write(UC_ARM_REG_R0,O+index*48);machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook,begin=0x0800f9b4,end=0x0800f9b4)
edge=[-32768,-32767,-999,-1,0,1,2,999,32766,32767]
def compare(count,setting,god,mask,mutate=False):
 raw=[bytearray(rng.randbytes(256)) for _ in range(4)];objects=(Object*4)();inventories=(Inventory*4)();controls=(Control*4)();cooldowns=((u16*10)*4)();players=(Player*4)()
 for i in range(4):
  wr(raw[i],188,(rd(raw[i],188,'H')&~4)|(4 if mask&(1<<i) else 0),'H')
  wr(raw[i],44,0x02020000+i*48)
  c=controls[i];c.current=rd(raw[i],40);c.flags=rd(raw[i],48);c.requested=rd(raw[i],120,'H');c.keys=rd(raw[i],188,'H')
  inv=inventories[i];inv.weapons=rd(raw[i],52,'H');inv.pending_count=rd(raw[i],122,'H')
  for j in range(10):inv.ammo[j]=rd(raw[i],54+j*2,'h');cooldowns[i][j]=rd(raw[i],74+j*2,'H')
  objects[i]=Object.from_buffer_copy(rng.randbytes(48));players[i]=Player(C.pointer(inv),C.pointer(c),OP(),cooldowns[i])
  uc.mem_write(P+i*256,bytes(raw[i]));uc.mem_write(O+i*48,bytes(objects[i]))
 table=Loadouts.from_buffer_copy(base);rules=Rules.from_buffer_copy(baserules)
 if mutate:
  for j in range(9):table.ammo[setting-1][j]=rng.choice(edge)
  uc.mem_write(0x0844bcf0+setting*20,struct.pack('<9h',*table.ammo[setting-1]))
  for j in range(10):
   rules.capacity[j]=rng.choice(edge)&65535
   uc.mem_write(0x0844bc8c+j*12+6,struct.pack('<H',rules.capacity[j]))
 context.clear();context.update(raw=raw,objects=objects,players=players,table=table,rules=rules,count=count,setting=setting,god=god,active=[i for i in range(count) if mask&(1<<i)])
 logs[0].clear();logs[1].clear();uc.mem_write(G+16,bytes([count]));uc.mem_write(SETTINGS+38,bytes([god,setting]))
 assert run(*args())==1 and not state.updating
 uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(0x0801240d,STOP,count=100000)
 assert uc.reg_read(UC_ARM_REG_PC)==STOP
 assert logs[0]==logs[1],(count,setting,god,mask,'allocation order/snapshot')
 actual=tuple(bytes(uc.mem_read(P+i*256,256)) for i in range(4))+tuple(bytes(uc.mem_read(O+i*48,48)) for i in range(4))
 assert snapshot()==actual,(count,setting,god,mask,[(k,[(i,x,y) for i,(x,y) in enumerate(zip(a,b)) if x!=y]) for k,(a,b) in enumerate(zip(snapshot(),actual)) if a!=b])
 if mutate:
  uc.mem_write(0x0844bcf0+setting*20,ROM[0x44bcf0+setting*20:0x44bcf0+setting*20+18])
  for j in range(10):uc.mem_write(0x0844bc8c+j*12+6,struct.pack('<H',baserules.capacity[j]))
 report['whole_rom_comparisons']+=1;report['allocation_calls']+=len(logs[0])
for setting in range(37):
 for count in range(5):
  for mask in range(16):
   for god in (0,1,255):compare(count,setting,god,mask)
print('PASS all loadout settings,player counts/active masks and god-mode states',flush=True)
for _ in range(4096):compare(rng.randrange(5),rng.randrange(1,37),rng.choice([0,0,0,1,255]),rng.randrange(16),True)
print('PASS signed loadout/capacity ROM fuzz',flush=True)
# Invalid integration leaves all existing state unchanged.
compare(4,1,0,15)
valid=args();before=snapshot()
for index in (0,1,3,4,7):
 invalid=valid.copy();invalid[index]=None;assert run(*invalid)==-1;report['native_guards']+=1
for index,value in [(2,5),(5,37),(5,255)]:
 invalid=valid.copy();invalid[index]=value;assert run(*invalid)==-1;report['native_guards']+=1
state.updating=1;assert run(*valid)==-1;state.updating=0;report['native_guards']+=1
context['table'].loaded=0;assert run(*valid)==-1;context['table'].loaded=1;report['native_guards']+=1
assert snapshot()==before
for field in ('control','inventory','cooldowns'):
 p=context['players'][3];old=getattr(p,field);saved=C.cast(old,C.c_void_p).value
 typ=dict(Player._fields_)[field];setattr(p,field,typ());assert run(*valid)==-1
 setattr(p,field,C.cast(saved,typ));report['native_guards']+=1
assert snapshot()==before
for field in ('fail_index','null_index'):
 compare(4,2,0,15);context[field]=2
 assert run(*args())==-1 and not state.updating;report['native_failure_paths']+=1
 assert context['players'][0].object and context['players'][1].object
for size in (0,0x44bfd3):
 out=Loadouts.from_buffer_copy(base);old=bytes(out);assert lib.ssa_weapon_loadouts_load(C.byref(out),ROM,size)==-1 and bytes(out)==old;report['native_guards']+=1
assert lib.ssa_weapon_loadouts_load(None,ROM,len(ROM))==-1;report['native_guards']+=1
out=Loadouts.from_buffer_copy(base);assert lib.ssa_weapon_loadouts_load(C.byref(out),None,len(ROM))==-1;report['native_guards']+=1
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/weapon-loadout-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
