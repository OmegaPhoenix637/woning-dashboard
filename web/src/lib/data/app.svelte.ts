// Centrale app-state: één plek waar alle pagina's hun data vandaan halen.
import type { DayData, DayTotals, LiveData, Settings, YearSummary } from '../types';
import { DEVICES } from '../types';
import { DEFAULT_SETTINGS, computeMoney, sumMoney, type Money } from '../calc/tariff';
import { todayIso } from '../date';
import type { DataSource } from './source';
import { DemoSource } from './mock';
import { GithubSource, type GithubConfig } from './github';

const LS_GITHUB = 'mw.github';

function loadConfig(): GithubConfig | null {
  try {
    const raw = localStorage.getItem(LS_GITHUB);
    return raw ? (JSON.parse(raw) as GithubConfig) : null;
  } catch {
    return null;
  }
}

/** Koppellink: #/koppel/<base64 JSON>. Zo hoeft papa niets over te typen. */
function consumeLinkFromUrl(): GithubConfig | null {
  const m = location.hash.match(/^#\/koppel\/([A-Za-z0-9+/=_-]+)/);
  if (!m) return null;
  try {
    const json = atob(m[1].replace(/-/g, '+').replace(/_/g, '/'));
    const cfg = JSON.parse(json) as GithubConfig;
    if (cfg.owner && cfg.repo && cfg.token) {
      localStorage.setItem(LS_GITHUB, JSON.stringify(cfg));
      history.replaceState(null, '', location.pathname + '#/');
      return cfg;
    }
  } catch {
    /* ongeldige link */
  }
  return null;
}

export function makeKoppelLink(cfg: GithubConfig): string {
  const b = btoa(JSON.stringify(cfg)).replace(/\+/g, '-').replace(/\//g, '_');
  return `${location.origin}${location.pathname}#/koppel/${b}`;
}

class AppState {
  source: DataSource = $state(new DemoSource());
  config: GithubConfig | null = $state(null);
  live = $state<LiveData | null>(null);
  today = $state<DayData | null>(null);
  settings = $state<Settings>(DEFAULT_SETTINGS);
  years = $state<Record<number, YearSummary>>({});
  error = $state<string | null>(null);
  ready = $state(false);
  now = $state(Date.now());

  private timers: ReturnType<typeof setInterval>[] = [];
  private loadingYears = new Map<number, Promise<YearSummary>>();

  get isDemo() {
    return this.source.kind === 'demo';
  }

  /** Alle bekende dagtotalen, gesorteerd (voor de euroberekening). */
  allDays = $derived.by(() => {
    const out: DayTotals[] = [];
    for (const y of Object.keys(this.years).map(Number).sort()) out.push(...this.years[y].days);
    return out;
  });

  money = $derived(computeMoney(this.allDays, this.settings.tariffs));

  moneyFor(dates: string[]) {
    return sumMoney(dates.map((d) => this.money.get(d)));
  }

  moneyOn(date: string): Money | undefined {
    return this.money.get(date);
  }

  /** Naam van een kamer/apparaat: eigen naam uit Instellingen, anders de standaardnaam. */
  nameOf(id: string): string {
    const own = this.settings.names?.[id];
    if (own) return own;
    const all = [...DEVICES.climate, ...DEVICES.heaters, ...DEVICES.appliances, ...DEVICES.batteries];
    return all.find((d) => d.id === id)?.name ?? id;
  }

  totalsOn(date: string): DayTotals | undefined {
    const y = this.years[Number(date.slice(0, 4))];
    return y?.days.find((d) => d.date === date);
  }

  async init() {
    const linked = consumeLinkFromUrl();
    const cfg = linked ?? loadConfig();
    if (cfg) {
      this.config = cfg;
      this.source = new GithubSource(cfg);
    }
    await this.reload();
    this.startPolling();
    document.addEventListener('visibilitychange', () => {
      if (!document.hidden) this.refreshLive(true);
    });
  }

  async reload() {
    this.error = null;
    this.years = {};
    this.loadingYears.clear();
    try {
      const [settings, live, today] = await Promise.all([
        this.source.settings(),
        this.source.live(),
        this.source.today(),
      ]);
      this.settings = settings;
      this.live = live;
      this.today = today;
      const y = new Date().getFullYear();
      await this.ensureYear(y);
      // Vorig jaar alvast op de achtergrond voor vergelijkingen en week/maand rond de jaarwissel.
      this.ensureYear(y - 1).catch(() => {});
    } catch (e) {
      this.error = (e as Error).message;
    } finally {
      this.ready = true;
    }
  }

  ensureYear(y: number): Promise<YearSummary> {
    if (this.years[y]) return Promise.resolve(this.years[y]);
    let p = this.loadingYears.get(y);
    if (!p) {
      p = this.source.year(y).then((s) => {
        this.years = { ...this.years, [y]: s };
        return s;
      });
      p.catch(() => this.loadingYears.delete(y));
      this.loadingYears.set(y, p);
    }
    return p;
  }

  async refreshLive(withToday = false) {
    try {
      this.live = await this.source.live();
      if (withToday) {
        this.today = await this.source.today();
        const y = new Date().getFullYear();
        const s = this.years[y];
        if (s && this.today) {
          const days = s.days.filter((d) => d.date !== this.today!.date);
          days.push(this.today.totals);
          days.sort((a, b) => a.date.localeCompare(b.date));
          this.years = { ...this.years, [y]: { year: y, days } };
        }
      }
      this.error = null;
    } catch (e) {
      this.error = (e as Error).message;
    }
  }

  private startPolling() {
    this.timers.forEach(clearInterval);
    const demo = this.isDemo;
    let tick = 0;
    this.timers = [
      setInterval(() => (this.now = Date.now()), 15_000),
      setInterval(
        () => {
          if (document.hidden) return;
          tick++;
          // Demo: elke 5 s een nieuwe waarde zodat je de animaties ziet. Echt: elke minuut.
          this.refreshLive(demo ? tick % 12 === 0 : tick % 5 === 0);
        },
        demo ? 5_000 : 60_000,
      ),
    ];
  }

  async connect(cfg: GithubConfig) {
    const src = new GithubSource(cfg);
    await src.test();
    localStorage.setItem(LS_GITHUB, JSON.stringify(cfg));
    this.config = cfg;
    this.source = src;
    this.ready = false;
    await this.reload();
    this.startPolling();
  }

  async disconnect() {
    localStorage.removeItem(LS_GITHUB);
    this.config = null;
    this.source = new DemoSource();
    this.ready = false;
    await this.reload();
    this.startPolling();
  }

  async saveSettings(s: Settings) {
    await this.source.saveSettings(s);
    this.settings = s;
  }

  day(date: string): Promise<DayData | null> {
    if (date === todayIso() && this.today) return Promise.resolve(this.today);
    return this.source.day(date);
  }

  get firstDate(): string {
    const s = this.source;
    return s instanceof DemoSource ? s.firstDate : '2026-01-01';
  }
}

export const app = new AppState();
