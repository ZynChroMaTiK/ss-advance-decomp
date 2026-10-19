import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createGraphicsView,filterGraphics,imageFor,uniqueGraphics,graphicsUrl,PAGE_SIZE,createGraphics} from '../site/graphics.js';
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
const follow=hash=>{const parts=hash.slice(1).split('/');gallery.open(parts[1]||'',{palette:parts[2]==='rome'?1:0})};
gallery.open();
for(const id of ['graphics-group','graphics-view','graphics-back-group','graphics-collection'])assert(!elements.has(id),'group controls are removed');
assert(html.includes('<label>Category<select id="graphics-category">'));
assert.equal($('graphics-category').children[0].textContent,'All');
assert.match($('graphics-count').textContent,/pictures$/);
for(const query of ['colt','dual colts','shotgun'])assert(filterGraphics(data.entries,query,'weapons').length>0,query);
assert.equal($('graphics-controls').hidden,false);
assert.equal($('graphics-grid').children.length,PAGE_SIZE);
assert.equal($('graphics-previous-page').disabled,true);
const singleCard=$('graphics-grid').children[0];singleCard.onclick();follow(links.at(-1));
assert.equal($('graphics-detail').hidden,false,'a picture opens directly');
$('graphics-back-pictures').onclick();assert.equal($('graphics-grid').children.length,PAGE_SIZE);
assert($('graphics-grid').children.some(b=>b.focused),'return to the picture thumbnail');
$('graphics-next-page').onclick();assert.match($('graphics-count').textContent,/61/);
assert($('graphics-count').focused);
$('graphics-previous-page').onclick();assert($('graphics-previous-page').disabled);
$('graphics-category').value='weapons';$('graphics-category').onchange();
const weaponPictures=uniqueGraphics(filterGraphics(data.entries,'','weapons'));
for(const [index,card] of $('graphics-grid').children.entries()){
 card.onclick();follow(links.at(-1));
 assert.equal($('graphics-preview').src,imageFor(weaponPictures[index]).path,'each weapon card opens its picture directly');
 $('graphics-back-pictures').onclick();
}
for(const category of data.usageSections){
 $('graphics-category').value=category.id;$('graphics-category').onchange();
 const pictures=uniqueGraphics(filterGraphics(data.entries,'',category.id));
 assert.equal($('graphics-grid').children.length,Math.min(PAGE_SIZE,pictures.length));
}
const actor=data.entries.find(e=>e.category==='actors'&&e.images[0].path!==e.images[1].path);
gallery.open(actor.id,{focus:true});
assert.equal($('graphics-category').value,actor.uses[0].section);assert($('graphics-title').focused);
assert($('graphics-detail').scrolled);assert.equal($('graphics-preview').src,actor.images[0].path);
$('graphics-palette').value='1';$('graphics-palette').onchange();
assert.equal(links.at(-1),graphicsUrl(actor.id,1),'palette persists in the URL');
follow(links.at(-1));
assert.equal($('graphics-preview').src,actor.images[1].path);
assert.equal($('graphics-download').href,actor.images[1].path);
assert.equal($('graphics-original').href,actor.images[1].path);
assert.equal($('graphics-download').download,actor.images[1].source.replaceAll('/','-'));
$('graphics-zoom').value='4';$('graphics-zoom').onchange();
assert.equal($('graphics-preview').style.width,`${actor.images[1].width*4}px`);
$('graphics-background').value='light';$('graphics-background').onchange();
assert.equal($('graphics-preview-area').dataset.background,'light');
$('graphics-next-image').onclick();assert(links.at(-1).startsWith('#graphics/actor-'));
follow(links.at(-1));
$('graphics-previous-image').onclick();assert.equal(links.at(-1),graphicsUrl(actor.id,1));
$('graphics-category').value='world';$('graphics-category').onchange();
assert($('graphics-detail').hidden,'changing categories clears an excluded preview');
assert($('graphics-palette-label').hidden,'palette control is hidden when it cannot change the pictures');
assert.equal(links.at(-1),graphicsUrl('',1),'excluded selection is removed from URL');
$('graphics-search').value='no-such-graphic-ever';$('graphics-search').oninput();
assert.equal($('graphics-grid').children.length,0);assert($('graphics-next-page').disabled);
assert($('graphics-next-image').disabled);assert.match($('graphics-count').textContent,/No pictures/);
$('graphics-clear').onclick();assert.equal($('graphics-category').value,'all');assert($('graphics-search').focused);
gallery.open(actor.id);
globalThis.window={scrollY:850,scrollTo:options=>window.restoredY=options.top};
const actorButton=$('graphics-grid').children.find(b=>b.attrs['aria-pressed']==='true');
actorButton.onclick();follow(links.at(-1));$('graphics-back-pictures').onclick();
assert($('graphics-detail').hidden);assert.equal(window.restoredY,850);
assert($('graphics-grid').children.some(b=>b.focused),'back restores the selected thumbnail focus');
delete globalThis.window;
const last=uniqueGraphics(data.entries).at(-1);gallery.open(last.id);
assert($('graphics-next-page').disabled);assert($('graphics-next-image').disabled);
assert($('graphics-grid').children.length<=PAGE_SIZE);
assert.equal(imageFor(last,1),last.images[0]);
$('graphics-grid').children.at(-1).onclick();assert.equal(links.at(-1),`#graphics/${last.id}`);
gallery.open('does-not-exist');assert.equal($('graphics-not-found').hidden,false);assert($('graphics-detail').hidden);
gallery.open();assert($('graphics-not-found').hidden);
for(const palette of [0,1]) {
 const unique=uniqueGraphics(data.entries,palette);
 const paths=unique.map(e=>imageFor(e,palette).path);
 assert.equal(new Set(paths).size,paths.length,'one thumbnail per visible image');
 assert.deepEqual(new Set(paths),new Set(data.entries.map(e=>imageFor(e,palette).path)),'all different images remain reachable');
}
const duplicate={id:'test-duplicate',images:data.entries[0].images};
assert.deepEqual(uniqueGraphics([data.entries[0],duplicate],0,duplicate),[duplicate],'keep a directly linked picture selected when its pixels are shared');
const alias='actor-033-001';gallery.open(alias,{palette:1});
const canonical=data.entries.find(e=>e.id===data.aliases[alias]);
assert(canonical && canonical.collection==='pickup-01','the old shotgun-table link now identifies the pistol it actually shows');
assert.equal($('graphics-preview').src,imageFor(canonical,1).path);
assert.equal($('graphics-not-found').hidden,true,'old duplicate links resolve');
assert.match($('graphics-usage').textContent,/Pistol/,'preview explains actual game usage');
const soldier=data.entries.find(e=>e.uses.some(u=>u.title==='Methug soldier · Attacking')&&e.uses.some(u=>u.title==='Methug soldier · Taking damage'));
assert(soldier,'shared poses retain both confirmed uses');
$('graphics-category').value='enemies';$('graphics-category').onchange();
gallery.open(soldier.id);
assert.match($('graphics-usage').textContent,/Attacking/);
assert.match($('graphics-usage').textContent,/Taking damage/);
assert.equal($('graphics-grid').children.filter(b=>b.children[0].children[0].src===imageFor(soldier).path).length,1,'shared poses appear once');
for(const action of ['attacking','taking damage'])assert(filterGraphics(data.entries,`Methug soldier ${action}`,'enemies').includes(soldier),'usage remains searchable');
const texture=data.entries.find(e=>e.category==='textures');
assert(filterGraphics(data.entries,texture.title.toUpperCase(),texture.uses[0].section).some(e=>e.id===texture.id));
assert.equal(filterGraphics(data.entries,'<script>not markup</script>').length,0);
assert(html.indexOf('<aside class="graphics-inspector"')<html.indexOf('<div class="graphics-browse"'),'mobile visual order matches reading order');
assert(!html.includes('data-route="wiki"'),'NETRICSA navigation stays removed');
assert(html.includes('European')&&html.includes('enable in Options → Extras'));
assert(html.includes('secret-sources.html')&&html.includes('/issues/new'));
globalThis.fetch=async()=>({ok:false});await assert.rejects(createGraphics,/unavailable/);
console.log(JSON.stringify({status:'PASS',checks:'Category and All labels, direct picture browsing, game-use filters and search, usage descriptions, deduplication in both palettes, complete browsing, bounded pages, old links, restored scroll/focus, original downloads, zoom, retry'}));
