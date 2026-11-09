import 'package:atrium/core/util/format.dart';
import 'package:atrium/features/insights/data/sample_data.dart';
import 'package:atrium/features/insights/domain/metrics.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:flutter_test/flutter_test.dart';

/// Demo data must match what the HTML prototype shows (same seeded RNG), so the two never disagree.
void main() {
  final d = SampleData.instance;

  test('last night matches the prototype', () {
    expect(d.last.rhr, 54);
    expect(d.last.recovery, 78);
    expect(d.last.sleepScore, 84);
    expect(hm(d.asleep), '7 h 0 min');
    expect(d.inBed - d.asleep, 26);
  });

  test('usual ranges match the prototype tiles (Usual 56-57 bpm, 42-50 ms)', () {
    expect(d.usual[Metric.rhr]!.lo.round(), 56);
    expect(d.usual[Metric.rhr]!.hi.round(), 57);
    expect(d.usual[Metric.hrv]!.lo.round(), 42);
    expect(d.usual[Metric.hrv]!.hi.round(), 50);
    expect(d.usual[Metric.active]!.mean.round(), 47);
  });

  test('30-day averages match the prototype Trends list', () {
    expect(d.series(Metric.recovery, Range.month).mean.round(), 71);
    expect(d.series(Metric.effort, Range.month).mean.round(), 58);
    expect(d.series(Metric.rhr, Range.month).mean.round(), 56);
    expect(d.series(Metric.hrv, Range.month).mean.round(), 46);
    expect(hm(d.series(Metric.sleepMin, Range.month).mean), '7 h 2 min');
  });

  test('battery life per mode (30 mAh, 95 % usable)', () {
    expect(formatLife(MeasureMode.smart.lifeDays()), '4 days');
    expect(formatLife(MeasureMode.full.lifeDays()), '2 days');
    expect(formatLife(MeasureMode.smart.lifeDays(60)), '2 days');
  });

  test('time formatting never shows 60 minutes', () {
    expect(hm(419.6), '7 h 0 min');
    expect(signed1(-0.04), '0.0');
    expect(signed1(0.2), '+0.2');
  });
}
