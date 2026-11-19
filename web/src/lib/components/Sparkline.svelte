<script lang="ts">
  let {
    values,
    color = '--c-house',
    height = 36,
    min,
    max,
    label,
  }: { values: (number | null)[]; color?: string; height?: number; min?: number; max?: number; label: string } = $props();

  const W = 120;
  const geo = $derived.by(() => {
    const vs = values.filter((v): v is number => v != null);
    if (vs.length < 2) return null;
    const lo = min ?? Math.min(...vs);
    const hi = max ?? Math.max(...vs);
    const span = hi - lo || 1;
    let d = '';
    let pen = false;
    values.forEach((v, i) => {
      if (v == null) {
        pen = false;
        return;
      }
      const x = (i / Math.max(1, values.length - 1)) * W;
      const y = 3 + (1 - (v - lo) / span) * (height - 6);
      d += `${pen ? 'L' : 'M'}${x.toFixed(1)},${y.toFixed(1)}`;
      pen = true;
    });
    return d;
  });
</script>

<svg viewBox="0 0 {W} {height}" preserveAspectRatio="none" style="height:{height}px" role="img" aria-label={label}>
  {#if geo}
    <path d={geo} style="stroke: var({color})" vector-effect="non-scaling-stroke" />
  {/if}
</svg>

<style>
  svg {
    width: 100%;
    display: block;
    overflow: visible;
  }
  path {
    fill: none;
    stroke-width: 2;
    stroke-linejoin: round;
    stroke-linecap: round;
  }
</style>
