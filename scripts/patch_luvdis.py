"""Apply two bounded-stack fixes to the pinned Luvdis analyzer, idempotently.

SSA reaches code paths that adjust SP above the analyzer's entry stack. Such
slots are unknown, not Python negative indices. Large stack frames must grow
the tracked stack rather than leaving a gap before the next PUSH.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / ".toolchains/luvdis/venv/lib/python3.12/site-packages/luvdis/analyze.py"
fixes = [
    # Writing a line at a time across WSL's Windows mount is very slow.
    ("open(path, 'w', buffering=1)", "open(path, 'w', buffering=1024 * 1024)"),
    ("range(len(self.stack)-self.sp)", "range(max(0, self.sp-len(self.stack)))"),
    ("                        else:\n                            self.stack[self.sp] = value",
     "                        elif self.sp >= 0:\n                            self.stack[self.sp] = value"),
    # A partial BL has no rd; do not treat it as an ADD/MOV-to-PC.
    # This analyzer does not model LR-relative calls, so reject that speculative
    # path rather than inventing a call target or crashing during discovery.
    ("            elif ins.id == Opcode.ill:\n                break",
     "            elif ins.id in (Opcode.ill, Opcode.pbl):\n                break"),
    ("if ins is None or ins.id == Opcode.ill:",
     "if ins is None or ins.id in (Opcode.ill, Opcode.pbl):"),
]
text = path.read_text()
for old, new in fixes:
    if old in text:
        if text.count(old) != 1:
            raise SystemExit("Unexpected Luvdis source: ambiguous patch")
        text = text.replace(old, new)
    elif new not in text:
        raise SystemExit("Unexpected Luvdis version: patch does not apply")
path.write_text(text)
print("Luvdis stack bounds and output buffering fixes applied")
