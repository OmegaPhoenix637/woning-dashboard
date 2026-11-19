<script lang="ts">
  import { Sun, Zap, UtilityPole, ArrowUpFromLine, Leaf, PiggyBank, Euro, Flame, Table2, ChartColumn, Gauge } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import StatTile from '../lib/components/StatTile.svelte';
  import Chart from '../lib/components/Chart.svelte';
  import BarChart from '../lib/components/BarChart.svelte';
  import SegBar from '../lib/components/SegBar.svelte';
  import Explain from '../lib/components/Explain.svelte';
  import PeriodPicker, { type Period } from '../lib/components/PeriodPicker.svelte';
  import { app } from '../lib/data/app.svelte';
  import { router } from '../lib/router.svelte';
  import { periodRange, monthsOfYear } from '../lib/period';
  import {
    aggregate,
    batterySeries,
    consumptionKwh,
    houseSeries,
    selfSufficiency,
    solarSplit,
    sourceSplit,
  } from '../lib/calc/energy';
  import { dayStart, fmtDateShortWd, fmtMonthShortName, fmtWeekday, fmtDateShort, fmtMonthName, todayIso, parseDate } from '../lib/date';
  import { axis, euro, kwh, n0, n1, n2, power } from '../lib/format';
  import type { DayData } from '../lib/types';

  let period = $state<Period>((router.params.get('p') as Period) || 'dag');
  let date = $state(router.params.get('d') || todayIso());
  let showTable = $state(false);

  const range = $derived(periodRange(period, date));
  $effect(() => {
    for (const y of range.years) app.ensureYear(y).catch(() => {});
  });

  // Dagdetail
  let day = $state<DayData | null>(null);
  let dayLoading = $state(false);
  $effect(() => {
    if (period !== 'dag') return;
    const d = range.start;
    void app.today; // ververs mee met live data
    dayLoading = true;
    app.day(d).then((res) => {
      if (range.start === d) {
        day = res;
        dayLoading = false;
      }
    });
  });

  const totals = $derived(range.dates.map((d) => app.totalsOn(d)).filter((t) => t != null));
  const agg = $derived(aggregate(totals));
  const money = $derived(app.moneyFor(range.dates));
  const cons = $derived(consumptionKwh(agg));
  const sSplit = $derived(solarSplit(agg));
  const srcSplit = $derived(sourceSplit(agg));

  const dayChart = $derived.by(() => {
    if (!day) return null;
    return {
      x: Array.from({ length: day.n }, (_, i) => day!.start + i * day!.step),
      series: [
        { label: 'Verbruik', values: houseSeries(day), color: '--c-house', fill: true },
        { label: 'Zon', values: day.ch.solar_w, color: '--c-solar', fill: true },
        { label: 'Batterijen', values: batterySeries(day), color: '--c-battery' },
        { label: 'Net', values: day.ch.grid_w, color: '--c-grid' },
      ],
    };
  });

  // Staafgrafieken voor week/maand/jaar
  interface Row {
    key: string;
    label: string;
    title: string;
    cons: number | null;
    solar: number | null;
    imp: number | null;
    exp: number | null;
    cost: number | null;
    saved: number | null;
  }
  const rows = $derived.by((): Row[] => {
    if (period === 'dag') return [];
    if (period === 'jaar') {
      const y = Number(range.start.slice(0, 4));
      return monthsOfYear(y).map(([s, e]) => {
        const ds = range.dates.filter((d) => d >= s && d <= e);
        const ts = ds.map((d) => app.totalsOn(d)).filter((t) => t != null);
        const a = aggregate(ts);
        const m = app.moneyFor(ds);
        const has = ts.length > 0;
        return {
          key: s,
          label: fmtMonthShortName(s),
          title: fmtMonthName(s),
          cons: has ? consumptionKwh(a) : null,
          solar: has ? a.solarKwh : null,
          imp: has ? a.importKwh : null,
          exp: has ? a.exportKwh : null,
          cost: has ? m.cost : null,
          saved: has ? m.saved : null,
        };
      });
    }
    const all = [];
    for (let d = range.start; d <= range.end; d = nextDay(d)) all.push(d);
    return all.map((d) => {
      const t = app.totalsOn(d);
      const m = app.moneyOn(d);
      return {
        key: d,
        label: period === 'week' ? fmtWeekday(d) : String(parseDate(d).getDate()),
        title: fmtDateShortWd(d),
        cons: t ? consumptionKwh(t) : null,
        solar: t ? t.solarKwh : null,
        imp: t ? t.importKwh : null,
        exp: t ? t.exportKwh : null,
        cost: m ? m.cost : null,
        saved: m ? m.saved : null,
      };
    });
  });
  function nextDay(d: string) {
    const x = parseDate(d);
    x.setDate(x.getDate() + 1);
    return `${x.getFullYear()}-${String(x.getMonth() + 1).padStart(2, '0')}-${String(x.getDate()).padStart(2, '0')}`;
  }
  const todayIdx = $derived(rows.findIndex((r) => r.key === todayIso() || (period === 'jaar' && todayIso().startsWith(r.key.slice(0, 7)))));

  function drill(i: number) {
    const r = rows[i];
    if (!r || r.cons == null) return;
    if (period === 'jaar') {
      period = 'maand';
      date = r.key;
    } else {
      period = 'dag';
      date = r.key;
    }
  }
</script>

<div class="page">
  <header class="page-head rise">
    <div>
      <p class="eyebrow">Energie</p>
      <h1>Stroom in en uit huis</h1>
      <p>Kies bovenaan <b>dag, week, maand of jaar</b> en blader met de pijltjes. Tik op een staaf om die dag te openen.</p>
    </div>
  </header>

  <div class="card rise picker" style="--i:1">
    <PeriodPicker bind:period bind:date min={app.firstDate} />
  </div>

  <section class="tiles">
    <StatTile i={2} label="Verbruikt" value={cons} format={n1} unit="kWh" icon={Zap} accent="var(--g-house)" />
    <StatTile i={3} label="Opgewekt" value={agg.solarKwh} format={n1} unit="kWh" icon={Sun} accent="var(--g-solar)" />
    <StatTile i={4} label="Van het net" value={agg.importKwh} format={n1} unit="kWh" icon={UtilityPole} accent="var(--g-grid)" sub={agg.peakImportW ? `piek ${power(agg.peakImportW)}` : undefined} />
    <StatTile i={5} label="Teruggeleverd" value={agg.exportKwh} format={n1} unit="kWh" icon={ArrowUpFromLine} accent="var(--g-grid)" />
    <StatTile i={6} label="Zelfvoorzienend" value={selfSufficiency(agg) * 100} format={n0} unit="%" icon={Leaf} accent="var(--g-money)" />
    <StatTile i={7} label="Stroomkosten" value={money.cost} format={euro} icon={Euro} accent="var(--g-grid)" sub="zonder vaste kosten" />
    <StatTile i={8} label="Bespaard" value={money.saved} format={euro} icon={PiggyBank} accent="var(--g-money)" sub={`zon ${euro(money.savedSolar)} · accu ${euro(money.savedBattery)}`} />
    {#if agg.gasM3 != null}
      <StatTile i={9} label="Gas" value={agg.gasM3} format={n2} unit="m³" icon={Flame} accent="var(--g-heat)" sub={euro(money.gasCost)} />
    {/if}
  </section>

  {#if period === 'dag'}
    <section class="card rise" style="--i:9">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-solar)"><Icon icon={Gauge} size={18} /></span>Vermogen door de dag</h2>
        <Explain>
          <p>Elke lijn laat zien hoeveel stroom er op dat moment was. <b>Verbruik</b> = het huis, <b>Zon</b> = de panelen.</p>
          <p><b>Net</b> boven de nullijn = stroom van het net, eronder = teruglevering. <b>Batterijen</b> boven = laden, onder = ontladen.</p>
          <p>Tik op een lijn in de legenda om hem aan of uit te zetten. Met <b>+</b> zoom je in, met het vierkantje maak je de grafiek groot.</p>
        </Explain>
      </div>
      {#if dayChart}
        <Chart
          x={dayChart.x}
          series={dayChart.series}
          height={320}
          xRange={[dayStart(range.start), dayStart(range.start) + 86400]}
          yFormat={axis}
          valueFormat={(v) => power(v, false)}
          ariaLabel="Vermogen door de dag"
        />
        <p class="note muted">Net: boven de nullijn = afname, eronder = teruglevering. Batterijen: boven = laden, onder = ontladen.</p>
      {:else if dayLoading}
        <div class="empty">Laden…</div>
      {:else}
        <div class="empty">Geen gegevens voor deze dag.</div>
      {/if}
    </section>
  {:else}
    <section class="card rise" style="--i:9">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-house)"><Icon icon={ChartColumn} size={18} /></span>Verbruik en opwek</h2>
        <button class="btn ghost" type="button" onclick={() => (showTable = !showTable)} aria-pressed={showTable}>
          <Icon icon={Table2} size={17} />{showTable ? 'Grafiek' : 'Tabel'}
        </button>
      </div>
      {#if showTable}
        <div class="table-wrap">
          <table class="data">
            <thead>
              <tr><th>{period === 'jaar' ? 'Maand' : 'Dag'}</th><th>Verbruik</th><th>Opwek</th><th>Van net</th><th>Terug</th><th>Kosten</th><th>Bespaard</th></tr>
            </thead>
            <tbody>
              {#each rows.filter((r) => r.cons != null) as r (r.key)}
                <tr>
                  <td>{r.title}</td>
                  <td>{n1(r.cons!)}</td>
                  <td>{n1(r.solar!)}</td>
                  <td>{n1(r.imp!)}</td>
                  <td>{n1(r.exp!)}</td>
                  <td>{euro(r.cost)}</td>
                  <td>{euro(r.saved)}</td>
                </tr>
              {/each}
            </tbody>
          </table>
        </div>
      {:else}
        <BarChart
          labels={rows.map((r) => r.label)}
          titles={rows.map((r) => r.title)}
          series={[
            { label: 'Verbruik', values: rows.map((r) => r.cons), color: '--c-house' },
            { label: 'Opwek', values: rows.map((r) => r.solar), color: '--c-solar' },
          ]}
          yFormat={axis}
          valueFormat={(v) => kwh(v)}
          highlight={todayIdx}
          onselect={drill}
          height={280}
          ariaLabel="Verbruik en opwek per {period === 'jaar' ? 'maand' : 'dag'} in kWh"
        />
      {/if}
    </section>

    <section class="card rise" style="--i:10">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-money)"><Icon icon={PiggyBank} size={18} /></span>Bespaard per {period === 'jaar' ? 'maand' : 'dag'}</h2>
        <span class="pill">Totaal {euro(money.saved)}</span>
      </div>
      <BarChart
        labels={rows.map((r) => r.label)}
        titles={rows.map((r) => r.title)}
        series={[{ label: 'Bespaard', values: rows.map((r) => r.saved), color: '--c-money' }]}
        yFormat={(v) => `€${n0(v)}`}
        valueFormat={euro}
        highlight={todayIdx}
        onselect={drill}
        height={200}
        ariaLabel="Bespaarde euro's per periode"
      />
    </section>
  {/if}

  <section class="splits">
    <div class="card rise" style="--i:11">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-solar)"><Icon icon={Sun} size={18} /></span>Waar ging de zonnestroom heen?</h2>
      </div>
      <SegBar
        label="Zonnestroom"
        format={(v) => kwh(v)}
        parts={[
          { label: 'Direct in huis gebruikt', value: sSplit.direct, color: '--c-house' },
          { label: 'In de batterijen', value: sSplit.toBattery, color: '--c-battery' },
          { label: 'Teruggeleverd aan het net', value: sSplit.toGrid, color: '--c-grid' },
        ]}
      />
    </div>
    <div class="card rise" style="--i:12">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-house)"><Icon icon={Zap} size={18} /></span>Waar kwam de stroom vandaan?</h2>
      </div>
      <SegBar
        label="Herkomst stroom"
        format={(v) => kwh(v)}
        parts={[
          { label: 'Rechtstreeks van de zon', value: srcSplit.fromSolar, color: '--c-solar' },
          { label: 'Uit de batterijen', value: srcSplit.fromBattery, color: '--c-battery' },
          { label: 'Van het net', value: srcSplit.fromGrid, color: '--c-grid' },
        ]}
      />
    </div>
  </section>

  <section class="card rise money" style="--i:13">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-money)"><Icon icon={Euro} size={18} /></span>Zo is de besparing berekend</h2>
    </div>
    <div class="calc">
      <div><span>Zonder zonnepanelen en batterijen had je betaald</span><b>{euro(money.costWithout)}</b></div>
      <div><span>Nu betaald voor stroom van het net</span><b>{euro(money.cost + money.exportCredit)}</b></div>
      <div><span>Vergoeding voor teruglevering</span><b>− {euro(money.exportCredit)}</b></div>
      <div class="total"><span>Bespaard</span><b>{euro(money.saved)}</b></div>
      <div class="muted small"><span>Vaste kosten in deze periode (niet meegeteld)</span><b>{euro(money.fixed)}</b></div>
    </div>
    <p class="note muted">Tarieven aanpassen kan bij <a href="#/instellingen">Instellingen</a>. Saldering geldt t/m 31 december 2026.</p>
  </section>
</div>

<style>
  .picker {
    padding: 12px;
    border-radius: 999px;
  }
  @media (max-width: 640px) {
    .picker {
      border-radius: var(--radius);
    }
  }
  .tiles {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
  }
  .splits {
    display: grid;
    gap: 16px;
  }
  .empty {
    height: 260px;
    display: grid;
    place-items: center;
    color: var(--text-3);
  }
  .note {
    font-size: 13px;
    margin-top: 10px;
  }
  .note a {
    text-decoration: underline;
  }
  .calc {
    display: grid;
    gap: 2px;
    font-size: 15px;
  }
  .calc div {
    display: flex;
    justify-content: space-between;
    gap: 16px;
    padding: 9px 0;
    border-bottom: 1px solid var(--border);
    color: var(--text-2);
  }
  .calc b {
    color: var(--text);
    font-variant-numeric: tabular-nums;
    white-space: nowrap;
  }
  .calc .total {
    font-size: 17px;
    border-bottom: 0;
    color: var(--text);
    font-weight: 650;
  }
  .calc .total b {
    color: var(--s-good-text);
  }
  .calc .small {
    font-size: 13px;
    border-bottom: 0;
  }
  @media (min-width: 720px) {
    .tiles {
      grid-template-columns: repeat(4, minmax(0, 1fr));
    }
    .splits {
      grid-template-columns: 1fr 1fr;
    }
  }
</style>
