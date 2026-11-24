import type { DayData, DayTotals } from '../types';

/**
 * Dagbestanden van de ESP bevatten trage kanalen (temperaturen e.d.) per 5 minuten in `ch`
 * en vermogens (net, zon, batterij, fasen) per minuut in `fast`. Oudere dagen hebben alles per 5 minuten.
 * De site werkt intern met één raster: alles per minuut. Daarna worden korte gaten (≤ 10 min,
 * bv. door een herstart van de ESP) netjes doorgetrokken.
 */
export interface RawDay extends DayData {
  fast?: { step: number; n: number; ch: Record<string, (number | null)[]> };
}

export const BASE_STEP = 60;
export const MAX_GAP_S = 600;

/** Kanalen die een doorlopende grootheid zijn (mag tussen twee meetpunten worden doorgetrokken). */
export function isContinuous(name: string): boolean {
  return /(_w|_max|_soc|_t|^t_out|^cloud|^grid_l[123])$/.test(name) || name === 't_out' || name === 'cloud';
}

/** Vergroot de resolutie: k punten per bronpunt; doorlopende kanalen lineair, standen (aan/uit, modus) vasthouden. */
function expand(a: (number | null)[], k: number, n: number, linear: boolean): (number | null)[] {
  const out: (number | null)[] = new Array(n).fill(null);
  for (let i = 0; i < a.length; i++) {
    const v = a[i];
    if (v == null) continue;
    const next = linear ? a[i + 1] : null;
    for (let j = 0; j < k; j++) {
      const idx = i * k + j;
      if (idx >= n) break;
      out[idx] = next != null ? v + ((next - v) * j) / k : v;
    }
  }
  return out;
}

/** Korte gaten tussen twee meetpunten lineair opvullen. Gaten aan het begin/eind (nog geen data) blijven leeg. */
export function fillGaps(a: (number | null)[], maxPoints: number): (number | null)[] {
  const out = a.slice();
  let last = -1;
  for (let i = 0; i < out.length; i++) {
    if (out[i] == null) continue;
    const gap = i - last - 1;
    if (last >= 0 && gap > 0 && gap <= maxPoints) {
      const a0 = out[last] as number;
      const a1 = out[i] as number;
      for (let j = 1; j <= gap; j++) out[last + j] = Math.round((a0 + ((a1 - a0) * j) / (gap + 1)) * 10) / 10;
    }
    last = i;
  }
  return out;
}

function integKwh(a: (number | null)[] | undefined, sign: 1 | -1, step: number): number {
  let wh = 0;
  for (const v of a ?? []) if (v != null && v * sign > 0) wh += (Math.abs(v) * step) / 3600;
  return wh / 1000;
}

/**
 * Dagtotalen komen uit de tellerstanden (exact). Is een tellertotaal duidelijk LAGER dan wat er volgens
 * de metingen minimaal doorheen is gegaan, dan klopt de tellerstand niet (bv. een foute beginstand) en
 * nemen we het gemeten totaal. Gemeten kan alleen te laag zijn (gaten), nooit te hoog, dus dit is veilig.
 */
export function repairTotals(t: DayTotals | null | undefined, ch: Record<string, (number | null)[]>, step: number): DayTotals | null {
  if (!t) return t ?? null;
  const fix = (v: number | null | undefined, measured: number) =>
    measured > 0.3 && (v == null || v < measured * 0.9 - 0.3) ? Math.round(measured * 1000) / 1000 : (v ?? 0);
  const both = (sign: 1 | -1) => integKwh(ch.bat1_w, sign, step) + integKwh(ch.bat2_w, sign, step);
  return {
    ...t,
    importKwh: fix(t.importKwh, integKwh(ch.grid_w, 1, step)),
    exportKwh: fix(t.exportKwh, integKwh(ch.grid_w, -1, step)),
    solarKwh: fix(t.solarKwh, integKwh(ch.solar_w, 1, step)),
    batChargeKwh: fix(t.batChargeKwh, both(1)),
    batDischargeKwh: fix(t.batDischargeKwh, both(-1)),
  };
}

export function normalizeDay<T extends DayData | null>(d: T): T {
  if (!d) return d;
  const raw = d as RawDay;
  const fast = raw.fast;
  if (raw.step === BASE_STEP && !fast && (raw as { _norm?: boolean })._norm) return d;
  const n = fast?.n ?? Math.round((raw.n * raw.step) / BASE_STEP);
  const ch: Record<string, (number | null)[]> = {};
  if (raw.step % BASE_STEP === 0) {
    const k = raw.step / BASE_STEP;
    for (const [name, a] of Object.entries(raw.ch)) ch[name] = k === 1 ? a.slice(0, n) : expand(a, k, n, isContinuous(name));
  } else {
    Object.assign(ch, raw.ch);
  }
  if (fast) {
    const k = Math.max(1, Math.round(fast.step / BASE_STEP));
    for (const [name, a] of Object.entries(fast.ch)) ch[name] = k === 1 ? a.slice(0, n) : expand(a, k, n, true);
  }
  const maxPts = Math.floor(MAX_GAP_S / BASE_STEP);
  for (const name of Object.keys(ch)) if (isContinuous(name)) ch[name] = fillGaps(ch[name], maxPts);
  const totals = repairTotals(raw.totals, ch, BASE_STEP);
  const out = { date: raw.date, start: raw.start, step: BASE_STEP, n, ch, totals, _norm: true };
  return out as unknown as T;
}
