// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/presentation/device_view.dart';
import 'package:atrium/features/device/presentation/firmware_view.dart';
import 'package:atrium/features/device/presentation/pad_view.dart';
import 'package:atrium/features/metrics/presentation/effort_view.dart';
import 'package:atrium/features/metrics/presentation/heart_view.dart';
import 'package:atrium/features/metrics/presentation/recovery_view.dart';
import 'package:atrium/features/metrics/presentation/sleep_view.dart';
import 'package:atrium/features/metrics/presentation/temperature_view.dart';
import 'package:atrium/features/metrics/presentation/trends_view.dart';
import 'package:atrium/features/onboarding/presentation/about_view.dart';
import 'package:atrium/features/onboarding/presentation/baseline_view.dart';
import 'package:atrium/features/onboarding/presentation/bluetooth_view.dart';
import 'package:atrium/features/onboarding/presentation/fit_view.dart';
import 'package:atrium/features/onboarding/presentation/pair_view.dart';
import 'package:atrium/features/onboarding/presentation/place_view.dart';
import 'package:atrium/features/onboarding/presentation/scan_view.dart';
import 'package:atrium/features/onboarding/presentation/signal_view.dart';
import 'package:atrium/features/onboarding/presentation/wake_view.dart';
import 'package:atrium/features/onboarding/presentation/welcome_view.dart';
import 'package:atrium/features/profile/presentation/profile_view.dart';
import 'package:atrium/features/profile/presentation/you_view.dart';
import 'package:atrium/features/research/presentation/recordings_view.dart';
import 'package:atrium/features/settings/presentation/name_story_view.dart';
import 'package:atrium/features/settings/presentation/settings_view.dart';
import 'package:atrium/features/today/presentation/today_view.dart';
import 'package:flutter/widgets.dart';
import 'package:go_router/go_router.dart';

const _metrics = ['recovery', 'heart', 'sleep', 'effort', 'temperature'];

List<GoRoute> _details(List<String> names) => [
  for (final n in names)
    GoRoute(
      path: n,
      builder: (_, _) => switch (n) {
        'recovery' => const RecoveryView(),
        'heart' => const HeartView(),
        'sleep' => const SleepView(),
        'effort' => const EffortView(),
        'temperature' => const TemperatureView(),
        'pad' => const PadView(),
        'profile' => const ProfileView(),
        'story' => const NameStoryView(),
        'recordings' => const RecordingsView(),
        _ => const FirmwareView(),
      },
    ),
];

extension DetailNav on BuildContext {
  /// Pushes a detail page ('/heart', '/pad', ...) inside the current tab.
  void pushDetail(String path) {
    final segs = GoRouterState.of(this).uri.pathSegments;
    final tab = segs.isEmpty ? 'today' : segs.first;
    push('/$tab$path');
  }
}

/// Route table (BLUEPRINT.md section 4).
///
/// Deep links (`atrium:///today?dev=charging`) open any screen; [onQuery] receives the query so debug
/// builds can set the demo state the same way the HTML prototype's URL does. The query is then dropped.
GoRouter buildRouter({String initial = '/today', void Function(Map<String, String> query)? onQuery}) => GoRouter(
  initialLocation: initial,
  redirect: (_, state) {
    final q = state.uri.queryParameters;
    if (q.isEmpty || onQuery == null) return null;
    onQuery(q);
    return state.uri.path.isEmpty ? '/today' : state.uri.path;
  },
  routes: [
    StatefulShellRoute(
      builder: (context, state, shell) => TabShell(shell: shell),
      // Tabs keep their state (an indexed stack) and fade in when chosen instead of snapping.
      navigatorContainerBuilder: (context, shell, children) => FadeIndexedStack(index: shell.currentIndex, children: children),
      branches: [
        // Detail pages open inside the tab they came from, so the tab bar stays (as in the prototype).
        StatefulShellBranch(
          routes: [
            GoRoute(path: '/today', builder: (_, _) => const TodayView(), routes: _details([..._metrics, 'pad'])),
          ],
        ),
        StatefulShellBranch(
          routes: [GoRoute(path: '/trends', builder: (_, _) => const TrendsView(), routes: _details(_metrics))],
        ),
        StatefulShellBranch(
          routes: [
            GoRoute(path: '/device', builder: (_, _) => const DeviceView(), routes: _details(['pad', 'firmware'])),
          ],
        ),
        StatefulShellBranch(
          routes: [
            GoRoute(path: '/settings', builder: (_, _) => const SettingsView(), routes: _details(['pad', 'firmware', 'profile', 'story', 'recordings'])),
          ],
        ),
      ],
    ),
    // Old top-level paths (deep links) land in the tab that owns them.
    for (final m in _metrics) GoRoute(path: '/$m', redirect: (_, _) => '/today/$m'),
    GoRoute(path: '/pad', redirect: (_, _) => '/device/pad'),
    GoRoute(path: '/firmware', redirect: (_, _) => '/device/firmware'),
    GoRoute(path: '/welcome', builder: (_, _) => const WelcomeView()),
    GoRoute(path: '/about', builder: (_, _) => const AboutView()),
    GoRoute(path: '/you', builder: (_, _) => const YouView()),
    GoRoute(path: '/bluetooth', builder: (_, _) => const BluetoothView()),
    GoRoute(path: '/wake', builder: (_, _) => const WakeView()),
    GoRoute(path: '/scan', builder: (_, _) => const ScanView()),
    GoRoute(path: '/pair', builder: (_, _) => const PairView()),
    GoRoute(path: '/fit', builder: (_, _) => const FitView()),
    GoRoute(path: '/place', builder: (_, _) => const PlaceView()),
    GoRoute(path: '/signal', builder: (_, _) => const SignalView()),
    GoRoute(path: '/baseline', builder: (_, _) => const BaselineView()),
  ],
);
