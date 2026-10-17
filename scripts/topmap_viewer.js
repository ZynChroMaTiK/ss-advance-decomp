
'use strict';
const atlas=__DATA__;
const $=id=>document.getElementById(id), esc=v=>String(v).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
for(const [name,key] of Object.entries({stone:'stone',grid:'grid',egypt:'egypt',rome:'rome','font-white':'white','font-green':'green'}))document.documentElement.style.setProperty('--'+name,`url("${atlas.theme[key]}")`);
$('logo').src=atlas.theme.logo;
function bitmap(text,white=false){
 const font=atlas.theme.font;
 return `<span class="bitmap${white?' white':''}" role="img" aria-label="${esc(text)}">`+Array.from(text).map(c=>{
  if(c===' ')return '<i aria-hidden="true" style="width:8px;height:16px;background:none"></i>';
  const glyph=font.character_to_glyph[c.charCodeAt(0)]??1,metric=font.glyphs[glyph].metric_bytes;
  return `<i aria-hidden="true" style="width:${(metric[1]===205?1:metric[1])+1}px;height:16px;background-position:${-16*(glyph%16)-metric[0]}px ${-16*Math.floor(glyph/16)}px"></i>`;
 }).join('')+'</span>';
}
document.querySelectorAll('[data-bitmap]').forEach(n=>n.innerHTML=bitmap(n.dataset.bitmap,n.dataset.white==='true'));
function panel(name){for(const key of ['layers','secrets','intel']){$('panel-'+key).hidden=key!==name;$('tab-'+key).setAttribute('aria-selected',String(key===name));$('tab-'+key).tabIndex=key===name?0:-1}$('panel-scroll').scrollTop=0}
document.querySelectorAll('[data-panel]').forEach(b=>{
 b.onclick=()=>panel(b.dataset.panel);
 b.onkeydown=e=>{if(!['ArrowLeft','ArrowRight','Home','End'].includes(e.key))return;e.preventDefault();const keys=['layers','secrets','intel'],i=keys.indexOf(b.dataset.panel),n=e.key==='Home'?0:e.key==='End'?2:(i+(e.key==='ArrowRight'?1:2))%3;panel(keys[n]);$('tab-'+keys[n]).focus()};
});
document.querySelectorAll('[data-help]').forEach(b=>b.onclick=()=>$('help').showModal());$('close-help').onclick=()=>$('help').close();
$('help').onclick=e=>{if(e.target===$('help')){const b=$('help').getBoundingClientRect();if(e.clientX<b.left||e.clientX>b.right||e.clientY<b.top||e.clientY>b.bottom)$('help').close()}};
const chapters=[{name:'EGYPT',note:'CAMPAIGN / 01–06',cls:'egypt',start:0,end:6},{name:'ROME',note:'CAMPAIGN / 07–12',cls:'rome',start:6,end:12},{name:'MULTIPLAYER',note:'FOUR ARENAS',cls:'multi',start:12,end:16}];
$('level-nav').innerHTML=chapters.map(c=>`<section class="chapter"><div class="chapter-heading ${c.cls}"><strong>${c.name}</strong><small>${c.note}</small></div>`+atlas.maps.slice(c.start,c.end).map(m=>`<button class="level-link" data-level="${m.level}"><span class="num">${String(m.level+1).padStart(2,'0')}</span><span>${esc(m.title)}</span></button>`).join('')+'</section>').join('');
$('level-nav').onclick=e=>{const b=e.target.closest('[data-level]');if(b)choose(+b.dataset.level)};
function themeMap(){
 const colors={'#101822':'#030a03','#172330':'#0c2108','#23303e':'#123110','#334555':'#1b3319','#26323f':'#101f0d','#58452c':'#4f4320','#c5d3e2':'#acc495','#6b8196':'#48683c','#dfe7f2':'#dfebd3','#a1b5c9':'#91ad7e','#5ed8d0':'#8fde5b'};
 svg.querySelectorAll('[fill],[stroke]').forEach(n=>{for(const a of ['fill','stroke']){let c=n.getAttribute(a);if(colors[c])n.setAttribute(a,colors[c])}});
 svg.querySelector('style').textContent=svg.querySelector('style').textContent.replaceAll('#101822','#030a03');
}
const layers=[['weapons','Weapons','#f2c94c'],['health','Health','#6ce0b1'],['armor','Armor / shields','#68bdff'],['ammo','Ammo','#b5a2ff'],['items','Other items','#eee'],['doors','Doors / lifts','#ffad5a'],['switches','Switches','#57d5ec'],['secrets','Secrets','#ff7bd5'],['starts','Player starts','#4be3df'],['exits','Exits','#fff'],['respawn','MP respawns','#9fa9bc'],['enemies','Initial enemies','#e77a80'],['scripted-enemies','Scripted enemies','#e77a80'],['triggers','Entry triggers','#9ae1aa']];
let current=0,svg,box=[0,145,1740,1640],down=null;
const map=()=>atlas.maps[current];
const rows=obj=>'<dl>'+Object.entries(obj).map(([k,v])=>`<dt>${esc(k)}</dt><dd>${esc(v)}</dd>`).join('')+'</dl>';
const btn=(ref,label)=>`<button class="row" data-focus="${esc(ref)}">${esc(label)}</button>`;
const scriptBtn=id=>`<button class="row" data-script="${id}">Inspect script ${id}</button>`;
$('level').innerHTML=atlas.maps.map((m,i)=>`<option value="${i}">${String(i+1).padStart(2,'0')} · ${esc(m.title)}${i>=12?' (multiplayer)':''}</option>`).join('');
$('layers').innerHTML=layers.map(([id,label,color])=>`<label><input type="checkbox" data-layer="${id}" ${['enemies','scripted-enemies','triggers'].includes(id)?'':'checked'}><span>${label}</span><i class="swatch" style="--layer-color:${color}" aria-hidden="true"></i></label>`).join('');
function applyLayers(){document.querySelectorAll('#layers input').forEach(c=>svg.querySelectorAll(`[data-layer="${c.dataset.layer}"]`).forEach(n=>n.style.display=c.checked?'':'none'));['enemies','scripted-enemies','triggers'].forEach(l=>{if(document.querySelector(`#layers [data-layer="${l}"]`).checked)svg.querySelectorAll(`[data-layer="${l}"]`).forEach(n=>n.style.display='inline')})}
function setBox(v){box=v;svg.setAttribute('viewBox',v.join(' '));$('zoom').textContent=Math.round(1740/v[2]*100)+'%'}
function fit(){setBox([0,145,1740,1640]);$('full').setAttribute('aria-pressed','false')}
function zoom(f,x=box[0]+box[2]/2,y=box[1]+box[3]/2){if(box[2]*f<70||box[2]*f>14000)return;setBox([x+(box[0]-x)*f,y+(box[1]-y)*f,box[2]*f,box[3]*f])}
function choose(n){
 current=n;down=null;$('level').value=n;$('map').innerHTML=atlas.svgs[n];svg=$('map').querySelector('svg');svg.removeAttribute('width');svg.removeAttribute('height');themeMap();applyLayers();fit();let m=map();
 $('level-number').textContent=String(n+1).padStart(2,'0');$('level-title').textContent=m.title;$('chapter-title').textContent=n<6?'EGYPT / CAMPAIGN':n<12?'ROME / CAMPAIGN':(n<14?'EGYPT':'ROME')+' / MULTIPLAYER';
 $('previous').disabled=n===0;$('next').disabled=n===15;
 document.querySelectorAll('[data-level]').forEach(b=>{if(+b.dataset.level===n)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current')});
 $('summary').textContent=`${m.markers.length} recorded placements / ${m.movers.length} doors & lifts`;
 $('tab-secrets').textContent='Secrets'+(m.secrets.length?' ('+m.secrets.length+')':'');
 $('secrets').innerHTML=`<p class="coverage">${esc(m.secret_status)}</p>`+m.secrets.map(s=>`<div class="secret">${btn(s.id,s.id+' · '+s.title)}<p>${esc(s.note)}</p>${s.access?btn(s.id+'-access','Show opening trigger'):''}<a class="fine" href="${esc(s.source)}" target="_blank" rel="noopener">Identification source ↗</a></div>`).join('');
 for(const ext of ['png','svg','json'])$(ext).href=`${ext==='json'?'data':ext}/level-${String(n+1).padStart(2,'0')}.${ext}`;
 document.querySelectorAll('[data-png-download]').forEach(a=>a.href=$('png').href);
 $('search').value='';$('results').innerHTML='';$('details').innerHTML='<div class="empty"><div class="empty-icon" aria-hidden="true">+</div><h2>Awaiting target</h2><p>Select an item, door or room on the map to inspect its location, contents and activation scripts.</p></div>';
 location.hash='level-'+(n+1);
}
function focus(ref){let nodes=Array.from(svg.querySelectorAll('[data-ref]')).filter(n=>n.dataset.ref===ref),n=nodes.find(n=>n.tagName!=='text')||nodes[0];if(!n)return;let layer=n.dataset.layer,c=document.querySelector(`#layers [data-layer="${layer}"]`);if(c){c.checked=true;applyLayers()}let b=n.getBBox();let w=Math.max(450,b.width*1.8),h=Math.max(430,b.height*1.8);setBox([b.x+b.width/2-w/2,b.y+b.height/2-h/2,w,h]);inspect(ref)}
function inspect(ref){panel('intel');svg.querySelectorAll('.selected').forEach(n=>n.classList.remove('selected'));svg.querySelectorAll('[data-ref]').forEach(n=>{if(n.dataset.ref===ref)n.classList.add('selected')});let m=map(),out='',p=m.markers.find(p=>p.id===ref),sec=m.secrets.find(s=>s.id===ref||s.id+'-access'===ref);
 if(p){let same=m.markers.filter(q=>q.x===p.x&&q.y===p.y&&q.type===p.type);out=`<h2>${esc(p.name)}</h2>`+rows({Location:`X ${p.x}, Y ${p.y}`,Sector:p.sector,'Initial floor':p.floor,Code:p.code,Placement:p.origin==='map'?'Initial map object':'Scripted appearance',Instances:same.length});out+=same.map(q=>`<p>${esc(q.id)} · ${q.origin==='map'?'Initial placement':`Script ${q.script}, instruction ${q.instruction+1}`}</p>`+(q.script?scriptBtn(q.script):'')).join('');if(p.trigger&256)out+='<p>Collecting / activating this object requests a script:</p>'+scriptBtn(p.trigger&255);if(p.group==='respawn')out+='<p>One of the multiplayer scheduler’s rotating placement points. Item type varies by schedule; an occupied XY blocks a spawn.</p>'+rows({'Schedule item IDs':p.schedule_item_ids.join(', ')});}
 else if(sec){out=`<h2>${esc(sec.id+' · '+sec.title)}</h2><p>${esc(sec.note)}</p>`+rows({Location:`X ${sec.x}, Y ${sec.y}`,Evidence:sec.status})+btn(sec.marker,'Inspect reward pickup');if(sec.access)out+=btn(sec.id+'-access','Locate opening trigger')+scriptBtn(sec.access.script);out+=`<p><a href="${esc(sec.source)}" target="_blank" rel="noopener">Read identification source ↗</a></p>`;}
 else if(ref.startsWith('door-')){let d=m.movers[+ref.slice(5)];out=`<h2>${d.label} · ${esc(d.kind)}</h2>`+rows({Sector:d.sector,Location:d.center.map(Math.round).join(', '),'Initial lock':d.locked?'Locked':'Unlocked','Lower / upper':`${d.lower} / ${d.upper}`});out+=d.scripts.length?'<p>Script commands, in source order:</p>'+d.scripts.map(s=>scriptBtn(s.script)+`<span class="chip">${esc(s.command)} ${d.label}</span>`).join(''):'<p>No script commands target this mover. Door interaction can open unlocked movers.</p>';}
 else if(ref.startsWith('switch-')){let sw=m.switches.find(s=>s.id===+ref.slice(7));out=`<h2>B${sw.id} · Wall switch</h2>`+rows({Location:sw.center.join(', '),Line:sw.line})+scriptBtn(sw.script);}
 else if(ref.startsWith('sector-')){let s=m.sectors[+ref.slice(7)];out=`<h2>Room / sector ${s.id}</h2>`+rows({'Initial floor':s.floor,'Ceiling Z':s.ceiling,Flags:'0x'+s.flags.toString(16)});if(s.flags&256)out+='<p>First entry requests:</p>'+scriptBtn(s.flags&255);out+=m.movers.filter(d=>d.sector===s.id).map(d=>btn('door-'+d.id,'Inspect '+d.label)).join('');out+=m.markers.filter(p=>p.sector===s.id&&p.group!=='enemies').map(p=>btn(p.id,p.name+' · '+p.id)).join('');}
 else if(ref.startsWith('line-')){let l=m.lines[+ref.slice(5)];out=`<h2>${l.kind===1?'Door edge':l.kind===2?'Switch edge':'Wall / boundary'} ${l.id}</h2>`+rows({'Front / back':`${l.front} / ${l.back}`,Kind:l.kind,Flags:'0x'+l.flags.toString(16)});out+=m.movers.filter(d=>d.sector===l.front||d.sector===l.back).map(d=>btn('door-'+d.id,'Inspect '+d.label)).join('');if(l.kind===2)out+=scriptBtn(l.trigger);}
 const icon=p&&Array.from(svg.querySelectorAll('image[data-ref]')).find(n=>n.dataset.ref===p.id);
 if(icon)out='<div class="sprite-preview"><img alt="'+esc(p.name)+'" src="'+icon.getAttribute('href')+'"></div>'+out;
 $('details').innerHTML=out||'<p>No additional details.</p>';
}
function showScript(id){panel('intel');let m=map(),s=m.scripts.find(s=>s.id===id);if(!s){$('details').innerHTML='<h2>Script '+id+'</h2><p>No script table entry is available.</p>';return}let triggers=m.switches.filter(q=>q.script===id).map(q=>btn('switch-'+q.id,'Switch B'+q.id)).join('')+m.sectors.filter(q=>(q.flags&256)&&(q.flags&255)===id).map(q=>btn('sector-'+q.id,'Enter sector '+q.id)).join('')+m.markers.filter(q=>(q.trigger&256)&&(q.trigger&255)===id).map(q=>btn(q.id,'Pickup '+q.name)).join('');
 const optext=o=>{let a=o.args[1];if(o.op===1)return `Spawn ${o.markers.length} objects (group / tag ${a})`+o.markers.map(id=>{let p=m.markers.find(p=>p.id===id);return btn(id,p.name+' · '+id)}).join('');if(o.op===2)return'Wait for group '+a+' to be defeated';if(o.op===3)return'Advance';if(o.op>=4&&o.op<=7)return['Open','Close','Unlock','Lock'][o.op-4]+btn('door-'+a,'D'+(a+1));if(o.op===8)return'Delay '+a+' seconds';return o.op===9?'Request level exit':'Finish script'};
 $('details').innerHTML=`<h2>Script ${id}</h2><h3>Activation locations</h3>${triggers||'<p>No map-local switch, sector or pickup trigger found. It may be started by level setup.</p>'}<h3>Sequence</h3><ol id="scriptList">${s.ops.map(o=>'<li>'+optext(o)+'</li>').join('')}</ol>`;
}
$('level').onchange=e=>choose(+e.target.value);$('layers').onchange=applyLayers;$('fit').onclick=fit;$('plus').onclick=()=>zoom(.7);$('minus').onclick=()=>zoom(1/.7);$('full').onclick=()=>{if($('full').getAttribute('aria-pressed')==='true')fit();else{setBox([0,0,1760,1900]);$('full').setAttribute('aria-pressed','true')}};
$('previous').onclick=()=>{if(current>0)choose(current-1)};$('next').onclick=()=>{if(current<15)choose(current+1)};
document.querySelector('.brand').onclick=e=>{e.preventDefault();choose(0)};
window.addEventListener('hashchange',()=>{const n=Number(location.hash.match(/^#level-(\d+)$/)?.[1]||1)-1;if(n>=0&&n<16&&n!==current)choose(n)});
$('search').oninput=()=>{let q=$('search').value.trim().toLowerCase(),m=map();$('results').innerHTML=q?m.markers.filter(p=>`${p.name} ${p.code} ${p.id}`.toLowerCase().includes(q)).map(p=>btn(p.id,p.name+' · '+p.id+(p.origin==='script'?' (script)':''))).join('')+m.movers.filter(d=>`${d.label} ${d.kind}`.toLowerCase().includes(q)).map(d=>btn('door-'+d.id,d.label+' · '+d.kind)).join('')+m.sectors.filter(s=>`room ${s.id} sector ${s.id}`.includes(q)).map(s=>btn('sector-'+s.id,'Room / sector '+s.id)).join('')+m.switches.filter(s=>`switch b${s.id}`.includes(q)).map(s=>btn('switch-'+s.id,'Switch B'+s.id)).join('')+m.lines.filter(l=>`wall ${l.id} line ${l.id}`.includes(q)).map(l=>btn('line-'+l.id,'Wall / line '+l.id)).join(''):'';if(q&&!$('results').innerHTML)$('results').innerHTML='<p>No matches in this level.</p>'};
document.querySelector('aside').onclick=e=>{let b=e.target.closest('[data-focus],[data-script]');if(!b)return;if(b.dataset.focus)focus(b.dataset.focus);else showScript(+b.dataset.script)};
function point(e){let p=svg.createSVGPoint();p.x=e.clientX;p.y=e.clientY;return p.matrixTransform(svg.getScreenCTM().inverse())}
$('map').onkeydown=e=>{const pan={ArrowLeft:[-.1,0],ArrowRight:[.1,0],ArrowUp:[0,-.1],ArrowDown:[0,.1]}[e.key];if(pan){e.preventDefault();setBox([box[0]+pan[0]*box[2],box[1]+pan[1]*box[3],box[2],box[3]])}else if(['+','=','-','Home'].includes(e.key)){e.preventDefault();if(e.key==='Home')fit();else zoom(e.key==='-'?1.15:1/1.15)}};
$('map').addEventListener('wheel',e=>{e.preventDefault();let p=point(e);zoom(e.deltaY>0?1.15:1/1.15,p.x,p.y)},{passive:false});
$('map').onpointerdown=e=>{if(e.button!==0)return;let p=point(e);down={x:e.clientX,y:e.clientY,px:p.x,py:p.y,box:[...box],ref:e.target.closest('[data-ref]')?.dataset.ref};$('map').setPointerCapture(e.pointerId)};
$('map').onpointermove=e=>{if(!down)return;let p=point(e);setBox([box[0]+down.px-p.x,box[1]+down.py-p.y,box[2],box[3]])};
$('map').onpointerup=e=>{if(down&&Math.hypot(e.clientX-down.x,e.clientY-down.y)<5&&down.ref)inspect(down.ref);down=null};$('map').onpointercancel=()=>down=null;
let initial=Number(location.hash.match(/^#level-(\d+)$/)?.[1]||1)-1;choose(initial>=0&&initial<16?initial:0);
