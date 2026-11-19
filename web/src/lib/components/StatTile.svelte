<script lang="ts">
  import type { IconNode } from 'lucide';
  import Icon from './Icon.svelte';
  import AnimatedNumber from './AnimatedNumber.svelte';

  let {
    label,
    value,
    format,
    unit = '',
    icon,
    accent = 'var(--g-house)',
    sub,
    delta,
    deltaGoodWhenUp = true,
    href,
    i = 0,
  }: {
    label: string;
    value: number | null | undefined;
    format: (v: number) => string;
    unit?: string;
    icon: IconNode;
    accent?: string;
    sub?: string;
    delta?: { text: string; up: boolean } | null;
    deltaGoodWhenUp?: boolean;
    href?: string;
    i?: number;
  } = $props();

  const good = $derived(delta ? delta.up === deltaGoodWhenUp : true);
</script>

<svelte:element this={href ? 'a' : 'div'} {href} class="tile card rise" style="--i:{i}; --accent:{accent}">
  <div class="top">
    <span class="ico"><Icon {icon} size={18} /></span>
    <span class="label">{label}</span>
  </div>
  <div class="value">
    <AnimatedNumber {value} {format} />{#if unit}<span class="unit">{unit}</span>{/if}
  </div>
  {#if sub || delta}
    <div class="foot">
      {#if delta}
        <span class="delta" class:good class:bad={!good}>{delta.up ? '▲' : '▼'} {delta.text}</span>
      {/if}
      {#if sub}<span class="sub">{sub}</span>{/if}
    </div>
  {/if}
</svelte:element>

<style>
  .tile {
    display: flex;
    flex-direction: column;
    gap: 10px;
    padding: 18px;
    overflow: hidden;
    transition: transform 0.35s var(--ease), border-color 0.3s;
  }
  a.tile:hover {
    transform: translateY(-2px);
    border-color: var(--border-strong);
  }
  .top {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .ico {
    display: grid;
    place-items: center;
    width: 34px;
    height: 34px;
    border-radius: 11px;
    color: var(--accent);
    background: color-mix(in srgb, var(--accent) 14%, transparent);
    flex: none;
  }
  .label {
    font-size: 14px;
    font-weight: 550;
    color: var(--text-2);
    line-height: 1.25;
    min-width: 0;
  }
  @media (max-width: 480px) {
    .top {
      flex-direction: column;
      align-items: flex-start;
      gap: 8px;
    }
  }
  .value {
    font-family: var(--font-display);
    font-size: clamp(28px, 2.4vw, 34px);
    font-weight: 700;
    letter-spacing: -0.035em;
    line-height: 1.05;
    white-space: nowrap;
  }
  .unit {
    font-size: 15px;
    font-weight: 550;
    color: var(--text-2);
    margin-left: 5px;
    letter-spacing: 0;
  }
  .foot {
    display: flex;
    flex-wrap: wrap;
    gap: 4px 10px;
    font-size: 13px;
    color: var(--text-3);
  }
  .delta {
    font-weight: 600;
  }
  .delta.good {
    color: var(--s-good-text);
  }
  .delta.bad {
    color: var(--s-serious);
  }

</style>
