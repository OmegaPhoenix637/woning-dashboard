// Maakt screenshots van de site met demo-data (pc + iPhone 16 Pro Max staand/liggend).
// Gebruik:  npm run build && node tools/screenshots.mjs [filter]
// Vereist Microsoft Edge (standaard op Windows) — er wordt geen browser gedownload.
// Eigen namen (optioneel, niet in git): tools/local-names.json
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const out = path.resolve(root, '..', 'screenshots');
mkdirSync(out, { recursive: true });
const filter = process.argv[2] ?? '';
const namesFile = path.join(root, 'tools', 'local-names.json');
const names = existsSync(namesFile) ? JSON.parse(readFileSync(namesFile, 'utf8')) : null;

const server = await preview({ root, preview: { port: 4179, strictPort: true }, logLevel: 'silent' });
const base = 'http://localhost:4179/';
const browser = await chromium.launch({ channel: 'msedge', headless: true });

const D = { width: 1920, height: 1080 };
// iPhone 16 Pro Max: 440 x 956 CSS-pixels
const P = { width: 440, height: 956, scale: 2, mobile: true };
const PL = { width: 956, height: 440, scale: 2, mobile: true };

async function shoot(name, hash, { width, height, scale = 1, theme = 'dark', mobile = false, full = true, action } = {}) {
  if (filter && !name.includes(filter)) return;
  const ctx = await browser.newContext({
    viewport: { width, height },
    deviceScaleFactor: scale,
    colorScheme: theme,
    isMobile: mobile,
    hasTouch: mobile,
  });
  await ctx.addInitScript(
    ([t, n]) => {
      localStorage.setItem('mw.theme', t);
      sessionStorage.setItem('mw.hideDemo', '1');
      if (n) localStorage.setItem('mw.demo.settings', JSON.stringify({ names: n }));
    },
    [theme, names],
  );
  const page = await ctx.newPage();
  await page.goto(base + hash);
  await page.waitForSelector('main .page');
  if (action) await action(page);
  await page.waitForTimeout(2800);
  if (full && mobile) {
    // Volledige pagina: vaste balken op hun plek laten staan zoals op de telefoon
    await page.addStyleTag({ content: '.shell{position:relative}.tabbar{position:absolute!important}' });
  }
  await page.screenshot({ path: path.join(out, name + '.png'), fullPage: full });
  await ctx.close();
  console.log('✓', name);
}

const click = (text) => async (p) => {
  await p.getByRole('button', { name: text, exact: true }).first().click();
  await p.waitForTimeout(400);
};

await shoot('01-pc-overzicht', '#/overzicht', D);
await shoot('02-pc-energie-dag', '#/energie', D);
await shoot('03-pc-energie-maand', '#/energie?p=maand', D);
await shoot('04-pc-batterijen', '#/batterijen', D);
await shoot('05-pc-klimaat', '#/klimaat', D);
await shoot('06-pc-apparaten', '#/apparaten', D);
await shoot('07-pc-vergelijken-jaar', '#/vergelijken', { ...D, action: click('Jaar') });
await shoot('08-pc-instellingen', '#/instellingen', D);
await shoot('09-pc-overzicht-licht', '#/overzicht', { ...D, theme: 'light' });
await shoot('10-iphone-overzicht-scherm', '#/overzicht', { ...P, full: false });
await shoot('11-iphone-overzicht', '#/overzicht', P);
await shoot('12-iphone-energie', '#/energie', P);
await shoot('13-iphone-energie-maand', '#/energie?p=maand', P);
await shoot('14-iphone-klimaat', '#/klimaat', P);
await shoot('15-iphone-batterijen', '#/batterijen', P);
await shoot('16-iphone-vergelijken', '#/vergelijken', P);
await shoot('17-iphone-apparaten', '#/apparaten', P);
await shoot('18-iphone-liggend-overzicht', '#/overzicht', { ...PL, full: false });
await shoot('19-iphone-liggend-energie', '#/energie', { ...PL, full: false, action: async (p) => p.evaluate(() => window.scrollTo(0, 380)) });
await shoot('20-iphone-grafiek-volledig', '#/energie', {
  ...PL,
  full: false,
  action: async (p) => {
    await p.getByRole('button', { name: 'Grafiek vergroten' }).first().click();
    await p.waitForTimeout(500);
  },
});

// Andere telefoons
await shoot('21-android-klein-overzicht', '#/overzicht', { width: 360, height: 780, scale: 2, mobile: true, full: false });
await shoot('22-iphone15-overzicht', '#/overzicht', { width: 393, height: 852, scale: 2, mobile: true, full: false });
await shoot('23-android-klein-energie', '#/energie', { width: 360, height: 780, scale: 2, mobile: true });

await shoot('24-android-klein-scene', '#/overzicht', { width: 360, height: 780, scale: 2, mobile: true, full: false, action: async (p) => p.evaluate(() => window.scrollTo(0, 230)) });
await shoot('25-iphone15-scene', '#/overzicht', { width: 393, height: 852, scale: 2, mobile: true, full: false, action: async (p) => p.evaluate(() => window.scrollTo(0, 230)) });
await shoot('26-iphone16promax-scene', '#/overzicht', { width: 440, height: 956, scale: 2, mobile: true, full: false, action: async (p) => p.evaluate(() => window.scrollTo(0, 230)) });

// iPad (Air/Pro 11: 820x1180) staand en liggend
const IPAD = { width: 820, height: 1180, scale: 2, mobile: true };
const IPADL = { width: 1180, height: 820, scale: 2, mobile: true };
await shoot('27-ipad-staand-overzicht', '#/overzicht', { ...IPAD, full: false });
await shoot('28-ipad-liggend-overzicht', '#/overzicht', { ...IPADL, full: false });
await shoot('29-ipad-staand-energie', '#/energie', IPAD);
await shoot('30-iphone-liggend-overzicht-scene', '#/overzicht', { ...PL, full: false, action: async (p) => p.evaluate(() => window.scrollTo(0, 260)) });
await shoot('31-iphone-liggend-batterij', '#/batterijen', { ...PL, full: false });

// App-iconen (PNG) voor installatie op telefoon
if (!filter) {
  const ctx = await browser.newContext({ viewport: { width: 512, height: 512 } });
  const p = await ctx.newPage();
  for (const size of [180, 192, 512]) {
    await p.setViewportSize({ width: size, height: size });
    await p.goto(base + 'icon.svg');
    await p.screenshot({ path: path.join(root, 'public', `icon-${size}.png`) });
  }
  await ctx.close();
}

await browser.close();
await new Promise((r) => server.httpServer.close(r));
console.log('Klaar →', out);
