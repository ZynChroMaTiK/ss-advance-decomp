"""Gallery groups from game animation selections and their callers.

Content names live in graphics_content.py. Uses are deliberately separate:
one picture can serve multiple animations. Only frames actually selected by
the sequence (expanded over directions and distance sizes) receive that use.
Unused artwork stays with its subject; being stored in a table does not prove gameplay.
"""
from collections import defaultdict
from pathlib import Path
import re
import json
import struct

SECTIONS = {
    'enemies':'Enemies', 'player':'Sam and player colours', 'weapons':'Weapons in hand',
    'pickups':'Pickups and level exits', 'projectiles':'Projectiles and weapon effects',
    'effects':'Hits, explosions and spawning', 'hud':'HUD and on-screen messages', 'old-hud':'Old HUD',
    'netricsa':'NETRICSA', 'menus':'Menus and startup', 'story':'Chapter screens and ending',
    'textures-egypt':'Textures · Egypt', 'textures-rome':'Textures · Rome',
    'world':'Skies', 'text':'Text and colour tables',
}


def use(key, title, section, source):
    assert section in SECTIONS
    return {'id':key,'title':title,'section':section,'source':source}


def selected_actor_frames(resource, words):
    count = len(resource['images'])
    if not count: return set()
    directions = resource['directions']; sizes = resource['resolution_flags'].bit_count()
    # Match actor_assets.c's truncated byte stride, including resource 15's
    # seven images / five directions. The final two images are not selected.
    stride = (count//directions//sizes) & 255
    frames = {w for w in words if w < 0xfffa}
    selected = {1+w+stride*(direction+directions*size)
        for w in frames for direction in range(directions) for size in range(sizes)}
    assert all(1 <= frame <= count for frame in selected), (resource['id'],selected)
    return selected


def actor_uses(manifest, sequences):
    result = defaultdict(list)
    def add(resource, title, section, source, key=None):
        record = use(key or f'animation-{resource:03}-'+re.sub('[^a-z0-9]+','-',title.lower()).strip('-'), title, section, source)
        for frame in selected_actor_frames(manifest['actor_resources'][resource],sequences[resource]['words']):
            result[resource,frame].append(record)
    # Controllers choose these animation IDs explicitly. Hurt and attack may
    # share one sequence; retain BOTH uses rather than labelling by appearance.
    enemies = [
        ('Cyclops',[(4,'Walking'),(5,'Dying'),(30,'Taking damage'),(67,'Firing')]),
        ('Methug commander',[(6,'Moving'),(17,'Dying'),(18,'Taking damage')]),
        ('Methug soldier',[(7,'Walking'),(19,'Dying'),(20,'Taking damage'),(20,'Attacking')]),
        ('Kamikaze',[(8,'Running'),(28,'Exploding')]),
        ('Bladder beast',[(9,'Walking'),(21,'Dying'),(22,'Taking damage'),(137,'Attacking')]),
        ('Werebull',[(10,'Charging'),(23,'Dying'),(24,'Taking damage')]),
        ('Gunrilla',[(25,'Walking'),(26,'Dying'),(27,'Taking damage'),(141,'Firing')]),
        ('Hermit Crabuloid',[(64,'Walking'),(65,'Dying'),(66,'Taking damage'),(138,'Hiding'),(139,'Hidden'),(140,'Emerging')]),
        ('Arachnoid',[(69,'Walking'),(70,'Dying'),(71,'Taking damage'),(72,'Firing')]),
        ('Reptiloid',[(73,'Moving'),(74,'Dying'),(75,'Taking damage'),(76,'Firing')]),
    ]
    for name,states in enemies:
        source = 'src/enemy_cd.c' if name == 'Kamikaze' else 'src/enemy_control.c' if name in ('Cyclops','Methug commander','Methug soldier','Bladder beast') else 'src/enemy_ranged.c'
        for resource,state in states: add(resource,f'{name} · {state}','enemies',source)
    for name,base in [('Wolfiator',77),('Sirian Sphinx',83)]:
        for offset,state in enumerate(['Waiting for summoned enemies','Dying','Taking damage','Firing','Summoning enemies','Aiming']):
            add(base+offset,f'{name} · {state}','enemies','src/enemy_boss.c')
    # Player and separately drawn shirt layers follow the same five states.
    for base,name in [(101,'Sam'),(106,'Blue player shirt'),(111,'Green player shirt'),(116,'Orange player shirt'),(121,'White player shirt')]:
        for offset,state in [(0,'Standing'),(1,'Moving'),(3,'Dying')]:
            add(base+offset,f'{name} · {state}','player','src/player_frame.c; src/player_lifecycle.c; src/actor_projection.c')
    pickups = ['Pistol','Shotgun','Tommygun','Rocket launcher','Unconfirmed weapon pickup','Minigun','Cannon','U.M.T','Timestrike','Prion gun',
        '25 shield','50 shield','100 shield','25 armour','50 armour','100 armour','25 health','50 health','100 health','Backpack',
        'Pistol bullets','Shotgun cartridges','U.M.T ammo','Serious power','Rockets','Cannon balls','Tommygun ammo','Minigun ammo','Timestrike ammo','Prion ammo','Shared blue ammo','Ankh / level exit']
    for resource,title in enumerate(pickups,32):
        if resource in (36,62): continue  # A sequence alone does not establish a live use.
        add(resource,title,'pickups','scripts/export_topmaps.py; src/pickup.c',f'pickup-use-{resource}')
    for resource,title in {
        11:'Moving marker · Type 0x64',15:'Rocket · In flight',68:'Enemy fireball · In flight',
        90:'Cannonball · In flight',95:'Prion shot · In flight',91:'Prion conversion · Target marker',
        92:'Spreading projectile · Charging',93:'Spreading projectile · In flight',94:'Spreading projectile · Expiring',
        96:'Time Strike · Falling piano',97:'Time Strike · Falling safe',98:'Time Strike · Falling anvil',
        131:'Sirian Sphinx projectile · In flight',133:'Wolfiator projectile · In flight',135:'Green projectile · In flight',
    }.items():
        add(resource,title,'projectiles','src/projectile_controller.c; src/projectile_extended.c; src/projectile_runtime.c; src/weapon_controller.c')
    # Original particle table at ROM 0x44C0BC, with callers in the named files.
    particles = [(14,'Bullet hitting a wall'),(12,'Bullet hitting a target'),(100,'Chainsaw hitting a target'),
        (31,'Projectile explosion'),(89,'Player / scripted spawn'),(99,'Time Strike impact'),
        (126,'Pistol muzzle flash'),(127,'Tommygun muzzle flash'),(128,'Minigun muzzle flash'),(129,'Shotgun muzzle flash'),(130,'Rocket launcher muzzle flash'),
        (132,'Sirian Sphinx projectile impact'),(134,'Wolfiator projectile impact'),(136,'Green projectile impact')]
    for resource,title in particles:
        add(resource,title,'effects','src/particle_create.c; src/weapon_controller.c; src/projectile_controller.c; src/projectile_extended.c')
    return result


def screen_uses(sequences, wiki):
    result = defaultdict(list)
    def add(sequence,title,section,source,key=None):
        record = use(key or f'screen-sequence-{sequence}',title,section,source)
        for frame in {w & 0x3fff for w in sequences[sequence]['words'] if w < 0xfffa}:
            result[frame].append(record)
    for weapon in (w for w in wiki if w['category']=='weapons'):
        n=weapon['weapon_id'];title=weapon['title']
        add(n-1,title+' · Ready','weapons','src/weapon_animation.c')
        add(n+9,title+' · Firing','weapons','src/weapon_controller.c')
        add(0x41+n,title+' · Selected weapon icon','hud','src/hud_sprites.c')
        if n != 1: add(0xde+n,title+' · Ammo icon','hud','src/hud_sprites.c')
    for sequence,title in [(20,'Chainsaw · Sustained cutting'),(22,'Coach Gun · One shell')]:
        add(sequence,title,'weapons','src/weapon_controller.c')
    # Sequences 0x24..0x41 belong to the old numeric HUD. The type-5
    # controller remains, but the original HUD constructor creates no type 5.
    add(0x4c,'HUD · Selection rectangle','hud','src/hud_sprites.c')
    for sequence in range(92,217):
        add(sequence,'HUD · Messages and counters','hud','src/hud_sprites.c','hud-text')
    for sequence,title in {84:'HUD · Health and armour bars',85:'HUD · Health and armour bars',86:'HUD · Health icon',
        87:'HUD · Armour icon',89:'HUD · Egypt boss health',90:'HUD · Rome boss health',91:'HUD · Lives',
        217:'HUD · Crosshair',218:'HUD · Crosshair on target',219:'HUD · Player 1',220:'HUD · Player 2',221:'HUD · Player 3',222:'HUD · Player 4'}.items():
        add(sequence,title,'hud','src/hud_sprites.c', 'hud-bars' if sequence in (84,85) else None)
    return result


def other_uses(entry):
    category=entry['category'];path=entry['images'][0]['source']
    if category == 'netricsa': return [use('netricsa-pictures','NETRICSA · Weapon and enemy illustrations','netricsa','src/netricsa.c')]
    if category == 'story':
        index=int(Path(path).stem[-2:])
        if index >= 5: return []
        chapter=0 if index < 3 else index-2
        return [use('story-'+str(chapter),['Egypt chapter','Rome chapter','Ending'][chapter],'story','src/campaign_story.c; src/campaign_screen.c')]
    if category == 'splashes':
        if 'hidden-photo' in path: return []
        return [use('startup','Startup logos','menus','src/menu.c')]
    if category == 'menus':
        if 'netricsa' in path: return [use('netricsa-interface','NETRICSA · Panels and frame','netricsa','src/netricsa.c')]
        return [use('menu-backgrounds','Menu backgrounds and title','menus','src/menu.c')]
    if category == 'sky': return [use('level-skies','Level skies','world','src/pc_renderer.c')]
    if category == 'fonts': return [use('text-'+path.split('/')[1],path.split('/')[1]+' menu and briefing text','text','src/menu.c; src/campaign_screen.c')]
    if category == 'palettes': return [use('colour-tables','Colour tables for rendering','text','scripts/export_presentation_assets.py')]
    return []


def texture_uses(family):
    # The original texture banks define the setting, including stored textures
    # not referenced by an exported level. Doors and walls share those banks.
    setting = ('Egypt', 'Rome')[family]
    key = 'textures-'+setting.lower()
    return [use(key, setting+' textures', key, 'src/texture_assets.c')]


def merge_uses(first, second):
    by_id={u['id']:u for u in first+second}
    return list(by_id.values())


def referenced_textures(archive, manifest):
    """All 16 maps, every side layer, both switch states, and named mip images.

    Read original map records rather than just currently visible wall geometry.
    This includes multiplayer maps and textures behind closed doors. Floors and
    ceilings use colours/skies, not the wall texture banks (map_texture_bindings.c).
    """
    banks = {(family,mip):{} for family in range(2) for mip in range(3)}
    for texture in manifest['textures']:
        banks[texture['family'],texture['mip']].setdefault(texture['name'].split('\0')[0][:8].lower(), texture['index'])
    used = set()
    pairs = {'sw1_off ':'sw1_on  ','sw1_on  ':'sw1_off ', 'sw2_off ':'sw2_on  ','sw2_on  ':'sw2_off '}
    for level in range(16):
        meta = json.loads(archive.read(f'maps/map-{level:02}.json'))
        raw = archive.read(f'maps/map-{level:02}.wod0')
        family = 0 if level < 6 or level in (12,13) else 1
        sections = meta['sections']; sides = sections['sides']; lines = sections['lines']
        names = [name.split('\0')[0][:8].lower() for name in meta['texture_names']]
        referenced = set()
        for side in range(sides['count']):
            for index in struct.unpack_from('<3h',raw,sides['offset']+side*12+4):
                if index >= 0: referenced.add(names[index])
        for line in range(lines['count']):
            offset = lines['offset']+line*40
            if struct.unpack_from('<H',raw,offset+36)[0] != 2: continue
            side = struct.unpack_from('<h',raw,offset+8)[0]
            if side < 0: continue
            index = struct.unpack_from('<h',raw,sides['offset']+side*12+6)[0]
            if index >= 0 and names[index] in pairs:
                partner = pairs[names[index]]
                if partner in names: referenced.add(partner)
        for name in referenced:
            # These are the same first-name matches used by the native loader.
            assert name in banks[family,0], (level,name)
            for mip in range(3):
                if name in banks[family,mip]: used.add((family,mip,banks[family,mip][name]))
    return used


def unused_category(entry):
    category = entry['category']
    if category == 'story': return 'story'
    if category == 'splashes': return 'menus'
    if category == 'actors':
        resource = int(entry['id'].split('-')[1])
        if resource in (21,23): return 'enemies'
        if resource in (13,15,68): return 'projectiles'
        if resource in (16,29): return 'effects'
        if 32 <= resource <= 63: return 'pickups'
    if category == 'sprites':
        index = int(entry['id'].split('-')[1])
        if index == 25: return 'weapons'
        if 63 <= index <= 139 or index == 269: return 'old-hud'
        if index in (287,288,293,295,296,297): return 'hud'
        if 304 <= index <= 310: return 'effects'
    raise ValueError(f'Unclassified unused graphic: {entry["id"]}')


def finalize_uses(entries):
    for entry in entries:
        if not entry['uses']:
            section = unused_category(entry)
            entry['uses']=[use('stored-'+entry['collection'],entry['collectionTitle'],section,'scripts/graphics_usage.py')]
            entry['unused'] = True
            entry['unusedReason'] = (
                'Left over from the old HUD. The game uses a different HUD.' if section == 'old-hud' else
                'Stored in the game, but not selected by the mapped animation sequences and game screens.')
        else:
            entry.setdefault('unused',False)
        if not entry['unused']: entry.pop('unusedReason',None)
