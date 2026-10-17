"""Native PC/Game movement -> scene geometry pixels on all16 real maps."""
from pathlib import Path
import hashlib,json,os,subprocess,struct,zlib
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_world_scene.c'),*map(str,sources),'-o',str(B/'test_world_scene')],check=True)
rom=ROOT/'orig/SSA.gba';sha=hashlib.sha1(rom.read_bytes()).hexdigest();assert sha=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(B/'test_world_scene'),str(rom),str(B)],text=True,capture_output=True,env=env,timeout=180)
if result.returncode:
    print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout);report['rom_sha1']=sha
report['scope']='ActualnativePC/Game fixtureson16maps withjump/H/mouseyaw, cameraBSPsectorlookup, nativeworldvisibility/walls/portals/planes/skypixels. Existinggame presentation/script boundaries controlled. No actors/Hbody/HUD/audio/windoworplayablegameclaim.'
report['source_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
report['test_sha256']=hashlib.sha256((ROOT/'tests/test_world_scene.c').read_bytes()).hexdigest()
def png_from_ppm(path):
    signature,dimensions,maximum,rgb=path.read_bytes().split(b'\n',3)
    assert (signature,dimensions,maximum)==(b'P6',b'120 120',b'255') and len(rgb)==120*120*3
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    raw=b''.join(b'\0'+rgb[row*360:(row+1)*360] for row in range(120))
    data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',120,120,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
    path.with_suffix('.png').write_bytes(data)
for level in range(16):png_from_ppm(B/f'world-level-{level:02}.ppm')
(ROOT/'analysis/native-world-scene-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='source_sha256'},indent=2))
