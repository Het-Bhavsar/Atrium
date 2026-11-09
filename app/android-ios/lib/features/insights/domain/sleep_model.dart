import 'dart:convert';
import 'dart:math' as math;

/// Sleep and wake from the pod log (FEATURES.md #4): a port of `app/python/atrium_ml/sleep.py`. An estimate, not a
/// sleep study.
///
/// Per 30 s epoch, from fields the pod already logs (activity index in mg, pulse in bpm):
/// - act: log(1 + activity index), its mean over +-2.5 min and its max over +-5 min;
/// - hr: (pulse - the night's median) / the night's interquartile range (at least 1), and its standard deviation
///   over +-2.5 min;
/// - t: hours since the first epoch of the window the model runs on.
/// A logistic regression on these 6 features gives P(asleep). The coefficients are data (`assets/models/
/// sleep_v1.json`), so a retrained model ships without a code change.
const double _epochS = 30;

/// Features per epoch, one row of 6 each. [actMg] and [hrBpm] hold NaN where the value is missing.
List<List<double>> epochFeatures(List<double> actMg, List<double> hrBpm) {
  final n = actMg.length;
  final act = [for (final a in actMg) a.isNaN ? 0.0 : math.log(1 + a)];
  var hrz = List<double>.filled(n, 0);
  final good = [
    for (var i = 0; i < n; i++)
      if (!hrBpm[i].isNaN) i,
  ];
  if (good.isNotEmpty) {
    final hr = _interp(n, good, hrBpm);
    final s = [...hr]..sort();
    final q1 = _percentile(s, 25);
    final med = _percentile(s, 50);
    final q3 = _percentile(s, 75);
    final iqr = math.max(q3 - q1, 1.0);
    hrz = [for (final h in hr) (h - med) / iqr];
  }
  final actMean = _rolling(act, 5, _mean);
  final actMax = _rolling(act, 10, (w) => w.reduce(math.max));
  final hrSd = _rolling(hrz, 5, _std);
  return [
    for (var i = 0; i < n; i++) [act[i], actMean[i], actMax[i], hrz[i], hrSd[i], i * _epochS / 3600],
  ];
}

/// Linear interpolation over the indices in [good], held flat beyond the ends (numpy `interp`).
List<double> _interp(int n, List<int> good, List<double> y) {
  final out = List<double>.filled(n, 0);
  var k = 0;
  for (var i = 0; i < n; i++) {
    if (i <= good.first) {
      out[i] = y[good.first];
    } else if (i >= good.last) {
      out[i] = y[good.last];
    } else {
      while (good[k + 1] < i) {
        k++;
      }
      final a = good[k];
      final b = good[k + 1];
      out[i] = y[a] + (y[b] - y[a]) * (i - a) / (b - a);
    }
  }
  return out;
}

/// Percentile of sorted values, linear between ranks (numpy's default).
double _percentile(List<double> sorted, double q) {
  final pos = (sorted.length - 1) * q / 100;
  final lo = pos.floor();
  final hi = math.min(lo + 1, sorted.length - 1);
  return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - lo);
}

double _mean(List<double> w) => w.reduce((a, b) => a + b) / w.length;

double _std(List<double> w) {
  final m = _mean(w);
  return math.sqrt(w.map((v) => (v - m) * (v - m)).reduce((a, b) => a + b) / w.length);
}

List<double> _rolling(List<double> x, int half, double Function(List<double>) fn) => [
  for (var i = 0; i < x.length; i++) fn(x.sublist(math.max(0, i - half), math.min(x.length, i + half + 1))),
];

/// One multinomial logistic model: weights (bias + 6 features) x classes, feature mean and sd.
class LogisticModel {
  LogisticModel.fromJson(Map<String, dynamic> d)
    : w = [
        for (final row in d['w'] as List<dynamic>) [for (final v in row as List<dynamic>) (v as num).toDouble()],
      ],
      mu = [for (final v in d['mu'] as List<dynamic>) (v as num).toDouble()],
      sd = [for (final v in d['sd'] as List<dynamic>) (v as num).toDouble()],
      threshold = (d['sleep_threshold'] as num?)?.toDouble(),
      source = d['source'] as String? ?? '';

  final List<List<double>> w;
  final List<double> mu;
  final List<double> sd;

  /// Two classes: asleep when P(asleep) is at least this.
  final double? threshold;
  final String source;

  int get classes => w.first.length;

  /// Class probabilities per epoch (softmax).
  List<List<double>> proba(List<List<double>> x) => [
    for (final row in x)
      () {
        final z = [for (var k = 0; k < classes; k++) w[0][k]];
        for (var j = 0; j < row.length; j++) {
          final v = (row[j] - mu[j]) / sd[j];
          for (var k = 0; k < classes; k++) {
            z[k] += w[j + 1][k] * v;
          }
        }
        final top = z.reduce(math.max);
        final e = [for (final v in z) math.exp(v - top)];
        final s = e.reduce((a, b) => a + b);
        return [for (final v in e) v / s];
      }(),
  ];

  /// Class per epoch. Two classes with a threshold: 1 = asleep when P(asleep) >= it; otherwise the most likely.
  List<int> predict(List<List<double>> x) => [
    for (final p in proba(x))
      if (classes == 2 && threshold != null)
        p[1] >= threshold! ? 1 : 0
      else
        [for (var k = 0; k < p.length; k++) k].reduce((a, b) => p[b] > p[a] ? b : a),
  ];
}

/// The shipped models: sleep/wake (#4) and wake / non-REM / REM (Labs).
class SleepModels {
  SleepModels.parse(String json) : this._(jsonDecode(json) as Map<String, dynamic>);
  SleepModels._(Map<String, dynamic> d)
    : version = d['version'] as String? ?? '',
      wakeSleep = LogisticModel.fromJson(d['wake_sleep'] as Map<String, dynamic>),
      stages = LogisticModel.fromJson(d['wake_nrem_rem'] as Map<String, dynamic>);

  static const asset = 'assets/models/sleep_v1.json';

  final String version;
  final LogisticModel wakeSleep;
  final LogisticModel stages;
}
