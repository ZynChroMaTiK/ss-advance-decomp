"""Bounded actual decomp.wtf matching for original074F0 affine defaults."""
from pathlib import Path
import json,os,sys
root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug'];agent,_=net.local_agent('codex-display')
name='sub_080074F0'
notes=('074F0 writes display030000C0 BG2X/Y+18/+1C=0,PA+8=128,PB+C=0,PC+10=0,PD+14=192. '
       '0750C normalgameplay writes DISPCNT1441 andBG2CNT6085/6089, yieldingexact120x120to240x160 '
       'samplingx/2,floor3y/4. ARM264 swizzles120x120indicesinto225columnordered8x8tiles. '
       'Native display.c implementsboundedpalette/OAMcomposition; tests/test_display.py verifies448wholeROMcalls '
       'and635independentpixelframes. Matching stoppedafter5manualcompiles atscore235/83.2%,NOTexact. '
       'Same14instructioncount, base/zero assignedr2/r1 instead oforiginalr1/r2; initialLDR scheduledbeforeMOV0. '
       'Array,assignmentchain,volatile,andnamedstruct variants tied235; groupedzero stores worsened495. '
       'Bestnamedstruct restored in recovered/display_defaults.c. Noassembly/registerforcing used.')
if len(sys.argv)>1:
    tool=sys.argv[1]
    args={'topic':sys.argv[2] if len(sys.argv)>2 else 'workflow'} if tool=='guide' else {'function':name,'text':notes} if tool=='add_note' else {'function':name,'notes':notes} if tool=='release_function' else {'function':name}
    print(call(net,agent,slug,tool,args),flush=True);raise SystemExit
p=root/'analysis/display-defaults-match-attempts.json';history=json.loads(p.read_text()) if p.exists() else []
if len(history)>=5:raise SystemExit('Five attempt budget exhausted.')
source=(root/'recovered/display_defaults.c').read_text()
result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
history.append({'source':source,'result':result});p.write_text(json.dumps(history,indent=2)+'\n');print(result,flush=True)
