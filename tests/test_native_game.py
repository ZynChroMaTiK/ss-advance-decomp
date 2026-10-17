"""Actual native gameplay and pixel composition; no emulated runtime."""
from pathlib import Path
import hashlib, json, os, subprocess
root=Path(__file__).resolve().parents[1]
build=root/'build/native-tests'
build.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(root/'include'),
 str(root/'tests/test_native_game.c'),*map(str,sorted((root/'src').glob('*.c'))),'-o',str(build/'test_native_game')],check=True)
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=1')
result=subprocess.run([str(build/'test_native_game'),str(root/'orig/SSA.gba')],capture_output=True,text=True,env=env,timeout=180)
print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
report.update(sanitizer_options=env['ASAN_OPTIONS'],source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'tests/test_native_game.c',*sorted((root/'src').glob('*.c'))]})
(root/'analysis/native-game-integration-validation.json').write_text(json.dumps(report,indent=2)+'\n')
