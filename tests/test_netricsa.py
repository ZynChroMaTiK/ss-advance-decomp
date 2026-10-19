from pathlib import Path
import os,subprocess,json,hashlib,sys
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
names=['src/netricsa.c','src/map_assets.c','tests/test_netricsa.c']
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined',
 '-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),*[str(r/n) for n in names],'-o',str(b/'test_netricsa')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_netricsa'),str(r/'orig/SSA.gba'),str(b/'netricsa.bmp')],env=env,capture_output=True,text=True)
print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report['sanitizers']='ASan/UBSan/leaks=1';names+=['include/ssa/netricsa.h','tests/test_netricsa.py','tests/test_netricsa_rom.py']
oracle=subprocess.run([sys.executable,str(r/'tests/test_netricsa_rom.py')],capture_output=True,text=True,check=True)
report['original_rom']=json.loads(oracle.stdout)
report['source_sha256']={n:hashlib.sha256((r/n).read_bytes()).hexdigest() for n in names}
(r/'analysis/netricsa-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
