"""ROM-derived overhead atlas: SVG, JSON and a self-contained interactive viewer."""
from pathlib import Path
import base64, hashlib, html, json, math, struct, zipfile
from collections import defaultdict, Counter
from export_presentation_assets import decompress
from export_graphics_archive import png

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/exports/ssa-topmaps'
ROM=(ROOT/'orig/SSA.gba').read_bytes()
SHA1='4ff48d8c18b70a5184758f1917343ae85a70858e'
assert hashlib.sha1(ROM).hexdigest()==SHA1
u16=lambda b,p:struct.unpack_from('<H',b,p)[0]
s16=lambda b,p:struct.unpack_from('<h',b,p)[0]
u32=lambda b,p:struct.unpack_from('<I',b,p)[0]
ptr=lambda p:u32(ROM,p)-0x8000000
SPRITES={}

def theme_assets():
 # These are the original menu graphics and font sheets, kept byte-for-byte.
 paths=dict(logo='menus/title-logo.png',stone='menus/scrolling-background-512x512.png',
            grid='menus/single-tile.png',egypt='menus/egypt-background.png',rome='menus/rome-background.png',
            white='fonts/16px/palette-03-sheet.png',green='fonts/16px/palette-04-sheet.png')
 assets={};manifest={};(OUT/'theme').mkdir(exist_ok=True)
 with zipfile.ZipFile(ROOT/'build/exports/ssa-graphics.zip') as archive:
  for key,path in paths.items():
   data=archive.read(path);(OUT/'theme'/f'{key}.png').write_bytes(data)
   assets[key]='data:image/png;base64,'+base64.b64encode(data).decode()
   manifest[key]=dict(source=path,path=f'theme/{key}.png',sha256=hashlib.sha256(data).hexdigest())
  assets['font']=json.loads(archive.read('fonts/16px/metrics.json'))
 (OUT/'theme-manifest.json').write_text(json.dumps(manifest,indent=2))
 return assets

def sprite_for(marker,level):
 # Start markers have no animation; render the original standing Sam layers.
 family=int(6<=level<12 or level>=14)
 resource=101 if marker['group']=='starts' else marker['animation']
 if not resource:return None
 variant=(3 if level<12 else marker['entity_type']-201) if marker['group']=='starts' else None
 key=f'p{family}-r{resource:03}'+(f'-shirt{variant}' if variant is not None else '')
 if key in SPRITES:return SPRITES[key]
 def decode(r):
  desc=0x449d2c+12*r;frame=u16(ROM,ptr(desc+4));assert frame<0xfffa
  image=frame+1;table=ptr(desc);assert 1<=image<=u32(ROM,table)
  q=ptr(table+4*image);assert ROM[q:q+4]==b'AGB*'
  ox,oy,w,h=struct.unpack_from('<hhHH',ROM,q+24);base=q+32+2*w;pixels=bytearray(w*h)
  for x in range(w):
   at=base+u16(ROM,q+32+2*x);y=0
   while y<h:
    packet=ROM[at];at+=1;n=packet&127;assert n and y+n<=h
    if not packet&128:
     for k in range(n):pixels[(y+k)*w+x]=ROM[at+k]
     at+=n
    y+=n
  return dict(resource=r,image=image,rom_offset=q,origin_x=ox,origin_y=oy,width=w,height=h,pixels=pixels)
 layers=[decode(resource)]
 if variant is not None:layers.append(decode(101+(variant+1)*5))
 left=min(a['origin_x'] for a in layers);top=min(a['origin_y'] for a in layers)
 w=max(a['origin_x']+a['width'] for a in layers)-left;h=max(a['origin_y']+a['height'] for a in layers)-top
 pixels=bytearray(w*h)
 for a in layers:
  for y in range(a['height']):
   for x in range(a['width']):
    c=a['pixels'][y*a['width']+x]
    if c:pixels[(y+a['origin_y']-top)*w+x+a['origin_x']-left]=c
 palette=list(struct.unpack_from('<256H',ROM,0x454a5c+family*512));palette[0]=0
 data=png(w,h,bytes(pixels),palette,True);path=f'icons/{key}.png'
 (OUT/path).write_bytes(data)
 s=dict(path=path,width=w,height=h,palette=family,sha256=hashlib.sha256(data).hexdigest(),
        layers=[{k:v for k,v in a.items() if k!='pixels'} for a in layers],uri='data:image/png;base64,'+base64.b64encode(data).decode())
 SPRITES[key]=s;return s
def text(i):
 p=ptr(0x44c364+i*4)
 return ROM[p:ROM.index(0,p)].decode('latin1').replace('*',"'").strip()
DEFS=[struct.unpack_from('<4H',ROM,0x44a3d4+8*i) for i in range(64)]
ITEMS={32:('Pistol','PI','weapons'),33:('Shotgun','SG','weapons'),34:('Tommygun','TG','weapons'),35:('Rocket launcher','RL','weapons'),
 36:('Unused weapon item','?','items'),37:('Minigun','MG','weapons'),38:('Cannon','CA','weapons'),39:('U.M.T','UM','weapons'),
 40:('Timestrike','TS','weapons'),41:('Prion gun','PR','weapons'),42:('25 shield','S25','armor'),43:('50 shield','S50','armor'),44:('100 shield','S100','armor'),
 45:('25 armor','A25','armor'),46:('50 armor','A50','armor'),47:('100 armor','A100','armor'),48:('25 health','H25','health'),
 49:('50 health','H50','health'),50:('100 health (collect below 200)','H100','health'),51:('Backpack','BP','items'),52:('Pistol bullets','PB','ammo'),
 53:('Shotgun cartridges','SC','ammo'),54:('U.M.T yellow cell','YC','ammo'),55:('Serious power','SP','items'),56:('Rockets','RK','ammo'),
 57:('Cannon balls','CB','ammo'),58:('Tommygun ammo','TA','ammo'),59:('Minigun ammo','MA','ammo'),60:('Timestrike blue cell','BC','ammo'),61:('Prion red cell','RC','ammo'),63:('Ankh / trigger pickup','X','exits')}
ENEMIES={205:'Kamikaze',206:'Werebull',207:'Methug soldier',208:'Methug commander',209:'Cyclops',210:'Bladder beast',
 211:'Arachnoid',212:'Reptiloid',213:'Hermit Crabuloid',214:'Gunrilla',215:'Sirian Sphinx',216:'Wolfiator'}
COLORS={'weapons':'#f2c94c','ammo':'#b5a2ff','health':'#6ce0b1','armor':'#68bdff','items':'#eeeeee','enemies':'#e77a80',
 'starts':'#4be3df','respawn':'#9fa9bc','exits':'#ffffff','doors':'#ffad5a','switches':'#57d5ec','secrets':'#ff7bd5','triggers':'#9ae1aa','unknown':'#eeeeee'}
SOURCES={0:'https://tasvideos.org/10399S',1:'https://serioussam.fandom.com/wiki/Thompson_submachine_gun',
 2:'https://serioussam.fandom.com/wiki/Amon_Thule%2C_Subterranean_Palace_of_the_Pharaohs',3:'https://serioussam.fandom.com/wiki/Slave_Compound',
 4:'https://serioussam.fandom.com/wiki/Pyramid_Entrance_Maze',6:'https://serioussam.fandom.com/wiki/The_Forum_of_Mars',
 7:'https://serioussam.fandom.com/wiki/Slave_Quarters',9:'https://serioussam.fandom.com/wiki/Baths_of_Diocletian',10:'https://serioussam.fandom.com/wiki/Praetorian_Fort'}

def load(level):
 raw,consumed=decompress(ROM,ptr(0x44aef4+4*level)); expected=json.loads((ROOT/'analysis/native-map-assets-validation.json').read_text())['maps'][level]
 assert hashlib.sha256(raw).hexdigest()==expected['decoded_sha256']
 verts=[struct.unpack_from('<hh',raw,u32(raw,0x3c)+4*i) for i in range(u16(raw,22))]
 sectors=[]
 for i in range(u16(raw,10)):
  p=u32(raw,0x24)+20*i
  sectors.append(dict(id=i,floor=s16(raw,p),ceiling=s16(raw,p+2),floor_color=raw[p+10],flags=u16(raw,p+16),raw=raw[p:p+20].hex(),loops=[]))
 lines=[]
 for i in range(u16(raw,20)):
  p=u32(raw,0x38)+40*i
  lines.append(dict(id=i,a=u16(raw,p),b=u16(raw,p+2),front=s16(raw,p+12),back=s16(raw,p+14),flags=u16(raw,p+26),kind=u16(raw,p+36),trigger=u16(raw,p+38)))
 # Clip BSP leaf regions against their original wall halfplanes, as pc_renderer_pitch.inc.
 # This retains implicit BSP edges and handles dangling decorative map lines.
 def clip(poly,ox,oy,dx,dy,positive):
  out=[]
  for a,b in zip(poly,poly[1:]+poly[:1]):
   da=dx*(a[1]-oy)-dy*(a[0]-ox);db=dx*(b[1]-oy)-dy*(b[0]-ox)
   ai=da>=0 if positive else da<=0;bi=db>=0 if positive else db<=0
   if ai:out.append(a)
   if ai!=bi:
    t=da/(da-db);out.append([a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t])
  return out
 xmin,xmax=min(p[0] for p in verts)-1,max(p[0] for p in verts)+1
 ymin,ymax=min(p[1] for p in verts)-1,max(p[1] for p in verts)+1
 visited=[]
 def bsp(index,poly,depth=0):
  assert depth<=u16(raw,12)+1
  if index&0x8000:
   leaf=index&0x7fff;visited.append(leaf);p=u32(raw,0x2c)+leaf*8
   count,first,sector=struct.unpack_from('<3H',raw,p)
   for j in range(count):
    seg=u32(raw,0x30)+(first+j)*16;line=lines[u16(raw,seg+4)];side=u16(raw,seg+8)
    a,b=verts[line['a']],verts[line['b']]
    poly=clip(poly,*a,b[0]-a[0],b[1]-a[1],side)
   if len(poly)>=3:sectors[sector]['loops'].append(poly)
   return
  p=u32(raw,0x28)+index*32;ox,oy,dx,dy=struct.unpack_from('<hhhh',raw,p)
  for side in (0,1):bsp(u16(raw,p+24+side*2),clip(poly,ox,oy,dx,dy,side),depth+1)
 bsp(u16(raw,12)-1,[[xmin,ymin],[xmax,ymin],[xmax,ymax],[xmin,ymax]])
 assert sorted(visited)==list(range(u16(raw,14)))
 for sec in sectors:
  pts=max(sec['loops'],key=lambda p:abs(sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(p,p[1:]+p[:1]))),default=[])
  sec['center']=[sum(p[k] for p in pts)/len(pts) for k in (0,1)] if pts else [0,0]
 def sector_at(x,y):
  index=u16(raw,12)-1
  for _ in range(u16(raw,12)+1):
   if index&0x8000:return u16(raw,u32(raw,0x2c)+(index&0x7fff)*8+4)
   p=u32(raw,0x28)+index*32;ox,oy,dx,dy=struct.unpack_from('<hhhh',raw,p)
   if not dx:side=dy>0 if x<=ox else dy<0
   elif not dy:side=dx<0 if y<=oy else dx>0
   else:
    px,py=x-ox,y-oy
    a=((dy&0xffffffff)^(px&0xffffffff))>>31;b=((dx&0xffffffff)^(py&0xffffffff))>>31
    signed=lambda v:((v+0x80000000)&0xffffffff)-0x80000000
    side=a if a!=b else signed(py*dx)>=signed(dy*px)
   index=u16(raw,p+24+2*int(side))
  raise AssertionError('BSP cycle')
 markers=[]
 def marker(x,y,typ,heading,trigger,origin,**extra):
  et,variant,animation,period=DEFS[typ]
  if origin=='map' and level==10 and x in (2240,3008) and y==-2304 and et not in range(201,205) and et!=217:y=-2108
  if et==225:name,code,group=ITEMS.get(animation,(f'Pickup animation {animation}','?','unknown'))
  elif et in ENEMIES:name,code,group=ENEMIES[et],'E','enemies'
  elif 201<=et<=204:name,code,group=f'Player {et-200} start',f'P{et-200}','starts'
  elif et==217:name,code,group='Multiplayer pickup respawn point','R','respawn'
  else:name,code,group=f'Object type {typ}','?','unknown'
  sec=sector_at(x,y)
  m=dict(id=f'M{len(markers)+1:03}',x=x,y=y,type=typ,entity_type=et,animation=animation,name=name,code=code,group=group,
   trigger=trigger,origin=origin,heading=heading,sector=sec,floor=sectors[sec]['floor'],**extra)
  markers.append(m);return m
 for i in range(u16(raw,6)):
  p=u32(raw,0x1c)+i*12;x,y,heading,typ,trig,last=struct.unpack_from('<hhHHHH',raw,p)
  assert typ<64
  marker(x,y,typ,heading,trig,'map',thing_index=i,unused_halfword=last)
 scripts=[];table=ptr(0x45280c+4*level);end=ptr(0x45280c+4*(level+1)) if level<15 else 0x45280c
 for i in range((end-table)//4):
  pc=ptr(table+i*4);ops=[]
  for step in range(512):
   op,a,b,c,d=struct.unpack_from('<5I',ROM,pc);assert 1<=op<=10,(level,i,hex(pc),op)
   entry=dict(op=op,rom_address=pc+0x8000000,args=[a,b,c,d]);ops.append(entry)
   if op==1:
    at=a-0x8000000;spawned=[]
    for j in range(200):
     x,y,typ,heading=struct.unpack_from('<hhHH',ROM,at+j*8)
     if not (x or y):break
     m=marker(x,y,typ,heading,b,'script',script=i+1,instruction=step,spawn_index=j)
     spawned.append(m['id'])
    else:raise AssertionError('Spawn list not terminated')
    entry['markers']=spawned
   if op in (9,10):break
   pc+=20
  else:raise AssertionError('Script not terminated')
  scripts.append(dict(id=i+1,ops=ops))
 movers=[];table=ptr(0x44dd4c+level*4)
 for i in range(ROM[0x44dd3c+level]):
  p=table+20*i;sector=u16(ROM,p);flags=u16(ROM,p+14);assert sector<len(sectors)
  movers.append(dict(id=i,label=f'D{i+1}',sector=sector,kind='floor lift' if flags&256 else 'ceiling door',flags=flags,locked=bool(flags&4),
   lower=s16(ROM,p+8),upper=s16(ROM,p+10),speed=u16(ROM,p+12),center=sectors[sector]['center'],
   scripts=[dict(script=s['id'],command={4:'open',5:'close',6:'unlock',7:'lock'}[o['op']]) for s in scripts for o in s['ops'] if o['op'] in (4,5,6,7) and o['args'][1]==i]))
 # Floor lifts are initialized from their mover endpoint before level objects.
 for d in movers:
  if d['flags']&256:
   sec=sectors[d['sector']];sec['decoded_floor']=sec['floor']
   sec['floor']=d['upper'] if d['flags']&1 else d['lower']
 for p in markers:p['floor']=sectors[p['sector']]['floor']
 switches=[]
 for l in lines:
  if l['kind']==2:
   a,b=verts[l['a']],verts[l['b']]
   switches.append(dict(id=len(switches)+1,line=l['id'],script=l['trigger'],center=[(a[0]+b[0])/2,(a[1]+b[1])/2]))
 exitscripts=[s['id'] for s in scripts if any(o['op']==9 for o in s['ops'])]
 for m in markers:
  if m['trigger']&256 and m['trigger']&255 in exitscripts:m['exit']=True;m['group']='exits';m['code']='X';m['name']='Level exit / ankh trigger'
  if m['group']=='respawn':m['schedule_item_ids']=sorted(set(u16(ROM,0x44d204+(level-12)*68+i*4+2) for i in range(16)))
 m=dict(level=level,title=text(48+level).title(),rom_sha256=hashlib.sha256(raw).hexdigest(),vertices=verts,lines=lines,sectors=sectors,
        markers=markers,movers=movers,switches=switches,scripts=scripts,exit_scripts=exitscripts,secrets=[],source=SOURCES.get(level),
        secret_status='No secret classification established; every room and item is still shown.')
 if level in (0,1,2,3):
  target={0:27,1:28,2:29,3:41}[level];matches=[p for p in markers if p['origin']=='map' and p['type']==target];assert len(matches)==1
  p=matches[0]
  notes={0:'Hidden shotgun. Exact position matches the only shotgun in this ROM map; identified as secret in the TAS notes.',
   1:'Hidden Tommygun. Exact position is the only Tommygun pickup in this ROM map; the weapon guide identifies this early pickup as a secret.',
   2:'Hidden rocket launcher. After the arena stairs, approach the central part of the right wall with flower decorations.',
   3:'Hidden health on the elevated ledge. After the second battle, take the left door, climb the corridor stairs and use the switch through the right-hand opening. Return to the health.'}
  m['secrets'].append(dict(id='S1',x=p['x'],y=p['y'],marker=p['id'],title=p['name'],note=notes[level],status='Guide identification + exact ROM pickup',source=SOURCES[level]))
  if level==3:
   m['secrets'][0]['access']=dict(point=[2432,-736],switch='B3',script=7,door='D9')
   m['secrets'][0]['note']='Elevated health. Use switch B3 (S1 access); ROM script 7 unlocks and opens D9 beside the ledge. Return to collect the health. The guide describes the switch through an opening after the corridor stairs.'
  m['secret_status']='Documented hidden pickup mapped to its exact ROM position.'
 if level in (6,7,9,10):m['secret_status']='The cited level guide lists no secrets. All items and geometry remain visible.'
 if level>=12:m['secret_status']='Multiplayer arena. Respawn markers show all scheduled pickup locations; secret status is unclassified.'
 return m

def scene(m):
 W,H=1760,1900;left,top,width,height=60,175,1660,1550
 pts=m['vertices']+[(p['x'],p['y']) for p in m['markers']]
 xmin,xmax=min(p[0] for p in pts),max(p[0] for p in pts);ymin,ymax=min(p[1] for p in pts),max(p[1] for p in pts)
 scale=min((width-90)/(xmax-xmin),(height-90)/(ymax-ymin));cx=(xmin+xmax)/2;cy=(ymin+ymax)/2
 def point(x,y):return [round(left+width/2+(x-cx)*scale,2),round(top+height/2-(y-cy)*scale,2)]
 art=[]
 def add(kind,layer='base',**kw):art.append(dict(kind=kind,layer=layer,**kw))
 def txt(x,y,value,size=20,color='#dfe7f2',**kw):add('text',x=x,y=y,text=str(value),size=size,color=color,**kw)
 add('rect',x=0,y=0,w=W,h=H,fill='#101822')
 txt(60,49,'SERIOUS SAM ADVANCE   /   EXPLORATION ATLAS',18,'#5ed8d0')
 txt(60,102,f'{m["level"]+1:02}  {m["title"]}',38,'#ffffff')
 txt(60,138,'Complete ROM geometry  |  +Y up, +X right  |  Select doors in the atlas for details  |  Coordinates in original map units',18,'#a1b5c9')
 for x in range(math.ceil(xmin/512)*512,xmax+1,512):
  a=point(x,ymin);b=point(x,ymax);add('line',points=[a,b],color='#23303e',stroke=1)
 for y in range(math.ceil(ymin/512)*512,ymax+1,512):add('line',points=[point(xmin,y),point(xmax,y)],color='#23303e',stroke=1)
 moving={d['sector']:d for d in m['movers']}
 for s in m['sectors']:
  hazard=s['floor_color']==255 or (m['level']>5 and s['floor_color']==35)
  fill='#633a41' if hazard else ('#58452c' if s['id'] in moving else '#334555' if s['ceiling']>s['floor'] else '#26323f')
  if s['loops']:add('path',loops=[[point(*p) for p in loop] for loop in s['loops']],fill=fill,ref=f'sector-{s["id"]}')
  if s['flags']&256:
   add('path','triggers',loops=[[point(*p) for p in loop] for loop in s['loops']],fill='none',color=COLORS['triggers'],stroke=2,ref=f'sector-{s["id"]}')
 for l in m['lines']:
  if l['flags']&4 and l['front']==l['back']:continue
  a,b=m['vertices'][l['a']],m['vertices'][l['b']]
  door=l['kind']==1;wall=not l['flags']&4
  add('line','doors' if door else 'geometry',points=[point(*a),point(*b)],color=COLORS['doors'] if door else '#c5d3e2' if wall else '#6b8196',stroke=4 if door else 2.3 if wall else 1,ref=f'line-{l["id"]}')
 for d in m['movers']:
  x,y=point(*d['center']);txt(x+4,y-5,d['label'],15,COLORS['doors'],layer='doors',ref=f'door-{d["id"]}')
 for sw in m['switches']:
  x,y=point(*sw['center']);add('rect','switches',x=x-6,y=y-6,w=12,h=12,fill=COLORS['switches'],ref=f'switch-{sw["id"]}')
  txt(x+10,y-8,f'B{sw["id"]}',16,COLORS['switches'],layer='switches',ref=f'switch-{sw["id"]}')
 # Merge only coincident same-kind markers for display; all records remain in JSON.
 grouped=defaultdict(list)
 for p in m['markers']:grouped[(p['x'],p['y'],p['group'],p['code'],p['type'])].append(p)
 icons={p['id']:sprite_for(p,m['level']) for p in m['markers']}
 for p in m['markers']:
  if icons[p['id']]:p['sprite']=icons[p['id']]['path']
 # Keep labels legible in dense ammo piles without moving any actual location.
 occupied=[]
 for (x,y,group,code,typ) in grouped:
  if group!='enemies':
   px,py=point(x,y);occupied.append((px-15,py-15,px+15,py+15))
 for d in m['movers']:
  px,py=point(*d['center']);occupied.append((px+2,py-22,px+6+len(d['label'])*9,py-2))
 for sec in m['secrets']:
  px,py=point(sec['x'],sec['y']);occupied.append((px-18,py-51,px+25,py-26))
  if 'access' in sec:
   px,py=point(*sec['access']['point']);occupied.append((px+16,py-30,px+103,py-9))
 def place_label(px,py,code):
  w=len(code)*8+4
  candidates=[(px+11,py+5),(px-w-11,py+5),(px-w/2,py-12),(px-w/2,py+26)]
  for radius in range(24,205,18):
   for angle in range(0,360,30):
    a=math.radians(angle);candidates.append((px+radius*math.cos(a)-w/2,py+radius*math.sin(a)+5))
  for x,y in candidates:
   rect=(x-2,y-15,x+w+2,y+3)
   if rect[0]<left or rect[2]>left+width or rect[1]<top or rect[3]>top+height:continue
   if not any(rect[0]<r[2] and rect[2]>r[0] and rect[1]<r[3] and rect[3]>r[1] for r in occupied):
    occupied.append(rect);return x,y
  return px+11,py+5
 for (x,y,group,code,typ),items in sorted(grouped.items(),key=lambda kv:kv[0][2]!='enemies'):
  p=items[0];px,py=point(x,y);layer='scripted-enemies' if group=='enemies' and p['origin']=='script' else group
  color=COLORS[group];icon=icons[p['id']]
  if icon:
   size=36 if group=='starts' else 30 if group=='enemies' else 26
   factor=size/max(icon['width'],icon['height']);iw=icon['width']*factor;ih=icon['height']*factor
   add('image',layer,x=px-iw/2,y=py-ih/2,w=iw,h=ih,path=icon['path'],uri=icon['uri'],ref=p['id'],ids=[q['id'] for q in items])
   if all(q['origin']=='script' for q in items):txt(px+iw/2+1,py-ih/2+5,'*',15,color,layer=layer,ref=p['id'])
  if not icon:txt(px-5,py+5,code,14,color,layer=layer,ref=p['id'])
  if group!='enemies' and icon:
   tx,ty=place_label(px,py,code)
   if abs(tx-px)>15 or abs(ty-py)>15:
    add('line',layer,points=[[px,py],[tx+len(code)*4,ty-5]],color=color,stroke=.65,ref=p['id'])
   txt(tx,ty,code,14,color,layer=layer,ref=p['id'])
 for sec in m['secrets']:
  x,y=point(sec['x'],sec['y']);add('circle','secrets',x=x,y=y,r=19,fill='none',color=COLORS['secrets'],stroke=3,ref=sec['id'])
  txt(x-16,y-28,sec['id'],21,COLORS['secrets'],layer='secrets',ref=sec['id'])
  if 'access' in sec:
   ax,ay=point(*sec['access']['point']);add('circle','secrets',x=ax,y=ay,r=16,fill='none',color=COLORS['secrets'],stroke=2,ref=sec['id']+'-access')
   txt(ax+18,ay-12,sec['id']+' access',16,COLORS['secrets'],layer='secrets',ref=sec['id']+'-access')
 # Sector exit triggers, including the pit ending in level 10.
 for s in m['sectors']:
  if s['flags']&256 and s['flags']&255 in m['exit_scripts']:
   x,y=point(*s['center']);txt(x,y,'EXIT',18,'#ffffff',layer='exits',ref=f'sector-{s["id"]}')
 txt(60,1795,f'{len(m["lines"])} lines  /  {len(m["sectors"])} sectors  /  {sum(p["origin"]=="map" for p in m["markers"])} map objects  /  {sum(p["origin"]=="script" for p in m["markers"])} scripted placements',18)
 txt(60,1830,'All rooms are revealed. Use index.html to zoom, filter layers, inspect items and trace door scripts.',18,'#a1b5c9')
 txt(60,1865,'Secret labels use cited guides + ROM positions, not an invented secret flag. Full evidence and sources are included.',16,'#a1b5c9')
 add('line',points=[[70,1735],[70+256*scale,1735]],color='#dfe7f2',stroke=4);txt(70,1760,'256 units',14)
 return dict(width=W,height=H,art=art,transform=dict(xmin=xmin,xmax=xmax,ymin=ymin,ymax=ymax,scale=scale))

def svg(sc):
 out=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{sc["width"]}" height="{sc["height"]}" viewBox="0 0 {sc["width"]} {sc["height"]}">',
 '<style>text{font-family:Arial,sans-serif;paint-order:stroke;stroke:#101822;stroke-width:2px;stroke-linejoin:round} [data-ref]{cursor:pointer} [data-layer="enemies"],[data-layer="scripted-enemies"],[data-layer="triggers"]{display:none}</style>']
 for p in sc['art']:
  attr=f' data-layer="{p["layer"]}"'+(f' data-ref="{p["ref"]}"' if 'ref' in p else '')
  attr+=f' fill="{p.get("fill","none")}" stroke="{p.get("color","none")}" stroke-width="{p.get("stroke",0)}"'
  if p['kind']=='text':
   attr=f' data-layer="{p["layer"]}"'+(f' data-ref="{p["ref"]}"' if 'ref' in p else '')
   s=f'<text{attr} x="{p["x"]}" y="{p["y"]}" fill="{p["color"]}" font-size="{p["size"]}">{html.escape(p["text"])}</text>'
  elif p['kind']=='rect':s=f'<rect{attr} x="{p["x"]}" y="{p["y"]}" width="{p["w"]}" height="{p["h"]}"/>'
  elif p['kind']=='circle':s=f'<circle{attr} cx="{p["x"]}" cy="{p["y"]}" r="{p["r"]}"/>'
  elif p['kind']=='image':s=f'<image{attr} x="{p["x"]}" y="{p["y"]}" width="{p["w"]}" height="{p["h"]}" href="{p["uri"]}" style="image-rendering:pixelated"/>'
  elif p['kind']=='line':s=f'<path{attr} d="M{p["points"][0][0]},{p["points"][0][1]} L{p["points"][1][0]},{p["points"][1][1]}"/>'
  elif p['kind']=='path':
   d=' '.join('M'+' L'.join(f'{x},{y}' for x,y in loop)+' Z' for loop in p['loops'])
   s=f'<path{attr} fill-rule="evenodd" d="{d}"/>'
  out.append(s)
 out.append('</svg>');return '\n'.join(out)

def main():
 OUT.mkdir(parents=True,exist_ok=True);(OUT/'svg').mkdir(exist_ok=True);(OUT/'data').mkdir(exist_ok=True);(OUT/'icons').mkdir(exist_ok=True)
 maps=[load(i) for i in range(16)]
 config=ROOT/'scripts/topmap_secrets.json'
 if config.exists():
  for key,info in json.loads(config.read_text()).items():maps[int(key)].update(info)
 scenes=[];svgs=[]
 for m in maps:
  sc=scene(m);s=svg(sc);scenes.append(sc);svgs.append(s)
  (OUT/'svg'/f'level-{m["level"]+1:02}.svg').write_text(s)
  (OUT/'data'/f'level-{m["level"]+1:02}.json').write_text(json.dumps(m,indent=2))
 (OUT/'render-scenes.json').write_text(json.dumps(scenes,separators=(',',':')))
 (OUT/'sprite-manifest.json').write_text(json.dumps({key:{k:v for k,v in s.items() if k!='uri'} for key,s in SPRITES.items()},indent=2))
 payload=json.dumps(dict(maps=maps,svgs=svgs,theme=theme_assets()),separators=(',',':')).replace('</',r'<\/')
 template=(ROOT/'scripts/topmap_viewer.html').read_text()
 javascript=(ROOT/'scripts/topmap_viewer.js').read_text().replace('__DATA__',payload)
 style=(ROOT/'scripts/topmap_viewer.css').read_text()
 (OUT/'index.html').write_text(template.replace('__STYLE__',style).replace('__SCRIPT__',javascript))
 report=dict(rom_sha1=SHA1,maps=16,lines=sum(len(m['lines']) for m in maps),sectors=sum(len(m['sectors']) for m in maps),
  initial_objects=sum(sum(p['origin']=='map' for p in m['markers']) for m in maps),scripted_placements=sum(sum(p['origin']=='script' for p in m['markers']) for m in maps),
  movers=sum(len(m['movers']) for m in maps),switches=sum(len(m['switches']) for m in maps),secrets=sum(len(m['secrets']) for m in maps),
  decoded_map_hashes_match_native_validation=True,all_bsp_leaves_processed=True,
  original_sprite_icons=len(SPRITES),sprite_markers=sum('sprite' in p for m in maps for p in m['markers']))
 (OUT/'validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 assert hashlib.sha1((ROOT/'orig/SSA.gba').read_bytes()).hexdigest()==SHA1
if __name__=='__main__':main()
