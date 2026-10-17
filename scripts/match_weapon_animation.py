"""Bounded actual decomp.wtf matcher; root owns export/rebuild."""
from pathlib import Path
import json
import os
import sys
root = Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS'] = str(root / '.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net = RomNetwork(root / 'decomp/.decomp/decomp.db', root / 'decomp')
slug = json.loads((root / 'decomp/project.json').read_text())['slug']
agent, _ = net.local_agent('codex-session')
name = 'sub_0800FC04'
notes = ('Sprite animation setter: ID0..E8 orFFFF accepted; other IDs no-op. '
         'Set+20ID,+24byteperiod; clear+22frame,+25elapsed,+26done. '
         'Full animation tick0800E460 reads table0844B36C, native weapon_animation.c; '
         '275520 ROM comparisons in tests/test_weapon_animation_rom.py. '
         'Recovered candidate recovered/weapon_animation_set.c.')
if len(sys.argv) > 1:
    tool = sys.argv[1]
    args = {'function': name, 'notes': notes} if tool == 'release_function' else {'function': name, 'text': notes}
    print(call(net, agent, slug, tool, args), flush=True)
    raise SystemExit
path = root / 'analysis/weapon-animation-match-attempts.json'
records = json.loads(path.read_text()) if path.exists() else []
if len(records) >= 20: raise SystemExit('Twenty-attempt limit reached.')
source = (root / 'recovered/weapon_animation_set.c').read_text()
result = call(net, agent, slug, 'compile_and_diff', {'function': name, 'source': source})
records.append({'function': name, 'source': source, 'result': result})
path.write_text(json.dumps(records, indent=2) + '\n')
print(result, flush=True)
