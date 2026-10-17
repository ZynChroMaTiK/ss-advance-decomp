from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=[r/'src/menu.c',r/'src/map_view.c',r/'src/map_assets.c',r/'tests/test_menu.c']
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 *map(str,sources),'-o',str(b/'test_menu')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_menu'),str(r/'orig/SSA.gba')],capture_output=True,text=True,env=env,timeout=120)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['sanitizers']='ASan/UBSan/leaks=1'
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in
 sources+[r/'include/ssa/menu.h',r/'include/ssa/map_view.h',Path(__file__)]}
(r/'analysis/menu-validation.json').write_text(json.dumps(report,indent=2)+'\n')
