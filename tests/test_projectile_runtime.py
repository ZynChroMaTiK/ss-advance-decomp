"""Native weapon creation, projectile flight/impact and shared pool integration."""
from pathlib import Path
import hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),
 str(ROOT/'tests/test_projectile_runtime.c'),*map(str,sources),
 '-o',str(BUILD/'test_projectile_runtime')],check=True)
rom=ROOT/'orig/SSA.gba';sha1=hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_projectile_runtime'),str(rom)],text=True,capture_output=True,timeout=120,env=env)
if result.returncode:
 print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native weapon 6/7/10 creation, real-map flight, collision, combat, shared-pool exhaustion/reuse, bounce and conversion/E2 follow. The legacy freezes metric counts conversion impacts. Sixteen maps retain jump/H movement. Two far-target cannon cases verify incoming credit preservation. Two multiplayer rocket kills use distinct owner counters and emit four observed network requests; packets are not sent or applied. Shot/hit arrays count single-player cases only. Private ROM-copy scripted CD fixtures; presentation, enemy navigation and corpse animation completion remain explicit controlled boundaries. No ROM instructions execute.',
 source_sha256={s.name:hashlib.sha256(s.read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-projectile-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
