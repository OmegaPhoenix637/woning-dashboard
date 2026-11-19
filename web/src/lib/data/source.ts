import type { DayData, LiveData, Settings, YearSummary } from '../types';

export interface DataSource {
  readonly kind: 'demo' | 'github';
  live(): Promise<LiveData>;
  today(): Promise<DayData>;
  day(date: string): Promise<DayData | null>;
  year(y: number): Promise<YearSummary>;
  settings(): Promise<Settings>;
  saveSettings(s: Settings): Promise<void>;
}
