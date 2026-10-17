"""Native original-order entity phase across players, enemies, projectiles and pickups."""
from pathlib import Path
import hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),
 str(ROOT/'tests/test_entity_runtime.c'),*map(str,sources),
 '-o',str(BUILD/'test_entity_runtime')],check=True)
rom=ROOT/'orig/SSA.gba';sha1=hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_entity_runtime'),str(rom)],text=True,capture_output=True,timeout=120,env=env)
if result.returncode:
 print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Original active-slot entity phase on all16realmaps with native player gameplay/level interactions/jump/H, CD sight/navigation/world/combat, projectile flight/impact and E1timer. Actual projectile and corpse releases prove swapped entity skips until next phase. Unsupported CE handler fails; D9table no-op unchanged. Scripted CD and manually constructed E1/D9/CE fixtures; presentation and corpse animation completion controlled. No ROM execution, other enemy AI, complete game loop or rendering.',
 source_sha256={s.name:hashlib.sha256(s.read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-entity-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
