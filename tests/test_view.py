"""Behavior tests for the new H-toggle chase camera; no emulation runtime."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build/native-tests"
BUILD.mkdir(parents=True,exist_ok=True)
sources=["view.c","geometry.c","physics.c","movement.c","map_assets.c"]
# Use the same non-PIE sanitizer setup as the other native integration tests
# to avoid intermittent ASan startup stalls in local WSL.
subprocess.run(["gcc","-std=c11","-O2","-Wall","-Wextra","-Werror","-pedantic",
    "-fsanitize=address,undefined","-fno-sanitize-recover=all","-no-pie","-I",str(ROOT/"include"),
    str(ROOT/"tests/test_view.c"),*[str(ROOT/"src"/s) for s in sources],"-o",str(BUILD/"test_view")],check=True)
rom=ROOT/"orig/SSA.gba"
sha1=hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1=="4ff48d8c18b70a5184758f1917343ae85a70858e"
# The local WSL run repeatedly stalled with LeakSanitizer enabled; disabling
# only leak scanning completed the identical tests. Keep ASan/UBSan active.
# An explicit ASAN_OPTIONS from the caller can opt back into leak scanning.
test_env=os.environ.copy()
test_env.setdefault("ASAN_OPTIONS","detect_leaks=0")
result=subprocess.run([str(BUILD/"test_view"),str(rom)],text=True,capture_output=True,timeout=60,env=test_env)
if result.returncode:
    print(result.stdout,end=""); print(result.stderr,end="")
    result.check_returncode()
report=json.loads(result.stdout)
report.update(rom_sha1=sha1,scope="Native new-feature tests: H key routing, chase pose, static obstruction and jump following. No renderer/OS event loop or avatar rendering.",
    sanitizer_options=test_env["ASAN_OPTIONS"],
    source_sha256={s:hashlib.sha256((ROOT/"src"/s).read_bytes()).hexdigest() for s in sources})
(ROOT/"analysis/native-view-validation.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))
