import '@fontsource-variable/inter';
import './app.css';
import { mount } from 'svelte';
import App from './App.svelte';
import { startReveal } from './lib/reveal';

try {
  if (localStorage.getItem('mw.motion') === 'off') document.documentElement.dataset.motion = 'off';
} catch {
  /* geen opslag */
}

mount(App, { target: document.getElementById('app')! });
startReveal();

if (import.meta.env.PROD && 'serviceWorker' in navigator) {
  navigator.serviceWorker.register('./sw.js').catch(() => {});
}
