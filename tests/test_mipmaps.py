from native_build import ROOT, build_check
import hashlib, json, os, subprocess
out = ROOT / 'build/native-tests/test_mipmaps'
out.parent.mkdir(parents=True, exist_ok=True)
build_check(ROOT / 'tests/test_mipmaps.c', out)
p = subprocess.run([str(out), str(ROOT / 'orig/SSA.gba')], capture_output=True, text=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1'), timeout=180)
print(p.stdout, end=''); print(p.stderr, end=''); p.check_returncode()
report = json.loads(p.stdout)
report['source_sha256'] = {f: hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in (
    'src/pc_scene.c','src/pc_renderer_pitch.inc','src/settings.c','src/menu.c',
    'include/ssa/settings.h','include/ssa/pc_hardware.h','src/native_settings.c',
    'platform/win32/vulkan_renderer.c','platform/vulkan/scene.frag','tests/test_mipmaps.c')}
(ROOT/'analysis/mipmaps-validation.json').write_text(json.dumps(report,indent=2)+'\n')
