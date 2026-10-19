from native_build import build_check
from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/classic-hud-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'));test=r/'tests/test_classic_hud.c'
build_check(test,b/'test_classic_hud')
p=subprocess.run([str(b/'test_classic_hud'),str(r/'orig/SSA.gba'),str(b)],capture_output=True,text=True,
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=180)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report['sanitizers']='ASan/UBSan/leaks=1'
inputs=sources+list((r/'include/ssa').glob('*.h'))+list((r/'src').glob('*.inc'))+[test,Path(__file__),r/'tests/native_build.py']
inputs += [r/'assets/classic-hud/time-strike-reference.png', r/'scripts/import-classic-time-strike.ps1']
inputs += [r/'assets/classic-hud/tommygun-ammo.png', r/'scripts/import-classic-tommy-ammo.ps1']
report['source_sha256']={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(r/'analysis/classic-hud-validation.json').write_text(json.dumps(report,indent=2)+'\n')
