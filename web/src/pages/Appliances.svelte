<script lang="ts">
  import { WashingMachine, CookingPot, CalendarDays, History, Info } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import { app } from '../lib/data/app.svelte';
  import { addDays, fmtDateShortWd, fmtMinutes, parseDate, rangeDays, todayIso, weekStart, monthStart } from '../lib/date';
  import { n0 } from '../lib/format';
  import { DEVICES, type ApplianceRun } from '../lib/types';

  const today = todayIso();
  const live = $derived(app.live!);

  // 5 volle weken, eindigend met de huidige week
  const calStart = addDays(weekStart(today), -28);
  const calDays = rangeDays(calStart, addDays(calStart, 34));

  const runsByDay = $derived(
    Object.fromEntries(calDays.map((d) => [d, app.totalsOn(d)?.runs ?? []])) as Record<string, ApplianceRun[]>,
  );

  const history = $derived.by(() => {
    const out: (ApplianceRun & { date: string })[] = [];
    for (let i = 0; i < 60 && out.length < 25; i++) {
      const d = addDays(today, -i);
      const runs = app.totalsOn(d)?.runs ?? [];
      for (const r of [...runs].reverse()) out.push({ ...r, date: d });
    }
    return out.slice(0, 25);
  });

  const monthStats = $derived.by(() => {
    const ds = rangeDays(monthStart(today), today);
    return DEVICES.appliances.map((a) => {
      const runs = ds.flatMap((d) => (app.totalsOn(d)?.runs ?? []).filter((r) => r.id === a.id));
      const avg = runs.length ? runs.reduce((s, r) => s + (r.end - r.start), 0) / runs.length : 0;
      const progs: Record<string, number> = {};
      runs.forEach((r) => r.program && (progs[r.program] = (progs[r.program] ?? 0) + 1));
      const fav = Object.entries(progs).sort((x, y) => y[1] - x[1])[0]?.[0];
      return { ...a, count: runs.length, avg, fav };
    });
  });

  const nameOf = (id: string) => app.nameOf(id);
  const endClock = (min: number) => {
    const d = new Date(Date.now() + min * 60000);
    return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
  };
</script>

<div class="page">
  <header class="page-head rise">
    <div>
      <p class="eyebrow">Apparaten</p>
      <h1>Wassen & afwassen</h1>
      <p>Zie of de wasmachine of vaatwasser draait, hoe lang nog, en wanneer ze gedraaid hebben.</p>
    </div>
  </header>

  <section class="live">
    {#each live.appliances as a, i (a.id)}
      {@const st = monthStats.find((s) => s.id === a.id)}
      <div class="app card rise" style="--i:{i + 1}" class:run={a.state === 'running'}>
        <div class="ring" style="--p:{a.state === 'running' ? (a.progress ?? 0) : a.state === 'finished' ? 100 : 0}">
          <Icon icon={a.kind === 'washer' ? WashingMachine : CookingPot} size={34} stroke={1.8} />
        </div>
        <div class="body">
          <h2>{app.nameOf(a.id)}</h2>
          {#if a.state === 'running'}
            <p class="big">Nog {a.remainingMin} min</p>
            <p class="sub">{a.program} · klaar rond {endClock(a.remainingMin ?? 0)}</p>
          {:else if a.state === 'finished'}
            <p class="big done">✓ Klaar</p>
            <p class="sub">{a.program}</p>
          {:else}
            <p class="big idle">Niet actief</p>
            <p class="sub">&nbsp;</p>
          {/if}
          {#if st}
            <div class="stats">
              <div><b>{st.count}×</b><span>deze maand</span></div>
              <div><b>{st.avg ? fmtMinutes(st.avg).replace(':', 'u') : '—'}</b><span>gem. duur</span></div>
              <div><b>{st.fav ?? '—'}</b><span>meest gebruikt</span></div>
            </div>
          {/if}
        </div>
      </div>
    {/each}
  </section>

  <section class="card rise" style="--i:3">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-appl)"><Icon icon={CalendarDays} size={18} /></span>Laatste 5 weken</h2>
      <div class="cal-legend">
        <span><i class="w"></i>Wasmachine</span>
        <span><i class="d"></i>Vaatwasser</span>
      </div>
    </div>
    <div class="cal" role="grid" aria-label="Kalender met was- en vaatwasbeurten">
      {#each ['ma', 'di', 'wo', 'do', 'vr', 'za', 'zo'] as wd (wd)}<span class="wd">{wd}</span>{/each}
      {#each calDays as d (d)}
        {@const runs = runsByDay[d] ?? []}
        <div
          class="day"
          class:future={d > today}
          class:today={d === today}
          role="gridcell"
          aria-label="{fmtDateShortWd(d)}: {runs.length ? runs.map((r) => nameOf(r.id)).join(', ') : 'niets'}"
        >
          <span class="num">{parseDate(d).getDate()}</span>
          <span class="marks">
            {#each runs as r, j (j)}<i class={r.id === 'wasmachine' ? 'w' : 'd'}></i>{/each}
          </span>
        </div>
      {/each}
    </div>
  </section>

  <section class="card rise" style="--i:4">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-appl)"><Icon icon={History} size={18} /></span>Historie</h2>
    </div>
    <div class="table-wrap">
      <table class="data">
        <thead><tr><th>Apparaat</th><th>Dag</th><th>Tijd</th><th>Duur</th><th>Programma</th></tr></thead>
        <tbody>
          {#each history as r, i (r.date + r.id + i)}
            <tr>
              <td><span class="tag" class:w={r.id === 'wasmachine'}></span>{nameOf(r.id)}</td>
              <td>{fmtDateShortWd(r.date)}</td>
              <td>{fmtMinutes(r.start)}–{fmtMinutes(r.end)}</td>
              <td>{n0(r.end - r.start)} min</td>
              <td>{r.program ?? '—'}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    </div>
    <p class="note muted"><Icon icon={Info} size={14} /> Stroomverbruik per wasbeurt geeft Home Connect niet door. Wil je dat ook zien, dan kan er een slimme stekker (bijv. HomeWizard Energy Socket) tussen.</p>
  </section>
</div>

<style>
  .live {
    display: grid;
    gap: 16px;
  }
  .app {
    display: flex;
    gap: 22px;
    align-items: center;
    padding: 22px;
  }
  .ring {
    --size: 104px;
    width: var(--size);
    height: var(--size);
    flex: none;
    border-radius: 50%;
    display: grid;
    place-items: center;
    color: var(--text-3);
    background:
      radial-gradient(circle, var(--surface-2) 63%, transparent 64%),
      conic-gradient(from -90deg, var(--g-appl) calc(var(--p) * 1%), color-mix(in srgb, var(--g-appl) 10%, transparent) 0);
  }
  .app.run .ring {
    color: var(--g-appl);
    box-shadow: 0 0 46px -12px color-mix(in srgb, var(--g-appl) 70%, transparent);
  }
  .app.run .ring :global(svg) {
    animation: wobble 2.4s ease-in-out infinite;
  }
  @keyframes wobble {
    25% {
      transform: rotate(-6deg);
    }
    75% {
      transform: rotate(6deg);
    }
  }
  .body {
    display: grid;
    gap: 2px;
    min-width: 0;
    flex: 1;
  }
  .body h2 {
    font-size: 17px;
    color: var(--text-2);
    font-weight: 600;
  }
  .big {
    font-size: 28px;
    font-weight: 700;
    letter-spacing: -0.03em;
  }
  .big.done {
    color: var(--s-good-text);
  }
  .big.idle {
    color: var(--text-3);
    font-weight: 600;
  }
  .sub {
    color: var(--text-2);
    font-size: 14px;
  }
  .stats {
    display: grid;
    grid-template-columns: repeat(3, auto);
    justify-content: start;
    gap: 18px;
    margin-top: 12px;
  }
  .stats div {
    display: grid;
  }
  .stats b {
    font-size: 15px;
  }
  .stats span {
    font-size: 12px;
    color: var(--text-3);
  }
  .cal-legend {
    display: flex;
    gap: 14px;
    font-size: 13px;
    color: var(--text-2);
  }
  .cal-legend span {
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }
  i.w,
  i.d {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    display: inline-block;
  }
  i.w {
    background: var(--c-grid);
  }
  i.d {
    background: var(--c-house);
  }
  .cal {
    display: grid;
    grid-template-columns: repeat(7, minmax(0, 1fr));
    gap: 6px;
  }
  .wd {
    font-size: 12px;
    color: var(--text-3);
    text-align: center;
    font-weight: 600;
    padding-bottom: 2px;
  }
  .day {
    aspect-ratio: 1.15;
    border-radius: 12px;
    background: var(--surface-2);
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    padding: 6px 8px;
    min-width: 0;
  }
  .day.future {
    opacity: 0.35;
  }
  .day.today {
    box-shadow: inset 0 0 0 2px var(--g-house);
  }
  .num {
    font-size: 12.5px;
    color: var(--text-2);
    font-weight: 600;
  }
  .marks {
    display: flex;
    gap: 4px;
    flex-wrap: wrap;
  }
  .tag {
    display: inline-block;
    width: 9px;
    height: 9px;
    border-radius: 50%;
    background: var(--c-house);
    margin-right: 8px;
  }
  .tag.w {
    background: var(--c-grid);
  }
  .note {
    display: flex;
    gap: 8px;
    align-items: flex-start;
    font-size: 13px;
    margin-top: 14px;
  }
  .note :global(svg) {
    flex: none;
    margin-top: 3px;
  }
  @media (min-width: 900px) {
    .live {
      grid-template-columns: 1fr 1fr;
    }
    .day {
      aspect-ratio: 1.6;
    }
  }
  @media (max-width: 460px) {
    .app {
      gap: 16px;
      padding: 18px;
    }
    .ring {
      --size: 84px;
    }
    .stats {
      gap: 12px;
    }
  }
</style>
