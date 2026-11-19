<script lang="ts" module>
  export interface ChartSeries {
    label: string;
    values: (number | null)[];
    /** CSS-variabele, bv. '--c-house' */
    color: string;
    fill?: boolean;
    dash?: number[];
    width?: number;
    step?: boolean;
  }
</script>

<script lang="ts">
  // Lijngrafiek (uPlot) met:
  //  - telefoon: 1 vinger schuiven = waarden aflezen, 2 vingers knijpen = zoomen,
  //    ingezoomd 1 vinger slepen = verschuiven, dubbel tikken = alles tonen
  //  - pc: slepen = inzoomen op stuk, dubbelklik = alles tonen
  //  - knoppen − / + / herstel / vergroten (schermvullend, telefoon draaien = breed)
  import uPlot from 'uplot';
  import 'uplot/dist/uPlot.min.css';
  import { onMount } from 'svelte';
  import { ZoomIn, ZoomOut, RotateCcw, Maximize2, X, Smartphone } from 'lucide';
  import Icon from './Icon.svelte';
  import { fmtClock } from '../date';

  let {
    x,
    series,
    height = 260,
    yFormat = (v: number) => String(v),
    valueFormat,
    xMode = 'time',
    xRange,
    xFormat,
    yMin,
    yMax,
    legend = true,
    title,
    ariaLabel,
  }: {
    x: number[];
    series: ChartSeries[];
    height?: number;
    yFormat?: (v: number) => string;
    valueFormat?: (v: number, seriesIdx: number) => string;
    xMode?: 'time' | 'num';
    xRange?: [number, number];
    xFormat?: (x: number) => string;
    yMin?: number;
    yMax?: number;
    legend?: boolean;
    title?: string;
    ariaLabel: string;
  } = $props();

  let wrap: HTMLDivElement;
  let plotEl: HTMLDivElement;
  let tip: HTMLDivElement;
  let u: uPlot | null = null;
  let hidden = $state<Record<string, boolean>>({});
  let zoom = $state<[number, number] | null>(null);
  let full = $state(false);
  let vw = $state(typeof window !== 'undefined' ? window.innerWidth : 1000);
  let vh = $state(typeof window !== 'undefined' ? window.innerHeight : 800);
  const coarse = typeof window !== 'undefined' && matchMedia('(pointer: coarse)').matches;

  const landscapePhone = $derived(vw > vh && vh <= 540);
  const h = $derived(full ? Math.max(200, vh - (landscapePhone ? 96 : 190)) : landscapePhone ? Math.max(height, vh - 130) : height);

  const css = (name: string) => getComputedStyle(document.documentElement).getPropertyValue(name).trim() || '#888';
  const fmtX = (v: number) => (xFormat ? xFormat(v) : xMode === 'time' ? fmtClock(v) : String(v));
  const fmtV = (v: number, i: number) => (valueFormat ? valueFormat(v, i) : yFormat(v));

  const fullRange = $derived<[number, number]>(xRange ?? [x[0] ?? 0, x[x.length - 1] ?? 1]);
  const minSpan = $derived((fullRange[1] - fullRange[0]) / 48);

  function withAlpha(hex: string, a: number) {
    const hh = hex.replace('#', '');
    const n = parseInt(hh.length === 3 ? hh.replace(/./g, (c) => c + c) : hh, 16);
    return `rgba(${(n >> 16) & 255},${(n >> 8) & 255},${n & 255},${a})`;
  }

  function setZoom(min: number, max: number) {
    const [f0, f1] = fullRange;
    let span = Math.max(minSpan, Math.min(f1 - f0, max - min));
    let a = Math.max(f0, Math.min(min, f1 - span));
    if (span >= f1 - f0 - 1e-9) {
      zoom = null;
      u?.setScale('x', { min: f0, max: f1 });
      return;
    }
    zoom = [a, a + span];
    u?.setScale('x', { min: a, max: a + span });
  }
  function resetZoom() {
    setZoom(fullRange[0], fullRange[1]);
  }
  function zoomBy(f: number) {
    const [a, b] = zoom ?? fullRange;
    const c = (a + b) / 2;
    const span = (b - a) * f;
    setZoom(c - span / 2, c + span / 2);
  }

  let builtWith: unknown[] = [];

  function build() {
    if (!plotEl) return;
    builtWith = [x, series, hidden];
    u?.destroy();
    const text3 = css('--text-3');
    const grid = css('--gridline');
    const axis = css('--axis');
    const font = `12px ${css('--font')}`;

    const opts: uPlot.Options = {
      width: Math.max(200, wrap.clientWidth),
      height: h,
      padding: [10, 6, 0, 0],
      legend: { show: false },
      cursor: {
        points: { size: 10, width: 2, stroke: css('--surface'), fill: (self, si) => css(series[si - 1].color) },
        drag: { x: !coarse, y: false, setScale: false },
        focus: { prox: 30 },
      },
      focus: { alpha: 0.35 },
      scales: {
        x: { time: xMode === 'time' },
        y: {
          range: (self, min, max) => {
            const lo = yMin ?? Math.min(0, min);
            const hi = yMax ?? Math.max(max, lo + 1);
            const pad = (hi - lo) * 0.06;
            return [yMin ?? (lo < 0 ? lo - pad : lo), yMax ?? hi + pad];
          },
        },
      },
      axes: [
        {
          stroke: text3,
          font,
          grid: { show: false },
          ticks: { show: true, stroke: axis, width: 1, size: 4 },
          values: (self, splits) => splits.map((v) => fmtX(v)),
          space: 64,
        },
        {
          stroke: text3,
          font,
          grid: { stroke: grid, width: 1 },
          ticks: { show: false },
          values: (self, splits) => splits.map((v) => yFormat(v)),
          size: 50,
          space: 36,
          // y-as schaalt mee met het zichtbare (ingezoomde) stuk
        },
      ],
      series: [
        {},
        ...series.map((s) => {
          const c = css(s.color);
          return {
            label: s.label,
            stroke: c,
            width: s.width ?? 2,
            dash: s.dash,
            show: !hidden[s.label],
            spanGaps: false,
            paths: s.step ? uPlot.paths.stepped!({ align: 1 }) : undefined,
            fill: s.fill
              ? (self: uPlot) => {
                  const g = self.ctx.createLinearGradient(0, self.bbox.top, 0, self.bbox.top + self.bbox.height);
                  g.addColorStop(0, withAlpha(c, 0.22));
                  g.addColorStop(1, withAlpha(c, 0.01));
                  return g;
                }
              : undefined,
            points: { show: false },
          } satisfies uPlot.Series;
        }),
      ],
      hooks: {
        drawAxes: [
          (self) => {
            const y0 = self.valToPos(0, 'y', true);
            if (y0 < self.bbox.top || y0 > self.bbox.top + self.bbox.height) return;
            const ctx = self.ctx;
            ctx.save();
            ctx.strokeStyle = axis;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(self.bbox.left, Math.round(y0) + 0.5);
            ctx.lineTo(self.bbox.left + self.bbox.width, Math.round(y0) + 0.5);
            ctx.stroke();
            ctx.restore();
          },
        ],
        setSelect: [
          (self) => {
            if (self.select.width > 12) {
              setZoom(self.posToVal(self.select.left, 'x'), self.posToVal(self.select.left + self.select.width, 'x'));
            }
            self.setSelect({ left: 0, top: 0, width: 0, height: 0 }, false);
          },
        ],
        setCursor: [
          (self) => {
            const idx = self.cursor.idx;
            if (idx == null || self.cursor.left == null || self.cursor.left < 0) {
              tip.style.opacity = '0';
              return;
            }
            let rows = '';
            series.forEach((s, i) => {
              if (hidden[s.label]) return;
              const v = self.data[i + 1][idx];
              rows += `<div class="r"><i style="background:${css(s.color)}"></i><span>${s.label}</span><b>${v == null ? '—' : fmtV(v as number, i)}</b></div>`;
            });
            tip.innerHTML = `<div class="t">${fmtX(self.data[0][idx] as number)}</div>${rows}`;
            const left = self.cursor.left + self.over.offsetLeft;
            const tw = tip.offsetWidth;
            const W = wrap.clientWidth;
            const pos = left + 18 + tw > W ? left - tw - 18 : left + 18;
            tip.style.transform = `translate(${Math.max(0, pos)}px, 6px)`;
            tip.style.opacity = '1';
          },
        ],
      },
    };
    const data = [x, ...series.map((s) => s.values)] as uPlot.AlignedData;
    u = new uPlot(opts, data, plotEl);
    const [a, b] = zoom ?? fullRange;
    u.setScale('x', { min: a, max: b });
    attachGestures(u);
  }

  // ---------- Aanraakgebaren ----------
  function attachGestures(plot: uPlot) {
    const over = plot.over;
    over.style.touchAction = 'pan-y';
    let g: { mode: 'scrub' | 'pan' | 'pinch'; x0: number; a0: number; b0: number; d0: number; c0: number } | null = null;
    let lastTap = 0;
    let hideT: ReturnType<typeof setTimeout> | undefined;

    const rel = (clientX: number) => clientX - over.getBoundingClientRect().left;
    const cursorAt = (t: Touch) => {
      const r = over.getBoundingClientRect();
      plot.setCursor({ left: t.clientX - r.left, top: t.clientY - r.top });
    };
    const hide = () => plot.setCursor({ left: -10, top: -10 });

    over.addEventListener(
      'touchstart',
      (e) => {
        clearTimeout(hideT);
        const [a0, b0] = zoom ?? fullRange;
        if (e.touches.length === 2) {
          const [t1, t2] = [e.touches[0], e.touches[1]];
          g = { mode: 'pinch', x0: 0, a0, b0, d0: Math.abs(t1.clientX - t2.clientX) || 1, c0: rel((t1.clientX + t2.clientX) / 2) };
          hide();
          e.preventDefault();
          return;
        }
        const t = e.touches[0];
        const now = Date.now();
        if (now - lastTap < 300) {
          resetZoom();
          lastTap = 0;
          return;
        }
        lastTap = now;
        g = { mode: zoom ? 'pan' : 'scrub', x0: t.clientX, a0, b0, d0: 0, c0: 0 };
        cursorAt(t);
      },
      { passive: false },
    );

    over.addEventListener(
      'touchmove',
      (e) => {
        if (!g) return;
        const W = over.clientWidth || 1;
        if (g.mode === 'pinch' && e.touches.length === 2) {
          const [t1, t2] = [e.touches[0], e.touches[1]];
          const d = Math.abs(t1.clientX - t2.clientX) || 1;
          const c = rel((t1.clientX + t2.clientX) / 2);
          const span0 = g.b0 - g.a0;
          const span = span0 * (g.d0 / d);
          const anchor = g.a0 + (g.c0 / W) * span0;
          const min = anchor - (c / W) * span;
          setZoom(min, min + span);
          e.preventDefault();
        } else if (g.mode === 'pan' && e.touches.length === 1) {
          const dx = e.touches[0].clientX - g.x0;
          if (Math.abs(dx) > 4) {
            const shift = (-dx / W) * (g.b0 - g.a0);
            setZoom(g.a0 + shift, g.b0 + shift);
            hide();
          }
        } else if (g.mode === 'scrub' && e.touches.length === 1) {
          cursorAt(e.touches[0]);
        }
      },
      { passive: false },
    );

    over.addEventListener('touchend', (e) => {
      if (e.touches.length === 0) {
        g = null;
        hideT = setTimeout(hide, 3500);
      }
    });
    over.addEventListener('dblclick', () => resetZoom());
  }

  // ---------- Volledig scherm ----------
  function openFull() {
    full = true;
    document.documentElement.classList.add('chart-full');
    history.pushState({ chartFull: true }, '');
  }
  function closeFull(fromPop = false) {
    if (!full) return;
    full = false;
    document.documentElement.classList.remove('chart-full');
    if (!fromPop && history.state?.chartFull) history.back();
  }

  onMount(() => {
    build();
    const ro = new ResizeObserver(() => u?.setSize({ width: Math.max(200, wrap.clientWidth), height: h }));
    ro.observe(wrap);
    const mo = new MutationObserver(() => build());
    mo.observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] });
    const onResize = () => {
      vw = window.innerWidth;
      vh = window.innerHeight;
    };
    const onPop = () => closeFull(true);
    const onKey = (e: KeyboardEvent) => e.key === 'Escape' && closeFull();
    window.addEventListener('resize', onResize);
    window.addEventListener('orientationchange', onResize);
    window.addEventListener('popstate', onPop);
    window.addEventListener('keydown', onKey);
    return () => {
      ro.disconnect();
      mo.disconnect();
      window.removeEventListener('resize', onResize);
      window.removeEventListener('orientationchange', onResize);
      window.removeEventListener('popstate', onPop);
      window.removeEventListener('keydown', onKey);
      document.documentElement.classList.remove('chart-full');
      u?.destroy();
      u = null;
    };
  });

  // Nieuwe data → opnieuw opbouwen (zoom blijft behouden)
  $effect(() => {
    const deps = [x, series, hidden];
    if (u && deps.some((d, i) => d !== builtWith[i])) build();
  });
  // Hoogte (draaien/volledig scherm) → alleen formaat aanpassen
  $effect(() => {
    const hh = h;
    void full;
    if (u) requestAnimationFrame(() => u?.setSize({ width: Math.max(200, wrap.clientWidth), height: hh }));
  });

  function toggle(label: string) {
    hidden = { ...hidden, [label]: !hidden[label] };
  }
</script>

<figure class="chart" class:full aria-label={ariaLabel}>
  {#if full}
    <header class="full-head">
      <h3>{title ?? ariaLabel}</h3>
      <button type="button" class="tbtn close" onclick={() => closeFull()} aria-label="Sluiten"><Icon icon={X} size={22} /></button>
    </header>
  {/if}

  <div class="bar">
    {#if legend && series.length > 1}
      <div class="legend" role="group" aria-label="Legenda">
        {#each series as s (s.label)}
          <button type="button" class:off={hidden[s.label]} onclick={() => toggle(s.label)} aria-pressed={!hidden[s.label]}>
            <i style="background: var({s.color}); {s.dash ? 'height:0; border-top: 2px dashed var(' + s.color + '); background: none' : ''}"></i>{s.label}
          </button>
        {/each}
      </div>
    {:else}
      <span></span>
    {/if}
    <div class="tools">
      <button type="button" class="tbtn" onclick={() => zoomBy(1.6)} disabled={!zoom} aria-label="Uitzoomen"><Icon icon={ZoomOut} size={18} /></button>
      <button type="button" class="tbtn" onclick={() => zoomBy(0.55)} aria-label="Inzoomen"><Icon icon={ZoomIn} size={18} /></button>
      {#if zoom}
        <button type="button" class="tbtn" onclick={resetZoom} aria-label="Alles tonen"><Icon icon={RotateCcw} size={17} /></button>
      {/if}
      {#if !full}
        <button type="button" class="tbtn" onclick={openFull} aria-label="Grafiek vergroten"><Icon icon={Maximize2} size={17} /></button>
      {/if}
    </div>
  </div>

  <div class="plot" bind:this={wrap}>
    <div bind:this={plotEl}></div>
    <div class="tip" bind:this={tip} role="status" aria-live="off"></div>
  </div>

  {#if coarse}
    <p class="hint">
      {#if full && !landscapePhone}<Icon icon={Smartphone} size={14} /> Draai je telefoon voor een breder beeld ·{/if}
      {zoom ? 'Sleep om te schuiven · dubbeltik = alles' : 'Schuif met je vinger · knijp om te zoomen'}
    </p>
  {/if}
</figure>

<style>
  .chart {
    margin: 0;
    min-width: 0;
  }
  .chart.full {
    position: fixed;
    inset: 0;
    z-index: 200;
    background: var(--bg);
    padding: calc(10px + env(safe-area-inset-top)) max(14px, env(safe-area-inset-right)) calc(10px + env(safe-area-inset-bottom))
      max(14px, env(safe-area-inset-left));
    display: flex;
    flex-direction: column;
    animation: zoomIn 0.35s var(--ease-out);
  }
  @keyframes zoomIn {
    from {
      opacity: 0;
      transform: scale(0.97);
    }
  }
  .full-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    margin-bottom: 6px;
  }
  .full-head h3 {
    font-size: 20px;
  }
  .bar {
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    gap: 8px;
    margin-bottom: 8px;
  }
  .legend {
    display: flex;
    flex-wrap: wrap;
    gap: 2px 4px;
    margin-left: -8px;
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
    -webkit-tap-highlight-color: transparent;
  }
  .legend button:hover {
    background: var(--surface-2);
  }
  .legend button.off {
    opacity: 0.35;
  }
  .legend i {
    width: 14px;
    height: 3px;
    border-radius: 2px;
    display: inline-block;
  }
  .tools {
    display: flex;
    gap: 4px;
    flex: none;
  }
  .tbtn {
    display: grid;
    place-items: center;
    width: 36px;
    height: 36px;
    border-radius: 50%;
    border: 0;
    background: var(--surface-2);
    color: var(--text-2);
    cursor: pointer;
    -webkit-tap-highlight-color: transparent;
    transition:
      background 0.2s,
      opacity 0.2s;
  }
  .tbtn:hover {
    background: var(--surface-3);
    color: var(--text);
  }
  .tbtn:disabled {
    opacity: 0.35;
    cursor: default;
  }
  .tbtn.close {
    width: 42px;
    height: 42px;
  }
  .plot {
    position: relative;
    width: 100%;
    user-select: none;
    -webkit-user-select: none;
  }
  .full .plot {
    flex: 1;
  }
  .hint {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    margin-top: 6px;
    font-size: 12.5px;
    color: var(--text-3);
    text-align: center;
  }
  .tip {
    position: absolute;
    top: 0;
    left: 0;
    pointer-events: none;
    opacity: 0;
    transition: opacity 0.15s;
    background: color-mix(in srgb, var(--surface-2) 92%, transparent);
    border: 0.5px solid var(--border-strong);
    border-radius: 14px;
    padding: 10px 12px;
    min-width: 170px;
    box-shadow: 0 12px 30px -10px rgba(0, 0, 0, 0.45);
    z-index: 3;
    font-size: 13.5px;
    backdrop-filter: blur(16px);
    -webkit-backdrop-filter: blur(16px);
  }
  .tip :global(.t) {
    font-weight: 700;
    margin-bottom: 6px;
    color: var(--text);
  }
  .tip :global(.r) {
    display: flex;
    align-items: center;
    gap: 8px;
    color: var(--text-2);
    line-height: 1.7;
  }
  .tip :global(.r i) {
    width: 10px;
    height: 10px;
    border-radius: 3px;
    flex: none;
  }
  .tip :global(.r b) {
    margin-left: auto;
    padding-left: 14px;
    color: var(--text);
    font-weight: 600;
    font-variant-numeric: tabular-nums;
  }
  :global(html.chart-full),
  :global(html.chart-full body) {
    overflow: hidden;
  }
  :global(html.chart-full .tabbar),
  :global(html.chart-full .topbar) {
    visibility: hidden;
  }
</style>
