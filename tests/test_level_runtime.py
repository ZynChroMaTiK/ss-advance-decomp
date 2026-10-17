"""Native level scripts, movers, switches and player runtime on all real maps."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
sources=['level_runtime.c','level_script.c','sector_movers.c','switches.c',
 'player_gameplay.c','player_lifecycle.c','player_hazards.c','respawn_select.c',
 'player_runtime.c','player_frame.c','player_spawn.c','player_motion.c','spawn.c',
 'world.c','slide.c','horizontal.c','view.c','geometry.c','physics.c','movement.c',
 'map_assets.c','inventory.c','weapon_select.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),
 str(ROOT/'tests/test_level_runtime.c'),*[str(ROOT/'src'/s) for s in sources],
 '-o',str(BUILD/'test_level_runtime')],check=True)
rom=ROOT/'orig/SSA.gba'; sha1=hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy(); env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_level_runtime'),str(rom)],text=True,capture_output=True,timeout=30,env=env)
if result.returncode:
 print(result.stdout,end=''); print(result.stderr,end=''); result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native all16map level initialization before players, scripts/movers/switches/sector queue and player movement/jump/H-camera/death. Controlled script-spawn and presentation callbacks. Synthetic command data only in private ROM copy; no ROM instructions execute. No host entity AI or renderer.',
 source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-level-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
