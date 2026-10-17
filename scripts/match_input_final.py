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
base = (root / "recovered/input_record.c").read_text()
variants = {
 "pointer comparison, global append": base.replace("(u32)D_030042F0.record_count", "(u32)input->record_count"),
 "local unsigned limit, global append": base.replace("u8 packed;", "u8 packed;\n    u32 limit = 0xE0E;").replace("(u32)D_030042F0.record_count <= 0xE0EU", "input->record_count <= limit"),
 "local signed limit, global append": base.replace("u8 packed;", "u8 packed;\n    s32 limit = 0xE0E;").replace("(u32)D_030042F0.record_count <= 0xE0EU", "input->record_count <= limit"),
 "limit declared first, global append": base.replace("InputState *input = &D_030042F0;", "u32 limit = 0xE0E;\n    InputState *input = &D_030042F0;").replace("(u32)D_030042F0.record_count <= 0xE0EU", "input->record_count <= limit"),
}
records = []
for label, source in variants.items():
    result = call(net, agent, slug, "compile_and_diff", {"function": "sub_08003150", "source": source})
    print(label + ": " + result.splitlines()[0], flush=True)
    records.append({"variant": label, "result": result})
    if net._function("sub_08003150", slug)["status"] == "matched": break
fn = net._function("sub_08003150", slug)
(root / "recovered/input_record.c").write_text((fn["matched_source"] or fn["best_source"]) + "\n")
(root / "analysis/input-final-variants.json").write_text(json.dumps(records, indent=2) + "\n")
if fn["status"] != "matched":
    print(call(net, agent, slug, "release_function", {"function": "sub_08003150", "notes": "Recovered input recorder and struct. Best 97.5% before final variants; only initial pointer/limit/load ordering differs. Full notes in workspace analysis/input-variants.json. Do not spend unbounded compiles."}), flush=True)
