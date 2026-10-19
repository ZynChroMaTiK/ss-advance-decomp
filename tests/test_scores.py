from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=[r/s for s in ('src/scores.c','src/menu.c','src/map_view.c','src/map_assets.c','tests/test_scores.c')]
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),*map(str,sources),'-o',str(b/'test_scores')],check=True)
p=subprocess.run([str(b/'test_scores'),str(r/'orig/SSA.gba')],capture_output=True,text=True,
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=120)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['sanitizers']='ASan/UBSan/leaks=1'
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in sources+[r/'include/ssa/scores.h',r/'include/ssa/menu.h',Path(__file__)]}
(r/'analysis/scores-validation.json').write_text(json.dumps(report,indent=2)+'\n')
