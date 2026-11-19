<script lang="ts">
  import { BatteryCharging, BatteryMedium, Repeat, PiggyBank, ArrowDownToLine, ArrowUpFromLine, Activity } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import AnimatedNumber from '../lib/components/AnimatedNumber.svelte';
  import StatTile from '../lib/components/StatTile.svelte';
  import Chart from '../lib/components/Chart.svelte';
  import BarChart from '../lib/components/BarChart.svelte';
  import PeriodPicker, { type Period } from '../lib/components/PeriodPicker.svelte';
  import { app } from '../lib/data/app.svelte';
  import { aggregate } from '../lib/calc/energy';
  import { addDays, dayStart, fmtDateShortWd, parseDate, rangeDays, todayIso } from '../lib/date';
  import { axis, euro, kwh, n0, n1, power } from '../lib/format';
  import type { DayData } from '../lib/types';

  const today = todayIso();
  let period = $state<Period>('dag');
  let date = $state(today);
  let day = $state<DayData | null>(null);

  $effect(() => {
    const d = date;
    void app.today;
    app.day(d).then((r) => {
      if (date === d) day = r;
    });
  });

  const live = $derived(app.live!);
  const last30 = rangeDays(addDays(today, -29), today);
  const ytd = $derived(rangeDays(today.slice(0, 4) + '-01-01', today));
  const yAgg = $derived(aggregate(ytd.map((d) => app.totalsOn(d)).filter((t) => t != null)));
  const yMoney = $derived(app.moneyFor(ytd));
  const capTotal = $derived(live.batteries.reduce((a, b) => a + b.capacityKwh, 0));
  const t = $derived(app.today?.totals);

  const x = $derived(day ? Array.from({ length: day.n }, (_, i) => day!.start + i * day!.step) : []);
  const batState = (w: number) => (w > 25 ? 'Laden' : w < -25 ? 'Ontladen' : 'In rust');
</script>

<div class="page">
  <header class="page-head rise">
    <div>
      <p class="eyebrow">Thuisbatterijen</p>
      <h1>2 × Marstek Venus E V3</h1>
      <p>Samen {n1(capTotal)} kWh opslag. Ze laden met zonne-overschot en ontladen als het huis stroom vraagt.</p>
    </div>
  </header>

  <section class="bats">
    {#each live.batteries as b, i (b.id)}
      <div class="bat card rise" style="--i:{i + 1}">
        <div class="ring" style="--p:{b.soc}">
          <div class="inner">
            <span class="soc"><AnimatedNumber value={b.soc} format={n0} /><small>%</small></span>
            <span class="kwh">{n1((b.soc / 100) * b.capacityKwh)} kWh</span>
          </div>
        </div>
        <div class="info">
          <h2>{app.nameOf(b.id)}</h2>
          <span class="state" class:charging={b.w > 25} class:discharging={b.w < -25}>
            <Icon icon={b.w > 25 ? BatteryCharging : BatteryMedium} size={16} />
            {batState(b.w)}{#if Math.abs(b.w) > 25}&nbsp;· {power(b.w)}{/if}
          </span>
          <dl>
            <div><dt>Capaciteit</dt><dd>{n1(b.capacityKwh)} kWh</dd></div>
            <div><dt>Bruikbaar nu</dt><dd>{n1(Math.max(0, (b.soc - 10) / 100) * b.capacityKwh)} kWh</dd></div>
          </dl>
        </div>
      </div>
    {/each}
  </section>

  {#if t}
    <section class="tiles">
      <StatTile i={3} label="Geladen vandaag" value={t.batChargeKwh} format={n1} unit="kWh" icon={ArrowDownToLine} accent="var(--g-battery)" />
      <StatTile i={4} label="Ontladen vandaag" value={t.batDischargeKwh} format={n1} unit="kWh" icon={ArrowUpFromLine} accent="var(--g-house)" />
      <StatTile i={5} label="Cycli dit jaar" value={yAgg.batDischargeKwh / capTotal} format={n0} icon={Repeat} accent="var(--g-battery)" sub={`${n0(yAgg.batDischargeKwh)} kWh ontladen`} />
      <StatTile i={6} label="Opbrengst dit jaar" value={yMoney.savedBattery} format={euro} icon={PiggyBank} accent="var(--g-money)" sub="t.o.v. terugleveren" />
    </section>
  {/if}

  <div class="card rise picker" style="--i:7">
    <PeriodPicker bind:period bind:date periods={['dag']} min={app.firstDate} />
  </div>

  <section class="card rise" style="--i:8">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-battery)"><Icon icon={BatteryMedium} size={18} /></span>Laadniveau</h2>
    </div>
    {#if day}
      <Chart
        {x}
        series={[
          { label: app.nameOf('bat1'), values: day.ch.bat1_soc, color: '--c-battery', fill: true },
          { label: app.nameOf('bat2'), values: day.ch.bat2_soc, color: '--c-house' },
        ]}
        height={240}
        yMin={0}
        yMax={100}
        xRange={[dayStart(date), dayStart(date) + 86400]}
        yFormat={(v) => `${n0(v)}%`}
        valueFormat={(v) => `${n0(v)}%`}
        ariaLabel="Laadniveau van beide batterijen"
      />
    {/if}
  </section>

  <section class="card rise" style="--i:9">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-battery)"><Icon icon={Activity} size={18} /></span>Laad- en ontlaadvermogen</h2>
    </div>
    {#if day}
      <Chart
        {x}
        series={[
          { label: app.nameOf('bat1'), values: day.ch.bat1_w, color: '--c-battery' },
          { label: app.nameOf('bat2'), values: day.ch.bat2_w, color: '--c-house' },
        ]}
        height={220}
        xRange={[dayStart(date), dayStart(date) + 86400]}
        yFormat={axis}
        valueFormat={(v) => `${power(v)} ${v > 0 ? 'laden' : v < 0 ? 'ontladen' : ''}`}
        ariaLabel="Laad- en ontlaadvermogen per batterij"
      />
      <p class="note muted">Boven de nullijn = laden, eronder = ontladen.</p>
    {/if}
  </section>

  <section class="card rise" style="--i:10">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-battery)"><Icon icon={Repeat} size={18} /></span>Laatste 30 dagen</h2>
    </div>
    <BarChart
      labels={last30.map((d) => String(parseDate(d).getDate()))}
      titles={last30.map(fmtDateShortWd)}
      series={[
        { label: 'Geladen', values: last30.map((d) => app.totalsOn(d)?.batChargeKwh ?? null), color: '--c-battery' },
        { label: 'Ontladen', values: last30.map((d) => app.totalsOn(d)?.batDischargeKwh ?? null), color: '--c-house' },
      ]}
      yFormat={axis}
      valueFormat={(v) => kwh(v)}
      highlight={29}
      onselect={(i) => (date = last30[i])}
      ariaLabel="Geladen en ontladen kWh per dag, laatste 30 dagen"
    />
  </section>
</div>

<style>
  .picker {
    padding: 12px;
    border-radius: 999px;
  }
  .bats {
    display: grid;
    gap: 16px;
  }
  .bat {
    display: flex;
    align-items: center;
    gap: 22px;
    padding: 22px;
  }
  .ring {
    --size: 132px;
    width: var(--size);
    height: var(--size);
    flex: none;
    border-radius: 50%;
    display: grid;
    place-items: center;
    background:
      radial-gradient(circle, var(--surface) 64%, transparent 65%),
      conic-gradient(from -90deg, var(--g-battery) calc(var(--p) * 1%), color-mix(in srgb, var(--g-battery) 12%, transparent) 0);
    box-shadow: 0 0 50px -14px color-mix(in srgb, var(--g-battery) 60%, transparent);
    transition: background 1s;
  }
  .inner {
    display: grid;
    justify-items: center;
  }
  .soc {
    font-size: 36px;
    font-weight: 700;
    letter-spacing: -0.04em;
    line-height: 1;
  }
  .soc small {
    font-size: 16px;
    color: var(--text-2);
    margin-left: 2px;
  }
  .kwh {
    font-size: 13px;
    color: var(--text-3);
    margin-top: 4px;
  }
  .info {
    display: grid;
    gap: 10px;
    min-width: 0;
  }
  .info h2 {
    font-size: 20px;
  }
  .state {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 14px;
    font-weight: 600;
    color: var(--text-3);
  }
  .state.charging {
    color: var(--g-battery);
  }
  .state.discharging {
    color: var(--g-house);
  }
  dl {
    margin: 0;
    display: grid;
    gap: 4px;
  }
  dl div {
    display: flex;
    gap: 10px;
    font-size: 14px;
  }
  dt {
    color: var(--text-3);
  }
  dd {
    margin: 0;
    font-weight: 600;
  }
  .tiles {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
  }
  .note {
    font-size: 13px;
    margin-top: 10px;
  }
  @media (min-width: 720px) {
    .bats {
      grid-template-columns: 1fr 1fr;
    }
    .tiles {
      grid-template-columns: repeat(4, minmax(0, 1fr));
    }
  }
  @media (max-width: 420px) {
    .bat {
      gap: 16px;
      padding: 18px;
    }
    .ring {
      --size: 110px;
    }
  }
</style>
