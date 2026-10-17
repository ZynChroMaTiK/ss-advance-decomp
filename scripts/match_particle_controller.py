"""Bounded decomp.wtf matching for particle96 controller14C58."""
from pathlib import Path
import json,os,sys
root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug'];agent,_=net.local_agent('codex-particle')
name='sub_08014C58'
notes=('14C58 type96 only reads entity animationdone+5F; nonzero calls037E0 unlink/release. '
 'Particle14B60 creates all16 effectIDs as type96; src/particle_create.c reproduces allocation, '
 'animation05394, insertionA5D0, optional8D5C sound and flags20/2000 ordering. '
 'tests/test_particle_create_rom.py verifies4096whole14B60,256whole14C58,8192whole9D4C. '
 'Native runtime actual00908 animation completes256particlecycles across16maps underASan/UBSan. '
 'One manualcompile matches all22executed bytes but score100/90.9percent, NOTexact: imported24byte '
 'extent includes trailing0000 alignment afterBX. No forcedpadding/assembly added; claim released.')
if len(sys.argv)>1:
 tool=sys.argv[1]
 args={'topic':sys.argv[2] if len(sys.argv)>2 else 'workflow'} if tool=='guide' else {'function':name,'text':notes} if tool=='add_note' else {'function':name,'notes':notes} if tool=='release_function' else {'function':name}
 print(call(net,agent,slug,tool,args),flush=True);raise SystemExit
p=root/'analysis/particle-controller-match-attempts.json';history=json.loads(p.read_text()) if p.exists() else []
if len(history)>=5:raise SystemExit('Five attempt budget exhausted.')
source=(root/'recovered/particle_controller.c').read_text();result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
history.append({'source':source,'result':result});p.write_text(json.dumps(history,indent=2)+'\n');print(result,flush=True)
