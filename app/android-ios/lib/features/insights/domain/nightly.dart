import 'dart:math' as math;

import 'package:atrium/features/insights/domain/position.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/log_record.dart';

/// Phone-side nightly summary from the synced pod log (FEATURES.md #2, #3, #4, #5, #8): a port of
/// `app/python/atrium_ml/nightly.py`. Everything here needs a whole night or several nights, the wear side, or local
/// time, which the pod does not have, so the app computes it. Thresholds marked [ASSUMED] are set from V3/V4 data.
///
/// Order: night window (18:00 to 14:00 local) -> gap-free 30 s grid -> rest run, padded -> sleep/wake per epoch ->
/// sleep period -> per-night values from the asleep epochs in it.
const double epochMin = 0.5;

class NightConfig {
  const NightConfig();
  final double windowStartH = 18;
  final double windowEndH = 14; // next day
  final double restMg = 15; // [ASSUMED]
  final double restJoinMin = 20; // [ASSUMED]
  final double restMinH = 2; // [ASSUMED]
  final double padMin = 30; // [ASSUMED]
  final double onsetMin = 10; // [ASSUMED]
  final double wakeUpMin = 5; // [ASSUMED]
  final double rhrWinMin = 10; // [ASSUMED]
  final double rhrMinConf = 0.5; // [ASSUMED]
  final int hrvMinDiffs = 300; // [ASSUMED] about 10 min of clean beats
  final double skinSettleMin = 30; // [ASSUMED]
  final int uprightMinWalkEpochs = 10;
  final int minNights = 5; // [ASSUMED]
}

/// An epoch with nothing logged: off skin, so it counts as wake (gaps in the grid).
const Epoch emptyEpoch = Epoch(
  pulseBpm: 0,
  pulseConfidence: 0,
  onSkin: false,
  pulseFromIr: false,
  docked: false,
  charging: false,
  rmssdMs: 0,
  cleanDiffs: 0,
  activityClass: ActivityClass.still,
  activityMg: 0,
  mlcLevelMax: 0,
  pulsePresentS: 0,
  skinC: double.nan,
  steps: 0,
  pitchDeg: double.nan,
  rollDeg: double.nan,
  burstR: 0,
  burstConfidence: 0,
);

/// One night. NaN where there was not enough to say.
class Night {
  double startS = double.nan; // sleep period, Unix s
  double endS = double.nan;
  double inBedMin = 0;
  double asleepMin = 0;
  double periodMin = 0;
  double efficiency = double.nan;
  int wakeUps = 0;
  double restingHrBpm = double.nan;
  double rmssdMs = double.nan;
  int hrvDiffs = 0;
  double skinC = double.nan;
  double oxygenRatio = double.nan;
  int oxygenBursts = 0;
  Map<HeadLabel, double> positionMin = {};
  double onPodMin = 0;

  /// The sleep period as asleep (true) / awake runs, in epochs from [startS] (for the hypnogram).
  List<bool> asleep = const [];

  /// Per-epoch stage in the sleep period from the Labs model: 0 wake, 1 non-REM, 2 REM.
  List<int> stages = const [];

  bool get hasSleep => !startS.isNaN;
}

bool usable(Epoch e) => e.onSkin && !e.docked;

/// Epochs of the night that starts on the local day containing [nightDateS] (18:00 to 14:00 next day).
List<bool> nightWindow(List<int> times, double utcOffsetH, int nightDateS, [NightConfig cfg = const NightConfig()]) {
  final off = utcOffsetH * 3600;
  final day0 = ((nightDateS + off) / 86400).floorToDouble() * 86400;
  final lo = day0 + cfg.windowStartH * 3600;
  final hi = day0 + (24 + cfg.windowEndH) * 3600;
  return [for (final t in times) t + off >= lo && t + off < hi];
}

/// (start, end) of true runs, end exclusive.
List<(int, int)> runs(List<bool> m) {
  final out = <(int, int)>[];
  int? s;
  for (var i = 0; i < m.length; i++) {
    if (m[i] && s == null) s = i;
    if (!m[i] && s != null) {
      out.add((s, i));
      s = null;
    }
  }
  if (s != null) out.add((s, m.length));
  return out;
}

List<double> _centredMean(List<double> x, int half) {
  final c = [0.0];
  for (final v in x) {
    c.add(c.last + v);
  }
  return [
    for (var i = 0; i < x.length; i++) (c[math.min(i + half + 1, x.length)] - c[math.max(i - half, 0)]) / (math.min(i + half + 1, x.length) - math.max(i - half, 0)),
  ];
}

/// Puts epochs on a gap-free 30 s grid from the first to the last; gaps become [emptyEpoch].
(List<int>, List<Epoch>) regrid(List<int> times, List<Epoch> epochs) {
  if (times.isEmpty) return (const [], const []);
  final k = [for (final t in times) ((t - times.first) / 30).round()];
  final grid = List<Epoch>.filled(k.last + 1, emptyEpoch);
  for (var i = 0; i < k.length; i++) {
    grid[k[i]] = epochs[i];
  }
  return ([for (var i = 0; i <= k.last; i++) times.first + 30 * i], grid);
}

/// The padded rest run (start, end), or null: the longest still run, joining short breaks, at least 2 h.
(int, int)? restCandidate(List<Epoch> epochs, [NightConfig cfg = const NightConfig()]) {
  final act = _centredMean([for (final e in epochs) usable(e) ? e.activityMg : 255.0], 10);
  final still = [for (var i = 0; i < epochs.length; i++) usable(epochs[i]) && act[i] < cfg.restMg];
  final join = cfg.restJoinMin ~/ epochMin;
  final merged = <(int, int)>[];
  for (final (a, b) in runs(still)) {
    if (merged.isNotEmpty && a - merged.last.$2 < join) {
      merged.last = (merged.last.$1, b);
    } else {
      merged.add((a, b));
    }
  }
  final long = merged.where((r) => (r.$2 - r.$1) * epochMin >= cfg.restMinH * 60).toList();
  if (long.isEmpty) return null;
  final best = long.reduce((x, y) => (y.$2 - y.$1) > (x.$2 - x.$1) ? y : x);
  final pad = cfg.padMin ~/ epochMin;
  return (math.max(best.$1 - pad, 0), math.min(best.$2 + pad, epochs.length));
}

List<List<double>> _features(List<Epoch> epochs) => epochFeatures(
  [for (final e in epochs) e.onSkin ? e.activityMg : double.nan],
  [for (final e in epochs) e.pulseBpm > 0 ? e.pulseBpm : double.nan],
);

/// Asleep (1) per epoch and P(asleep); epochs not usable are wake.
(List<int>, List<double>) sleepWake(List<Epoch> epochs, LogisticModel model) {
  final x = _features(epochs);
  final p = model.predict(x);
  return ([for (var i = 0; i < epochs.length; i++) usable(epochs[i]) ? p[i] : 0], [for (final q in model.proba(x)) q[1]]);
}

/// Mean gravity of walking epochs (pod frame), or +x (as worn) before [NightConfig.uprightMinWalkEpochs].
Vec3 uprightFromLog(Iterable<Epoch> epochs, [NightConfig cfg = const NightConfig()]) {
  final g = [
    for (final e in epochs)
      if (e.activityClass == ActivityClass.walk && !e.pitchDeg.isNaN && !e.rollDeg.isNaN) gravityFromPitchRoll(e.pitchDeg, e.rollDeg),
  ];
  return uprightFromWalking(g, minEpochs: cfg.uprightMinWalkEpochs);
}

/// Head position per epoch (null where gravity is unknown).
List<HeadLabel?> positions(List<Epoch> epochs, {required bool podOnRight, required Vec3 up}) {
  final frame = headFrame(up, podOnRight: podOnRight);
  final known = [for (final e in epochs) !e.pitchDeg.isNaN && !e.rollDeg.isNaN];
  final labels = classifyEpochs([
    for (var i = 0; i < epochs.length; i++) known[i] ? gravityFromPitchRoll(epochs[i].pitchDeg, epochs[i].rollDeg) : up,
  ], frame);
  return [for (var i = 0; i < epochs.length; i++) known[i] ? labels[i] : null];
}

/// Lowest mean pulse over [NightConfig.rhrWinMin] of confident epochs; a window counts with 70 % present.
double restingHr(List<Epoch> epochs, [NightConfig cfg = const NightConfig()]) {
  final n = cfg.rhrWinMin ~/ epochMin;
  final hr = [for (final e in epochs) e.pulseBpm > 0 && e.pulseConfidence >= cfg.rhrMinConf ? e.pulseBpm : double.nan];
  var best = double.nan;
  for (var i = 0; i + n <= hr.length; i++) {
    final w = hr.sublist(i, i + n).where((v) => !v.isNaN).toList();
    if (w.length >= 0.7 * n) {
      final m = w.reduce((a, b) => a + b) / w.length;
      if (best.isNaN || m < best) best = m;
    }
  }
  return best;
}

/// Pooled RMSSD: sqrt(sum(r^2 n) / sum(n)), as docs/ble_gatt.md section 7.
double pooledRmssd(List<(double, int)> pairs) {
  var num = 0.0;
  var den = 0;
  for (final (r, n) in pairs) {
    num += r * r * n;
    den += n;
  }
  return den > 0 ? math.sqrt(num / den) : double.nan;
}

double median(List<double> xs) {
  final s = [...xs]..sort();
  final m = s.length ~/ 2;
  return s.length.isOdd ? s[m] : (s[m - 1] + s[m]) / 2;
}

/// Confidence-weighted median R of one night's oxygen bursts (Labs, a trend only; never a percentage).
double oxygenNightRatio(List<(double, double)> bursts, {double minConf = 0.3}) {
  final b = bursts.where((x) => !x.$1.isNaN && x.$2 >= minConf).toList()..sort((x, y) => x.$1.compareTo(y.$1));
  if (b.isEmpty) return double.nan;
  final half = b.fold(0.0, (a, x) => a + x.$2) / 2;
  var c = 0.0;
  for (final x in b) {
    c += x.$2;
    if (c >= half) return x.$1;
  }
  return b.last.$1;
}

/// Relative change of tonight's R against the median of up to the last 14 nights; null before [minNights].
double? oxygenTrend(double tonight, List<double> history, {int minNights = 7}) {
  final h = history.sublist(math.max(0, history.length - 14)).where((x) => !x.isNaN).toList();
  if (h.length < minNights || tonight.isNaN) return null;
  final base = median(h);
  return (tonight - base) / base;
}

/// Tonight minus the median of up to the last 14 nights; null before [NightConfig.minNights] (I).
double? deviation(double tonight, List<double> history, [NightConfig cfg = const NightConfig()]) {
  final h = history.sublist(math.max(0, history.length - 14)).where((x) => !x.isNaN).toList();
  if (h.length < cfg.minNights || tonight.isNaN) return null;
  return tonight - median(h);
}

/// One night from the synced log. [times] are sorted Unix seconds; [up] is the upright vector (default: from this
/// log's walking epochs).
Night summarise(
  List<int> times,
  List<Epoch> epochs, {
  required bool podOnRight,
  required double utcOffsetH,
  required int nightDateS,
  required SleepModels models,
  Vec3? up,
  NightConfig cfg = const NightConfig(),
}) {
  final upV = up ?? uprightFromLog(epochs, cfg);
  final m = nightWindow(times, utcOffsetH, nightDateS, cfg);
  final (t0, ep0) = regrid([
    for (var i = 0; i < times.length; i++)
      if (m[i]) times[i],
  ], [
    for (var i = 0; i < epochs.length; i++)
      if (m[i]) epochs[i],
  ]);
  final night = Night();
  final rc = ep0.isEmpty ? null : restCandidate(ep0, cfg);
  if (rc == null) return night;
  final t = t0.sublist(rc.$1, rc.$2);
  final ep = ep0.sublist(rc.$1, rc.$2);
  night.inBedMin = ep.length * epochMin;
  final (asleep, _) = sleepWake(ep, models.wakeSleep);
  final onset = cfg.onsetMin ~/ epochMin;
  for (final (s, e) in runs([for (final a in asleep) a == 1])) {
    if (e - s < onset) asleep.fillRange(s, e, 0);
  }
  final idx = [
    for (var i = 0; i < asleep.length; i++)
      if (asleep[i] == 1) i,
  ];
  if (idx.isEmpty) return night;
  final s0 = idx.first;
  final s1 = idx.last + 1;
  final per = ep.sublist(s0, s1);
  final sl = [for (var i = s0; i < s1; i++) asleep[i] == 1];
  night
    ..startS = t[s0].toDouble()
    ..endS = t[s1 - 1] + 30.0
    ..periodMin = (s1 - s0) * epochMin
    ..asleepMin = sl.where((x) => x).length * epochMin
    ..asleep = sl;
  night
    ..efficiency = night.asleepMin / night.periodMin
    ..wakeUps = runs([for (final x in sl) !x]).where((r) => (r.$2 - r.$1) * epochMin >= cfg.wakeUpMin).length;
  final asleepEpochs = [
    for (var i = 0; i < per.length; i++)
      if (sl[i]) per[i],
  ];
  night.restingHrBpm = restingHr(asleepEpochs, cfg);
  final pairs = [
    for (final e in asleepEpochs)
      if (e.rmssdMs > 0 && e.cleanDiffs >= 5 && e.activityClass == ActivityClass.still) (e.rmssdMs, e.cleanDiffs),
  ];
  night.hrvDiffs = pairs.fold(0, (a, p) => a + p.$2);
  if (night.hrvDiffs >= cfg.hrvMinDiffs) night.rmssdMs = pooledRmssd(pairs);

  final labels = positions(per, podOnRight: podOnRight, up: upV);
  final podSide = podOnRight ? HeadLabel.right : HeadLabel.left;
  final pos = {for (final l in HeadLabel.values) l: 0.0};
  final onPod = List<bool>.filled(per.length, false);
  for (var i = 0; i < per.length; i++) {
    final lab = labels[i];
    if (sl[i] && lab != null) {
      pos[lab] = pos[lab]! + epochMin;
      onPod[i] = lab == podSide;
    }
  }
  night
    ..positionMin = pos
    ..onPodMin = onPod.where((x) => x).length * epochMin;
  final settle = cfg.skinSettleMin ~/ epochMin;
  final skin = [
    for (var i = 0; i < per.length; i++)
      if (sl[i] && i >= settle && !onPod[i] && !per[i].skinC.isNaN) per[i].skinC,
  ];
  if (skin.isNotEmpty) night.skinC = median(skin);
  final bursts = [
    for (final e in per)
      if (e.burstR > 0) (e.burstR, e.burstConfidence),
  ];
  night.oxygenBursts = bursts.length;
  if (bursts.isNotEmpty) night.oxygenRatio = oxygenNightRatio(bursts);
  // Labs: same features as sleep/wake (whole window, so time and pulse normalisation match), sliced to the period.
  final st = models.stages.predict(_features(ep));
  night.stages = [for (var i = s0; i < s1; i++) usable(ep[i]) ? st[i] : 0];
  return night;
}
