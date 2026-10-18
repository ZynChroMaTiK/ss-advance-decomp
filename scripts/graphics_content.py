"""Content labels for the gallery, checked against the original PNGs.

Actor resource numbers identify animations, not necessarily a single subject:
32..63 share the entire pickup table; 96..98 share the piano/anvil/safe table.
The second half of those tables holds smaller versions of the same pictures.
Enemy names also follow enemy_control.c, enemy_ranged.c and enemy_boss.c.
These labels describe pictures, not claims that every frame is used in gameplay.
"""
import hashlib
import struct
import zlib


def pixel_digest(raw):
    """Hash visible RGBA, ignoring unused palette slots and transparent RGB.

    The verified exporter writes only non-interlaced, 8-bit indexed PNGs with
    unfiltered rows. Reject any other format instead of silently misreading it.
    Original PNG bytes are retained for delivery; this never re-encodes artwork.
    """
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8; chunks = {}; compressed = bytearray()
    while pos < len(raw):
        size, kind = struct.unpack_from('>I4s', raw, pos)
        payload = raw[pos+8:pos+8+size]
        assert len(payload) == size
        assert zlib.crc32(kind+payload) & 0xffffffff == struct.unpack_from('>I',raw,pos+8+size)[0]
        if kind == b'IDAT': compressed.extend(payload)
        else: chunks[kind] = payload
        pos += size+12
    width,height,depth,colour,compression,filtering,interlace = struct.unpack('>IIBBBBB',chunks[b'IHDR'])
    assert (depth,colour,compression,filtering,interlace) == (8,3,0,0,0)
    palette = chunks[b'PLTE']; alpha = chunks.get(b'tRNS',b'')
    rgba = []
    for i in range(len(palette)//3):
        a = alpha[i] if i < len(alpha) else 255
        rgba.append(palette[i*3:i*3+3]+bytes([a]) if a else b'\0'*4)
    rows = zlib.decompress(compressed)
    assert len(rows) == height*(width+1)
    digest = hashlib.sha256(struct.pack('>II',width,height))
    for y in range(height):
        offset = y*(width+1); assert rows[offset] == 0
        digest.update(b''.join(rgba[i] for i in rows[offset+1:offset+1+width]))
    return digest.hexdigest()


def is_sheet(path):
    return path.endswith(('-sheet.png','-tile-atlas.png','-unmapped-tiles.png'))


ACTOR_GROUPS = [
    ('Cyclops', [4,5,30,67]), ('Methug commander', [6,17,18]),
    ('Methug soldier', [7,19,20]), ('Kamikaze', [8]),
    ('Bladder beast', [9,21,22,137]), ('Werebull', [10,23,24]),
    ('Gunrilla', [25,26,27,141]), ('Hermit Crabuloid', [64,65,66,138,139,140]),
    ('Arachnoid', [69,70,71,72]), ('Reptiloid', [73,74,75,76]),
    ('Wolfiator', list(range(77,83))), ('Sirian Sphinx', list(range(83,89))),
    ('Sam', list(range(101,106))), ('Sam’s blue shirt', list(range(106,111))),
    ('Sam’s green shirt', list(range(111,116))), ('Sam’s orange shirt', list(range(116,121))),
    ('Sam’s white shirt', list(range(121,126))),
    ('Blood and hit effects', [12,100]), ('Purple energy effects', [13,15]),
    ('Smoke and debris', [14,99]), ('Explosions', [16,28,31,68]),
    ('Green splatter', [29]), ('Blue energy effects', [89,92,93,94,131,132]),
    ('Cannonball', [90]), ('Red spark', [95]), ('Fireballs and flames', list(range(126,131))),
    ('Green energy effects', [133,134,135,136]),
    ('Unused white marker', [11]), ('Question mark', [91]),
]
ACTORS = {r:(f'actor-{resources[0]:03}',title) for title,resources in ACTOR_GROUPS for r in resources}
PICKUPS = [
    'Pistol', 'Shotgun', 'Tommygun', 'Unused green weapon', 'Rocket launcher',
    'Minigun', 'Cannon', 'Prion gun', 'Timestrike', 'U.M.T',
    '25 armour', '50 armour', '100 armour', '25 health', '50 health', '100 health',
    'Backpack', 'Pistol bullets', 'Shotgun cartridges', 'U.M.T yellow cell',
    'Serious power', 'Rockets', 'Prion red cell', 'Timestrike blue cell',
    'Cannon balls', 'Tommygun ammo', 'Minigun ammo', 'Ankh',
]


def actor_content(resource, frame):
    if 32 <= resource <= 63:
        item = (frame-1)%28
        return f'pickup-{item+1:02}', PICKUPS[item]+' pickup'
    if 96 <= resource <= 98:
        item = (frame-1)%3
        return f'prop-{item}', ['Piano','Anvil','Safe'][item]
    return ACTORS[resource]


def screen_content(index):
    """Fallbacks for artwork outside the named weapon/HUD sequences."""
    if index == 25: return 'weapon-3', 'Coach Gun', 'Coach Gun · weapon frame'
    if 63 <= index <= 88: return 'netricsa-ui', 'NETRICSA interface', 'NETRICSA interface piece'
    if 89 <= index <= 109: return 'hud-meter-pieces', 'Meter pieces', 'Meter piece'
    if 110 <= index <= 139: return 'hud-digits', 'Health numbers', 'Number'
    if 140 <= index <= 264: return 'hud-letters', 'HUD letters and numbers', 'Letter or symbol'
    if 265 <= index <= 270: return 'hud-status', 'Health and armour icons', 'Status icon'
    if 271 <= index <= 279 or index in (288,294): return 'hud-ammo', 'Ammo icons', 'Ammo icon'
    if 280 <= index <= 293: return 'hud-weapons', 'Weapon icons', 'Weapon icon'
    if 295 <= index <= 298: return 'crosshairs', 'Crosshairs', 'Crosshair'
    if 299 <= index <= 303: return 'hud-bosses', 'Boss and player icons', 'Boss or player icon'
    if 304 <= index <= 310: return 'screen-effects', 'On-screen impact effects', 'Impact effect'
    raise ValueError(f'Unclassified screen sprite: {index}')
