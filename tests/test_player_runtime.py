"""Native composition tests; no emulator and no OS input/rendering backend."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
sources = ["player_runtime.c", "player_frame.c", "player_spawn.c", "player_motion.c",
           "spawn.c", "world.c", "slide.c", "horizontal.c", "view.c", "geometry.c",
           "physics.c", "movement.c", "map_assets.c", "inventory.c", "pickup.c"]
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie", "-I", str(ROOT / "include"),
                str(ROOT / "tests/test_player_runtime.c"),
                *(str(ROOT / "src" / source) for source in sources),
                "-o", str(BUILD / "test_player_runtime")], check=True)
rom = ROOT / "orig/SSA.gba"
sha1 = hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1 == "4ff48d8c18b70a5184758f1917343ae85a70858e"
env = os.environ.copy()
# Same WSL LeakSanitizer workaround as test-view; ASan and UBSan remain enabled.
env.setdefault("ASAN_OPTIONS", "detect_leaks=0")
try:
    result = subprocess.run([str(BUILD / "test_player_runtime"), str(rom)], text=True,
                            capture_output=True, timeout=20, env=env)
except subprocess.TimeoutExpired as error:
    print((error.stderr or b"")[:4000])
    raise
if result.returncode:
    print(result.stdout, end=""); print(result.stderr, end="")
    result.check_returncode()
report = json.loads(result.stdout)
report.update(rom_sha1=sha1, sanitizer_options=env["ASAN_OPTIONS"], sanitizer_binary_pie=False,
              scope="Native real-map player spawn/frame/movement/camera/jump/H-view/pickup integration; controlled external death/hazard/script/animation/audio boundaries. No OS input or rendered game.",
              source_sha256={source: hashlib.sha256((ROOT / "src" / source).read_bytes()).hexdigest() for source in sources})
(ROOT / "analysis/native-player-runtime-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
