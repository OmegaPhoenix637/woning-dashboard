<script lang="ts">
  // Strakke, minimalistische energiescène: het huis als lijntekening (naar de echte woning),
  // met zon, net, batterijen en laadpaal eromheen. Energie stroomt als lichtpuntjes over dunne lijnen.
  import { Sun, Moon, UtilityPole, Car, PlugZap } from 'lucide';
  import Icon from './Icon.svelte';
  import AnimatedNumber from './AnimatedNumber.svelte';
  import { powerParts, n0 } from '../format';
  import type { Battery, EvCharger } from '../types';

  let {
    solarW,
    gridW,
    houseW,
    batteries,
    ev = null,
    night = false,
  }: {
    solarW: number;
    gridW: number;
    houseW: number;
    batteries: Battery[];
    ev?: EvCharger | null;
    night?: boolean;
  } = $props();

  let width = $state(900);
  const tall = $derived(width < 600);
  const TH = 25;

  // Lay-out in viewBox-eenheden. Huis lokaal 220 x 240.
  const L = $derived(
    tall
      ? {
          W: 400,
          H: 610,
          house: { x: 114, y: 196, s: 0.76 },
          houseLabel: [200, 388],
          nodes: { sun: [70, 128], bat: [330, 128], grid: [70, 482], ev: [330, 482] },
          r: 44,
        }
      : {
          W: 1000,
          H: 470,
          house: { x: 380, y: 92, s: 1.08 },
          houseLabel: [500, 378],
          nodes: { sun: [170, 112], bat: [830, 112], grid: [170, 330], ev: [830, 330] },
          r: 46,
        },
  );

  const H = (lx: number, ly: number) => [L.house.x + lx * L.house.s, L.house.y + ly * L.house.s];

  function link(node: number[], target: number[]) {
    const [nx, ny] = node;
    const [ex, ey] = target;
    if (tall) {
      // Bovenste rondjes: lijn vanaf de onderkant; onderste: vanaf de bovenkant. Tekst staat aan de buitenkant.
      const sy = ny + (ey > ny ? L.r + 4 : -(L.r + 4));
      return `M${nx} ${sy} C ${nx} ${ey}, ${nx} ${ey}, ${ex} ${ey}`;
    }
    const sx = nx + (ex > nx ? L.r + 4 : -(L.r + 4));
    const mx = (sx + ex) / 2;
    return `M${sx} ${ny} C ${mx} ${ny}, ${mx} ${ey}, ${ex} ${ey}`;
  }

  const evW = $derived(ev?.state === 'charging' ? ev.w : 0);
  const batW = $derived(batteries.reduce((a, b) => a + b.w, 0));
  const batSoc = $derived(
    batteries.reduce((a, b) => a + b.soc * b.capacityKwh, 0) / Math.max(0.01, batteries.reduce((a, b) => a + b.capacityKwh, 0)),
  );
  const solarK = $derived(Math.min(1, Math.max(0, solarW / 6500)));

  const flows = $derived([
    { id: 'sun', d: link(L.nodes.sun, H(tall ? 10 : 6, 77)), w: solarW, c: 'solar', rev: false },
    { id: 'grid', d: link(L.nodes.grid, H(0, 214)), w: gridW, c: 'grid', rev: gridW < 0 },
    { id: 'bat', d: link(L.nodes.bat, H(210, tall ? 118 : 128)), w: batW, c: 'battery', rev: batW > 0 },
    { id: 'ev', d: link(L.nodes.ev, H(210, 204)), w: evW, c: 'cool', rev: true },
  ]);

  const motionOk =
    typeof window !== 'undefined' &&
    !matchMedia('(prefers-reduced-motion: reduce)').matches &&
    document.documentElement.dataset.motion !== 'off';

  function dur(w: number) {
    const d = Math.max(0.9, Math.min(3.6, 4.4 - Math.log10(Math.max(30, Math.abs(w))) * 0.95));
    return Math.round(d * 4) / 4;
  }

  const pos = (p: number[]) => `left:${(p[0] / L.W) * 100}%; top:${(p[1] / L.H) * 100}%`;
  const P = (w: number) => powerParts(w);
  const lit = $derived(night ? 0.5 + Math.min(0.5, houseW / 3000) : 0);
</script>

<div class="scene" class:tall bind:clientWidth={width} style="aspect-ratio:{L.W} / {L.H}">
  <svg viewBox="0 0 {L.W} {L.H}" preserveAspectRatio="xMidYMid meet" aria-hidden="true">
    <defs>
      <radialGradient id="es-halo">
        <stop offset="0" style="stop-color: var(--g-house)" stop-opacity="0.16" />
        <stop offset="1" style="stop-color: var(--g-house)" stop-opacity="0" />
      </radialGradient>
      <linearGradient id="es-fade-g" x1="0" y1="0" x2="1" y2="0">
        <stop offset="0" stop-color="#fff" stop-opacity="0" />
        <stop offset="1" stop-color="#fff" stop-opacity="1" />
      </linearGradient>
      <mask id="es-fade" maskUnits="userSpaceOnUse" x="-100" y="0" width="100" height="250">
        <rect x="-100" y="0" width="100" height="250" fill="url(#es-fade-g)" />
      </mask>
      {#each ['solar', 'grid', 'battery', 'cool'] as c (c)}
        <radialGradient id="es-dot-{c}">
          <stop offset="0" style="stop-color: var(--g-{c})" stop-opacity="0.9" />
          <stop offset="0.4" style="stop-color: var(--g-{c})" stop-opacity="0.35" />
          <stop offset="1" style="stop-color: var(--g-{c})" stop-opacity="0" />
        </radialGradient>
      {/each}
    </defs>

    <!-- zachte gloed achter het huis -->
    <ellipse cx={H(110, 140)[0]} cy={H(110, 140)[1]} rx={200 * L.house.s} ry={170 * L.house.s} fill="url(#es-halo)" class="breath" />

    <!-- lijnen -->
    {#each flows as f (f.id)}
      <path d={f.d} class="line" />
      {#if Math.abs(f.w) >= TH}<path d={f.d} class="line-on" style="stroke: var(--g-{f.c})" />{/if}
    {/each}

    <!-- huis: rechterhelft van een twee-onder-een-kap (links de buren, met snijlijn) -->
    <g transform="translate({L.house.x} {L.house.y}) scale({L.house.s})" class="house">
      <!-- buren: vervagende schets links van de snijlijn -->
      <g class="neighbour" mask="url(#es-fade)">
        <path d="M-90 -14 H0 M-90 92 H0 M-90 96 V240 M-70 110 h22 v44 h-22 z M-40 110 h22 v44 h-22 z M-70 184 h28 v56 h-28 z" />
        <path d="M-90 176 H0" class="canopy-n" />
      </g>
      <!-- dak (links recht afgesneden, rechts schuin); de nok ligt hoger dan de dakkapel -->
      <rect x="8" y="-30" width="13" height="18" rx="1.5" class="fill-2" />
      <path d="M0 92 V-14 H146 L222 92 Z" class="fill" />
      <!-- brede dakkapel: 3 ramen, dicht paneel, 3 ramen -->
      <rect x="12" y="10" width="160" height="56" rx="3" class="fill-2" />
      <path d="M6 10 H178" class="strong" />
      {#each [20, 38, 56, 104, 122, 140] as wx (wx)}
        <rect x={wx} y="20" width="14" height="38" rx="1.5" class="win" />
        <rect x={wx} y="20" width="14" height="38" rx="1.5" class="glow" style="opacity:{lit}" />
      {/each}
      <rect x="76" y="20" width="22" height="38" rx="1.5" class="fill-2 panel-closed" />
      <!-- zonnepanelen: rij op het dak onder de dakkapel -->
      {#each Array(9) as _, i (i)}
        <rect
          x={6 + i * 19.5}
          y="70"
          width="17"
          height="13"
          rx="1.5"
          class="panel"
          class:on={solarW > TH}
          style="--k:{0.35 + solarK * 0.65}; animation-delay:-{i * 0.3}s"
        />
      {/each}
      <!-- overstek -->
      <path d="M0 92 H232" class="strong" />
      <!-- gevel -->
      <rect x="0" y="96" width="210" height="144" class="fill" />
      <path d="M98 96 V240" class="thin" />
      <!-- verdieping: twee ramen met sierlijst, erker rechts -->
      <rect x="16" y="110" width="22" height="44" rx="2" class="win" />
      <rect x="46" y="110" width="22" height="44" rx="2" class="win" />
      <rect x="16" y="110" width="52" height="44" class="glow" style="opacity:{lit * 0.7}" />
      <path d="M10 160 H74" class="strong" />
      <path d="M18 160 v4 M32 160 v4 M46 160 v4 M60 160 v4" class="thin" />
      <rect x="108" y="106" width="88" height="50" rx="2" class="win" />
      <path d="M122 106 V156 M182 106 V156" class="thin" />
      <rect x="108" y="106" width="88" height="50" class="glow" style="opacity:{lit}" />
      <!-- begane grond: raampje, luifel (loopt door naar de buren), deur, krans, erker -->
      <rect x="30" y="170" width="30" height="9" rx="1.5" class="win" />
      <path d="M0 184 H80" class="strong" />
      <rect x="20" y="190" width="24" height="50" rx="2" class="door" />
      <circle cx="58" cy="210" r="6" class="thin wreath" />
      <rect x="106" y="172" width="92" height="58" rx="2" class="win" />
      <path d="M120 172 V230 M184 172 V230 M106 186 H198" class="thin" />
      <rect x="106" y="172" width="92" height="58" class="glow" style="opacity:{lit}" />
      <!-- heg -->
      <path d="M128 240 a14 10 0 0 1 28 0 M170 240 a18 13 0 0 1 36 0" class="hedge" />
      <!-- snijlijn: hier begint de buurwoning -->
      <path d="M0 2 V244" class="cut" />
      <path d="M-30 240 H250" class="ground" />
    </g>

    <!-- bewegende energie -->
    {#if motionOk}
      {#each flows as f (f.id)}
        {#if Math.abs(f.w) >= TH}
          {@const dd = dur(f.w)}
          {#key `${f.id}-${dd}-${f.rev}-${f.d}`}
            {#each [0, 1, 2] as k (k)}
              <g>
                <circle r="9" fill="url(#es-dot-{f.c})" />
                <circle r="2.6" style="fill: var(--g-{f.c})" />
                <animateMotion
                  dur="{dd}s"
                  begin="-{(k * dd) / 3}s"
                  repeatCount="indefinite"
                  keyPoints={f.rev ? '1;0' : '0;1'}
                  keyTimes="0;1"
                  calcMode="linear"
                  path={f.d}
                />
              </g>
            {/each}
          {/key}
        {/if}
      {/each}
    {/if}
  </svg>

  <!-- knooppunten -->
  <div class="node" style="{pos(L.nodes.sun)}; --c: var(--g-solar)" class:on={solarW > TH} class:above={tall}>
    <div class="orb">
      {#if solarW > TH}<span class="spin" aria-hidden="true"></span>{/if}
      <Icon icon={night ? Moon : Sun} size={26} stroke={1.75} />
    </div>
    <div class="cap">
      <span class="name">Zonnepanelen</span>
      <span class="val">{#if solarW > TH}<AnimatedNumber value={solarW} format={(v) => P(v)[0]} /><small>{P(solarW)[1]}</small>{:else}<em>{night ? 'nacht' : 'rust'}</em>{/if}</span>
    </div>
  </div>

  <div class="node" style="{pos(L.nodes.grid)}; --c: var(--g-grid)" class:on={Math.abs(gridW) > TH}>
    <div class="orb"><Icon icon={UtilityPole} size={26} stroke={1.75} /></div>
    <div class="cap">
      <span class="name">{gridW < -TH ? 'Teruglevering' : gridW > TH ? 'Van het net' : 'Net'}</span>
      <span class="val"><AnimatedNumber value={Math.abs(gridW)} format={(v) => P(v)[0]} /><small>{P(gridW)[1]}</small></span>
    </div>
  </div>

  <div class="node" style="{pos(L.nodes.bat)}; --c: var(--g-battery)" class:on={Math.abs(batW) > TH} class:above={tall}>
    <div class="orb ring" style="--p:{batSoc}">
      <!-- twee staafjes: elke batterij apart; de ring eromheen = samen -->
      <div class="minibats" aria-hidden="true">
        {#each batteries.slice(0, 2) as b (b.id)}
          <span class="mb" class:charging={b.w > TH} class:discharging={b.w < -TH}><i style="height:{Math.max(4, Math.min(100, b.soc))}%"></i></span>
        {/each}
      </div>
    </div>
    <div class="cap">
      <span class="name">Batterijen · samen {n0(batSoc)}%</span>
      <span class="each">
        {#each batteries.slice(0, 2) as b, i (b.id)}
          <span class="pct"><b>{i + 1}</b>{n0(b.soc)}%</span>
        {/each}
      </span>
      <span class="val">
        {#if Math.abs(batW) <= TH}<em>rust</em>{:else}<AnimatedNumber value={Math.abs(batW)} format={(v) => P(v)[0]} /><small>{P(batW)[1]} {batW > 0 ? 'laden' : 'ontladen'}</small>{/if}
      </span>
    </div>
  </div>

  <div class="node" style="{pos(L.nodes.ev)}; --c: var(--g-cool)" class:on={evW > TH}>
    <div class="orb"><Icon icon={evW > TH ? PlugZap : Car} size={26} stroke={1.75} /></div>
    <div class="cap">
      <span class="name">{evW > TH && ev?.car ? ev.car : 'Laadpaal'}</span>
      <span class="val">
        {#if evW > TH}<AnimatedNumber value={evW} format={(v) => P(v)[0]} /><small>{P(evW)[1]} laden</small>
        {:else if ev?.state === 'done'}<em>klaar</em>{:else}<em>vrij</em>{/if}
      </span>
    </div>
  </div>

  <div class="house-cap" style={pos(L.houseLabel)}>
    <span class="name">Huis verbruikt</span>
    <span class="val big"><AnimatedNumber value={houseW} format={(v) => P(v)[0]} /><small>{P(houseW)[1]}</small></span>
  </div>
</div>

<style>
  .scene {
    position: relative;
    width: 100%;
    container-type: inline-size;
  }
  svg {
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
    overflow: visible;
  }
  .breath {
    animation: breath 7s ease-in-out infinite;
    transform-box: fill-box;
    transform-origin: center;
  }
  @keyframes breath {
    50% {
      opacity: 0.55;
      transform: scale(0.94);
    }
  }
  .line {
    fill: none;
    stroke: var(--border-strong);
    stroke-width: 1.5;
    vector-effect: non-scaling-stroke;
  }
  .line-on {
    fill: none;
    stroke-width: 1.5;
    opacity: 0.45;
    vector-effect: non-scaling-stroke;
  }
  .house :global(*) {
    vector-effect: non-scaling-stroke;
  }
  .house .fill,
  .house .fill-2,
  .house .win,
  .house .door {
    stroke: color-mix(in srgb, var(--text) 55%, transparent);
    stroke-width: 1.5;
    stroke-linejoin: round;
  }
  .house .fill {
    fill: color-mix(in srgb, var(--surface-2) 70%, transparent);
  }
  .house .fill-2 {
    fill: var(--surface-2);
  }
  .house .win {
    fill: color-mix(in srgb, var(--g-house) 8%, var(--surface));
  }
  .house .door {
    fill: var(--surface-3);
  }
  .house .glow {
    fill: #ffc66b;
    transition: opacity 1.5s;
    mix-blend-mode: screen;
  }
  .house .strong {
    stroke: color-mix(in srgb, var(--text) 70%, transparent);
    stroke-width: 2;
    stroke-linecap: round;
    fill: none;
  }
  .house .thin {
    stroke: color-mix(in srgb, var(--text) 35%, transparent);
    stroke-width: 1.2;
    fill: none;
  }
  .house .wreath {
    stroke-dasharray: 2 2;
  }
  .neighbour path {
    fill: none;
    stroke: color-mix(in srgb, var(--text) 30%, transparent);
    stroke-width: 1.2;
  }
  .house .cut {
    stroke: color-mix(in srgb, var(--text) 45%, transparent);
    stroke-width: 1.2;
    stroke-dasharray: 5 4;
    fill: none;
  }
  .house .panel-closed {
    stroke-width: 1;
  }
  .house .hedge {
    fill: none;
    stroke: color-mix(in srgb, var(--g-battery) 45%, var(--text-3));
    stroke-width: 1.4;
  }
  .house .ground {
    stroke: var(--border-strong);
    stroke-width: 1.5;
    stroke-linecap: round;
  }
  .panel {
    fill: color-mix(in srgb, var(--g-house) 12%, var(--surface));
    stroke: color-mix(in srgb, var(--text) 40%, transparent);
    stroke-width: 1;
    transition: fill 0.8s;
  }
  .panel.on {
    fill: color-mix(in srgb, var(--g-solar) calc(var(--k) * 70%), var(--surface));
    stroke: var(--g-solar);
    animation: shimmer 3.2s ease-in-out infinite;
  }
  @keyframes shimmer {
    50% {
      opacity: 0.65;
    }
  }
  .node {
    position: absolute;
    transform: translate(-50%, calc(var(--size) / -2));
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 10px;
    --size: clamp(64px, 10.5cqw, 116px);
  }
  .orb :global(svg) {
    width: 40%;
    height: 40%;
  }
  .minibats {
    display: flex;
    gap: 9%;
    height: 44%;
    width: 36%;
    justify-content: center;
  }
  .mb {
    position: relative;
    flex: 1;
    border-radius: 4px;
    background: color-mix(in srgb, var(--c) 8%, var(--surface));
    box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--c) 30%, transparent);
    overflow: hidden;
  }
  .mb i {
    position: absolute;
    left: 0;
    right: 0;
    bottom: 0;
    background: var(--c);
    border-radius: 3px;
    transition: height 1.2s var(--ease-out);
  }
  .mb.charging i {
    animation: fillpulse 1.8s ease-in-out infinite;
  }
  @keyframes fillpulse {
    50% {
      opacity: 0.6;
    }
  }
  .each {
    display: flex;
    gap: 6px;
    margin: 2px 0 1px;
  }
  .pct {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 2px 8px 2px 3px;
    border-radius: 999px;
    background: var(--surface-2);
    font-size: clamp(12px, 1.4cqw, 14px);
    font-weight: 650;
    font-variant-numeric: tabular-nums;
  }
  .pct b {
    display: grid;
    place-items: center;
    width: 18px;
    height: 18px;
    border-radius: 50%;
    font-size: 11px;
    color: #04130d;
    background: var(--g-battery);
  }
  .tall .node {
    --size: clamp(54px, 20cqw, 80px);
  }
  .node.above {
    flex-direction: column-reverse;
    transform: translate(-50%, calc(-100% + var(--size) / 2));
  }
  .orb {
    position: relative;
    width: var(--size);
    height: var(--size);
    border-radius: 50%;
    display: grid;
    place-items: center;
    color: color-mix(in srgb, var(--c) 55%, var(--text-3));
    background: var(--surface-2);
    border: 1.5px solid var(--border-strong);
    transition:
      color 0.5s,
      border-color 0.5s,
      box-shadow 0.5s;
  }
  .node.on .orb {
    color: var(--c);
    border-color: color-mix(in srgb, var(--c) 60%, transparent);
    box-shadow:
      0 0 0 6px color-mix(in srgb, var(--c) 8%, transparent),
      0 0 32px -4px color-mix(in srgb, var(--c) 55%, transparent);
  }
  .orb.ring {
    border-color: transparent;
    color: var(--c);
    background:
      radial-gradient(circle, var(--surface-2) 64%, transparent 65%),
      conic-gradient(var(--c) calc(var(--p) * 1%), color-mix(in srgb, var(--c) 14%, var(--surface-3)) 0);
  }
  .spin {
    position: absolute;
    inset: -7px;
    border-radius: 50%;
    border: 1.5px dashed color-mix(in srgb, var(--c) 55%, transparent);
    animation: spin 18s linear infinite;
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
  .cap,
  .house-cap {
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
    line-height: 1.15;
    gap: 3px;
  }
  .house-cap {
    position: absolute;
    transform: translate(-50%, 0);
  }
  .name {
    font-size: clamp(12px, 1.6cqw, 16px);
    font-weight: 600;
    color: var(--text-2);
    white-space: nowrap;
  }
  .val {
    font-size: clamp(19px, 3cqw, 32px);
    font-weight: 700;
    letter-spacing: -0.03em;
    white-space: nowrap;
  }
  .val.big {
    font-size: clamp(26px, 4.2cqw, 46px);
  }
  .tall .name {
    font-size: clamp(12px, 3.4cqw, 14px);
  }
  .tall .val {
    font-size: clamp(18px, 5.6cqw, 24px);
  }
  .tall .pct {
    font-size: 12.5px;
  }
  .tall .val.big {
    font-size: clamp(26px, 8.4cqw, 36px);
  }
  .val small {
    font-size: 0.55em;
    font-weight: 600;
    color: var(--text-2);
    margin-left: 4px;
    letter-spacing: 0;
  }
  .val em {
    font-style: normal;
    font-size: 0.7em;
    font-weight: 600;
    color: var(--text-3);
  }
</style>
