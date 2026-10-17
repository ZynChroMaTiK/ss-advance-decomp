import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {createLevelLoader} from './topmap_loader.mjs';
const folder=new URL('../site/atlas/',import.meta.url);let requests=[];
const fetcher=async path=>{requests.push(path);const value=readFileSync(new URL(path,folder),'utf8');return {ok:true,json:async()=>JSON.parse(value),text:async()=>value}};
const load=createLevelLoader(fetcher,3);
assert.equal(load(0),load(0),'concurrent load deduplicates');await load(0);assert.equal(requests.length,2);
for(let n=1;n<16;n++){const result=await load(n);assert.equal(result.map.level,n);assert(!result.svg.includes('data:image/png'),'shared sprite URLs');}
assert.equal(requests.length,32);await load(0);assert.equal(requests.length,34,'old maps leave bounded cache');
await assert.rejects(load(16),/Invalid level/);
let calls=0;const retry=createLevelLoader(async path=>{if(calls++<2)throw Error('offline');return fetcher(path)});
await assert.rejects(retry(1),/offline/);assert.equal((await retry(1)).map.level,1);
// Execute the real choose() body through races, failure and recovery.
const source=readFileSync(new URL('topmap_viewer.js',import.meta.url),'utf8');
const fn=source.slice(source.indexOf('async function choose(n)'),source.indexOf('function focus(ref)'));
const ids=new Map();const get=id=>{if(!ids.has(id))ids.set(id,{replaceChildren(){},querySelector(){return {removeAttribute(){}}}});return ids.get(id)};
const queue=[];const maps=Array.from({length:16},(_,level)=>({level,title:'Map '+level}));
const ctx={atlas:{maps,svgs:[],loadLevel:n=>new Promise((resolve,reject)=>queue.push({n,resolve,reject}))},requested:0,current:0,loadRevision:0,down:null,svg:null,$:get,location:{},document:{querySelectorAll:()=>[]},themeMap(){},applyLayers(){},fit(){},map:()=>ctx.atlas.maps[ctx.current],esc:String,btn(){}};
vm.createContext(ctx);vm.runInContext(fn,ctx);
const data=n=>({map:{level:n,title:'Map '+n,markers:[],movers:[],secrets:[]},svg:'<svg/>'});
const first=ctx.choose(1),second=ctx.choose(2);queue[1].resolve(data(2));await second;queue[0].resolve(data(1));await first;
assert.equal(ctx.current,2);assert.equal(get('level-title').textContent,'Map 2');
const failed=ctx.choose(3);queue[2].reject(Error('offline'));await failed;assert.equal(ctx.svg,null);assert.equal(get('retry-map').hidden,false);assert.equal(get('search').disabled,true);
const back=ctx.choose(2);queue[3].resolve(data(2));await back;assert.equal(ctx.current,2);assert.equal(get('search').disabled,false);assert.equal(get('map-notice').hidden,true);
assert.equal(readFileSync(new URL('loader.js',folder),'utf8'),readFileSync(new URL('topmap_loader.mjs',import.meta.url),'utf8'));
console.log(JSON.stringify({status:'PASS',levels:16,checks:'real per-level assets, shared sprites, bounded cache, deduplication, retry, invalid input, actual choose race/failure/recovery'}));
