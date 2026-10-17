// Only the requested level is fetched; failed requests can be retried.
export function createLevelLoader(fetcher = fetch, maxCached = 3) {
  const cache = new Map();
  return function load(n) {
    if (!Number.isInteger(n) || n < 0 || n >= 16) return Promise.reject(Error('Invalid level'));
    if (cache.has(n)) {
      const value = cache.get(n); cache.delete(n); cache.set(n, value); return value;
    }
    const id = String(n + 1).padStart(2, '0');
    const request = Promise.all([['data', 'json'], ['views', 'svg']].map(async ([folder, ext]) => {
      const response = await fetcher(`${folder}/level-${id}.${ext}`);
      if (!response.ok) throw Error(`Level ${n + 1} could not load (${response.status})`);
      return ext === 'json' ? response.json() : response.text();
    })).then(([map, svg]) => {
      if (map.level !== n || !Array.isArray(map.markers) || !svg.includes('<svg')) throw Error('Invalid level data');
      return {map, svg};
    }).catch(error => { if (cache.get(n) === request) cache.delete(n); throw error; });
    cache.set(n, request);
    while (cache.size > maxCached) cache.delete(cache.keys().next().value);
    return request;
  };
}
