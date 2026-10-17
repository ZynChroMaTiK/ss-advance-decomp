// Keep bookmarked historical headings reachable even when their group is closed.
export function revealReference(hash = location.hash, doc = document) {
  let id; try { id = decodeURIComponent(hash.slice(1)); } catch { return; }
  const target = doc.getElementById(id); if (!target) return;
  let parent = target.parentElement, opened = false;
  while (parent) {
    if (parent.tagName === 'DETAILS' && !parent.open) { parent.open = true; opened = true; }
    parent = parent.parentElement;
  }
  if (opened) target.scrollIntoView({block:'start'});
}
if (typeof window !== 'undefined') {
  window.addEventListener('hashchange', () => revealReference());
  revealReference();
}
