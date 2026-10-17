"""Read campaign entity/script requirements from the user's unchanged ROM."""
from collections import Counter
import hashlib
import json
import struct
from inspect_maps import ROM, unpack_lz77

assert hashlib.sha1(ROM).hexdigest() == '4ff48d8c18b70a5184758f1917343ae85a70858e'


def u32(offset):
    return struct.unpack_from('<I', ROM, offset)[0]


def definition(index):
    return struct.unpack_from('<4H', ROM, 0x44a3d4 + index * 8)


for level in range(12):
    raw, _ = unpack_lz77(ROM[u32(0x44aef4 + level * 4) - 0x08000000:])
    count = struct.unpack_from('<H', raw, 6)[0]
    things = struct.unpack_from('<I', raw, 0x1c)[0]
    initial = Counter()
    exits = []
    for index in range(count):
        x, y, heading, kind, tag, flags = struct.unpack_from('<hh4H', raw, things + index * 12)
        entity, variant, animation, period = definition(kind)
        initial[f'{entity:02X}:{animation:02X}'] += 1
        if entity == 0xe1 and animation == 0x3f:
            exits.append([x, y])
    table = u32(0x45280c + level * 4) - 0x08000000
    end = u32(0x45280c + (level + 1) * 4) - 0x08000000
    spawned = Counter()
    for entry in range(table, end, 4):
        pc = u32(entry) - 0x08000000
        for _ in range(256):
            op, address, group, unused, timer = struct.unpack_from('<5I', ROM, pc)
            if op == 1:
                offset = address - 0x08000000
                for _ in range(256):
                    x, y, kind, heading = struct.unpack_from('<hh2H', ROM, offset)
                    if not (x or y):
                        break
                    entity, variant, animation, period = definition(kind)
                    spawned[f'{entity:02X}'] += 1
                    offset += 8
                else:
                    raise ValueError('unterminated spawn list')
            if op in (9, 10):
                break
            pc += 20
        else:
            raise ValueError('unterminated script')
    print(json.dumps(dict(level=level, initial=initial, scripted=spawned, exits=exits)))
