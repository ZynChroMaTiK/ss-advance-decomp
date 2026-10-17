import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {objectRecords,filterObjects} from '../site/viewer_objects.js';
import {bindFlightControls} from '../site/flight_controls.js';
import {revealReference} from '../site/reference.js';
let count=0;
for(let i=1;i<=16;i++){
 const d=JSON.parse(readFileSync(new URL(`../site/data/level-${String(i).padStart(2,'0')}.json`,import.meta.url)));
 for(const open of [false,true]){
  const triggers=filterObjects(objectRecords(d,open),'','triggers');
  assert.equal(triggers.length,d.sectors.filter(s=>s.flags&256).length);
  for(const r of triggers){const s=d.sectors.find(s=>'trigger:'+s.id===r.id);assert.equal(r.fields.Script,s.flags&255);assert(filterObjects(triggers,`sector ${s.id} script ${s.flags&255}`).includes(r));}
 }
 for(const open of [false,true]){const records=objectRecords(d,open);count+=records.length;assert.equal(new Set(records.map(r=>r.id)).size,records.length);assert(records.every(r=>r.point.length===3&&r.point.every(Number.isFinite)));assert.equal(filterObjects(records,'','rooms').length,d.sectors.length);assert.equal(filterObjects(records,'','doors').length,d.movers.length);for(const m of d.markers)assert(records.some(r=>r.id==='marker:'+m.id));assert(filterObjects(records,'sector 0','rooms').length>0);assert.equal(filterObjects(records,'impossible-query-xyz').length,0);}
}
const buttons=['KeyW','KeyD','KeyE'].map(code=>({dataset:{flightKey:code},events:{},attrs:{},addEventListener(e,f){this.events[e]=f},setAttribute(k,v){this.attrs[k]=v},setPointerCapture(){}}));
let active=true,keys;const controls=bindFlightControls(buttons,k=>keys=k,()=>active);
const event=id=>({pointerId:id,preventDefault(){}});
buttons[0].events.pointerdown(event(1));buttons[1].events.pointerdown(event(2));assert(keys.has('KeyW')&&keys.has('KeyD'));
buttons[0].events.pointercancel(event(1));assert(!keys.has('KeyW')&&keys.has('KeyD'));
buttons[1].events.lostpointercapture(event(2));assert.equal(keys.size,0);
buttons[2].events.keydown({code:'Space',preventDefault(){}});assert(keys.has('KeyE'));buttons[2].events.blur();assert.equal(keys.size,0);
buttons[0].events.pointerdown(event(3));controls.clear();assert.equal(keys.size,0);assert.equal(buttons[0].attrs['aria-pressed'],'false');
active=false;buttons[0].events.pointerdown(event(4));assert.equal(keys.size,0);
const group={tagName:'DETAILS',open:false,parentElement:null};let scrolled=false;
const target={parentElement:group,scrollIntoView(){scrolled=true}};revealReference('#history',{getElementById:()=>target});assert(group.open&&scrolled);
revealReference('#%zz',{getElementById(){throw Error('malformed hash must be ignored')}});
console.log(JSON.stringify({status:'PASS',object_records:count,checks:'all maps/open-door states, searchable geometry and placements, multitouch, cancellation, held keyboard buttons, history deep links'}));
