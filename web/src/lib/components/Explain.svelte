<script lang="ts">
  // Klein ⓘ-knopje dat een korte uitleg in gewone taal uitklapt. Zit niet in de weg.
  import type { Snippet } from 'svelte';
  import { slide } from 'svelte/transition';
  import { Info, X } from 'lucide';
  import Icon from './Icon.svelte';

  let { children, label = 'Uitleg' }: { children: Snippet; label?: string } = $props();
  let open = $state(false);
  const id = `ex-${Math.random().toString(36).slice(2, 8)}`;
</script>

<button type="button" class="ex-btn" class:open aria-expanded={open} aria-controls={id} onclick={() => (open = !open)}>
  <Icon icon={open ? X : Info} size={15} />
  <span>{open ? 'Sluiten' : label}</span>
</button>
{#if open}
  <div class="ex-box" {id} transition:slide={{ duration: 260 }}>
    {@render children()}
  </div>
{/if}

<style>
  .ex-btn {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    height: 30px;
    padding: 0 11px 0 9px;
    border-radius: 999px;
    border: 0;
    background: var(--surface-2);
    color: var(--text-2);
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    flex: none;
    -webkit-tap-highlight-color: transparent;
  }
  .ex-btn:hover,
  .ex-btn.open {
    color: var(--text);
    background: var(--surface-3);
  }
  .ex-box {
    grid-column: 1 / -1;
    width: 100%;
    margin: 4px 0 14px;
    padding: 14px 16px;
    border-radius: 16px;
    background: color-mix(in srgb, var(--g-house) 8%, var(--surface-2));
    color: var(--text-2);
    font-size: 14.5px;
    line-height: 1.55;
  }
  .ex-box :global(p + p) {
    margin-top: 8px;
  }
  .ex-box :global(b) {
    color: var(--text);
  }
</style>
