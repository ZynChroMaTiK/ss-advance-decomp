"""Inspect the AENP resource table and GBA LZ77 map headers (no extraction)."""
from pathlib import Path
import struct

ROM = (Path(__file__).resolve().parents[1] / "orig/SSA.gba").read_bytes()

def unpack_lz77(data):
    if data[0] != 0x10:
        raise ValueError("not GBA LZ77")
    length = int.from_bytes(data[1:4], "little")
    output = bytearray()
    pos = 4
    while len(output) < length:
        flags = data[pos]
        pos += 1
        for bit in range(7, -1, -1):
            if len(output) == length:
                break
            if flags & (1 << bit):
                pair = (data[pos] << 8) | data[pos + 1]
                pos += 2
                distance = (pair & 4095) + 1
                count = (pair >> 12) + 3
                if distance > len(output) or len(output) + count > length:
                    raise ValueError("invalid back reference")
                for _ in range(count):
                    output.append(output[-distance])
            else:
                output.append(data[pos])
                pos += 1
    return bytes(output), pos

if __name__ == "__main__":
    for index in range(24):
        address = struct.unpack_from("<I", ROM, 0x44aef4 + index * 4)[0]
        off = address - 0x08000000
        print(index, hex(address), ROM[off:off+12].hex())
        if 0 <= off < len(ROM) and ROM[off] == 0x10:
            data, size = unpack_lz77(ROM[off:])
            print("  sizes", len(data), size, "counts", struct.unpack_from("<12H", data),
                  "offsets", [hex(x) for x in struct.unpack_from("<12I", data, 0x18)])
