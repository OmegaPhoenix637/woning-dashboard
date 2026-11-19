<script lang="ts">
  import { Euro, Link, Palette, Activity, Plus, Trash2, Check, Copy, CircleCheck, CircleAlert, CircleX, BookOpen, Tag, Share, SquarePlus } from 'lucide';
  import Icon from '../lib/components/Icon.svelte';
  import { app, makeKoppelLink } from '../lib/data/app.svelte';
  import { sortTariffs } from '../lib/calc/tariff';
  import { relativeAgo, fmtDateLong, todayIso } from '../lib/date';
  import { n0 } from '../lib/format';
  import { DEVICES, SOURCE_NAMES, type TariffPeriod } from '../lib/types';

  // ---- Tarieven ----
  let tariffs = $state<TariffPeriod[]>(sortTariffs(app.settings.tariffs).map((t) => ({ ...t })));
  let saving = $state(false);
  let saved = $state(false);
  let saveErr = $state<string | null>(null);

  function addPeriod() {
    const last = tariffs.at(-1);
    tariffs = [...tariffs, { ...(last ?? { importPrice: 0.27, exportPrice: 0.05, exportCost: 0, fixedPerMonth: 6.5, gasPrice: 1.35, netting: false }), from: todayIso() } as TariffPeriod];
  }
  function remove(i: number) {
    tariffs = tariffs.filter((_, j) => j !== i);
  }
  // ---- Namen ----
  const NAME_GROUPS = [
    { title: 'Kamers (airco’s)', items: DEVICES.climate },
    { title: 'Verwarming', items: DEVICES.heaters },
    { title: 'Apparaten', items: DEVICES.appliances },
    { title: 'Batterijen', items: DEVICES.batteries },
  ];
  let names = $state<Record<string, string>>({ ...(app.settings.names ?? {}) });
  let namesSaved = $state(false);
  async function saveNames() {
    const clean = Object.fromEntries(Object.entries(names).filter(([, v]) => v && v.trim()).map(([k, v]) => [k, v.trim()]));
    await app.saveSettings({ ...app.settings, names: clean });
    namesSaved = true;
    setTimeout(() => (namesSaved = false), 2500);
  }

  async function save() {
    saving = true;
    saveErr = null;
    try {
      await app.saveSettings({ ...app.settings, version: 1, tariffs: sortTariffs(tariffs) });
      tariffs = sortTariffs(tariffs);
      saved = true;
      setTimeout(() => (saved = false), 2500);
    } catch (e) {
      saveErr = (e as Error).message;
    } finally {
      saving = false;
    }
  }

  // ---- Koppeling ----
  let owner = $state(app.config?.owner ?? '');
  let repo = $state(app.config?.repo ?? 'woning-data');
  let token = $state('');
  let connecting = $state(false);
  let connErr = $state<string | null>(null);
  let copied = $state(false);

  async function connect() {
    connecting = true;
    connErr = null;
    try {
      await app.connect({ owner: owner.trim(), repo: repo.trim(), token: token.trim() });
      token = '';
    } catch (e) {
      connErr = (e as Error).message;
    } finally {
      connecting = false;
    }
  }
  async function copyLink() {
    if (!app.config) return;
    await navigator.clipboard.writeText(makeKoppelLink(app.config));
    copied = true;
    setTimeout(() => (copied = false), 2500);
  }

  // ---- Weergave ----
  let theme = $state<'dark' | 'light' | 'auto'>(readLS('mw.theme', 'dark') as 'dark');
  let motion = $state(readLS('mw.motion', 'on'));
  function readLS(k: string, d: string) {
    try {
      return localStorage.getItem(k) ?? d;
    } catch {
      return d;
    }
  }
  function setTheme(t: typeof theme) {
    theme = t;
    try {
      localStorage.setItem('mw.theme', t);
    } catch {
      /* */
    }
    const eff = t === 'auto' ? (matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark') : t;
    document.documentElement.dataset.theme = eff;
  }
  function setMotion(m: string) {
    motion = m;
    try {
      localStorage.setItem('mw.motion', m);
    } catch {
      /* */
    }
    if (m === 'off') document.documentElement.dataset.motion = 'off';
    else delete document.documentElement.dataset.motion;
  }

  const health = $derived(app.live?.health);
  const fmtUptime = (s: number) => {
    const d = Math.floor(s / 86400);
    const h = Math.floor((s % 86400) / 3600);
    return d ? `${d} d ${h} u` : `${h} u`;
  };
</script>

<div class="page">
  <header class="page-head rise">
    <div>
      <p class="eyebrow">Instellingen</p>
      <h1>Instellingen</h1>
      <p>Tarieven, namen, koppeling en een korte uitleg van de app.</p>
    </div>
  </header>

  <!-- Uitleg -->
  <section class="card rise guide" style="--i:1">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-house)"><Icon icon={BookOpen} size={18} /></span>Zo werkt de app</h2>
    </div>
    <div class="tips">
      <div class="tip">
        <h3><Icon icon={SquarePlus} size={18} /> Als app op je iPhone</h3>
        <p>Open de site in <b>Safari</b> → tik onderin op <b>Deel</b> <Icon icon={Share} size={14} /> → kies <b>Zet op beginscherm</b> → <b>Voeg toe</b>. Daarna open je hem met het huisje-icoon, net als een gewone app.</p>
      </div>
      <div class="tip">
        <h3>Onderin wisselen</h3>
        <p>Met de balk onderin ga je naar Overzicht, Energie, Klimaat, Apparaten en Vergelijken. Het tandwieltje rechtsboven is deze pagina.</p>
      </div>
      <div class="tip">
        <h3>Lijngrafieken</h3>
        <p><b>Schuif met je vinger</b> over de grafiek om de waarden te zien. <b>Knijp met twee vingers</b> (of tik op +) om in te zoomen, en sleep dan om te schuiven. <b>Dubbel tikken</b> = weer alles. Het <b>vierkantje</b> maakt de grafiek schermvullend, en <b>draai je telefoon</b> voor een breder beeld.</p>
      </div>
      <div class="tip">
        <h3>Staafjes</h3>
        <p><b>Tik één keer</b> op een staaf om de waarde te zien, <b>nog een keer</b> om die dag te openen. Bij veel dagen kun je naar links <b>swipen</b> voor eerdere dagen.</p>
      </div>
      <div class="tip">
        <h3>Live</h3>
        <p>Het <b>groene bolletje</b> bovenin betekent dat alles actueel is. De gegevens verversen vanzelf elke paar minuten.</p>
      </div>
      <div class="tip">
        <h3>Bespaard</h3>
        <p>Wat je zonder zonnepanelen en batterijen had betaald, min wat je nu betaalt. Klopt het tarief niet? Pas het hieronder aan.</p>
      </div>
    </div>
  </section>

  <!-- Tarieven -->
  <section class="card rise" style="--i:1">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-money)"><Icon icon={Euro} size={18} /></span>Stroomtarieven</h2>
    </div>
    <p class="intro">
      Vul de tarieven van ENGIE in (inclusief btw en energiebelasting). Verandert je tarief? Voeg dan een nieuwe periode toe
      met de ingangsdatum, dan blijven oude dagen met het oude tarief berekend. De salderingsregeling stopt op <b>1 januari 2027</b>.
    </p>
    <div class="periods">
      {#each tariffs as t, i (i)}
        <div class="period">
          <div class="p-head">
            <div class="field">
              <label for="from-{i}">Geldig vanaf</label>
              <input id="from-{i}" type="date" bind:value={t.from} />
            </div>
            <span class="p-label">{t.from ? fmtDateLong(t.from, true) : ''}</span>
            {#if tariffs.length > 1}
              <button type="button" class="btn icon ghost" onclick={() => remove(i)} aria-label="Periode verwijderen"><Icon icon={Trash2} size={18} /></button>
            {/if}
          </div>
          <div class="p-grid">
            <div class="field">
              <label for="imp-{i}">Stroom inkoop</label>
              <div class="unit-in"><input id="imp-{i}" type="number" step="0.001" min="0" bind:value={t.importPrice} /><span>€/kWh</span></div>
            </div>
            <div class="field">
              <label for="exp-{i}">Terugleververgoeding</label>
              <div class="unit-in"><input id="exp-{i}" type="number" step="0.001" min="0" bind:value={t.exportPrice} /><span>€/kWh</span></div>
            </div>
            <div class="field">
              <label for="ec-{i}">Terugleverkosten</label>
              <div class="unit-in"><input id="ec-{i}" type="number" step="0.001" min="0" bind:value={t.exportCost} /><span>€/kWh</span></div>
            </div>
            <div class="field">
              <label for="fix-{i}">Vaste kosten</label>
              <div class="unit-in"><input id="fix-{i}" type="number" step="0.01" bind:value={t.fixedPerMonth} /><span>€/mnd</span></div>
            </div>
            <div class="field">
              <label for="gas-{i}">Gas</label>
              <div class="unit-in"><input id="gas-{i}" type="number" step="0.001" min="0" bind:value={t.gasPrice} /><span>€/m³</span></div>
            </div>
            <label class="toggle">
              <input type="checkbox" bind:checked={t.netting} />
              <span class="sw" aria-hidden="true"></span>
              <span>Saldering actief</span>
            </label>
          </div>
        </div>
      {/each}
    </div>
    <div class="actions">
      <button type="button" class="btn" onclick={addPeriod}><Icon icon={Plus} size={18} />Nieuwe periode</button>
      <button type="button" class="btn primary" onclick={save} disabled={saving}>
        {#if saved}<Icon icon={Check} size={18} />Opgeslagen{:else}{saving ? 'Opslaan…' : 'Opslaan'}{/if}
      </button>
    </div>
    {#if saveErr}<p class="err">{saveErr}</p>{/if}
    {#if app.isDemo}<p class="hint">In demo-modus worden tarieven alleen in deze browser bewaard.</p>{/if}
  </section>

  <!-- Namen -->
  <section class="card rise" style="--i:2">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-appl)"><Icon icon={Tag} size={18} /></span>Namen van kamers en apparaten</h2>
    </div>
    <p class="intro">Geef alles een eigen naam. Leeg laten = standaardnaam. Deze namen worden privé bewaard (niet in de openbare code).</p>
    <div class="name-groups">
      {#each NAME_GROUPS as g (g.title)}
        <div class="name-group">
          <h3>{g.title}</h3>
          {#each g.items as d (d.id)}
            <div class="field">
              <label for="nm-{d.id}">{d.name}</label>
              <input id="nm-{d.id}" bind:value={names[d.id]} placeholder={d.name} autocomplete="off" />
            </div>
          {/each}
        </div>
      {/each}
    </div>
    <div class="actions">
      <button type="button" class="btn primary" onclick={saveNames}>{#if namesSaved}<Icon icon={Check} size={18} />Opgeslagen{:else}Namen opslaan{/if}</button>
    </div>
  </section>

  <!-- Koppeling -->
  <section class="card rise" style="--i:2">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-house)"><Icon icon={Link} size={18} /></span>Koppeling met de data</h2>
      <span class="pill">{app.isDemo ? 'Demo-data' : 'Gekoppeld'}</span>
    </div>
    {#if app.config}
      <p class="intro">Gekoppeld met <b>{app.config.owner}/{app.config.repo}</b>. Stuur de koppellink naar een ander apparaat (telefoon, tablet) om daar ook in te loggen.</p>
      <div class="actions">
        <button type="button" class="btn" onclick={copyLink}><Icon icon={copied ? Check : Copy} size={18} />{copied ? 'Gekopieerd' : 'Koppellink kopiëren'}</button>
        <button type="button" class="btn ghost" onclick={() => app.disconnect()}>Ontkoppelen</button>
      </div>
      <p class="hint">Deel de koppellink alleen met gezinsleden: wie de link heeft, kan de data zien.</p>
    {:else}
      <p class="intro">Vul de gegevens van de privé data-repo in. De koppelcode is een GitHub fine-grained token met alleen toegang tot die ene repo.</p>
      <form
        class="conn"
        onsubmit={(e) => {
          e.preventDefault();
          connect();
        }}
      >
        <div class="field"><label for="own">GitHub-gebruiker</label><input id="own" bind:value={owner} autocomplete="off" required /></div>
        <div class="field"><label for="rep">Data-repo</label><input id="rep" bind:value={repo} autocomplete="off" required /></div>
        <div class="field wide"><label for="tok">Koppelcode</label><input id="tok" type="password" bind:value={token} autocomplete="off" placeholder="github_pat_…" required /></div>
        <button class="btn primary" type="submit" disabled={connecting}>{connecting ? 'Controleren…' : 'Koppelen'}</button>
      </form>
      {#if connErr}<p class="err">{connErr}</p>{/if}
    {/if}
  </section>

  <!-- Weergave -->
  <section class="card rise" style="--i:3">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-appl)"><Icon icon={Palette} size={18} /></span>Weergave</h2>
    </div>
    <div class="rows">
      <div class="row">
        <span>Thema</span>
        <div class="seg" role="group" aria-label="Thema">
          <button type="button" aria-pressed={theme === 'dark'} onclick={() => setTheme('dark')}>Donker</button>
          <button type="button" aria-pressed={theme === 'light'} onclick={() => setTheme('light')}>Licht</button>
          <button type="button" aria-pressed={theme === 'auto'} onclick={() => setTheme('auto')}>Automatisch</button>
        </div>
      </div>
      <div class="row">
        <span>Animaties</span>
        <div class="seg" role="group" aria-label="Animaties">
          <button type="button" aria-pressed={motion !== 'off'} onclick={() => setMotion('on')}>Aan</button>
          <button type="button" aria-pressed={motion === 'off'} onclick={() => setMotion('off')}>Uit</button>
        </div>
      </div>
    </div>
  </section>

  <!-- Systeemstatus -->
  <section class="card rise" style="--i:4">
    <div class="card-head">
      <h2 class="card-title"><span class="ico" style="--accent: var(--g-battery)"><Icon icon={Activity} size={18} /></span>Systeemstatus</h2>
    </div>
    {#if health}
      <div class="sys">
        {#if health.uptime != null}<div><span>ESP32 aan sinds</span><b>{fmtUptime(health.uptime)}</b></div>{/if}
        {#if health.rssi != null}<div><span>Wifi-signaal</span><b>{health.rssi} dBm</b></div>{/if}
        {#if health.heap != null}<div><span>Vrij geheugen</span><b>{n0(health.heap / 1024)} KB</b></div>{/if}
        {#if health.fw}<div><span>Firmware</span><b>{health.fw}</b></div>{/if}
      </div>
      <ul class="sources">
        {#each Object.entries(health.sources) as [id, s] (id)}
          {@const age = app.now / 1000 - s.last}
          {@const st = !s.ok ? 'bad' : age > 3600 ? 'warn' : 'good'}
          <li>
            <span class="s-ico {st}"><Icon icon={st === 'good' ? CircleCheck : st === 'warn' ? CircleAlert : CircleX} size={18} /></span>
            <span class="s-name">{SOURCE_NAMES[id] ?? id}</span>
            <span class="s-meta">
              {st === 'good' ? 'Werkt' : st === 'warn' ? 'Verouderd' : 'Storing'} · {relativeAgo(s.last, app.now)}
              {#if s.calls != null && s.limit}· {s.calls}/{s.limit} calls{/if}
            </span>
            {#if s.err}<span class="s-err">{s.err}</span>{/if}
          </li>
        {/each}
      </ul>
    {:else}
      <p class="muted">Nog geen status ontvangen.</p>
    {/if}
  </section>
</div>

<style>
  .tips {
    display: grid;
    gap: 12px;
  }
  .tip {
    padding: 14px 16px;
    border-radius: 16px;
    background: var(--surface-2);
  }
  .tip h3 {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 16px;
    margin-bottom: 4px;
  }
  .tip p {
    color: var(--text-2);
    font-size: 15px;
  }
  .tip b {
    color: var(--text);
  }
  .tip :global(svg) {
    vertical-align: -2px;
  }
  .name-groups {
    display: grid;
    gap: 18px;
  }
  .name-group {
    display: grid;
    gap: 10px;
    align-content: start;
  }
  .name-group h3 {
    font-size: 15px;
    color: var(--text-2);
  }
  .intro {
    color: var(--text-2);
    font-size: 15px;
    margin-bottom: 16px;
    max-width: 70ch;
  }
  .periods {
    display: grid;
    gap: 14px;
  }
  .period {
    padding: 16px;
    border-radius: 16px;
    background: var(--surface-2);
    border: 1px solid var(--border);
    display: grid;
    gap: 14px;
  }
  .p-head {
    display: flex;
    align-items: flex-end;
    gap: 14px;
    flex-wrap: wrap;
  }
  .p-label {
    color: var(--text-3);
    font-size: 14px;
    padding-bottom: 12px;
    flex: 1;
  }
  .p-grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
    align-items: end;
  }
  .unit-in {
    position: relative;
    display: flex;
  }
  .unit-in input {
    width: 100%;
    padding-right: 64px !important;
  }
  .unit-in span {
    position: absolute;
    right: 12px;
    top: 50%;
    transform: translateY(-50%);
    font-size: 13px;
    color: var(--text-3);
    pointer-events: none;
  }
  .toggle {
    display: flex;
    align-items: center;
    gap: 10px;
    height: 44px;
    font-weight: 600;
    font-size: 14px;
    cursor: pointer;
  }
  .toggle input {
    position: absolute;
    opacity: 0;
  }
  .sw {
    width: 44px;
    height: 26px;
    border-radius: 999px;
    background: var(--surface-3);
    border: 1px solid var(--border-strong);
    position: relative;
    transition: background 0.25s;
    flex: none;
  }
  .sw::after {
    content: '';
    position: absolute;
    top: 2px;
    left: 2px;
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: var(--text);
    transition: transform 0.25s var(--ease);
  }
  .toggle input:checked + .sw {
    background: var(--s-good);
  }
  .toggle input:checked + .sw::after {
    transform: translateX(18px);
    background: #fff;
  }
  .toggle input:focus-visible + .sw {
    outline: 2px solid var(--g-house);
    outline-offset: 2px;
  }
  .actions {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    margin-top: 16px;
  }
  .err {
    color: var(--s-serious);
    margin-top: 10px;
    font-size: 14px;
  }
  .hint {
    color: var(--text-3);
    font-size: 13px;
    margin-top: 10px;
  }
  .conn {
    display: grid;
    gap: 12px;
    align-items: end;
  }
  .rows {
    display: grid;
    gap: 14px;
  }
  .row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    flex-wrap: wrap;
    font-weight: 600;
  }
  .sys {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 10px;
    margin-bottom: 16px;
  }
  .sys div {
    display: grid;
    padding: 12px 14px;
    border-radius: 14px;
    background: var(--surface-2);
  }
  .sys span {
    font-size: 12.5px;
    color: var(--text-3);
  }
  .sys b {
    font-size: 16px;
  }
  .sources {
    list-style: none;
    margin: 0;
    padding: 0;
    display: grid;
  }
  .sources li {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 2px 12px;
    padding: 11px 0;
    border-bottom: 1px solid var(--border);
    align-items: center;
  }
  .sources li:last-child {
    border-bottom: 0;
  }
  .s-ico {
    grid-row: span 2;
    display: grid;
  }
  .s-ico.good {
    color: var(--s-good-text);
  }
  .s-ico.warn {
    color: var(--s-warn);
  }
  .s-ico.bad {
    color: var(--s-crit);
  }
  .s-name {
    font-weight: 600;
    font-size: 14.5px;
  }
  .s-meta {
    font-size: 13px;
    color: var(--text-3);
  }
  .s-err {
    grid-column: 2;
    font-size: 13px;
    color: var(--s-serious);
  }
  @media (min-width: 720px) {
    .tips {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
    .name-groups {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
    .p-grid {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
    .conn {
      grid-template-columns: 1fr 1fr 2fr auto;
    }
    .sys {
      grid-template-columns: repeat(4, minmax(0, 1fr));
    }
    .sources {
      grid-template-columns: 1fr 1fr;
      column-gap: 28px;
    }
  }
</style>
