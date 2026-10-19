from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=[r/f for f in ('src/profile.c','src/menu.c','src/scores.c','src/map_view.c','src/map_assets.c','tests/test_profile.c')]
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 *map(str,sources),'-o',str(b/'test_profile')],check=True)
p=subprocess.run([str(b/'test_profile'),str(r/'orig/SSA.gba')],cwd=r,capture_output=True,text=True,
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=120)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report.update(sanitizers='ASan/UBSan/leaks=1',scope='Profile codec, immediate-predecessor unlocks, original localized campaign names, keyboard/mouse level selection and Clear Data confirmation; disk persistence is checked separately on Windows.')
inputs=sources+[r/f for f in ('include/ssa/profile.h','include/ssa/menu.h','include/ssa/map_view.h','include/ssa/native_game.h')]+[Path(__file__)]
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs}
(r/'analysis/profile-validation.json').write_text(json.dumps(report,indent=2)+'\n')
