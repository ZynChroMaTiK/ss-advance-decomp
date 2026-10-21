"""Compile the shared effect for ARM7TDMI; this is not a hardware timing test."""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[1]
output = root / 'build/godrays-arm'
output.mkdir(parents=True, exist_ok=True)
prefix = root / '.toolchains/arm-gnu/bin/arm-none-eabi-'
cc = str(prefix) + 'gcc'
units = ('godrays', 'godrays_reference', 'actor_raster')
for unit in units:
    subprocess.run([cc, '-std=c11', '-O2', '-mcpu=arm7tdmi', '-marm',
                    '-mfloat-abi=soft', '-Wall', '-Wextra', '-Werror',
                    '-I', str(root / 'include'), '-fstack-usage', '-c',
                    str(root / f'src/{unit}.c'), '-o', str(output / f'{unit}.o')], check=True)
objects = [str(output / f'{unit}.o') for unit in units]
symbols = subprocess.check_output([str(prefix)+'nm', '-u', *objects], text=True)
for line in symbols.splitlines():
    if '__aeabi_' in line:
        assert line.split()[-1] in ('__aeabi_ldivmod', '__aeabi_uidivmod', '__aeabi_uldivmod'), line
sizes = subprocess.check_output([str(prefix)+'size', *objects], text=True)
inputs = [root / f'src/{u}.c' for u in units] + [root / 'include/ssa/godrays.h', root / 'include/ssa/godrays_reference.h']
report = {'result': 'pass', 'scope': __doc__,
          'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
          'object_sizes': sizes, 'undefined_symbols': symbols,
          'stack_usage': {u: (output / f'{u}.su').read_text() for u in units},
          'source_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
(root / 'analysis/godrays-arm-validation.json').write_text(json.dumps(report, indent=2)+'\n')
print(sizes, end='')
print('ARM7TDMI compilation passed; integer helpers only, no floating-point dependencies.')
