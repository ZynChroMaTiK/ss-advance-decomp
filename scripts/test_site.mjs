import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import * as THREE from '../site/vendor/three.module.js';
import {OrbitControls} from '../site/vendor/OrbitControls.js';
import {floorPositions,wallBuffers} from '../site/geometry.js';
import {sectorMeshes,surfaceMaterial,entryTrigger,triggerArrows,updateTriggerArrows} from '../site/viewer3d.js';
assert.equal(THREE.REVISION,'180');assert.equal(typeof OrbitControls,'function');
const catalog=JSON.parse(readFileSync(new URL('../site/data/catalog.json',import.meta.url)));
let meshes=0,triangles=0,skyCeilingsSkipped=0,wallSideChecks=0;
let triggers=0;
let arrowCount=0;
for(const entry of catalog){const d=JSON.parse(readFileSync(new URL('../site/'+entry.path,import.meta.url)));
 const tg=new THREE.Group(),mg=new THREE.Group();
 for(const s of d.sectors){const t=entryTrigger(s,d);if(t)tg.add(t)}
 for(const m of d.markers){const target=new THREE.Object3D();target.userData.marker=m;target.position.set(m.x/64,m.initial_floor/64+(m.group.includes('enemies')?1.1:.625)+.03,-m.y/64);mg.add(target)}
 const arrows=triggerArrows(d,tg,mg);
 const expected=d.sectors.filter(s=>s.flags&256).flatMap(s=>d.markers.filter(m=>m.origin==='script'&&m.script===(s.flags&255)).map(m=>`${s.id}:${m.id}`));
 assert.deepEqual(arrows.children.map(a=>`${a.userData.trigger.userData.trigger.id}:${a.userData.target.userData.marker.id}`).sort(),expected.sort());
 arrowCount+=arrows.children.length;
 for(const open of [false,true]){
  tg.children.forEach(t=>{const s=t.userData.trigger;t.scale.y=((open?s.open_ceiling:s.initial_ceiling)-s.initial_floor)/64});
  updateTriggerArrows(arrows,tg,mg);arrows.updateMatrixWorld(true);
  for(const arrow of arrows.children){
   assert(arrow.visible);assert(arrow.position.toArray().every(Number.isFinite));
   assert(arrow.cone.localToWorld(new THREE.Vector3()).distanceTo(arrow.userData.target.position)<1e-6,'arrowhead ends at target');
   assert.equal(arrow.cone.material.side,THREE.FrontSide);
   const hits=[];arrow.cone.raycast(new THREE.Raycaster(),hits);assert.equal(hits.length,0,'arrow cannot steal object picking');
  }
 }
 for(const g of [arrows,tg])g.traverse(o=>{o.geometry?.dispose();o.material?.dispose()});
 for(const s of d.sectors){
  const trigger=entryTrigger(s,d);
  if(!(s.flags&256)){assert.equal(trigger,null);continue}
  triggers++;const [fill,border]=trigger.children;
  assert.equal(fill.material.color.getHex(),0x00ff66);assert.equal(fill.material.opacity,.32);
  assert(fill.material.transparent&&!fill.material.depthWrite&&fill.material.depthTest);
  assert.equal(fill.material.side,THREE.FrontSide);assert.equal(fill.userData.trigger,s);
  assert.equal(border.material.color.getHex(),0);assert(border.material.depthTest);
  assert(border.renderOrder>fill.renderOrder);
  const p=fill.geometry.attributes.position;
  const boundary=d.lines.filter(l=>(l.front===s.id)!==(l.back===s.id));
  const corners=new Set(boundary.flatMap(l=>[l.a,l.b]).map(id=>d.vertices[id].join(',')));
  assert.equal(p.count,2*floorPositions(s.mesh_loops,0).length/3+boundary.length*6);
  assert.equal(border.geometry.attributes.position.count,boundary.length*4+corners.size*2);
  assert.equal(trigger.position.y,s.initial_floor/64);
  assert.equal(trigger.scale.y,(s.initial_ceiling-s.initial_floor)/64);
  // Check a unit-height volume, including initially closed door sectors.
  trigger.scale.y=1;trigger.updateMatrixWorld(true);
  let volume=0,area=0;
  for(const loop of s.mesh_loops)for(let i=1;i<loop.length-1;i++){
   const [a,b,c]=[loop[0],loop[i],loop[i+1]];
   area+=((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/8192;
  }
  for(let i=0;i<p.count;i+=3){
   const [a,b,c]=[0,1,2].map(j=>new THREE.Vector3().fromBufferAttribute(p,i+j));
   const n=b.clone().sub(a).cross(c.clone().sub(a)).normalize();assert(n.length()>.99);
   volume+=a.dot(b.clone().cross(c))/6;
   const center=a.clone().add(b).add(c).divideScalar(3).applyMatrix4(fill.matrixWorld);
   for(const side of [-1,1]){
    const hits=new THREE.Raycaster(center.clone().addScaledVector(n,.001*side),n.clone().multiplyScalar(-side),0,.002).intersectObject(fill);
    assert.equal(hits.length>0,side===1,`trigger ${entry.level}/${s.id} face ${i/3} culling`);
   }
  }
  assert(Math.abs(volume-area)<.001,`trigger ${entry.level}/${s.id} encloses sector area with outward faces`);
  trigger.traverse(o=>{o.geometry?.dispose();o.material?.dispose()});
 }
 for(const s of d.sectors){const pair=sectorMeshes(s),sky=s.ceiling_color===0;if(sky)skyCeilingsSkipped++;assert.deepEqual(pair.map(m=>m.userData.kind),sky?['floor']:['floor','ceiling']);for(const m of pair){assert.equal(m.material.side,THREE.FrontSide);assert.equal(m.geometry.attributes.position.count,floorPositions(s.mesh_loops,s.initial_floor).length/3);m.updateMatrixWorld();const attr=m.geometry.attributes.position,ids=m.geometry.index?Array.from(m.geometry.index.array):Array.from({length:attr.count},(_,i)=>i);
 for(let i=0;i<ids.length;i+=3){const [a,b,c]=ids.slice(i,i+3).map(n=>new THREE.Vector3().fromBufferAttribute(attr,n));const normal=b.clone().sub(a).cross(c.clone().sub(a));assert(m.userData.kind==='floor'?normal.y>0:normal.y<0,`level ${entry.level} ${m.userData.kind} faces room`)}
 const center=new THREE.Vector3();for(const n of ids.slice(0,3))center.add(new THREE.Vector3().fromBufferAttribute(attr,n));center.divideScalar(3);
 for(const side of [-1,1]){const ray=new THREE.Raycaster(center.clone().add(new THREE.Vector3(0,side,0)),new THREE.Vector3(0,-side,0));assert.equal(ray.intersectObject(m).length>0,m.userData.kind==='floor'?side===1:side===-1)}
 m.geometry.dispose();m.material.dispose()}}
 const buffers=[...d.sectors.flatMap(s=>(s.ceiling_color===0?[s.initial_floor]:[s.initial_floor,s.initial_ceiling]).map(h=>({positions:floorPositions(s.mesh_loops,h)}))),wallBuffers(d.walls.initial),wallBuffers(d.walls.open)];
 for(const b of buffers){const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(b.positions,3));if(b.indices)g.setIndex(b.indices);if(b.uvs)g.setAttribute('uv',new THREE.Float32BufferAttribute(b.uvs,2));g.computeBoundingBox();g.computeBoundingSphere();assert(Number.isFinite(g.boundingSphere.radius));assert(g.boundingSphere.radius>0);triangles+=(b.indices?.length??b.positions.length/3)/3;meshes++;g.dispose()}
 // Real wall fronts are visible and backs are culled in closed/open door states.
 for(const state of ['initial','open']){
  const b=wallBuffers([d.walls[state][0]]),g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(b.positions,3));g.setIndex(b.indices);
  const wall=new THREE.Mesh(g,surfaceMaterial({color:0xffffff}));wall.updateMatrixWorld();
  const [a,c,e]=b.indices.slice(0,3).map(i=>new THREE.Vector3().fromArray(b.positions,i*3));
  const center=a.clone().add(c).add(e).divideScalar(3),normal=c.clone().sub(a).cross(e.clone().sub(a)).normalize();
  for(const side of [-1,1]){const ray=new THREE.Raycaster(center.clone().addScaledVector(normal,side),normal.clone().multiplyScalar(-side));assert.equal(ray.intersectObject(wall).length>0,side===1,`level ${entry.level} ${state} wall side ${side}`);wallSideChecks++}
  g.dispose();wall.material.dispose();
 }
 // Orbit reset fits the map bounds on both a phone and desktop aspect ratio.
 const b=entry.bounds,span=Math.max(b.max_x-b.min_x,b.max_y-b.min_y)/64;
 for(const aspect of [.6,1,1.8]){const camera=new THREE.PerspectiveCamera(48,aspect,.08,3000),center=new THREE.Vector3((b.min_x+b.max_x)/128,b.min_height/64,-(b.min_y+b.max_y)/128),distance=span*.82/Math.sin(Math.atan(Math.tan(THREE.MathUtils.degToRad(camera.fov/2))*Math.min(aspect,1)));camera.position.copy(center).add(new THREE.Vector3(.62,.85,.8).normalize().multiplyScalar(distance));camera.lookAt(center);camera.updateMatrixWorld();for(const x of [b.min_x,b.max_x])for(const y of [b.min_y,b.max_y]){const p=new THREE.Vector3(x/64,b.min_height/64,-y/64).project(camera);assert(Math.abs(p.x)<1&&Math.abs(p.y)<1,`map ${entry.level} camera fit at aspect ${aspect}`)}}
}
// Actual viewer materials must reject intersections from the back side.
const surface=new THREE.Mesh(new THREE.PlaneGeometry(2,2),surfaceMaterial({color:0xffffff}));surface.updateMatrixWorld();
for(const z of [-1,1]){const ray=new THREE.Raycaster(new THREE.Vector3(0,0,z),new THREE.Vector3(0,0,-z));assert.equal(ray.intersectObject(surface).length>0,z===1)}
surface.geometry.dispose();surface.material.dispose();
const page=readFileSync(new URL('../site/index.html',import.meta.url),'utf8');
for(const id of ['show-floors','show-ceilings','show-walls','show-items','show-enemies','show-scripted','show-secrets'])assert(new RegExp(`id="${id}"[^>]*checked`).test(page),`${id} defaults visible`);
assert(skyCeilingsSkipped>0);
assert.equal(triggers,53);assert(/id="show-triggers"[^>]*checked/.test(page));
assert(arrowCount>0);console.log(JSON.stringify({status:'PASS',trigger_arrows:arrowCount,arrow_checks:'exact script-to-placement links on all maps, both door states, arrowhead direction, non-intercepting picking'}));
console.log(JSON.stringify({status:'PASS',entry_triggers:triggers,trigger_checks:'original sector boundaries, transparent green fill, black borders, closed volumes, floor/ceiling heights, outward faces, front-side picking, default visibility'}));
console.log(JSON.stringify({status:'PASS',three_revision:THREE.REVISION,levels:catalog.length,meshes,triangles,camera_fits:48,sky_ceilings_skipped:skyCeilingsSkipped,indoor_ceilings_retained:true,front_side_raycast:'PASS',wall_culling_rays:wallSideChecks,all_layers_visible_by_default:true}));
