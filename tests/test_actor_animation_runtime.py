"""Real native actor playback and corpse lifecycle on all16maps."""
from pathlib import Path
import hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),
 str(ROOT/'tests/test_actor_animation_runtime.c'),*map(str,sources),'-o',str(BUILD/'test_actor_animation_runtime')],check=True)
rom=ROOT/'orig/SSA.gba';sha1=hashlib.sha1(rom.read_bytes()).hexdigest();assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_actor_animation_runtime'),str(rom)],text=True,capture_output=True,timeout=120,env=env)
if result.returncode:
 print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native actor00908 stage, all16real maps, actual combat/CD death and32 automatic animation completions/corpse releases, slot reuse, reserved player animation setters and2048jump/Hframes. Native enemy queries; presentation observed only; no manual completion writes or runtime emulation.',
 source_sha256={s.name:hashlib.sha256(s.read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-actor-animation-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
