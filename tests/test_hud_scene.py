"""Real-map actor/HUD presentation integration plus independent RGB reference."""
from pathlib import Path
import ast,ctypes as C,hashlib,json,math,os,struct,subprocess
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'));test=ROOT/'tests/test_hud_scene.c'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(test),*map(str,sources),'-o',str(B/'test_hud_scene')],check=True)
result=subprocess.run([str(B/'test_hud_scene'),str(ROOT/'orig/SSA.gba')],cwd=ROOT,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True)
if result.returncode:print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
nodes=[n for n in ast.parse((ROOT/'tests/test_display.py').read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='reference']
exec(compile(ast.Module(nodes,type_ignores=[]),'independent sprite painter','exec'))
class Entry(C.Structure):_fields_=[('a0',C.c_uint16),('a1',C.c_uint16),('a2',C.c_uint16),('affine',C.c_uint16)]
class Palettes:pass
assets=Palettes();shapes=[[(8,8),(16,16),(32,32),(64,64)],[(16,8),(32,8),(32,16),(64,32)],[(8,16),(8,32),(16,32),(32,64)]]
report['independent_rgb_frames']=0;report['display_sha256']=[]
for level in range(16):
 for frame in (0,140):
  data=(B/f'hud-scene-{level:02d}-{frame:03d}.bin').read_bytes();assert len(data)==126016
  world=data[:14400];entries=(Entry*128).from_buffer_copy(data[14400:15424]);tiles=data[15424:48192]
  bg=struct.unpack_from('<256H',data,48192);assets.palette=struct.unpack_from('<256H',data,48704)
  base=[bg[world[(y*3//4)*120+x//2]]&32767 for y in range(160) for x in range(240)]
  actual=list(struct.unpack_from('<38400H',data,49216));expected=reference(entries,tiles)
  assert actual==expected,(level,frame,next((i for i in range(38400) if actual[i]!=expected[i]),None))
  report['independent_rgb_frames']+=1;report['display_sha256'].append(hashlib.sha256(data[49216:]).hexdigest())
  if level==0:
   import zlib
   rgb=bytes(channel for color in actual for k in range(3) for channel in [((((color>>(5*k))&31)<<3)|(((color>>(5*k))&31)>>2))])
   def chunk(kind,payload):return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xffffffff)
   rows=b''.join(b'\0'+rgb[y*720:(y+1)*720] for y in range(160))
   (B/f'hud-scene-00-{frame:03d}.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',240,160,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
report['rom_sha1']=hashlib.sha1((ROOT/'orig/SSA.gba').read_bytes()).hexdigest()
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
report['fixture_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [test,Path(__file__),ROOT/'tests/test_actor_scene.c',ROOT/'tests/test_display.py']}
report['scope']='Real16-map world/actor rendering with native player movement/jump/H, original1240C loadout and101F4 HUD allocation, scalar HUD handlers, animation, cache transfers, OAM and display. Idle-only12160 weapon raise; no combat/firing. Canonical health changes are test stimuli. Independent hardware sprite pixel reference covers32 captured frames. No complete game or OS event delivery claim.'
(ROOT/'analysis/native-hud-scene-integration-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if not k.endswith('sha256')},indent=2))
