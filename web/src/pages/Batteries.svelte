<script lang="ts">
  import {
    BatteryCharging,
    BatteryMedium,
    Repeat,
    PiggyBank,
    ArrowDownToLine,
    ArrowUpFromLine,
    Activity,
    Gauge,
    ChartColumn,
  } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import AnimatedNumber from '../lib/components/AnimatedNumber.svelte';
  import StatTile from '../lib/components/StatTile.svelte';
  import Chart from '../lib/components/Chart.svelte';
  import BarChart from '../lib/components/BarChart.svelte';
  import Explain from '../lib/components/Explain.svelte';
  import PeriodPicker, { type Period } from '../lib/components/PeriodPicker.svelte';
  import { app } from '../lib/data/app.svelte';
  import { aggregate } from '../lib/calc/energy';
  import { addDays, dayStart, fmtDateShortWd, parseDate, rangeDays, todayIso } from '../lib/date';
  import { axis, euro, kwh, n0, n1, n2, power } from '../lib/format';
  import type { Battery, DayData } from '../lib/types';

  const today = todayIso();
  const MIN_SOC = 10; // Marstek ontlaadt standaard tot ongeveer 10%
  const TH = 25;
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
  const agg30 = $derived(aggregate(last30.map((d) => app.totalsOn(d)).filter((t) => t != null)));
  const efficiency = $derived(agg30.batChargeKwh > 0.5 ? (agg30.batDischargeKwh / agg30.batChargeKwh) * 100 : null);
  const capTotal = $derived(live.batteries.reduce((a, b) => a + b.capacityKwh, 0));
  const t = $derived(app.today?.totals);

  const x = $derived(day ? Array.from({ length: day.n }, (_, i) => day!.start + i * day!.step) : []);

  // kWh geladen/ontladen uit een 5-minutenreeks
  function integ(vals: (number | null)[] | undefined, sign: 1 | -1, from = 0, to = Infinity) {
    let wh = 0;
    (vals ?? []).forEach((v, i) => {
      if (v == null || i < from || i >= to) return;
      if (sign > 0 && v > 0) wh += (v * 300) / 3600;
      if (sign < 0 && v < 0) wh += (-v * 300) / 3600;
    });
    return wh / 1000;
  }
  // Per batterij vandaag
  const todayPer = $derived(
    live.batteries.map((b, i) => {
      const s = app.today?.ch[i === 0 ? 'bat1_w' : 'bat2_w'];
      return { chg: integ(s, 1), dis: integ(s, -1) };
    }),
  );

  // Wanneer vol / leeg?
  function eta(b: Battery): string | null {
    const cap = b.capacityKwh;
    if (b.w > TH) {
      const h = (((100 - b.soc) / 100) * cap) / (b.w / 1000);
      if (h > 24 || h <= 0) return null;
      const d = new Date(Date.now() + h * 3600_000);
      return `vol rond ${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
    }
    if (b.w < -TH) {
      const h = (((b.soc - MIN_SOC) / 100) * cap) / (-b.w / 1000);
      if (h <= 0) return 'bijna leeg';
      if (h > 48) return null;
      const hh = Math.floor(h);
      const mm = Math.round((h - hh) * 60);
      return `nog ±${hh ? hh + ' u ' : ''}${mm} min`;
    }
    return null;
  }

  // Laden en ontladen per uur (beide batterijen samen)
  const hourly = $derived.by(() => {
    if (!day) return null;
    const perHour = Math.round(3600 / day.step);
    const chg: number[] = [];
    const dis: number[] = [];
    for (let h = 0; h < 24; h++) {
      const a = h * perHour;
      const b = a + perHour;
      chg.push(integ(day.ch.bat1_w, 1, a, b) + integ(day.ch.bat2_w, 1, a, b));
      dis.push(-(integ(day.ch.bat1_w, -1, a, b) + integ(day.ch.bat2_w, -1, a, b)));
    }
    return { chg, dis };
  });
  const hourLabels = Array.from({ length: 24 }, (_, h) => String(h));
  const hourTitles = Array.from({ length: 24 }, (_, h) => `${String(h).padStart(2, '0')}:00 – ${String((h + 1) % 24).padStart(2, '0')}:00`);
  const nowHour = new Date().getHours();
</script>

<div class="page">
  <header class="page-head rise">
    <div>
      <p class="eyebrow">Thuisbatterijen</p>
      <h1>2 × Marstek Venus E</h1>
      <p>Samen {n1(capTotal)} kWh opslag. Ze laden met zonne-overschot en ontladen als het huis stroom vraagt.</p>
    </div>
    <Explain label="Hoe werkt het?">
      <p><b>Laden</b>: er is meer zonnestroom dan het huis gebruikt, het overschot gaat de batterij in. <b>Ontladen</b>: het huis vraagt stroom en de batterij levert die, zodat er minder van het net komt.</p>
      <p><b>Bruikbaar</b> is wat er nog uit kan tot de batterij op ±{MIN_SOC}% stopt. <b>Rendement</b>: hoeveel van de geladen stroom er weer uit komt (een paar procent gaat verloren als warmte). <b>Cycli</b>: hoe vaak de batterij in totaal één keer helemaal is leeggegaan.</p>
    </Explain>
  </header>

  <section class="bats">
    {#each live.batteries as b, i (b.id)}
      {@const st = b.w > TH ? 'charge' : b.w < -TH ? 'discharge' : 'idle'}
      {@const e = eta(b)}
      <div class="bat card rise {st}" style="--i:{i + 1}">
        <div class="ring" style="--p:{b.soc}">
          <div class="inner">
            <span class="soc"><AnimatedNumber value={b.soc} format={n0} /><small>%</small></span>
            <span class="kwh">{n1((b.soc / 100) * b.capacityKwh)} van {n1(b.capacityKwh)} kWh</span>
          </div>
        </div>
        <div class="info">
          <h2>{app.nameOf(b.id)}</h2>
          <span class="state">
            <Icon icon={st === 'charge' ? BatteryCharging : BatteryMedium} size={17} />
            {#if st === 'charge'}Laden · {power(b.w)}{:else if st === 'discharge'}Ontladen · {power(b.w)}{:else}In rust{/if}
          </span>
          {#if e}<span class="eta">{e}</span>{/if}
          <dl>
            <div><dt>Bruikbaar nu</dt><dd>{n1(Math.max(0, (b.soc - MIN_SOC) / 100) * b.capacityKwh)} kWh</dd></div>
            <div><dt>Vandaag</dt><dd><span class="up">↓ {n1(todayPer[i]?.chg ?? 0)}</span> <span class="down">↑ {n1(todayPer[i]?.dis ?? 0)}</span> kWh</dd></div>
            {#if b.totalChargeKwh != null}
              <div><dt>Sinds installatie</dt><dd><span class="up">↓ {n0(b.totalChargeKwh)}</span> <span class="down">↑ {n0(b.totalDischargeKwh ?? 0)}</span> kWh</dd></div>
            {/if}
          </dl>
        </div>
      </div>
    {/each}
  </section>
  <p class="legend-note muted"><span class="up">↓ geladen</span> · <span class="down">↑ ontladen</span></p>

  {#if t}
    <section class="tiles">
      <StatTile i={3} label="Geladen vandaag" value={t.batChargeKwh} format={n1} unit="kWh" icon={ArrowDownToLine} accent="var(--g-battery)" sub="beide samen" />
      <StatTile i={4} label="Ontladen vandaag" value={t.batDischargeKwh} format={n1} unit="kWh" icon={ArrowUpFromLine} accent="var(--g-house)" sub="naar het huis" />
      <StatTile i={5} label="Rendement" value={efficiency} format={n0} unit="%" icon={Gauge} accent="var(--g-battery)" sub={`laatste 30 dagen · ${n0(yAgg.batDischargeKwh / capTotal)} cycli dit jaar`} />
      <StatTile i={6} label="Opbrengst dit jaar" value={yMoney.savedBattery} format={euro} icon={PiggyBank} accent="var(--g-money)" sub={yMoney.savedBattery < 0 ? 'nog negatief door saldering (tot 2027)' : 't.o.v. terugleveren'} />
    </section>
  {/if}

  <div class="card rise picker" style="--i:7">
    <PeriodPicker bind:period bind:date periods={['dag']} min={app.firstDate} />
  </div>

  <section class="card rise" style="--i:8">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-battery)"><Icon icon={ChartColumn} size={18} /></span>Laden en ontladen per uur</h2>
      <Explain>
        <p>Elk staafje is één uur. <b>Omhoog</b> = zoveel kWh is er dat uur in de batterijen gegaan (laden), <b>omlaag</b> = zoveel kWh kwam eruit naar het huis (ontladen). Beide batterijen samen.</p>
      </Explain>
    </div>
    {#if hourly}
      <BarChart
        labels={hourLabels}
        titles={hourTitles}
        series={[
          { label: 'Geladen', values: hourly.chg, color: '--c-battery' },
          { label: 'Ontladen', values: hourly.dis, color: '--c-house' },
        ]}
        yFormat={(v) => (Math.abs(v) < 1 && v !== 0 ? n2(Math.abs(v)) : n1(Math.abs(v)))}
        valueFormat={(v) => kwh(Math.abs(v), 2)}
        minGroupWidth={13}
        highlight={date === today ? nowHour : -1}
        height={240}
        ariaLabel="Geladen en ontladen kWh per uur"
      />
    {/if}
  </section>

  <section class="card rise" style="--i:9">
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
        title="Laadniveau"
        ariaLabel="Laadniveau van beide batterijen"
      />
    {/if}
  </section>

  <section class="card rise" style="--i:10">
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
        title="Laad- en ontlaadvermogen"
        ariaLabel="Laad- en ontlaadvermogen per batterij"
      />
      <p class="note muted">Boven de nullijn = laden, eronder = ontladen.</p>
    {/if}
  </section>

  <section class="card rise" style="--i:11">
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
    --size: 136px;
    width: var(--size);
    height: var(--size);
    flex: none;
    border-radius: 50%;
    display: grid;
    place-items: center;
    background:
      radial-gradient(circle, var(--surface) 64%, transparent 65%),
      conic-gradient(from 0deg, var(--g-battery) calc(var(--p) * 1%), color-mix(in srgb, var(--g-battery) 12%, transparent) 0);
    transition: box-shadow 0.6s;
  }
  .bat.charge .ring {
    box-shadow: 0 0 50px -12px color-mix(in srgb, var(--g-battery) 70%, transparent);
  }
  .bat.discharge .ring {
    box-shadow: 0 0 50px -12px color-mix(in srgb, var(--g-house) 70%, transparent);
  }
  .inner {
    display: grid;
    justify-items: center;
    text-align: center;
  }
  .soc {
    font-family: var(--font-display);
    font-size: 38px;
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
    font-size: 12px;
    color: var(--text-3);
    margin-top: 4px;
    max-width: 96px;
  }
  .info {
    display: grid;
    gap: 6px;
    min-width: 0;
  }
  .info h2 {
    font-size: 20px;
  }
  .state {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 15px;
    font-weight: 650;
    color: var(--text-3);
  }
  .charge .state {
    color: var(--g-battery);
  }
  .discharge .state {
    color: var(--g-house);
  }
  .eta {
    font-size: 13.5px;
    color: var(--text-2);
  }
  dl {
    margin: 6px 0 0;
    display: grid;
    gap: 4px;
  }
  dl div {
    display: flex;
    gap: 10px;
    font-size: 14px;
    flex-wrap: wrap;
  }
  dt {
    color: var(--text-3);
    min-width: 116px;
  }
  dd {
    margin: 0;
    font-weight: 600;
    font-variant-numeric: tabular-nums;
  }
  .up {
    color: var(--g-battery);
  }
  .down {
    color: var(--g-house);
  }
  .legend-note {
    font-size: 13px;
    margin: -8px 4px 0;
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
  @media (max-width: 480px) {
    dl div {
      display: grid;
      gap: 0;
    }
    .bat {
      gap: 16px;
      padding: 18px;
    }
    .ring {
      --size: 112px;
    }
    .soc {
      font-size: 32px;
    }
    dt {
      min-width: 0;
    }
  }
</style>
