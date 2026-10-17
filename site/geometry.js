// Pure geometry helpers shared by the viewer and the all-level validation.
export function floorPositions(loops,height){const out=[];for(const loop of loops)for(let i=1;i<loop.length-1;i++)for(const [x,y] of [loop[0],loop[i],loop[i+1]])out.push(x/64,height/64,-y/64);return out}
export function wallBuffers(faces){const positions=[],uvs=[],indices=[];for(const f of faces){const base=positions.length/3;positions.push(...f.positions.map(v=>v/64));uvs.push(...f.uvs);indices.push(base,base+1,base+2,base,base+2,base+3)}return {positions,uvs,indices}}
