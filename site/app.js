export function createApp(catalog, loadViewer = () => import('./viewer3d.js').then(m => m.createViewer()), loadWiki = () => import('./wiki.js').then(m => m.createWiki())) {
  const $ = id => document.getElementById(id);
  let level = 0, route = 'project', mode = '2d', viewer, viewerPromise, atlasReady = false, revision = 0;
  let wikiPromise;
  const levelUrl = () => `#levels/${level + 1}/${mode}`;
  for (const select of document.querySelectorAll('.level-select')) {
    for (const entry of catalog) {
      const option = document.createElement('option');
      option.value = entry.level;
      option.textContent = `${String(entry.level + 1).padStart(2, '0')} / ${entry.title}`;
      select.append(option);
    }
    select.onchange = () => { level = Number(select.value); location.hash = levelUrl(); };
  }
  for (const button of document.querySelectorAll('[data-mode]')) {
    button.onclick = () => { mode = button.dataset.mode; location.hash = levelUrl(); };
  }
  async function render() {
    const ticket = ++revision;
    const parts = location.hash.slice(1).split('/');
    const legacy = ['atlas', 'explorer'].includes(parts[0]);
    route = legacy ? 'levels' : ['project', 'features', 'levels', 'wiki', 'development'].includes(parts[0]) ? parts[0] : 'project';
    const n = Number(parts[1]);
    if (route === 'levels' && Number.isInteger(n) && n >= 1 && n <= catalog.length) level = n - 1;
    if (legacy) mode = parts[0] === 'atlas' ? '2d' : '3d';
    else if (['2d', '3d'].includes(parts[2])) mode = parts[2];
    if (route === 'levels') history.replaceState(null, '', levelUrl());
    document.querySelectorAll('.page').forEach(p => p.hidden = p.id !== route);
    document.querySelectorAll('[data-route]').forEach(a => {
      a.toggleAttribute('aria-current', a.dataset.route === route);
      if (a.dataset.route === route) a.setAttribute('aria-current', 'page');
      if (a.dataset.route === 'levels') a.href = levelUrl();
    });
    document.querySelectorAll('.level-select').forEach(s => s.value = level);
    document.querySelectorAll('[data-mode]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.mode === mode)));
    $('atlas-panel').hidden = mode !== '2d';
    $('explorer-panel').hidden = mode !== '3d';
    $('level-status').textContent = level < 10 ? 'Campaign · supported by the native prototype' : level < 12 ? 'Campaign · native progression still in development' : 'Multiplayer arena · exploration available; multiplayer integration remains unfinished';
    document.title = `${{project:'Native PC Port', features:'Features & Roadmap', levels:'Level Browser', wiki:'NETRICSA Field Guide', development:'Build & Development'}[route]} — Serious Sam Advance`;
    if (route === 'levels' && mode === '2d') {
      if (!atlasReady) { $('atlas-frame').src = `atlas/index.html#level-${level + 1}`; atlasReady = true; }
      else $('atlas-frame').contentWindow.postMessage({type:'ssa:set-level', level}, location.origin);
    }
    const active3d = route === 'levels' && mode === '3d';
    viewer?.setActive(active3d);
    if (route === 'wiki') {
      try {
        wikiPromise ??= loadWiki();
        const wiki = await wikiPromise;
        if (ticket !== revision) return;
        wiki.open(parts[1] || 'field-guide');
      } catch (error) {
        wikiPromise = null;
        if (ticket !== revision) return;
        $('wiki-article').textContent = `The field guide could not load: ${error.message}. Reload the page to retry.`;
      }
    }
    if (active3d) {
      try {
        viewerPromise ??= loadViewer();
        viewer = await viewerPromise;
        if (ticket !== revision) return;
        viewer.setActive(true);
        await viewer.load(level, catalog[level]);
      } catch (error) {
        viewerPromise = null;
        if (ticket !== revision) return;
        $('view-message').hidden = false;
        $('view-message').textContent = `3D could not start: ${error.message}. Switch to 2D Atlas to explore this level.`;
      }
    }
  }
  $('atlas-frame').addEventListener('load', () => $('atlas-frame').contentWindow.postMessage({type:'ssa:set-level', level}, location.origin));
  window.addEventListener('hashchange', render);
  window.addEventListener('message', e => {
    if (e.origin === location.origin && e.source === $('atlas-frame').contentWindow && e.data?.type === 'ssa:level' && Number.isInteger(e.data.level) && e.data.level >= 0 && e.data.level < catalog.length && route === 'levels' && mode === '2d' && e.data.level !== level) {
      level = e.data.level; history.replaceState(null, '', levelUrl()); render();
    }
  });
  render();
  return {render};
}

if (typeof document !== 'undefined') {
  try {
    const response = await fetch('data/catalog.json');
    if (!response.ok) throw Error('Level catalog unavailable');
    createApp(await response.json());
  } catch (error) {
    document.getElementById('level-status').textContent = `${error.message}. Reload to retry.`;
  }
}
