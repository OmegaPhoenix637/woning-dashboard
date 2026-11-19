import type { DayData, DayTotals } from '../types';

/** Huisverbruik = netafname + zon - batterij (batterij + = laden) - laadpaal. */
export function houseW(grid: number, solar: number, batteries: number[], ev = 0): number {
  return Math.max(0, grid + solar - batteries.reduce((a, b) => a + b, 0) - ev);
}

export function houseSeries(day: DayData): (number | null)[] {
  const g = day.ch.grid_w ?? [];
  const s = day.ch.solar_w ?? [];
  const b1 = day.ch.bat1_w ?? [];
  const b2 = day.ch.bat2_w ?? [];
  const ev = day.ch.ev_w ?? [];
  return g.map((gv, i) => {
    if (gv == null) return null;
    return houseW(gv, s[i] ?? 0, [b1[i] ?? 0, b2[i] ?? 0], ev[i] ?? 0);
  });
}

export function batterySeries(day: DayData): (number | null)[] {
  const b1 = day.ch.bat1_w ?? [];
  const b2 = day.ch.bat2_w ?? [];
  return b1.map((v, i) => (v == null && b2[i] == null ? null : (v ?? 0) + (b2[i] ?? 0)));
}

export function consumptionKwh(t: Pick<DayTotals, 'importKwh' | 'exportKwh' | 'solarKwh' | 'batChargeKwh' | 'batDischargeKwh'>): number {
  return Math.max(0, t.importKwh - t.exportKwh + t.solarKwh + t.batDischargeKwh - t.batChargeKwh);
}

/** Deel van het verbruik dat NIET van het net kwam. */
export function selfSufficiency(t: DayTotals | Agg): number {
  const c = consumptionKwh(t);
  return c > 0 ? Math.max(0, Math.min(1, 1 - t.importKwh / c)) : 0;
}

/** Deel van de zonnestroom dat zelf gebruikt is (direct of via batterij). */
export function selfConsumption(t: DayTotals | Agg): number {
  return t.solarKwh > 0 ? Math.max(0, Math.min(1, 1 - t.exportKwh / t.solarKwh)) : 0;
}

/** Waar ging de zonnestroom heen? */
export function solarSplit(t: DayTotals | Agg) {
  const toGrid = Math.min(t.exportKwh, t.solarKwh);
  const toBattery = Math.min(t.batChargeKwh, Math.max(0, t.solarKwh - toGrid));
  const direct = Math.max(0, t.solarKwh - toGrid - toBattery);
  return { direct, toBattery, toGrid };
}

/** Waar kwam het verbruik vandaan? */
export function sourceSplit(t: DayTotals | Agg) {
  const c = consumptionKwh(t);
  const fromGrid = Math.min(t.importKwh, c);
  const fromBattery = Math.min(t.batDischargeKwh, Math.max(0, c - fromGrid));
  const fromSolar = Math.max(0, c - fromGrid - fromBattery);
  return { fromSolar, fromBattery, fromGrid, total: c };
}

export interface Agg {
  days: number;
  importKwh: number;
  exportKwh: number;
  solarKwh: number;
  batChargeKwh: number;
  batDischargeKwh: number;
  gasM3: number | null;
  evKwh: number;
  climateKwh: Record<string, number>;
  heaterHours: Record<string, number>;
  runs: Record<string, number>;
  tOutAvg: number | null;
  tOutMin: number | null;
  tOutMax: number | null;
  peakImportW: number;
}

export function aggregate(days: DayTotals[]): Agg {
  const a: Agg = {
    days: days.length,
    importKwh: 0,
    exportKwh: 0,
    solarKwh: 0,
    batChargeKwh: 0,
    batDischargeKwh: 0,
    gasM3: null,
    evKwh: 0,
    climateKwh: {},
    heaterHours: {},
    runs: {},
    tOutAvg: null,
    tOutMin: null,
    tOutMax: null,
    peakImportW: 0,
  };
  let tSum = 0;
  let tN = 0;
  for (const d of days) {
    a.importKwh += d.importKwh;
    a.exportKwh += d.exportKwh;
    a.solarKwh += d.solarKwh;
    a.batChargeKwh += d.batChargeKwh;
    a.batDischargeKwh += d.batDischargeKwh;
    if (d.gasM3 != null) a.gasM3 = (a.gasM3 ?? 0) + d.gasM3;
    a.evKwh += d.evKwh ?? 0;
    for (const [k, v] of Object.entries(d.climateKwh)) a.climateKwh[k] = (a.climateKwh[k] ?? 0) + v;
    for (const [k, v] of Object.entries(d.heaterHours)) a.heaterHours[k] = (a.heaterHours[k] ?? 0) + v;
    for (const r of d.runs) a.runs[r.id] = (a.runs[r.id] ?? 0) + 1;
    if (d.tOutAvg != null) {
      tSum += d.tOutAvg;
      tN++;
    }
    if (d.tOutMin != null) a.tOutMin = a.tOutMin == null ? d.tOutMin : Math.min(a.tOutMin, d.tOutMin);
    if (d.tOutMax != null) a.tOutMax = a.tOutMax == null ? d.tOutMax : Math.max(a.tOutMax, d.tOutMax);
    a.peakImportW = Math.max(a.peakImportW, d.peakImportW);
  }
  a.tOutAvg = tN ? tSum / tN : null;
  return a;
}

/** Integreer een vermogensreeks (W) naar kWh, optioneel alleen positief/negatief deel. */
export function integrateKwh(series: (number | null)[], stepS: number, part: 'pos' | 'neg' | 'all' = 'all'): number {
  let wh = 0;
  for (const v of series) {
    if (v == null) continue;
    if (part === 'pos' && v < 0) continue;
    if (part === 'neg' && v > 0) continue;
    wh += (Math.abs(v) * stepS) / 3600;
  }
  return wh / 1000;
}

/** Gemiddelde per uur uit een 5-minutenreeks (voor rustigere vergelijkingsgrafieken). */
export function hourly(series: (number | null)[], perHour = 12): (number | null)[] {
  const out: (number | null)[] = [];
  for (let i = 0; i < series.length; i += perHour) {
    let s = 0;
    let n = 0;
    for (let j = i; j < Math.min(i + perHour, series.length); j++) {
      const v = series[j];
      if (v != null) {
        s += v;
        n++;
      }
    }
    out.push(n ? s / n : null);
  }
  return out;
}
