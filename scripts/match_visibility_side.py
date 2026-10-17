"""Bounded actual decomp.wtf matching for BSP node-side adapter B220."""
from pathlib import Path
import json,os,sys
root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug'];agent,_=net.local_agent('codex-session')
name='sub_0800B220'
notes=('B220 signextendsnode+4/+6direction and+0/+2origin into two local32bit vectors thenB1B0(camera,origin,direction). '
 'Native world_visibility.c recoversB6B0/B428/B7B0/B818/BA04 plusAC8/C10 and nativeplaneA888; '
 'wholeROMtests test_world_visibility_rom.py acrossall16maps compareorderedwallqueue,projection,occlusion,planesandbitsets. '
 'Drawing017CC/D2D0/01828 remainsseparateboundary.')
if len(sys.argv)>1:
 tool=sys.argv[1]
 args={'topic':'workflow'} if tool=='guide' else {'function':name,'text':notes} if tool=='add_note' else {'function':name,'notes':notes} if tool=='release_function' else {'function':name}
 print(call(net,agent,slug,tool,args),flush=True);raise SystemExit
p=root/'analysis/visibility-side-match-attempts.json';history=json.loads(p.read_text()) if p.exists() else []
if len(history)>=20:raise SystemExit('Twenty attempt budget exhausted.')
source=(root/'recovered/visibility_side.c').read_text()
result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
history.append({'source':source,'result':result});p.write_text(json.dumps(history,indent=2)+'\n');print(result,flush=True)
