"""Bounded decomp.wtf experiments for the original entity contact callback."""
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
name = "sub_08003D7C"
if len(sys.argv) > 1 and sys.argv[1] in ("variants", "refine"):
    import itertools
    base = (root / "recovered/entity_contact.c").read_text()
    declarations = "    ContactEntity *current;\n    u32 radius;\n    s32 distance;"
    lines = declarations.splitlines()
    variants = [("declaration order " + str(i), base.replace(declarations, "\n".join(order)))
                for i, order in enumerate(itertools.permutations(lines)) if list(order) != lines]
    variants += [
        ("scope current after callback", base.replace("    ContactEntity *current;\n", "").replace("    current = D_03004330;", "    { ContactEntity *current = D_03004330;").replace("    return 0;\n}", "    return 0;\n    }\n}")),
        ("explicit flags local", base.replace("    u32 radius;", "    u32 radius;\n    u32 flags = other->flags;").replace("other->flags &", "flags &")),
        ("explicit initial current", base.replace("    ContactEntity *current;", "    ContactEntity *current = D_03004330;").replace("other == D_03004330", "other == current").replace("D_03004330->flags & 0x10", "current->flags & 0x10")),
        ("volatile query pointer", base.replace("*D_03004330, *D_03004328", "*volatile D_03004330, *D_03004328")),
        ("invert owner comparison", base.replace("D_03004330->player_index - 1 == other->player_index", "other->player_index == D_03004330->player_index - 1")),
        ("nested player skip", base.replace("if ((other->flags & 0x10) && (D_03004330->flags & 0x10)) return 1;", "if (other->flags & 0x10) { if (D_03004330->flags & 0x10) return 1; }")),
    ]
    if sys.argv[1] == "refine":
        initial_only = base.replace("(current->flags & 0x100)", "(D_03004330->flags & 0x100)")
        separate = initial_only.replace("ContactEntity *current = D_03004330;", "ContactEntity *initial = D_03004330;\n    ContactEntity *current;").replace("other == current", "other == initial").replace("(current->flags & 0x10)", "(initial->flags & 0x10)")
        variants = [
            ("initial pointer only for first player test", initial_only),
            ("distinct initial and later pointer", separate),
            ("explicit flags local corrected", base.replace("    s32 distance;", "    s32 distance;\n    u32 flags;").replace("    if (other == current) return 1;", "    if (other == current) return 1;\n    flags = other->flags;").replace("other->flags &", "flags &")),
            ("owner uses refreshed pointer", base.replace("    if ((other->flags & 0x10) && (current->flags & 0x100)", "    current = D_03004330;\n    if ((other->flags & 0x10) && (current->flags & 0x100)").replace("D_03004330->player_index", "current->player_index")),
            ("all checks use current", base.replace("D_03004330->", "current->")),
            ("initial pointer plus volatile flags", initial_only.replace("    u16 flags;", "    volatile u16 flags;")),
            ("separate pointer plus initial flags reuse", separate.replace("    s32 distance;", "    s32 distance;\n    u32 flags;").replace("    if (other == initial) return 1;", "    if (other == initial) return 1;\n    flags = other->flags;").replace("other->flags &", "flags &")),
            ("separate pointer owner refreshed", separate.replace("    if ((other->flags & 0x10) && (D_03004330->flags & 0x100)", "    initial = D_03004330;\n    if ((other->flags & 0x10) && (initial->flags & 0x100)").replace("D_03004330->player_index", "initial->player_index")),
        ]
    path = root / "analysis/entity-match-attempts.json"
    records = json.loads(path.read_text()) if path.exists() else []
    for label, source in variants:
        result = call(net, agent, slug, "compile_and_diff", {"function": name, "source": source})
        print(label + ": " + result.splitlines()[0], flush=True)
        records.append({"trial": len(records) + 1, "label": label, "source": source, "result": result})
        path.write_text(json.dumps(records, indent=2) + "\n")
        fn = net._function(name, slug)
        if fn["status"] == "matched": break
    best = fn["matched_source"] or fn["best_source"]
    if best: (root / "recovered/entity_contact.c").write_text(best + "\n")
elif len(sys.argv) > 1:
    command = sys.argv[1]
    args = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    if command in ("workflow", "agbcc", "arm"):
        args = {"topic": command}
        command = "guide"
    elif command in ("claim_function", "get_function"):
        args = {"function": name}
    elif command == "discover":
        command = "split_function"
        args = {"function": "sub_08003B98", "at": "0x08003D7C", "new_name": name}
    elif command == "release_function":
        args = {"function": name, "notes": "Recovered full callback and added validated 224-byte range through supported split_function gap discovery. 20 matching compiles (one compile-error variant retained in evidence), best score 880 / 92.0%, 111 target and candidate instructions; not exact. Remaining mainly register allocation/load scheduling in first player/filter checks and owner comparison. Best has distinct initial/current pointers plus explicit u32 other flags. Original type E1 calls 0801B99C(current,other,0) BEFORE distance checks; 0x200 check uses current.x as vertical position (preserved original quirk). Radius addition and absolute differences wrap to32bit then unsigned comparisons. Source recovered/entity_contact.c; full trials analysis/entity-match-attempts.json. Native collision oracle work proceeding separately."}
    print(call(net, agent, slug, command, args))
else:
    source = (root / "recovered/entity_contact.c").read_text()
    result = call(net, agent, slug, "compile_and_diff", {"function": name, "source": source})
    print(result)
    path = root / "analysis/entity-match-attempts.json"
    records = json.loads(path.read_text()) if path.exists() else []
    records.append({"trial": len(records) + 1, "source": source, "result": result})
    path.write_text(json.dumps(records, indent=2) + "\n")
