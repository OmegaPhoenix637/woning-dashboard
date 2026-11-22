<script lang="ts">
  import { GitCompareArrows, CalendarDays, ArrowRight, Sparkles } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import Chart from '../lib/components/Chart.svelte';
  import BarChart from '../lib/components/BarChart.svelte';
  import { app } from '../lib/data/app.svelte';
  import { periodRange, monthsOfYear } from '../lib/period';
  import { aggregate, consumptionKwh, houseSeries, selfSufficiency, type Agg } from '../lib/calc/energy';
  import {
    addDays,
    addMonths,
    fmtDateLong,
    fmtDateShort,
    fmtMonthName,
    fmtMonthShortName,
    isoWeek,
    rangeDays,
    parseDate,
    todayIso,
  } from '../lib/date';
  import { axis, euro, kwh, n0, n1, power, temp } from '../lib/format';
  import type { DayData } from '../lib/types';
  import type { Period } from '../lib/components/PeriodPicker.svelte';

  type Mode = 'dag' | 'week' | 'maand' | 'jaar';
  type Metric = 'house' | 'solar' | 'grid' | 'tout' | 'soc' | 'l1' | 'l2' | 'l3';

  const today = todayIso();
  let mode = $state<Mode>('dag');
  let dateA = $state(today);
  let dateB = $state(addDays(today, -1));
  let metric = $state<Metric>('house');

  const rA = $derived(periodRange(mode as Period, dateA));
  const rB = $derived(periodRange(mode as Period, dateB));

  $effect(() => {
    for (const y of new Set([...rA.years, ...rB.years])) app.ensureYear(y).catch(() => {});
  });

  let dayA = $state<DayData | null>(null);
  let dayB = $state<DayData | null>(null);
  $effect(() => {
    if (mode !== 'dag') return;
    const a = rA.start;
    const b = rB.start;
    void app.today;
    app.day(a).then((d) => rA.start === a && (dayA = d));
    app.day(b).then((d) => rB.start === b && (dayB = d));
  });

  function quick(kind: 'prev' | 'week' | 'year') {
    if (kind === 'prev')
      dateB =
        mode === 'dag'
          ? addDays(dateA, -1)
          : mode === 'week'
            ? addDays(dateA, -7)
            : mode === 'maand'
              ? addMonths(dateA, -1)
              : `${Number(dateA.slice(0, 4)) - 1}-${dateA.slice(5)}`;
    if (kind === 'week') dateB = addDays(dateA, -7);
    if (kind === 'year') {
      const d = parseDate(dateA);
      d.setFullYear(d.getFullYear() - 1);
      // Zelfde weekdag vorig jaar (364 dagen terug) voor eerlijke dag/week-vergelijking
      dateB = mode === 'maand' || mode === 'jaar' ? `${d.getFullYear()}-${dateA.slice(5)}` : addDays(dateA, -364);
    }
  }

  function label(r: { start: string }, m: Mode) {
    if (m === 'dag') return fmtDateLong(r.start, true);
    if (m === 'week') return `Week ${isoWeek(r.start)} · ${fmtDateShort(r.start)} ${r.start.slice(0, 4)}`;
    if (m === 'maand') return fmtMonthName(r.start);
    return `Jaar ${r.start.slice(0, 4)}`;
  }

  const aggA = $derived(aggregate(rA.dates.map((d) => app.totalsOn(d)).filter((t) => t != null)));
  const aggB = $derived(aggregate(rB.dates.map((d) => app.totalsOn(d)).filter((t) => t != null)));
  const mA = $derived(app.moneyFor(rA.dates));
  const mB = $derived(app.moneyFor(rB.dates));

  const sumObj = (o: Record<string, number>) => Object.values(o).reduce((a, b) => a + b, 0);
  const runs = (a: Agg) => sumObj(a.runs);

  interface Row {
    label: string;
    a: number | null;
    b: number | null;
    f: (v: number) => string;
    goodUp: boolean | null;
  }
  const rows = $derived<Row[]>([
    { label: 'Verbruikt', a: consumptionKwh(aggA), b: consumptionKwh(aggB), f: (v) => kwh(v), goodUp: false },
    { label: 'Opgewekt', a: aggA.solarKwh, b: aggB.solarKwh, f: (v) => kwh(v), goodUp: true },
    { label: 'Van het net', a: aggA.importKwh, b: aggB.importKwh, f: (v) => kwh(v), goodUp: false },
    { label: 'Teruggeleverd', a: aggA.exportKwh, b: aggB.exportKwh, f: (v) => kwh(v), goodUp: null },
    { label: 'Zelfvoorzienend', a: selfSufficiency(aggA) * 100, b: selfSufficiency(aggB) * 100, f: (v) => `${n0(v)}%`, goodUp: true },
    { label: 'Stroomkosten', a: mA.cost, b: mB.cost, f: euro, goodUp: false },
    { label: 'Bespaard', a: mA.saved, b: mB.saved, f: euro, goodUp: true },
    { label: 'Gem. buitentemperatuur', a: aggA.tOutAvg, b: aggB.tOutAvg, f: (v) => temp(v), goodUp: null },
    { label: "Airco's", a: sumObj(aggA.climateKwh), b: sumObj(aggB.climateKwh), f: (v) => kwh(v), goodUp: false },
    { label: 'Pelletkachel aan', a: aggA.heaterHours.pellet ?? 0, b: aggB.heaterHours.pellet ?? 0, f: (v) => `${n1(v)} uur`, goodUp: null },
    { label: 'Was- en afwasbeurten', a: runs(aggA), b: runs(aggB), f: (v) => n0(v), goodUp: null },
  ]);

  const summary = $derived.by(() => {
    if (!aggA.days || !aggB.days) return null;
    const ca = consumptionKwh(aggA);
    const cb = consumptionKwh(aggB);
    const parts: string[] = [];
    if (cb > 0.5) {
      const p = ((ca - cb) / cb) * 100;
      parts.push(Math.abs(p) < 3 ? 'ongeveer evenveel verbruikt' : `${n0(Math.abs(p))}% ${p > 0 ? 'meer' : 'minder'} verbruikt`);
    }
    if (aggA.solarKwh + aggB.solarKwh > 0.5) {
      const d = aggA.solarKwh - aggB.solarKwh;
      parts.push(Math.abs(d) < 0.3 ? 'evenveel zon' : `${kwh(Math.abs(d))} ${d > 0 ? 'meer' : 'minder'} zon`);
    }
    if (aggA.tOutAvg != null && aggB.tOutAvg != null) {
      const d = aggA.tOutAvg - aggB.tOutAvg;
      if (Math.abs(d) >= 1) parts.push(`${n1(Math.abs(d))}° ${d > 0 ? 'warmer' : 'kouder'} buiten`);
    }
    if (rA.dates.includes(today) || rB.dates.includes(today)) parts.push('(de huidige periode is nog niet voorbij)');
    return parts.join(', ');
  });

  // Overlay-grafiek (dag)
  const overlay = $derived.by(() => {
    if (mode !== 'dag' || !dayA || !dayB) return null;
    const pick = (d: DayData) =>
      metric === 'house'
        ? houseSeries(d)
        : metric === 'solar'
          ? d.ch.solar_w
          : metric === 'grid'
            ? d.ch.grid_w
            : metric === 'tout'
              ? d.ch.t_out
              : metric === 'l1' || metric === 'l2' || metric === 'l3'
                ? (d.ch['grid_' + metric] ?? [])
                : d.ch.bat1_soc.map((v, i) => (v == null ? null : (v + (d.ch.bat2_soc[i] ?? v)) / 2));
    const n = Math.max(dayA.n, dayB.n);
    const st = dayA.step;
    const x = Array.from({ length: n }, (_, i) => (i * st) / 3600);
    const pad = (a: (number | null)[]) => Array.from({ length: n }, (_, i) => a[i] ?? null);
    return {
      x,
      series: [
        { label: 'A', values: pad(pick(dayA)), color: '--c-a', width: 2.5 },
        { label: 'B', values: pad(pick(dayB)), color: '--c-b' },
      ],
    };
  });
  const metricFmt = $derived(
    metric === 'tout' ? (v: number) => temp(v) : metric === 'soc' ? (v: number) => `${n0(v)}%` : (v: number) => power(v, false),
  );
  const axisFmt = $derived(metric === 'tout' ? (v: number) => `${v}°` : metric === 'soc' ? (v: number) => `${v}%` : axis);
  const METRICS: { id: Metric; label: string }[] = [
    { id: 'house', label: 'Verbruik' },
    { id: 'solar', label: 'Zon' },
    { id: 'grid', label: 'Net' },
    { id: 'soc', label: 'Batterij %' },
    { id: 'tout', label: 'Buiten' },
    { id: 'l1', label: 'Fase 1' },
    { id: 'l2', label: 'Fase 2' },
    { id: 'l3', label: 'Fase 3' },
  ];

  // Staafvergelijking (week/maand)
  const bars = $derived.by(() => {
    if (mode === 'dag') return null;
    if (mode === 'jaar') {
      const monthVal = (y: string, mi: number) => {
        const [s, e] = monthsOfYear(Number(y))[mi];
        const ts = rangeDays(s, e < today ? e : today)
          .map((d) => app.totalsOn(d))
          .filter((t) => t != null);
        if (!ts.length || s > today) return null;
        const a = aggregate(ts);
        return metric === 'solar' ? a.solarKwh : metric === 'grid' ? a.importKwh : metric === 'tout' ? a.tOutAvg : consumptionKwh(a);
      };
      const labels = monthsOfYear(2000).map(([s]) => fmtMonthShortName(s));
      return {
        labels,
        series: [
          { label: 'A', values: labels.map((_, i) => monthVal(rA.start.slice(0, 4), i)), color: '--c-a' },
          { label: 'B', values: labels.map((_, i) => monthVal(rB.start.slice(0, 4), i)), color: '--c-b' },
        ],
      };
    }
    const len = mode === 'week' ? 7 : 31;
    const val = (start: string, i: number) => {
      const d = addDays(start, i);
      if (mode === 'maand' && d.slice(0, 7) !== start.slice(0, 7)) return null;
      const t = app.totalsOn(d);
      if (!t) return null;
      return metric === 'solar' ? t.solarKwh : metric === 'grid' ? t.importKwh : metric === 'tout' ? t.tOutAvg : consumptionKwh(t);
    };
    const labels = Array.from({ length: len }, (_, i) => (mode === 'week' ? ['ma', 'di', 'wo', 'do', 'vr', 'za', 'zo'][i] : String(i + 1)));
    return {
      labels,
      series: [
        { label: 'A', values: labels.map((_, i) => val(rA.start, i)), color: '--c-a' },
        { label: 'B', values: labels.map((_, i) => val(rB.start, i)), color: '--c-b' },
      ],
    };
  });
</script>

<div class="page">
  <header class="page-head rise">
    <div>
      <p class="eyebrow">Vergelijken</p>
      <h1>Hoe deed deze periode het?</h1>
      <p>Kies twee dagen, weken, maanden of jaren en zie direct het verschil.</p>
    </div>
  </header>

  <section class="card rise pick" style="--i:1">
    <div class="seg" role="group" aria-label="Wat vergelijken">
      {#each ['dag', 'week', 'maand', 'jaar'] as m (m)}
        <button type="button" aria-pressed={mode === m} onclick={() => {
            mode = m as Mode;
            quick('prev');
          }}>{m[0].toUpperCase() + m.slice(1)}</button>
      {/each}
    </div>

    <div class="ab">
      <label class="side a">
        <span class="badge">A</span>
        <span class="lbl">{label(rA, mode)}</span>
        <Icon icon={CalendarDays} size={17} />
        <input type="date" bind:value={dateA} max={today} min={app.firstDate} aria-label="Datum A" />
      </label>
      <span class="vs"><Icon icon={GitCompareArrows} size={18} /></span>
      <label class="side b">
        <span class="badge">B</span>
        <span class="lbl">{label(rB, mode)}</span>
        <Icon icon={CalendarDays} size={17} />
        <input type="date" bind:value={dateB} max={today} min={app.firstDate} aria-label="Datum B" />
      </label>
    </div>

    <div class="quick">
      <span class="muted">B snel kiezen:</span>
      <button class="btn" type="button" onclick={() => quick('prev')}>{mode === 'dag' ? 'Dag ervoor' : mode === 'week' ? 'Week ervoor' : mode === 'maand' ? 'Maand ervoor' : 'Jaar ervoor'}</button>
      {#if mode === 'dag'}<button class="btn" type="button" onclick={() => quick('week')}>Zelfde dag vorige week</button>{/if}
      {#if mode !== 'jaar'}<button class="btn" type="button" onclick={() => quick('year')}>Vorig jaar</button>{/if}
    </div>
  </section>

  {#if summary}
    <div class="summary rise" style="--i:2">
      <Icon icon={Sparkles} size={20} />
      <p><b>A</b> ten opzichte van <b>B</b>: {summary}.</p>
    </div>
  {/if}

  <section class="card rise" style="--i:3">
    <div class="card-head wrap">
      <h2 class="card-title">{mode === 'dag' ? 'Door de dag heen' : mode === 'jaar' ? 'Per maand' : 'Per dag'}</h2>
      <div class="seg small" role="group" aria-label="Grootheid">
        {#each METRICS.filter((m) => mode === 'dag' || !['soc', 'l1', 'l2', 'l3'].includes(m.id)) as m (m.id)}
          <button type="button" aria-pressed={metric === m.id} onclick={() => (metric = m.id)}>{m.label}</button>
        {/each}
      </div>
    </div>
    {#if mode === 'dag' && overlay}
      <Chart
        x={overlay.x}
        series={overlay.series}
        xMode="num"
        xRange={[0, 24]}
        xFormat={(h) => `${String(Math.floor(h)).padStart(2, '0')}:${String(Math.round((h % 1) * 60)).padStart(2, '0')}`}
        height={300}
        yFormat={axisFmt}
        valueFormat={metricFmt}
        ariaLabel="Vergelijking A en B door de dag"
      />
    {:else if bars}
      <BarChart
        labels={bars.labels}
        series={bars.series}
        height={280}
        yFormat={metric === 'tout' ? (v) => `${v}°` : axis}
        valueFormat={metric === 'tout' ? (v) => temp(v) : (v) => kwh(v)}
        ariaLabel="Vergelijking A en B per dag"
      />
    {/if}
  </section>

  <section class="card rise" style="--i:4">
    <div class="card-head"><h2 class="card-title">Alle verschillen</h2></div>
    <div class="table-wrap">
      <table class="data">
        <thead>
          <tr><th></th><th><span class="th-a">A</span></th><th><span class="th-b">B</span></th><th>Verschil</th></tr>
        </thead>
        <tbody>
          {#each rows as r (r.label)}
            {@const d = r.a != null && r.b != null ? r.a - r.b : null}
            {@const good = d == null || r.goodUp == null || Math.abs(d) < 1e-6 ? null : d > 0 === r.goodUp}
            <tr>
              <td>{r.label}</td>
              <td>{r.a == null ? '—' : r.f(r.a)}</td>
              <td>{r.b == null ? '—' : r.f(r.b)}</td>
              <td class:good={good === true} class:bad={good === false}>
                {#if d == null}—{:else if Math.abs(d) < 1e-6}gelijk{:else}{d > 0 ? '▲' : '▼'} {r.f(Math.abs(d))}{/if}
              </td>
            </tr>
          {/each}
        </tbody>
      </table>
    </div>
    <p class="note muted"><Icon icon={ArrowRight} size={13} /> Groen = beter voor A, oranje = minder goed voor A.</p>
  </section>
</div>

<style>
  .pick {
    display: grid;
    gap: 16px;
  }
  .ab {
    display: grid;
    gap: 10px;
    align-items: center;
  }
  .side {
    position: relative;
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 16px;
    border-radius: 16px;
    background: var(--surface-2);
    border: 1px solid var(--border);
    cursor: pointer;
    min-width: 0;
  }
  .side.a {
    box-shadow: inset 4px 0 0 var(--c-a);
  }
  .side.b {
    box-shadow: inset 4px 0 0 var(--c-b);
  }
  .side input {
    position: absolute;
    inset: 0;
    opacity: 0;
    width: 100%;
    cursor: pointer;
  }
  .side input::-webkit-calendar-picker-indicator {
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
    cursor: pointer;
  }
  .badge {
    display: grid;
    place-items: center;
    width: 30px;
    height: 30px;
    border-radius: 9px;
    font-weight: 750;
    color: #fff;
    flex: none;
  }
  .a .badge {
    background: var(--c-a);
  }
  .b .badge {
    background: var(--c-b);
  }
  .lbl {
    flex: 1;
    font-weight: 600;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .lbl::first-letter {
    text-transform: uppercase;
  }
  .side :global(svg) {
    color: var(--text-3);
    flex: none;
  }
  .vs {
    justify-self: center;
    color: var(--text-3);
  }
  .quick {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 8px;
    font-size: 14px;
  }
  .quick .btn {
    height: 36px;
    font-size: 14px;
    padding: 0 14px;
  }
  .summary {
    display: flex;
    gap: 12px;
    align-items: flex-start;
    padding: 16px 18px;
    border-radius: 18px;
    background: linear-gradient(120deg, color-mix(in srgb, var(--c-a) 14%, var(--surface)), color-mix(in srgb, var(--c-b) 12%, var(--surface)));
    border: 1px solid var(--border-strong);
    font-size: 15.5px;
  }
  .summary :global(svg) {
    color: var(--g-solar);
    flex: none;
    margin-top: 2px;
  }
  .card-head.wrap {
    flex-wrap: wrap;
  }
  .seg.small {
    flex-wrap: wrap;
    border-radius: 18px;
  }
  .seg.small button {
    height: 32px;
    padding: 0 12px;
    font-size: 13px;
  }
  .th-a,
  .th-b {
    display: inline-grid;
    place-items: center;
    width: 22px;
    height: 22px;
    border-radius: 6px;
    color: #fff;
  }
  .th-a {
    background: var(--c-a);
  }
  .th-b {
    background: var(--c-b);
  }
  td.good {
    color: var(--s-good-text);
    font-weight: 600;
  }
  td.bad {
    color: var(--s-serious);
    font-weight: 600;
  }
  .note {
    display: flex;
    gap: 6px;
    align-items: center;
    font-size: 13px;
    margin-top: 12px;
  }
  @media (min-width: 820px) {
    .ab {
      grid-template-columns: 1fr auto 1fr;
    }
  }
</style>
