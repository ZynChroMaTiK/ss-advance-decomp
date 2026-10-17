import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('topmap_viewer.js',import.meta.url),'utf8');
const nodes={map:{},search:{value:''},results:{}};
const level=JSON.parse(readFileSync(new URL('../site/atlas/data/level-04.json',import.meta.url)));
let prevented=0;
const ctx={$:id=>nodes[id],box:[0,0,100,200],setBox:v=>ctx.box=v,fit:()=>ctx.fitted=true,zoom:f=>ctx.factor=f,map:()=>level,btn:(id,label)=>`${id}:${label}|`};
vm.runInNewContext(source.split('\n').find(line=>line.startsWith("$('map').onkeydown=")),ctx);
for(const key of ['ArrowRight','ArrowDown'])nodes.map.onkeydown({key,preventDefault:()=>prevented++});
assert.deepEqual(Array.from(ctx.box),[10,20,100,200]);assert.equal(prevented,2);
nodes.map.onkeydown({key:'Home',preventDefault(){}});assert(ctx.fitted);
nodes.map.onkeydown({key:'+',preventDefault(){}});assert(ctx.factor<1);
nodes.map.onkeydown({key:'-',preventDefault(){}});assert(ctx.factor>1);
vm.runInNewContext(source.split('\n').find(line=>line.startsWith("$('search').oninput=")),ctx);
for(const [query,ref] of [['room 12','sector-12'],['wall 5','line-5'],['B3','switch-3'],['D1','door-0']]){
 nodes.search.value=query;nodes.search.oninput();assert(nodes.results.innerHTML.includes(ref),query);
}
nodes.search.value='missing-entry-999';nodes.search.oninput();assert(nodes.results.innerHTML.includes('No matches'));
console.log(JSON.stringify({status:'PASS',checks:'actual atlas keyboard handlers, pan/fit/zoom, room/wall/switch/door lookup, empty results'}));
