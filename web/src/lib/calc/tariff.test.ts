import { describe, expect, it } from 'vitest';
import { computeMoney, tariffFor } from './tariff';
import type { DayTotals, TariffPeriod } from '../types';

const base: TariffPeriod = { from: '2026-01-01', importPrice: 0.3, exportPrice: 0.05, exportCost: 0, fixedPerMonth: 0, gasPrice: 1, netting: true };
const noNet: TariffPeriod = { ...base, from: '2027-01-01', netting: false };

function day(date: string, p: Partial<DayTotals> = {}): DayTotals {
  return {
    date,
    importKwh: 0,
    exportKwh: 0,
    solarKwh: 0,
    batChargeKwh: 0,
    batDischargeKwh: 0,
    peakImportW: 0,
    peakExportW: 0,
    tOutMin: null,
    tOutMax: null,
    tOutAvg: null,
    cloudAvg: null,
    climateKwh: {},
    heaterHours: {},
    runs: [],
    ...p,
  };
}

describe('tariffFor', () => {
  it('kiest de laatste periode met from <= datum', () => {
    expect(tariffFor('2026-12-31', [noNet, base]).netting).toBe(true);
    expect(tariffFor('2027-01-01', [noNet, base]).netting).toBe(false);
    expect(tariffFor('2025-06-01', [base, noNet]).from).toBe('2026-01-01'); // vóór eerste periode: eerste
  });
});

describe('computeMoney', () => {
  it('zonder zon of accu is besparing 0', () => {
    const m = computeMoney([day('2026-03-01', { importKwh: 10 })], [base]).get('2026-03-01')!;
    expect(m.cost).toBeCloseTo(3);
    expect(m.costWithout).toBeCloseTo(3);
    expect(m.saved).toBeCloseTo(0);
  });

  it('saldering: export tegen inkoopprijs tot maximaal de import van het jaar', () => {
    // 10 kWh import, 4 kWh export, 8 kWh zon → verbruik 14
    const m = computeMoney([day('2026-03-01', { importKwh: 10, exportKwh: 4, solarKwh: 8 })], [base]).get('2026-03-01')!;
    expect(m.exportCredit).toBeCloseTo(4 * 0.3);
    expect(m.cost).toBeCloseTo(10 * 0.3 - 4 * 0.3);
    expect(m.costWithout).toBeCloseTo(14 * 0.3);
    expect(m.saved).toBeCloseTo(8 * 0.3);
  });

  it('saldering: overschot boven jaarimport krijgt terugleververgoeding', () => {
    const m = computeMoney([day('2026-06-01', { importKwh: 2, exportKwh: 10, solarKwh: 20 })], [base]).get('2026-06-01')!;
    expect(m.exportCredit).toBeCloseTo(2 * 0.3 + 8 * 0.05);
  });

  it('jaar-tot-nu: winterimport telt mee voor zomerexport', () => {
    const days = [day('2026-01-10', { importKwh: 20 }), day('2026-06-01', { importKwh: 0, exportKwh: 15, solarKwh: 20 })];
    const m = computeMoney(days, [base]).get('2026-06-01')!;
    expect(m.exportCredit).toBeCloseTo(15 * 0.3);
  });

  it('teller reset op 1 januari', () => {
    const days = [day('2026-12-31', { importKwh: 50 }), day('2027-01-01', { exportKwh: 5, solarKwh: 5 })];
    const m = computeMoney(days, [base, { ...noNet, netting: true }]).get('2027-01-01')!;
    expect(m.exportCredit).toBeCloseTo(5 * 0.05); // nieuw jaar: nog geen import om tegen te salderen
  });

  it('na einde saldering: export alleen terugleververgoeding minus terugleverkosten', () => {
    const t = { ...noNet, exportCost: 0.02 };
    const m = computeMoney([day('2027-03-01', { importKwh: 10, exportKwh: 4, solarKwh: 8 })], [base, t]).get('2027-03-01')!;
    expect(m.exportCredit).toBeCloseTo(4 * 0.05 - 4 * 0.02);
  });

  it('batterij levert pas echt op zonder saldering', () => {
    const d = { importKwh: 5, exportKwh: 0, solarKwh: 10, batChargeKwh: 4, batDischargeKwh: 3.6 };
    const withNet = computeMoney([day('2026-04-01', d)], [base]).get('2026-04-01')!;
    const without = computeMoney([day('2027-04-01', d)], [base, noNet]).get('2027-04-01')!;
    expect(withNet.savedBattery).toBeLessThan(0.01);
    expect(without.savedBattery).toBeCloseTo(3.6 * 0.3 - 4 * 0.05);
  });

  it('tariefwissel midden in een periode gebruikt per dag het juiste tarief', () => {
    const t2 = { ...base, from: '2026-07-01', importPrice: 0.4 };
    const res = computeMoney([day('2026-06-30', { importKwh: 10 }), day('2026-07-01', { importKwh: 10 })], [base, t2]);
    expect(res.get('2026-06-30')!.cost).toBeCloseTo(3);
    expect(res.get('2026-07-01')!.cost).toBeCloseTo(4);
  });
});
