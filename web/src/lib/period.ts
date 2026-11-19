import type { Period } from './components/PeriodPicker.svelte';
import { periodAnchor } from './components/PeriodPicker.svelte';
import { addDays, addMonths, daysInMonth, rangeDays, todayIso } from './date';

export interface PeriodRange {
  start: string;
  end: string; // inclusief
  /** Dagen in de periode t/m vandaag. */
  dates: string[];
  years: number[];
}

export function periodRange(p: Period, date: string): PeriodRange {
  const start = periodAnchor(p, date);
  const end =
    p === 'dag'
      ? start
      : p === 'week'
        ? addDays(start, 6)
        : p === 'maand'
          ? addDays(start, daysInMonth(start) - 1)
          : start.slice(0, 4) + '-12-31';
  const today = todayIso();
  const dates = start > today ? [] : rangeDays(start, end < today ? end : today);
  const years = [...new Set([start, end].map((d) => Number(d.slice(0, 4))))];
  return { start, end, dates, years };
}

/** Maanden van een jaar als [start, eind]-paren. */
export function monthsOfYear(year: number): [string, string][] {
  const out: [string, string][] = [];
  let m = `${year}-01-01`;
  for (let i = 0; i < 12; i++) {
    out.push([m, addDays(m, daysInMonth(m) - 1)]);
    m = addMonths(m, 1);
  }
  return out;
}
