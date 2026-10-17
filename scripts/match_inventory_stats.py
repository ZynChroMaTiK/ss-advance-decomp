"""Bounded matching experiments for recovered original pickup stat helpers."""
from pathlib import Path
import json
import os
import sys

root = Path(__file__).resolve().parents[1]
os.environ["DECOMPNET_TOOLCHAINS"] = str(root / ".toolchains")
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net = RomNetwork(root / "decomp/.decomp/decomp.db", root / "decomp")
slug = json.loads((root / "decomp/project.json").read_text())["slug"]
agent, _ = net.local_agent("codex-inventory")
suffix = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] in ("c6", "health") else "c4"
name = {"c4": "sub_080054D8", "c6": "sub_08005510", "health": "sub_0800549C"}[suffix]
source_path = root / ("recovered/inventory_stat_" + suffix + ".c")
base = source_path.read_text()
variants = [("natural struct", base)]
if len(sys.argv) > 1 and sys.argv[1] == "variants":
    variants = [
        ("unsigned field signed checks", base.replace("s16 value", "u16 value").replace("player->value <", "(s16)player->value <").replace("player->value >", "(s16)player->value >")),
        ("pointer local", base.replace("    if (amount", "    s16 *value = &player->value;\n    if (amount").replace("player->value", "*value").replace("&*value", "&player->value")),
        ("signed amount int param", base.replace("s16 amount)", "s32 input)").replace("    if (amount", "    s16 amount = input;\n    if (amount")),
        ("upper comparison63", base.replace("player->value < 100", "player->value <= 99")),
        ("return100", base.replace("    } else if", "        return;\n    }\n    if")),
    ]
path = root / "analysis/inventory-match-attempts.json"
records = json.loads(path.read_text()) if path.exists() else []
for label, source in variants:
    result = call(net, agent, slug, "compile_and_diff", {"function": name, "source": source})
    print(label + ": " + result, flush=True)
    records.append({"function": name, "label": label, "source": source, "result": result})
    path.write_text(json.dumps(records, indent=2) + "\n")
    fn = net._function(name, slug)
    if fn["status"] == "matched":
        break
best = fn["matched_source"] or fn["best_source"]
if best:
    source_path.write_text(best + "\n")
