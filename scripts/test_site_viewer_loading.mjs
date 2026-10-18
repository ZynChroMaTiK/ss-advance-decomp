// Run actual viewer loading/scene transitions with real Three.js objects.
// Only the GPU renderer, DOM and network are substituted; this is not visual QA.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createViewer} from '../site/viewer3d.js';
class Element {
 constructor(){Object.assign(this,{children:[],style:{},listeners:{},checked:true,clientWidth:800,clientHeight:600,focusCount:0})}
 append(...nodes){this.children.push(...nodes)} prepend(node){this.children.unshift(node)} replaceChildren(...nodes){this.children=nodes}
 setAttribute(){} addEventListener(k,f){(this.listeners[k]??=[]).push(f)} removeEventListener(){}
 getRootNode(){return document} focus(){this.focusCount++}
 getContext(){return {fillRect(){},strokeRect(){},fillText(){}}}
}
const nodes=new Map();const get=id=>{if(!nodes.has(id))nodes.set(id,new Element());return nodes.get(id)};
globalThis.document={getElementById:get,createElement:()=>new Element(),querySelectorAll:s=>s==='.view-options input'?[get('show-triggers'),get('show-floors'),get('open-doors'),get('show-scripted'),get('show-items'),get('show-enemies')]:[],addEventListener(){},removeEventListener(){},hidden:false};
globalThis.window={addEventListener(){}};globalThis.devicePixelRatio=1;globalThis.ResizeObserver=class{observe(){}};
let tick,scene,camera;globalThis.requestAnimationFrame=f=>tick=f;
const canvas=new Element(),renderer={domElement:canvas,setPixelRatio(){},setSize(){},render:(s,c)=>{scene=s;camera=c}};
const pending=[];globalThis.fetch=path=>new Promise((resolve,reject)=>pending.push({path,resolve,reject}));
const catalog=JSON.parse(readFileSync(new URL('../site/data/catalog.json',import.meta.url)));
const source=JSON.parse(readFileSync(new URL('../site/'+catalog[0].path,import.meta.url)));
const data={...source,sectors:[source.sectors[0],{...source.sectors.find(s=>s.flags&256),initial_ceiling:0,open_ceiling:224}],walls:{initial:[],open:[]},markers:[{id:'enemy',origin:'script',group:'enemies',name:'Enemy',x:512,y:512,initial_floor:0},{id:'item',origin:'script',group:'items',name:'Item',x:256,y:256,initial_floor:0}],scripts:[{id:5,ops:[{op:1,markers:['enemy','item']}]}],movers:[],secrets:[]};
const viewer=createViewer(()=>renderer);viewer.setActive(true);
let load=viewer.load(0,catalog[0]);pending.shift().resolve({ok:true,json:async()=>data});await load;tick(16);
const original=scene.children[0];assert(original.visible);assert.equal(canvas.focusCount,0,'async loads must not steal focus');
assert.equal(get('view-name').textContent,`01 / ${data.title}`);
assert.equal(get('object-results').children.length,5,'rooms, placements and entry trigger available without a pointer pick');
const triggers=original.getObjectByName('triggers');assert(triggers.visible);assert.equal(triggers.children.length,1);
const arrows=original.getObjectByName('trigger-arrows');assert.equal(arrows.children.length,2);
for(let mask=0;mask<16;mask++){
 const ids=['show-triggers','show-scripted','show-enemies','show-items'];ids.forEach((id,i)=>get(id).checked=!!(mask&(1<<i)));get('show-triggers').onchange();
 const visible=[];arrows.traverseVisible(o=>{if(o.type==='ArrowHelper')visible.push(o.userData.target.userData.marker.id)});
 assert.deepEqual(visible,[(mask&7)===7?'enemy':null,(mask&11)===11?'item':null].filter(Boolean));
}
const oldArrowHeight=arrows.children[0].position.y;
get('open-doors').checked=false;get('open-doors').onchange();assert.equal(triggers.children[0].scale.y,0);assert.notEqual(arrows.children[0].position.y,oldArrowHeight);
get('open-doors').checked=true;get('open-doors').onchange();assert.equal(triggers.children[0].scale.y,3.5);assert.equal(arrows.children[0].position.y,oldArrowHeight);
get('show-triggers').checked=false;get('show-triggers').onchange();assert(!triggers.visible);
get('show-triggers').checked=true;get('show-triggers').onchange();assert(triggers.visible);
get('show-floors').checked=false;get('show-floors').onchange();assert(triggers.visible,'trigger visibility is independent of floors');
get('object-kind').value='triggers';get('object-kind').listeners.change[0]();assert.equal(get('object-results').children.length,1);
get('object-inspect').onclick();assert.match(get('selection').children[0].textContent,/Event area.*event/);
get('object-kind').value='all';get('object-kind').listeners.change[0]();
const firstRoom=get('object-results').children.find(o=>o.value==='sector:0');
get('object-results').value=firstRoom.value;const before=camera.position.clone();
for(const event of [{key:'Enter',code:''},{key:'Enter',code:'NumpadEnter'},{code:'Enter'}]){
 camera.position.copy(before);let prevented=false;
 get('object-results').listeners.keydown[0]({...event,preventDefault(){prevented=true}});
 assert(prevented);assert(!before.equals(camera.position));assert.equal(canvas.focusCount,0,'inspection keeps keyboard focus in the object list');
}
assert.equal(get('selection').children[0].textContent,firstRoom.textContent);
get('object-search').value='no matching record';get('object-search').listeners.input[0]();
assert.equal(get('object-results').children.length,0);assert(get('object-inspect').disabled);
get('object-search').value='';get('object-search').listeners.input[0]();assert(!get('object-inspect').disabled);
load=viewer.load(1,catalog[1]);assert.equal(original.visible,false,'old map must not remain under the new selection');
assert.equal(get('selection').children.length,0);assert.equal(get('secret-list').children.length,0);
await viewer.load(0,catalog[0]);assert(original.visible);
pending.shift().resolve({ok:true,json:async()=>({...data,title:'late response'})});await load;tick(32);
assert.equal(scene.children[0],original,'return to cached map cancels pending switch');assert(!get('view-name').textContent.includes('late response'));
load=viewer.load(1,catalog[1]);pending.shift().reject(Error('offline'));await load;
assert.equal(original.visible,false);assert.match(get('view-message').textContent,/Unable to load/);
await viewer.load(0,catalog[0]);assert(original.visible);assert.equal(get('view-message').hidden,true);
assert.equal(canvas.focusCount,0);get('view-zoom-in').onclick();assert.equal(canvas.focusCount,1);
viewer.setActive(false);
console.log(JSON.stringify({status:'PASS',checks:'real scene loading, failed switch hides stale map and intel, cached return cancels late response, recovery, async focus preservation, keyboard zoom'}));
