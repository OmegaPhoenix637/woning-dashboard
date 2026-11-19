import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

// base './' zodat de site werkt op https://<gebruiker>.github.io/<repo>/
export default defineConfig({
  base: './',
  plugins: [svelte()],
  build: {
    target: 'es2022',
    cssCodeSplit: true,
    rollupOptions: {
      output: {
        manualChunks: { uplot: ['uplot'] },
      },
    },
  },
  test: {
    include: ['src/**/*.test.ts'],
  },
});
