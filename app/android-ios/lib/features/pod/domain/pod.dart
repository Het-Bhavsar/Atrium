// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Pod and device entities (FEATURES.md #15-#21, #25-#27). Pure Dart.
library;

/// Measurement mode, `ble_gatt.md` Mode 0x0601.
enum MeasureMode { full, smart, lite }

extension MeasureModeInfo on MeasureMode {
  String get title => switch (this) {
    MeasureMode.full => 'Full',
    MeasureMode.smart => 'Smart',
    MeasureMode.lite => 'Lite',
  };
  String get description => switch (this) {
    MeasureMode.full => 'Every light at its highest rate. Best signal, shortest battery life.',
    MeasureMode.smart => "Light rates follow what you're doing. The default.",
    MeasureMode.lite => 'Heart rate in short bursts. No HRV. Longest battery life.',
  };

  /// Average draw in mA. Smart is from the EVT simulation; Full and Lite are [ASSUMED].
  double get averageMa => switch (this) {
    MeasureMode.full => 0.6,
    MeasureMode.smart => 0.3,
    MeasureMode.lite => 0.2,
  };

  /// Days from [percent] of the 30 mAh cell (95 % usable).
  double lifeDays([int percent = 100]) => (30 * 0.95 * percent / 100) / (averageMa * 24);
}

String formatLife(double days) {
  final h = (days * 24).round();
  if (h < 24) return '$h h';
  if (days >= 2) return '${days.round()} days';
  final d = h ~/ 24;
  final rh = h % 24;
  final ds = '$d day${d > 1 ? 's' : ''}';
  return rh > 0 ? '$ds $rh h' : ds;
}

/// Pod state as the app sees it (Charge State 0x0101, wear, connection).
enum PodSituation { worn, offSkin, charging, full, cold, syncDue, notFound }

class PodStatus {
  const PodStatus({
    required this.label,
    required this.batteryPercent,
    required this.onPlinth,
    required this.charging,
    required this.connected,
    required this.line,
    required this.lastSync,
    required this.hoursHeld,
    this.offSkin = false,
    this.eta,
    this.alert,
  });

  final String label;

  /// Shown in 10 % steps.
  final int batteryPercent;
  final bool onPlinth;
  final bool charging;
  final bool connected;
  final bool offSkin;
  final String line;
  final String lastSync;

  /// Hours of readings held on the pod, not yet synced (the pod keeps about 16-22 h).
  final int hoursHeld;
  final String? eta;
  final PodAlert? alert;

  static const Map<PodSituation, PodStatus> demo = {
    PodSituation.worn: PodStatus(
      label: 'On skin',
      batteryPercent: 60,
      onPlinth: false,
      charging: false,
      connected: true,
      line: 'On your temporal region',
      lastSync: '4 min ago',
      hoursHeld: 1,
    ),
    PodSituation.offSkin: PodStatus(
      label: 'Off skin',
      batteryPercent: 60,
      onPlinth: false,
      charging: false,
      connected: true,
      offSkin: true,
      line: 'Not on your skin',
      lastSync: '2 min ago',
      hoursHeld: 1,
    ),
    PodSituation.charging: PodStatus(
      label: 'On the Plinth',
      batteryPercent: 70,
      onPlinth: true,
      charging: true,
      connected: true,
      line: 'On the Plinth, charging',
      eta: 'Full in about an hour',
      lastSync: 'Just now',
      hoursHeld: 0,
    ),
    PodSituation.full: PodStatus(
      label: 'Charged',
      batteryPercent: 100,
      onPlinth: true,
      charging: false,
      connected: true,
      line: 'On the Plinth, charged',
      lastSync: 'Just now',
      hoursHeld: 0,
    ),
    PodSituation.cold: PodStatus(
      label: 'Too cold to charge',
      batteryPercent: 50,
      onPlinth: true,
      charging: false,
      connected: true,
      line: 'On the Plinth, charging paused',
      eta: 'Paused until the pod warms up',
      lastSync: 'Just now',
      hoursHeld: 0,
      alert: PodAlert.cold,
    ),
    PodSituation.syncDue: PodStatus(
      label: 'Sync due',
      batteryPercent: 40,
      onPlinth: false,
      charging: false,
      connected: true,
      line: 'On your temporal region',
      lastSync: '14 h ago',
      hoursHeld: 14,
      alert: PodAlert.sync,
    ),
    PodSituation.notFound: PodStatus(
      label: 'Not nearby',
      batteryPercent: 40,
      onPlinth: false,
      charging: false,
      connected: false,
      line: 'Not connected',
      lastSync: '08:12',
      hoursHeld: 9,
    ),
  };
}

enum PodAlert { cold, sync }

/// The pod keeps about 16 h of readings; the app syncs at least every 16 h (#20).
const int podLogHours = 16;

/// Fit Quality 0x0206: q 0-100 (255 = not measuring yet, about 4 s after fit mode starts) and flags.
/// Green only on [fitGood] (bit 5: q >= 60, pulse present, not moving, saturated or dark). Bit 3 (pulse
/// from IR) is ignored.
class FitQuality {
  const FitQuality({required this.q, required this.flags});
  final int q;
  final int flags;
  bool get moving => flags & 0x01 != 0;
  bool get saturated => flags & 0x02 != 0;
  bool get dark => flags & 0x04 != 0;
  bool get pulsePresent => flags & 0x10 != 0;
  bool get fitGood => flags & 0x20 != 0;
  bool get measuring => q != 255;
}

enum WearSide { left, right }

extension WearSideLabel on WearSide {
  String get label => this == WearSide.left ? 'Left temporal' : 'Right temporal';
  String get word => this == WearSide.left ? 'left' : 'right';
  WearSide get other => this == WearSide.left ? WearSide.right : WearSide.left;
}
