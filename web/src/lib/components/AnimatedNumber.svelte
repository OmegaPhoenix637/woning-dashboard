<script lang="ts">
  import { Tween } from 'svelte/motion';
  import { cubicOut } from 'svelte/easing';

  let {
    value,
    format = (v: number) => String(Math.round(v)),
    duration = 900,
  }: { value: number | null | undefined; format?: (v: number) => string; duration?: number } = $props();

  const reduce = typeof matchMedia !== 'undefined' && matchMedia('(prefers-reduced-motion: reduce)').matches;
  // Begin vanaf de eerste waarde (niet vanaf 0) zodat niet alles bij het laden 'optelt'.
  const tw = new Tween(0, { duration: reduce ? 0 : duration, easing: cubicOut });
  let first = true;

  $effect(() => {
    const v = value;
    if (v == null || !Number.isFinite(v)) return;
    if (first) {
      first = false;
      tw.set(v, { duration: reduce ? 0 : duration * 1.3 });
    } else tw.target = v;
  });
</script>

{#if value == null || !Number.isFinite(value)}—{:else}{format(tw.current)}{/if}
