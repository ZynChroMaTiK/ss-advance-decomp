"""Bounded matching experiments for recovered original vertical physics."""
from pathlib import Path
import json
import os
root = Path(__file__).resolve().parents[1]
os.environ["DECOMPNET_TOOLCHAINS"] = str(root / ".toolchains")
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net = RomNetwork(root / "decomp/.decomp/decomp.db", root / "decomp")
slug = json.loads((root / "decomp/project.json").read_text())["slug"]
agent,_ = net.local_agent("codex-session")
base = (root / "recovered/vertical_step.c").read_text()
variants = {
    "volatile flags":base.replace("u16 flags;", "volatile u16 flags;"),
    "volatile entity":base.replace("(PhysicsEntity *body)","(volatile PhysicsEntity *body)"),
    "volatile player":base.replace("PhysicsPlayer *player", "volatile PhysicsPlayer *player"),
    "volatile heights":base.replace("s16 ceiling, floor;", "volatile s16 ceiling, floor;"),
    "volatile entity/player":base.replace("(PhysicsEntity *body)","(volatile PhysicsEntity *body)").replace("PhysicsPlayer *player", "volatile PhysicsPlayer *player"),
    "volatile flags/heights":base.replace("u16 flags;", "volatile u16 flags;").replace("s16 ceiling, floor;", "volatile s16 ceiling, floor;"),
    "load tick before velocity":base.replace("s32 old_velocity;", "s32 old_velocity;\n    u32 ticks;").replace("    old_velocity = body->vz;", "    ticks = D_03004010->ticks;\n    old_velocity = body->vz;").replace("old_velocity * D_03004010->ticks", "ticks * old_velocity"),
}
records=[]
for label,source in variants.items():
    result=call(net,agent,slug,"compile_and_diff",{"function":"sub_08004CE0","source":source})
    print(label+": "+result.splitlines()[0],flush=True)
    records.append({"label":label,"result":result})
    fn=net._function("sub_08004CE0",slug)
    if fn["status"]=="matched": break
best=fn["matched_source"] or fn["best_source"]
if best: (root / "recovered/vertical_step.c").write_text(best+"\n")
(root / "analysis/vertical-match-attempts.json").write_text(json.dumps(records,indent=2)+"\n")
