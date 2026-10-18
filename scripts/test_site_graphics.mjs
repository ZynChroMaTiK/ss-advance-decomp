import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createGraphicsView,filterGraphics,imageFor,PAGE_SIZE,createGraphics} from '../site/graphics.js';
const data=JSON.parse(readFileSync(new URL('../site/data/graphics.json',import.meta.url)));
const html=readFileSync(new URL('../site/index.html',import.meta.url),'utf8');
class Element {
 constructor(tag='div'){Object.assign(this,{tag,children:[],attrs:{},dataset:{},style:{},value:'',hidden:false,textContent:''})}
 append(...nodes){this.children.push(...nodes)} replaceChildren(...nodes){this.children=nodes}
 setAttribute(k,v){this.attrs[k]=v} focus(){this.focused=true} scrollIntoView(){this.scrolled=true}
}
const elements=new Map([...html.matchAll(/id="(graphics[^"]*)"/g)].map(m=>[m[1],new Element()]));
const $=id=>{assert(elements.has(id),`Missing markup: ${id}`);return elements.get(id)};
$('graphics-zoom').value='fit';
const doc={getElementById:$,createElement:tag=>new Element(tag)};
let links=[];
const gallery=createGraphicsView(data,doc,hash=>links.push(hash));
gallery.open();
assert.equal($('graphics-controls').hidden,false);
assert.equal($('graphics-grid').children.length,PAGE_SIZE);
assert.equal($('graphics-previous-page').disabled,true);
$('graphics-next-page').onclick();assert.match($('graphics-count').textContent,/61/);
assert($('graphics-count').focused);
$('graphics-previous-page').onclick();assert($('graphics-previous-page').disabled);
for(const category of data.categories){
 $('graphics-category').value=category.id;$('graphics-category').onchange();
 assert.equal(filterGraphics(data.entries,'',category.id).length,category.count);
 assert.equal($('graphics-grid').children.length,Math.min(PAGE_SIZE,category.count));
 assert.equal($('graphics-group-label').hidden,category.id!=='actors');
}
const actor=data.entries.find(e=>e.category==='actors'&&e.images[0].path!==e.images[1].path);
gallery.open(actor.id,{focus:true});
assert.equal($('graphics-category').value,'actors');assert($('graphics-title').focused);
assert($('graphics-detail').scrolled);assert.equal($('graphics-preview').src,actor.images[0].path);
$('graphics-group').value=String(actor.group);$('graphics-group').onchange();
assert(filterGraphics(data.entries,'','actors',String(actor.group)).every(e=>e.group===actor.group));
$('graphics-palette').value='1';$('graphics-palette').onchange();
assert.equal($('graphics-preview').src,actor.images[1].path);
assert.equal($('graphics-download').href,actor.images[1].path);
assert.equal($('graphics-original').href,actor.images[1].path);
assert.equal($('graphics-download').download,actor.images[1].source.replaceAll('/','-'));
$('graphics-zoom').value='4';$('graphics-zoom').onchange();
assert.equal($('graphics-preview').style.width,`${actor.images[1].width*4}px`);
$('graphics-background').value='light';$('graphics-background').onchange();
assert.equal($('graphics-preview-area').dataset.background,'light');
$('graphics-next-image').onclick();assert(links.at(-1).startsWith('#graphics/actor-'));
gallery.open(links.at(-1).slice('#graphics/'.length));
$('graphics-previous-image').onclick();assert.equal(links.at(-1),`#graphics/${actor.id}`);
$('graphics-search').value='no-such-graphic-ever';$('graphics-search').oninput();
assert.equal($('graphics-grid').children.length,0);assert($('graphics-next-page').disabled);
assert($('graphics-next-image').disabled);assert.match($('graphics-count').textContent,/No pictures/);
$('graphics-clear').onclick();assert.equal($('graphics-category').value,'all');assert($('graphics-search').focused);
const last=data.entries.at(-1);gallery.open(last.id);
assert($('graphics-next-page').disabled);assert($('graphics-next-image').disabled);
assert($('graphics-grid').children.length<=PAGE_SIZE);
assert.equal(imageFor(last,1),last.images[0]);
$('graphics-grid').children.at(-1).onclick();assert.equal(links.at(-1),`#graphics/${last.id}`);
gallery.open('does-not-exist');assert.equal($('graphics-not-found').hidden,false);assert($('graphics-detail').hidden);
gallery.open();assert($('graphics-not-found').hidden);
const texture=data.entries.find(e=>e.category==='textures');
assert(filterGraphics(data.entries,texture.title.toUpperCase(),'textures').some(e=>e.id===texture.id));
assert.equal(filterGraphics(data.entries,'<script>not markup</script>').length,0);
globalThis.fetch=async()=>({ok:false});await assert.rejects(createGraphics,/unavailable/);
console.log(JSON.stringify({status:'PASS',checks:'categories, resource filters, search, bounded pagination, deep links, original downloads, both palettes, zoom, background, unknown entry, fetch failure'}));
