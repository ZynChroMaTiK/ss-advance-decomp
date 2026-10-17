"""Produce local ROM metadata and conservative porting leads, never alter ROM."""
from pathlib import Path
import hashlib
import json
import re
import struct

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_LITTLE_ENDIAN
from decompnet.rom.gba import read_info

root = Path(__file__).resolve().parents[1]
data = (root / "orig/SSA.gba").read_bytes()
out = root / "analysis"
out.mkdir(exist_ok=True)
info = read_info(data)
info["sha256"] = hashlib.sha256(data).hexdigest()
info["compiler_guess"] = "unverified; decomp.wtf defaults all GBA ROMs to agbcc"
(out / "rom.json").write_text(json.dumps(info, indent=2) + "\n")
md = Cs(CS_ARCH_ARM, CS_MODE_ARM | CS_MODE_LITTLE_ENDIAN)
entry = 0x08000008 + ((struct.unpack_from("<I", data)[0] & 0xffffff) << 2)
asm = [f"{i.address:08x}: {i.mnemonic:8} {i.op_str}"
       for i in md.disasm(data[entry - 0x08000000:0xF4], entry)]
(out / "startup.txt").write_text("\n".join(asm) + "\n")
registers = {0x04000000: "DISPCNT", 0x04000130: "KEYINPUT", 0x04000132: "KEYCNT",
             0x040000B0: "DMA0", 0x040000D4: "DMA3", 0x04000200: "IE",
             0x040000A0: "FIFO_A", 0x040000A4: "FIFO_B", 0x05000000: "palette",
             0x06000000: "VRAM", 0x07000000: "OAM"}
leads = []
for off in range(0, len(data) - 3, 4):
    word = struct.unpack_from("<I", data, off)[0]
    if word in registers:
        leads.append({"rom_address": f"0x{0x08000000 + off:08X}",
                      "value": f"0x{word:08X}", "candidate": registers[word]})
(out / "hardware-literal-candidates.json").write_text(json.dumps(leads, indent=2) + "\n")
strings = []
for match in re.finditer(rb"[\x20-\x7e]{8,}\x00", data):
    s = match.group()[:-1].decode("ascii")
    if re.search(r"[A-Za-z]{4}", s):
        strings.append(f"0x{0x08000000 + match.start():08X}\t{s}")
(out / "strings.tsv").write_text("\n".join(strings) + "\n")
print(json.dumps(info, indent=2))
print(f"{len(leads)} hardware literal candidates; {len(strings)} printable strings")
print("These are analysis leads, not verified code references or recovered systems.")
