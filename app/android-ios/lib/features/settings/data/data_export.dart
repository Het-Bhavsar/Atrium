// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Data export (#22): the user's daily summaries and the pod's 30-second records as CSV, written from what is stored
/// on this phone and handed to the system share sheet. Nothing is uploaded by the app; where the files go is the
/// user's choice in the share sheet. The columns are documented in the app README ("Data export").
library;

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter/foundation.dart' show compute;

/// Daily summary columns, units in the names. One row per local day, oldest first.
const List<String> dailyColumns = [
  'date',
  'recovery_score',
  'sleep_score',
  'effort_score',
  'resting_hr_bpm',
  'hrv_rmssd_ms',
  'asleep_min',
  'skin_temp_deviation_c',
  'steps',
  'active_min',
];

/// 30-second record columns: every field of the pod's log record (`log_record.dart`), the time in UTC, and the 12
/// record bytes as logged so the row can be decoded again.
const List<String> recordColumns = [
  'time_unix_s',
  'time_utc',
  'pulse_bpm',
  'pulse_confidence',
  'on_skin',
  'pulse_from_ir',
  'docked',
  'charging',
  'rmssd_ms',
  'clean_diffs',
  'activity',
  'activity_mg',
  'mlc_level_max',
  'pulse_present_s',
  'skin_temp_c',
  'steps',
  'pitch_deg',
  'roll_deg',
  'burst_ratio_r',
  'burst_confidence',
  'record_hex',
];

String _d(DateTime d) => '${d.year.toString().padLeft(4, '0')}-${d.month.toString().padLeft(2, '0')}-${d.day.toString().padLeft(2, '0')}';

/// A number with [dp] decimals, or empty when it isn't known (NaN, or zero where zero means "none").
String _n(double v, int dp, {bool zeroIsNone = false}) => !v.isFinite || (zeroIsNone && v == 0) ? '' : v.toStringAsFixed(dp);
String _i(int v, {bool zeroIsNone = false}) => zeroIsNone && v == 0 ? '' : '$v';
String _b(bool v) => v ? '1' : '0';

/// The daily summaries as CSV. Values the app doesn't have for a day (no night, no baseline yet) are empty cells.
String dailyCsv(List<DayMetrics> days) {
  final out = StringBuffer()..writeln(dailyColumns.join(','));
  for (final d in days) {
    final night = d.sleepMin > 0;
    out.writeln(
      [
        _d(d.date),
        _i(d.recovery, zeroIsNone: true),
        _i(d.sleepScore, zeroIsNone: true),
        _i(d.effort),
        _n(d.rhr, 1, zeroIsNone: true),
        _n(d.hrv, 1, zeroIsNone: true),
        _n(d.sleepMin, 0, zeroIsNone: true),
        night ? _n(d.tempDelta, 2) : '',
        _i(d.steps),
        _i(d.active),
      ].join(','),
    );
  }
  return out.toString();
}

String _utc(int t) {
  final s = DateTime.fromMillisecondsSinceEpoch(t * 1000, isUtc: true).toIso8601String();
  return '${s.substring(0, 19)}Z';
}

String _hex(Uint8List b) => [for (final x in b) x.toRadixString(16).padLeft(2, '0')].join();

/// One record row (no newline).
String recordRow(int time, Uint8List raw) {
  final e = Epoch.decode(raw);
  return [
    '$time',
    _utc(time),
    _n(e.pulseBpm, 0),
    _n(e.pulseConfidence, 3),
    _b(e.onSkin),
    _b(e.pulseFromIr),
    _b(e.docked),
    _b(e.charging),
    _n(e.rmssdMs, 0),
    '${e.cleanDiffs}',
    e.activityClass.name,
    _n(e.activityMg, 0),
    '${e.mlcLevelMax}',
    '${e.pulsePresentS}',
    _n(e.skinC, 1),
    '${e.steps}',
    _n(e.pitchDeg, 0),
    _n(e.rollDeg, 0),
    _n(e.burstR, 2),
    _n(e.burstConfidence, 3),
    _hex(raw),
  ].join(',');
}

/// The records CSV from store rows (16 bytes each: u32 Unix time, then the 12 record bytes), oldest first.
String recordsCsv(Uint8List rows) {
  final out = StringBuffer()..writeln(recordColumns.join(','));
  final bd = ByteData.sublistView(rows);
  for (var o = 0; o + EpochStore.rowSize <= rows.length; o += EpochStore.rowSize) {
    out.writeln(recordRow(bd.getUint32(o, Endian.little), Uint8List.sublistView(rows, o + 4, o + EpochStore.rowSize)));
  }
  return out.toString();
}

/// What the export sheet shows and writes.
class ExportPlan {
  const ExportPlan({required this.days, required this.records, this.recordRows});

  final List<DayMetrics> days;

  /// The pod's stored 30-second records, null in the demo (it has none).
  final int records;
  final Uint8List? recordRows;

  bool get isEmpty => days.isEmpty && records == 0;
  bool get hasRecords => records > 0;

  /// The span of the stored data, first to last day (local), or null when there is nothing.
  (DateTime, DateTime)? get span {
    DateTime? lo = days.isEmpty ? null : days.first.date;
    DateTime? hi = days.isEmpty ? null : days.last.date;
    final rows = recordRows;
    if (rows != null && rows.length >= EpochStore.rowSize) {
      final bd = ByteData.sublistView(rows);
      DateTime at(int o) {
        final t = DateTime.fromMillisecondsSinceEpoch(bd.getUint32(o, Endian.little) * 1000);
        return DateTime(t.year, t.month, t.day);
      }

      final first = at(0);
      final last = at(rows.length - EpochStore.rowSize);
      if (lo == null || first.isBefore(lo)) lo = first;
      if (hi == null || last.isAfter(hi)) hi = last;
    }
    return lo == null || hi == null ? null : (lo, hi);
  }

  /// "4 Nov – 10 Nov 2026 · 7 days" for the sheet.
  String get rangeLabel {
    final s = span;
    if (s == null) return 'Nothing stored yet';
    const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
    String f(DateTime d, {bool year = false}) => '${d.day} ${months[d.month - 1]}${year ? ' ${d.year}' : ''}';
    final n = DateTime.utc(s.$2.year, s.$2.month, s.$2.day).difference(DateTime.utc(s.$1.year, s.$1.month, s.$1.day)).inDays + 1;
    final range = n == 1 ? f(s.$1, year: true) : '${f(s.$1, year: s.$1.year != s.$2.year)} – ${f(s.$2, year: true)}';
    return '$range · $n ${n == 1 ? 'day' : 'days'}';
  }
}

/// Nothing to export: the sheet says so instead of writing an empty file.
class NothingToExport implements Exception {
  const NothingToExport();
  @override
  String toString() => 'Nothing to export yet. Readings appear here after the pod has synced.';
}

/// Writes the CSVs into [dir] and returns them: the daily summaries, and the 30-second records when [withRecords]
/// and there are any. The records file is built off the UI isolate (months of records are tens of MB).
Future<List<File>> writeExport(ExportPlan plan, Directory dir, {required bool withRecords, DateTime? now}) async {
  if (plan.isEmpty) throw const NothingToExport();
  final stamp = _d(now ?? DateTime.now()).replaceAll('-', '');
  await dir.create(recursive: true);
  final files = <File>[];
  if (plan.days.isNotEmpty) {
    files.add(await File('${dir.path}/atrium-daily-$stamp.csv').writeAsString(dailyCsv(plan.days), flush: true));
  }
  final rows = plan.recordRows;
  if (withRecords && rows != null && plan.hasRecords) {
    final csv = rows.length > EpochStore.isolateAbove ? await compute(recordsCsv, rows) : recordsCsv(rows);
    files.add(await File('${dir.path}/atrium-records-$stamp.csv').writeAsString(csv, flush: true));
  }
  if (files.isEmpty) throw const NothingToExport();
  return files;
}
