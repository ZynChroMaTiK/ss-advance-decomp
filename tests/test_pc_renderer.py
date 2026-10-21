from native_build import build_check
from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'))
build_check(r/'tests/test_pc_renderer.c',b/'test_pc_renderer')
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_pc_renderer'),str(r/'orig/SSA.gba'),str(b/'pc-renderer.bmp'),str(b/'pc-chase.bmp'),str(b/'pc-chase-original.bmp')],capture_output=True,text=True,env=env,timeout=180)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in [r/'tests/test_pc_renderer.c',r/'src/pc_renderer.c',r/'src/pc_scene.c',r/'include/ssa/pc_scene.h',r/'tests/native_build.py',r/'src/pc_renderer_pitch.inc',r/'src/pc_actor_clip.inc',r/'tests/sprite_clip_fixture.h',r/'include/ssa/pc_renderer.h']}
(r/'analysis/pc-renderer-validation.json').write_text(json.dumps(report,indent=2)+'\n')
