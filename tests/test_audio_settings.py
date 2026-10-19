from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[1]
b=r/'build/audio-settings';b.mkdir(parents=True,exist_ok=True)
sources=[r/p for p in ('src/audio.c','src/audio/soundfont.c','src/menu.c','src/scores.c','src/map_view.c',
                      'src/map_assets.c','src/settings.c','tests/test_audio_settings.c')]
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 *map(str,sources),'-lm','-o',str(b/'test_audio_settings')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_audio_settings'),str(r/'orig/SSA.gba'),str(r/'soundfonts/SC-55.sf2'),str(b)],
 capture_output=True,text=True,env=env,timeout=120)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['sanitizers']='ASan/UBSan/leaks=1'
report['physical_listening_tested']=False
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in sources}
for name in ('include/ssa/music_instrument.h','include/ssa/settings.h','include/ssa/menu.h'):
    report['source_sha256'][name]=hashlib.sha256((r/name).read_bytes()).hexdigest()
report['soundfont_sha256']=hashlib.sha256((r/'soundfonts/SC-55.sf2').read_bytes()).hexdigest()
(r/'analysis/audio-settings-validation.json').write_text(json.dumps(report,indent=2)+'\n')
