from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 str(r/'tests/test_pc_playability.c'),*map(str,sorted((r/'src').glob('*.c'))),'-o',str(b/'test_pc_playability')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_pc_playability')],cwd=r,capture_output=True,text=True,env=env,timeout=180)
print(p.stdout,end='');(b/'pc-playability.log').write_text(p.stderr)
if p.returncode:print(p.stderr,end='');p.check_returncode()
reports=[json.loads(line) for line in p.stdout.splitlines()];report={'result':'pass','completion':reports[0],'renderer':reports[1]}
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in [r/'src/pc_renderer.c',r/'src/pc_renderer_pitch.inc',r/'src/native_game.c',r/'tests/test_pc_playability.c',r/'tests/test_native_playability.c']}
(r/'analysis/pc-renderer-playability-validation.json').write_text(json.dumps(report,indent=2)+'\n')
