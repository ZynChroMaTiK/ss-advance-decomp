export function objectRecords(data, opened = false) {
  const records = data.markers.map(m => ({id:'marker:'+m.id, kind:m.group.includes('enemies')?'enemies':'items', label:`${m.id} / ${m.name}${m.origin==='script'?' / scripted':''}`, point:[m.x,m.y,m.initial_floor], fields:{ID:m.id,Sector:m.sector,Placement:m.origin,Group:m.group}, marker:m}));
  for (const s of data.sectors) records.push({id:'sector:'+s.id,kind:'rooms',label:`Room / sector ${s.id}`,point:[...s.center,s.initial_floor],fields:{Sector:s.id,Floor:s.initial_floor,Ceiling:opened?s.open_ceiling:s.initial_ceiling,Flags:s.flags}});
  const seen = new Set();
  for (const f of data.walls[opened?'open':'initial']) {
    const id=`wall:${f.line}:${f.sector}:${f.layer}`;if(seen.has(id))continue;seen.add(id);
    const p=f.positions,average=k=>(p[k]+p[k+3]+p[k+6]+p[k+9])/4;
    records.push({id,kind:'walls',label:`Wall ${f.line} / sector ${f.sector} / ${f.layer}`,point:[average(0),-average(2),average(1)],fields:{Wall:f.line,Sector:f.sector,Layer:f.layer,Door:f.door?'Yes':'No',Texture:f.texture?.split('/').pop()??'Absent'}});
  }
  for (const d of data.movers) records.push({id:'door:'+d.id,kind:'doors',label:`${d.label} / ${d.kind}`,point:[...d.center,data.sectors[d.sector].initial_floor],fields:{Sector:d.sector,Kind:d.kind,'Initial lock':d.locked?'Locked':'Unlocked'}});
  for (const s of data.secrets) records.push({id:'secret:'+s.id,kind:'secrets',label:`${s.id} / ${s.title}`,point:[s.x,s.y,data.markers.find(m=>m.id===s.marker)?.initial_floor??0],fields:{Status:s.status},secret:s});
  return records;
}
export function filterObjects(records, query='', kind='all') {
  const words=query.toLowerCase().trim().split(/\s+/).filter(Boolean);
  return records.filter(r=>(kind==='all'||r.kind===kind)&&words.every(w=>`${r.label} ${r.kind} ${Object.values(r.fields).join(' ')}`.toLowerCase().includes(w)));
}
