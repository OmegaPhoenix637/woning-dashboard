<script lang="ts" module>
  export type Period = 'dag' | 'week' | 'maand' | 'jaar';

  export function periodAnchor(p: Period, d: string): string {
    if (p === 'week') return weekStart(d);
    if (p === 'maand') return monthStart(d);
    if (p === 'jaar') return d.slice(0, 4) + '-01-01';
    return d;
  }

  import { addDays, addMonths, fmtDateLong, fmtDateShort, fmtMonthName, isoWeek, monthStart, todayIso, weekStart } from '../date';
</script>

<script lang="ts">
  import { ChevronLeft, ChevronRight, CalendarDays } from 'lucide';
  import Icon from './Icon.svelte';

  let {
    period = $bindable('dag'),
    date = $bindable(todayIso()),
    min,
    periods = ['dag', 'week', 'maand', 'jaar'],
  }: { period?: Period; date?: string; min?: string; periods?: Period[] } = $props();

  const today = todayIso();

  function shift(dir: -1 | 1) {
    const a = periodAnchor(period, date);
    let next =
      period === 'dag'
        ? addDays(a, dir)
        : period === 'week'
          ? addDays(a, 7 * dir)
          : period === 'maand'
            ? addMonths(a, dir)
            : `${Number(a.slice(0, 4)) + dir}-01-01`;
    if (next > today) next = today;
    date = next;
  }

  const label = $derived.by(() => {
    const a = periodAnchor(period, date);
    if (period === 'dag')
      return a === today ? 'Vandaag' : a === addDays(today, -1) ? 'Gisteren' : fmtDateLong(a, a.slice(0, 4) !== today.slice(0, 4));
    if (period === 'week') return `Week ${isoWeek(a)} · ${fmtDateShort(a)} – ${fmtDateShort(addDays(a, 6))}`;
    if (period === 'maand') return fmtMonthName(a);
    return a.slice(0, 4);
  });

  const canNext = $derived(periodAnchor(period, today) > periodAnchor(period, date));
  const canPrev = $derived(!min || periodAnchor(period, date) > periodAnchor(period, min));
  const isNow = $derived(periodAnchor(period, date) === periodAnchor(period, today));
</script>

<div class="pp">
  {#if periods.length > 1}
    <div class="seg" role="group" aria-label="Periode">
      {#each periods as p (p)}
        <button type="button" aria-pressed={period === p} onclick={() => (period = p)}>{p[0].toUpperCase() + p.slice(1)}</button>
      {/each}
    </div>
  {:else}
    <span class="single">Kies een dag</span>
  {/if}
  <div class="nav">
    <button type="button" class="btn icon ghost" onclick={() => shift(-1)} disabled={!canPrev} aria-label="Vorige periode">
      <Icon icon={ChevronLeft} />
    </button>
    <label class="date">
      <Icon icon={CalendarDays} size={17} />
      <span class="lbl">{label}</span>
      <input
        type="date"
        value={date}
        max={today}
        {min}
        onchange={(e) => (date = (e.target as HTMLInputElement).value || date)}
        aria-label="Kies datum"
      />
    </label>
    <button type="button" class="btn icon ghost" onclick={() => shift(1)} disabled={!canNext} aria-label="Volgende periode">
      <Icon icon={ChevronRight} />
    </button>
    {#if !isNow}
      <button type="button" class="btn" onclick={() => (date = today)}>Nu</button>
    {/if}
  </div>
</div>

<style>
  .pp {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  .single {
    padding-left: 8px;
    font-weight: 600;
    color: var(--text-2);
  }
  .nav {
    display: flex;
    align-items: center;
    gap: 4px;
    min-width: 0;
    max-width: 100%;
  }
  .date {
    position: relative;
    display: inline-flex;
    align-items: center;
    gap: 8px;
    height: 42px;
    padding: 0 14px;
    border-radius: 999px;
    background: var(--surface-2);
    border: 1px solid var(--border);
    font-weight: 600;
    font-size: 15px;
    cursor: pointer;
    min-width: 0;
  }
  .lbl {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .lbl::first-letter {
    text-transform: uppercase;
  }
  .date input {
    position: absolute;
    inset: 0;
    opacity: 0;
    cursor: pointer;
    width: 100%;
  }
  .date input::-webkit-calendar-picker-indicator {
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
    cursor: pointer;
  }
</style>
