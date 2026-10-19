"""Native automap, live actors/movers, dedicated contacts, controls and draw purity."""
from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'));test=r/'tests/test_automap.c';binary=b/'test_automap'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),str(test),*map(str,sources),'-lm','-o',str(binary)],check=True)
p=subprocess.run([str(binary),str(r/'orig/SSA.gba')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=180)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report.update(scope=__doc__,sanitizers='ASan/UBSan/leaks=1')
inputs=sources+list((r/'include/ssa').glob('*.h'))+list((r/'src').glob('*.inc'))+[test,Path(__file__)]
report['source_sha256']={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
(r/'analysis/automap-validation.json').write_text(json.dumps(report,indent=2)+'\n')
