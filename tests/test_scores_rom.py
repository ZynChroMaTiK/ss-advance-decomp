"""Focused original1737C Scores viewer: ARM execution is a test oracle only."""
from pathlib import Path
import hashlib,json,runpy
r=Path(__file__).resolve().parents[1]
o=runpy.run_path(str(r/'tests/test_menu_rom.py'),run_name='ssa_menu_oracle')
fixture,compare,uc,rom=(o[k] for k in ('fixture','compare','uc','rom'))
for language in range(3):
 m=fixture(13,language)
 uc.mem_write(0x02000040,rom[0xcc00c:0xcc00c+18*16])
 for tick in range(72):compare(m,0,tick in (10,31,50,71))
 assert m.score_y==16*65536 and not m.score_speed
 for key in (1,2,2,1,1,2):
  compare(m,key,True)
  for tick in range(88):
   if not m.score_speed:break
   compare(m,0,tick%80==0)
  assert not m.score_speed
 compare(m,8,True);compare(m,0,True);assert m.page==0
state=compare.__globals__
report={'result':'pass','score_rom_frames':state['score_frames'],
 'composed_rom_pixels':state['pixels'],'languages':3,'entries':18,
 'scope':'Original1737C seeded Scores viewer state, commands, glyphs and pixels. New record entry/persistence are not implemented.',
 'test_only_arm_execution':True,
 'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (
 r/'src/menu.c',r/'include/ssa/menu.h',r/'tests/test_map_view_rom.py',r/'tests/test_menu_rom.py',Path(__file__))}}
(r/'analysis/scores-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
