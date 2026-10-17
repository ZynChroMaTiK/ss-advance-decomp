"""Locate assembly labels whose emitted offsets differ from ROM addresses."""
from pathlib import Path
import json
import re
from elftools.elf.elffile import ELFFile
root = Path(__file__).resolve().parents[1]
slug = json.loads((root / "decomp/project.json").read_text())["slug"]
source = root / "decomp" / slug / "source"
pieces = json.loads((source / "asm/pieces.json").read_text())
issues = []
for piece in pieces:
    obj = source / "build" / Path(piece["src"]).with_suffix(".o")
    with obj.open("rb") as f:
        elf = ELFFile(f)
        section = elf.get_section_by_name(".text")
        syms = elf.get_section_by_name(".symtab")
        wrong = []
        for sym in syms.iter_symbols():
            match = re.fullmatch(r"(?:sub_|_)(08[0-9A-F]{6})", sym.name)
            if match and isinstance(sym["st_shndx"], int):
                actual = piece["addr"] + (sym["st_value"] & ~1)
                expected = int(match.group(1), 16)
                if actual != expected:
                    wrong.append({"symbol": sym.name, "expected": hex(expected), "actual": hex(actual)})
        if wrong:
            issues.append({"piece": piece["src"], "size": section["sh_size"], "moved_labels": wrong})
(root / "analysis/assembly-layout.json").write_text(json.dumps(issues, indent=2) + "\n")
for issue in issues:
    print(issue["piece"], issue["size"], len(issue["moved_labels"]), issue["moved_labels"][:4])
