<script lang="ts" module>
  export interface BarSeries {
    label: string;
    values: (number | null)[];
    color: string; // CSS-variabele
  }
</script>

<script lang="ts">
  // Staafgrafiek. Op smalle schermen met veel staven: horizontaal swipen (begint bij vandaag).
  // Aanraken: 1e tik = waarde tonen, 2e tik = openen. Muis: hover = waarde, klik = openen.
  import { onMount, tick } from 'svelte';

  let {
    labels,
    titles,
    series,
    height = 250,
    yFormat = (v: number) => String(v),
    valueFormat,
    highlight = -1,
    onselect,
    ariaLabel,
  }: {
    labels: string[];
    titles?: string[];
    series: BarSeries[];
    height?: number;
    yFormat?: (v: number) => string;
    valueFormat?: (v: number) => string;
    highlight?: number;
    onselect?: (i: number) => void;
    ariaLabel: string;
  } = $props();

  let width = $state(600);
  let hover = $state(-1);
  let hidden = $state<Record<string, boolean>>({});
  let scroller: HTMLDivElement;
  let pointerType = 'mouse';

  const AXIS_W = 46;
  const PAD_B = 28;
  const PAD_T = 12;
  const visible = $derived(series.filter((s) => !hidden[s.label]));
  const n = $derived(labels.length);
  const k = $derived(Math.max(1, visible.length));
  // Minimale breedte per groep: genoeg voor duimvriendelijke staven
  const minGroup = $derived(k > 1 ? 40 : 28);
  const avail = $derived(Math.max(50, width - AXIS_W));
  const plotW = $derived(Math.max(avail, n * minGroup));
  const scrolls = $derived(plotW > avail + 1);
  const plotH = $derived(height - PAD_B - PAD_T);

  function niceStep(range: number, target: number) {
    const raw = range / Math.max(1, target);
    const mag = Math.pow(10, Math.floor(Math.log10(raw || 1)));
    const f = raw / mag;
    return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 2.5 ? 2.5 : f <= 5 ? 5 : 10) * mag;
  }

  const scale = $derived.by(() => {
    let lo = 0;
    let hi = 0;
    for (const s of visible) for (const v of s.values) if (v != null) ((lo = Math.min(lo, v)), (hi = Math.max(hi, v)));
    if (hi === lo) hi = lo + 1;
    const step = niceStep(hi - lo, Math.max(2, Math.floor(plotH / 48)));
    const min = Math.floor(lo / step) * step;
    const max = Math.ceil(hi / step) * step;
    const ticks: number[] = [];
    for (let t = min; t <= max + step / 2; t += step) ticks.push(Math.round(t * 1e6) / 1e6);
    return { min, max, ticks };
  });

  const y = (v: number) => PAD_T + plotH - ((v - scale.min) / (scale.max - scale.min)) * plotH;
  const groupW = $derived(plotW / Math.max(1, n));
  const GAP = 2;
  const barW = $derived(Math.max(2, Math.min(24, (groupW * 0.72 - GAP * (k - 1)) / k)));
  const labelEvery = $derived(Math.max(1, Math.ceil(n / Math.max(1, Math.floor(plotW / 40)))));

  function barPath(x: number, v: number) {
    const y0 = y(0);
    const y1 = y(v);
    const h = Math.abs(y1 - y0);
    if (h < 0.5) return '';
    const r = Math.min(4, barW / 2, h);
    if (v >= 0)
      return `M${x},${y0} V${y1 + r} Q${x},${y1} ${x + r},${y1} H${x + barW - r} Q${x + barW},${y1} ${x + barW},${y1 + r} V${y0} Z`;
    return `M${x},${y0} V${y1 - r} Q${x},${y1} ${x + r},${y1} H${x + barW - r} Q${x + barW},${y1} ${x + barW},${y1 - r} V${y0} Z`;
  }

  const fmtV = (v: number | null) => (v == null ? '—' : (valueFormat ?? yFormat)(v));

  function onTap(i: number) {
    if (pointerType !== 'mouse' && hover !== i) {
      hover = i;
      return;
    }
    onselect?.(i);
  }
  function onKey(e: KeyboardEvent, i: number) {
    if ((e.key === 'Enter' || e.key === ' ') && onselect) {
      e.preventDefault();
      onselect(i);
    }
  }

  // Bij scrollen: start rechts (bij vandaag / gemarkeerde staaf)
  async function scrollToFocus() {
    await tick();
    if (!scroller || !scrolls) return;
    const idx = highlight >= 0 ? highlight : n - 1;
    scroller.scrollLeft = Math.max(0, (idx + 1) * groupW - scroller.clientWidth + groupW * 1.5);
  }
  onMount(scrollToFocus);
  $effect(() => {
    void n;
    void highlight;
    void scrolls;
    scrollToFocus();
  });
</script>

<figure class="bars" aria-label={ariaLabel}>
  {#if series.length > 1}
    <div class="legend" role="group" aria-label="Legenda">
      {#each series as s (s.label)}
        <button type="button" class:off={hidden[s.label]} aria-pressed={!hidden[s.label]} onclick={() => (hidden = { ...hidden, [s.label]: !hidden[s.label] })}>
          <i style="background: var({s.color})"></i>{s.label}
        </button>
      {/each}
    </div>
  {/if}

  <div class="wrap" bind:clientWidth={width} style="height:{height}px">
    <!-- vaste y-as -->
    <svg class="axis" width={AXIS_W} {height} aria-hidden="true">
      {#each scale.ticks as t (t)}
        <text x={AXIS_W - 10} y={y(t) + 4} class="yl">{yFormat(t)}</text>
      {/each}
    </svg>

    <div class="scroller" class:scrolls bind:this={scroller} onpointerdown={(e) => (pointerType = e.pointerType)}>
      <div class="content" style="width:{plotW}px; height:{height}px">
        <svg width={plotW} {height} role="presentation" onmouseleave={() => pointerType === 'mouse' && (hover = -1)}>
          {#each scale.ticks as t (t)}
            <line x1="0" x2={plotW} y1={Math.round(y(t)) + 0.5} y2={Math.round(y(t)) + 0.5} class={t === 0 ? 'base' : 'gl'} />
          {/each}

          {#each labels as lab, i (i)}
            {@const gx = i * groupW}
            {@const total = barW * k + GAP * (k - 1)}
            {#if hover === i || highlight === i}
              <rect x={gx + 1} y={PAD_T} width={groupW - 2} height={plotH} rx="8" class="band" class:hl={highlight === i && hover !== i} />
            {/if}
            {#each visible as s, j (s.label)}
              {@const v = s.values[i]}
              {#if v != null}
                <path d={barPath(gx + (groupW - total) / 2 + j * (barW + GAP), v)} style="fill: var({s.color})" />
              {/if}
            {/each}
            {#if i % labelEvery === 0}
              <text x={gx + groupW / 2} y={height - 8} class="xl" class:strong={highlight === i}>{lab}</text>
            {/if}
            <!-- svelte-ignore a11y_no_noninteractive_tabindex -->
            <rect
              x={gx}
              y={PAD_T}
              width={groupW}
              height={plotH}
              class="hit"
              class:clickable={!!onselect}
              role={onselect ? 'button' : 'img'}
              tabindex={onselect ? 0 : undefined}
              aria-label={`${titles?.[i] ?? lab}: ${visible.map((s) => `${s.label} ${fmtV(s.values[i])}`).join(', ')}`}
              onmouseenter={() => pointerType === 'mouse' && (hover = i)}
              onfocus={() => (hover = i)}
              onblur={() => (hover = -1)}
              onclick={() => onTap(i)}
              onkeydown={(e) => onKey(e, i)}
            />
          {/each}
        </svg>

        {#if hover >= 0}
          {@const gx = hover * groupW + groupW / 2}
          <div class="tip" style="left:{Math.min(Math.max(gx, 95), plotW - 95)}px">
            <div class="t">{titles?.[hover] ?? labels[hover]}</div>
            {#each visible as s (s.label)}
              <div class="r"><i style="background: var({s.color})"></i><span>{s.label}</span><b>{fmtV(s.values[hover])}</b></div>
            {/each}
            {#if onselect}<div class="hint">{pointerType === 'mouse' ? 'Klik om te openen' : 'Tik nogmaals om te openen'}</div>{/if}
          </div>
        {/if}
      </div>
    </div>
  </div>
  {#if scrolls}<p class="swipe">← Swipe voor eerdere dagen</p>{/if}
</figure>

<style>
  .bars {
    margin: 0;
    min-width: 0;
  }
  .wrap {
    position: relative;
    display: flex;
    width: 100%;
  }
  .axis {
    flex: none;
    display: block;
    overflow: visible;
  }
  .scroller {
    flex: 1;
    min-width: 0;
    overflow-x: auto;
    overflow-y: visible;
    scrollbar-width: none;
    -webkit-overflow-scrolling: touch;
  }
  .scroller::-webkit-scrollbar {
    display: none;
  }
  .scroller.scrolls {
    mask: linear-gradient(90deg, transparent, #000 18px, #000 calc(100% - 6px), transparent);
    -webkit-mask: linear-gradient(90deg, transparent, #000 18px, #000 calc(100% - 6px), transparent);
  }
  .content {
    position: relative;
  }
  .content svg {
    display: block;
    overflow: visible;
  }
  .gl {
    stroke: var(--gridline);
    stroke-width: 1;
  }
  .base {
    stroke: var(--axis);
    stroke-width: 1;
  }
  .yl {
    fill: var(--text-3);
    font-size: 12px;
    text-anchor: end;
    font-variant-numeric: tabular-nums;
  }
  .xl {
    fill: var(--text-3);
    font-size: 12px;
    text-anchor: middle;
  }
  .xl.strong {
    fill: var(--text);
    font-weight: 700;
  }
  .band {
    fill: color-mix(in srgb, var(--text) 7%, transparent);
  }
  .band.hl {
    fill: color-mix(in srgb, var(--text) 4%, transparent);
  }
  .hit {
    fill: transparent;
    outline: none;
    -webkit-tap-highlight-color: transparent;
  }
  .hit.clickable {
    cursor: pointer;
  }
  .hit:focus-visible {
    stroke: var(--g-house);
    stroke-width: 2;
  }
  .legend {
    display: flex;
    flex-wrap: wrap;
    gap: 2px 4px;
    margin: 0 0 10px -8px;
  }
  .legend button {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    border: 0;
    background: transparent;
    padding: 6px 8px;
    border-radius: 10px;
    font-size: 14px;
    font-weight: 550;
    color: var(--text-2);
    cursor: pointer;
  }
  .legend button:hover {
    background: var(--surface-2);
  }
  .legend button.off {
    opacity: 0.35;
  }
  .legend i {
    width: 10px;
    height: 10px;
    border-radius: 3px;
  }
  .tip {
    position: absolute;
    top: 0;
    transform: translate(-50%, -4px);
    pointer-events: none;
    background: color-mix(in srgb, var(--surface-2) 94%, transparent);
    border: 0.5px solid var(--border-strong);
    border-radius: 14px;
    padding: 10px 12px;
    min-width: 175px;
    box-shadow: 0 12px 30px -10px rgba(0, 0, 0, 0.45);
    font-size: 13.5px;
    z-index: 3;
    backdrop-filter: blur(16px);
    -webkit-backdrop-filter: blur(16px);
  }
  .t {
    font-weight: 700;
    margin-bottom: 6px;
  }
  .r {
    display: flex;
    align-items: center;
    gap: 8px;
    color: var(--text-2);
    line-height: 1.7;
  }
  .r i {
    width: 10px;
    height: 10px;
    border-radius: 3px;
    flex: none;
  }
  .r b {
    margin-left: auto;
    padding-left: 14px;
    color: var(--text);
    font-variant-numeric: tabular-nums;
  }
  .hint {
    margin-top: 6px;
    color: var(--text-3);
    font-size: 12px;
  }
  .swipe {
    margin-top: 4px;
    font-size: 12.5px;
    color: var(--text-3);
    text-align: center;
  }
</style>
