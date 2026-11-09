// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

/// Tapping a chart shows the point's value and label, as the prototype's chart scrubbing does; dragging moves
/// it, and it fades a moment after the finger lifts.
void main() {
  Widget host(Widget chart) => MaterialApp(
    theme: buildTheme(Brightness.light),
    home: Scaffold(
      body: Padding(
        padding: const EdgeInsets.only(top: 80),
        child: SizedBox(width: 360, child: chart),
      ),
    ),
  );

  testWidgets('tap shows the readout, drag moves it, it fades after release', (tester) async {
    await tester.pumpWidget(host(const AtriumChart(points: [50, 60, 70, 80], labels: ['Mon', 'Tue', 'Wed', 'Thu'], unit: 'bpm', color: Colors.red)));
    expect(find.textContaining('Mon', findRichText: true), findsNothing);

    final box = tester.getRect(find.byType(AtriumChart));
    // Plot width is 360 - 34 = 326, four points: each 81.5 wide.
    final g = await tester.startGesture(Offset(box.left + 20, box.center.dy));
    await tester.pump(const Duration(milliseconds: 150)); // tap-down waits out the drag recognizer
    expect(find.textContaining('50 bpm  Mon', findRichText: true), findsOneWidget);

    for (var k = 0; k < 20; k++) {
      await g.moveBy(const Offset(13, 0));
      await tester.pump(const Duration(milliseconds: 16));
    }
    expect(find.textContaining('80 bpm  Thu', findRichText: true), findsOneWidget);

    await g.up();
    await tester.pump(const Duration(seconds: 1));
    expect(find.textContaining('Thu', findRichText: true), findsOneWidget);
    await tester.pump(const Duration(seconds: 2));
    expect(find.textContaining('Thu', findRichText: true), findsNothing);
  });

  testWidgets('a chart without labels stays static', (tester) async {
    await tester.pumpWidget(host(const AtriumChart(points: [1, 2, 3], color: Colors.green)));
    await tester.tap(find.byType(AtriumChart));
    await tester.pump();
    expect(find.byType(GestureDetector), findsNothing);
  });
}
