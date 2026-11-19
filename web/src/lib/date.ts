// Datumhulpjes. Alles in lokale tijd (Europe/Amsterdam op de apparaten van de familie).

export function isoDate(d: Date): string {
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, '0');
  const day = String(d.getDate()).padStart(2, '0');
  return `${y}-${m}-${day}`;
}

export function parseDate(s: string): Date {
  const [y, m, d] = s.split('-').map(Number);
  return new Date(y, m - 1, d);
}

export function addDays(s: string, n: number): string {
  const d = parseDate(s);
  d.setDate(d.getDate() + n);
  return isoDate(d);
}

export function todayIso(): string {
  return isoDate(new Date());
}

/** Unix-seconden van lokale middernacht. */
export function dayStart(s: string): number {
  return Math.floor(parseDate(s).getTime() / 1000);
}

/** Aantal 5-minutenstappen in een dag (276 of 300 bij zomer-/wintertijdwissel). */
export function stepsInDay(s: string, step = 300): number {
  return Math.round((dayStart(addDays(s, 1)) - dayStart(s)) / step);
}

/** Maandag van de week waarin de datum valt. */
export function weekStart(s: string): string {
  const d = parseDate(s);
  const dow = (d.getDay() + 6) % 7; // ma=0
  d.setDate(d.getDate() - dow);
  return isoDate(d);
}

export function monthStart(s: string): string {
  return s.slice(0, 8) + '01';
}

export function daysInMonth(s: string): number {
  const d = parseDate(s);
  return new Date(d.getFullYear(), d.getMonth() + 1, 0).getDate();
}

export function addMonths(s: string, n: number): string {
  const d = parseDate(monthStart(s));
  d.setMonth(d.getMonth() + n);
  return isoDate(d);
}

export function dayOfYear(s: string): number {
  const d = parseDate(s);
  const start = new Date(d.getFullYear(), 0, 1);
  return Math.round((d.getTime() - start.getTime()) / 86400000) + 1;
}

export function rangeDays(from: string, toIncl: string): string[] {
  const out: string[] = [];
  for (let d = from; d <= toIncl; d = addDays(d, 1)) out.push(d);
  return out;
}

/** ISO-weeknummer. */
export function isoWeek(s: string): number {
  const d = parseDate(s);
  const t = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()));
  const dayNum = t.getUTCDay() || 7;
  t.setUTCDate(t.getUTCDate() + 4 - dayNum);
  const yearStart = new Date(Date.UTC(t.getUTCFullYear(), 0, 1));
  return Math.ceil(((t.getTime() - yearStart.getTime()) / 86400000 + 1) / 7);
}

const fmtLong = new Intl.DateTimeFormat('nl-NL', { weekday: 'long', day: 'numeric', month: 'long' });
const fmtLongY = new Intl.DateTimeFormat('nl-NL', { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' });
const fmtShort = new Intl.DateTimeFormat('nl-NL', { day: 'numeric', month: 'short' });
const fmtShortWd = new Intl.DateTimeFormat('nl-NL', { weekday: 'short', day: 'numeric', month: 'short' });
const fmtMonth = new Intl.DateTimeFormat('nl-NL', { month: 'long', year: 'numeric' });
const fmtMonthShort = new Intl.DateTimeFormat('nl-NL', { month: 'short' });
const fmtWd = new Intl.DateTimeFormat('nl-NL', { weekday: 'short' });

export const fmtDateLong = (s: string, withYear = false) =>
  (withYear ? fmtLongY : fmtLong).format(parseDate(s));
export const fmtDateShort = (s: string) => fmtShort.format(parseDate(s));
export const fmtDateShortWd = (s: string) => fmtShortWd.format(parseDate(s));
export const fmtMonthName = (s: string) => fmtMonth.format(parseDate(s));
export const fmtMonthShortName = (s: string) => fmtMonthShort.format(parseDate(s)).replace('.', '');
export const fmtWeekday = (s: string) => fmtWd.format(parseDate(s));

export function fmtClock(unixS: number): string {
  const d = new Date(unixS * 1000);
  return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
}

export function fmtMinutes(min: number): string {
  const h = Math.floor(min / 60);
  const m = Math.round(min % 60);
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
}

export function relativeAgo(unixS: number, nowMs = Date.now()): string {
  const s = Math.max(0, Math.round(nowMs / 1000 - unixS));
  if (s < 45) return 'zojuist';
  const m = Math.round(s / 60);
  if (m < 60) return `${m} min geleden`;
  const h = Math.round(m / 60);
  if (h < 24) return `${h} uur geleden`;
  const d = Math.round(h / 24);
  return d === 1 ? 'gisteren' : `${d} dagen geleden`;
}

export function greeting(d = new Date()): string {
  const h = d.getHours();
  if (h < 6) return 'Goedenacht';
  if (h < 12) return 'Goedemorgen';
  if (h < 18) return 'Goedemiddag';
  return 'Goedenavond';
}
