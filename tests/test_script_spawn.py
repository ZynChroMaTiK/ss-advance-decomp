"""Native script-spawn ownership and activation integration on real maps."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['script_spawn.c','entity_pool.c','enemy_activation.c','entity_timer.c',
 'level_runtime.c','level_script.c','sector_movers.c','switches.c',
 'player_gameplay.c','player_lifecycle.c','player_hazards.c','respawn_select.c',
 'player_runtime.c','player_frame.c','player_spawn.c','player_motion.c','spawn.c',
 'world.c','slide.c','horizontal.c','view.c','geometry.c','physics.c','movement.c',
 'map_assets.c','inventory.c','weapon_select.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),
 str(ROOT/'tests/test_script_spawn.c'),*[str(ROOT/'src'/s) for s in sources],
 '-o',str(BUILD/'test_script_spawn')],check=True)
rom=ROOT/'orig/SSA.gba'; sha1=hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy(); env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_script_spawn'),str(rom)],text=True,capture_output=True,timeout=45,env=env)
if result.returncode:
 print(result.stdout,end=''); print(result.stderr,end=''); result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native all-16-map script spawn pool, world insertion, initializer countdown, group release and player jump/H-camera integration. All 116 original scripts execute with actual native entity insertion and explicit test-driven release. Presentation effects are observed callbacks. Synthetic records are only in a private ROM copy; no ROM instructions execute. No enemy AI, attacks, animation playback or renderer.',
 source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-script-spawn-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
