// Leest data uit de PRIVÉ data-repo via de GitHub API.
// De koppelcode is een fine-grained token met alleen toegang tot die ene repo
// (Contents: read & write — write is nodig om tarieven op te slaan).
import type { DayData, LiveData, Settings, YearSummary } from '../types';
import { DEFAULT_SETTINGS } from '../calc/tariff';
import { todayIso } from '../date';
import { normalizeDay } from './normalize';
import type { DataSource } from './source';

export interface GithubConfig {
  owner: string;
  repo: string;
  token: string;
}

const API = 'https://api.github.com';

function b64utf8(s: string): string {
  const bytes = new TextEncoder().encode(s);
  let bin = '';
  for (const b of bytes) bin += String.fromCharCode(b);
  return btoa(bin);
}

export class GithubError extends Error {
  constructor(
    message: string,
    public status: number,
  ) {
    super(message);
  }
}

export class GithubSource implements DataSource {
  readonly kind = 'github' as const;
  private immutable = new Map<string, Promise<unknown>>();
  private yearCache = new Map<number, { at: number; data: YearSummary }>();

  constructor(private cfg: GithubConfig) {}

  private url(path: string, ref: string) {
    const p = path.split('/').map(encodeURIComponent).join('/');
    return `${API}/repos/${encodeURIComponent(this.cfg.owner)}/${encodeURIComponent(this.cfg.repo)}/contents/${p}?ref=${ref}`;
  }

  private headers(accept = 'application/vnd.github.raw+json'): HeadersInit {
    return {
      Authorization: `Bearer ${this.cfg.token}`,
      Accept: accept,
      'X-GitHub-Api-Version': '2022-11-28',
    };
  }

  private async get<T>(path: string, ref = 'main'): Promise<T | null> {
    const res = await fetch(this.url(path, ref), { headers: this.headers(), cache: 'no-cache' });
    if (res.status === 404) return null;
    if (res.status === 401) throw new GithubError('De koppelcode is ongeldig of verlopen.', 401);
    if (res.status === 403) throw new GithubError('Geen toegang (of te veel verzoeken). Probeer het straks opnieuw.', 403);
    if (!res.ok) throw new GithubError(`GitHub gaf fout ${res.status}`, res.status);
    return (await res.json()) as T;
  }

  /** Controleer of de koppeling werkt. */
  async test(): Promise<void> {
    const res = await fetch(`${API}/repos/${this.cfg.owner}/${this.cfg.repo}`, {
      headers: this.headers('application/vnd.github+json'),
    });
    if (!res.ok) throw new GithubError(res.status === 404 || res.status === 401 ? 'Repo niet gevonden of koppelcode onjuist.' : `Fout ${res.status}`, res.status);
  }

  async live(): Promise<LiveData> {
    const d = await this.get<LiveData>('live.json', 'live');
    if (!d) throw new GithubError('Nog geen live data. Staat de ESP32 aan?', 404);
    return d;
  }

  async today(): Promise<DayData> {
    const d = await this.get<DayData>('today.json', 'live');
    if (!d) throw new GithubError('Nog geen data van vandaag.', 404);
    return d;
  }

  async day(date: string): Promise<DayData | null> {
    if (date === todayIso()) {
      const t = await this.today();
      if (t.date === date) return t;
    }
    const path = `days/${date.slice(0, 4)}/${date}.json`;
    // Afgelopen dagen veranderen niet meer: één keer ophalen per sessie.
    if (!this.immutable.has(path)) {
      const p = this.get<DayData>(path).catch((e) => {
        this.immutable.delete(path);
        throw e;
      });
      this.immutable.set(path, p);
    }
    return (await this.immutable.get(path)) as DayData | null;
  }

  async year(y: number): Promise<YearSummary> {
    const isCurrent = y === new Date().getFullYear();
    const hit = this.yearCache.get(y);
    if (hit && (!isCurrent || Date.now() - hit.at < 10 * 60_000)) return this.withToday(hit.data);
    const data = (await this.get<YearSummary>(`summary/${y}.json`)) ?? { year: y, days: [] };
    data.days.sort((a, b) => a.date.localeCompare(b.date));
    this.yearCache.set(y, { at: Date.now(), data });
    return this.withToday(data);
  }

  /** De dagtotalen van vandaag staan in today.json, niet in de jaarsamenvatting. */
  private async withToday(s: YearSummary): Promise<YearSummary> {
    const t = todayIso();
    if (!t.startsWith(String(s.year))) return s;
    try {
      const today = await this.today();
      const days = s.days.filter((d) => d.date !== today.date);
      days.push(normalizeDay(today).totals);
      days.sort((a, b) => a.date.localeCompare(b.date));
      return { year: s.year, days };
    } catch {
      return s;
    }
  }

  async settings(): Promise<Settings> {
    return (await this.get<Settings>('settings.json')) ?? DEFAULT_SETTINGS;
  }

  async saveSettings(s: Settings): Promise<void> {
    const meta = await fetch(this.url('settings.json', 'main'), { headers: this.headers('application/vnd.github+json'), cache: 'no-cache' });
    const sha = meta.ok ? ((await meta.json()) as { sha: string }).sha : undefined;
    const res = await fetch(`${API}/repos/${this.cfg.owner}/${this.cfg.repo}/contents/settings.json`, {
      method: 'PUT',
      headers: { ...this.headers('application/vnd.github+json'), 'Content-Type': 'application/json' },
      body: JSON.stringify({
        message: 'Tarieven bijgewerkt via dashboard',
        content: b64utf8(JSON.stringify(s, null, 2)),
        sha,
        branch: 'main',
      }),
    });
    if (!res.ok) throw new GithubError(`Opslaan mislukt (${res.status}). Heeft de koppelcode schrijfrechten?`, res.status);
  }
}
