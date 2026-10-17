from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined',
 '-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),str(r/'tests/test_third_level.c'),*map(str,sources),'-o',str(b/'test_third_level')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_third_level')],cwd=r,capture_output=True,text=True,env=env,timeout=300)
(b/'third-level.log').write_text(p.stderr);print(p.stdout,end='')
if p.returncode:print(p.stderr[-7000:],end='');p.check_returncode()
levels=[json.loads(line) for line in p.stdout.splitlines()]
assert len(levels)==3 and all(level['result']=='pass' and level['finished']==1 for level in levels)
report={'result':'pass','levels':levels,'sanitizers':'ASan/UBSan/leaks=1',
 'source_sha256':{str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in
 sources+list((r/'include/ssa').glob('*.h'))+[r/'src/pc_renderer_pitch.inc',r/'src/native_audio.inc',
 r/'tests/test_third_level.c',r/'tests/test_second_level.c',r/'tests/test_native_playability.c',Path(__file__)]}}
(r/'analysis/third-level-validation.json').write_text(json.dumps(report,indent=2)+'\n')
