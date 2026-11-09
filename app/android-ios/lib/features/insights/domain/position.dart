import 'dart:math' as math;

import 'package:atrium/features/insights/domain/metrics.dart';

/// Sleep head position (FEATURES.md #5): a port of `app/python/atrium_ml/position.py`.
///
/// Pod frame: +x up along the long axis to the antenna end, +z out of the skin, +y = z x x.
/// Head frame: up = the calibrated vertical (mean gravity while walking, else +x); right = the skin
/// normal, its sign set by the wear side; forward = up x right.
class Vec3 {
  const Vec3(this.x, this.y, this.z);
  final double x;
  final double y;
  final double z;
  double dot(Vec3 o) => x * o.x + y * o.y + z * o.z;
  Vec3 operator -(Vec3 o) => Vec3(x - o.x, y - o.y, z - o.z);
  Vec3 operator *(double k) => Vec3(x * k, y * k, z * k);
  Vec3 operator +(Vec3 o) => Vec3(x + o.x, y + o.y, z + o.z);
  double get norm => math.sqrt(dot(this));
  Vec3 unit() => this * (1 / (norm + 1e-12));
  Vec3 cross(Vec3 o) => Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
}

/// Unit gravity in the pod frame from the logged angles: pitch = atan2(-x, sqrt(y^2 + z^2)), roll = atan2(y, z).
Vec3 gravityFromPitchRoll(double pitchDeg, double rollDeg) {
  final p = pitchDeg * math.pi / 180;
  final r = rollDeg * math.pi / 180;
  return Vec3(-math.sin(p), math.cos(p) * math.sin(r), math.cos(p) * math.cos(r));
}

/// Rows: right, forward, up, in pod coordinates.
List<Vec3> headFrame(Vec3 upPod, {required bool podOnRight, Vec3 normal = const Vec3(0, 0, 1)}) {
  final up = upPod.unit();
  var right = podOnRight ? normal : normal * -1;
  right = (right - up * right.dot(up)).unit();
  final fwd = up.cross(right);
  return [right, fwd, up];
}

enum HeadLabel { upright, back, left, right, front }

/// [lieDeg]: head axis more than this from vertical counts as lying.
HeadLabel classifyVector(Vec3 g, List<Vec3> frame, {double lieDeg = 50}) {
  final u = g.unit();
  final r = frame[0].dot(u);
  final f = frame[1].dot(u);
  final up = frame[2].dot(u);
  if (up > math.cos(lieDeg * math.pi / 180)) return HeadLabel.upright;
  final roll = math.atan2(-r, f) * 180 / math.pi; // 0 back, +90 right side down, -90 left side down, 180 front
  if (roll.abs() <= 45) return HeadLabel.back;
  if (roll.abs() >= 135) return HeadLabel.front;
  return roll > 0 ? HeadLabel.right : HeadLabel.left;
}

/// Per-epoch labels with hysteresis: a new position must hold [minEpochs] epochs.
List<HeadLabel> classifyEpochs(List<Vec3> g, List<Vec3> frame, {int minEpochs = 2}) {
  final out = <HeadLabel>[];
  HeadLabel? cur;
  HeadLabel? cand;
  var run = 0;
  for (final v in g) {
    final lab = classifyVector(v, frame);
    if (cur == null) {
      cur = lab;
    } else if (lab != cur) {
      run = lab == cand ? run + 1 : 1;
      cand = lab;
      if (run >= minEpochs) {
        cur = lab;
        run = 0;
      }
    } else {
      run = 0;
    }
    out.add(cur);
  }
  return out;
}

/// Mean gravity direction over walking epochs; +x (as worn) when there are too few.
Vec3 uprightFromWalking(List<Vec3> walkingGravity, {int minEpochs = 20}) {
  if (walkingGravity.length < minEpochs) return const Vec3(1, 0, 0);
  var s = const Vec3(0, 0, 0);
  for (final v in walkingGravity) {
    s = s + v;
  }
  return s.unit();
}

HeadSide? toHeadSide(HeadLabel l) => switch (l) {
  HeadLabel.back => HeadSide.back,
  HeadLabel.left => HeadSide.left,
  HeadLabel.right => HeadSide.right,
  HeadLabel.front => HeadSide.front,
  HeadLabel.upright => null,
};
