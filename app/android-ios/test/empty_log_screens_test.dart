// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/routes/app_router.dart';
import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';
import 'package:atrium/features/insights/domain/pod_metrics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

/// Real-data builds start with an empty pod log. Every screen must render that state, not throw
/// (a `days.last` on no days crashed Today on the first launch).
class _Empty extends MetricsNotifier {
  @override
  MetricsSource build() => PodMetrics(const [], podOnRight: false, now: DateTime(2026, 11, 8, 9));
}

void main() {
  const paths = [
    '/today',
    '/trends',
    '/device',
    '/settings',
    '/today/recovery',
    '/today/heart',
    '/today/sleep',
    '/today/effort',
    '/today/temperature',
    '/device/pad',
    '/device/firmware',
  ];
  for (final path in paths) {
    testWidgets('renders $path with an empty pod log', (tester) async {
      // The test font's square glyphs are wider than SF, so RenderFlex overflow warnings here are not real.
      final errors = <FlutterErrorDetails>[];
      final previous = FlutterError.onError;
      FlutterError.onError = (e) {
        if (!e.toString().contains('overflowed')) errors.add(e);
      };
      addTearDown(() => FlutterError.onError = previous);
      tester.view.physicalSize = const Size(1206, 2622);
      tester.view.devicePixelRatio = 3;
      addTearDown(tester.view.reset);
      await tester.pumpWidget(
        ProviderScope(
          overrides: [metricsProvider.overrideWith(_Empty.new)],
          child: MaterialApp.router(
            theme: buildTheme(Brightness.light),
            routerConfig: buildRouter(initial: path),
          ),
        ),
      );
      for (var i = 0; i < 5; i++) {
        await tester.pump(const Duration(milliseconds: 200));
      }
      FlutterError.onError = previous;
      expect(errors.map((e) => e.exceptionAsString()).toList(), isEmpty);
    });
  }
}
