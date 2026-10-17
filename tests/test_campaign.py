"""Complete every admitted native campaign map using only PC input and earned carry."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
count=int(re.search(r'SSA_NATIVE_CAMPAIGN_LEVELS=(\d+)',(r/'include/ssa/native_game.h').read_text()).group(1))
fresh=None
if len(sys.argv)>1:
 if len(sys.argv)!=3 or sys.argv[1]!='--fresh' or not sys.argv[2].isdigit() or not 2<=int(sys.argv[2])<=11:
  raise SystemExit('Usage: test_campaign.py [--fresh LEVEL_INDEX]')
 fresh=int(sys.argv[2])
sources=sorted((r/'src').glob('*.c'))
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 str(r/'tests/test_campaign.c'),*map(str,sources),'-o',str(b/'test_campaign')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_campaign'),*(['--fresh',str(fresh)] if fresh is not None else [])],cwd=r,capture_output=True,text=True,env=env,timeout=600)
(b/('campaign-standalone.log' if fresh is not None else 'campaign.log')).write_text(p.stderr);print(p.stdout,end='')
if p.returncode:print(p.stderr[-10000:],end='');p.check_returncode()
levels=[json.loads(line) for line in p.stdout.splitlines()]
assert len(levels)==(1 if fresh is not None else count) and all(level['result']=='pass' and level['finished']==1 for level in levels)
if fresh is not None:assert levels[0]['level']==fresh
inputs=sources+list((r/'include/ssa').glob('*.h'))+[r/'src/pc_renderer_pitch.inc',r/'src/native_audio.inc',
 r/'tests/test_campaign.c',r/'tests/test_second_level.c',r/'tests/test_native_playability.c',Path(__file__)]
report={'result':'pass','level_count':len(levels),'levels':levels,'sanitizers':'ASan/UBSan/leaks=1',
 'scope':('Standalone normal-input completion from the original Normal-difficulty level-start loadout. No grants or script/world shortcuts; does not establish campaign carry-over or admit this map in the frontend.' if fresh is not None else
 'Consecutive normal-input completions with earned inventory only; no grants or script/world shortcuts. Does not establish complete original feature parity.'),
 'source_sha256':{str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs}}
(r/'analysis'/('campaign-standalone-validation.json' if fresh is not None else 'campaign-validation.json')).write_text(json.dumps(report,indent=2)+'\n')
