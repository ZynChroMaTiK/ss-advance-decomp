import * as T from './vendor/three.module.js';
import {OrbitControls} from './vendor/OrbitControls.js';
import {floorPositions,wallBuffers} from './geometry.js';
import {objectRecords,filterObjects,triggerRecord} from './viewer_objects.js';
import {bindFlightControls} from './flight_controls.js';
const $=id=>document.getElementById(id);
export function surfaceMaterial(options){return new T.MeshBasicMaterial({...options,side:T.FrontSide})}
export function triggerArrows(data,triggers,markers){
 const group=new T.Group();group.name='trigger-arrows';
 const scripts=new Map(data.scripts.map(s=>[s.id,new Set(s.ops.filter(o=>o.op===1).flatMap(o=>o.markers??[]))]));
 const targets=new Map(markers.children.filter(o=>o.userData.marker.origin==='script').map(o=>[o.userData.marker.id,o]));
 for(const trigger of triggers.children)for(const id of scripts.get(trigger.userData.trigger.flags&255)??[]){
  const target=targets.get(id);if(!target)continue;
  const arrow=new T.ArrowHelper(new T.Vector3(0,1,0),new T.Vector3(),1,0x9affae,.5,.3);
  // ArrowHelper normally shares geometry; each level owns and disposes its copies.
  for(const part of [arrow.line,arrow.cone]){
   part.geometry=part.geometry.clone();part.material.transparent=true;part.material.opacity=.9;part.material.depthWrite=false;
   part.renderOrder=3;part.raycast=()=>{}; // Annotations must not intercept object picking.
  }
  arrow.userData={trigger,target};group.add(arrow);
 }
 updateTriggerArrows(group,triggers,markers);return group;
}
export function updateTriggerArrows(group,triggers,markers){
 group.visible=triggers.visible&&markers.visible;
 for(const arrow of group.children){
  const {trigger,target}=arrow.userData,s=trigger.userData.trigger;
  arrow.position.set(s.center[0]/64,trigger.position.y+trigger.scale.y/2,-s.center[1]/64);
  // Keep a direction even when a placement coincides with the sector center.
  if(arrow.position.distanceToSquared(target.position)<1e-8){
   const p=s.mesh_loops[0][0];arrow.position.x=(s.center[0]+p[0])/128;arrow.position.z=-(s.center[1]+p[1])/128;
  }
  const direction=target.position.clone().sub(arrow.position),length=direction.length();
  arrow.visible=trigger.visible&&target.visible&&length>1e-6;
  if(length>1e-6){arrow.setDirection(direction.divideScalar(length));arrow.setLength(length,Math.min(.65,length*.25),Math.min(.4,length*.15))}
 }
}
export function entryTrigger(sector,data){
 if(!(sector.flags&256))return null;
 const group=new T.Group(),geometry=new T.BufferGeometry(),positions=[],edges=[],corners=new Map();
 // Unit-height closed volume; scaling follows the displayed ceiling height.
 // Caps face outward: downward at the floor, upward at the ceiling.
 const cap=floorPositions(sector.mesh_loops,0);
 for(let i=0;i<cap.length;i+=9){
  const a=cap.slice(i,i+3),b=cap.slice(i+3,i+6),c=cap.slice(i+6,i+9);
  positions.push(...a,...c,...b,...[a,b,c].flatMap(p=>[p[0],1,p[2]]));
 }
 // Original sector boundaries exclude internal BSP seams and same-sector lines.
 for(const line of data.lines)if((line.front===sector.id)!==(line.back===sector.id)){
  const ids=line.front===sector.id?[line.b,line.a]:[line.a,line.b];
  const [a,b]=ids.map(id=>{const [x,y]=data.vertices[id],p=[x/64,0,-y/64];corners.set(`${x},${y}`,p);return p});
  if(a[0]===b[0]&&a[2]===b[2])continue;
  const at=[a[0],1,a[2]],bt=[b[0],1,b[2]];
  positions.push(...a,...b,...bt,...a,...bt,...at);
  edges.push(...a,...b,...at,...bt);
 }
 for(const p of corners.values())edges.push(...p,p[0],1,p[2]);
 geometry.setAttribute('position',new T.Float32BufferAttribute(positions,3));
 const fill=new T.Mesh(geometry,surfaceMaterial({color:0x00ff66,transparent:true,opacity:.32,depthWrite:false,polygonOffset:true,polygonOffsetFactor:-1,polygonOffsetUnits:-1}));
 fill.userData={trigger:sector};fill.renderOrder=1;group.add(fill);
 const borderGeometry=new T.BufferGeometry();borderGeometry.setAttribute('position',new T.Float32BufferAttribute(edges,3));
 const border=new T.LineSegments(borderGeometry,new T.LineBasicMaterial({color:0x000000,transparent:true,opacity:1,depthWrite:false}));
 border.renderOrder=2;group.add(border);group.userData={trigger:sector};
 group.position.y=sector.initial_floor/64;group.scale.y=(sector.initial_ceiling-sector.initial_floor)/64;
 return group;
}
export function sectorMeshes(sector){
 return [['floor',sector.initial_floor,sector.floor_rgb],['ceiling',sector.initial_ceiling,sector.ceiling_rgb]].filter(([kind])=>kind==='floor'||sector.ceiling_color!==0).map(([kind,height,color])=>{
  const geometry=new T.BufferGeometry();geometry.setAttribute('position',new T.Float32BufferAttribute(floorPositions(sector.mesh_loops,height),3));
  // Floors face up; ceilings face down into the room.
  if(kind==='ceiling'){const indices=[];for(let i=0;i<geometry.attributes.position.count;i+=3)indices.push(i,i+2,i+1);geometry.setIndex(indices)}
  const object=new T.Mesh(geometry,surfaceMaterial({color}));object.userData={sector,kind,openOffset:kind==='ceiling'?(sector.open_ceiling-sector.initial_ceiling)/64:0};return object;
 });
}
export function createViewer(makeRenderer=()=>new T.WebGLRenderer({antialias:true,alpha:true})){
 const host=$('viewport'),message=$('view-message'),scene=new T.Scene(),camera=new T.PerspectiveCamera(48,1,.08,3000),renderer=makeRenderer();
 renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.outputColorSpace=T.SRGBColorSpace;host.prepend(renderer.domElement);renderer.domElement.tabIndex=0;renderer.domElement.setAttribute('aria-label','3D level. Drag to rotate, scroll to zoom. Use the view controls for free flight.');
 const controls=new OrbitControls(camera,renderer.domElement);controls.enableDamping=true;controls.dampingFactor=.09;controls.maxPolarAngle=Math.PI*.97;controls.minDistance=.4;controls.maxDistance=600;
 const ray=new T.Raycaster(),pointer=new T.Vector2(),loader=new T.TextureLoader(),cache=new Map(),keys=new Set();let root=null,data=null,index=-1,token=0,active=false,fly=false,drag=null,last=0,span=100,center=new T.Vector3(),groups={},lost=false;
 let touchKeys=new Set(),records=[];const touch=bindFlightControls([...document.querySelectorAll('[data-flight-key]')],value=>touchKeys=value,()=>active&&fly&&!lost);const held=code=>keys.has(code)||touchKeys.has(code);
 function texture(path){if(!cache.has(path))cache.set(path,loader.loadAsync(path).then(t=>{t.colorSpace=T.SRGBColorSpace;t.magFilter=T.NearestFilter;t.minFilter=T.NearestFilter;t.generateMipmaps=false;t.wrapS=t.wrapT=T.RepeatWrapping;return t}).catch(e=>{cache.delete(path);throw Error(`Asset could not load: ${path}`)}));return cache.get(path)}
 function mesh(positions,material,uvs,indices){const g=new T.BufferGeometry();g.setAttribute('position',new T.Float32BufferAttribute(positions,3));if(uvs)g.setAttribute('uv',new T.Float32BufferAttribute(uvs,2));if(indices)g.setIndex(indices);g.computeBoundingSphere();return new T.Mesh(g,material)}
 function dispose(g){g?.traverse(o=>{if(o.geometry)o.geometry.dispose();if(o.material)o.material.dispose()})}
 function setFlight(value,focusCanvas=true){fly=value;keys.clear();touch.clear();$('flight-controls').hidden=!fly;controls.enabled=!fly;$('view-fly').setAttribute('aria-pressed',String(fly));$('view-hint').textContent=fly?'Drag to look · WASD move · Q/E down/up · Shift faster · touch buttons move · Esc to return':'Drag to rotate · scroll to zoom · right-drag to pan';if(focusCanvas)renderer.domElement.focus({preventScroll:true})}
 function reset(top=false,focusCanvas=true){setFlight(false,focusCanvas);controls.target.copy(center);camera.up.set(0,1,0);const distance=span*.82/Math.sin(Math.atan(Math.tan(T.MathUtils.degToRad(camera.fov/2))*Math.min(camera.aspect,1)));controls.maxDistance=Math.max(600,span*10);camera.position.copy(center).add((top?new T.Vector3(0,1,.0001):new T.Vector3(.62,.85,.8)).normalize().multiplyScalar(distance));camera.lookAt(center);controls.update()}
 function focus(x,y,h=0,focusCanvas=true){setFlight(false,focusCanvas);const p=new T.Vector3(x/64,h/64,-y/64);controls.target.copy(p);camera.position.copy(p).add(new T.Vector3(8,13,12));controls.update()}
 function showInfo(title,fields,note,source){const dest=$('selection');dest.replaceChildren();const h=document.createElement('h3');h.textContent=title;dest.append(h);const dl=document.createElement('dl');for(const [k,v] of Object.entries(fields)){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=({Sector:"Room section",Sectors:"Room sections",Placements:"Enemies and items",Script:"Event",Flags:"Internal flags",Group:"Category"})[k]||k;dd.textContent=String(v);dl.append(dt,dd)}dest.append(dl);if(note){const p=document.createElement('p');p.textContent=note;dest.append(p)}if(source&&/^https:\/\//.test(source)){const a=document.createElement('a');a.href=source;a.textContent='Read the guide ↗';a.target='_blank';a.rel='noopener';dest.append(a)}}
 function secretInfo(s){const m=data.markers.find(m=>m.id===s.marker);showInfo(s.title,{Location:s.id,X:s.x,Y:s.y,Status:s.status},s.note,s.source);focus(s.x,s.y,m?.initial_floor??0);if(s.access?.point){const button=document.createElement('button');button.className='secret-button';button.textContent='Show opening trigger ↗';button.onclick=()=>{focus(...s.access.point,m?.initial_floor??0);showInfo('Secret access',s.access,s.note,s.source)};$('selection').append(button)}}
 function settings(){if(!root)return;const opened=$('open-doors').checked;groups.closed.visible=!opened&&$('show-walls').checked;groups.open.visible=opened&&$('show-walls').checked;groups.floors.visible=$('show-floors').checked;groups.ceilings.visible=$('show-ceilings').checked;groups.ceilings.children.forEach(o=>{o.position.y=opened?o.userData.openOffset:0});groups.markers.children.forEach(o=>{const m=o.userData.marker;const enemy=m.group.includes('enemies');o.visible=(enemy?$('show-enemies').checked:$('show-items').checked)&&(m.origin==='map'||$('show-scripted').checked)});groups.secrets.visible=$('show-secrets').checked;groups.triggers.visible=$('show-triggers').checked;groups.triggers.children.forEach(o=>{const s=o.userData.trigger;o.scale.y=((opened?s.open_ceiling:s.initial_ceiling)-s.initial_floor)/64});updateTriggerArrows(groups.arrows,groups.triggers,groups.markers);for(const name of ['floors','ceilings','closed','open'])groups[name].traverse(o=>{if(o.material)o.material.wireframe=$('show-wireframe').checked})}
 function levelInfo(){records=objectRecords(data,$('open-doors').checked);renderObjects();$('view-name').textContent=`${String(index+1).padStart(2,'0')} / ${data.title}`;$('secret-list').replaceChildren();if(!data.secrets.length)$('secret-list').textContent='No secrets marked for this level yet.';for(const s of data.secrets){const button=document.createElement('button');button.className='secret-button';button.textContent=`${s.id} / ${s.title} ↗`;button.onclick=()=>secretInfo(s);$('secret-list').append(button)}showInfo(data.title,{Sectors:data.sectors.length,Placements:data.markers.length,Secrets:data.secrets.length},'Select a room, wall, enemy or item to learn more.');}
 async function load(n,entry){const mine=++token;if(index===n&&root){root.visible=true;levelInfo();message.hidden=true;return}if(root)root.visible=false;keys.clear();touch.clear();records=[];renderObjects();$('selection').replaceChildren();$('secret-list').replaceChildren();$('view-name').textContent='Loading selected level…';message.hidden=false;message.textContent='Loading the view…';let next=null;try{const response=await fetch(entry?.path??`data/level-${String(n+1).padStart(2,'0')}.json`);if(!response.ok)throw Error(`Level data unavailable (${response.status})`);const d=await response.json();if(mine!==token)return;
   const paths=new Set([...d.walls.initial,...d.walls.open].map(f=>f.texture).filter(Boolean));d.markers.forEach(m=>{if(m.sprite)paths.add('atlas/'+m.sprite)});const textures=new Map(await Promise.all([...paths].map(async p=>[p,await texture(p)])));if(mine!==token)return;
   next=new T.Group();const gs={};for(const name of ['floors','ceilings','closed','open','markers','secrets','triggers']){gs[name]=new T.Group();gs[name].name=name;next.add(gs[name])}
   for(const s of d.sectors){const [floor,ceiling]=sectorMeshes(s);gs.floors.add(floor);if(ceiling)gs.ceilings.add(ceiling);const trigger=entryTrigger(s,d);if(trigger)gs.triggers.add(trigger)}
   for(const [state,name] of [['initial','closed'],['open','open']]){const batches=new Map();for(const f of d.walls[state]){const key=f.texture??'';if(!batches.has(key))batches.set(key,[]);batches.get(key).push(f)}for(const [path,faces]of batches){const b=wallBuffers(faces),o=mesh(b.positions,surfaceMaterial({map:textures.get(path)||null,color:path?0xffffff:0x000000}),b.uvs,b.indices);o.userData={faces};gs[name].add(o)}}
   // Every placement is retained, including co-located and scripted records.
   if(!cache.has('respawn-symbol')){const c=document.createElement('canvas');c.width=c.height=64;const ctx=c.getContext('2d');ctx.fillStyle='#18271d';ctx.fillRect(4,4,56,56);ctx.strokeStyle='#b3d6a4';ctx.lineWidth=3;ctx.strokeRect(4,4,56,56);ctx.fillStyle='#ecf5df';ctx.font='bold 40px monospace';ctx.textAlign='center';ctx.fillText('R',32,46);const t=new T.CanvasTexture(c);t.colorSpace=T.SRGBColorSpace;t.magFilter=T.NearestFilter;cache.set('respawn-symbol',Promise.resolve(t))}const respawn=await cache.get('respawn-symbol');
   for(const m of d.markers){const t=m.sprite?textures.get('atlas/'+m.sprite):respawn,isActor=m.group==='starts'||m.group.includes('enemies'),height=isActor?2.2:1.25,o=new T.Sprite(new T.SpriteMaterial({side:T.FrontSide,map:t,transparent:true,alphaTest:.15,depthWrite:false}));o.scale.set(height*t.image.width/t.image.height,height,1);o.position.set(m.x/64,m.initial_floor/64+height/2+.03,-m.y/64);o.userData={marker:m};gs.markers.add(o)}
   gs.arrows=triggerArrows(d,gs.triggers,gs.markers);next.add(gs.arrows);
   for(const s of d.secrets){const m=d.markers.find(m=>m.id===s.marker),h=(m?.initial_floor??0)/64;const o=new T.Mesh(new T.CylinderGeometry(.15,.15,6,8),surfaceMaterial({color:0xff7bd5,transparent:true,opacity:.8}));o.position.set(s.x/64,h+3,-s.y/64);o.userData={secret:s};gs.secrets.add(o);const ring=new T.Mesh(new T.TorusGeometry(1,.1,6,24),surfaceMaterial({color:0xff7bd5}));ring.rotation.x=Math.PI/2;ring.position.set(s.x/64,h+.12,-s.y/64);ring.userData={secret:s};gs.secrets.add(ring)}
   if(mine!==token){dispose(next);return}if(root){scene.remove(root);dispose(root)}root=next;groups=gs;data=d;index=n;scene.add(root);const b=entry.bounds;span=Math.max(b.max_x-b.min_x,b.max_y-b.min_y)/64;center.set((b.min_x+b.max_x)/128,b.min_height/64,-(b.min_y+b.max_y)/128);camera.far=Math.max(2000,span*15);camera.updateProjectionMatrix();resize();reset(false,false);settings();levelInfo();message.hidden=true;resize();
  }catch(e){if(next&&next!==root)dispose(next);if(mine===token){message.hidden=false;message.textContent=`Unable to load this level: ${e.message}. Choose another level or switch to the 2D map.`}}}

 function renderObjects(){
  const list=$('object-results'),old=list.value;list.replaceChildren();
  const filtered=filterObjects(records,$('object-search').value||'',$('object-kind').value||'all');
  for(const record of filtered){const option=document.createElement('option');option.value=record.id;option.textContent=record.label;list.append(option)}
  list.value=filtered.some(r=>r.id===old)?old:(filtered[0]?.id??'');list.disabled=!filtered.length;$('object-inspect').disabled=!filtered.length;
  $('object-count').textContent=`${filtered.length} of ${records.length} results`;
 }
 function inspectObject(){
  const record=records.find(r=>r.id===$('object-results').value);if(!record||!root?.visible)return;
  focus(...record.point,false);showInfo(record.label,record.fields,record.note||record.secret?.note||'Use View settings to show any hidden items or parts of the map.',record.secret?.source);
 }
 $('object-search').addEventListener('input',renderObjects);$('object-kind').addEventListener('change',renderObjects);$('object-inspect').onclick=inspectObject;
 $('object-results').addEventListener('keydown',e=>{if(e.key==='Enter'||e.code==='Enter'||e.code==='NumpadEnter'){e.preventDefault();inspectObject()}});
 function resize(){const w=host.clientWidth,h=host.clientHeight;if(w&&h){renderer.setSize(w,h,false);camera.aspect=w/h;camera.updateProjectionMatrix()}}new ResizeObserver(resize).observe(host);
 function zoom(factor){if(!root||!root.visible)return;setFlight(false);const offset=camera.position.clone().sub(controls.target);offset.setLength(Math.max(controls.minDistance,Math.min(controls.maxDistance,offset.length()*factor)));camera.position.copy(controls.target).add(offset);controls.update()}
 $('view-zoom-in').onclick=()=>zoom(.8);$('view-zoom-out').onclick=()=>zoom(1.25);$('view-fit').onclick=()=>reset();$('view-top').onclick=()=>reset(true);$('view-fly').onclick=()=>setFlight(!fly);$('view-start').onclick=()=>{const m=data?.markers.find(m=>m.group==='starts');if(!m)return;setFlight(true);camera.position.set(m.x/64,m.initial_floor/64+1.25,-m.y/64);const yaw=m.heading/65536*Math.PI*2;camera.lookAt(camera.position.clone().add(new T.Vector3(Math.cos(yaw),0,-Math.sin(yaw))))};document.querySelectorAll('.view-options input').forEach(el=>el.onchange=()=>{settings();if(data&&root?.visible){records=objectRecords(data,$('open-doors').checked);renderObjects()}});
 const canvas=renderer.domElement;controls.listenToKeyEvents(canvas);canvas.addEventListener('contextmenu',e=>e.preventDefault());canvas.addEventListener('pointerdown',e=>{drag={x:e.clientX,y:e.clientY,startX:e.clientX,startY:e.clientY};canvas.focus({preventScroll:true});if(fly)canvas.setPointerCapture(e.pointerId)});canvas.addEventListener('pointermove',e=>{if(!drag||!fly)return;const euler=new T.Euler().setFromQuaternion(camera.quaternion,'YXZ');euler.y-=(e.clientX-drag.x)*.004;euler.x=Math.max(-1.52,Math.min(1.52,euler.x-(e.clientY-drag.y)*.004));camera.quaternion.setFromEuler(euler);drag.x=e.clientX;drag.y=e.clientY});
 canvas.addEventListener('pointerup',e=>{if(!drag)return;const click=Math.hypot(e.clientX-drag.startX,e.clientY-drag.startY)<5;drag=null;if(!click||!root)return;const r=canvas.getBoundingClientRect();pointer.set((e.clientX-r.left)/r.width*2-1,-(e.clientY-r.top)/r.height*2+1);ray.setFromCamera(pointer,camera);const visible=[];root.traverseVisible(o=>{if(o.isMesh||o.isSprite)visible.push(o)});const hit=ray.intersectObjects(visible,false)[0];if(!hit)return;const u=hit.object.userData;if(u.secret)secretInfo(u.secret);else if(u.marker){const m=u.marker;showInfo(m.name,{ID:m.id,X:m.x,Y:m.y,Sector:m.sector,Placement:m.origin,Group:m.group},'Marks a location in the original game. Some enemies and items appear later.')}else if(u.faces){const f=u.faces[Math.floor(hit.faceIndex/2)];showInfo(`Wall ${f.line}`,{Sector:f.sector,Layer:f.layer,Door:f.door?'Yes':'No',Texture:f.texture?.split('/').pop()??'Absent'})}else if(u.trigger){const r=triggerRecord(u.trigger);showInfo(r.label,r.fields,r.note)}else if(u.sector){const s=u.sector;showInfo(`Sector ${s.id}`,{Floor:s.initial_floor,Ceiling:$('open-doors').checked?s.open_ceiling:s.initial_ceiling,Flags:s.flags},'Heights are in original map units.')}});
 canvas.addEventListener('pointercancel',()=>drag=null);canvas.addEventListener('keydown',e=>{if(e.code==='Escape'&&fly){const dir=new T.Vector3();camera.getWorldDirection(dir);controls.target.copy(camera.position).addScaledVector(dir,10);setFlight(false)}if(fly&&['KeyW','KeyA','KeyS','KeyD','KeyQ','KeyE','ShiftLeft','ShiftRight'].includes(e.code)){e.preventDefault();keys.add(e.code)}});window.addEventListener('keyup',e=>keys.delete(e.code));window.addEventListener('blur',()=>{keys.clear();touch.clear();drag=null});document.addEventListener('visibilitychange',()=>{if(document.hidden){keys.clear();touch.clear();drag=null}});canvas.addEventListener('blur',()=>keys.clear());
 canvas.addEventListener('webglcontextlost',e=>{e.preventDefault();lost=true;keys.clear();touch.clear();message.hidden=false;message.textContent='The 3D view stopped working. Reload the page to try again.'});
 function frame(now){requestAnimationFrame(frame);const dt=Math.min((now-last)/1000,.05);last=now;if(!active||document.hidden||lost)return;if(fly){const speed=dt*(held('ShiftLeft')||held('ShiftRight')?30:9);camera.translateX(((held('KeyD')?1:0)-(held('KeyA')?1:0))*speed);camera.translateZ(((held('KeyS')?1:0)-(held('KeyW')?1:0))*speed);camera.position.y+=((held('KeyE')?1:0)-(held('KeyQ')?1:0))*speed}else controls.update();renderer.render(scene,camera)}requestAnimationFrame(frame);
 return {load,setActive(value){active=value;keys.clear();touch.clear();if(value)resize()}};
}
