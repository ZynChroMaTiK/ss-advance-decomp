"""Bounded declaration experiments for the recovered input recorder."""
from pathlib import Path
import json
import os
import re

root = Path(__file__).resolve().parents[1]
os.environ["DECOMPNET_TOOLCHAINS"] = str(root / ".toolchains")
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call

net = RomNetwork(root / "decomp/.decomp/decomp.db", root / "decomp")
slug = json.loads((root / "decomp/project.json").read_text())["slug"]
agent, _ = net.local_agent("codex-session")
base = (root / "recovered/input_record.c").read_text()
variants = {
    "register pointer": base.replace("InputState *input =", "register InputState *input ="),
    "volatile count field": base.replace("u16 record_count;", "volatile u16 record_count;"),
    "volatile global": base.replace("extern InputState D_", "extern volatile InputState D_").replace("InputState *input =", "volatile InputState *input ="),
    "signed limit local": base.replace("u8 packed;", "u8 packed;\n    int limit = 0xE0E;").replace("(u32)D_030042F0.record_count <= 0xE0EU", "input->record_count <= limit"),
    "unsigned limit local": base.replace("u8 packed;", "u8 packed;\n    u32 limit = 0xE0E;").replace("(u32)D_030042F0.record_count <= 0xE0EU", "input->record_count <= limit"),
    "pointer in comparison": base.replace("InputState *input = &D_030042F0;", "InputState *input;").replace("(u32)D_030042F0.record_count", "(u32)(input = &D_030042F0)->record_count"),
    "direct global accesses": base.replace("input->record_buffer[input->record_count++]", "D_030042F0.record_buffer[D_030042F0.record_count++]"),
    "volatile comparison": base.replace("(u32)D_030042F0.record_count", "(u32)((volatile InputState *)&D_030042F0)->record_count"),
    "late pointer": base.replace("InputState *input = &D_030042F0;", "InputState *input;").replace("        packed = 0;", "        input = &D_030042F0;\n        packed = 0;"),
    "const limit first": base.replace("InputState *input = &D_030042F0;", "u32 limit = 0xE0E;\n    InputState *input = &D_030042F0;").replace("(u32)D_030042F0.record_count <= 0xE0EU", "input->record_count <= limit"),
}
records = []
for label, source in variants.items():
    result = call(net, agent, slug, "compile_and_diff", {"function": "sub_08003150", "source": source})
    summary = result.splitlines()[0]
    print(label + ": " + summary, flush=True)
    records.append({"variant": label, "result": result})
    fn = net._function("sub_08003150", slug)
    if fn["status"] == "matched":
        break
fn = net._function("sub_08003150", slug)
best = fn["matched_source"] or fn["best_source"]
if best:
    (root / "recovered/input_record.c").write_text(best + "\n")
(root / "analysis/input-variants.json").write_text(json.dumps(records, indent=2) + "\n")
print("Saved best candidate. Status: " + fn["status"], flush=True)
