"""Original initial loadout through native firing and real-map gameplay."""
from pathlib import Path
import ast,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
tree=ast.parse((ROOT/'tests/test_script_spawn.py').read_text())
sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets))
tree=ast.parse((ROOT/'tests/test_weapon_runtime.py').read_text())
sources+=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.AugAssign) and isinstance(n.target,ast.Name) and n.target.id=='sources')
if 'weapon_loadout.c' not in sources:sources.append('weapon_loadout.c')
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-Wl,--wrap=calloc','-I',str(ROOT/'include'),str(ROOT/'tests/test_weapon_start.c'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'test_weapon_start')],check=True)
rom=ROOT/'orig/SSA.gba';sha1=hashlib.sha1(rom.read_bytes()).hexdigest();assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(B/'test_weapon_start'),str(rom)],capture_output=True,text=True,timeout=120,env=env)
if result.returncode:print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout);report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native1240C loads allsettings0..36 plusgod on16maps from unarmed freshlyinitialized gameplay; actualinitial selection/animation/cooldowns and firing weapons1/2/3/6/7/10 without test-selectedloadout. Weapon9initialization tested butfiringoutsideboundeffects. Real-map hitscan/combat andprojectileflight; newjump/H frames. Two-player active/inactive/indexedcooldowns andinjectedsecond-playercallocfailure preservingfirstbinding. Spriteobjectallocation usesembeddednativeobjects, nooriginalspritepool claim; presentation callbacks explicit. NoROM instructions execute.',
 source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-weapon-start-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
