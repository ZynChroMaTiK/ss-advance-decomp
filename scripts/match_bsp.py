"""Bounded byte-matching experiments for original BSP side classification."""
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
base = (root / "recovered/bsp_side.c").read_text()
variants = {
    "C89 declare y first": base.replace("s32 x=point[1], y=point[3];\n    s32 dx=delta[0], dy=delta[1];\n    s32 ox=origin[0], oy=origin[1];\n    u32 a,b;", "s32 y,x,dx,dy,ox,oy; u32 a,b;\n    x=point[1]; y=point[3]; dx=delta[0]; dy=delta[1]; ox=origin[0]; oy=origin[1];"),
    "C89 declare dx first": base.replace("s32 x=point[1], y=point[3];\n    s32 dx=delta[0], dy=delta[1];\n    s32 ox=origin[0], oy=origin[1];\n    u32 a,b;", "s32 dx,dy,x,y,ox,oy; u32 a,b;\n    x=point[1]; y=point[3]; dx=delta[0]; dy=delta[1]; ox=origin[0]; oy=origin[1];"),
    "destructive products": base.replace("    if (y*dx>=dy*x)", "    dy*=x; y*=dx;\n    if (y>=dy)"),
    "multiply delta first": base.replace("    if (y*dx>=dy*x)", "    dy*=x; dx*=y;\n    if (dx>=dy)"),
    "reuse offset locals": base.replace("    if (y*dx>=dy*x)", "    ox=dy*x; oy=y*dx;\n    if (oy>=ox)"),
    "signed sign locals": base.replace("u32 a,b;", "s32 a,b;"),
    "two dimensional coordinates": base.replace("s32 x=point[1], y=point[3];", "s32 x=((s32 *)point)[0]>>16, y=((s32 *)point)[1]>>16;"),
}
records=[]
for label, source in variants.items():
    result=call(net,agent,slug,"compile_and_diff",{"function":"sub_0800B1B0","source":source})
    print(label+": "+result, flush=True)
    records.append({"label":label,"result":result})
    fn=net._function("sub_0800B1B0",slug)
    if fn["status"]=="matched": break
best=fn["matched_source"] or fn["best_source"]
if best: (root / "recovered/bsp_side.c").write_text(best+"\n")
report=root / "analysis/bsp-match-attempts.json"
previous=json.loads(report.read_text()) if report.exists() else []
report.write_text(json.dumps(previous+records,indent=2)+"\n")
