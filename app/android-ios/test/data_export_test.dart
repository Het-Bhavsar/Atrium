// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/settings/data/data_export.dart';
import 'package:flutter_test/flutter_test.dart';

/// Data export (#22): the CSVs come from the stored data, with the documented headers, and an empty store says so
/// instead of writing an empty file.

/// 72 bpm at confidence 12/15 on skin, RMSSD 45 ms, walking with 20 clean differences, 120 mg, MLC level 2, pulse
/// present 30 s, skin 35.0 °C, 18 steps, pitch -5°, roll 20°, no oxygen burst.
final full = Uint8List.fromList([72, 0x1C, 45, 0x54, 120, 242, 100, 18, 0xFB, 10, 0, 0]);

/// Docked and charging, nothing measured: skin, pitch and roll unknown; an oxygen burst R 0.85 at confidence 9/15.
final docked = Uint8List.fromList([0, 0xC0, 0, 0, 0, 0, 0, 0, 0x80, 0x80, 85, 0x09]);

void main() {
  late Directory tmp;
  setUp(() => tmp = Directory.systemTemp.createTempSync('atrium_export'));
  tearDown(() => tmp.deleteSync(recursive: true));

  Future<EpochStore> store() async {
    final s = EpochStore(File('${tmp.path}/epochs.bin'));
    // Added out of order: the export is oldest first.
    await s.add([TimedEpoch(1762750030, false, Epoch.decode(docked), docked), TimedEpoch(1762750000, false, Epoch.decode(full), full)]);
    return s;
  }

  test('the documented columns, in order', () {
    expect(dailyColumns.first, 'date');
    expect(recordColumns.take(2), ['time_unix_s', 'time_utc']);
    expect(recordColumns.last, 'record_hex');
    expect(recordColumns.toSet(), hasLength(recordColumns.length));
  });

  test('30-second records: every log field, UTC time, the raw bytes, unknown values empty', () async {
    final s = await store();
    // Through the file and back, as the app reads it.
    final reloaded = EpochStore(s.file);
    await reloaded.load();
    final lines = recordsCsv(reloaded.rowBytes()).trimRight().split('\n');
    expect(lines, [
      recordColumns.join(','),
      '1762750000,2025-11-10T04:46:40Z,72,0.800,1,0,0,0,45,20,walk,120,2,30,35.0,18,-5,20,0.00,0.000,481c2d5478f26412fb0a0000',
      '1762750030,2025-11-10T04:47:10Z,0,0.000,0,0,1,1,0,0,still,0,0,0,,0,,,0.85,0.600,00c000000000000080805509',
    ]);
  });

  test('daily summaries: one row per day, empty where the app has no value', () {
    final csv = dailyCsv([
      DayMetrics(date: DateTime(2026, 11, 9), rhr: 54.26, hrv: 51.94, sleepMin: 421, tempDelta: -0.134, steps: 8123, effort: 58, active: 49, recovery: 71, sleepScore: 84),
      DayMetrics(date: DateTime(2026, 11, 10), rhr: 0, hrv: 0, sleepMin: 0, tempDelta: 0, steps: 312, effort: 3, active: 2),
    ]);
    expect(csv.trimRight().split('\n'), [
      'date,recovery_score,sleep_score,effort_score,resting_hr_bpm,hrv_rmssd_ms,asleep_min,skin_temp_deviation_c,steps,active_min',
      '2026-11-09,71,84,58,54.3,51.9,421,-0.13,8123,49',
      '2026-11-10,,,3,,,,,312,2',
    ]);
  });

  test('the range is the real span of stored days and records', () async {
    final rows = (await store()).rowBytes();
    final t0 = DateTime.fromMillisecondsSinceEpoch(1762750000 * 1000);
    final day0 = DateTime(t0.year, t0.month, t0.day);
    final plan = ExportPlan(
      days: [DayMetrics(date: day0.add(const Duration(days: 1)), rhr: 55, hrv: 50, sleepMin: 400, tempDelta: 0, steps: 0)],
      records: 2,
      recordRows: rows,
    );
    expect(plan.span, (day0, day0.add(const Duration(days: 1))));
    expect(plan.rangeLabel, endsWith('· 2 days'));
  });

  test('writes the daily file, and the records file only when asked', () async {
    final rows = (await store()).rowBytes();
    final plan = ExportPlan(
      days: [DayMetrics(date: DateTime(2026, 11, 10), rhr: 55, hrv: 50, sleepMin: 400, tempDelta: 0.1, steps: 10)],
      records: 2,
      recordRows: rows,
    );
    final now = DateTime(2026, 11, 10, 9);
    final both = await writeExport(plan, Directory('${tmp.path}/a'), withRecords: true, now: now);
    expect([for (final f in both) f.uri.pathSegments.last], ['atrium-daily-20261110.csv', 'atrium-records-20261110.csv']);
    expect(both.last.readAsStringSync(), recordsCsv(rows));
    final daily = await writeExport(plan, Directory('${tmp.path}/b'), withRecords: false, now: now);
    expect([for (final f in daily) f.uri.pathSegments.last], ['atrium-daily-20261110.csv']);
  });

  test('an empty store gives a clear message, not an empty file', () async {
    final s = EpochStore(File('${tmp.path}/none.bin'));
    await s.load();
    final plan = ExportPlan(days: const [], records: s.count, recordRows: s.rowBytes());
    expect(plan.isEmpty, isTrue);
    expect(plan.span, isNull);
    expect(plan.rangeLabel, 'Nothing stored yet');
    final out = Directory('${tmp.path}/empty');
    await expectLater(writeExport(plan, out, withRecords: true), throwsA(isA<NothingToExport>()));
    expect(out.existsSync() && out.listSync().isNotEmpty, isFalse, reason: 'no file written');
    expect('${const NothingToExport()}', contains('Nothing to export yet'));
    expect(recordsCsv(Uint8List(0)).trimRight(), recordColumns.join(','));
  });

  test('a large store is written off the UI isolate with the same result', () async {
    final s = EpochStore(File('${tmp.path}/big.bin'));
    const n = EpochStore.isolateAbove ~/ EpochStore.rowSize + 10;
    await s.add([for (var i = 0; i < n; i++) TimedEpoch(1762750000 + i * 30, false, Epoch.decode(full), full)]);
    final rows = s.rowBytes();
    final plan = ExportPlan(days: const [], records: n, recordRows: rows);
    final files = await writeExport(plan, Directory('${tmp.path}/big'), withRecords: true);
    final lines = files.single.readAsLinesSync();
    expect(lines.length, n + 1);
    expect(lines[1], startsWith('1762750000,'));
  });
}
