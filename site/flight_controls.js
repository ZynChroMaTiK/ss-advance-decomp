// Track each pointer independently, so diagonal movement and cancellation work.
export function bindFlightControls(buttons, changed, enabled) {
  const held = new Map();
  function update(){
    const keys=new Set(held.values());
    for(const b of buttons)b.setAttribute('aria-pressed',String(keys.has(b.dataset.flightKey)));
    changed(keys);
  }
  for(const button of buttons){
    button.addEventListener('pointerdown',e=>{if(!enabled())return;e.preventDefault();held.set(e.pointerId,button.dataset.flightKey);button.setPointerCapture(e.pointerId);update()});
    for(const event of ['pointerup','pointercancel','lostpointercapture'])button.addEventListener(event,e=>{held.delete(e.pointerId);update()});
    button.addEventListener('keydown',e=>{if(!enabled()||!['Space','Enter'].includes(e.code))return;e.preventDefault();held.set('keyboard:'+button.dataset.flightKey,button.dataset.flightKey);update()});
    button.addEventListener('keyup',e=>{if(!['Space','Enter'].includes(e.code))return;e.preventDefault();held.delete('keyboard:'+button.dataset.flightKey);update()});
    button.addEventListener('blur',()=>{held.delete('keyboard:'+button.dataset.flightKey);update()});
  }
  return {clear(){held.clear();update()}};
}
