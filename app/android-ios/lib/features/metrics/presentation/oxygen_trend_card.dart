// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:flutter/material.dart';

/// Nights of baseline before the trend shows (`nightly.dart` `oxygenTrend`, minNights).
const int oxygenMinNights = 7;

/// Within this relative change the night reads as "close to your usual" [ASSUMED].
const double oxygenCloseBand = 0.03;

/// Labs (#8): shown only in real-data builds, with Labs on, and with at least [oxygenMinNights] earlier nights.
bool showOxygenTrend({required bool realData, required bool labs, required OxygenTrend? trend}) =>
    realData && labs && trend != null && trend.nights >= oxygenMinNights && trend.change.isFinite;

/// The words for a night's change. Says which way the light ratio moved, not what it means.
String oxygenTrendWords(OxygenTrend t) => t.change.abs() < oxygenCloseBand
    ? 'Close to your usual'
    : t.change > 0
    ? 'Higher ratio than your usual'
    : 'Lower ratio than your usual';

/// Overnight oxygen trend (Labs): last night's red/infrared ratio as a multiple of the user's own baseline. Never a
/// percentage, and no reading of what the number means for health.
class OxygenTrendCard extends StatelessWidget {
  const OxygenTrendCard(this.trend, {super.key});
  final OxygenTrend trend;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return AtriumCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(oxygenTrendWords(trend), style: AtriumType.bodyStrong(t.ink)),
          const SizedBox(height: 4),
          Text.rich(
            TextSpan(
              children: [
                TextSpan(
                  text: trend.factor.toStringAsFixed(2),
                  style: AtriumType.serifAt(30, color: t.ink),
                ),
                TextSpan(text: ' × your usual', style: AtriumType.secondary(t.ink2)),
              ],
            ),
          ),
          const SizedBox(height: Space.s),
          Text(
            "A research trend: last night's ratio of red to infrared light, against your own last ${trend.nights} nights. "
            'It is not a blood-oxygen percentage and not a medical reading.',
            style: AtriumType.secondary(t.ink2),
          ),
        ],
      ),
    );
  }
}
