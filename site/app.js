export function createApp(catalog = [], loadViewer = () => import('./viewer3d.js').then(m => m.createViewer()), loadWiki = () => import('./wiki.js').then(m => m.createWiki())) {
  const $ = id => document.getElementById(id);
  let level = 0, route = 'project', mode = '2d', viewer, viewerPromise, atlasReady = false, revision = 0;
  let wikiPromise, catalogError = '', renderedRoute, renderedWikiId;
  const supportedLevels = Number($('project').dataset.nativeCampaignLevels);
  function focusContent(element) {
    element.tabIndex = -1;
    element.focus({preventScroll:true});
    element.scrollIntoView({block:'start'});
  }
  $('skip-content').onclick = event => { event.preventDefault(); focusContent($('main')); };
  const levelUrl = () => `#levels/${level + 1}/${mode}`;
  function fillCatalog() {
    for (const select of document.querySelectorAll('.level-select')) {
      select.replaceChildren(); select.disabled = !catalog.length;
      for (const entry of catalog) {
        const option = document.createElement('option');
        option.value = entry.level;
        option.textContent = `${String(entry.level + 1).padStart(2, '0')} / ${entry.title}`;
        select.append(option);
      }
      select.onchange = () => { level = Number(select.value); location.hash = levelUrl(); };
    }
  }
  fillCatalog();
  for (const button of document.querySelectorAll('[data-mode]')) {
    button.onclick = () => { mode = button.dataset.mode; location.hash = levelUrl(); };
  }
  async function render() {
    const ticket = ++revision;
    const parts = location.hash.slice(1).split('/');
    const legacy = ['atlas', 'explorer'].includes(parts[0]);
    route = legacy ? 'levels' : ['project', 'features', 'levels', 'wiki', 'development'].includes(parts[0]) ? parts[0] : 'project';
    const n = Number(parts[1]);
    if (route === 'levels' && Number.isInteger(n) && n >= 1 && n <= (catalog.length || 16)) level = n - 1;
    if (legacy) mode = parts[0] === 'atlas' ? '2d' : '3d';
    else if (['2d', '3d'].includes(parts[2])) mode = parts[2];
    if (route === 'levels') history.replaceState(null, '', levelUrl());
    document.querySelectorAll('.page').forEach(p => p.hidden = p.id !== route);
    if (renderedRoute !== undefined && renderedRoute !== route) focusContent($(route).querySelector('h1'));
    renderedRoute = route;
    document.querySelectorAll('[data-route]').forEach(a => {
      a.toggleAttribute('aria-current', a.dataset.route === route);
      if (a.dataset.route === route) a.setAttribute('aria-current', 'page');
      if (a.dataset.route === 'levels') a.href = levelUrl();
    });
    document.querySelectorAll('.level-select').forEach(s => s.value = level);
    document.querySelectorAll('[data-mode]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.mode === mode)));
    $('atlas-panel').hidden = mode !== '2d';
    $('explorer-panel').hidden = mode !== '3d';
    $('level-status').textContent = catalogError || (!catalog.length ? 'Loading the level list…' : level < supportedLevels ? 'Campaign level · available in the PC game' : level < 12 ? 'Campaign level · not yet playable' : 'Multiplayer map · explore here; multiplayer is not playable yet');
    document.title = `${{project:'PC port', features:'About the game', levels:'Level Browser', wiki:'NETRICSA Field Guide', development:'Download & Help'}[route]} — Serious Sam Advance`;
    if (route === 'levels' && mode === '2d') {
      if (!atlasReady) { $('atlas-frame').src = `atlas/index.html#level-${level + 1}`; atlasReady = true; }
      else $('atlas-frame').contentWindow.postMessage({type:'ssa:set-level', level}, location.origin);
    }
    const active3d = route === 'levels' && mode === '3d' && catalog.length > 0;
    viewer?.setActive(active3d);
    if (route === 'levels' && mode === '3d' && !catalog.length) {
      $('view-message').hidden = false;
      $('view-message').textContent = catalogError || 'Loading the level list…';
    }
    if (route === 'wiki') {
      try {
        wikiPromise ??= loadWiki();
        const wiki = await wikiPromise;
        if (ticket !== revision) return;
        const id = parts[1] || 'field-guide';
        wiki.open(id, {focus:renderedWikiId !== undefined && renderedWikiId !== id});
        renderedWikiId = id;
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
        $('view-message').textContent = `3D could not start: ${error.message}. Switch to the 2D map to explore this level.`;
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
  return {render, setCatalog(entries) {
    if (!Array.isArray(entries) || entries.length !== 16 || entries.some((entry, i) => entry.level !== i || !entry.path || !entry.bounds)) throw Error('Invalid level catalog');
    catalog = entries; catalogError = ''; fillCatalog(); return render();
  }, catalogFailed() {
    catalogError = 'The level list could not load. Try reloading the page, or choose a level inside the 2D map.';
    return render();
  }};
}

if (typeof document !== 'undefined') {
  // Navigation and the field guide must work even when map data is unavailable.
  const app = createApp();
  try {
    const response = await fetch('data/catalog.json');
    if (!response.ok) throw Error('Level catalog unavailable');
    await app.setCatalog(await response.json());
  } catch (error) {
    app.catalogFailed();
  }
}
