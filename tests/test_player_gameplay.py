"""Native lifecycle/hazard runtime composition, with real map geometry."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/native-tests"
BUILD.mkdir(parents=True, exist_ok=True)
sources = ["player_gameplay.c", "player_lifecycle.c", "player_hazards.c",
           "respawn_select.c", "player_runtime.c", "player_frame.c", "player_spawn.c",
           "player_motion.c", "spawn.c", "world.c", "slide.c", "horizontal.c",
           "view.c", "geometry.c", "physics.c", "movement.c", "map_assets.c",
           "inventory.c", "weapon_select.c"]
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
                "-I", str(ROOT / "include"), str(ROOT / "tests/test_player_gameplay.c"),
                *(str(ROOT / "src" / source) for source in sources),
                "-o", str(BUILD / "test_player_gameplay")], check=True)
rom = ROOT / "orig/SSA.gba"
sha1 = hashlib.sha1(rom.read_bytes()).hexdigest()
assert sha1 == "4ff48d8c18b70a5184758f1917343ae85a70858e"
env = os.environ.copy()
env.setdefault("ASAN_OPTIONS", "detect_leaks=0")
result = subprocess.run([str(BUILD / "test_player_gameplay"), str(rom)], text=True,
                        capture_output=True, timeout=30, env=env)
if result.returncode:
    print(result.stdout, end="")
    print(result.stderr, end="")
    result.check_returncode()
report = json.loads(result.stdout)
report.update(rom_sha1=sha1, sanitizer_options=env["ASAN_OPTIONS"], sanitizer_binary_pie=False,
              scope="Native death/respawn/selector/hazard stages connected to real-map movement, jump and H-view. Presentation, line scripts and sector effects are callback boundaries; no OS input/rendering.",
              source_sha256={source: hashlib.sha256((ROOT / "src" / source).read_bytes()).hexdigest() for source in sources})
(ROOT / "analysis/native-player-gameplay-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
