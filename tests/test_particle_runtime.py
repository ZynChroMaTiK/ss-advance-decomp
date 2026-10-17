from pathlib import Path
import hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_particle_runtime.c'),*map(str,sources),'-o',str(B/'test_particle_runtime')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
p=subprocess.run([str(B/'test_particle_runtime'),str(ROOT/'orig/SSA.gba')],env=env,text=True,capture_output=True,timeout=120)
if p.returncode:print(p.stdout,p.stderr);p.check_returncode()
report=json.loads(p.stdout);report['source_sha256']={s.name:hashlib.sha256(s.read_bytes()).hexdigest() for s in sources}
report['scope']='Native14B60 allocation/insertion,96 controller, actual00908 animation completion and release on all16maps. Native68 spread,6A splitting,E3 falling explosions and240children afterparentrelease;64 portalrelocation excluded. Scoped combat and controlled VM-phase admission, callback failures and reentry. Audio observed, not played. ASan+UBSan, no PIE, leak scanning disabled.'
(ROOT/'analysis/native-particle-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
