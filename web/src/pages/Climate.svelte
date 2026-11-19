<script lang="ts">
  import { Flame, Snowflake, Power, Thermometer, Zap, CloudSun, Clock } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import Sparkline from '../lib/components/Sparkline.svelte';
  import Chart from '../lib/components/Chart.svelte';
  import BarChart from '../lib/components/BarChart.svelte';
  import PeriodPicker, { type Period } from '../lib/components/PeriodPicker.svelte';
  import { app } from '../lib/data/app.svelte';
  import { aggregate } from '../lib/calc/energy';
  import { addDays, dayStart, fmtDateShortWd, fmtWeekday, monthStart, rangeDays, todayIso } from '../lib/date';
  import { kwh, n1, temp } from '../lib/format';
  import { DEVICES, type DayData } from '../lib/types';

  const today = todayIso();
  const live = $derived(app.live!);
  let selected = $state(DEVICES.climate[0].id);
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

  const todayDay = $derived(app.today);
  const monthAgg = $derived(aggregate(rangeDays(monthStart(today), today).map((d) => app.totalsOn(d)).filter((t) => t != null)));
  const maxKwh = $derived(Math.max(0.1, ...live.climate.map((c) => c.kwhToday ?? 0)));
  const last14 = rangeDays(addDays(today, -13), today);
  const selName = $derived(app.nameOf(selected));
  const x = $derived(day ? Array.from({ length: day.n }, (_, i) => day!.start + i * day!.step) : []);
  const t = $derived(todayDay?.totals);
</script>

<div class="page">
  <header class="page-head rise">
    <div>
      <p class="eyebrow">Klimaat</p>
      <h1>Kamers & verwarming</h1>
      <p>Tik op een kamer om het temperatuurverloop van die dag te zien.</p>
    </div>
    <div class="outside card">
      <Icon icon={CloudSun} size={28} />
      <div>
        <span class="o-temp">{temp(live.weather.temp)}</span>
        <span class="o-sub">Buiten{#if t} · {temp(t.tOutMin, 0)} / {temp(t.tOutMax, 0)}{/if}</span>
      </div>
    </div>
  </header>

  <section class="rooms">
    {#each live.climate as c, i (c.id)}
      <button
        type="button"
        class="room card rise"
        class:sel={selected === c.id}
        style="--i:{i + 1}"
        onclick={() => (selected = c.id)}
        aria-pressed={selected === c.id}
      >
        <div class="r-top">
          <span class="r-name">{app.nameOf(c.id)}</span>
          {#if c.mode === 'heat'}
            <span class="mode heat"><Icon icon={Flame} size={14} />Verwarmen</span>
          {:else if c.mode === 'cool'}
            <span class="mode cool"><Icon icon={Snowflake} size={14} />Koelen</span>
          {:else}
            <span class="mode off"><Icon icon={Power} size={14} />Uit</span>
          {/if}
        </div>
        <div class="r-mid">
          <span class="r-temp">{temp(c.temp)}</span>
          {#if c.on && c.target != null}<span class="r-target">→ {temp(c.target)}</span>{/if}
        </div>
        {#if todayDay}
          <Sparkline values={todayDay.ch['ac_' + c.id + '_t'] ?? []} color={c.mode === 'cool' ? '--c-house' : '--c-heat'} label="Temperatuur {app.nameOf(c.id)} vandaag" />
        {/if}
        <div class="r-foot"><Icon icon={Zap} size={13} /> {kwh(c.kwhToday)} vandaag</div>
      </button>
    {/each}
  </section>

  <div class="card rise picker" style="--i:7">
    <PeriodPicker bind:period bind:date periods={['dag']} min={app.firstDate} />
  </div>

  <section class="card rise" style="--i:8">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-heat)"><Icon icon={Thermometer} size={18} /></span>{selName}</h2>
      <select class="room-select" bind:value={selected} aria-label="Kies kamer">
        {#each DEVICES.climate as c (c.id)}<option value={c.id}>{app.nameOf(c.id)}</option>{/each}
      </select>
    </div>
    {#if day}
      <Chart
        {x}
        series={[
          { label: 'Kamer', values: day.ch['ac_' + selected + '_t'], color: '--c-house', width: 2.5 },
          { label: 'Ingesteld', values: day.ch['ac_' + selected + '_set'], color: '--c-heat', dash: [6, 5], step: true },
          { label: 'Buiten', values: day.ch.t_out, color: '--c-grid' },
        ]}
        height={280}
        xRange={[dayStart(date), dayStart(date) + 86400]}
        yFormat={(v) => `${v}°`}
        valueFormat={(v) => temp(v)}
        ariaLabel="Temperatuur {selName}, ingestelde temperatuur en buitentemperatuur"
      />
    {/if}
  </section>

  <section class="two">
    <div class="card rise" style="--i:9">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-house)"><Icon icon={Zap} size={18} /></span>Stroomverbruik airco's</h2>
      </div>
      <ul class="hbars">
        {#each live.climate as c (c.id)}
          <li>
            <span class="hb-name">{app.nameOf(c.id)}</span>
            <span class="hb-track"><span class="hb-fill" style="width:{((c.kwhToday ?? 0) / maxKwh) * 100}%"></span></span>
            <b>{kwh(c.kwhToday)}</b>
            <span class="hb-month">{kwh(monthAgg.climateKwh[c.id] ?? 0, 0)} deze maand</span>
          </li>
        {/each}
      </ul>
      <p class="note muted">Daikin geeft het verbruik per airco door via Onecta (vertraging tot ~2 uur).</p>
    </div>

    <div class="card rise" style="--i:10">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-heat)"><Icon icon={Flame} size={18} /></span>Kachel & kacheltjes</h2>
      </div>
      <div class="heaters">
        {#each live.heaters as h (h.id)}
          <div class="heater" class:on={h.state === 'on'}>
            <span class="h-ico"><Icon icon={Flame} size={20} /></span>
            <div class="h-body">
              <span class="h-name">{app.nameOf(h.id)}</span>
              <span class="h-state">
                {h.state === 'on' ? (h.kind === 'pellet' && h.level ? `Aan · stand ${h.level}` : 'Aan') : h.state === 'starting' ? 'Ontsteken' : h.state === 'cooling' ? 'Afkoelen' : h.state === 'unknown' ? 'Niet gekoppeld' : 'Uit'}
                {#if h.temp != null}· {temp(h.temp)}{/if}
              </span>
            </div>
            <span class="h-hours"><Icon icon={Clock} size={13} /> {n1(t?.heaterHours[h.id] ?? 0)} u</span>
          </div>
        {/each}
      </div>
      <h3 class="mini-title">Pelletkachel: uren aan per dag</h3>
      <BarChart
        labels={last14.map(fmtWeekday)}
        titles={last14.map(fmtDateShortWd)}
        series={[{ label: 'Uren aan', values: last14.map((d) => app.totalsOn(d)?.heaterHours.pellet ?? null), color: '--c-heat' }]}
        yFormat={(v) => `${v}u`}
        valueFormat={(v) => `${n1(v)} uur`}
        highlight={13}
        onselect={(i) => (date = last14[i])}
        height={170}
        ariaLabel="Uren dat de pelletkachel aan stond, laatste 14 dagen"
      />
    </div>
  </section>
</div>

<style>
  .outside {
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 12px 18px;
    color: var(--g-solar);
  }
  .outside div {
    display: grid;
    line-height: 1.15;
  }
  .o-temp {
    font-size: 26px;
    font-weight: 650;
    color: var(--text);
    letter-spacing: -0.02em;
  }
  .o-sub {
    font-size: 13px;
    color: var(--text-2);
  }
  .picker {
    padding: 12px;
    border-radius: 999px;
  }
  .rooms {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
  }
  .room {
    display: grid;
    gap: 8px;
    text-align: left;
    cursor: pointer;
    padding: 16px;
    transition: transform 0.35s var(--ease), border-color 0.3s, box-shadow 0.3s;
  }
  .room:hover {
    transform: translateY(-2px);
  }
  .room.sel {
    border-color: color-mix(in srgb, var(--g-house) 55%, transparent);
    box-shadow: 0 0 0 1px color-mix(in srgb, var(--g-house) 40%, transparent), var(--shadow);
  }
  .r-top {
    display: flex;
    justify-content: space-between;
    gap: 6px;
    flex-wrap: wrap;
    align-items: center;
  }
  .r-name {
    font-weight: 600;
    font-size: 14.5px;
  }
  .r-mid {
    display: flex;
    align-items: baseline;
    gap: 8px;
  }
  .r-temp {
    font-size: 34px;
    font-weight: 650;
    letter-spacing: -0.03em;
    line-height: 1.05;
  }
  .r-target {
    color: var(--text-3);
    font-size: 14px;
  }
  .r-foot {
    display: flex;
    align-items: center;
    gap: 5px;
    font-size: 13px;
    color: var(--text-3);
  }
  .mode {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    font-size: 12px;
    font-weight: 650;
    padding: 3px 8px;
    border-radius: 999px;
  }
  .mode.heat {
    color: var(--g-heat);
    background: color-mix(in srgb, var(--g-heat) 13%, transparent);
  }
  .mode.cool {
    color: var(--g-cool);
    background: color-mix(in srgb, var(--g-cool) 13%, transparent);
  }
  .mode.off {
    color: var(--text-3);
    background: var(--surface-2);
  }
  .room-select {
    height: 38px;
    border-radius: 999px;
    border: 1px solid var(--border-strong);
    background: var(--surface-2);
    color: var(--text);
    padding: 0 12px;
    font: inherit;
    font-size: 14px;
    font-weight: 600;
  }
  .two {
    display: grid;
    gap: 16px;
  }
  .hbars {
    list-style: none;
    margin: 0;
    padding: 0;
    display: grid;
    gap: 14px;
  }
  .hbars li {
    display: grid;
    grid-template-columns: 1fr auto;
    gap: 4px 12px;
    align-items: center;
  }
  .hb-name {
    font-size: 14px;
    font-weight: 550;
  }
  .hbars b {
    font-size: 14px;
    font-variant-numeric: tabular-nums;
    text-align: right;
  }
  .hb-track {
    grid-column: 1;
    height: 8px;
    border-radius: 999px;
    background: var(--surface-2);
    overflow: hidden;
  }
  .hb-fill {
    display: block;
    height: 100%;
    border-radius: 999px;
    background: var(--c-house);
    transition: width 1s var(--ease);
  }
  .hb-month {
    font-size: 12.5px;
    color: var(--text-3);
    text-align: right;
  }
  .note {
    font-size: 13px;
    margin-top: 14px;
  }
  .heaters {
    display: grid;
    gap: 10px;
    margin-bottom: 18px;
  }
  .heater {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 10px 12px;
    border-radius: 14px;
    background: var(--surface-2);
  }
  .h-ico {
    display: grid;
    place-items: center;
    width: 38px;
    height: 38px;
    border-radius: 12px;
    color: var(--text-3);
    background: var(--surface-3);
    flex: none;
  }
  .heater.on .h-ico {
    color: var(--g-heat);
    background: color-mix(in srgb, var(--g-heat) 16%, transparent);
  }
  .h-body {
    display: grid;
    flex: 1;
    min-width: 0;
  }
  .h-name {
    font-weight: 600;
    font-size: 14px;
  }
  .h-state {
    font-size: 13px;
    color: var(--text-2);
  }
  .h-hours {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    font-size: 13px;
    color: var(--text-2);
    white-space: nowrap;
  }
  .mini-title {
    font-size: 14px;
    font-weight: 600;
    color: var(--text-2);
    margin-bottom: 10px;
  }
  @media (min-width: 720px) {
    .rooms {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
  }
  @media (min-width: 1100px) {
    .two {
      grid-template-columns: 1fr 1fr;
    }
  }
</style>
