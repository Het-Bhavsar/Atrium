import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/insights/domain/nightly.dart';
import 'package:atrium/features/insights/domain/position.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter_test/flutter_test.dart';

/// The phone-side port must match `atrium_ml/nightly.py` and `sleep.py` on the ML session's vectors
/// (`make_phone_vectors.py`): 8 synthetic pages, UTC+5:30, left side, asleep 23:30-07:00 with a 15 min wake at 02:00.
/// Tolerances: features and P(asleep) 1e-6 relative; labels, counts and minutes equal; bpm, ms and degC 0.01.
void main() {
  const dir = 'test/fixtures/phone';
  final models = SleepModels.parse(File('assets/models/sleep_v1.json').readAsStringSync());
  final log = decodeBlocks(Uint8List.fromList(File('$dir/phone_log.bin').readAsBytesSync()));
  final times = [for (final e in log) e.time];
  final epochs = [for (final e in log) e.epoch];
  final inputs = (jsonDecode(File('$dir/phone_night.json').readAsStringSync()) as Map<String, dynamic>)['inputs'] as Map<String, dynamic>;
  final want = (jsonDecode(File('$dir/phone_night.json').readAsStringSync()) as Map<String, dynamic>)['night'] as Map<String, dynamic>;
  final upL = (inputs['upright'] as List<dynamic>).cast<num>();
  final up = Vec3(upL[0].toDouble(), upL[1].toDouble(), upL[2].toDouble());
  final offset = (inputs['utc_offset_h'] as num).toDouble();
  final nightDate = inputs['night_date_s'] as int;

  List<List<String>> csv(String f) => [for (final l in File('$dir/$f').readAsLinesSync().skip(1)) l.split(',')];
  void near(double got, double want, double tol, String why) => expect((got - want).abs() <= tol, isTrue, reason: '$why: got $got, want $want');

  test('A: decodes the synced log like logrec.decode_blocks', () {
    final rows = csv('phone_epochs.csv');
    expect(log.length, rows.length);
    for (var i = 0; i < rows.length; i++) {
      final r = rows[i];
      final e = epochs[i];
      expect(times[i], int.parse(r[0]), reason: 'time $i');
      near(e.pulseBpm, double.parse(r[2]), 1e-9, 'pulse $i');
      expect(e.onSkin, r[4] == '1', reason: 'on skin $i');
      expect(e.docked, r[6] == '1', reason: 'docked $i');
      expect(e.activityClass.name, r[10], reason: 'class $i');
      near(e.activityMg, double.parse(r[11]), 1e-9, 'activity $i');
      expect(e.steps, int.parse(r[15]), reason: 'steps $i');
    }
  });

  test('B, C, J: rest window, features, P(asleep), asleep and position per epoch', () {
    final m = nightWindow(times, offset, nightDate);
    final (t0, ep0) = regrid([
      for (var i = 0; i < times.length; i++)
        if (m[i]) times[i],
    ], [
      for (var i = 0; i < epochs.length; i++)
        if (m[i]) epochs[i],
    ]);
    final (a, b) = restCandidate(ep0)!;
    final t = t0.sublist(a, b);
    final ep = ep0.sublist(a, b);
    final rows = csv('phone_night_epochs.csv');
    expect(ep.length, rows.length, reason: 'rest window length');
    final x = epochFeatures([for (final e in ep) e.onSkin ? e.activityMg : double.nan], [for (final e in ep) e.pulseBpm > 0 ? e.pulseBpm : double.nan]);
    final (asleep, p) = sleepWake(ep, models.wakeSleep);
    for (final (s, e) in runs([for (final v in asleep) v == 1])) {
      if (e - s < 20) asleep.fillRange(s, e, 0);
    }
    final labels = positions(ep, podOnRight: false, up: up);
    for (var i = 0; i < rows.length; i++) {
      final r = rows[i];
      expect(t[i], int.parse(r[0]), reason: 'time $i');
      for (var j = 0; j < 6; j++) {
        final w = double.parse(r[1 + j]);
        near(x[i][j], w, 1e-6 * w.abs() + 1e-8, 'feature $j at $i');
      }
      final wp = double.parse(r[7]);
      near(p[i], wp, 1e-6 * wp.abs() + 1e-8, 'P(asleep) at $i');
      expect(asleep[i], int.parse(r[8]), reason: 'asleep $i');
      expect(labels[i]?.name ?? '', r[9], reason: 'position $i');
      expect(labels[i] == HeadLabel.left ? '1' : '0', r[10], reason: 'on pod $i');
    }
  });

  test('E-H, J, M: the night summary matches nightly.summarise', () {
    final n = summarise(times, epochs, podOnRight: false, utcOffsetH: offset, nightDateS: nightDate, models: models, up: up);
    double w(String k) => (want[k] as num).toDouble();
    expect(n.startS, w('start_s'));
    expect(n.endS, w('end_s'));
    expect(n.inBedMin, w('in_bed_min'));
    expect(n.asleepMin, w('asleep_min'));
    expect(n.periodMin, w('period_min'));
    near(n.efficiency, w('efficiency'), 1e-9, 'efficiency');
    expect(n.wakeUps, want['wake_ups']);
    near(n.restingHrBpm, w('resting_hr_bpm'), 0.01, 'resting HR');
    near(n.rmssdMs, w('rmssd_ms'), 0.01, 'RMSSD');
    expect(n.hrvDiffs, want['hrv_diffs']);
    near(n.skinC, w('skin_c'), 0.01, 'skin');
    near(n.oxygenRatio, w('oxygen_ratio'), 1e-9, 'oxygen R');
    expect(n.oxygenBursts, want['oxygen_bursts']);
    final pos = want['position_min'] as Map<String, dynamic>;
    for (final l in HeadLabel.values) {
      expect(n.positionMin[l], (pos[l.name] as num).toDouble(), reason: 'position ${l.name}');
    }
    expect(n.onPodMin, w('on_pod_min'));
  });

  test('J2: upright from this log matches upright_from_log', () {
    final u = uprightFromLog(epochs);
    near(u.x, up.x, 1e-9, 'x');
    near(u.y, up.y, 1e-9, 'y');
    near(u.z, up.z, 1e-9, 'z');
  });

  test('I: deviation from the 14-night baseline, none before 5 nights', () {
    for (final r in csv('phone_baseline.csv')) {
      final hist = [for (final s in r[1].split(' ')) s == 'nan' ? double.nan : double.parse(s)];
      final d = deviation(double.parse(r[2]), hist);
      if (r[3].isEmpty) {
        expect(d, isNull, reason: r.join(','));
      } else {
        near(d!, double.parse(r[3]), 1e-4, r.join(','));
      }
    }
  });
}
