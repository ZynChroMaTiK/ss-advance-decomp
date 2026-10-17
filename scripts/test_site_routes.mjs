// Exercise the actual router, with an injected viewer and a minimal DOM.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createApp} from '../site/app.js';
class Element {
 constructor(id=''){Object.assign(this,{id,dataset:{},attrs:{},listeners:{},children:[],value:'',hidden:false,messages:[]});this.contentWindow={postMessage:(message,origin)=>this.messages.push({message,origin})}}
 append(x){this.children.push(x)} setAttribute(k,v){this.attrs[k]=v}
 toggleAttribute(k,v){if(v)this.attrs[k]='';else delete this.attrs[k]}
 addEventListener(k,f){this.listeners[k]=f}
}
const pages=['project','features','levels','development'].map(id=>new Element(id));
const links=pages.map(p=>{const e=new Element();e.dataset.route=p.id;return e});
const selects=[new Element()], modes=['2d','3d'].map(mode=>{const e=new Element();e.dataset.mode=mode;return e});
const elements=new Map(pages.map(p=>[p.id,p]));
for(const id of ['atlas-frame','atlas-panel','explorer-panel','view-message','level-status'])elements.set(id,new Element(id));
let hash='#project';globalThis.location={get hash(){return hash},set hash(v){hash=v.startsWith('#')?v:'#'+v},origin:'https://example.test'};
globalThis.history={replaceState:(_a,_b,path)=>location.hash=path};
globalThis.document={getElementById:id=>elements.get(id),createElement:()=>new Element(),querySelectorAll:s=>({'.page':pages,'[data-route]':links,'.level-select':selects,'[data-mode]':modes}[s])};
const events={};globalThis.window={addEventListener:(name,fn)=>events[name]=fn};
const catalog=JSON.parse(readFileSync(new URL('../site/data/catalog.json',import.meta.url)));
let loads=[],active=false,resolveViewer;const viewer={setActive:v=>active=v,load:async(n,e)=>loads.push([n,e.level])};
const app=createApp(catalog,()=>new Promise(resolve=>resolveViewer=resolve));
assert.equal(selects[0].children.length,16);assert.equal(pages[0].hidden,false);
location.hash='#atlas/16';await app.render();assert.equal(location.hash,'#levels/16/2d');assert.equal(elements.get('atlas-frame').src,'atlas/index.html#level-16');assert.match(elements.get('level-status').textContent,/Multiplayer/);
selects[0].value='2';selects[0].onchange();await app.render();assert.equal(location.hash,'#levels/3/2d');
const frame=elements.get('atlas-frame');
for(const event of [{origin:'https://untrusted.test',source:frame.contentWindow},{origin:location.origin,source:{}}])events.message({...event,data:{type:'ssa:level',level:10}});
assert.equal(selects[0].value,2);
events.message({origin:location.origin,source:frame.contentWindow,data:{type:'ssa:level',level:5}});assert.equal(location.hash,'#levels/6/2d');
events.message({origin:location.origin,source:frame.contentWindow,data:{type:'ssa:level',level:99}});assert.equal(selects[0].value,5);
frame.listeners.load();assert.equal(frame.messages.at(-1).message.level,5);
// A late viewer initialization must not reactivate a hidden page.
modes[1].onclick();const pending=app.render();location.hash='#features';await app.render();resolveViewer(viewer);await pending;assert.equal(active,false);assert.equal(loads.length,0);
location.hash='#explorer/11';await app.render();assert.equal(location.hash,'#levels/11/3d');assert.equal(active,true);assert.deepEqual(loads.at(-1),[10,10]);assert.equal(elements.get('atlas-panel').hidden,true);assert.equal(modes[1].attrs['aria-pressed'],'true');assert.match(elements.get('level-status').textContent,/still in development/);
events.message({origin:location.origin,source:frame.contentWindow,data:{type:'ssa:level',level:1}});assert.equal(selects[0].value,10);
modes[0].onclick();await app.render();assert.equal(active,false);assert.equal(location.hash,'#levels/11/2d');assert.equal(frame.messages.at(-1).message.level,10);
location.hash='#levels/999/3d';await app.render();assert.equal(selects[0].value,10);assert.equal(location.hash,'#levels/11/3d');
location.hash='#development';await app.render();assert.equal(pages[3].hidden,false);assert.equal(active,false);
assert.equal(document.title,'Build & Development — Serious Sam Advance');
console.log(JSON.stringify({status:'PASS',level_options:16,checks:'unified modes, legacy links, shared selection, stale async activation, iframe synchronization, origin/source/range guards'}));
