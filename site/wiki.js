export function searchEntries(entries, query = '', category = 'all') {
  const words = query.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
  return entries.filter(entry => (category === 'all' || entry.category === category) && words.every(word => [entry.title, entry.category, entry.summary, ...(entry.aliases || []), ...entry.sections.map(s => s.text)].join(' ').toLocaleLowerCase().includes(word)));
}

export function createWikiView(entries, doc = document) {
  const $ = id => doc.getElementById(id);
  const node = (tag, text, className) => { const el = doc.createElement(tag); if (text) el.textContent = text; if (className) el.className = className; return el; };
  const link = (text, href, className) => { const el = node('a', text, className); el.href = href; return el; };
  let category = 'all', current = '';
  const categories = ['all', 'levels', 'weapons', 'enemies', 'items', 'systems', 'secrets'];
  const names = {all:'All entries',levels:'Levels',weapons:'Weapons',enemies:'Enemies',items:'Items',systems:'Playing tips',secrets:'Secrets'};
  function renderList() {
    const results = searchEntries(entries, $('wiki-search').value, category);
    $('wiki-count').textContent = `${results.length} of ${entries.length} entries`;
    $('wiki-results').replaceChildren();
    $('wiki-empty').hidden = results.length !== 0;
    for (const entry of results) {
      const row = link('', `#wiki/${entry.id}`, 'wiki-result');
      if (entry.id === current) row.setAttribute('aria-current', 'page');
      if (entry.image) { const img = node('img'); img.src = entry.image; img.alt = ''; img.loading = 'lazy'; row.append(img); }
      else row.append(node('span', entry.category === 'levels' ? String(entry.level + 1).padStart(2, '0') : 'N', 'wiki-glyph'));
      const body = node('span'); body.append(node('strong', entry.title), node('small', names[entry.category])); row.append(body); $('wiki-results').append(row);
    }
    for (const button of $('wiki-categories').children) button.setAttribute('aria-pressed', String(button.dataset.category === category));
  }
  for (const key of categories) {
    const button = node('button', `${names[key]} ${key === 'all' ? entries.length : entries.filter(e => e.category === key).length}`);
    button.type = 'button'; button.dataset.category = key;
    button.onclick = () => { category = key; renderList(); };
    $('wiki-categories').append(button);
  }
  $('wiki-search').addEventListener('input', renderList);
  $('wiki-clear').onclick = () => { category = 'all'; $('wiki-search').value = ''; renderList(); $('wiki-search').focus(); };
  function open(id = 'field-guide', {focus = false} = {}) {
    const article = $('wiki-article'); article.replaceChildren();
    const entry = entries.find(e => e.id === id);
    if (!entry) {
      current = ''; article.append(node('p', 'ENTRY NOT FOUND', 'eyebrow'), node('h2', 'We couldn’t find that entry.'), link('Return to the field guide →', '#wiki/field-guide', 'text-link')); doc.title = 'Entry not found — NETRICSA — Serious Sam Advance'; renderList(); if (focus) article.focus(); return;
    }
    current = entry.id;
    if (!searchEntries(entries, $('wiki-search').value, category).includes(entry)) { category = 'all'; $('wiki-search').value = ''; }
    const heading = node('div', '', 'wiki-entry-heading');
    const copy = node('div'); copy.append(node('p', `NETRICSA / ${names[entry.category].toUpperCase()}`, 'eyebrow'), node('h2', entry.title), node('p', entry.summary, 'wiki-summary')); heading.append(copy);
    if (entry.image) { const img = node('img'); img.src = entry.image; img.alt = entry.title + (entry.image.startsWith('assets/netricsa/') ? ' — original NETRICSA illustration' : ' — original game sprite'); img.className = 'wiki-entry-sprite'; heading.append(img); }
    article.append(heading);
    if (entry.facts?.length) { const facts = node('dl', '', 'wiki-facts'); for (const [label, value] of entry.facts) { const item = node('div'); item.append(node('dt', label), node('dd', String(value))); facts.append(item); } article.append(facts); }
    for (const section of entry.sections) { const block = node('section'); block.append(node('h3', section.title), node('p', section.text)); article.append(block); }
    if (entry.level !== undefined) { const actions = node('div', '', 'actions'); actions.append(link('Explore in 3D ↗', `#levels/${entry.level + 1}/3d`, 'button primary')); article.append(actions); }
    if (entry.appearances?.length) {
      const block = node('section'); block.append(node('h3', 'Where to find it'), node('p', 'Here’s where it appears. Some enemies and items arrive later, so they won’t all be there at once.', 'fine'));
      const links = node('div', '', 'wiki-related'); for (const item of entry.appearances) links.append(link(`${String(item.level + 1).padStart(2, '0')} / ${item.title} · ${item.count}`, `#wiki/level-${item.level + 1}`)); block.append(links); article.append(block);
    }
    if (entry.related?.length) { const block = node('section'); block.append(node('h3', 'Related entries')); const links = node('div', '', 'wiki-related'); for (const id of entry.related) { const related = entries.find(e => e.id === id); if (related) links.append(link(related.title, `#wiki/${id}`)); } block.append(links); article.append(block); }
    const evidence = node('details', '', 'wiki-evidence'); evidence.append(node('summary', 'Sources & notes'));
    evidence.append(node('p', entry.coverage || 'These notes come from playing and studying the original game. The pictures are from the GBA game; the descriptions were written for this guide.'));
    if (Number.isInteger(entry.original_text_id)) evidence.append(node('p', `Original English NETRICSA entry ${entry.original_text_id}. The illustration is original game artwork; the description above was written for this guide.`));
    for (const source of entry.sources) evidence.append(link(source.title + ' ↗', source.href, 'text-link'));
    article.append(evidence, link('Link to this entry ↗', `#wiki/${entry.id}`, 'wiki-permalink text-link'));
    doc.title = `${entry.title} — NETRICSA — Serious Sam Advance`;
    renderList();
    article.scrollTop = 0;
    if (focus) { article.focus({preventScroll:true}); article.scrollIntoView({block:'nearest'}); }
  }
  return {open};
}

export function createWiki() {
  return fetch('data/wiki.json').then(response => { if (!response.ok) throw Error('Field guide unavailable'); return response.json(); }).then(data => createWikiView(data.entries));
}
