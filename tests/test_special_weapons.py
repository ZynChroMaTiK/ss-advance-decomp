from pathlib import Path
import hashlib,json,os,re,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'))
from native_build import build_check
build_check(r/'tests/test_special_weapons.c',b/'test_special_weapons')

env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
assert hashlib.sha1((r/'orig/SSA.gba').read_bytes()).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
p=subprocess.run([str(b/'test_special_weapons'),str(r/'orig/SSA.gba')],capture_output=True,text=True,env=env,timeout=180)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['sanitizers']='ASan/UBSan/leaks=1'
fixtures=set()
def include_fixture(path):
 if path in fixtures:return
 fixtures.add(path)
 for name in re.findall(r'^\s*#include "([^\"]+\.c)"',path.read_text(),re.M):include_fixture(path.parent/name)
include_fixture(r/'tests/test_special_weapons.c')
inputs=sources+list((r/'include/ssa').glob('*.h'))+list((r/'src').glob('*.h'))+sorted(fixtures)+list((r/'src').glob('*.inc'))+[r/'tests/native_build.py',Path(__file__)]
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs}
(r/'analysis/special-weapons-validation.json').write_text(json.dumps(report,indent=2)+'\n')
