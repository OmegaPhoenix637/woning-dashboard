// Getalnotatie in het Nederlands (1.234,5).

const nf = (min: number, max: number) =>
  new Intl.NumberFormat('nl-NL', { minimumFractionDigits: min, maximumFractionDigits: max });
const nf0 = nf(0, 0);
const nf1 = nf(1, 1);
const nf2 = nf(2, 2);
const eur = new Intl.NumberFormat('nl-NL', { style: 'currency', currency: 'EUR' });

export const n0 = (v: number) => nf0.format(v);
export const n1 = (v: number) => nf1.format(v);
export const n2 = (v: number) => nf2.format(v);

/** Vermogen: 640 W of 2,4 kW */
export function power(w: number | null | undefined, abs = true): string {
  if (w == null || !Number.isFinite(w)) return '—';
  const v = abs ? Math.abs(w) : w;
  if (Math.abs(v) < 1000) return `${nf0.format(Math.round(v))} W`;
  return `${nf1.format(v / 1000)} kW`;
}
export function powerParts(w: number | null | undefined): [string, string] {
  if (w == null || !Number.isFinite(w)) return ['—', ''];
  const v = Math.abs(w);
  if (v < 1000) return [nf0.format(Math.round(v)), 'W'];
  return [nf1.format(v / 1000), 'kW'];
}

export const kwh = (v: number | null | undefined, digits = 1) =>
  v == null || !Number.isFinite(v) ? '—' : `${(digits === 0 ? nf0 : digits === 2 ? nf2 : nf1).format(v)} kWh`;

export const euro = (v: number | null | undefined) => {
  if (v == null || !Number.isFinite(v)) return '—';
  const r = Math.round(v * 100) / 100;
  return eur.format(r === 0 ? 0 : r); // geen "€ -0,00"
};

export const temp = (v: number | null | undefined, digits = 1) =>
  v == null || !Number.isFinite(v) ? '—' : `${(digits === 0 ? nf0 : nf1).format(v)}°`;

export const pct = (v: number | null | undefined) =>
  v == null || !Number.isFinite(v) ? '—' : `${nf0.format(Math.round(v))}%`;

/** Compacte as-labels: 0 / 500 / 1.000 / 10K */
export function axis(v: number): string {
  const a = Math.abs(v);
  if (a >= 10000) return `${nf0.format(v / 1000)}K`;
  return nf0.format(v);
}

export function signed(v: number, f: (x: number) => string): string {
  if (Math.abs(v) < 1e-9) return f(0);
  return (v > 0 ? '+' : '−') + f(Math.abs(v));
}
