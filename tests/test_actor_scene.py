"""Sanitized native actor-scene integration; ROM is data, never executed."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/native-tests'
BUILD.mkdir(parents=True, exist_ok=True)
sources = sorted((ROOT / 'src').glob('*.c'))
test = ROOT / 'tests/test_actor_scene.c'
binary = BUILD / 'test_actor_scene'
subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
                '-I', str(ROOT / 'include'), str(test), *map(str, sources), '-o', str(binary)], check=True)
rom = ROOT / 'orig/SSA.gba'
sha = hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha == '4ff48d8c18b70a5184758f1917343ae85a70858e'
env = os.environ.copy()
env['ASAN_OPTIONS'] = 'detect_leaks=0'
result = subprocess.run([str(binary), str(rom)], text=True, capture_output=True,
                        env=env, timeout=180)
if result.returncode:
    print(result.stdout, end='')
    print(result.stderr, end='')
    result.check_returncode()
report = json.loads(result.stdout)
report['rom_sha1'] = sha
report['sanitizers'] = 'ASan+UBSan, -fno-sanitize-recover=all, -no-pie; detect_leaks=0'
report['scope'] = ('Actual SsaMapActors construction/animation, native player movement and world collision, '
                   'portable WASD/Space/H/relative mouse events, native camera/geometry/actor projection, '
                   'mixed depth queue and original decoded sprite pixels on all 16 maps. First-person '
                   'local body hidden; third-person snapshot alone clears flag80. No AI/combat, game-frame '
                   'or OS event delivery claim. Missing sources preserve partial queue and leave pixels '
                   'untouched; camera borrow and guards clear; borrowed owners survive scene teardown.')
report['source_sha256'] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sources}
report['header_sha256'] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted((ROOT / 'include/ssa').glob('*.h'))}
report['test_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (test, Path(__file__).resolve())}
(ROOT / 'analysis/native-actor-scene-integration-validation.json').write_text(
    json.dumps(report, indent=2) + '\n')
print(json.dumps({k: v for k, v in report.items() if not k.endswith('sha256')}, indent=2))
