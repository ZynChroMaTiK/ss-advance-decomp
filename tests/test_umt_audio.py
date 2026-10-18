"""U.M.T. pickup1D must finish without preempting independent PC sounds."""
from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
names=['tests/test_umt_audio.c','src/audio.c','src/pickup.c','src/inventory.c']
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined',
 '-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),*[str(r/p) for p in names],'-o',str(b/'test_umt_audio')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_umt_audio'),str(r/'orig/SSA.gba')],capture_output=True,text=True,env=env)
print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report['scope']=__doc__;report['sanitizers']='ASan/UBSan/leaks=1'
names+=['include/ssa/audio.h','include/ssa/pickup.h','include/ssa/inventory.h','tests/test_umt_audio.py']
report['source_sha256']={name:hashlib.sha256((r/name).read_bytes()).hexdigest() for name in names}
(r/'analysis/umt-audio-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
