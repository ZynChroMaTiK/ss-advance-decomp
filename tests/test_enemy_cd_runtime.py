"""Native type-CD controller, world, script lifecycle and player integration."""
from pathlib import Path
import ast
import hashlib
import json
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests'; BUILD.mkdir(parents=True,exist_ok=True)
tree=ast.parse((ROOT/'tests/test_script_spawn.py').read_text())
sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets))
sources+=['enemy_cd_runtime.c','enemy_cd.c','enemy_navigation.c','enemy_lifecycle.c','enemy_motion.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_enemy_cd_runtime.c'),*[str(ROOT/'src'/s) for s in sources],'-o',str(BUILD/'test_enemy_cd_runtime')],check=True)
rom=ROOT/'orig/SSA.gba'; sha1=hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy(); env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(BUILD/'test_enemy_cd_runtime'),str(rom)],text=True,capture_output=True,timeout=45,env=env)
if result.returncode:
 print(result.stdout,end=''); print(result.stderr,end=''); result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,scope='Native type CD controller/navigation/path geometry/turn/world walk/gravity/hazards/drain/death/corpse/script-group/pool-release on all16maps; native player jump/H updates. Visibility/nearest and path-ray clearance explicitly controlled. Animation/audio/contact/explosion events observed; no combat or animation playback implemented by test. Synthetic spawn scripts modify private ROM copy only; no ROM instructions execute.',source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-enemy-cd-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
