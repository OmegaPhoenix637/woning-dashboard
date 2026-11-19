// Demo-modus: realistische nepdata voor ruim 2 jaar, volledig in de browser berekend.
// Hiermee is de hele site te bekijken en te testen voordat de ESP32 data levert.
import type {
  ApplianceRun,
  DayData,
  DayTotals,
  LiveData,
  Settings,
  YearSummary,
} from '../types';
import { DEVICES, MODE_CODE } from '../types';
import { addDays, dayOfYear, dayStart, parseDate, stepsInDay, todayIso } from '../date';
import { DEFAULT_SETTINGS } from '../calc/tariff';
import type { DataSource } from './source';

// Ruim 2 jaar, zodat ook hele jaren met elkaar te vergelijken zijn.
const HISTORY_DAYS = 760;
const STEP = 300;
const CAP_WH = 5120;
const MAX_CHARGE = 2500;
const MAX_DISCHARGE = 800;
const MIN_SOC = 10;

function hashStr(s: string): number {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}

function mulberry32(seed: number) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

const clamp = (v: number, lo: number, hi: number) => Math.max(lo, Math.min(hi, v));
const r1 = (v: number) => Math.round(v * 10) / 10;

interface CarryState {
  soc: [number, number];
  rooms: Record<string, number>;
  meterImport: number;
  meterExport: number;
  meterSolar: number;
  meterGas: number;
  evCar: string;
}

interface SimResult {
  day: DayData;
  end: CarryState;
}

const WASH_PROGRAMS = ['Katoen 40°', 'Synthetisch 30°', 'Eco 40-60', 'Katoen 60°', 'Fijne was 30°'];
const DISH_PROGRAMS = ['Eco 50°', 'Auto 45-65°', 'Intensief 70°'];

interface RoomCfg {
  id: string;
  size: number;
  heatTarget: number;
  coolTarget: number;
  windows: (wkd: boolean) => [number, number][];
  coolCapable: boolean;
}

const ROOMS: RoomCfg[] = [
  { id: 'woonkamer', size: 1, heatTarget: 20.5, coolTarget: 22, coolCapable: true, windows: (w) => (w ? [[8, 23.5]] : [[7, 23]]) },
  { id: 'keuken', size: 0.6, heatTarget: 20, coolTarget: 22.5, coolCapable: true, windows: () => [[7, 20.5]] },
  { id: 'studio', size: 0.6, heatTarget: 19.5, coolTarget: 21.5, coolCapable: true, windows: (w) => (w ? [[10, 23.5]] : [[15, 23.5]]) },
  { id: 'slaap1', size: 0.5, heatTarget: 19, coolTarget: 21, coolCapable: true, windows: () => [[6.5, 7.5], [21.5, 23.5]] },
  { id: 'slaap2', size: 0.5, heatTarget: 19.5, coolTarget: 21, coolCapable: true, windows: () => [[6.5, 7.5], [19, 22.5]] },
  { id: 'slaap3', size: 0.7, heatTarget: 19, coolTarget: 21, coolCapable: true, windows: () => [[15.5, 24]] },
  { id: 'slaap4', size: 0.55, heatTarget: 19.5, coolTarget: 20, coolCapable: true, windows: () => [[6.5, 7.5], [21, 23.5]] },
];

const inWin = (h: number, wins: [number, number][]) => wins.some(([a, b]) => h >= a && h < b);

function isDst(date: string): boolean {
  const d = parseDate(date);
  const noon = new Date(d.getFullYear(), d.getMonth(), d.getDate(), 12);
  const jan = new Date(d.getFullYear(), 0, 1, 12);
  return noon.getTimezoneOffset() < jan.getTimezoneOffset();
}

function simulateDay(date: string, carry: CarryState, limitSteps?: number): SimResult {
  const n = stepsInDay(date);
  const start = dayStart(date);
  const r = mulberry32(hashStr('mw:' + date));
  const doy = dayOfYear(date);
  const season = Math.cos((2 * Math.PI * (doy - 172)) / 365); // +1 juni, -1 december
  const dow = parseDate(date).getDay();
  const weekend = dow === 0 || dow === 6;

  const cloudDay = clamp(0.6 - 0.22 * season + (r() - 0.5) * 0.95, 0.03, 0.97);
  const tAvg = 11 + 6.8 * season + (r() - 0.5) * 6;
  const tAmp = 2.2 + 3.6 * (1 - cloudDay);
  const dayLen = 12.2 + 4.4 * season;
  const solarNoon = 12.67 + (isDst(date) ? 1 : 0);
  const peakClear = 7800 * (0.33 + (0.67 * (season + 1)) / 2);
  const cp = [r() * 6, r() * 6, r() * 6];

  // Pelletkachel op koude avonden
  const pelletOn = tAvg < 9 && r() < 0.78;
  const pelletStart = 16 + r() * 1.5;
  const pelletEnd = 22 + r() * 1.3;
  const pelletLevel = tAvg < 4 ? 4 : 3;
  const euromSlaap = tAvg < 8.5;
  const euromBad = tAvg < 12;

  // Wasbeurten
  const runs: ApplianceRun[] = [];
  if (r() < (weekend ? 0.8 : 0.45)) {
    const dur = 100 + r() * 45;
    const st = cloudDay < 0.5 ? 10.5 + r() * 2 : 8.5 + r() * 9;
    runs.push({ id: 'wasmachine', start: Math.round(st * 60), end: Math.round(st * 60 + dur), program: WASH_PROGRAMS[Math.floor(r() * WASH_PROGRAMS.length)] });
  }
  if (r() < 0.75) {
    const dur = 175 + r() * 40;
    const st = weekend && cloudDay < 0.45 ? 12.5 + r() : 19.4 + r();
    runs.push({ id: 'vaatwasser', start: Math.round(st * 60), end: Math.round(st * 60 + dur), program: DISH_PROGRAMS[Math.floor(r() * DISH_PROGRAMS.length)] });
  }

  // Gas: alleen als back-up op hele koude dagen
  const gasDay = tAvg < 2 ? (2 - tAvg) * 0.9 + r() * 0.6 : r() < 0.04 ? r() * 0.3 : 0;

  const ch: Record<string, (number | null)[]> = {};
  const mk = (k: string) => (ch[k] = new Array(n).fill(null));
  ['grid_w', 'grid_max', 'solar_w', 'bat1_w', 'bat2_w', 'bat1_soc', 'bat2_soc', 't_out', 'cloud'].forEach(mk);
  for (const rm of ROOMS) ['_t', '_set', '_mode'].forEach((s) => mk('ac_' + rm.id + s));
  ['pellet_t', 'pellet_on', 'eurom_slaap_on', 'eurom_bad_on', 'eurom_slaap_t', 'eurom_bad_t'].forEach(mk);

  const soc = [...carry.soc] as [number, number];
  const rooms = { ...carry.rooms };
  const climateWh: Record<string, number> = Object.fromEntries(ROOMS.map((x) => [x.id, 0]));
  const heaterSteps: Record<string, number> = { pellet: 0, eurom_slaap: 0, eurom_bad: 0 };
  let euromSlaapT = rooms.slaap4 ?? 17;
  let euromBadT = rooms.bad ?? 18;
  let imp = 0,
    exp = 0,
    sol = 0,
    chg = 0,
    dis = 0,
    peakI = 0,
    peakE = 0;
  let tMin = Infinity,
    tMax = -Infinity,
    tSum = 0,
    cSum = 0,
    cnt = 0;

  const last = limitSteps == null ? n : Math.min(n, limitSteps);
  for (let i = 0; i < last; i++) {
    const h = (i * STEP) / 3600 + (n === 276 && i >= 24 ? 1 : 0) - (n === 300 && i >= 36 ? 1 : 0);
    const tOut = tAvg + tAmp * Math.sin((2 * Math.PI * (h - 9)) / 24) + (r() - 0.5) * 0.4;
    const cloud = clamp(
      cloudDay + 0.28 * Math.sin(h * 1.3 + cp[0]) + 0.15 * Math.sin(h * 3.1 + cp[1]) + (r() - 0.5) * 0.25,
      0,
      1,
    );
    const x = (h - solarNoon) / (dayLen / 2);
    const pvDc = Math.abs(x) < 1 ? peakClear * Math.pow(Math.cos((x * Math.PI) / 2), 1.4) * (1 - 0.8 * cloud) * (0.96 + r() * 0.08) : 0;
    const pv = Math.min(pvDc, 6900); // SB3.0 + SB4.0: samen max ~7 kW AC

    // Huishouden
    let load = 255 + (i % 7 < 3 ? 75 : 0);
    if (weekend ? h >= 8 && h < 10.2 : h >= 6.6 && h < 8.4) load += 420 + (r() < 0.25 ? 1500 : 0);
    if (weekend && h >= 11 && h < 17) load += 300 + r() * 250;
    if (h >= 12 && h < 13) load += 180;
    if (h >= 17.4 && h < 19) load += 1100 + r() * 1000;
    if (h >= 19 && h < 23.2) load += 360 + (220 * (1 - season)) / 2 + r() * 80;
    if (r() < 0.03) load += 1400;

    for (const run of runs) {
      const m = h * 60;
      if (m >= run.start && m < run.end) {
        const f = (m - run.start) / (run.end - run.start);
        if (run.id === 'wasmachine') load += f < 0.18 ? 1950 : f < 0.86 ? 150 + (i % 3 === 0 ? 260 : 0) : 540;
        else load += f < 0.12 || (f > 0.62 && f < 0.72) ? 2050 : 75;
      }
    }

    // Pelletkachel (elektrisch: ontsteking 300 W, daarna ~90 W)
    let pLevel = 0;
    if (pelletOn && h >= pelletStart && h < pelletEnd) {
      pLevel = pelletLevel;
      const sinceStart = (h - pelletStart) * 60;
      load += sinceStart < 12 ? 320 : 90;
      heaterSteps.pellet++;
    }

    // Eurom
    const ecOn = euromSlaap && h >= 21 && h < 22.6 ? 1 : 0;
    const ebOn = euromBad && h >= 6.4 && h < 7.8 ? 1 : 0;
    if (ecOn) {
      load += i % 3 !== 0 ? 1000 : 0;
      heaterSteps.eurom_slaap++;
    }
    if (ebOn) {
      load += i % 4 !== 0 ? 1500 : 0;
      heaterSteps.eurom_bad++;
    }
    euromSlaapT += ((ecOn ? 20.5 : 15.5 + 0.3 * (tOut - 5)) - euromSlaapT) * (ecOn ? 0.2 : 0.03);
    euromBadT += ((ebOn ? 22 : 16.5 + 0.3 * (tOut - 5)) - euromBadT) * (ebOn ? 0.25 : 0.03);

    // Airco's
    for (const rm of ROOMS) {
      const wins = rm.windows(weekend);
      let mode: 'off' | 'heat' | 'cool' = 'off';
      if (inWin(h, wins)) {
        if (tAvg < 15) mode = 'heat';
        else if (rm.coolCapable && tAvg > 18 && tOut > 23 && h > 11) mode = 'cool';
      }
      if (rm.id === 'woonkamer' && pLevel > 0 && h > pelletStart + 0.4) mode = 'off';
      const gain = (pv / 9000) * (rm.id === 'slaap3' ? 3 : 1.5);
      const free = 15.5 + 0.35 * (tOut - 5) + gain + (rm.id === 'woonkamer' && pLevel ? 6.5 : 0);
      let t = rooms[rm.id] ?? free;
      const target = mode === 'heat' ? rm.heatTarget : mode === 'cool' ? rm.coolTarget : null;
      if (target != null) {
        t += (target - t) * 0.18 + (r() - 0.5) * 0.12;
        const p =
          mode === 'heat'
            ? clamp(55 * (target - tOut) * rm.size, 120, 1500)
            : clamp(70 * (tOut - target) * rm.size, 150, 1300);
        const w = p * (0.85 + r() * 0.3);
        load += w;
        climateWh[rm.id] += (w * STEP) / 3600;
      } else {
        t += (free - t) * 0.03 + (r() - 0.5) * 0.05;
      }
      rooms[rm.id] = t;
      ch['ac_' + rm.id + '_t'][i] = r1(t);
      ch['ac_' + rm.id + '_set'][i] = target;
      ch['ac_' + rm.id + '_mode'][i] = MODE_CODE[mode];
    }

    // Batterijen: nul-op-de-meter
    const net = load - pv;
    const bw: [number, number] = [0, 0];
    if (net < -30) {
      const surplus = -net - 20;
      const room = (b: number) => ((100 - soc[b]) / 100) * CAP_WH * (3600 / STEP);
      bw[0] = Math.max(0, Math.min(MAX_CHARGE, surplus * 0.55, room(0)));
      bw[1] = Math.max(0, Math.min(MAX_CHARGE, surplus - bw[0], room(1)));
    } else if (net > 30) {
      let need = net - 25;
      for (let b = 0; b < 2; b++) {
        const avail = soc[b] > MIN_SOC ? ((soc[b] - MIN_SOC) / 100) * CAP_WH * (3600 / STEP) : 0;
        const d = Math.min(MAX_DISCHARGE, b === 0 ? need / 2 + 40 : need, avail);
        bw[b] = -Math.max(0, d);
        need -= Math.max(0, d);
      }
    }
    for (let b = 0; b < 2; b++) {
      if (bw[b] !== 0) bw[b] += (r() - 0.5) * 20;
      const dWh = (bw[b] * STEP) / 3600;
      soc[b] = clamp(soc[b] + ((dWh > 0 ? dWh * 0.965 : dWh / 0.965) / CAP_WH) * 100, 0, 100);
    }
    const grid = load - pv + bw[0] + bw[1];
    const gridMax = grid > 0 ? grid + r() * 450 + (r() < 0.2 ? 1200 : 0) : grid;

    ch.grid_w[i] = Math.round(grid);
    ch.grid_max[i] = Math.round(gridMax);
    ch.solar_w[i] = Math.round(pv);
    ch.bat1_w[i] = Math.round(bw[0]);
    ch.bat2_w[i] = Math.round(bw[1]);
    ch.bat1_soc[i] = r1(soc[0]);
    ch.bat2_soc[i] = r1(soc[1]);
    ch.t_out[i] = r1(tOut);
    ch.cloud[i] = Math.round(cloud * 100);
    ch.pellet_on[i] = pLevel;
    ch.pellet_t[i] = r1(rooms.woonkamer);
    ch.eurom_slaap_on[i] = ecOn;
    ch.eurom_bad_on[i] = ebOn;
    ch.eurom_slaap_t[i] = r1(euromSlaapT);
    ch.eurom_bad_t[i] = r1(euromBadT);

    const k = STEP / 3600 / 1000;
    if (grid > 0) imp += grid * k;
    else exp += -grid * k;
    sol += pv * k;
    for (const v of bw) {
      if (v > 0) chg += v * k;
      else dis += -v * k;
    }
    peakI = Math.max(peakI, gridMax);
    peakE = Math.max(peakE, -grid);
    tMin = Math.min(tMin, tOut);
    tMax = Math.max(tMax, tOut);
    tSum += tOut;
    cSum += cloud;
    cnt++;
  }
  rooms.bad = euromBadT;

  const nowMin = limitSteps == null ? Infinity : (last * STEP) / 60;
  const totals: DayTotals = {
    date,
    importKwh: imp,
    exportKwh: exp,
    solarKwh: sol,
    batChargeKwh: chg,
    batDischargeKwh: dis,
    gasM3: Math.round(gasDay * (last / n) * 100) / 100,
    evKwh: null,
    peakImportW: Math.round(peakI),
    peakExportW: Math.round(peakE),
    tOutMin: cnt ? r1(tMin) : null,
    tOutMax: cnt ? r1(tMax) : null,
    tOutAvg: cnt ? r1(tSum / cnt) : null,
    cloudAvg: cnt ? Math.round((cSum / cnt) * 100) : null,
    climateKwh: Object.fromEntries(Object.entries(climateWh).map(([k2, v]) => [k2, Math.round(v / 10) / 100])),
    heaterHours: Object.fromEntries(Object.entries(heaterSteps).map(([k2, v]) => [k2, (v * STEP) / 3600])),
    runs: runs.filter((x) => x.start < nowMin),
  };

  return {
    day: { date, start, step: STEP, n, ch, totals },
    end: {
      soc,
      rooms,
      meterImport: carry.meterImport + imp,
      meterExport: carry.meterExport + exp,
      meterSolar: carry.meterSolar + sol,
      meterGas: carry.meterGas + gasDay * (last / n),
      evCar: carry.evCar,
    },
  };
}

/** Bouwt de hele demo-geschiedenis één keer op (dag voor dag, zodat batterijen doorlopen). */
class MockWorld {
  readonly first: string;
  private carryAt = new Map<string, CarryState>();
  private totals = new Map<string, DayTotals>();
  private full = new Map<string, DayData>();
  private builtUpTo: string;

  constructor() {
    this.first = addDays(todayIso(), -HISTORY_DAYS);
    this.builtUpTo = addDays(this.first, -1);
    this.carryAt.set(this.first, {
      soc: [42, 38],
      rooms: {},
      meterImport: 18342.4,
      meterExport: 9120.7,
      meterSolar: 21410.2,
      meterGas: 4120.3,
      evCar: 'Auto 1',
    });
  }

  /** Simuleer alle volledige dagen t/m gisteren. */
  private build() {
    const yesterday = addDays(todayIso(), -1);
    while (this.builtUpTo < yesterday) {
      const d = addDays(this.builtUpTo, 1);
      const res = simulateDay(d, this.carryAt.get(d)!);
      this.totals.set(d, res.day.totals);
      this.carryAt.set(addDays(d, 1), res.end);
      this.builtUpTo = d;
    }
  }

  day(date: string): DayData | null {
    this.build();
    const today = todayIso();
    if (date < this.first || date > today) return null;
    if (date === today) return this.today().day;
    let d = this.full.get(date);
    if (!d) {
      d = simulateDay(date, this.carryAt.get(date)!).day;
      if (this.full.size > 40) this.full.delete(this.full.keys().next().value!);
      this.full.set(date, d);
    }
    return d;
  }

  today(): SimResult {
    this.build();
    const t = todayIso();
    const steps = Math.floor((Date.now() / 1000 - dayStart(t)) / STEP) + 1;
    return simulateDay(t, this.carryAt.get(t)!, steps);
  }

  year(y: number): YearSummary {
    this.build();
    const days: DayTotals[] = [];
    for (const [d, tot] of this.totals) if (d.startsWith(String(y))) days.push(tot);
    const t = todayIso();
    if (t.startsWith(String(y))) days.push(this.today().day.totals);
    days.sort((a, b) => a.date.localeCompare(b.date));
    return { year: y, days };
  }
}

function lastVal(a: (number | null)[] | undefined): number | null {
  if (!a) return null;
  for (let i = a.length - 1; i >= 0; i--) if (a[i] != null) return a[i];
  return null;
}

export class DemoSource implements DataSource {
  readonly kind = 'demo' as const;
  private world = new MockWorld();
  private settingsLocal: Settings = DEFAULT_SETTINGS;

  constructor() {
    try {
      const raw = localStorage.getItem('mw.demo.settings');
      if (raw) this.settingsLocal = { ...DEFAULT_SETTINGS, ...JSON.parse(raw) };
    } catch {
      /* geen opslag beschikbaar */
    }
  }

  get firstDate() {
    return this.world.first;
  }

  async live(): Promise<LiveData> {
    const { day, end } = this.world.today();
    const now = Date.now() / 1000;
    const jit = (v: number, a: number) => v + (Math.random() - 0.5) * a;
    const nowMin = (now - day.start) / 60;
    const grid = lastVal(day.ch.grid_w) ?? 0;
    const solar = lastVal(day.ch.solar_w) ?? 0;
    const b1 = lastVal(day.ch.bat1_w) ?? 0;
    const b2 = lastVal(day.ch.bat2_w) ?? 0;
    const liveSolar = solar > 0 ? Math.max(0, jit(solar, solar * 0.06)) : 0;
    const liveB1 = b1 !== 0 ? jit(b1, 30) : 0;
    const liveB2 = b2 !== 0 ? jit(b2, 30) : 0;
    const liveGrid = jit(grid, 40) - (liveSolar - solar) + (liveB1 - b1) + (liveB2 - b2);
    const tot = day.totals;
    const oc = Math.min(200, Math.floor(nowMin / 10) + 3);

    return {
      ts: Math.floor(now - 20),
      grid: {
        w: Math.round(liveGrid),
        importKwh: end.meterImport,
        exportKwh: end.meterExport,
        gasM3: Math.round(end.meterGas * 1000) / 1000,
        l1: Math.round(liveGrid * 0.45),
        l2: Math.round(liveGrid * 0.3),
        l3: Math.round(liveGrid * 0.25),
        voltage: r1(jit(231.5, 3)),
      },
      solar: {
        w: Math.round(liveSolar),
        totalKwh: end.meterSolar,
        inverters: [
          { id: 'sma1', name: 'SMA SB3.0', w: Math.round((liveSolar * 3) / 7) },
          { id: 'sma2', name: 'SMA SB4.0', w: Math.round((liveSolar * 4) / 7) },
        ],
      },
      ev: null,
      batteries: DEVICES.batteries.map((b, i) => ({
        id: b.id,
        name: b.name,
        capacityKwh: b.capacityKwh,
        soc: end.soc[i],
        w: Math.round(i === 0 ? liveB1 : liveB2),
      })),
      climate: DEVICES.climate.map((c) => {
        const mode = (lastVal(day.ch['ac_' + c.id + '_mode']) ?? 0) as number;
        const m = (['off', 'heat', 'cool', 'auto', 'dry', 'fan'] as const)[mode];
        return {
          id: c.id,
          name: c.name,
          temp: lastVal(day.ch['ac_' + c.id + '_t']),
          target: lastVal(day.ch['ac_' + c.id + '_set']),
          mode: m,
          on: m !== 'off',
          kwhToday: tot.climateKwh[c.id] ?? 0,
        };
      }),
      heaters: [
        {
          id: 'pellet',
          name: DEVICES.heaters[0].name,
          kind: 'pellet',
          state: (lastVal(day.ch.pellet_on) ?? 0) > 0 ? 'on' : 'off',
          temp: lastVal(day.ch.pellet_t),
          target: 22,
          level: lastVal(day.ch.pellet_on),
        },
        {
          id: 'eurom_slaap',
          name: DEVICES.heaters[1].name,
          kind: 'eurom',
          state: lastVal(day.ch.eurom_slaap_on) ? 'on' : 'off',
          temp: lastVal(day.ch.eurom_slaap_t),
          target: 20.5,
        },
        {
          id: 'eurom_bad',
          name: DEVICES.heaters[2].name,
          kind: 'eurom',
          state: lastVal(day.ch.eurom_bad_on) ? 'on' : 'off',
          temp: lastVal(day.ch.eurom_bad_t),
          target: 22,
        },
      ],
      appliances: DEVICES.appliances.map((a) => {
        // In de demo staat de geplande eindtijd al in de run.
        const run = tot.runs.filter((x) => x.id === a.id).at(-1);
        if (run && nowMin >= run.start && nowMin < run.end) {
          return {
            ...a,
            state: 'running' as const,
            program: run.program,
            remainingMin: Math.round(run.end - nowMin),
            progress: Math.round(((nowMin - run.start) / (run.end - run.start)) * 100),
          };
        }
        if (run && nowMin >= run.end && nowMin < run.end + 60) {
          return { ...a, state: 'finished' as const, program: run.program, remainingMin: 0, progress: 100 };
        }
        return { ...a, state: 'idle' as const, program: null, remainingMin: null, progress: null };
      }),
      weather: { temp: lastVal(day.ch.t_out), cloud: lastVal(day.ch.cloud) },
      health: {
        sources: {
          p1: { ok: true, last: Math.floor(now - 8) },
          sma: { ok: true, last: Math.floor(now - 25) },
          bat1: { ok: true, last: Math.floor(now - 22) },
          bat2: { ok: true, last: Math.floor(now - 21) },
          onecta: { ok: true, last: Math.floor(now - 240), calls: oc, limit: 200 },
          homeconnect: { ok: true, last: Math.floor(now - 130), calls: Math.floor(nowMin / 7), limit: 1000 },
          aguaiot: { ok: true, last: Math.floor(now - 170) },
          eurom: { ok: true, last: Math.floor(now - 200) },
          weather: { ok: true, last: Math.floor(now - 1500) },
          github: { ok: true, last: Math.floor(now - 20) },
        },
        rssi: -61,
        heap: 148320,
        uptime: 86400 * 6 + 3600 * 4,
        fw: 'demo',
      },
    };
  }

  async today(): Promise<DayData> {
    return this.world.today().day;
  }

  async day(date: string): Promise<DayData | null> {
    return this.world.day(date);
  }

  async year(y: number): Promise<YearSummary> {
    return this.world.year(y);
  }

  async settings(): Promise<Settings> {
    return this.settingsLocal;
  }

  async saveSettings(s: Settings): Promise<void> {
    this.settingsLocal = s;
    try {
      localStorage.setItem('mw.demo.settings', JSON.stringify(s));
    } catch {
      /* negeren */
    }
  }
}
