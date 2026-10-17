"""Bounded real decomp.wtf timer matching experiments; preserve all trial evidence."""
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
agent, _ = net.local_agent("codex-session")
name = "sub_08012CFC"
base = (root / "recovered/entity_timer.c").read_text()
path = root / "analysis/entity-timer-match-attempts.json"
records = json.loads(path.read_text()) if path.exists() else []
variants = [
    ("unsigned timer signed cast", base.replace("s16 timer;", "u16 timer;").replace("if (entity->timer <= 0)", "if ((s16)entity->timer <= 0)")),
    ("explicit masked flag", base.replace("    if (!(0x80 & entity->flags))", "    u16 masked = 0x80 & entity->flags;\n    if (!masked)").replace("entity->timer = 0;", "entity->timer = masked;")),
    ("explicit timestep", base.replace("    if (!(0x80", "    u32 ticks;\n    if (!(0x80").replace("        entity->timer = entity->timer - D_03004010->ticks;", "        ticks = D_03004010->ticks;\n        entity->timer = entity->timer - ticks;")),
    ("unsigned return", base.replace("int sub_", "u32 sub_")),
    ("signed half return", base.replace("int sub_", "s16 sub_")),
    ("volatile fields", base.replace("u16 flags;", "volatile u16 flags;").replace("s16 timer;", "volatile s16 timer;")),
    ("volatile timer", base.replace("s16 timer;", "volatile s16 timer;")),
    ("single return", base.replace("    if (!(0x80", "    int result = 0;\n    if (!(0x80").replace("return 1;", "result = 1;").replace("return 0;", "return result;")),
    ("masked unsigned timer", base.replace("s16 timer;", "u16 timer;").replace("    if (!(0x80 & entity->flags))", "    u16 masked = 0x80 & entity->flags;\n    if (!masked)").replace("if (entity->timer <= 0)", "if ((s16)entity->timer <= 0)").replace("entity->timer = 0;", "entity->timer = masked;")),
    ("explicit narrow subtraction temporary", base.replace("    if (!(0x80", "    s16 remaining;\n    if (!(0x80").replace("        entity->timer = entity->timer - D_03004010->ticks;", "        remaining = entity->timer - D_03004010->ticks;\n        entity->timer = remaining;").replace("if (entity->timer <= 0)", "if (remaining <= 0)")),
]
if len(sys.argv) > 1:
    original = base
    variants = [
        ("flat branches shared return", base.replace("    } else {", "        goto done;\n    }\n    {").replace("    return 0;", "done:\n    return 0;")),
        ("single unsigned half return", base.replace("    if (!(entity", "    u16 result = 0;\n    if (!(entity").replace("return 1;", "result = 1;").replace("return 0;", "return result;")),
        ("unsigned byte return", base.replace("int sub_", "u8 sub_")),
        ("pointer argument r1", base.replace("TimerEntity *entity) {", "TimerEntity *input) {\n    register TimerEntity *entity asm(\"r1\") = input;")),
        ("u16 timer pointer arithmetic", base.replace("    s16 timer;", "    u16 timer;").replace("if (entity->timer <= 0)", "if ((s32)(entity->timer << 16) <= 0)")),
        ("void pointer overlay", base.replace("TimerEntity *entity) {", "void *input) {\n    TimerEntity *entity = input;")),
        ("explicit masked flags and unsigned byte result", base.replace("int sub_", "u8 sub_").replace("    if (!(entity->flags & 0x80))", "    u16 masked = entity->flags & 0x80;\n    if (masked == 0)").replace("entity->timer = 0;", "entity->timer = masked;")),
    ]
for label, source in variants:
    result = call(net, agent, slug, "compile_and_diff", {"function": name, "source": source})
    records.append({"label": label, "source": source, "result": result})
    print(label + ": " + result.splitlines()[0], flush=True)
    (root / "analysis/entity-timer-match-attempts.json").write_text(json.dumps(records, indent=2) + "\n")
    fn = net._function(name, slug)
    if fn["status"] == "matched": break
best = fn["matched_source"] or fn["best_source"]
if best: (root / "recovered/entity_timer.c").write_text(best + "\n")
print(call(net, agent, slug, "get_function", {"function":name}))
