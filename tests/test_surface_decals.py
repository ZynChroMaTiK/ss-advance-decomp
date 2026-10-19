"""Optional floor/ceiling art, world anchoring, depth and original-off behavior."""
from native_build import build_check
from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/surface-decals';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'));test=r/'tests/test_surface_decals.c'
build_check(test,b/'test_decals')
p=subprocess.run([str(b/'test_decals'),str(r/'orig/SSA.gba'),str(b)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},capture_output=True,text=True,timeout=180)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report['sanitizers']='ASan/UBSan/leaks=1';report['scope']=__doc__
inputs=sources+list((r/'include/ssa').glob('*.h'))+list((r/'src').glob('*.inc'))+[test,Path(__file__),r/'tests/native_build.py']
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs}
(r/'analysis/surface-decals-validation.json').write_text(json.dumps(report,indent=2)+'\n')
