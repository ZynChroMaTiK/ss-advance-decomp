"""Validate original map allocation order survives transfer into the live pool."""
from pathlib import Path
import json, os, subprocess
root = Path(__file__).resolve().parents[1]
build = root / 'build/native-tests'
build.mkdir(parents=True, exist_ok=True)
sources = sorted((root / 'src').glob('*.c'))
subprocess.run(['gcc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
 '-pedantic', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
 '-I', str(root/'include'), str(root/'tests/test_map_gameplay_ownership.c'),
 *map(str, sources), '-o', str(build/'test_map_gameplay_ownership')], check=True)
env = os.environ.copy()
env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
result = subprocess.run([str(build/'test_map_gameplay_ownership'), str(root/'orig/SSA.gba')],
 capture_output=True, text=True, env=env, timeout=180)
print(result.stdout, end='')
print(result.stderr, end='')
result.check_returncode()
report = json.loads(result.stdout)
(root/'analysis/map-gameplay-ownership-validation.json').write_text(json.dumps(report, indent=2)+'\n')
