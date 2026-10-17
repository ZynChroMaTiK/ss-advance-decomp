"""Bounded compiler experiments for the recovered sine/cosine lookup."""
from pathlib import Path
import json
import os

root = Path(__file__).resolve().parents[1]
os.environ["DECOMPNET_TOOLCHAINS"] = str(root / ".toolchains")
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net = RomNetwork(root / "decomp/.decomp/decomp.db", root / "decomp")
slug = json.loads((root / "decomp/project.json").read_text())["slug"]
agent, _ = net.local_agent("codex-session")
base = (root / "recovered/sincos.c").read_text()
records = []
for table_type in ("u16", "volatile u16", "const u16"):
    for locals_order in (0, 1, 2, 3):
        source = base.replace("extern const u16", "extern " + table_type)
        old = "    s32 s = 0;\n    s32 c = 0;\n    u32 quadrant = heading >> 14;\n    u32 phase = heading & 0x3FFF;"
        blocks = ["s32 s = 0;", "s32 c = 0;", "u32 quadrant = heading >> 14;", "u32 phase = heading & 0x3FFF;"]
        orders = [(0,1,3,2),(2,3,0,1),(3,2,0,1),(1,0,2,3)]
        source = source.replace(old, "\n".join("    " + blocks[i] for i in orders[locals_order]))
        result = call(net, agent, slug, "compile_and_diff", {"function":"sub_080094E8", "source":source})
        print(f"{table_type} order {locals_order}: {result.splitlines()[0]}", flush=True)
        records.append({"table":table_type,"order":locals_order,"result":result})
        fn = net._function("sub_080094E8", slug)
        if fn["status"] == "matched": break
    if fn["status"] == "matched": break
best = fn["matched_source"] or fn["best_source"]
if best: (root / "recovered/sincos.c").write_text(best + "\n")
(root / "analysis/sincos-variants.json").write_text(json.dumps(records,indent=2)+"\n")
