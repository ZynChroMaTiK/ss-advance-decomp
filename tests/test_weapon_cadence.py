from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 str(r/'tests/test_weapon_cadence.c'),*map(str,sources),'-o',str(b/'test_weapon_cadence')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_weapon_cadence'),str(r/'orig/SSA.gba')],capture_output=True,text=True,env=env,timeout=180)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['sanitizers']='ASan/UBSan/leaks=1'
inputs=sources+list((r/'include/ssa').glob('*.h'))+[r/'src/native_audio.inc',r/'tests/test_weapon_cadence.c',Path(__file__)]
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs}
(r/'analysis/weapon-cadence-validation.json').write_text(json.dumps(report,indent=2)+'\n')
