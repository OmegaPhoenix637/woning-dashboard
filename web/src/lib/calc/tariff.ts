import type { DayTotals, Settings, TariffPeriod } from '../types';
import { consumptionKwh } from './energy';

/** Voorbeeldtarieven. Echte ENGIE-tarieven stel je in op de pagina Instellingen. */
export const DEFAULT_SETTINGS: Settings = {
  version: 1,
  tariffs: [
    {
      from: '2025-01-01',
      importPrice: 0.27,
      exportPrice: 0.05,
      exportCost: 0,
      fixedPerMonth: 6.5,
      gasPrice: 1.35,
      netting: true,
    },
    {
      // Einde salderingsregeling in Nederland.
      from: '2027-01-01',
      importPrice: 0.27,
      exportPrice: 0.05,
      exportCost: 0,
      fixedPerMonth: 6.5,
      gasPrice: 1.35,
      netting: false,
    },
  ],
};

export function sortTariffs(t: TariffPeriod[]): TariffPeriod[] {
  return [...t].sort((a, b) => a.from.localeCompare(b.from));
}

/** Tarief dat op een datum geldt: de laatste periode met from <= datum. */
export function tariffFor(date: string, tariffs: TariffPeriod[]): TariffPeriod {
  const sorted = sortTariffs(tariffs);
  let cur = sorted[0];
  for (const t of sorted) if (t.from <= date) cur = t;
  return cur;
}

export interface Money {
  date: string;
  /** Wat de stroom deze dag kostte (variabel deel, zonder vaste kosten). */
  cost: number;
  /** Wat het zonder zonnepanelen en batterijen had gekost. */
  costWithout: number;
  saved: number;
  savedSolar: number;
  savedBattery: number;
  exportCredit: number;
  fixed: number;
  gasCost: number;
}

export const ZERO_MONEY: Omit<Money, 'date'> = {
  cost: 0,
  costWithout: 0,
  saved: 0,
  savedSolar: 0,
  savedBattery: 0,
  exportCredit: 0,
  fixed: 0,
  gasCost: 0,
};

/**
 * Bereken euro's per dag.
 *
 * Saldering: teruggeleverde kWh worden tegen de inkoopprijs verrekend, maar per kalenderjaar
 * nooit meer dan er is afgenomen. We houden dat lopend bij (jaar-tot-nu); wat daarboven komt
 * krijgt de terugleververgoeding. Geef daarom ALLE dagen van het jaar t/m de gewenste periode mee,
 * gesorteerd op datum.
 */
export function computeMoney(days: DayTotals[], tariffs: TariffPeriod[]): Map<string, Money> {
  const out = new Map<string, Money>();
  let year = '';
  let cumImport = 0;
  let cumNetted = 0;
  for (const d of days) {
    const y = d.date.slice(0, 4);
    if (y !== year) {
      year = y;
      cumImport = 0;
      cumNetted = 0;
    }
    const t = tariffFor(d.date, tariffs);
    cumImport += d.importKwh;
    let nettable = 0;
    if (t.netting) {
      nettable = Math.max(0, Math.min(d.exportKwh, cumImport - cumNetted));
      cumNetted += nettable;
    }
    const exportCredit =
      nettable * t.importPrice + (d.exportKwh - nettable) * t.exportPrice - d.exportKwh * t.exportCost;
    const cost = d.importKwh * t.importPrice - exportCredit;
    const costWithout = consumptionKwh(d) * t.importPrice;
    const saved = costWithout - cost;
    // Batterij: ontladen kWh vervangen netstroom; geladen kWh hadden anders opgeleverd wat
    // teruglevering oplevert (bij saldering = inkoopprijs, dan levert de accu weinig op).
    const exportValue = t.netting ? t.importPrice - t.exportCost : t.exportPrice - t.exportCost;
    const savedBattery = d.batDischargeKwh * t.importPrice - d.batChargeKwh * exportValue;
    const fixed = (t.fixedPerMonth * 12) / 365;
    out.set(d.date, {
      date: d.date,
      cost,
      costWithout,
      saved,
      savedBattery,
      savedSolar: saved - savedBattery,
      exportCredit,
      fixed,
      gasCost: (d.gasM3 ?? 0) * t.gasPrice,
    });
  }
  return out;
}

export function sumMoney(list: (Money | undefined)[]): Omit<Money, 'date'> {
  const s = { ...ZERO_MONEY };
  for (const m of list) {
    if (!m) continue;
    s.cost += m.cost;
    s.costWithout += m.costWithout;
    s.saved += m.saved;
    s.savedSolar += m.savedSolar;
    s.savedBattery += m.savedBattery;
    s.exportCredit += m.exportCredit;
    s.fixed += m.fixed;
    s.gasCost += m.gasCost;
  }
  return s;
}
