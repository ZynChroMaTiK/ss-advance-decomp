export function createApp(catalog = [], loadViewer = () => import('./viewer3d.js').then(m => m.createViewer()), loadWiki = () => import('./wiki.js').then(m => m.createWiki()), loadGraphics = () => import('./graphics.js').then(m => m.createGraphics())) {
  const $ = id => document.getElementById(id);
  let level = 0, route = 'project', viewer, viewerPromise, revision = 0;
  let wikiPromise, catalogError = '', renderedRoute, renderedWikiId;
  let graphicsPromise, renderedGraphicId;
  const supportedLevels = Number($('project').dataset.nativeCampaignLevels);
  function focusContent(element) {
    element.tabIndex = -1;
    element.focus({preventScroll:true});
    element.scrollIntoView({block:'start'});
  }
  $('skip-content').onclick = event => { event.preventDefault(); focusContent($('main')); };
  const levelUrl = () => `#levels/${level + 1}/3d`;
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
  async function render() {
    const ticket = ++revision;
    const parts = location.hash.slice(1).split('/');
    const legacy = ['atlas', 'explorer'].includes(parts[0]);
    route = legacy ? 'levels' : ['project', 'features', 'levels', 'wiki', 'graphics', 'development'].includes(parts[0]) ? parts[0] : 'project';
    const n = Number(parts[1]);
    if (route === 'levels' && Number.isInteger(n) && n >= 1 && n <= (catalog.length || 16)) level = n - 1;
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
    $('level-status').textContent = catalogError || (!catalog.length ? 'Loading the level list…' : level < supportedLevels ? 'Campaign level · available in the PC game' : level < 12 ? 'Campaign level · not yet playable' : 'Multiplayer map · explore here; multiplayer is not playable yet');
    document.title = `${{project:'PC port', features:'About the game', levels:'Level Browser', wiki:'NETRICSA Field Guide', graphics:'Graphics', development:'Download & Help'}[route]} — Serious Sam Advance`;
    const active3d = route === 'levels' && catalog.length > 0;
    viewer?.setActive(active3d);
    if (route === 'levels' && !catalog.length) {
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
    if (route === 'graphics') {
      if (!graphicsPromise) {
        $('graphics-status').hidden = false;
        $('graphics-status-text').textContent = 'Loading the pictures…';
      }
      $('graphics-retry').hidden = true;
      try {
        graphicsPromise ??= loadGraphics();
        const gallery = await graphicsPromise;
        if (ticket !== revision) return;
        const id = parts[1] || '';
        gallery.open(id, {focus:!!id && renderedGraphicId !== id, palette:parts[2] === 'rome' ? 1 : 0});
        renderedGraphicId = id;
        $('graphics-status').hidden = true;
      } catch (error) {
        graphicsPromise = null;
        if (ticket !== revision) return;
        $('graphics-status').hidden = false;
        $('graphics-status-text').textContent = 'The pictures couldn’t load. Check your connection and try again.';
        $('graphics-retry').hidden = false;
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
        $('view-message').textContent = `The 3D view could not start: ${error.message}. Try reloading the page or using a browser with WebGL2 enabled.`;
      }
    }
  }
  $('graphics-retry').onclick = () => render();
  window.addEventListener('hashchange', render);
  render();
  return {render, setCatalog(entries) {
    if (!Array.isArray(entries) || entries.length !== 16 || entries.some((entry, i) => entry.level !== i || !entry.path || !entry.bounds)) throw Error('Invalid level catalog');
    catalog = entries; catalogError = ''; fillCatalog(); return render();
  }, catalogFailed() {
    catalogError = 'The level list could not load. Check your connection and reload the page.';
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
