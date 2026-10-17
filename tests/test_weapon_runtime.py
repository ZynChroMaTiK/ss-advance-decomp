"""Native animation-clock firing -> actual map hitscan -> combat -> enemy lifecycle."""
from pathlib import Path
import ast,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
tree=ast.parse((ROOT/'tests/test_script_spawn.py').read_text())
sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets))
sources+=['enemy_cd_runtime.c','enemy_cd.c','enemy_navigation.c','enemy_lifecycle.c','enemy_motion.c',
 'enemy_targeting.c','enemy_ray.c','ray_traversal.c','ray_intercepts.c',
 'combat_damage.c','combat_explosion.c','combat_rewards.c','combat_runtime.c',
 'weapon_animation.c','weapon_controller.c','weapon_aim.c','weapon_hit.c','weapon_runtime.c',
 'projectile_create.c','projectile_motion.c','projectile_controller.c','projectile_bounce.c','projectile_effect.c','projectile_runtime.c','weapon_loadout.c',
 'projectile_extended.c','projectile_target.c','particle_create.c','sector_clip.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),
 str(ROOT/'tests/test_weapon_runtime.c'),*[str(ROOT/'src'/s) for s in sources],
 '-o',str(BUILD/'test_weapon_runtime')],check=True)
rom=ROOT/'orig/SSA.gba';sha1=hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_weapon_runtime'),str(rom)],text=True,capture_output=True,timeout=120,env=env)
if result.returncode:
 print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native weapon1..5 firing with actual animation data, map aiming/hit geometry and combat on16maps:16directsingleplayerkills and64CDproximitydeaths,80corpse releases. Selection/cooldowns/pause/visibility/nearwall, unsupportedprojectile6/7/10 andpresentationfailures. 2048jump/Hframes,4816singleplayerdeathframes,16explicitrespawnhelperchecks. Genuine2playerbindings:twoindependentshooterkillcounters,301deathframes andoneautomaticrespawn. Private ROMcopy synthetic scripted CD fixtures. Presentation callbacks have no audio/render implementation; enemy navigation boundaries andcorpseanimationcompletion are test-controlled. No ROM instructions execute.',
 source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-weapon-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
