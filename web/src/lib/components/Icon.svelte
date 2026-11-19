<script lang="ts">
  import type { IconNode } from 'lucide';
  let {
    icon,
    size = 20,
    stroke = 2,
    class: cls = '',
    label,
  }: { icon: IconNode; size?: number; stroke?: number; class?: string; label?: string } = $props();
</script>

<svg
  class={cls}
  width={size}
  height={size}
  viewBox="0 0 24 24"
  fill="none"
  stroke="currentColor"
  stroke-width={stroke}
  stroke-linecap="round"
  stroke-linejoin="round"
  role={label ? 'img' : undefined}
  aria-label={label}
  aria-hidden={label ? undefined : 'true'}
>
  {#each icon as [tag, attrs], i (i)}
    <svelte:element this={tag} xmlns="http://www.w3.org/2000/svg" {...attrs} />
  {/each}
</svg>
