import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import * as THREE from '../site/vendor/three.module.js';
import {OrbitControls} from '../site/vendor/OrbitControls.js';
import {floorPositions,wallBuffers} from '../site/geometry.js';
assert.equal(THREE.REVISION,'180');assert.equal(typeof OrbitControls,'function');
const catalog=JSON.parse(readFileSync(new URL('../site/data/catalog.json',import.meta.url)));
let meshes=0,triangles=0;
for(const entry of catalog){const d=JSON.parse(readFileSync(new URL('../site/'+entry.path,import.meta.url)));
 const buffers=[...d.sectors.map(s=>({positions:floorPositions(s.mesh_loops,s.initial_floor)})),wallBuffers(d.walls.initial),wallBuffers(d.walls.open)];
 for(const b of buffers){const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(b.positions,3));if(b.indices)g.setIndex(b.indices);if(b.uvs)g.setAttribute('uv',new THREE.Float32BufferAttribute(b.uvs,2));g.computeBoundingBox();g.computeBoundingSphere();assert(Number.isFinite(g.boundingSphere.radius));assert(g.boundingSphere.radius>0);triangles+=(b.indices?.length??b.positions.length/3)/3;meshes++;g.dispose()}
 // Orbit reset fits the map bounds on both a phone and desktop aspect ratio.
 const b=entry.bounds,span=Math.max(b.max_x-b.min_x,b.max_y-b.min_y)/64;
 for(const aspect of [.6,1,1.8]){const camera=new THREE.PerspectiveCamera(48,aspect,.08,3000),center=new THREE.Vector3((b.min_x+b.max_x)/128,b.min_height/64,-(b.min_y+b.max_y)/128),distance=span*.82/Math.sin(Math.atan(Math.tan(THREE.MathUtils.degToRad(camera.fov/2))*Math.min(aspect,1)));camera.position.copy(center).add(new THREE.Vector3(.62,.85,.8).normalize().multiplyScalar(distance));camera.lookAt(center);camera.updateMatrixWorld();for(const x of [b.min_x,b.max_x])for(const y of [b.min_y,b.max_y]){const p=new THREE.Vector3(x/64,b.min_height/64,-y/64).project(camera);assert(Math.abs(p.x)<1&&Math.abs(p.y)<1,`map ${entry.level} camera fit at aspect ${aspect}`)}}
}
console.log(JSON.stringify({status:'PASS',three_revision:THREE.REVISION,levels:catalog.length,meshes,triangles,camera_fits:48}));
