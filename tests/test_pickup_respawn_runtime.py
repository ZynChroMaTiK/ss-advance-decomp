"""Native multiplayer pickup schedule -> shared pool/world on maps12..15."""
from pathlib import Path
import ast,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
tree=ast.parse((ROOT/'tests/test_script_spawn.py').read_text())
sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets))
tree=ast.parse((ROOT/'tests/test_weapon_runtime.py').read_text())
sources+=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.AugAssign) and isinstance(n.target,ast.Name) and n.target.id=='sources')
sources+=['pickup_respawn.c','pickup_respawn_runtime.c']
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_pickup_respawn_runtime.c'),*[str(ROOT/'src'/s) for s in sources],'-o',str(B/'test_pickup_respawn_runtime')],check=True)
rom=ROOT/'orig/SSA.gba';sha1=hashlib.sha1(rom.read_bytes()).hexdigest();assert sha1=='4ff48d8c18b70a5184758f1917343ae85a70858e'
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
result=subprocess.run([str(B/'test_pickup_respawn_runtime'),str(rom)],capture_output=True,text=True,timeout=120,env=env)
if result.returncode:print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
report=json.loads(result.stdout);report.update(rom_sha1=sha1,sanitizer_options=env['ASAN_OPTIONS'],sanitizer_binary_pie=False,
 scope='Native completepickuprespawn bridge onall4multiplayer maps12..15 withtwoactualplayers. All16scheduleslots,originalD9markers,sharedpool/world,animationmetadata,floorZ,hiddenpickup timer,occupancyanytype/Z,100slotexhaustion andfallbackparticle,release/reuse,presentationfailure retainedspawn. NativeF9B4/globalrenderer notinvolved; particleobservedonly; removalsaretestdriven notnetworkpickupdelivery. NoROM execution.',
 source_sha256={s:hashlib.sha256((ROOT/'src'/s).read_bytes()).hexdigest() for s in sources})
(ROOT/'analysis/native-pickup-respawn-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
