"""Six final register-allocation trials, then retain the best candidate."""
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
base = base.replace("    s32 c = 0;\n    s32 s = 0;", "    s32 s = 0;\n    s32 c = 0;")
declarations = {"s":("s32","r4"), "c":("s32","r1"), "phase":("u32","r2"), "quadrant":("u32","r3")}
for pins in (("s","c","phase","quadrant"),("c",),("phase",),("quadrant",),("c","phase"),()):
    source = base
    for name in pins:
        typ,reg = declarations[name]
        source = source.replace(f"{typ} {name} =", f'register {typ} {name} asm("{reg}") =')
    result = call(net, agent, slug, "compile_and_diff", {"function":"sub_080094E8", "source":source})
    print(f"{pins}: {result.splitlines()[0]}", flush=True)
    records.append({"pins":pins,"result":result})
    fn = net._function("sub_080094E8", slug)
    if fn["status"] == "matched": break
best = fn["matched_source"] or fn["best_source"]
if best: (root / "recovered/sincos.c").write_text(best + "\n")
(root / "analysis/sincos-final.json").write_text(json.dumps(records,indent=2)+"\n")
