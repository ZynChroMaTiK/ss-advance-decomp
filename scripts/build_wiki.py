"""Build the small NETRICSA field guide from exported maps and curated research.

Requires no ROM. Original illustrations are copied from an existing graphics
export on the first build; committed site artwork supports subsequent builds.
"""
from pathlib import Path
from collections import Counter
import json, shutil, hashlib

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT/'site'

def source(title, path):
    return {'title':title,'href':path}

def main():
    catalog=json.loads((SITE/'data/catalog.json').read_text())
    levels=[json.loads((SITE/e['path']).read_text()) for e in catalog]
    entries=json.loads((ROOT/'scripts/wiki_entities.json').read_text())
    illustrations=SITE/'assets/netricsa'; illustrations.mkdir(exist_ok=True)
    manifest={}
    for i in range(23):
        name=f'netricsa-{i:02}.png';target=illustrations/name
        original=ROOT/'build/exports/ssa-graphics/netricsa'/name
        if original.exists():shutil.copy2(original,target)
        assert target.exists(),f'Extract original NETRICSA artwork first: {name}'
        manifest[name]={'source':f'netricsa/{name}','sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
    (illustrations/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for entry in entries:
        entry['image']='assets/netricsa/'+Path(entry['image']).name
        entry['appearances']=[]
        for level in levels:
            matches=[m for m in level['markers'] if (entry.get('entity_type') is not None and m['entity_type']==entry['entity_type']) or (entry['category']=='weapons' and m['name'] in entry.get('pickup_names',[]))]
            if matches:entry['appearances'].append({'level':level['level'],'title':level['title'],'count':len(matches)})
        entry.setdefault('related',[])
        entry.setdefault('facts',[])
        entry['facts'].append(['Maps with pickup records' if entry['category']=='weapons' else 'Recorded levels',len(entry['appearances'])])
        entry.setdefault('aliases',[]).extend(entry.get('pickup_names',[]))
    enemy_lookup={e['entity_type']:e for e in entries if e['category']=='enemies'}
    for level in levels:
        n=level['level'];chapter='Egypt' if n<6 else 'Rome' if n<12 else 'Multiplayer'
        enemies=Counter(m['entity_type'] for m in level['markers'] if m['entity_type'] in enemy_lookup)
        summary=f"{chapter} {'campaign level' if n<12 else 'arena'} {n+1 if n<12 else n-11}. Explore its recovered architecture, original placements and scripted encounters."
        entry={'id':f'level-{n+1}','title':level['title'],'category':'levels','level':n,'summary':summary,'aliases':[chapter,f'map {n+1}',f'level {n+1}'],
          'facts':[['Region',chapter],['Sectors',len(level['sectors'])],['Placements',len(level['markers'])],['Scripts',len(level['scripts'])]],
          'sections':[{'title':'Native port availability','text':'Playable in the current native Windows prototype.' if n<10 else 'Decoded and available in the atlas and 3D viewer. Native campaign progression is still being validated; this map remains unavailable in the playable level selector.' if n<12 else 'Available for exploration. The native project contains multiplayer components, but playable multiplayer integration remains unfinished.'},
          {'title':'Encounter records','text':', '.join(f"{enemy_lookup[t]['title']} ({count})" for t,count in enemies.items())+'. Counts include scripted placement records, which need not be active together.' if enemies else 'No enemy placement records are present in this export.'},
          {'title':'Doors, switches and secrets','text':f"The map export records {len(level['movers'])} sector movers and {len(level['switches'])} switches. {len(level['secrets'])} secret locations are documented here. The secret catalogue is incomplete; absence of a pin does not mean absence of a secret."}],
          'related':[enemy_lookup[t]['id'] for t in enemies]+['doors-and-scripts','secrets-and-exploration'],
          'sources':[source('Exported level data',catalog[n]['path']),source('Map recovery','docs/map-assets.html')],
          'coverage':'Counts describe extracted records, not a live playthrough. The level browser shows original placements and static geometry; it does not simulate gameplay.'}
        entries.append(entry)
        for secret in level['secrets']:
            entries.append({'id':f'secret-{n+1}-{secret["id"].lower()}','title':secret['title'],'category':'secrets','level':n,'summary':f"{level['title']} / {secret['id']}",'facts':[['Map',n+1],['Location',f"{secret['x']}, {secret['y']}"]],'sections':[{'title':'Finding the location','text':secret['note']},{'title':'Evidence','text':secret['status']}],'related':[f'level-{n+1}','secrets-and-exploration'],'sources':[source('Secret coverage and source links','atlas/sources-and-secret-coverage.json')],'coverage':'A documented secret location from the existing atlas research. Secret identification is not exhaustive.'})
    item_descriptions={
      'health':('Health pickups','Restore Sam’s health.','The export records +25, +50 and +100 health pickups. Smaller pickups use a 100-health eligibility gate. The +100 pickup is collectible below 200 health, then adds 100 without capping the result: 199 becomes 299.','docs/inventory.html'),
      'armor':('Armor and shields','Protection pickups in several sizes.','The atlas identifies separate shield and armor families in 25-, 50- and 100-point variants. The recovered inventory helpers gate protection pickups at 100; damage resolves through the original ordered protection fields.','docs/combat-damage.html'),
      'ammo':('Ammunition','Keep the original weapon set supplied.','Ammo records include bullets, shotgun cartridges, rockets, cannon balls and colored energy cells. In single player, pickup acceptance depends on owning the corresponding weapon and available capacity.','docs/pickups.html'),
      'items':('Backpacks and power-ups','Other inventory and power-up records.','Backpacks and Serious power appear in the extracted placements. Item effects and eligibility follow the recovered pickup dispatch; the atlas separates these from health, protection and ammunition.','docs/pickups.html'),
      'exits':('Ankhs and trigger pickups','Pickups can participate in level scripts.','A pickup can carry a script trigger. The recovered handler may dispatch that trigger before deciding whether the inventory accepts the item. Consult the map’s script and exit records to understand its role.','docs/level-script.html')}
    for group,(title,summary,body,doc) in item_descriptions.items():
        matches=[m for d in levels for m in d['markers'] if m['group']==group]
        names=sorted({m['name'] for m in matches})
        entries.append({'id':'items-'+group,'title':title,'category':'items','summary':summary,'image':'atlas/'+next(m['sprite'] for m in matches if m.get('sprite')),'aliases':names,'facts':[['Recorded variants',len(names)],['Placement records',len(matches)]],'sections':[{'title':'How they work','text':body},{'title':'Atlas labels','text':', '.join(names)+'.'}],'related':['inventory-and-loadouts','doors-and-scripts'],'sources':[source('Recovered behavior',doc),source('Atlas data','data/catalog.json')]})
    systems=[
      ('field-guide','Your portable field guide','The worlds, creatures and equipment of Serious Sam Advance, brought together in a growing NETRICSA-style reference.', [('Start with a subject','Search for a name, choose a category, or follow a related record. Every level links directly to its 2D atlas and 3D world view. Weapons and enemies list maps where placement records identify them.'),('Original intel, recovered behavior','Weapon and enemy illustrations come from the original NETRICSA archive. Descriptions are short project summaries, with original lore kept separate from verified native behavior. Expand Sources & coverage for the underlying evidence.')],['pc-controls','level-1','weapon-chainsaw'], 'README'),
      ('pc-controls','Sam on PC','Keyboard and mouse controls, with deliberate additions to the handheld original.', [('Movement and view','WASD moves, the mouse looks, Space jumps, and H switches between first- and third-person views. Jump and the third-person toggle are PC additions. The camera checks nearby geometry.'),('Combat and interaction','Left-click fires. E interacts. Number keys 1–0 and the mouse wheel select weapons; Q recalls the last weapon. R restarts the current level. Escape releases the mouse.')],['inventory-and-loadouts','field-guide'],'README'),
      ('inventory-and-loadouts','Inventory and loadouts','Weapons, ammunition and carried progress belong to the campaign.', [('Starting a level','Standalone and password starts use the original level-start weapons and ammunition. Advancing from an earlier level carries the earned inventory and score; restarting restores the level-entry state.'),('Picking things up','Weapon pickups queue acquisition, while ammunition checks ownership and capacity. Health and protection pickups have their own eligibility and caps.')],['items-health','items-armor','items-ammo','saving-and-passwords'],'weapon-loadout'),
      ('doors-and-scripts','Doors, switches and encounters','The map can change as you move through it.', [('A connected world','Switches and trigger regions activate scripts. Scripts can request moving sectors, spawn groups of enemies and advance level progression. Defeating an encounter group can unlock a later scripted event.'),('Reading the atlas','The atlas exposes doors, lifts, switches, trigger regions and script records. The 3D open-door preview shows a static alternate ceiling height; it does not play the script or move a lift.')],['secrets-and-exploration','level-1'],'level-script'),
      ('saving-and-passwords','Progress and passwords','Return to completed campaign levels from the native menu.', [('Campaign progress','The prototype saves beaten levels and language preferences in save.dat. Finishing a level unlocks its successor; the last two campaign maps remain unavailable until supported.'),('Original password starts','The Password menu accepts the original case-insensitive codes and restores starting weapons, ammunition and difficulty for supported maps. Select Level also uses original starting loadouts.')],['inventory-and-loadouts','pc-controls'],'profile'),
      ('secrets-and-exploration','Reading the hidden world','The atlas exposes every decoded room, while secret identifications remain a work in progress.', [('Documented locations','Five secret locations are documented across the first five campaign maps. Their entries retain the existing guide evidence and recovered map details. This does not establish that every secret has been found.'),('Two views, one level','Use 2D for labels and original sprite markers. Use 3D for room heights and spatial relationships. Surfaces render from the front side only; sky boundaries are omitted. Hide indoor ceilings for an unobstructed overhead view.')],['level-1','level-2','level-3','level-4','level-5'],'map-assets')]
    for id,title,summary,sections,related,doc in systems:
        entries.append({'id':id,'title':title,'category':'systems','summary':summary,'sections':[{'title':a,'text':b} for a,b in sections],'related':related,'sources':[source('Project reference',f'docs/{doc}.html')]})
        if id=='field-guide':entries[-1]['image']='assets/netricsa/netricsa-22.png'
    order={name:i for i,name in enumerate(['levels','weapons','enemies','items','systems','secrets'])}
    entries.sort(key=lambda e:(-1 if e['id']=='field-guide' else order[e['category']],e.get('level',-1),e['title']))
    result={'version':1,'description':'Project-authored field guide with original NETRICSA artwork and recovered map evidence.','entries':entries}
    (SITE/'data/wiki.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    print(json.dumps({'entries':len(entries),'categories':dict(Counter(e['category'] for e in entries))}))

if __name__=='__main__':main()
