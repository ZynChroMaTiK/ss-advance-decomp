"""Shared incremental GCC/ASan build for the portable native regression checks.

Keep these checks independent of Windows and ROM execution. CMake/CTest is the
normal host build; this helper retains the existing WSL sanitizer workflow.
"""
from pathlib import Path
import concurrent.futures
import fcntl
import hashlib
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pedantic',
         '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie']


def build_check(test, output):
    cache = ROOT / 'build/native-sanitizer-cache'
    cache.mkdir(parents=True, exist_ok=True)
    sources = sorted((ROOT / 'src').glob('*.c'))
    common = hashlib.sha256(subprocess.check_output(['gcc', '--version']))
    common.update(repr(FLAGS).encode())
    for path in sorted((ROOT / 'include/ssa').glob('*.h')) + sorted((ROOT / 'src').glob('*.inc')):
        common.update(str(path.relative_to(ROOT)).encode())
        common.update(path.read_bytes())

    def compile_source(source):
        digest = common.copy()
        digest.update(source.read_bytes())
        obj = cache / (source.stem + '-' + digest.hexdigest() + '.o')
        if not obj.exists():
            temp = obj.with_suffix('.tmp.o')
            subprocess.run(['gcc', *FLAGS, '-I', str(ROOT / 'include'), '-c', str(source), '-o', str(temp)], check=True)
            temp.replace(obj)
        return obj

    # Separate test invocations share the cache without racing archive writes.
    with (cache / 'build.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
            objects = list(pool.map(compile_source, sources))
        key = hashlib.sha256(''.join(p.name for p in objects).encode()).hexdigest()
        library = cache / ('native-' + key + '.a')
        if not library.exists():
            temp = library.with_suffix('.tmp.a')
            # A fresh archive cannot retain removed translation units.
            temp.unlink(missing_ok=True)
            subprocess.run(['ar', 'rcs', str(temp), *map(str, objects)], check=True)
            temp.replace(library)
    subprocess.run(['gcc', *FLAGS, '-I', str(ROOT / 'include'), str(test),
                    str(library), '-lm', '-o', str(output)], check=True)
