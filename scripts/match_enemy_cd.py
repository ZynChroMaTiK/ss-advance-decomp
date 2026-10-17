"""Use the actual decomp.wtf byte matcher for the small type-CD dispatcher.

Root session owns the claim; this script uses that same local identity and
does not duplicate claim generation. No export/rebuild is performed here.
"""
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
name = 'sub_08013978'
notes = ('Type-CD outer dispatcher: switch on entity byte +3E; state0 calls13940, '
         'state1 calls13924, state2 calls1385C; all states then call137DC. '
         'Native behavior and controlled-boundary differential tests live in '
         'src/enemy_cd.c and tests/test_enemy_cd_rom.py. Matching candidate '
         'recovered/enemy_cd_dispatch.c; attempts analysis/enemy-cd-match-attempts.json.')
if len(sys.argv) > 1:
    tool = sys.argv[1]
    args = {'function': name, 'notes': notes} if tool == 'release_function' else {'function': name, 'text': notes}
    print(call(net, agent, slug, tool, args), flush=True)
    raise SystemExit
path = root / 'analysis/enemy-cd-match-attempts.json'
records = json.loads(path.read_text()) if path.exists() else []
if len(records) >= 20:
    raise SystemExit('Twenty-attempt budget exhausted; release with notes.')
source = (root / 'recovered/enemy_cd_dispatch.c').read_text()
result = call(net, agent, slug, 'compile_and_diff', {'function': name, 'source': source})
records.append({'function': name, 'source': source, 'result': result})
path.write_text(json.dumps(records, indent=2) + '\n')
print(result, flush=True)
