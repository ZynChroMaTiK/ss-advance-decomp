"""Read-only disassembly of the supplied ROM's channel/mixer paths."""
from pathlib import Path
import sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_ARM
rom=(Path(__file__).resolve().parents[1]/'orig/SSA.gba').read_bytes()
d=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
for arg in sys.argv[1:]:
    d=Cs(CS_ARCH_ARM,CS_MODE_ARM if arg.startswith('arm:') else CS_MODE_THUMB)
    arg=arg.removeprefix('arm:')
    start,end=(int(v,16) for v in arg.split(':'))
    for ins in d.disasm(rom[start:end],0x08000000+start):
        print(f'{ins.address:08x}: {ins.mnemonic:7} {ins.op_str}')
