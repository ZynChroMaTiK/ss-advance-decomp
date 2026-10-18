export const PAGE_SIZE = 60;
export function filterGraphics(entries, query = '', category = 'all', group = 'all') {
  const words = query.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
  return entries.filter(e => e.uses.some(u => (category === 'all' || u.section === category) && (group === 'all' || u.id === group))
    && words.every(word => `${e.title} ${e.collectionTitle || ''} ${e.id} ${e.tags || ''} ${e.uses.map(u => u.title).join(' ')} ${e.images.map(i => i.source).join(' ')}`.toLocaleLowerCase().includes(word)));
}
export function imageFor(entry, palette = 0) { return entry.images[palette] || entry.images[0]; }
export function uniqueGraphics(entries, palette = 0, preferred = null) {
  const pictures = new Map();
  for (const entry of entries) {
    const key = imageFor(entry, palette).path;
    if (!pictures.has(key) || entry === preferred) pictures.set(key, entry);
  }
  return [...pictures.values()];
}
export function groupGraphics(entries, palette = 0, preferred = null, section = 'all', usage = 'all') {
  const groups = new Map();
  for (const entry of entries) {
    for (const use of entry.uses) {
      if ((section !== 'all' && use.section !== section) || (usage !== 'all' && use.id !== usage)) continue;
      if (!groups.has(use.id)) groups.set(use.id, {key:use.id, title:use.title, section:use.section, entries:[]});
      groups.get(use.id).entries.push(entry);
    }
  }
  return [...groups.values()].map(g => ({...g, entries:uniqueGraphics(g.entries,palette,preferred)}))
    .sort((a,b) => a.title.localeCompare(b.title));
}
export function graphicsUrl(id = '', palette = 0) {
  return `#graphics${id || palette ? '/'+id : ''}${palette ? '/rome' : ''}`;
}
export function createGraphicsView(data, doc = document, navigate = (hash, {replace = false} = {}) => {
  if (replace) history.replaceState(null, '', hash); else location.hash = hash;
}) {
  const $ = id => doc.getElementById(id);
  const make = (tag, text = '', className = '') => {
    const node = doc.createElement(tag); node.textContent = text;
    if (className) node.className = className;
    return node;
  };
  let category = 'all', query = '', group = 'all', palette = 0, page = 0, selected = null;
  let grouped = true, collection = null, returnSpot = null, cards = new Map();
  let filtered = data.entries;
  const byId = new Map(data.entries.map(e => [e.id, e]));
  for (const [alias, id] of Object.entries(data.aliases || {})) byId.set(alias, byId.get(id));
  const categories = new Map(data.categories.map(e => [e.id, e.title]));
  const uses = new Map(data.entries.flatMap(e => e.uses.map(u => [u.id,u])));
  for (const c of [{id:'all', title:'Everywhere'}, ...data.usageSections]) {
    const option = make('option', c.title); option.value = c.id; $('graphics-category').append(option);
  }
  function updateUses() {
    $('graphics-group').replaceChildren();
    const all = make('option', 'Every use'); all.value = 'all'; $('graphics-group').append(all);
    for (const use of [...uses.values()].filter(u => category === 'all' || u.section === category).sort((a,b) => a.title.localeCompare(b.title))) {
      const option = make('option', use.title); option.value = use.id; $('graphics-group').append(option);
    }
    $('graphics-group').value = group;
  }
  updateUses();
  $('graphics-total').textContent = `Browse ${data.uniqueFiles.toLocaleString()} unique pictures by their role in the game, including different sizes and Egypt and Rome colours.`;
  $('graphics-controls').hidden = false; $('graphics-status').hidden = true;
  const frames = () => uniqueGraphics(collection ? filtered.filter(e => e.uses.some(u => u.id === collection)) : filtered, palette, selected);
  const items = () => grouped && !collection ? groupGraphics(filtered, palette, selected, category, group)
    : frames().map(e => ({key:e.id, title:e.title, entries:[e]}));
  const route = (id = '', replace = false) => navigate(graphicsUrl(id, palette), {replace});
  function renderGrid() {
    const list = items();
    page = Math.max(0, Math.min(page, Math.ceil(list.length/PAGE_SIZE)-1));
    const start = page*PAGE_SIZE, end = Math.min(list.length, start+PAGE_SIZE);
    const noun = grouped && !collection ? 'groups and pictures' : 'pictures';
    $('graphics-count').textContent = list.length ? `${start+1}–${end} of ${list.length.toLocaleString()} ${noun}` : 'No pictures found. Try another name or clear the filters.';
    $('graphics-page').textContent = `Page ${page+1} of ${Math.max(1, Math.ceil(list.length/PAGE_SIZE))}`;
    $('graphics-previous-page').disabled = page === 0; $('graphics-next-page').disabled = end >= list.length;
    $('graphics-back-group').hidden = !collection;
    $('graphics-collection').textContent = uses.get(collection)?.title || '';
    $('graphics-group-label').hidden = false;
    $('graphics-palette-label').hidden = !(selected?.category === 'actors' || frames().some(e => e.category === 'actors'));
    cards = new Map(); $('graphics-grid').replaceChildren();
    for (const item of list.slice(start, end)) {
      const entry = item.entries[0], original = imageFor(entry, palette), isGroup = item.entries.length > 1;
      const button = make('button', '', 'graphics-card'); button.type = 'button';
      button.setAttribute('aria-pressed', String(item.entries.includes(selected)));
      const frame = make('span', '', 'graphics-thumb'), img = make('img');
      img.src = original.path; img.alt = ''; img.width = original.width; img.height = original.height;
      img.loading = 'lazy'; img.decoding = 'async'; frame.append(img);
      button.append(frame, make('strong', item.title), make('small', isGroup
        ? `${item.entries.length} pictures · Open group →` : `${original.width} × ${original.height} · ${categories.get(entry.category)}`));
      button.onclick = () => {
        if (isGroup) {
          collection = item.key; page = 0; selected = null; returnSpot = null; route('', true);
          renderGrid(); renderSelection(); $('graphics-count').focus();
        } else { returnSpot = {id:entry.id, usage:grouped && !collection ? item.key : collection, y:globalThis.window?.scrollY}; route(entry.id); }
      };
      cards.set(item.key, button); $('graphics-grid').append(button);
    }
  }
  function updateImage() {
    if (!selected) return;
    const original = imageFor(selected, palette), img = $('graphics-preview'), zoom = $('graphics-zoom').value;
    img.src = original.path; img.alt = selected.title; img.width = original.width; img.height = original.height;
    img.style.width = zoom === 'fit' ? 'auto' : `${original.width*Number(zoom)}px`;
    img.style.height = zoom === 'fit' ? 'auto' : `${original.height*Number(zoom)}px`;
    img.style.maxWidth = zoom === 'fit' ? '100%' : 'none'; img.style.maxHeight = zoom === 'fit' ? '340px' : 'none';
    $('graphics-image-info').textContent = `${original.width} × ${original.height} pixels · ${original.label}`;
    $('graphics-source').textContent = original.source;
    $('graphics-download').href = original.path; $('graphics-download').download = original.source.replaceAll('/', '-');
    $('graphics-original').href = original.path;
  }
  function renderSelection() {
    $('graphics-detail').hidden = !selected; $('graphics-pick-hint').hidden = !!selected;
    $('graphics-previous-image').disabled = $('graphics-next-image').disabled = true;
    if (!selected) { doc.title = 'Graphics — Serious Sam Advance'; return; }
    $('graphics-title').textContent = selected.title;
    $('graphics-detail-category').textContent = categories.get(selected.category);
    $('graphics-usage').textContent = 'Used for: '+selected.uses.map(u => u.title).join('; ')+'.';
    $('graphics-note').textContent = selected.note || (selected.category === 'actors'
      ? 'An original sprite frame. Use Sprite colours to see its Egypt and Rome versions.'
      : selected.category === 'sprites' ? 'Original weapon or on-screen artwork. Some images are pieces the game draws together.'
      : selected.category === 'textures' ? 'An original texture. Uses are matched to the walls and doors in the exported levels.'
      : 'Original artwork. Zoom in to see the pixels.');
    $('graphics-origin').textContent = selected.origin ? `Drawing offset: ${selected.origin.join(', ')} pixels` : '';
    const sequence = frames(), index = sequence.indexOf(selected);
    $('graphics-previous-image').disabled = index <= 0;
    $('graphics-next-image').disabled = index < 0 || index >= sequence.length-1;
    doc.title = `${selected.title} — Graphics — Serious Sam Advance`; updateImage();
  }
  function filter() {
    page = 0; collection = null; filtered = filterGraphics(data.entries, query, category, group);
    if (selected && !filtered.includes(selected)) { selected = null; returnSpot = null; route('', true); }
    $('graphics-not-found').hidden = true; renderGrid(); renderSelection();
  }
  $('graphics-category').onchange = () => { category = $('graphics-category').value; group = 'all'; updateUses(); filter(); };
  $('graphics-group').onchange = () => { group = $('graphics-group').value; filter(); };
  $('graphics-search').oninput = () => { query = $('graphics-search').value; filter(); };
  $('graphics-clear').onclick = () => {
    category = group = 'all'; query = ''; $('graphics-category').value = category; $('graphics-group').value = group; $('graphics-search').value = query;
    updateUses(); filter(); $('graphics-search').focus();
  };
  $('graphics-view').onchange = () => { grouped = $('graphics-view').value !== 'frames'; page = 0; collection = null; renderGrid(); renderSelection(); };
  $('graphics-back-group').onclick = () => {
    const key = collection; collection = null; selected = null; returnSpot = null; route('', true);
    page = Math.max(0, Math.floor(items().findIndex(e => e.key === key)/PAGE_SIZE));
    renderGrid(); renderSelection(); cards.get(key)?.focus();
  };
  $('graphics-back-pictures').onclick = () => {
    const id = selected?.id; selected = null; route('', true); renderGrid(); renderSelection();
    const button = cards.get(id) || cards.get(collection) || cards.get(returnSpot?.usage) || byId.get(id)?.uses.map(u => cards.get(u.id)).find(Boolean);
    button?.focus({preventScroll:true});
    if (returnSpot?.id === id && Number.isFinite(returnSpot.y)) globalThis.window?.scrollTo({top:returnSpot.y, behavior:'instant'});
    else button?.scrollIntoView({block:'nearest'});
    returnSpot = null;
  };
  $('graphics-palette').onchange = () => { palette = Number($('graphics-palette').value); page = 0; route(selected?.id || '', true); renderGrid(); renderSelection(); };
  $('graphics-background').onchange = () => { $('graphics-preview-area').dataset.background = $('graphics-background').value; };
  $('graphics-zoom').onchange = updateImage;
  function turnPage(amount) { page += amount; renderGrid(); $('graphics-count').focus(); }
  $('graphics-previous-page').onclick = () => turnPage(-1); $('graphics-next-page').onclick = () => turnPage(1);
  function nextImage(amount) {
    const sequence = frames(), index = sequence.indexOf(selected), entry = sequence[index+amount];
    if (index >= 0 && entry) { returnSpot = null; route(entry.id); }
  }
  $('graphics-previous-image').onclick = () => nextImage(-1); $('graphics-next-image').onclick = () => nextImage(1);
  function open(id = '', {focus = false, palette:requestedPalette = 0} = {}) {
    const changed = selected?.id !== id;
    selected = byId.get(id) || null; palette = requestedPalette === 1 ? 1 : 0; $('graphics-palette').value = String(palette);
    $('graphics-not-found').hidden = !id || !!selected;
    if (selected) {
      if (!filtered.includes(selected)) {
        category = selected.uses[0].section; group = 'all'; query = '';
        $('graphics-category').value = category; $('graphics-group').value = group; $('graphics-search').value = query;
        updateUses();
        filtered = filterGraphics(data.entries, query, category, group);
      }
      if (grouped) {
        const context = returnSpot?.id === selected.id ? returnSpot.usage : collection;
        const key = selected.uses.some(u => u.id === context) ? context
          : selected.uses.find(u => (category === 'all' || u.section === category) && (group === 'all' || u.id === group))?.id;
        collection = key && uniqueGraphics(filtered.filter(e => e.uses.some(u => u.id === key)),palette,selected).length > 1 ? key : null;
      }
      const list = items(), fromUse = !collection && returnSpot?.id === selected.id ? list.findIndex(e => e.key === returnSpot.usage) : -1;
      page = Math.floor((fromUse >= 0 ? fromUse : list.findIndex(e => e.entries.includes(selected)))/PAGE_SIZE);
    }
    renderGrid(); renderSelection();
    if ((focus || changed) && selected) { $('graphics-title').focus({preventScroll:true}); $('graphics-detail').scrollIntoView({block:'nearest'}); }
  }
  return {open};
}
export async function createGraphics() {
  const response = await fetch('data/graphics.json');
  if (!response.ok) throw Error('Graphics catalog unavailable');
  return createGraphicsView(await response.json());
}
