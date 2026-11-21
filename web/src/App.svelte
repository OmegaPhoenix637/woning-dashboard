<script lang="ts">
  import { fade } from 'svelte/transition';
  import {
    LayoutDashboard,
    Zap,
    BatteryCharging,
    Thermometer,
    WashingMachine,
    GitCompareArrows,
    Settings as SettingsIcon,
  } from 'lucide';
  import type { IconNode } from 'lucide';
  import Icon from './lib/components/Icon.svelte';
  import { app } from './lib/data/app.svelte';
  import { router, href, type Route } from './lib/router.svelte';
  import { relativeAgo } from './lib/date';
  import Overview from './pages/Overview.svelte';
  import Energy from './pages/Energy.svelte';
  import Batteries from './pages/Batteries.svelte';
  import Climate from './pages/Climate.svelte';
  import Appliances from './pages/Appliances.svelte';
  import Compare from './pages/Compare.svelte';
  import SettingsPage from './pages/Settings.svelte';

  const NAV: { route: Route; label: string; short: string; icon: IconNode; tab: boolean }[] = [
    { route: 'overzicht', label: 'Overzicht', short: 'Overzicht', icon: LayoutDashboard, tab: true },
    { route: 'energie', label: 'Energie', short: 'Energie', icon: Zap, tab: true },
    { route: 'batterijen', label: 'Batterijen', short: 'Batterij', icon: BatteryCharging, tab: true },
    { route: 'klimaat', label: 'Klimaat', short: 'Klimaat', icon: Thermometer, tab: true },
    { route: 'apparaten', label: 'Apparaten', short: 'Apparaten', icon: WashingMachine, tab: true },
    { route: 'vergelijken', label: 'Vergelijken', short: 'Vergelijk', icon: GitCompareArrows, tab: true },
    { route: 'instellingen', label: 'Instellingen', short: 'Instellingen', icon: SettingsIcon, tab: false },
  ];

  const PAGES = {
    overzicht: Overview,
    energie: Energy,
    batterijen: Batteries,
    klimaat: Climate,
    apparaten: Appliances,
    vergelijken: Compare,
    instellingen: SettingsPage,
  };
  const Page = $derived(PAGES[router.route]);

  const ageS = $derived(app.live ? app.now / 1000 - app.live.ts : Infinity);
  const status = $derived(app.error && !app.live ? 'err' : ageS < 600 ? 'ok' : ageS < 1800 ? 'late' : 'err');
  const statusText = $derived(
    !app.live
      ? app.error
        ? 'Geen verbinding'
        : 'Laden…'
      : status === 'ok'
        ? `Live · ${relativeAgo(app.live.ts, app.now)}`
        : `Bijgewerkt ${relativeAgo(app.live.ts, app.now)}`,
  );

  app.init();
</script>

<div class="shell">
  <aside class="sidebar">
    <a class="brand" href={href('overzicht')}>
      <img src="./icon.svg" alt="" width="34" height="34" />
      <span>Mijn Woning</span>
    </a>
    <nav aria-label="Hoofdmenu">
      {#each NAV as n (n.route)}
        <a href={href(n.route)} class:active={router.route === n.route} aria-current={router.route === n.route ? 'page' : undefined}>
          <Icon icon={n.icon} size={20} />
          <span>{n.label}</span>
        </a>
      {/each}
    </nav>
    <div class="side-foot">
      <div class="status {status}"><span class="dot"></span>{statusText}</div>
      {#if app.isDemo}<div class="demo-tag">Demo-data</div>{/if}
    </div>
  </aside>

  <header class="topbar">
    <a class="brand" href={href('overzicht')}>
      <img src="./icon.svg" alt="" width="28" height="28" />
      <span>Mijn Woning</span>
    </a>
    <div class="top-right">
      <div class="status {status}"><span class="dot"></span>{statusText}</div>
      <a class="btn icon ghost" href={href('instellingen')} aria-label="Instellingen"><Icon icon={SettingsIcon} /></a>
    </div>
  </header>

  <main>
    {#if !app.ready}
      <div class="loading">
        <div class="spinner"></div>
        <p>Gegevens ophalen…</p>
      </div>
    {:else}
      {#key router.route}
        <div in:fade={{ duration: 280 }}>
          <Page />
        </div>
      {/key}
    {/if}
  </main>

  <!-- Telefoon: vaste tabbalk onderin (staand) of smalle balk links (liggend) -->
  <nav class="tabbar" aria-label="Hoofdmenu">
    <div class="only-landscape rail-status {status}" title={statusText}><span class="dot"></span><span>{status === 'ok' ? 'Live' : status === 'late' ? 'Oud' : 'Uit'}</span></div>
    {#each NAV.filter((n) => n.tab) as n (n.route)}
      <a href={href(n.route)} class:active={router.route === n.route} aria-current={router.route === n.route ? 'page' : undefined}>
        <Icon icon={n.icon} size={23} stroke={router.route === n.route ? 2.2 : 1.8} />
        <span>{n.short}</span>
      </a>
    {/each}
    <a href={href('instellingen')} class="only-landscape" class:active={router.route === 'instellingen'} aria-label="Instellingen">
      <Icon icon={SettingsIcon} size={22} />
      <span>Instellingen</span>
    </a>
  </nav>
</div>

<style>
  .shell {
    min-height: 100dvh;
  }
  main {
    max-width: 1440px;
    margin: 0 auto;
    padding: 12px max(16px, env(safe-area-inset-left)) calc(108px + env(safe-area-inset-bottom)) max(16px, env(safe-area-inset-right));
  }
  .brand {
    display: flex;
    align-items: center;
    gap: 10px;
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 17px;
    letter-spacing: -0.02em;
  }
  .sidebar {
    display: none;
  }

  /* ---------- Bovenbalk (telefoon/tablet) ---------- */
  .topbar {
    position: sticky;
    top: 0;
    z-index: 20;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    padding: calc(8px + env(safe-area-inset-top)) max(16px, env(safe-area-inset-right)) 8px max(16px, env(safe-area-inset-left));
    background: color-mix(in srgb, var(--bg) 72%, transparent);
    backdrop-filter: blur(20px) saturate(1.8);
    -webkit-backdrop-filter: blur(20px) saturate(1.8);
    border-bottom: 0.5px solid var(--border-strong);
  }
  .top-right {
    display: flex;
    align-items: center;
    gap: 4px;
  }
  .status {
    display: inline-flex;
    align-items: center;
    gap: 7px;
    font-size: 13px;
    font-weight: 600;
    color: var(--text-2);
    white-space: nowrap;
  }
  .status .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
  }
  .status.ok .dot {
    background: var(--s-good);
    color: var(--s-good);
    animation: pulse 2.4s infinite;
  }
  .status.late .dot {
    background: var(--s-warn);
  }
  .status.err .dot {
    background: var(--s-crit);
  }

  /* ---------- Tabbalk (iOS-stijl, zwevend glas) ---------- */
  .tabbar {
    position: fixed;
    left: max(12px, env(safe-area-inset-left));
    right: max(12px, env(safe-area-inset-right));
    bottom: max(10px, env(safe-area-inset-bottom));
    z-index: 40;
    display: grid;
    grid-template-columns: repeat(6, 1fr);
    padding: 6px;
    border-radius: 30px;
    background: color-mix(in srgb, var(--surface) 72%, transparent);
    backdrop-filter: blur(24px) saturate(1.8);
    -webkit-backdrop-filter: blur(24px) saturate(1.8);
    border: 0.5px solid var(--border-strong);
    box-shadow: 0 12px 40px -10px rgba(0, 0, 0, 0.45);
  }
  .tabbar a {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 3px;
    min-height: 56px;
    padding: 6px 2px;
    border-radius: 24px;
    font-size: 10.5px;
    font-weight: 600;
    color: var(--text-3);
    -webkit-tap-highlight-color: transparent;
    transition:
      color 0.2s,
      background 0.3s var(--ease);
  }
  .tabbar a.active {
    color: var(--g-house);
    background: color-mix(in srgb, var(--g-house) 12%, transparent);
  }
  .tabbar .only-landscape {
    display: none;
  }

  @media (max-width: 400px) {
    .topbar .brand span {
      display: none; /* smal scherm: alleen het icoon, zodat de live-melding past */
    }
  }
  .loading {
    display: grid;
    place-items: center;
    gap: 14px;
    min-height: 60vh;
    color: var(--text-2);
  }
  .spinner {
    width: 34px;
    height: 34px;
    border-radius: 50%;
    border: 3px solid var(--border-strong);
    border-top-color: var(--g-house);
    animation: spin 0.9s linear infinite;
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }

  /* ---------- Liggend (telefoon en iPad): navigatie als balk links, maximale ruimte voor inhoud ---------- */
  @media (orientation: landscape) and (max-height: 540px), (orientation: landscape) and (pointer: coarse) and (max-width: 1500px) {
    .topbar {
      display: none;
    }
    main {
      padding: 12px calc(12px + env(safe-area-inset-right)) 20px calc(96px + env(safe-area-inset-left));
    }
    .tabbar {
      left: max(8px, env(safe-area-inset-left));
      right: auto;
      top: 8px;
      bottom: 8px;
      width: 76px;
      grid-template-columns: 1fr;
      grid-auto-rows: 1fr;
      border-radius: 26px;
      padding: 6px;
    }
    .tabbar a {
      min-height: 0;
      font-size: 9.5px;
    }
    .tabbar .only-landscape {
      display: flex;
    }
    .rail-status {
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 3px;
      font-size: 9.5px;
      font-weight: 700;
      color: var(--text-2);
    }
  }
  .rail-status {
    display: none;
  }
  .rail-status .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--s-crit);
  }
  .rail-status.ok .dot {
    background: var(--s-good);
    color: var(--s-good);
    animation: pulse 2.4s infinite;
  }
  .rail-status.late .dot {
    background: var(--s-warn);
  }

  /* iPad liggend: bredere balk met grotere knoppen */
  @media (orientation: landscape) and (pointer: coarse) and (min-height: 541px) and (max-width: 1500px) {
    main {
      padding: 24px calc(24px + env(safe-area-inset-right)) 32px calc(124px + env(safe-area-inset-left));
    }
    .tabbar {
      width: 96px;
      left: max(12px, env(safe-area-inset-left));
      top: 12px;
      bottom: 12px;
      border-radius: 30px;
    }
    .tabbar a {
      font-size: 11.5px;
      gap: 5px;
    }
    .rail-status {
      font-size: 11px;
    }
  }

  /* iPad staand: tabbalk onderin, gecentreerd en wat groter; meer ruimte rondom */
  @media (orientation: portrait) and (pointer: coarse) and (min-width: 700px) {
    main {
      padding: 20px max(28px, env(safe-area-inset-left)) calc(124px + env(safe-area-inset-bottom)) max(28px, env(safe-area-inset-right));
    }
    .tabbar {
      left: max(24px, calc(50% - 360px));
      right: max(24px, calc(50% - 360px));
      bottom: max(16px, env(safe-area-inset-bottom));
      padding: 8px;
    }
    .tabbar a {
      min-height: 64px;
      font-size: 12.5px;
    }
    .topbar {
      padding-left: max(28px, env(safe-area-inset-left));
      padding-right: max(28px, env(safe-area-inset-right));
    }
  }

  /* ---------- Desktop (muis/trackpad) ---------- */
  @media (min-width: 1024px) and (min-height: 541px) and (pointer: fine) {
    .shell {
      display: grid;
      grid-template-columns: 248px 1fr;
    }
    .topbar,
    .tabbar {
      display: none;
    }
    main {
      width: 100%;
      padding: 36px 48px 56px;
    }
    .sidebar {
      position: sticky;
      top: 0;
      height: 100dvh;
      display: flex;
      flex-direction: column;
      gap: 30px;
      padding: 30px 16px;
      border-right: 0.5px solid var(--border-strong);
      background: color-mix(in srgb, var(--bg-2) 60%, transparent);
    }
    .sidebar .brand {
      padding: 0 12px;
      font-size: 19px;
    }
    .sidebar nav {
      display: grid;
      gap: 2px;
    }
    .sidebar nav a {
      display: flex;
      align-items: center;
      gap: 14px;
      height: 44px;
      padding: 0 14px;
      border-radius: 12px;
      font-weight: 500;
      font-size: 15.5px;
      color: var(--text-2);
      transition:
        background 0.2s,
        color 0.2s;
    }
    .sidebar nav a:hover {
      background: var(--surface);
      color: var(--text);
    }
    .sidebar nav a.active {
      background: var(--surface-2);
      color: var(--text);
      font-weight: 600;
    }
    .side-foot {
      margin-top: auto;
      display: grid;
      gap: 8px;
      padding: 14px;
      border-radius: 16px;
      background: var(--surface);
    }
    .demo-tag {
      font-size: 12.5px;
      color: var(--s-warn);
      font-weight: 600;
    }
  }
</style>
