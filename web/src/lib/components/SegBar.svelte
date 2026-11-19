<script lang="ts">
  // Hoge verdeelbalk: één geheel, verdeeld in delen (bv. waar ging de zonnestroom heen).
  let {
    parts,
    format,
    label,
  }: { parts: { label: string; value: number; color: string }[]; format: (v: number) => string; label: string } = $props();

  let width = $state(300);
  const total = $derived(parts.reduce((a, p) => a + Math.max(0, p.value), 0));
  const share = (v: number) => (total > 0 ? Math.max(0, v) / total : 0);
  const aria = $derived(
    `${label}: ${parts.map((p) => `${p.label} ${format(p.value)} (${Math.round(share(p.value) * 100)}%)`).join(', ')}`,
  );
</script>

<div class="seg-wrap">
  <div class="bar" bind:clientWidth={width} role="img" aria-label={aria}>
    {#if total <= 0}
      <div class="empty">Nog geen data</div>
    {:else}
      {#each parts as p (p.label)}
        {#if p.value > 0}
          {@const s = share(p.value)}
          <div class="part" style="flex-grow:{s}; background: var({p.color})" title="{p.label}: {format(p.value)}">
            {#if s * width > 46}<span>{Math.round(s * 100)}%</span>{/if}
          </div>
        {/if}
      {/each}
    {/if}
  </div>
  <ul class="legend">
    {#each parts as p (p.label)}
      <li><i style="background: var({p.color})"></i><span>{p.label}</span><b>{format(p.value)}</b></li>
    {/each}
  </ul>
</div>

<style>
  .bar {
    display: flex;
    gap: 2px;
    height: 28px;
    min-width: 0;
  }
  .part {
    flex-basis: 0;
    min-width: 3px;
    border-radius: 4px;
    display: flex;
    align-items: center;
    padding-left: 8px;
    overflow: hidden;
    transition: flex-grow 0.8s var(--ease);
  }
  .part span {
    color: #fff;
    font-size: 12.5px;
    font-weight: 650;
    white-space: nowrap;
  }
  .empty {
    flex: 1;
    border-radius: 4px;
    background: var(--surface-2);
    color: var(--text-3);
    font-size: 13px;
    display: grid;
    place-items: center;
  }
  .legend {
    list-style: none;
    padding: 0;
    margin: 12px 0 0;
    display: grid;
    gap: 6px;
  }
  .legend li {
    display: flex;
    align-items: center;
    gap: 10px;
    font-size: 14px;
    color: var(--text-2);
  }
  .legend i {
    width: 10px;
    height: 10px;
    border-radius: 3px;
    flex: none;
  }
  .legend b {
    margin-left: auto;
    color: var(--text);
    font-weight: 600;
    font-variant-numeric: tabular-nums;
  }
</style>
