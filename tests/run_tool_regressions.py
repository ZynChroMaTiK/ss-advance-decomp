"""Fetch upstream's checksum-pinned public zlib fixture and run GBA checks."""
from pathlib import Path
import importlib.util
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
checkout = root / "tools/decomp-wtf"
os.environ["DECOMPNET_TOOLCHAINS"] = str(root / ".toolchains")
spec = importlib.util.spec_from_file_location("zlib_fixture", checkout / "projects/zlib-n64/prepare.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.fetch()
result = subprocess.run([sys.executable, "-m", "pytest", "tests/test_gba.py", "-q", "--tb=short"],
                        cwd=checkout, capture_output=True, text=True)
log = result.stdout + result.stderr
(root / "analysis/tool-regressions.txt").write_text(log)
print(log)
raise SystemExit(result.returncode)
