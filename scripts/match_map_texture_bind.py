"""Bounded real decomp.wtf matching; root owns export/rebuild."""
from pathlib import Path
import json,os,sys
root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug'];agent,_=net.local_agent('codex-session')
name='sub_0800B0C4'
notes=('B0C4 resolves map16byte descriptors with B0FC: first case-insensitive8 match in WORLD+4 basearchive. '
 'On match copieswidth/height then overwrites+C with signed16 archiveindex. Missing retainsincomingdimensions,index0. '
 'Native map_texture_bindings.c preservesdecodedmapbytes and live side+4/+6/+8 IDs; FFFFside absent. '
 'Sector+A/+B areflatcolor/skyselectors, NOTmaptextureIDs (BA04/A888/AB10/A98C). '
 'WholeB0C4/B0FC/realC57F4 nohooktests test_map_texture_bindings_rom.py.')
if len(sys.argv)>1:
 tool=sys.argv[1];args={'function':name,'notes':notes} if tool=='release_function' else {'function':name,'text':notes}
 print(call(net,agent,slug,tool,args),flush=True);raise SystemExit
p=root/'analysis/map-texture-bind-match-attempts.json';history=json.loads(p.read_text()) if p.exists() else []
if len(history)>=20:raise SystemExit('Twenty attempt budget exhausted.')
source=(root/'recovered/map_texture_bind.c').read_text()
result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
history.append({'source':source,'result':result});p.write_text(json.dumps(history,indent=2)+'\n');print(result,flush=True)
