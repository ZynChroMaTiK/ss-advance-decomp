"""Bounded actual decomp.wtf matching for original actor-slot allocator F98."""
from pathlib import Path
import json,os,sys
root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug'];agent,_=net.local_agent('codex-session')
name='sub_08000F98'
notes=('F98 allocates one of32 actor12byte records from03003F60+6, returnsFFFF withoutmutation on exhaustion. '
 'Native actor_queue.c recovers1238/B24C mixed wall depth/clipping/insertion and01828 reverse draw. '
 '1238 writes clipped_width+2 top/bottom pairs but advances wordcursor only2*clipped_width, preserving4wordtailoverlap. '
 'tests/test_actor_queue_rom.py compares32768 whole1238 and256 whole01828 actualresourcepixelpasses on16maps. '
 'Matching stoppedafter5manualcompiles atscore315/80.3%. Bestfailure-firstif/else result=count++ hascorrectbodyregisters '
 'butagbccadds pushLR/popR1/BXR1 vs originalframelessBXLR, shiftingliteralpoolalignment. Returnu16vsu32unchanged. '
 'No nakedattribute orinlineassembly substituted; recovered/actor_queue_allocate.c retainsbestcandidate.')
if len(sys.argv)>1:
 tool=sys.argv[1];args={'topic':'workflow'} if tool=='guide' else {'function':name,'text':notes} if tool=='add_note' else {'function':name,'notes':notes} if tool=='release_function' else {'function':name}
 print(call(net,agent,slug,tool,args),flush=True);raise SystemExit
p=root/'analysis/actor-queue-allocate-match-attempts.json';history=json.loads(p.read_text()) if p.exists() else []
if len(history)>=20:raise SystemExit('Twenty attempt budget exhausted.')
source=(root/'recovered/actor_queue_allocate.c').read_text()
result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
history.append({'source':source,'result':result});p.write_text(json.dumps(history,indent=2)+'\n');print(result,flush=True)
