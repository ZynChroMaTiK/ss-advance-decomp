"""Use decomp.wtf claims and byte matcher for the two original body copies."""
from pathlib import Path
import json
import os
import sys

root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug']
agent,_=net.local_agent('codex-player-sync')
if len(sys.argv)>1:
    print(call(net,agent,slug,'get_context',{}) if sys.argv[1]=='get_context' else
          call(net,agent,slug,'guide',{'topic':sys.argv[1]}))
    raise SystemExit
path=root/'analysis/player-sync-match-attempts.json'
records=json.loads(path.read_text()) if path.exists() else []
for name,sourcefile in [('sub_08005E04','player_sync_to_body.c'),('sub_08005E28','player_sync_from_body.c')]:
    print(call(net,agent,slug,'claim_function',{'function':name}),flush=True)
    source=(root/'recovered'/sourcefile).read_text()
    result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
    print(result,flush=True)
    records.append({'function':name,'source':source,'result':result})
    path.write_text(json.dumps(records,indent=2)+'\n')
    if net._function(name,slug)['status']!='matched':
        print(call(net,agent,slug,'release_function',{'function':name,'notes':
            'Original six-component player/entity copy and heading high-byte conversion. Native complete064B8 in src/player_motion.c preserves quantization with NULL options; native retain_heading option deliberately retains low eight bits. Whole real-map motion differential validation in test_world_rom.py.'}),flush=True)
