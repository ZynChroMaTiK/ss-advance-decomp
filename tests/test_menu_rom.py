"""Original menu callbacks/VRAM/OAM vs native C. ARM is a test oracle only."""
from pathlib import Path
import ctypes as C,hashlib,json,runpy,struct,subprocess
r=Path(__file__).resolve().parents[1]
# Reuse the bounded BIOS/memory oracle, also rerunning the shared font/map proof.
o=runpy.run_path(str(r/'tests/test_map_view_rom.py'));uc=o['uc'];call=o['call'];word=o['word'];rom=o['rom'];raw=o['raw'];View=o['View'];events=o['events']
b=r/'build/native-tests'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/menu.c'),
 str(r/'src/scores.c'),str(r/'src/map_view.c'),str(r/'src/map_assets.c'),'-o',str(b/'libssa_menu.so')],check=True)
class ScoreEntry(C.Structure):
 _fields_=[('score',C.c_uint32),('name',C.c_char*12)]
class Scores(C.Structure):
 _fields_=[('entries',ScoreEntry*18),('name',C.c_char*13)]
class Menu(C.Structure):
 _fields_=[('canvas',View),('tiles',C.c_uint8*32768),('logo',C.c_uint8*12288),
 ('page',C.c_uint),('selection',C.c_uint),('marker',C.c_uint),('offset',C.c_uint),('language',C.c_uint),('difficulty',C.c_uint),('music_enabled',C.c_uint),
 ('counter',C.c_uint16),('scroll',C.c_uint16),('logo_y',C.c_int),('fade_volume',C.c_int),('fade_step',C.c_int),
 ('fading',C.c_uint),('leaving',C.c_uint),('done',C.c_uint),('unsupported',C.c_uint),('commands',C.c_uint*4),('command_count',C.c_uint),('fade_audio',C.c_uint),
 ('draw_page',C.c_uint),('draw_counter',C.c_uint16),('credits_offset',C.c_uint32),('credits_end',C.c_uint32),
 ('credits_y',C.c_int32),('credits_speed',C.c_int32),('credits_done',C.c_uint16),('credits_lines',C.c_uint16),
 ('level',C.c_uint),('loadout',C.c_uint),('level_limit',C.c_uint),('password',C.c_char*9),
 ('password_cursor',C.c_uint8),('password_up',C.c_uint8),('password_down',C.c_uint8),('password_typed',C.c_uint8),('grid',C.c_uint8*64),
 ('completed_levels',C.c_uint16),('level_scroll',C.c_uint),('selecting_level',C.c_uint),('clear_requested',C.c_uint),
 ('settings',C.c_uint*12),('soundfont_available',C.c_uint),('midi_available',C.c_uint),('settings_unavailable',C.c_uint),('quit_requested',C.c_uint),('score_y',C.c_int32),('score_speed',C.c_int32),('score_ticks',C.c_uint),('score_leaving',C.c_uint),
 ('scores',Scores),('score_candidate',C.c_uint32),('score_rank',C.c_uint),('score_highlight',C.c_uint),('score_submitted',C.c_uint),
 ('score_cursor',C.c_uint8),('score_up',C.c_uint8),('score_down',C.c_uint8),('score_typed',C.c_uint8),('score_length',C.c_uint)]
lib=C.CDLL(str(b/'libssa_menu.so'));lib.ssa_menu_init.argtypes=[C.POINTER(Menu),C.c_void_p,C.c_size_t,C.c_uint]
lib.ssa_menu_step.argtypes=[C.POINTER(Menu),C.c_uint];lib.ssa_menu_draw.argtypes=[C.POINTER(Menu),C.POINTER(C.c_uint16),C.c_size_t]
lib.ssa_menu_step_input.argtypes=[C.POINTER(Menu),C.c_uint,C.c_uint]
lib.ssa_menu_score_begin.argtypes=[C.POINTER(Menu),C.c_uint32]
out=(C.c_uint16*38400)();cases=pixels=credits_frames=credits_finishes=password_frames=password_starts=score_frames=0
def setup():
 o['setup'](0,0,1)
 uc.mem_write(0x02000004,struct.pack('<H',10));uc.mem_write(0x02000008,b'\xff\xff')
 uc.mem_write(0x0200001a,b'\0')
 uc.mem_write(0x03004010,struct.pack('<I',0x02000400))
 call(0x080164e8)
 call(0x08016650,0x02000012,0x02000014)
 call(0x08015614,2,64,31)
 m=Menu();assert lib.ssa_menu_init(C.byref(m),raw,len(rom),1)==1
 # Recovery fixture explicitly enters original Language; PC cold start is Title.
 m.page=m.draw_page=10
 assert bytes(m.grid)==bytes(uc.mem_read(0x06008000,64))
 return m
def compare(m,input,visual=False,held=None):
 global cases,pixels,credits_frames,password_frames,score_frames
 if held is None:held=input
 # Reproduce the outer1862C loop up to and through its menu callback.
 call(0x08015698)
 state=bytes(uc.mem_read(0x02000000,0x40));page=struct.unpack_from('<H',state,4)[0];counter=struct.unpack_from('<H',state,12)[0]
 scroll=struct.unpack_from('<H',state,14)[0]
 if counter&7==0:scroll=(scroll+64)&511;uc.mem_write(0x0200000e,struct.pack('<H',scroll))
 if page!=10:call(0x080166f8,struct.unpack_from('<H',state,18)[0],struct.unpack_from('<H',state,20)[0],-1 if page in (12,13) else 1)
 packed=0
 for mask,key in ((1,0x40),(2,0x80),(4,1),(8,2),(16,8),(32,0x20),(64,0x10)):
  if input&mask:packed|=key<<16
  if held&mask:packed|=key
 uc.mem_write(0x030042f0,struct.pack('<I',packed));uc.mem_write(0x03005730,b'\0');events.clear()
 result=call(0x08018574)
 if page==4:
  # Requested PC-only third option; use the actual original text renderer for
  # its glyphs/pixels too. The first two options keep their original layout.
  language=struct.unpack('<H',uc.mem_read(0x02000000,2))[0]
  table=struct.unpack_from('<I',rom,0x44cf34+language*4)[0]-0x08000000
  address=struct.unpack_from('<I',rom,table+(237+language)*4)[0]-0x08000000
  label=(b'Language',b'Sprache',b'Langue')[language]+b': '+rom[address:rom.index(0,address)]
  uc.mem_write(0x0203e000,label+b'\0');call(0x08016748,0x0203e000,2,122,0,0)
  label=(b'Clear Data',b'Daten loeschen',b'Effacer donnees')[language]
  uc.mem_write(0x0203e000,label+b'\0');call(0x08016748,0x0203e000,3,138,0,0)
 if page==1:
  # Original navigation has two rows. Skip the new PC-only Level Select row
  # in this recovery fixture, and independently draw its added label through
  # the actual original font routine. Native-only unlock navigation has its
  # own tests; this does not claim an original level-select callback match.
  language=struct.unpack('<H',uc.mem_read(0x02000000,2))[0]
  table=struct.unpack_from('<I',rom,0x44cf34+language*4)[0]-0x08000000
  uc.mem_write(0x03005730,b'\0')
  label=(b'Select Level',b'Levelauswahl',b'Choix du niveau')[language]
  uc.mem_write(0x0203e000,label+b'\0')
  for address,selection,y in ((struct.unpack_from('<I',rom,table+9*4)[0],0,82),(0x0203e000,2,98),(struct.unpack_from('<I',rom,table+4*4)[0],1,114)):
   call(0x08016748,address,selection,y,0,0)
 fade=struct.unpack('<5H',uc.mem_read(0x03005720,10));state=bytes(uc.mem_read(0x02000000,0x40));done=result==0 and not fade[4]
 if page==1 and ((input&2 and m.selection==0) or (input&1 and m.selection==2)):m.selection=1
 assert lib.ssa_menu_step_input(C.byref(m),input,held)==1
 if page==12:
  original_scroll,original_pointer,original_done,original_lines,original_speed=struct.unpack('<IIHHi',uc.mem_read(0x03006dd4,16))
  assert (m.credits_y&0xffffffff,m.credits_offset+0x08000000,m.credits_done,m.credits_lines,m.credits_speed)==(
   original_scroll,original_pointer,original_done,original_lines,original_speed),('credits state',input,held)
  credits_frames+=1
 if page==13:
  data=bytes(uc.mem_read(word(0x03006dd0),52))
  assert (m.score_y,m.score_speed,m.score_ticks,m.score_leaving)==(struct.unpack_from('<i',data,4)[0],struct.unpack_from('<i',data,24)[0],struct.unpack_from('<I',data,44)[0],struct.unpack_from('<H',data,20)[0]),('scores state',input,data.hex(),m.score_y,m.score_speed,m.score_ticks,m.score_leaving)
  score_frames+=1
 if page==14:
  assert bytes(m.scores.entries)==bytes(uc.mem_read(0x02000040,288))
  assert bytes(m.scores.name)==bytes(uc.mem_read(0x0200002e,12))
  assert (m.score_cursor,m.score_up,m.score_down)==(state[37],state[42],state[43])
  assert m.score_highlight==state[59]
 if page==8 or m.page==8:
  assert C.string_at(C.addressof(m)+Menu.password.offset,9)==state[28:37]
  assert (m.password_cursor,m.password_up,m.password_down)==(state[37],state[42],state[43])
  assert (m.level,m.loadout)==(struct.unpack_from('<H',state,16)[0],state[39])
  if m.leaving:
   assert uc.mem_read(0x0200047c,1)[0]==(5,3,2)[m.difficulty]
  password_frames+=1
 assert (14 if m.page==19 else m.page,1 if m.page==1 and m.selection==2 else m.selection,m.marker,m.offset,m.language,m.difficulty,m.music_enabled,m.scroll,m.fade_volume,m.fading,m.leaving,m.done)==(
  *struct.unpack_from('<4H',state,4),struct.unpack_from('<H',state,0)[0],state[26],state[44],scroll,fade[2],fade[4],state[25],int(done)),('state',page,input,(m.page,m.selection,m.marker,m.offset,m.language,m.difficulty,m.music_enabled,m.scroll,m.fade_volume,m.fading,m.leaving,m.done),state.hex())
 assert [(pc,arg) for pc,arg in events if pc==0x08008eb0]==[(0x08008eb0,c) for c in m.commands[:m.command_count]]
 assert [(pc,arg) for pc,arg in events if pc==0x08008e74]==([(0x08008e74,5)] if m.fade_audio else [])
 assert lib.ssa_menu_draw(C.byref(m),out,38400)==1
 count=uc.mem_read(0x03005730,1)[0];records=bytes(uc.mem_read(word(0x03005740),128*20))
 original=[(*struct.unpack_from('<HH',records,(n+3)*20),struct.unpack_from('<H',records,(n+3)*20+14)[0],(struct.unpack_from('<H',records,(n+3)*20+16)[0]>>4)&15) for n in range(count)]
 native=[(g.x&65535,g.y&65535,504+g.glyph if g.font else g.glyph*4,g.palette) for g in m.canvas.glyphs[:m.canvas.glyph_count]]
 assert native==original,(page,input,'glyphs',native,original)
 if visual:
  call(0x08015b54);call(0x08015c2c)
  vram=bytes(uc.mem_read(0x06000000,0x18000));oam=bytes(uc.mem_read(0x07000000,1024));pal=struct.unpack('<256H',uc.mem_read(0x05000000,512));image=[]
  for y in range(160):
   for x in range(240):
    sx=(x+scroll)&511;tx=sx//8;ty=y//8
    entry=struct.unpack_from('<H',vram,0xe000+((ty//32)*2+tx//32)*2048+((ty&31)*32+(tx&31))*2)[0]
    image.append(pal[vram[(entry&1023)*64+(y&7)*8+(sx&7)]])
  # Restore the actual BG1 layer from its original VRAM and tilemap. Startup
  # intentionally enables it after Language too; original cold boot left it off.
  if page!=10:
   bg1=struct.unpack('<H',uc.mem_read(0x0400000a,2))[0]
   for y in range(160):
    for x in range(240):
     entry=struct.unpack_from('<H',vram,((bg1>>8)&31)*2048+((y//8)*32+x//8)*2)[0]
     c=vram[((bg1>>2)&3)*16384+(entry&1023)*64+(y&7)*8+(x&7)]
     if c:image[y*240+x]=pal[c]
  for n in range(127,-1,-1):
   a0,a1,a2=struct.unpack_from('<3H',oam,n*8)
   if a0&0x300==0x200:continue
   assert not a0&0x100 and a0>>14==0
   size=8<<(a1>>14);sx=a1&511;sy=a0&255
   if sx>=240:sx-=512
   if sy>=160:sy-=256
   for dy in range(size):
    for dx in range(size):
     x=sx+dx;y=sy+dy
     if not (0<=x<240 and 0<=y<160):continue
     unit=64 if a0&0x2000 else 32
     at=0x10000+(a2&1023)*32+((dy//8)*(size//8)+dx//8)*unit+(dy&7)*(8 if unit==64 else 4)+(dx&7)//(1 if unit==64 else 2)
     c=vram[at] if unit==64 else (vram[at]>>((dx&1)*4))&15
     if c:image[y*240+x]=pal[c if unit==64 else ((a2>>12)&15)*16+c]
  darken=(fade[2]>>8)&15
  image=[sum((((c>>shift)&31)-(((c>>shift)&31)*darken//16))<<shift for shift in (0,5,10)) for c in image]
  assert list(out)==image,(page,input,'pixels',next((i for i,(a,b) in enumerate(zip(out,image)) if a!=b),None))
  pixels+=38400
 if not done:
  c=struct.unpack_from('<H',state,12)[0];uc.mem_write(0x0200000c,struct.pack('<H',(c+1)&65535))
 assert m.counter==struct.unpack('<H',uc.mem_read(0x0200000c,2))[0]
 cases+=1
def fixture(page,language=0,selection=0):
 m=setup();m.page=m.draw_page=page;m.language=language;m.selection=2 if page==1 and selection==1 else selection;m.counter=200
 m.fade_volume=0;m.fading=0
 uc.mem_write(0x02000000,struct.pack('<H',language));uc.mem_write(0x02000004,struct.pack('<HH',page,selection))
 uc.mem_write(0x0200000c,struct.pack('<H',200));uc.mem_write(0x03005724,bytes(2));uc.mem_write(0x03005728,bytes(2))
 return m
if __name__=='__main__':
 for language in range(3):
  m=setup()
  for tick in range(3):compare(m,2 if tick<language else 0)
  compare(m,4)
  for tick in range(100):compare(m,16 if tick in (20,95) else 0,tick in (0,32,64,94))
  for tick in range(20):compare(m,0,tick in (0,15))
  compare(m,4);compare(m,0,True);compare(m,4);compare(m,2);compare(m,0,True);compare(m,4)
  while not m.done:compare(m,0,m.fade_volume in (0,1024,2048))
 for language in range(3):
  m=fixture(4,language)
  for key in (1,32,64,4,2,1,1,8,0):compare(m,key,True)
  m=fixture(1,language)
  for key in (1,2,2,1,8,0):compare(m,key,True)
  for difficulty in range(3):
   m=fixture(9,language,difficulty);compare(m,4,True)
   while not m.done:compare(m,4,m.fade_volume in (0,2048))
   assert m.difficulty==difficulty
  # Fully settled native preview, generated directly from the ROM-backed renderer.
  m=fixture(0,language);m.logo_y=8;m.offset=0;m.marker=0
  assert lib.ssa_menu_draw(C.byref(m),out,38400)==1
  rgb=bytes(c for pixel in out for c in ((pixel&31)*255//31,((pixel>>5)&31)*255//31,((pixel>>10)&31)*255//31))
  (b/f'menu-{language}.ppm').write_bytes(b'P6\n240 160\n255\n'+rgb)
  # Enter via Options, pause (including both directions held), accelerate and
  # release. Confirm/B exit suppresses text now and returns on the NEXT tick.
  for exit_key in (4,8):
   m=fixture(4,language,1);compare(m,4,True)
   for tick in range(40):compare(m,0,tick in (0,20,39),held=0)
   for held in (1,3,1,2,2,0,0):compare(m,0,True,held=held)
   compare(m,exit_key,True,held=0);assert m.page==12 and m.credits_done==1
   compare(m,0,True);assert m.page==4 and m.marker==65535
   compare(m,0,True)
  # Full text, original blank lines, headings and automatic return. Re-entry
  # resets the scroll; fast-forward is the original held Down behavior.
  m=fixture(12,language)
  for tick in range(2000):
   compare(m,0,tick%150==0,held=2)
   if m.page==4:break
  else:raise AssertionError('Credits failed to end')
  assert m.credits_done==1 and m.credits_lines==88
  credits_finishes+=1;compare(m,0,True)
  # Original eight-character editor, cursor wrap and hold repeat (including
  # byte overflow from255 back to32); Down has priority when both repeat.
  m=fixture(1,language,1);compare(m,4,True);assert m.page==8
  for key in (32,64,64,1,2,3,96,4,0,8,0):compare(m,key,True)
  m=fixture(1,language,1);compare(m,4)
  for held in (1,2,3):
   for tick in range(280):compare(m,held if tick==0 else 0,tick in (30,31,254,255,279),held=held)
  compare(m,0,True,held=0);compare(m,8,True)
 for index in range(36):
  m=fixture(1,index%3,1);compare(m,4)
  address=struct.unpack_from('<I',rom,0x44c1d4+index*4)[0]-0x08000000
  password=rom[address:rom.index(0,address)]
  m.password=password;uc.mem_write(0x0200001c,password+b'\0'*(9-len(password)))
  compare(m,4,True);assert (m.level,m.difficulty,m.loadout)==(index%12,index//12,index+1)
  while not m.done:compare(m,4,m.fade_volume in (0,2048))
  password_starts+=1
 report={'result':'pass','rom_frames':cases,'composed_rom_pixels':pixels,'test_only_arm_execution':True,
  'score_frames':score_frames,
  'credits_frames':credits_frames,'credits_complete_scrolls':credits_finishes,
  'password_frames':password_frames,'password_starts':password_starts,
  'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (r/'src/menu.c',r/'include/ssa/menu.h',r/'src/map_view.c',r/'include/ssa/map_view.h',Path(__file__))}}
 (r/'analysis/menu-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
