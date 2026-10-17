import assert from 'node:assert/strict';
import {readFileSync,existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {createWikiView,searchEntries} from '../site/wiki.js';
const site=new URL('../site/',import.meta.url);
const {entries}=JSON.parse(readFileSync(new URL('data/wiki.json',site)));
const ids=new Set(entries.map(e=>e.id));assert.equal(ids.size,entries.length);assert.equal(entries.length,54);
const counts={};for(const entry of entries){counts[entry.category]=(counts[entry.category]||0)+1;assert(entry.summary&&entry.sections.length&&entry.sources.length);for(const id of entry.related||[])assert(ids.has(id),`${entry.id} -> ${id}`);for(const source of entry.sources)assert(source.href.startsWith('https://')?new URL(source.href).protocol==='https:':existsSync(new URL(source.href,site)),source.href);if(entry.image)assert(existsSync(new URL(entry.image,site)),entry.image);for(const appearance of entry.appearances||[])assert(ids.has(`level-${appearance.level+1}`))}
assert.deepEqual(counts,{enemies:12,items:5,levels:16,secrets:5,systems:6,weapons:10});
for(const [name,entry] of Object.entries(JSON.parse(readFileSync(new URL('assets/netricsa/manifest.json',site)))))assert.equal(createHash('sha256').update(readFileSync(new URL('assets/netricsa/'+name,site))).digest('hex'),entry.sha256);
assert(searchEntries(entries,'revolver','weapons').some(e=>e.id==='weapon-pistol'));
assert(searchEntries(entries,'  ROCKET   launcher ','weapons').some(e=>e.title==='Rocket Launcher'));
assert.equal(searchEntries(entries,'does-not-exist').length,0);
for(const [type,name] of [[209,'Cyclops'],[213,'Hermit Crabuloid'],[214,'Gunrilla']]){
 const entry=entries.find(e=>e.entity_type===type);assert.equal(entry.title,name);
 for(const level of entries.filter(e=>e.category==='levels'))for(const folder of ['data','atlas/data']){
  const d=JSON.parse(readFileSync(new URL(`${folder}/level-${String(level.level+1).padStart(2,'0')}.json`,site)));
  for(const marker of d.markers.filter(m=>m.entity_type===type))assert.equal(marker.name,name);
 }
}
class Element{
 constructor(tag='div'){Object.assign(this,{tag,children:[],attrs:{},dataset:{},value:'',listeners:{},hidden:false,textContent:''})}
 append(...children){this.children.push(...children)} replaceChildren(...children){this.children=children;this.textContent=''}
 setAttribute(k,v){this.attrs[k]=v} addEventListener(k,fn){this.listeners[k]=fn} focus(){this.focused=true} scrollIntoView(){this.scrolled=true}
}
const elements=new Map(['wiki-search','wiki-count','wiki-results','wiki-empty','wiki-categories','wiki-clear','wiki-article'].map(id=>[id,new Element()]));
const doc={getElementById:id=>elements.get(id),createElement:tag=>new Element(tag)};
const wiki=createWikiView(entries,doc),allNodes=e=>[e,...e.children.flatMap(allNodes)];
for(const entry of entries){wiki.open(entry.id);assert.equal(doc.title,`${entry.title} — NETRICSA — Serious Sam Advance`);assert(allNodes(elements.get('wiki-article')).some(n=>n.tag==='h2'&&n.textContent===entry.title));assert(elements.get('wiki-results').children.some(n=>n.href===`#wiki/${entry.id}`&&n.attrs['aria-current']==='page'));}
elements.get('wiki-categories').children.find(b=>b.dataset.category==='weapons').onclick();assert.equal(elements.get('wiki-results').children.length,10);
const search=elements.get('wiki-search');search.value='revolver';search.listeners.input();assert.equal(elements.get('wiki-results').children.length,1);
search.value='nothing matches here';search.listeners.input();assert.equal(elements.get('wiki-empty').hidden,false);
elements.get('wiki-clear').onclick();assert.equal(search.value,'');assert.equal(search.focused,true);assert.equal(elements.get('wiki-results').children.length,54);
wiki.open('level-16');assert(allNodes(elements.get('wiki-article')).some(n=>n.href==='#levels/16/3d'));
wiki.open('invalid-record');assert(allNodes(elements.get('wiki-article')).some(n=>n.href==='#wiki/field-guide'));
wiki.open('weapon-pistol',{focus:true});assert(elements.get('wiki-article').focused);assert(elements.get('wiki-article').scrolled);
wiki.open('invalid-record',{focus:true});assert.equal(doc.title,'Entry not found — NETRICSA — Serious Sam Advance');
console.log(JSON.stringify({status:'PASS',entries:entries.length,categories:counts,checks:'all entries rendered, links, portraits, search, aliases, filters, reset, unknown entry, atlas enemy labels'}));
