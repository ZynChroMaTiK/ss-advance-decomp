from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-I',str(r/'include'),
 str(r/'analysis/probe_native_levels.c'),*map(str,sorted((r/'src').glob('*.c'))),'-o',str(b/'probe_native_levels')],check=True)
p=subprocess.run([str(b/'probe_native_levels'),str(r/'orig/SSA.gba')],text=True,capture_output=True,timeout=120)
(b/'native-level-probe.txt').write_text(p.stdout);p.check_returncode();print(p.stdout)
