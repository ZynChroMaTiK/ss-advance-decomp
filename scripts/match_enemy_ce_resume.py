"""Bounded actual decomp.wtf match for the CE hurt-resume helper."""
from pathlib import Path
import json,os,sys
root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug'];agent,_=net.local_agent('codex-enemy-ce')
name='sub_08013438'
notes='CE hurt-resume 13438 waits for animation_done, sets state 1, clears flag 1, then sets animation 10 with period 5. Two manual compile attempts: unsigned flags scored 235 / 89.7%, 23 instructions; signed flags scored 540 / 76.5%, 20 instructions. NOT exact. Best unsigned candidate retained; pointer register allocation and halfword/literal load ordering differ. No assembly or register forcing. Full native 135C4 and D3-D6 controllers pass 11,994 whole-routine comparisons, including 134 CE knockbacks. See analysis/enemy-ranged.md.'
if len(sys.argv)>1:
    tool=sys.argv[1];args={'topic':sys.argv[2] if len(sys.argv)>2 else 'workflow'} if tool=='guide' else {'function':name,'text':notes} if tool=='add_note' else {'function':name,'notes':notes} if tool=='release_function' else {'function':name}
    print(call(net,agent,slug,tool,args),flush=True);raise SystemExit
path=root/'analysis/enemy-ce-resume-match-attempts.json';records=json.loads(path.read_text()) if path.exists() else []
if len(records)>=2:raise SystemExit('Two-attempt budget exhausted.')
source=(root/'recovered/enemy_ce_resume.c').read_text();result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
records.append({'source':source,'result':result});path.write_text(json.dumps(records,indent=2)+'\n');print(result,flush=True)
