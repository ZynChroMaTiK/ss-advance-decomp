"""Focused original1737C Scores viewer: ARM execution is a test oracle only."""
from pathlib import Path
import ctypes as C,hashlib,json,random,runpy,struct
from unicorn.arm_const import UC_ARM_REG_R7,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1]
o=runpy.run_path(str(r/'tests/test_menu_rom.py'),run_name='ssa_menu_oracle')
fixture,compare,uc,rom=(o[k] for k in ('fixture','compare','uc','rom'))
lib=o['lib'];lib.ssa_scores_rank.argtypes=[C.POINTER(o['Scores']),C.c_uint32]
def seed(m):
 uc.reg_write(UC_ARM_REG_R7,0x0300572c)
 uc.emu_start(0x08015531,0x08015572,count=10000)
 assert uc.reg_read(UC_ARM_REG_PC)==0x08015572
 assert bytes(m.scores.entries)==bytes(uc.mem_read(0x02000040,288))
for language in range(3):
 m=fixture(13,language)
 # Execute the ORIGINAL initializer loop, rather than treating the ROM's
 # zero-score name templates as a populated leaderboard.
 seed(m)
 records=bytes(uc.mem_read(0x02000040,18*16))
 for i in range(18):
  address=struct.unpack_from('<I',rom,0x44c18c+i*4)[0]-0x08000000
  assert records[i*16:i*16+4]==rom[0x44c144+i*4:0x44c148+i*4]
  assert records[i*16+4:(i+1)*16]==rom[address:address+12]
 for tick in range(72):compare(m,0,tick in (10,31,50,71))
 assert m.score_y==16*65536 and not m.score_speed
 for key in (1,2,2,1,1,2):
  compare(m,key,True)
  for tick in range(88):
   if not m.score_speed:break
   compare(m,0,tick%80==0)
  assert not m.score_speed
 compare(m,8,True);compare(m,0,True);assert m.page==0
rank_cases=name_frames=entries=0
rng=random.Random(0x16f70)
m=fixture(13);seed(m)
for trial in range(2048):
 values=sorted((rng.getrandbits(32) for _ in range(18)),reverse=True)
 for i,value in enumerate(values):m.scores.entries[i].score=value
 uc.mem_write(0x02000040,bytes(m.scores.entries))
 for score in (rng.getrandbits(32),values[trial%18],0,0xffffffff):
  expected=next((i for i,v in enumerate(values) if v<=score),18)
  assert lib.ssa_scores_rank(C.byref(m.scores),score)==o['call'](0x08016f70,score)==expected
  rank_cases+=1
for language in range(3):
 for score in (0xffffffff,2000000,1000001,1000000,900001,500000,450001,250000,100001,100000,99999,0):
  m=fixture(14,language);seed(m)
  uc.mem_write(0x02000488,struct.pack('<I',score));uc.mem_write(0x0200002e,b'____________\0')
  assert lib.ssa_menu_score_begin(C.byref(m),score)==1
  compare(m,0,True);name_frames+=1
  if m.page==0:continue
  for key in (1,64,2,3,32,32,64,1,8,0):compare(m,key);name_frames+=1
  # Long holds cover the original repeat delay and255->32 byte wrap.
  if score==1000000:
   for held in (1,2,3):
    for tick in range(280):compare(m,held if tick==0 else 0,tick in (31,255),held=held);name_frames+=1
  compare(m,4,True);name_frames+=1
  assert m.page==13 and m.score_submitted and m.score_highlight
  for tick in range(250):
   compare(m,0,tick in (0,71,159,247))
   if not m.score_speed:break
  else:raise AssertionError('Record viewer did not stop at the entered group')
  compare(m,0,True);entries+=1
state=compare.__globals__
report={'result':'pass','score_rom_frames':state['score_frames'],
 'composed_rom_pixels':state['pixels'],'languages':3,'entries':18,
 'ranking_rom_cases':rank_cases,'name_rom_frames':name_frames,'submitted_entries':entries,
 'scope':'Original15530 initialization,16F70 ranking,16F98 name editor/entry and1737C viewer, including record auto-scroll/highlight. Persistence and PC typing have separate native tests.',
 'test_only_arm_execution':True,
 'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (
 r/'src/menu.c',r/'src/scores.c',r/'include/ssa/scores.h',r/'include/ssa/menu.h',r/'tests/test_map_view_rom.py',r/'tests/test_menu_rom.py',Path(__file__))}}
(r/'analysis/scores-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
