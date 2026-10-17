import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import * as THREE from '../site/vendor/three.module.js';
import {OrbitControls} from '../site/vendor/OrbitControls.js';
import {floorPositions,wallBuffers} from '../site/geometry.js';
import {sectorMeshes,surfaceMaterial,entryTrigger} from '../site/viewer3d.js';
assert.equal(THREE.REVISION,'180');assert.equal(typeof OrbitControls,'function');
const catalog=JSON.parse(readFileSync(new URL('../site/data/catalog.json',import.meta.url)));
let meshes=0,triangles=0,skyCeilingsSkipped=0,wallSideChecks=0;
let triggers=0;
for(const entry of catalog){const d=JSON.parse(readFileSync(new URL('../site/'+entry.path,import.meta.url)));
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
  assert.equal(p.count,floorPositions(s.mesh_loops,s.initial_floor).length/3);
  for(let i=0;i<p.count;i++)assert.equal(p.getY(i),(s.initial_floor+1)/64);
  const boundary=d.lines.filter(l=>(l.front===s.id)!==(l.back===s.id));
  assert(boundary.length>0);assert.equal(border.geometry.attributes.position.count,boundary.length*2);
  assert.deepEqual(Array.from(border.geometry.attributes.position.array),boundary.flatMap(l=>[l.a,l.b].flatMap(id=>[d.vertices[id][0]/64,(s.initial_floor+1.25)/64,-d.vertices[id][1]/64])));
  fill.updateMatrixWorld();const center=new THREE.Vector3();
  for(let i=0;i<3;i++)center.add(new THREE.Vector3().fromBufferAttribute(p,i));center.divideScalar(3);
  for(const side of [-1,1])assert.equal(new THREE.Raycaster(center.clone().add(new THREE.Vector3(0,side,0)),new THREE.Vector3(0,-side,0)).intersectObject(fill).length>0,side===1);
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
console.log(JSON.stringify({status:'PASS',entry_triggers:triggers,trigger_checks:'original sector boundaries, transparent green fill, black borders, floor offset, front-side picking, default visibility'}));
console.log(JSON.stringify({status:'PASS',three_revision:THREE.REVISION,levels:catalog.length,meshes,triangles,camera_fits:48,sky_ceilings_skipped:skyCeilingsSkipped,indoor_ceilings_retained:true,front_side_raycast:'PASS',wall_culling_rays:wallSideChecks,all_layers_visible_by_default:true}));
