"""Native generic enemy combat/aim/projectile integration, all real maps."""
from pathlib import Path
import hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/native-tests';BUILD.mkdir(parents=True,exist_ok=True)
sources=sorted((ROOT/'src').glob('*.c'))
binary=BUILD/'test_enemy_control_runtime'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(ROOT/'include'),str(ROOT/'tests/test_enemy_control_runtime.c'),*map(str,sources),'-o',str(binary)],check=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
result=subprocess.run([str(binary),str(ROOT/'orig/SSA.gba')],capture_output=True,text=True,env=env)
if result.returncode:
    print(result.stdout);print(result.stderr);result.check_returncode()
report=json.loads(result.stdout)
report.update(status='passed',rom_sha1=hashlib.sha1((ROOT/'orig/SSA.gba').read_bytes()).hexdigest(),
    scope='Native CE..D6 controllers, real map queries/motion/autoaim/projectile creation/contact damage/group release and animation clocks. Includes converted Gunrilla firing through a retained cleared slot reused for its projectile on all16maps, with foreign-target and unregistered-source guards. Script fixtures place original enemy definitions at real player starts. Projectile flight is separately tested; here created projectiles are counted then released. Audio is observed, not played.',
    source_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+[
        ROOT/'src/projectile_runtime_internal.h',ROOT/'tests/test_enemy_control_runtime.c',
        ROOT/'tests/test_enemy_cd_runtime.c',ROOT/'tests/test_script_spawn.c',Path(__file__)]})
(ROOT/'analysis/native-enemy-control-runtime-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='source_sha256'},indent=2))
