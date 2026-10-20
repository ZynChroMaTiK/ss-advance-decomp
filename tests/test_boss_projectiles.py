"""Boss projectile ownership/contact/splash regression on isolated native map geometry."""
from pathlib import Path
import hashlib,json,os,subprocess
from native_build import build_check
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
binary=b/'test_boss_projectiles'
build_check(r/'tests/test_boss_projectiles.c',binary)
p=subprocess.run([str(binary),str(r/'orig/SSA.gba')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=120)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report.update(sanitizers='ASan/UBSan/leaks=1',scope=__doc__)
paths=['tests/test_boss_projectiles.c','src/combat_runtime.c','src/combat_runtime_internal.h','src/projectile_runtime.c','src/native_game.c','include/ssa/combat_runtime.h']
report['source_sha256']={s:hashlib.sha256((r/s).read_bytes()).hexdigest() for s in paths}
(r/'analysis/boss-projectile-validation.json').write_text(json.dumps(report,indent=2)+'\n')
