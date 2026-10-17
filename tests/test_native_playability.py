"""Complete level one through normal PC input and actual simulation/rendering."""
from pathlib import Path
import hashlib, json, os, subprocess

root=Path(__file__).resolve().parents[1]
build=root/'build/native-tests'
build.mkdir(parents=True,exist_ok=True)
source=root/'tests/test_native_playability.c'
sources=sorted((root/'src').glob('*.c'))
binary=build/'test_native_playability'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(root/'include'),
 str(source),*map(str,sources),'-o',str(binary)],check=True)
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=1')
result=subprocess.run([str(binary)],cwd=root,text=True,capture_output=True,env=env,timeout=180)
print(result.stdout,end='')
(build/'native-playability.log').write_text(result.stderr)
if result.returncode:
 print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
assert report['result']=='pass' and report['finished']==1
report.update(scope='Level0 complete through portable PC event APIs, original native gameplay and rendered frames. Bot reads map/actors; uses separate aim scratch and copied planning map. No emulator, direct health/ammo/damage/script/body modifications, forced kills, or forced animation completion.',
 rom_sha1=hashlib.sha1((root/'orig/SSA.gba').read_bytes()).hexdigest(),
 sanitizer_options=env['ASAN_OPTIONS'],source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source,*sources,*sorted((root/'include/ssa').glob('*.h')),root/'src/native_audio.inc']})
(root/'analysis/native-playability-validation.json').write_text(json.dumps(report,indent=2)+'\n')
