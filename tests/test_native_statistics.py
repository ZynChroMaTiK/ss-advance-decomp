"""Read-only native statistics projection on all16maps; counter fixtures are not completion proof."""
from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'));binary=b/'test_native_statistics'
subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 str(r/'tests/test_native_statistics.c'),*map(str,sources),'-o',str(binary)],check=True)
p=subprocess.run([str(binary),str(r/'orig/SSA.gba')],capture_output=True,text=True,
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=180)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode();report=json.loads(p.stdout)
report.update(sanitizers='ASan/UBSan/leaks=1',scope=__doc__)
inputs=sources+list((r/'include/ssa').glob('*.h'))+[r/'src/native_audio.inc',r/'tests/test_native_statistics.c',Path(__file__)]
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs}
(r/'analysis/native-statistics-validation.json').write_text(json.dumps(report,indent=2)+'\n')
