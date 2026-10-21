from native_build import build_check, ROOT
import os, subprocess, json, hashlib

output = ROOT / 'build/native-tests'
output.mkdir(parents=True, exist_ok=True)
build_check(ROOT / 'tests/test_godrays.c', output / 'test_godrays')
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1')
p = subprocess.run([str(output / 'test_godrays'), str(ROOT / 'orig/SSA.gba'),
                    str(output / 'godrays.bmp'), str(output / 'godrays-inside.bmp'), str(output / 'godrays-gba.bmp')], env=env, capture_output=True, text=True, timeout=180)
print(p.stdout, end=''); print(p.stderr, end=''); p.check_returncode()
report = json.loads(p.stdout)
report['source_sha256'] = {f: hashlib.sha256((ROOT / f).read_bytes()).hexdigest() for f in (
    'src/pc_scene.c', 'src/pc_godrays.inc', 'src/pc_renderer_pitch.inc',
    'src/godrays.c', 'src/godrays_reference.c', 'include/ssa/godrays.h', 'include/ssa/godrays_reference.h',
    'src/actor_raster.c','src/actor_projection.c','src/actor_queue.c','include/ssa/actor_queue.h','src/native_game.c',
    'include/ssa/pc_renderer.h', 'tests/test_godrays.c', 'tests/test_godrays.py', 'tests/godray_fixture.h')}
(ROOT / 'analysis/godrays-validation.json').write_text(json.dumps(report, indent=2)+'\n')
