/// Number and time formatting used across screens.
library;

/// "7 h 0 min" (rounds total minutes first, so never "6 h 60 min").
String hm(num minutes) {
  final t = minutes.round();
  return '${t ~/ 60} h ${t % 60} min';
}

/// "7h 0m".
String hmCompact(num minutes) {
  final t = minutes.round();
  return '${t ~/ 60}h ${t % 60}m';
}

/// Signed with one decimal and a true minus: "+0.2", "−0.1", "0.0".
String signed1(num v) {
  final r = (v * 10).round() / 10;
  final s = r > 0
      ? '+'
      : r < 0
      ? '−'
      : '';
  return '$s${r.abs().toStringAsFixed(1)}';
}
