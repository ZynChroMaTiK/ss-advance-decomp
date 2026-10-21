from pathlib import Path
from native_build import build_check
import os, subprocess, json, hashlib
root=Path(__file__).resolve().parents[1]
out=root/'build/sprite-shadows';out.mkdir(parents=True,exist_ok=True)
binary=out/'test_shadows'
build_check(root/'tests/test_sprite_shadows.c',binary)
p=subprocess.run([str(binary),str(root/'orig/SSA.gba'),str(out/'native.ppm')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=120)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['sanitizers']='ASan/UBSan/leaks=1'
paths=['src/sprite_shadows.c','src/sprite_shadow_texture.inc','include/ssa/sprite_shadows.h','src/native_game.c','src/pc_scene.c','src/pc_renderer_pitch.inc','src/menu.c','src/settings.c','include/ssa/settings.h','tests/test_sprite_shadows.c','tests/sprite_shadow_fixture.h','src/actor_queue.c','src/actor_scene.c','include/ssa/actor_queue.h','include/ssa/actor_scene.h']
report['source_sha256']={s:hashlib.sha256((root/s).read_bytes()).hexdigest() for s in paths}
(root/'analysis/sprite-shadows-validation.json').write_text(json.dumps(report,indent=2)+'\n')
