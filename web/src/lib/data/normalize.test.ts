import { describe, expect, it } from 'vitest';
import { fillGaps, normalizeDay, type RawDay } from './normalize';
import type { DayTotals } from '../types';

const totals = {} as DayTotals;

describe('normalizeDay', () => {
  it('combineert 5-min kanalen en minuutvermogens tot één minuutraster', () => {
    const raw: RawDay = {
      date: '2026-11-22',
      start: 0,
      step: 300,
      n: 3,
      ch: { t_out: [10, 15, null], pellet_on: [1, 0, null] },
      fast: { step: 60, n: 15, ch: { grid_w: [100, 200, null, null, 500, ...new Array(10).fill(null)] } },
      totals,
    };
    const d = normalizeDay(raw);
    expect(d.step).toBe(60);
    expect(d.n).toBe(15);
    expect(d.ch.grid_w.slice(0, 5)).toEqual([100, 200, 300, 400, 500]); // kort gat opgevuld
    expect(d.ch.grid_w[14]).toBeNull(); // einde (nog geen data) blijft leeg
    expect(d.ch.t_out.slice(0, 6)).toEqual([10, 11, 12, 13, 14, 15]); // lineair tussen 5-min punten
    expect(d.ch.pellet_on.slice(0, 6)).toEqual([1, 1, 1, 1, 1, 0]); // standen vasthouden
    expect(d.ch.t_out[10]).toBeNull();
  });

  it('zet een oude 5-minutendag om naar minuten en is idempotent', () => {
    const raw: RawDay = { date: 'x', start: 0, step: 300, n: 2, ch: { solar_w: [0, 500] }, totals };
    const d = normalizeDay(raw);
    expect(d.n).toBe(10);
    expect(d.ch.solar_w.slice(0, 6)).toEqual([0, 100, 200, 300, 400, 500]);
    expect(normalizeDay(d)).toBe(d);
  });

  it('vult geen lange gaten', () => {
    const a = [1, ...new Array(11).fill(null), 13];
    expect(fillGaps(a, 10)).toEqual(a);
    expect(fillGaps([1, null, 3], 10)).toEqual([1, 2, 3]);
  });
});
