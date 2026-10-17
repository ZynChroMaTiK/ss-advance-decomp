"""Regression checks for actual Thumb stack instruction sequences."""
import struct
from luvdis.analyze import CPUState, State
from luvdis.disasm import disasm


class Instructions:
    def __init__(self, *halfwords):
        self.code = struct.pack("<" + "H" * len(halfwords), *halfwords)

    def dist(self, address):
        return disasm(self.code, address)


# SUB SP, #128; PUSH {LR}; POP {PC}: a frame larger than 16 tracked slots.
state = CPUState()
state.emulate(Instructions(0xB0A0, 0xB500, 0xBD00), 0x08000100)
assert state[15] == state.return_addr
assert state.sp == 32
assert len(state.stack) >= 33

# ADD SP, #128; PUSH {LR}; POP {PC}: an unmatched epilogue in speculative code.
# It must not index Python's list negatively, crash, or invent a return address.
state = CPUState()
state.emulate(Instructions(0xB020, 0xB500, 0xBD00), 0x08000100)
assert state[15] is state.unknown
assert state.sp == -32

# An ordinary PUSH/POP must still recover LR.
state = CPUState()
state.emulate(Instructions(0xB500, 0xBD00), 0x08000100)
assert state[15] == state.return_addr
assert state.sp == 0

# A standalone BL suffix is unsupported by Luvdis's symbolic analyzer.
# It must be rejected as unknown control flow, rather than reading a missing rd.
exited, total, _, _, ranges = State().analyze_func(Instructions(0xF800), 0x08000100)
assert (exited, total) == (0, 1)
assert ranges == [(0x08000100, 0x08000100, 1)]
print("PASS: large frame, negative stack, ordinary return, partial BL")
