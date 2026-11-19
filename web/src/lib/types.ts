// Gedeeld datacontract tussen ESP32-firmware en website.
// Zie ../../docs/dataformaat.md voor de uitleg per veld.
// Tekenconventies: grid.w > 0 = afname van het net, < 0 = teruglevering.
//                  battery.w > 0 = laden, < 0 = ontladen.

export type ClimateMode = 'off' | 'heat' | 'cool' | 'auto' | 'dry' | 'fan';
export const MODE_CODE: Record<ClimateMode, number> = { off: 0, heat: 1, cool: 2, auto: 3, dry: 4, fan: 5 };
export const CODE_MODE: ClimateMode[] = ['off', 'heat', 'cool', 'auto', 'dry', 'fan'];

export interface ClimateUnit {
  id: string;
  name: string;
  temp: number | null; // kamertemperatuur °C
  target: number | null; // ingestelde temperatuur °C
  mode: ClimateMode;
  on: boolean;
  kwhToday: number | null;
}

export type HeaterState = 'off' | 'starting' | 'on' | 'cooling' | 'unknown';
export interface Heater {
  id: string;
  name: string;
  kind: 'pellet' | 'eurom';
  state: HeaterState;
  temp: number | null;
  target: number | null;
  level?: number | null; // vermogensstand (pelletkachel 1-5)
}

export type ApplianceState = 'off' | 'idle' | 'running' | 'paused' | 'finished';
export interface Appliance {
  id: string;
  name: string;
  kind: 'washer' | 'dishwasher';
  state: ApplianceState;
  program?: string | null;
  remainingMin?: number | null;
  progress?: number | null; // 0-100
}

export interface Battery {
  id: string;
  name: string;
  soc: number; // %
  w: number; // + laden, - ontladen
  capacityKwh: number;
}

export interface SourceHealth {
  ok: boolean;
  last: number; // unix s van laatste geslaagde update
  err?: string | null;
  calls?: number | null; // API-calls vandaag
  limit?: number | null; // daglimiet
}

export interface LiveData {
  ts: number; // unix s
  grid: {
    w: number;
    importKwh: number; // meterstand totaal
    exportKwh: number;
    gasM3?: number | null;
    l1?: number | null;
    l2?: number | null;
    l3?: number | null;
    voltage?: number | null;
  };
  solar: { w: number; totalKwh: number; inverters?: { id: string; name: string; w: number }[] };
  batteries: Battery[];
  /** Laadpaal (optioneel: alleen als die uitleesbaar is). w is onderdeel van het huisverbruik. */
  ev?: EvCharger | null;
  climate: ClimateUnit[];
  heaters: Heater[];
  appliances: Appliance[];
  weather: { temp: number | null; cloud: number | null };
  health: {
    sources: Record<string, SourceHealth>;
    rssi?: number | null;
    heap?: number | null;
    uptime?: number | null;
    fw?: string | null;
  };
}

export type EvState = 'idle' | 'connected' | 'charging' | 'done' | 'unknown';
export interface EvCharger {
  state: EvState;
  w: number;
  sessionKwh: number | null;
  kwhToday: number | null;
  car?: string | null;
}

export interface ApplianceRun {
  id: string; // appliance id
  start: number; // minuten na middernacht
  end: number;
  program?: string | null;
}

export interface DayTotals {
  date: string; // YYYY-MM-DD (lokale tijd)
  importKwh: number;
  exportKwh: number;
  solarKwh: number;
  batChargeKwh: number;
  batDischargeKwh: number;
  gasM3?: number | null;
  evKwh?: number | null;
  peakImportW: number;
  peakExportW: number;
  tOutMin: number | null;
  tOutMax: number | null;
  tOutAvg: number | null;
  cloudAvg: number | null; // 0-100 %
  climateKwh: Record<string, number>;
  heaterHours: Record<string, number>;
  runs: ApplianceRun[];
}

/** Eén dag op 5-minutenresolutie, kolomsgewijs opgeslagen. */
export interface DayData {
  date: string;
  start: number; // unix s van 00:00 lokale tijd
  step: number; // seconden per punt (300)
  n: number; // aantal punten (288, of 276/300 bij zomer-/wintertijd)
  ch: Record<string, (number | null)[]>;
  totals: DayTotals;
}

export interface YearSummary {
  year: number;
  days: DayTotals[];
}

export interface TariffPeriod {
  from: string; // YYYY-MM-DD, geldig vanaf
  importPrice: number; // €/kWh inkoop, all-in
  exportPrice: number; // €/kWh terugleververgoeding
  exportCost: number; // €/kWh terugleverkosten
  fixedPerMonth: number; // vaste leveringskosten + netbeheer - vermindering energiebelasting
  gasPrice: number; // €/m³
  netting: boolean; // salderingsregeling actief
}

export interface Settings {
  version: 1;
  tariffs: TariffPeriod[];
  /** Eigen namen voor kamers en apparaten (alleen in de privé-repo, niet in de openbare code). */
  names?: Record<string, string>;
}

export interface DeviceMeta {
  climate: { id: string; name: string }[];
  heaters: { id: string; name: string; kind: 'pellet' | 'eurom' }[];
  appliances: { id: string; name: string; kind: 'washer' | 'dishwasher' }[];
  batteries: { id: string; name: string; capacityKwh: number }[];
}

// Neutrale id's en standaardnamen. De echte namen (bv. "Slaapkamer van ...") stel je in op de pagina
// Instellingen; die worden in de privé data-repo bewaard en staan dus niet in deze openbare code.
export const DEVICES: DeviceMeta = {
  climate: [
    { id: 'woonkamer', name: 'Woonkamer' },
    { id: 'keuken', name: 'Keuken' },
    { id: 'studio', name: 'Studio' },
    { id: 'slaap1', name: 'Slaapkamer 1' },
    { id: 'slaap2', name: 'Slaapkamer 2' },
    { id: 'slaap3', name: 'Slaapkamer 3' },
    { id: 'slaap4', name: 'Slaapkamer 4' },
  ],
  heaters: [
    { id: 'pellet', name: 'Pelletkachel', kind: 'pellet' },
    { id: 'eurom_slaap', name: 'Eurom slaapkamer', kind: 'eurom' },
    { id: 'eurom_bad', name: 'Eurom badkamer', kind: 'eurom' },
  ],
  appliances: [
    { id: 'wasmachine', name: 'Wasmachine', kind: 'washer' },
    { id: 'vaatwasser', name: 'Vaatwasser', kind: 'dishwasher' },
  ],
  batteries: [
    { id: 'bat1', name: 'Batterij 1', capacityKwh: 5.12 },
    { id: 'bat2', name: 'Batterij 2', capacityKwh: 5.12 },
  ],
};

export const SOURCE_NAMES: Record<string, string> = {
  p1: 'HomeWizard P1',
  sma: 'SMA omvormer',
  bat1: 'Marstek 1',
  bat2: 'Marstek 2',
  onecta: 'Daikin Onecta',
  homeconnect: 'Home Connect',
  aguaiot: 'Nordic Fire',
  eurom: 'Eurom',
  weather: 'Weer (Open-Meteo)',
  github: 'GitHub-upload',
  ev: 'Laadpaal',
};
