"""Native contact/explosion damage, chain deaths, player death and script ownership."""
from pathlib import Path
import ast,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
tree=ast.parse((ROOT/'tests/test_script_spawn.py').read_text())
sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets))
sources+=['enemy_cd_runtime.c','enemy_cd.c','enemy_navigation.c','enemy_lifecycle.c','enemy_motion.c',
 'enemy_queries.c','enemy_targeting.c','enemy_ray.c','ray_traversal.c','ray_intercepts.c',
 'combat_damage.c','combat_explosion.c','combat_rewards.c','combat_runtime.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_combat_runtime.c'),*[str(ROOT/'src'/s) for s in sources],'-o',str(BUILD/'test_combat_runtime')],check=True)
rom=ROOT/'orig/SSA.gba';sha1=hashlib.sha1(rom.read_bytes()).hexdigest();assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_combat_runtime'),str(rom)],text=True,capture_output=True,timeout=90,env=env)
if result.returncode:
 print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native damage, explosion, rewards, CD controller with native queries, script groups/corpse release and player death on16maps; jump/H retained. Synthetic spawn fixtures in private ROMcopy, direct damage is a test trigger, no weapon firing or ROM instructions execute. Sound/animation remain presentation callbacks; network packets not delivered.',
 source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-combat-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
