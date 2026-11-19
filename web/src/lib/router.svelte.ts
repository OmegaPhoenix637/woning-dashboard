// Eenvoudige hash-router (werkt zonder serverconfiguratie op GitHub Pages).
export type Route = 'overzicht' | 'energie' | 'batterijen' | 'klimaat' | 'apparaten' | 'vergelijken' | 'instellingen';

export const ROUTES: Route[] = ['overzicht', 'energie', 'batterijen', 'klimaat', 'apparaten', 'vergelijken', 'instellingen'];

function parse(): { route: Route; params: URLSearchParams } {
  const raw = location.hash.replace(/^#\/?/, '');
  const [path, query] = raw.split('?');
  const r = (path || 'overzicht') as Route;
  return { route: ROUTES.includes(r) ? r : 'overzicht', params: new URLSearchParams(query ?? '') };
}

class Router {
  route = $state<Route>(parse().route);
  params = $state(parse().params);

  constructor() {
    window.addEventListener('hashchange', () => {
      const p = parse();
      if (p.route !== this.route) window.scrollTo({ top: 0, behavior: 'instant' as ScrollBehavior });
      this.route = p.route;
      this.params = p.params;
    });
  }

  go(route: Route, params?: Record<string, string>) {
    const q = params ? '?' + new URLSearchParams(params).toString() : '';
    location.hash = `#/${route}${q}`;
  }
}

export const router = new Router();
export const href = (route: Route, params?: Record<string, string>) =>
  `#/${route}${params ? '?' + new URLSearchParams(params).toString() : ''}`;
