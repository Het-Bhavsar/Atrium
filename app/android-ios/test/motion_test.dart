// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

/// Motion plays by default and is skipped when the system asks for less (Reduce Motion).
void main() {
  Widget host(Widget child, {bool reduce = false}) => MaterialApp(
    theme: buildTheme(Brightness.light),
    home: MediaQuery(
      data: MediaQueryData(disableAnimations: reduce),
      child: Scaffold(body: Center(child: child)),
    ),
  );

  double opacityOf(WidgetTester tester) => tester.widget<FadeTransition>(find.byType(FadeTransition).first).opacity.value;

  testWidgets('content reveals over time', (tester) async {
    await tester.pumpWidget(host(const Reveal(child: Text('hello'))));
    expect(opacityOf(tester), 0);
    await tester.pumpAndSettle();
    expect(opacityOf(tester), 1);
  });

  testWidgets('Reduce Motion shows content at once', (tester) async {
    await tester.pumpWidget(host(const Reveal(child: Text('hello')), reduce: true));
    expect(opacityOf(tester), 1);
  });

  testWidgets('cover numbers count up, or land at once under Reduce Motion', (tester) async {
    await tester.pumpWidget(host(const CoverNumber('54', 'bpm')));
    expect(find.textContaining('54', findRichText: true), findsNothing);
    await tester.pumpAndSettle();
    expect(find.textContaining('54', findRichText: true), findsOneWidget);

    await tester.pumpWidget(host(const CoverNumber('62', 'bpm'), reduce: true));
    expect(find.textContaining('62', findRichText: true), findsOneWidget);
  });

  testWidgets('a new tab fades in', (tester) async {
    Widget stack(int i) => host(FadeIndexedStack(index: i, children: const [Text('a'), Text('b')]));
    await tester.pumpWidget(stack(0));
    await tester.pumpWidget(stack(1));
    expect(opacityOf(tester), lessThan(0.5));
    await tester.pumpAndSettle();
    expect(opacityOf(tester), 1);
  });
}
