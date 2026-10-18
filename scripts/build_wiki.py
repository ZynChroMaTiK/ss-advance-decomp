"""Build the small NETRICSA field guide from exported maps and curated research.

Requires no ROM. Original illustrations are copied from an existing graphics
export on the first build; committed site artwork supports subsequent builds.
"""
from pathlib import Path
from collections import Counter
import json, shutil, hashlib, re

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT/'site'

def source(title, path):
    return {'title':title,'href':path}

def main():
    campaign_limit=int(re.search(r'SSA_NATIVE_CAMPAIGN_LEVELS=(\d+)',(ROOT/'include/ssa/native_game.h').read_text()).group(1))
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
        entry['facts'].append(['Maps with this pickup' if entry['category']=='weapons' else 'Found in',len(entry['appearances'])])
        entry.setdefault('aliases',[]).extend(entry.get('pickup_names',[]))
    enemy_lookup={e['entity_type']:e for e in entries if e['category']=='enemies'}
    for level in levels:
        n=level['level'];chapter='Egypt' if n<6 else 'Rome' if n<12 else 'Multiplayer'
        enemies=Counter(m['entity_type'] for m in level['markers'] if m['entity_type'] in enemy_lookup)
        summary=f"{chapter} {'campaign level' if n<12 else 'arena'} {n+1 if n<12 else n-11}. Explore its rooms, find pickups and see where enemies appear."
        entry={'id':f'level-{n+1}','title':level['title'],'category':'levels','level':n,'summary':summary,'aliases':[chapter,f'map {n+1}',f'level {n+1}'],
          'facts':[['Region',chapter],['Room sections',len(level['sectors'])],['Enemy and item locations',len(level['markers'])],['Events',len(level['scripts'])]],
          'sections':[{'title':'Can I play it?','text':'Playable in the current Windows release.' if n<campaign_limit else 'You can explore it in the maps here, but it is not yet available in the game.' if n<12 else 'You can explore this map here. Multiplayer is not yet playable.'},
          {'title':'Enemies here','text':', '.join(f"{enemy_lookup[t]['title']} ({count})" for t,count in enemies.items())+'. Some appear later, so these enemies will not all be present at once.' if enemies else 'No enemy locations are listed for this map.'},
          {'title':'Doors, switches and secrets','text':f"This map has {len(level['movers'])} doors or lifts and {len(level['switches'])} switches. We have marked {len(level['secrets'])} secrets so far. There may be more to find."}],
          'related':[enemy_lookup[t]['id'] for t in enemies]+['doors-and-scripts','secrets-and-exploration'],
          'sources':[source('Map data',catalog[n]['path']),source('Developer notes: maps','docs/map-assets.html')],
          'coverage':'The map shows locations from the original game. Enemies and doors stay still here, so you can take your time looking around.'}
        entries.append(entry)
        for secret in level['secrets']:
            entries.append({'id':f'secret-{n+1}-{secret["id"].lower()}','title':secret['title'],'category':'secrets','level':n,'summary':f"{level['title']} / {secret['id']}",'facts':[['Map',n+1],['Location',f"{secret['x']}, {secret['y']}"]],'sections':[{'title':'Where to look','text':secret['note']},{'title':'How we found it','text':secret['status']}],'related':[f'level-{n+1}','secrets-and-exploration'],'sources':[source('Walkthrough',secret['source']),source('Secret notes and sources','atlas/sources-and-secret-coverage.json')],'coverage':'A known secret marked using a walkthrough and the original map. We are still looking for others.'})
    item_descriptions={
      'health':('Health pickups','Restore Sam’s health.','Health pickups add 25, 50 or 100 points. You can collect the smaller ones when health is below 100. The +100 pickup works below 200 and adds the full amount: 199 becomes 299.','docs/inventory.html'),
      'armor':('Armor and shields','Help Sam take more hits.','Armor and shields come in 25-, 50- and 100-point sizes. You can collect them while the matching protection value is below 100.','docs/combat-damage.html'),
      'ammo':('Ammunition','Keep your weapons supplied.','Look for bullets, shells, rockets, cannonballs and coloured energy cells. You need the matching weapon and room for more ammo to pick them up.','docs/pickups.html'),
      'items':('Backpacks and power-ups','Extra supplies and temporary help.','Look for backpacks and Serious power among the map’s other items. Use the map filters to find them separately from health, armor and ammo.','docs/pickups.html'),
      'exits':('Ankhs and special pickups','Some pickups also make something happen in the level.','Touching certain pickups can start an event, even when you cannot collect the item itself. Select one on the map to see what it does.','docs/level-script.html')}
    for group,(title,summary,body,doc) in item_descriptions.items():
        matches=[m for d in levels for m in d['markers'] if m['group']==group]
        names=sorted({m['name'] for m in matches})
        entries.append({'id':'items-'+group,'title':title,'category':'items','summary':summary,'image':'atlas/'+next(m['sprite'] for m in matches if m.get('sprite')),'aliases':names,'facts':[['Different types',len(names)],['Locations',len(matches)]],'sections':[{'title':'How they work','text':body},{'title':'Names on the map','text':', '.join(names)+'.'}],'related':['inventory-and-loadouts','doors-and-scripts'],'sources':[source('Developer notes',doc),source('Atlas data','data/catalog.json')]})
    systems=[
      ('field-guide','About this guide','Stuck on an enemy or wondering what a weapon does? Start here.', [('Finding something','Search for a name or choose a category. Each level links to its 2D map and 3D view. Weapon and enemy pages also show where you can find them.'),('About this guide','The illustrations come from the original game. We wrote these descriptions to help you play. Open Sources & notes if you want to see the research behind an entry.')],['pc-controls','level-1','weapon-chainsaw'], 'README'),
      ('pc-controls','Sam on PC','Play with a keyboard and mouse, plus optional jumping and third-person view.', [('Moving around','WASD moves, the mouse looks, Space jumps, and H switches between first- and third-person views. Jump and the third-person toggle are PC additions. The camera moves closer when walls get in the way.'),('Fighting and using things','Left-click fires. E uses doors and switches. Number keys 1–0 and the mouse wheel select weapons; Q goes back to the last weapon. R restarts the current level. Esc/P pauses or resumes. Tab/N opens the in-game NETRICSA guide when enabled.')],['inventory-and-loadouts','field-guide'],'README'),
      ('inventory-and-loadouts','Weapons and supplies','What you carry, and what changes when you start a level.', [('Starting a level','Select Level and passwords give you the original starting weapons and ammo for that level. Finishing the previous level carries your supplies and score forward. Restarting puts you back with what you had when you entered.'),('Picking things up','Walk over pickups to collect them. Ammo needs a matching weapon and space to carry more. Health, armor and shields have their own limits.')],['items-health','items-armor','items-ammo','saving-and-passwords'],'weapon-loadout'),
      ('doors-and-scripts','Doors, switches and encounters','The map can change as you move through it.', [('Doors and enemy waves','Using a switch or entering an area can open a door, move a lift or bring in more enemies. Sometimes you need to defeat a group before you can move on.'),('Reading the atlas','Select a door, switch or green event area to see what happens there. In 3D, Show doors open lets you look through doorways. The viewer does not run the level’s events.')],['secrets-and-exploration','level-1'],'level-script'),
      ('saving-and-passwords','Progress and passwords','Pick up your campaign progress from the main menu.', [('Campaign progress','Beating a level unlocks the next one in Select Level. Keep save.dat when updating to keep your unlocks. All twelve campaign levels are available, including Caesar’s Palace. There are no mid-level saves.'),('Using a password','Enter an original game password to start with that level’s weapons, ammo and difficulty. Uppercase and lowercase both work. Select Level also gives you the original starting supplies.')],['inventory-and-loadouts','pc-controls'],'profile'),
      ('secrets-and-exploration','Finding secrets','Look around every map and discover some of its hidden places.', [('Secrets marked so far','Five secrets are marked across the first five campaign levels, with links to walkthroughs. The list is still growing, so an unmarked map may have secrets too.'),('Using the maps','Use the 2D map to find labelled items and enemies. Switch to 3D to look around rooms and see their heights. Hide ceilings for a clearer view from above.')],['level-1','level-2','level-3','level-4','level-5'],'map-assets')]
    for id,title,summary,sections,related,doc in systems:
        entries.append({'id':id,'title':title,'category':'systems','summary':summary,'sections':[{'title':a,'text':b} for a,b in sections],'related':related,'sources':[source('More notes',f'docs/{doc}.html')]})
        if id=='field-guide':entries[-1]['image']='assets/netricsa/netricsa-22.png'
    order={name:i for i,name in enumerate(['levels','weapons','enemies','items','systems','secrets'])}
    entries.sort(key=lambda e:(-1 if e['id']=='field-guide' else order[e['category']],e.get('level',-1),e['title']))
    result={'version':1,'description':'A guide to the game, with original NETRICSA artwork and maps.','entries':entries}
    (SITE/'data/wiki.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    print(json.dumps({'entries':len(entries),'categories':dict(Counter(e['category'] for e in entries))}))

if __name__=='__main__':main()
