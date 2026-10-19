"""Original texture brightness sets, verified against the exported pixels.

Each row lists one design from lightest to darkest, using original texture
indices at full resolution. The same records recur in all three mip banks.
Pixel correlation helped locate candidates; visual comparison excludes changed
motifs (Egypt pillar3/wallD/whi01), mirrored steps, switch states and diagonal
shadow designs. These are stored artwork variants, not runtime light levels.
"""

BRIGHTNESS_SETS = {
    0: [
        (4,5,6), (7,8,9), (10,11,12), (13,14), (17,18,19), (20,21),
        (22,23,24), (28,29,30), (31,32,33), (34,35), (36,37,38),
        (40,41,42), (46,47,48), (49,50,51), (52,53,54), (55,56,57),
        (60,61,62), (65,66,67), (71,72,73), (74,75,76), (77,78,79),
        (80,81,82), (83,84,85), (86,87,88), (89,90,91), (92,93,94),
    ],
    1: [
        (0,1,2), (8,9,10), (12,13,14), (15,16,17), (18,19,20),
        (24,25,26), (27,28,29), (30,31,32), (33,34,35), (36,37,38),
        (40,41,42), (43,44,45), (46,47,48), (49,50,51), (52,53,54),
        (55,56,57), (58,59,60), (65,66,67), (68,69,70), (71,72,73),
        (76,77,78), (81,82,83), (84,85,86), (87,88,89), (90,91,92),
        (93,94,95), (96,97,98), (99,100,101), (102,103,104),
        (105,106,107), (108,109,110), (112,113,114), (115,116,117),
        (118,119,120),
    ],
}


def texture_brightness(texture, textures):
    family, mip, index = (texture[k] for k in ('family','mip','index'))
    group = next((g for g in BRIGHTNESS_SETS[family] if index in g), None)
    if group is None: return {}
    rank = group.index(index)
    level = ('Light','Dark')[rank] if len(group)==2 else ('Light','Medium','Dark')[rank]
    return {
        'brightnessLabel': level+' variant',
        'brightnessVariants': [
            {'id':f'texture-{family}-{mip}-{i:03}', 'name':textures[family,mip,i]['name'].strip()}
            for i in group
        ],
    }
