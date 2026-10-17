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
globalThis.document={getElementById:get,createElement:()=>new Element(),querySelectorAll:()=>[],addEventListener(){},removeEventListener(){},hidden:false};
globalThis.window={addEventListener(){}};globalThis.devicePixelRatio=1;globalThis.ResizeObserver=class{observe(){}};
let tick,scene;globalThis.requestAnimationFrame=f=>tick=f;
const canvas=new Element(),renderer={domElement:canvas,setPixelRatio(){},setSize(){},render:s=>scene=s};
const pending=[];globalThis.fetch=path=>new Promise((resolve,reject)=>pending.push({path,resolve,reject}));
const catalog=JSON.parse(readFileSync(new URL('../site/data/catalog.json',import.meta.url)));
const source=JSON.parse(readFileSync(new URL('../site/'+catalog[0].path,import.meta.url)));
const data={...source,sectors:source.sectors.slice(0,1),walls:{initial:[],open:[]},markers:[],secrets:[]};
const viewer=createViewer(()=>renderer);viewer.setActive(true);
let load=viewer.load(0,catalog[0]);pending.shift().resolve({ok:true,json:async()=>data});await load;tick(16);
const original=scene.children[0];assert(original.visible);assert.equal(canvas.focusCount,0,'async loads must not steal focus');
assert.equal(get('view-name').textContent,`01 / ${data.title}`);
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
