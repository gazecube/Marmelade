export async function navigationSources(browser) {
  const items = await browser.evaluate(`(() => {
    const routeFor = href => {
      let path;
      try { path = new URL(href, location.origin).pathname.replace(/\\/+$/, '') || '/'; }
      catch { return ''; }
      if (path === '/listen-now' || path === '/home') return '/v1/listen-now';
      if (path === '/radio') return '/v1/radio';
      if (path === '/library/albums') return '/v1/library/albums';
      if (path === '/library/artists') return '/v1/library/artists';
      if (path === '/library/songs') return '/v1/library/songs';
      return '';
    };
    const roots = [...document.querySelectorAll(
      'nav, aside, [role="navigation"], [class*="sidebar"], [data-testid*="sidebar"]')];
    const links = roots.length ? roots.flatMap(root => [...root.querySelectorAll('a[href]')]) :
      [...document.querySelectorAll('a[href]')];
    const seen = new Set();
    const result = [];
    for (const link of links) {
      const path = routeFor(link.href);
      const title = (link.innerText || link.getAttribute('aria-label') || link.title || '').trim();
      if (!path || !title || seen.has(path)) continue;
      seen.add(path);
      result.push({title, path});
    }
    return result;
  })()`);

  if (items.length) return items;
  return [
    {title:'Home', path:'/v1/listen-now'},
    {title:'Radio', path:'/v1/radio'},
    {title:'Artists', path:'/v1/library/artists'},
    {title:'Albums', path:'/v1/library/albums'},
    {title:'Songs', path:'/v1/library/songs'}
  ];
}
