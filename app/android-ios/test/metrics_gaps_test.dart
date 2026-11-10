// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/today/presentation/today_view.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

/// Gaps in the pod's log stay gaps: the 24 h heart-rate chart breaks where no pulse was logged instead of carrying the
/// last one on, and a walk never spans a stretch the log doesn't cover.

/// 2026-11-10 00:00 UTC; the tests run at UTC so local minutes equal UTC minutes.
const midnight = 1762732800;

TimedEpoch ep(int t, {double bpm = 0, ActivityClass cls = ActivityClass.still}) => TimedEpoch(
  t,
  false,
  Epoch(
    pulseBpm: bpm,
    pulseConfidence: bpm > 0 ? 0.9 : 0,
    onSkin: true,
    pulseFromIr: false,
    docked: false,
    charging: false,
    rmssdMs: 0,
    cleanDiffs: 0,
    activityClass: cls,
    activityMg: cls == ActivityClass.still ? 0 : 200,
    mlcLevelMax: 0,
    pulsePresentS: bpm > 0 ? 30 : 0,
    skinC: double.nan,
    steps: cls == ActivityClass.still ? 0 : 50,
    pitchDeg: double.nan,
    rollDeg: double.nan,
    burstR: 0,
    burstConfidence: 0,
  ),
);

/// 30 s epochs from [fromMin] to [toMin] (minutes after midnight), end excluded.
List<TimedEpoch> span(int fromMin, int toMin, {double bpm = 0, ActivityClass cls = ActivityClass.still}) => [
  for (var t = midnight + fromMin * 60; t < midnight + toMin * 60; t += 30) ep(t, bpm: bpm, cls: cls),
];

PodMetrics metrics(List<TimedEpoch> log, {int nowMin = 18 * 60}) =>
    PodMetrics(log, podOnRight: false, now: DateTime.fromMillisecondsSinceEpoch((midnight + nowMin * 60) * 1000), utcOffset: Duration.zero);

void main() {
  group('heart rate, last 24 h', () {
    // Pulse 08:00-09:00 and 10:00-11:00; nothing logged in between (pod off).
    final log = [...span(8 * 60, 9 * 60, bpm: 70), ...span(10 * 60, 11 * 60, bpm: 90)];
    final m = metrics(log, nowMin: 12 * 60);

    test('an empty 5-min bin has no sample: the last pulse is not carried on', () {
      final hole = m.hr24.where((h) => h.minute >= 9 * 60 && h.minute < 10 * 60);
      expect(hole, isEmpty);
      expect(m.hr24.where((h) => h.minute >= 11 * 60), isEmpty, reason: 'nor after the last reading');
      expect(m.hr24.where((h) => h.bpm == 70), hasLength(12));
      expect(m.hr24.where((h) => h.bpm == 90), hasLength(12));
    });

    test('bins with epochs but no pulse (pod docked, off skin) are gaps too', () {
      final off = metrics([...span(8 * 60, 9 * 60, bpm: 70), ...span(9 * 60, 10 * 60), ...span(10 * 60, 11 * 60, bpm: 90)], nowMin: 12 * 60);
      expect(off.hr24.where((h) => h.minute >= 9 * 60 && h.minute < 10 * 60), isEmpty);
    });

    test('the chart grid holds NaN in the hole, and the line breaks there instead of joining across', () {
      final grid = hr24Grid(m.hr24);
      final values = [for (final (_, v) in grid) v];
      final holeSlots = [
        for (final (min, v) in grid)
          if (min >= 9 * 60 && min < 10 * 60) v,
      ];
      expect(holeSlots, hasLength(12));
      expect(holeSlots.every((v) => v.isNaN), isTrue);
      final runs = lineRuns(values);
      expect(runs, hasLength(2), reason: 'two stretches, one each side of the hole');
      expect(values[runs.first.$2], 70);
      expect(values[runs.last.$1], 90);
      expect(lineRuns(<double>[1, double.nan, 3]), [(0, 0), (2, 2)]);
      expect(lineRuns([double.nan]), isEmpty);
    });

    testWidgets('the line chart paints a gapped series, and the readout says there was no reading', (tester) async {
      final values = <double>[70, 72, double.nan, double.nan, 90, 91];
      await tester.pumpWidget(
        MaterialApp(
          theme: buildTheme(Brightness.light),
          home: Scaffold(
            body: Padding(
              padding: const EdgeInsets.only(top: 80),
              child: SizedBox(
                width: 360,
                child: AtriumChart(points: values, color: Colors.green, kind: ChartKind.line, labels: [for (var i = 0; i < 6; i++) 'slot $i'], unit: 'bpm'),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      // Touch the third slot (a gap), then the fifth (a reading).
      final box = tester.getRect(find.byType(AtriumChart));
      final slot = (box.width - 34) / 6;
      final g = await tester.startGesture(Offset(box.left + slot * 2.5, box.center.dy));
      await tester.pump(const Duration(milliseconds: 150));
      expect(find.textContaining('No reading  slot 2', findRichText: true), findsOneWidget);
      expect(find.textContaining('bpm', findRichText: true), findsNothing, reason: 'no unit without a value');
      await g.moveBy(Offset(slot * 2, 0));
      await tester.pump(const Duration(milliseconds: 16));
      expect(find.textContaining('90 bpm  slot 4', findRichText: true), findsOneWidget);
      await g.up();
      await tester.pump(const Duration(seconds: 3));
    });
  });

  group('workouts', () {
    test('two walks 3 h apart, with no log between them, are two walks, not one', () {
      final log = [...span(9 * 60, 9 * 60 + 12, cls: ActivityClass.walk, bpm: 100), ...span(12 * 60, 12 * 60 + 12, cls: ActivityClass.walk, bpm: 105)];
      final w = metrics(log).workouts;
      expect(w, hasLength(2));
      expect([for (final x in w) x.startMinute], [9 * 60, 12 * 60]);
      expect([for (final x in w) x.minutes], [12, 12]);
    });

    test('short walks far apart are not joined into a long one', () {
      final log = [...span(9 * 60, 9 * 60 + 6, cls: ActivityClass.walk), ...span(12 * 60, 12 * 60 + 6, cls: ActivityClass.walk)];
      expect(metrics(log).workouts, isEmpty, reason: 'each is under 10 min');
    });

    test('a stop of 2 min or less joins, a longer one splits', () {
      final joined = metrics([
        ...span(9 * 60, 9 * 60 + 6, cls: ActivityClass.walk),
        ...span(9 * 60 + 6, 9 * 60 + 8),
        ...span(9 * 60 + 8, 9 * 60 + 14, cls: ActivityClass.walk),
      ]).workouts;
      expect([for (final x in joined) x.minutes], [14]);

      final split = metrics([
        ...span(9 * 60, 9 * 60 + 6, cls: ActivityClass.walk),
        ...span(9 * 60 + 6, 9 * 60 + 9),
        ...span(9 * 60 + 9, 9 * 60 + 15, cls: ActivityClass.walk),
      ]).workouts;
      expect(split, isEmpty, reason: 'two 6 min walks, each under 10 min');
    });

    test('the 10-min minimum still holds', () {
      expect(metrics(span(9 * 60, 9 * 60 + 9, cls: ActivityClass.run)).workouts, isEmpty);
      final run = metrics(span(9 * 60, 9 * 60 + 10, cls: ActivityClass.run)).workouts;
      expect([for (final x in run) (x.kind, x.minutes)], [('Run', 10)]);
    });
  });
}
