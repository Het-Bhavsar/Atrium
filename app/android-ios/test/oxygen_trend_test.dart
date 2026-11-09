// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/metrics/presentation/oxygen_trend_card.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

/// Oxygen trend (#8, Labs), end to end: nights of the reference log -> PodMetrics -> the Sleep card. Relative to the
/// user's own baseline after 7 nights, never a percentage.
void main() {
  final models = SleepModels.parse(File('assets/models/sleep_v1.json').readAsStringSync());
  final log = decodeBlocks(Uint8List.fromList(File('test/fixtures/phone/phone_log.bin').readAsBytesSync()));
  const ist = Duration(hours: 5, minutes: 30);

  /// The reference night repeated on [n] consecutive days.
  PodMetrics nights(int n) {
    final all = [
      for (var k = 0; k < n; k++)
        for (final e in log) TimedEpoch(e.time + k * 86400, false, e.epoch, e.raw),
    ];
    final now = DateTime.fromMillisecondsSinceEpoch((all.last.time + 30) * 1000);
    return PodMetrics(all, podOnRight: false, models: models, now: now, utcOffset: ist);
  }

  test('no trend before 7 earlier nights', () {
    expect(nights(7).oxygenTrend, isNull, reason: '6 earlier nights');
  });

  test('after 7 earlier nights: last night against their median', () {
    final t = nights(8).oxygenTrend!;
    expect(t.nights, 7);
    expect(t.change, closeTo(0, 1e-9), reason: 'the same night every day');
    expect(oxygenTrendWords(t), 'Close to your usual');
  });

  test('demo data never has one', () {
    final MetricsSource empty = PodMetrics(const [], podOnRight: false);
    expect(empty.oxygenTrend, isNull);
  });

  test('shown only in real builds, with Labs on and enough nights', () {
    const t = OxygenTrend(change: 0.05, nights: 9);
    expect(showOxygenTrend(realData: true, labs: true, trend: t), isTrue);
    expect(showOxygenTrend(realData: true, labs: false, trend: t), isFalse);
    expect(showOxygenTrend(realData: false, labs: true, trend: t), isFalse);
    expect(showOxygenTrend(realData: true, labs: true, trend: null), isFalse);
    expect(showOxygenTrend(realData: true, labs: true, trend: const OxygenTrend(change: 0.05, nights: 6)), isFalse);
    expect(showOxygenTrend(realData: true, labs: true, trend: const OxygenTrend(change: double.nan, nights: 9)), isFalse);
  });

  test('words say which way the ratio moved', () {
    expect(oxygenTrendWords(const OxygenTrend(change: 0.06, nights: 7)), 'Higher ratio than your usual');
    expect(oxygenTrendWords(const OxygenTrend(change: -0.06, nights: 7)), 'Lower ratio than your usual');
    expect(oxygenTrendWords(const OxygenTrend(change: 0.01, nights: 7)), 'Close to your usual');
  });

  testWidgets('the card is relative, says it is research, and has no percentage or clinical words', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        theme: buildTheme(Brightness.light),
        home: const Scaffold(body: OxygenTrendCard(OxygenTrend(change: -0.04, nights: 12))),
      ),
    );
    final text = tester.widgetList<Text>(find.byType(Text)).map((w) => w.data ?? w.textSpan!.toPlainText()).join(' ');
    expect(text, contains('0.96'));
    expect(text, contains('× your usual'));
    expect(text, contains('research trend'));
    expect(text, contains('not a blood-oxygen percentage'));
    expect(text, contains('last 12 nights'));
    expect(text, isNot(contains('%')));
    for (final w in ['SpO', 'saturation', 'apnea', 'apnoea', 'hypox', 'diagnos', 'oxygen level']) {
      expect(text.toLowerCase(), isNot(contains(w.toLowerCase())), reason: w);
    }
  });
}
