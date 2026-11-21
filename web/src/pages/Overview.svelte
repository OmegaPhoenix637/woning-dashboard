<script lang="ts">
  import {
    Sun,
    Zap,
    UtilityPole,
    ArrowUpFromLine,
    Leaf,
    BatteryMedium,
    Flame,
    Snowflake,
    Power,
    WashingMachine,
    CookingPot,
    CloudSun,
    Cloud,
    Moon,
    PiggyBank,
    ChevronRight,
    Info,
  } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import EnergyScene from '../lib/components/EnergyScene.svelte';
  import StatTile from '../lib/components/StatTile.svelte';
  import Explain from '../lib/components/Explain.svelte';
  import AnimatedNumber from '../lib/components/AnimatedNumber.svelte';
  import Chart from '../lib/components/Chart.svelte';
  import SegBar from '../lib/components/SegBar.svelte';
  import { app } from '../lib/data/app.svelte';
  import { href } from '../lib/router.svelte';
  import {
    houseW,
    houseSeries,
    batterySeries,
    consumptionKwh,
    selfSufficiency,
    integrateKwh,
    sourceSplit,
  } from '../lib/calc/energy';
  import { addDays, dayStart, fmtDateLong, greeting, monthStart, rangeDays, todayIso, weekStart } from '../lib/date';
  import { euro, n1, n0, n2, power, temp, kwh, axis } from '../lib/format';
  import type { DayData } from '../lib/types';

  const today = todayIso();
  const live = $derived(app.live!);
  const day = $derived(app.today);
  const tot = $derived(day?.totals);

  const house = $derived(live ? houseW(live.grid.w, live.solar.w, live.batteries.map((b) => b.w), live.ev?.w ?? 0) : 0);
  const mToday = $derived(app.moneyFor([today]));
  const mWeek = $derived(app.moneyFor(rangeDays(weekStart(today), today)));
  const mMonth = $derived(app.moneyFor(rangeDays(monthStart(today), today)));
  const mYear = $derived(app.moneyFor(rangeDays(today.slice(0, 4) + '-01-01', today)));
  const avgSoc = $derived(
    live ? live.batteries.reduce((a, b) => a + b.soc * b.capacityKwh, 0) / Math.max(0.01, live.batteries.reduce((a, b) => a + b.capacityKwh, 0)) : 0,
  );

  // Vergelijk met gisteren tot hetzelfde tijdstip.
  let yesterday = $state<DayData | null>(null);
  $effect(() => {
    app.day(addDays(today, -1)).then((d) => (yesterday = d));
  });
  const ySame = $derived.by(() => {
    if (!yesterday || !day) return null;
    const idx = day.ch.grid_w.findLastIndex((v) => v != null) + 1;
    const cut = (a: (number | null)[]) => a.slice(0, idx);
    return {
      solar: integrateKwh(cut(yesterday.ch.solar_w), yesterday.step),
      house: integrateKwh(cut(houseSeries(yesterday)), yesterday.step),
    };
  });
  function delta(nowV: number, then: number | undefined) {
    if (then == null || then < 0.3) return null;
    const p = ((nowV - then) / then) * 100;
    if (Math.abs(p) < 1) return null;
    return { text: `${n0(Math.abs(p))}% vs gisteren`, up: p > 0 };
  }

  const chart = $derived.by(() => {
    if (!day) return null;
    const x = Array.from({ length: day.n }, (_, i) => day.start + i * day.step);
    return {
      x,
      series: [
        { label: 'Verbruik', values: houseSeries(day), color: '--c-house', fill: true },
        { label: 'Zon', values: day.ch.solar_w, color: '--c-solar', fill: true },
        { label: 'Batterijen', values: batterySeries(day), color: '--c-battery' },
        { label: 'Net', values: day.ch.grid_w, color: '--c-grid' },
      ],
    };
  });

  const nightNow = $derived(live ? live.solar.w < 25 : false);
  const weatherIcon = $derived(nightNow ? Moon : (live?.weather.cloud ?? 50) > 65 ? Cloud : CloudSun);

  let hideDemo = $state(false);
  try {
    hideDemo = sessionStorage.getItem('mw.hideDemo') === '1';
  } catch {
    /* */
  }
</script>

{#if live}
  <div class="page">
    <header class="page-head rise">
      <div>
        <p class="eyebrow">{fmtDateLong(today)}</p>
        <h1>{greeting()}</h1>
        <p>Alles van vandaag in één oogopslag. Tik op een kaart voor meer details.</p>
      </div>
      <div class="weather pill">
        <Icon icon={weatherIcon} size={18} />
        <span>Buiten {temp(live.weather.temp)}</span>
        {#if live.weather.cloud != null}<span class="muted">· {live.weather.cloud}% bewolkt</span>{/if}
      </div>
    </header>

    {#if app.isDemo && !hideDemo}
      <div class="notice rise" style="--i:1">
        <Icon icon={Info} size={18} />
        <p>Je kijkt naar <b>voorbeelddata</b>. Zodra de ESP32 draait, koppel je hem in <a href={href('instellingen')}>Instellingen</a>.</p>
        <button
          class="btn ghost"
          type="button"
          onclick={() => {
            hideDemo = true;
            try {
              sessionStorage.setItem('mw.hideDemo', '1');
            } catch {
              /* */
            }
          }}>Oké</button
        >
      </div>
    {/if}

    <section class="scene-card card rise" style="--i:1">
      <div class="card-head">
        <h2 class="card-title"><span class="ico" style="--accent: var(--g-house)"><Icon icon={Zap} size={18} /></span>Nu in huis</h2>
        <div class="head-right">
          <Explain label="Hoe lees ik dit?">
            <p>De <b>lichtpuntjes</b> laten zien waar de stroom op dit moment naartoe gaat. Hoe sneller ze bewegen, hoe meer stroom.</p>
            <p><b>Zon → huis:</b> de zonnepanelen leveren stroom. <b>Net:</b> stroom van of naar het elektriciteitsnet (teruglevering gaat van het huis naar het net). <b>Batterijen:</b> laden (stroom gaat erin) of ontladen (stroom gaat naar het huis); de ring om het rondje laat zien hoe vol ze zijn. </p>
            <p>Het getal bij het huis is wat het huis op dit moment opneemt. Laadt de batterij (ook vanaf het net), dan telt dat mee en zie je eronder de verdeling: apparaten + batterij laden. Alles ververst vanzelf elke paar minuten.</p>
          </Explain>
        </div>
      </div>
      <EnergyScene
        solarW={live.solar.w}
        gridW={live.grid.w}
        houseW={house}
        batteries={live.batteries}
        phases={[live.grid.l1, live.grid.l2, live.grid.l3]}
        night={nightNow}
      />
    </section>

    <section class="money card rise" style="--i:2">
      <div class="m-main">
        <div class="m-head">
          <p class="eyebrow"><Icon icon={PiggyBank} size={15} /> Vandaag bespaard</p>
          <Explain label="Wat is dit?">
            <p><b>Bespaard</b> is wat je zonder zonnepanelen en batterijen had betaald, min wat je nu echt betaalt voor stroom.</p>
            <p><b>Stroomkosten</b> is wat de stroom van het net vandaag kost (zonder vaste kosten). De tarieven kun je aanpassen bij Instellingen.</p>
          </Explain>
        </div>
        <div class="hero-fig"><AnimatedNumber value={mToday.saved} format={euro} duration={1400} /></div>
        <p class="split">
          <span><i style="background: var(--c-solar)"></i>door zon <b>{euro(mToday.savedSolar)}</b></span>
          <span><i style="background: var(--c-battery)"></i>door batterijen <b>{euro(mToday.savedBattery)}</b></span>
        </p>
        <div class="cost-row"><span>Stroomkosten vandaag</span><b>{euro(mToday.cost)}</b></div>
      </div>
      <div class="periods">
        <div><span>Deze week</span><b>{euro(mWeek.saved)}</b></div>
        <div><span>Deze maand</span><b>{euro(mMonth.saved)}</b></div>
        <div><span>Dit jaar</span><b>{euro(mYear.saved)}</b></div>
      </div>
      {#if tot}
        {@const src = sourceSplit(tot)}
        <div class="origin">
          <p class="origin-title">Waar kwam de stroom vandaag vandaan?</p>
          <SegBar
            label="Herkomst stroom vandaag"
            format={(v) => kwh(v)}
            parts={[
              { label: 'Zon', value: src.fromSolar, color: '--c-solar' },
              { label: 'Batterijen', value: src.fromBattery, color: '--c-battery' },
              { label: 'Net', value: src.fromGrid, color: '--c-grid' },
            ]}
          />
          <a class="more" href={href('energie')}>Alle energiecijfers <Icon icon={ChevronRight} size={16} /></a>
        </div>
      {/if}
    </section>

    {#if tot}
      <section class="tiles">
        <StatTile i={2} label="Opgewekt vandaag" value={tot.solarKwh} format={n1} unit="kWh" icon={Sun} accent="var(--g-solar)" delta={delta(tot.solarKwh, ySame?.solar)} href={href('energie')} />
        <StatTile i={3} label="Verbruikt vandaag" value={consumptionKwh(tot)} format={n1} unit="kWh" icon={Zap} accent="var(--g-house)" delta={delta(consumptionKwh(tot), ySame?.house)} deltaGoodWhenUp={false} href={href('energie')} />
        <StatTile i={4} label="Van het net" value={tot.importKwh} format={n1} unit="kWh" icon={UtilityPole} accent="var(--g-grid)" sub={`piek ${power(tot.peakImportW)}`} href={href('energie')} />
        <StatTile i={5} label="Teruggeleverd" value={tot.exportKwh} format={n1} unit="kWh" icon={ArrowUpFromLine} accent="var(--g-grid)" href={href('energie')} />
        <StatTile i={6} label="Zelfvoorzienend" value={selfSufficiency(tot) * 100} format={n0} unit="%" icon={Leaf} accent="var(--g-money)" sub="eigen stroom" href={href('energie')} />
        <StatTile i={7} label="Batterijen" value={avgSoc} format={n0} unit="%" icon={BatteryMedium} accent="var(--g-battery)" sub={`${n1(tot.batDischargeKwh)} kWh ontladen`} href={href('batterijen')} />
        <StatTile i={8} label="Airco's vandaag" value={Object.values(tot.climateKwh).reduce((a, b) => a + b, 0)} format={n1} unit="kWh" icon={Snowflake} accent="var(--g-cool)" sub="7 airco's samen" href={href('klimaat')} />
        <StatTile i={9} label="Gas (back-up)" value={tot.gasM3 ?? 0} format={n2} unit="m³" icon={Flame} accent="var(--g-heat)" sub={(tot.gasM3 ?? 0) > 0 ? 'vandaag gebruikt' : 'niet nodig vandaag'} href={href('energie')} />
      </section>
    {/if}

    {#if chart}
      <section class="card rise" style="--i:8">
        <div class="card-head">
          <h2 class="card-title"><span class="ico" style="--accent: var(--g-solar)"><Icon icon={Sun} size={18} /></span>Vermogen vandaag</h2>
          <a class="link" href={href('energie')}>Details <Icon icon={ChevronRight} size={16} /></a>
        </div>
        <Chart
          x={chart.x}
          series={chart.series}
          height={250}
          xRange={[dayStart(today), dayStart(today) + 86400]}
          yFormat={axis}
          valueFormat={(v) => power(v, false)}
          ariaLabel="Vermogen vandaag: verbruik, zon, batterijen en net"
        />
      </section>
    {/if}

    <div class="section-title"><h2>Klimaat</h2><a href={href('klimaat')}>Alle kamers →</a></div>
    <section class="rooms">
      {#each live.climate as c, i (c.id)}
        <a class="room card rise" style="--i:{9 + i}" href={href('klimaat')}>
          <div class="room-top">
            <span class="room-name">{app.nameOf(c.id)}</span>
            {#if c.mode === 'heat'}
              <span class="mode heat"><Icon icon={Flame} size={14} />Verwarmen</span>
            {:else if c.mode === 'cool'}
              <span class="mode cool"><Icon icon={Snowflake} size={14} />Koelen</span>
            {:else}
              <span class="mode off"><Icon icon={Power} size={14} />Uit</span>
            {/if}
          </div>
          <div class="room-temp">{temp(c.temp)}<span>C</span></div>
          <div class="room-foot muted">
            {#if c.on && c.target != null}→ {temp(c.target)} ingesteld{:else}&nbsp;{/if}
          </div>
        </a>
      {/each}
    </section>

    <section class="heaters">
      {#each live.heaters as h, i (h.id)}
        <div class="heater card rise" style="--i:{15 + i}; --accent: var(--g-heat)" class:on={h.state === 'on'}>
          <span class="h-ico"><Icon icon={Flame} size={20} /></span>
          <div class="h-body">
            <span class="h-name">{app.nameOf(h.id)}</span>
            <span class="h-state">
              {h.state === 'on' ? (h.kind === 'pellet' && h.level ? `Aan · stand ${h.level}` : 'Aan') : h.state === 'starting' ? 'Ontsteken' : h.state === 'cooling' ? 'Afkoelen' : h.state === 'unknown' ? 'Niet gekoppeld' : 'Uit'}
              {#if h.temp != null}<span class="muted"> · {temp(h.temp)}</span>{/if}
            </span>
          </div>
          {#if h.state === 'on'}<span class="flicker" aria-hidden="true"></span>{/if}
        </div>
      {/each}
    </section>

    <div class="section-title"><h2>Apparaten</h2><a href={href('apparaten')}>Historie →</a></div>
    <section class="appl">
      {#each live.appliances as a, i (a.id)}
        <a class="app card rise" style="--i:{18 + i}" href={href('apparaten')}>
          <span class="a-ico" class:run={a.state === 'running'}><Icon icon={a.kind === 'washer' ? WashingMachine : CookingPot} size={24} /></span>
          <div class="a-body">
            <span class="a-name">{app.nameOf(a.id)}</span>
            {#if a.state === 'running'}
              <span class="a-state">{a.program} · nog <b>{a.remainingMin} min</b></span>
              <div class="progress"><div style="width:{a.progress ?? 0}%"></div></div>
            {:else if a.state === 'finished'}
              <span class="a-state done">✓ Klaar · {a.program}</span>
            {:else}
              <span class="a-state muted">Niet actief</span>
            {/if}
          </div>
        </a>
      {/each}
    </section>
  </div>
{:else if app.error}
  <div class="card"><p>{app.error}</p></div>
{/if}

<style>
  .weather {
    height: 38px;
    font-size: 14px;
    gap: 8px;
  }
  .notice {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 10px 10px 10px 16px;
    border-radius: 16px;
    background: color-mix(in srgb, var(--s-warn) 10%, var(--surface));
    border: 1px solid color-mix(in srgb, var(--s-warn) 30%, transparent);
    color: var(--text-2);
    font-size: 14px;
  }
  .notice :global(svg) {
    color: var(--s-warn);
    flex: none;
  }
  .notice p {
    flex: 1;
  }
  .notice a {
    color: var(--text);
    text-decoration: underline;
  }

  .scene-card {
    padding: 16px 16px 16px;
  }
  .scene-card .card-head {
    padding: 2px 4px 0;
  }

  .money {
    display: grid;
    gap: 18px;
    overflow: hidden;
    background:
      radial-gradient(60% 90% at 0% 0%, color-mix(in srgb, var(--g-money) 10%, transparent), transparent 70%),
      color-mix(in srgb, var(--surface) 88%, transparent);
  }
  .head-right {
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .scene-card .card-head :global(.ex-box),
  .m-head :global(.ex-box) {
    order: 10;
  }
  .m-head {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    justify-content: space-between;
    gap: 8px;
  }
  .m-main {
    display: grid;
    gap: 8px;
    align-content: start;
  }
  .money .eyebrow {
    display: flex;
    align-items: center;
    gap: 6px;
    color: var(--g-money);
  }
  .hero-fig {
    font-size: clamp(48px, 7vw, 68px);
    font-weight: 700;
    letter-spacing: -0.045em;
    line-height: 1;
  }
  .split {
    display: flex;
    flex-wrap: wrap;
    gap: 4px 16px;
    font-size: 14px;
    color: var(--text-2);
  }
  .split span {
    display: inline-flex;
    align-items: center;
    gap: 7px;
  }
  .split i {
    width: 10px;
    height: 10px;
    border-radius: 3px;
  }
  .split b {
    color: var(--text);
    font-weight: 600;
  }
  .periods {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
    margin-top: 8px;
  }
  .periods div {
    display: grid;
    gap: 2px;
    padding: 12px;
    border-radius: 14px;
    background: var(--surface-2);
    border: 1px solid var(--border);
  }
  .periods span {
    font-size: 12.5px;
    color: var(--text-3);
    font-weight: 550;
  }
  .periods b {
    font-size: 18px;
    font-weight: 650;
    letter-spacing: -0.02em;
  }
  .cost-row {
    display: flex;
    justify-content: space-between;
    font-size: 14px;
    color: var(--text-2);
    padding: 6px 2px 0;
  }
  .cost-row b {
    color: var(--text);
  }
  .origin {
    margin-top: 8px;
    padding-top: 14px;
    border-top: 1px solid var(--border);
  }
  .origin-title {
    font-size: 13.5px;
    font-weight: 600;
    color: var(--text-2);
    margin-bottom: 10px;
  }
  .more,
  .link {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    font-size: 14px;
    font-weight: 600;
    color: var(--text-2);
  }
  .more {
    margin-top: auto;
    padding-top: 6px;
  }
  .more:hover,
  .link:hover {
    color: var(--text);
  }

  .tiles {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
  }

  .rooms {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
  }
  .room {
    padding: 16px;
    display: grid;
    gap: 6px;
    transition: transform 0.35s var(--ease), border-color 0.3s;
  }
  .room:hover {
    transform: translateY(-2px);
    border-color: var(--border-strong);
  }
  .room-top {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 6px;
  }
  .room-name {
    font-weight: 600;
    font-size: 14.5px;
  }
  .room-temp {
    font-size: 32px;
    font-weight: 650;
    letter-spacing: -0.03em;
    line-height: 1.1;
  }
  .room-temp span {
    font-size: 16px;
    color: var(--text-3);
    margin-left: 1px;
  }
  .room-foot {
    font-size: 13px;
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

  .heaters {
    display: grid;
    gap: 12px;
  }
  .heater {
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 14px 16px;
    overflow: hidden;
  }
  .h-ico {
    display: grid;
    place-items: center;
    width: 42px;
    height: 42px;
    border-radius: 13px;
    background: var(--surface-2);
    color: var(--text-3);
    flex: none;
  }
  .heater.on .h-ico {
    color: var(--g-heat);
    background: color-mix(in srgb, var(--g-heat) 15%, transparent);
  }
  .h-body {
    display: grid;
    min-width: 0;
  }
  .h-name {
    font-weight: 600;
    font-size: 14.5px;
  }
  .h-state {
    font-size: 13.5px;
    color: var(--text-2);
  }
  .flicker {
    position: absolute;
    right: -30px;
    bottom: -40px;
    width: 140px;
    height: 140px;
    border-radius: 50%;
    background: radial-gradient(closest-side, color-mix(in srgb, var(--g-heat) 28%, transparent), transparent);
    animation: flick 2.8s ease-in-out infinite alternate;
    pointer-events: none;
  }
  @keyframes flick {
    50% {
      opacity: 0.6;
      transform: scale(0.94);
    }
    to {
      opacity: 1;
      transform: scale(1.06);
    }
  }

  .appl {
    display: grid;
    gap: 12px;
  }
  .app {
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 16px 18px;
  }
  .a-ico {
    display: grid;
    place-items: center;
    width: 50px;
    height: 50px;
    border-radius: 15px;
    background: var(--surface-2);
    color: var(--text-3);
    flex: none;
  }
  .a-ico.run {
    color: var(--g-appl);
    background: color-mix(in srgb, var(--g-appl) 15%, transparent);
  }
  .a-ico.run :global(svg) {
    animation: spin 3s linear infinite;
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
  .a-body {
    display: grid;
    gap: 4px;
    flex: 1;
    min-width: 0;
  }
  .a-name {
    font-weight: 600;
  }
  .a-state {
    font-size: 14px;
    color: var(--text-2);
  }
  .a-state.done {
    color: var(--s-good-text);
    font-weight: 600;
  }
  .progress {
    height: 6px;
    border-radius: 999px;
    background: color-mix(in srgb, var(--g-appl) 14%, transparent);
    overflow: hidden;
  }
  .progress div {
    height: 100%;
    border-radius: 999px;
    background: var(--g-appl);
    transition: width 1s var(--ease);
  }

  @media (min-width: 720px) {
    .tiles {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
    .rooms {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
    .heaters {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
    .appl {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
  }
  @media (min-width: 1100px) {
    .scene-card {
      padding: 20px 22px 22px;
    }
    .money {
      grid-template-columns: 1.1fr 1fr 1.3fr;
      gap: 32px;
      padding: 24px 28px;
      align-items: start;
    }
    .periods {
      grid-template-columns: 1fr;
      margin-top: 0;
    }
    .origin {
      margin-top: 0;
      padding-top: 0;
      border-top: 0;
    }
    .tiles {
      grid-template-columns: repeat(4, minmax(0, 1fr));
    }
    .rooms {
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    }
  }
</style>
