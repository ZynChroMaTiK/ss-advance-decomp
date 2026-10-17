"""Native real-map pickup/world/pool/inventory integration, ASan and UBSan."""
from pathlib import Path
import hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'build/native-tests';B.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'));fixture=ROOT/'tests/test_pickup_runtime.c'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(fixture),*map(str,sources),'-o',str(B/'test_pickup_runtime')],check=True)
run=subprocess.run([str(B/'test_pickup_runtime'),str(ROOT/'orig/SSA.gba')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
if run.returncode:print(run.stdout);print(run.stderr);run.check_returncode()
report=json.loads(run.stdout);report['rom_sha1']=hashlib.sha1((ROOT/'orig/SSA.gba').read_bytes()).hexdigest()
assert report['rom_sha1']=='4ff48d8c18b70a5184758f1917343ae85a70858e'
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
report['fixture_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (fixture,Path(__file__))}
report['scope']='Actual16maps, adopted originalE1 pickups, native world contact/pool release/inventory/weapon queue handoff/script queue. Presentation observed; scripted queue contents are not executed here. Test player repositioning uses world remove/insert. Synthetic cases cover cell traversal, refusal, inherited update locks, reuse and malformed guards. No emulator in native runtime.'
(ROOT/'analysis/native-pickup-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if not k.endswith('sha256')},indent=2))
