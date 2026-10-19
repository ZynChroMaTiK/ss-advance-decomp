export function hasPaletteVariants(entry) {
  return entry.images.length > 1 && entry.images[0].path !== entry.images[1].path;
}
export function resolutionDescription(entry) {
  if (!entry.mipCount) return '';
  if (entry.mipCount === 1) return 'One stored resolution. Animation frames and viewing angles are not extra mip levels.';
  return `Mip level ${entry.mip} · ${entry.resolutionPercent}% scale. There are ${entry.mipCount} stored sizes for different distances.`;
}
export function filterGraphics(entries, query = '', category = 'all') {
  const words = query.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
  return entries.filter(e => e.uses.some(u => category === 'all' || u.section === category)
    && words.every(word => `${e.title} ${e.collectionTitle || ''} ${e.id} ${e.tags || ''} ${e.resolutionLabel || ''} ${e.brightnessLabel ? 'brightness variants '+e.brightnessLabel : ''} ${e.unused ? 'unused' : ''} ${hasPaletteVariants(e) ? 'palette variants' : ''} ${e.uses.map(u => u.title).join(' ')} ${e.images.map(i => i.source).join(' ')}`.toLocaleLowerCase().includes(word)));
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
  let category = 'all', query = '', palette = 0, selected = null;
  let cards = new Map(), renderedPalette = null, selectedCard = null;
  let filtered = data.entries;
  const byId = new Map(data.entries.map(e => [e.id, e]));
  for (const [alias, id] of Object.entries(data.aliases || {})) byId.set(alias, byId.get(id));
  for (const c of [{id:'all', title:'All'}, ...data.usageSections]) {
    const option = make('option', c.title); option.value = c.id; $('graphics-category').append(option);
  }
  $('graphics-total').textContent = `Browse ${data.uniqueFiles.toLocaleString()} unique pictures by their role in the game, including different sizes and Egypt and Rome colours.`;
  $('graphics-controls').hidden = false; $('graphics-status').hidden = true;
  // Keep the preview and scroll targets below the filters as they wrap or resize.
  if (typeof ResizeObserver !== 'undefined') {
    const controls = $('graphics-controls');
    new ResizeObserver(() => {
      const height = controls.getBoundingClientRect().height;
      if (height) $('graphics').style.setProperty('--graphics-filter-height', `${height}px`);
    }).observe(controls);
  }
  const frames = () => uniqueGraphics(filtered, palette, selected);
  const route = (id = '', replace = false) => navigate(graphicsUrl(id, palette), {replace});
  function renderGrid() {
    const list = frames();
    $('graphics-count').textContent = list.length ? `${list.length.toLocaleString()} pictures` : 'No pictures found. Try another name or clear the filters.';
    $('graphics-palette-label').hidden = !(selected?.category === 'actors' || list.some(e => e.category === 'actors'));
    // Selection changes the preview, not the result list. Keep thumbnail nodes
    // in place to preserve keyboard focus and the browser's scroll anchor.
    if (renderedPalette === palette && cards.size === list.length && list.every(e => cards.has(e.id))) {
      selectedCard?.setAttribute('aria-pressed', 'false');
      selectedCard = cards.get(selected?.id);
      selectedCard?.setAttribute('aria-pressed', 'true');
      return;
    }
    cards = new Map(); $('graphics-grid').replaceChildren();
    for (const entry of list) {
      const original = imageFor(entry, palette);
      const button = make('button', '', 'graphics-card'); button.type = 'button';
      button.setAttribute('aria-pressed', String(entry === selected));
      const frame = make('span', '', 'graphics-thumb'), img = make('img');
      img.src = original.path; img.alt = ''; img.width = original.width; img.height = original.height;
      img.loading = 'lazy'; img.decoding = 'async'; frame.append(img);
      button.append(frame, make('strong', entry.title), make('small', `${original.width} × ${original.height}`));
      const chips = make('span', '', 'graphics-chips');
      if (entry.unused) {
        const chip = make('span', 'Unused', 'graphics-unused'); chip.title = entry.unusedReason; chips.append(chip);
      }
      if (hasPaletteVariants(entry)) {
        const chip = make('span', 'Palette variants', 'graphics-palette-variant');
        chip.title = 'Different Egypt and Rome colours. Use Sprite colours to switch.'; chips.append(chip);
      }
      if (entry.resolutionLabel) {
        const chip = make('span', entry.resolutionLabel, 'graphics-resolution');
        chip.title = resolutionDescription(entry); chips.append(chip);
      }
      if (entry.brightnessLabel) chips.append(make('span', entry.brightnessLabel, 'graphics-brightness'));
      if (chips.children.length) button.append(chips);
      button.onclick = () => route(entry.id);
      cards.set(entry.id, button); $('graphics-grid').append(button);
    }
    renderedPalette = palette; selectedCard = cards.get(selected?.id);
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
    $('graphics-unused').hidden = !selected.unused;
    $('graphics-unused-note').hidden = !selected.unused;
    $('graphics-unused-note').textContent = selected.unusedReason || '';
    $('graphics-palette-variant').hidden = !hasPaletteVariants(selected);
    $('graphics-palette-note').hidden = !hasPaletteVariants(selected);
    $('graphics-resolution').hidden = !selected.resolutionLabel;
    $('graphics-resolution').textContent = selected.resolutionLabel || '';
    $('graphics-resolution').title = resolutionDescription(selected);
    $('graphics-resolution-note').textContent = resolutionDescription(selected);
    $('graphics-resolution-note').hidden = !selected.mipCount;
    $('graphics-brightness').hidden = !selected.brightnessLabel;
    $('graphics-brightness').textContent = selected.brightnessLabel || '';
    const brightnessNote = $('graphics-brightness-note');
    brightnessNote.hidden = !selected.brightnessLabel;
    brightnessNote.replaceChildren();
    if (selected.brightnessLabel) {
      brightnessNote.append(make('span', 'Same design, from light to dark: '));
      selected.brightnessVariants.forEach((variant, index) => {
        if (index) brightnessNote.append(make('span', ' · '));
        const link = make('a', variant.name, 'text-link'); link.href = graphicsUrl(variant.id, palette);
        link.onclick = event => { event.preventDefault(); route(variant.id); };
        brightnessNote.append(link);
      });
    }
    $('graphics-usage').textContent = (selected.unused ? 'Artwork: ' : 'Used for: ')+selected.uses.map(u => u.title).join('; ')+'.';
    $('graphics-note').textContent = selected.note || (selected.category === 'actors'
      ? 'An original sprite from the game.'
      : selected.category === 'sprites' ? 'Original weapon or on-screen artwork. Some images are pieces the game draws together.'
      : selected.category === 'textures' ? 'An original texture from the Egypt or Rome set. Each set includes both walls and doors.'
      : 'Original artwork. Zoom in to see the pixels.');
    $('graphics-origin').textContent = selected.origin ? `Drawing offset: ${selected.origin.join(', ')} pixels` : '';
    const sequence = frames(), index = sequence.indexOf(selected);
    $('graphics-previous-image').disabled = index <= 0;
    $('graphics-next-image').disabled = index < 0 || index >= sequence.length-1;
    doc.title = `${selected.title} — Graphics — Serious Sam Advance`; updateImage();
  }
  function filter() {
    filtered = filterGraphics(data.entries, query, category);
    if (selected && !filtered.includes(selected)) { selected = null; route('', true); }
    $('graphics-not-found').hidden = true; renderGrid(); renderSelection();
  }
  $('graphics-category').onchange = () => { category = $('graphics-category').value; filter(); };
  $('graphics-search').oninput = () => { query = $('graphics-search').value; filter(); };
  $('graphics-clear').onclick = () => {
    category = 'all'; query = ''; $('graphics-category').value = category; $('graphics-search').value = query;
    filter(); $('graphics-search').focus();
  };
  $('graphics-palette').onchange = () => { palette = Number($('graphics-palette').value); route(selected?.id || '', true); renderGrid(); renderSelection(); };
  $('graphics-background').onchange = () => { $('graphics-preview-area').dataset.background = $('graphics-background').value; };
  $('graphics-zoom').onchange = updateImage;
  function nextImage(amount) {
    const sequence = frames(), index = sequence.indexOf(selected), entry = sequence[index+amount];
    if (index >= 0 && entry) route(entry.id);
  }
  $('graphics-previous-image').onclick = () => nextImage(-1); $('graphics-next-image').onclick = () => nextImage(1);
  function open(id = '', {focus = false, palette:requestedPalette = 0} = {}) {
    selected = byId.get(id) || null; palette = requestedPalette === 1 ? 1 : 0; $('graphics-palette').value = String(palette);
    $('graphics-not-found').hidden = !id || !!selected;
    if (selected) {
      if (!filtered.includes(selected)) {
        category = selected.uses[0].section; query = '';
        $('graphics-category').value = category; $('graphics-search').value = query;
        filtered = filterGraphics(data.entries, query, category);
      }
    }
    renderGrid(); renderSelection();
    if (focus && selected) $('graphics-title').focus({preventScroll:true});
  }
  return {open};
}
export async function createGraphics() {
  const response = await fetch('data/graphics.json');
  if (!response.ok) throw Error('Graphics catalog unavailable');
  return createGraphicsView(await response.json());
}
