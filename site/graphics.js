export const PAGE_SIZE = 60;

export function filterGraphics(entries, query = '', category = 'all', group = 'all') {
  const words = query.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
  return entries.filter(e => (category === 'all' || e.category === category)
    && (category !== 'actors' || group === 'all' || String(e.group) === group)
    && words.every(word => `${e.title} ${e.id} ${e.tags || ''} ${e.images.map(i => i.source).join(' ')}`.toLocaleLowerCase().includes(word)));
}

export function imageFor(entry, palette = 0) {
  return entry.images[palette] || entry.images[0];
}

export function createGraphicsView(data, doc = document, navigate = hash => { location.hash = hash; }) {
  const $ = id => doc.getElementById(id);
  const make = (tag, text = '', className = '') => {
    const node = doc.createElement(tag); node.textContent = text;
    if (className) node.className = className;
    return node;
  };
  let category = 'all', query = '', group = 'all', palette = 0, page = 0, selected = null;
  let filtered = data.entries;
  const byId = new Map(data.entries.map(e => [e.id, e]));
  const categories = new Map(data.categories.map(e => [e.id, e.title]));
  for (const c of [{id:'all', title:'Everything', count:data.entryCount}, ...data.categories]) {
    const option = make('option', `${c.title} (${c.count.toLocaleString()})`); option.value = c.id;
    $('graphics-category').append(option);
  }
  const groups = new Map(data.entries.filter(e => e.category === 'actors').map(e => [e.group, e.groupLabel]));
  const all = make('option', 'All sprite groups'); all.value = 'all'; $('graphics-group').append(all);
  for (const [id, label] of groups) { const option = make('option', label); option.value = String(id); $('graphics-group').append(option); }
  $('graphics-total').textContent = `${data.imageCount.toLocaleString()} original PNGs, including colour variants. Pick a picture to take a closer look.`;
  $('graphics-controls').hidden = false;
  $('graphics-status').hidden = true;

  function renderGrid() {
    const start = page * PAGE_SIZE, end = Math.min(filtered.length, start + PAGE_SIZE);
    $('graphics-count').textContent = filtered.length ? `${start + 1}–${end} of ${filtered.length.toLocaleString()} pictures` : 'No pictures found. Try another name or clear the filters.';
    $('graphics-page').textContent = `Page ${page + 1} of ${Math.max(1, Math.ceil(filtered.length / PAGE_SIZE))}`;
    $('graphics-previous-page').disabled = page === 0;
    $('graphics-next-page').disabled = end >= filtered.length;
    $('graphics-grid').replaceChildren();
    for (const entry of filtered.slice(start, end)) {
      const original = imageFor(entry, palette);
      const button = make('button', '', 'graphics-card'); button.type = 'button';
      button.setAttribute('aria-pressed', String(entry.id === selected?.id));
      const frame = make('span', '', 'graphics-thumb');
      const img = make('img'); img.src = original.path; img.alt = ''; img.width = original.width; img.height = original.height;
      img.loading = 'lazy'; img.decoding = 'async'; frame.append(img);
      button.append(frame, make('strong', entry.title), make('small', `${original.width} × ${original.height} · ${categories.get(entry.category)}`));
      button.onclick = () => navigate(`#graphics/${entry.id}`);
      $('graphics-grid').append(button);
    }
  }
  function updateImage() {
    if (!selected) return;
    const original = imageFor(selected, palette), img = $('graphics-preview');
    img.src = original.path; img.alt = selected.title; img.width = original.width; img.height = original.height;
    const zoom = $('graphics-zoom').value;
    img.style.width = zoom === 'fit' ? 'auto' : `${original.width * Number(zoom)}px`;
    img.style.height = zoom === 'fit' ? 'auto' : `${original.height * Number(zoom)}px`;
    img.style.maxWidth = zoom === 'fit' ? '100%' : 'none'; img.style.maxHeight = zoom === 'fit' ? '340px' : 'none';
    $('graphics-image-info').textContent = `${original.width} × ${original.height} pixels · ${original.label}`;
    $('graphics-source').textContent = original.source;
    $('graphics-download').href = original.path;
    $('graphics-download').download = original.source.replaceAll('/', '-');
    $('graphics-original').href = original.path;
  }
  function renderSelection() {
    $('graphics-detail').hidden = !selected;
    $('graphics-pick-hint').hidden = !!selected;
    if (!selected) return;
    $('graphics-title').textContent = selected.title;
    $('graphics-detail-category').textContent = categories.get(selected.category);
    $('graphics-note').textContent = selected.note || (selected.category === 'actors'
      ? 'One frame from the original game. Some frames are shared between sprite groups. Switch between Egypt and Rome colours above.'
      : selected.category === 'sprites' ? 'An original weapon, HUD or other on-screen sprite. Some images are pieces the game draws together.'
      : selected.category === 'textures' ? 'The original texture at one of the game’s three sizes. Search its name to find the other sizes.'
      : 'Original artwork at its original size. Zooming keeps the pixels sharp.');
    $('graphics-origin').textContent = selected.origin ? `Drawing offset: ${selected.origin.join(', ')} pixels` : '';
    const index = filtered.indexOf(selected);
    $('graphics-previous-image').disabled = index <= 0;
    $('graphics-next-image').disabled = index < 0 || index >= filtered.length - 1;
    updateImage();
  }
  function filter() {
    page = 0; filtered = filterGraphics(data.entries, query, category, group);
    $('graphics-group-label').hidden = category !== 'actors';
    renderGrid(); renderSelection();
  }
  $('graphics-category').onchange = () => { category = $('graphics-category').value; group = 'all'; $('graphics-group').value = 'all'; filter(); };
  $('graphics-group').onchange = () => { group = $('graphics-group').value; filter(); };
  $('graphics-search').oninput = () => { query = $('graphics-search').value; filter(); };
  $('graphics-clear').onclick = () => {
    category = group = 'all'; query = ''; $('graphics-category').value = 'all'; $('graphics-group').value = 'all'; $('graphics-search').value = '';
    filter(); $('graphics-search').focus();
  };
  $('graphics-palette').onchange = () => { palette = Number($('graphics-palette').value); renderGrid(); updateImage(); };
  $('graphics-background').onchange = () => { $('graphics-preview-area').dataset.background = $('graphics-background').value; };
  $('graphics-zoom').onchange = updateImage;
  function turnPage(amount) {
    page = Math.max(0, Math.min(Math.max(0, Math.ceil(filtered.length/PAGE_SIZE)-1), page + amount));
    renderGrid(); $('graphics-count').focus();
  }
  $('graphics-previous-page').onclick = () => turnPage(-1);
  $('graphics-next-page').onclick = () => turnPage(1);
  function nextImage(amount) {
    const index = filtered.indexOf(selected), entry = filtered[index + amount];
    if (index >= 0 && entry) navigate(`#graphics/${entry.id}`);
  }
  $('graphics-previous-image').onclick = () => nextImage(-1);
  $('graphics-next-image').onclick = () => nextImage(1);
  function open(id = '', {focus = false} = {}) {
    selected = byId.get(id) || null;
    $('graphics-not-found').hidden = !id || !!selected;
    if (selected) {
      if (!filtered.includes(selected)) {
        category = selected.category; group = 'all'; query = '';
        $('graphics-category').value = category; $('graphics-group').value = group; $('graphics-search').value = query;
        filtered = filterGraphics(data.entries, query, category, group);
        $('graphics-group-label').hidden = category !== 'actors';
      }
      page = Math.floor(filtered.indexOf(selected)/PAGE_SIZE);
    }
    renderGrid(); renderSelection();
    doc.title = `${selected ? selected.title+' — ' : ''}Graphics — Serious Sam Advance`;
    if (focus && selected) { $('graphics-title').focus({preventScroll:true}); $('graphics-detail').scrollIntoView({block:'nearest'}); }
  }
  return {open};
}

export async function createGraphics() {
  const response = await fetch('data/graphics.json');
  if (!response.ok) throw Error('Graphics catalog unavailable');
  return createGraphicsView(await response.json());
}
